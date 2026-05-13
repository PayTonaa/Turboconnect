/*
 ============================================================================
 Name        : TurboSoundDownloader.cpp
 ============================================================================
 */

#include "TurboSoundDownloader.h"

#include <coemain.h>
#include <eikenv.h>
#include <stringpool.h>
#include <uri8.h>
#include <http/rhttpheaders.h>
#include <http/thttphdrval.h>
#include <http/mhttpdatasupplier.h>
#include <http/rhttpconnectioninfo.h>
#include <http.h>
#include <utf.h>
#include <commdbconnpref.h>

_LIT8(KGetMethod, "GET");

// Append the URL-encoded form of aQuery (treated as ASCII) to aOut.
// Reserved characters get %XX, spaces become '+'. The buffer must have room
// for at most 3x the input length plus the prefix.
static void UrlEncodeAppend(const TDesC& aQuery, TDes8& aOut)
	{
	for (TInt i = 0; i < aQuery.Length(); i++)
		{
		if (aOut.Length() + 3 > aOut.MaxLength())
			{
			break;
			}
		const TUint c = aQuery[i];
		const TBool unreserved =
			(c >= '0' && c <= '9') ||
			(c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			(c == '-' || c == '_' || c == '.' || c == '~');
		if (unreserved)
			{
			aOut.Append(static_cast<TUint8>(c));
			}
		else if (c == ' ')
			{
			aOut.Append('+');
			}
		else if (c < 0x80)
			{
			aOut.AppendFormat(_L8("%%%02X"), c);
			}
		else
			{
			// Non-ASCII: emit a placeholder so the URL stays valid.
			aOut.Append('_');
			}
		}
	}

CTurboSoundDownloader* CTurboSoundDownloader::NewL(MTurboSoundDownloadObserver& aObserver)
	{
	CTurboSoundDownloader* self = new (ELeave) CTurboSoundDownloader(aObserver);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

CTurboSoundDownloader::CTurboSoundDownloader(MTurboSoundDownloadObserver& aObserver)
	: iObserver(aObserver),
	  iSocketServOpen(EFalse),
	  iConnectionOpen(EFalse),
	  iSessionOpen(EFalse),
	  iTransactionActive(EFalse),
	  iMode(EIdle),
	  iBodyBuf(NULL),
	  iFileOpen(EFalse),
	  iLocalPath(NULL),
	  iBytesDone(0),
	  iContentLength(0),
	  iRemoteTitles(NULL),
	  iRemoteFilenames(NULL),
	  iRemoteUrls(NULL)
	{
	}

void CTurboSoundDownloader::ConstructL()
	{
	// IMPORTANT: Do NOT bring up the IAP / RHTTPSession here. This object is
	// created during app start; calling RConnection::Start() before the UI
	// is fully up makes the app exit back to the home screen. Network is
	// lazy-initialised the first time the user actually triggers an HTTP
	// operation - see EnsureConnectedL().
	iRemoteTitles = new (ELeave) CDesCArrayFlat(8);
	iRemoteFilenames = new (ELeave) CDesCArrayFlat(8);
	iRemoteUrls = new (ELeave) CDesCArrayFlat(8);
	}

void CTurboSoundDownloader::EnsureConnectedL()
	{
	if (iSessionOpen && iConnectionOpen && iSocketServOpen)
		{
		return;
		}

	iObserver.HandleNetStep(_L("Net 1/4: SocketServ"));
	if (!iSocketServOpen)
		{
		TInt e = iSocketServ.Connect();
		if (e != KErrNone)
			{
			TBuf<64> m;
			m.Format(_L("SocketServ err %d"), e);
			iObserver.HandleNetStep(m);
			User::Leave(e);
			}
		iSocketServOpen = ETrue;
		}

	iObserver.HandleNetStep(_L("Net 2/4: Conn.Open"));
	if (!iConnectionOpen)
		{
		TInt e = iConnection.Open(iSocketServ);
		if (e != KErrNone)
			{
			TBuf<64> m;
			m.Format(_L("Conn.Open err %d"), e);
			iObserver.HandleNetStep(m);
			User::Leave(e);
			}
		iConnectionOpen = ETrue;
		}

	iObserver.HandleNetStep(_L("Net 3/4: pick IAP..."));
	// Different S60 builds answer to different combinations. Try them in
	// order, each one logged so the status bar shows which path succeeded.
	TInt err = KErrNotFound;

	// Attempt 1: prompt + outgoing direction (the combo that worked on the
	// very first run when the IAP picker did appear).
	{
	TCommDbConnPref pref;
	pref.SetDialogPreference(ECommDbDialogPrefPrompt);
	pref.SetDirection(ECommDbConnectionDirectionOutgoing);
	err = iConnection.Start(pref);
	}

	// Attempt 2: re-use last IAP without prompting (covers case where the
	// system says 'I have one, no need to ask').
	if (err == KErrNotFound)
		{
		iObserver.HandleNetStep(_L("Net 3/4: try last IAP"));
		TCommDbConnPref pref;
		pref.SetDialogPreference(ECommDbDialogPrefDoNotPrompt);
		pref.SetDirection(ECommDbConnectionDirectionOutgoing);
		err = iConnection.Start(pref);
		}

	// Attempt 3: prompt without any direction filter (covers IAPs that are
	// registered without an explicit direction).
	if (err == KErrNotFound)
		{
		iObserver.HandleNetStep(_L("Net 3/4: prompt any"));
		TCommDbConnPref pref;
		pref.SetDialogPreference(ECommDbDialogPrefPrompt);
		err = iConnection.Start(pref);
		}

	// Attempt 4: bare Start() - Symbian's default selection behaviour.
	if (err == KErrNotFound)
		{
		iObserver.HandleNetStep(_L("Net 3/4: plain Start"));
		err = iConnection.Start();
		}

	if (err != KErrNone)
		{
		TBuf<64> m;
		m.Format(_L("Start err %d"), err);
		iObserver.HandleNetStep(m);
		iConnection.Close();
		iConnectionOpen = EFalse;
		iSocketServ.Close();
		iSocketServOpen = EFalse;
		User::Leave(err);
		}

	iObserver.HandleNetStep(_L("Net 4/4: HTTP session"));
	if (!iSessionOpen)
		{
		iSession.OpenL();
		iSessionOpen = ETrue;

		RStringPool sp = iSession.StringPool();
		RHTTPConnectionInfo ci = iSession.ConnectionInfo();
		ci.SetPropertyL(
			sp.StringF(HTTP::EHttpSocketServ, RHTTPSession::GetTable()),
			THTTPHdrVal(iSocketServ.Handle()));

		TInt connPtr = REINTERPRET_CAST(TInt, &iConnection);
		ci.SetPropertyL(
			sp.StringF(HTTP::EHttpSocketConnection, RHTTPSession::GetTable()),
			THTTPHdrVal(connPtr));
		}

	iObserver.HandleNetStep(_L("Net OK. GET..."));
	}

CTurboSoundDownloader::~CTurboSoundDownloader()
	{
	CancelOperation();
	if (iSessionOpen)
		{
		iSession.Close();
		iSessionOpen = EFalse;
		}
	if (iConnectionOpen)
		{
		iConnection.Stop();
		iConnection.Close();
		iConnectionOpen = EFalse;
		}
	if (iSocketServOpen)
		{
		iSocketServ.Close();
		iSocketServOpen = EFalse;
		}
	delete iRemoteTitles;
	delete iRemoteFilenames;
	delete iRemoteUrls;
	delete iBodyBuf;
	delete iLocalPath;
	}

void CTurboSoundDownloader::Cleanup()
	{
	if (iTransactionActive)
		{
		iTransaction.Close();
		iTransactionActive = EFalse;
		}
	if (iFileOpen)
		{
		iFile.Close();
		iFileOpen = EFalse;
		}
	delete iBodyBuf;
	iBodyBuf = NULL;
	iBytesDone = 0;
	iContentLength = 0;
	iMode = EIdle;
	}

void CTurboSoundDownloader::CancelOperation()
	{
	Cleanup();
	}

TInt CTurboSoundDownloader::RemoteTrackCount() const
	{
	return iRemoteTitles ? iRemoteTitles->MdcaCount() : 0;
	}

const TDesC& CTurboSoundDownloader::RemoteFilename(TInt aIndex) const
	{
	return iRemoteFilenames->MdcaPoint(aIndex);
	}

void CTurboSoundDownloader::FetchOnlineL(const TDesC8& aBaseUrl, const TDesC& aQuery)
	{
	EnsureConnectedL();
	Cleanup();
	iMode = EFetchOnline;

	// /api/fetch?q=<encoded>. Worst case: 3x the query length + prefix.
	const TInt cap = aBaseUrl.Length() + 32 + (aQuery.Length() * 3);
	HBufC8* url = HBufC8::NewLC(cap);
	TPtr8 p = url->Des();
	p.Copy(aBaseUrl);
	p.Append(_L8("/api/fetch?q="));
	UrlEncodeAppend(aQuery, p);

	// Buffer for the server's response text (we surface it to the user).
	iBodyBuf = HBufC8::NewL(0);

	StartGetL(*url);
	CleanupStack::PopAndDestroy(url);
	}

void CTurboSoundDownloader::FetchTrackListL(const TDesC8& aBaseUrl, const TDesC8& aQuery)
	{
	EnsureConnectedL();
	Cleanup();
	iMode = EFetchList;

	HBufC8* url = HBufC8::NewLC(aBaseUrl.Length() + aQuery.Length() + 32);
	TPtr8 p = url->Des();
	p.Copy(aBaseUrl);
	p.Append(_L8("/api/search_plain?q="));
	p.Append(aQuery);

	iBodyBuf = HBufC8::NewL(0);

	StartGetL(*url);
	CleanupStack::PopAndDestroy(url);
	}

void CTurboSoundDownloader::DownloadByIndexL(TInt aIndex, const TDesC8& aBaseUrl, const TDesC& aTargetFolder)
	{
	if (!iRemoteUrls || aIndex < 0 || aIndex >= iRemoteUrls->MdcaCount())
		{
		User::Leave(KErrArgument);
		}

	EnsureConnectedL();
	Cleanup();
	iMode = EDownloadFile;

	const TPtrC relativeUrl = iRemoteUrls->MdcaPoint(aIndex);
	const TPtrC fileName = iRemoteFilenames->MdcaPoint(aIndex);

	HBufC8* fullUrl = HBufC8::NewLC(aBaseUrl.Length() + relativeUrl.Length());
	TPtr8 up = fullUrl->Des();
	up.Copy(aBaseUrl);
	for (TInt i = 0; i < relativeUrl.Length(); i++)
		{
		up.Append(static_cast<TUint8>(relativeUrl[i]));
		}

	HBufC* path = HBufC::NewLC(aTargetFolder.Length() + fileName.Length());
	TPtr pp = path->Des();
	pp.Copy(aTargetFolder);
	pp.Append(fileName);

	RFs& fs = CCoeEnv::Static()->FsSession();
	TInt mk = fs.MkDirAll(*path);
	if ((mk != KErrNone) && (mk != KErrAlreadyExists))
		{
		}
	User::LeaveIfError(iFile.Replace(fs, *path, EFileWrite));
	iFileOpen = ETrue;

	delete iLocalPath;
	iLocalPath = path;
	CleanupStack::Pop(path);

	StartGetL(*fullUrl);
	CleanupStack::PopAndDestroy(fullUrl);
	}

void CTurboSoundDownloader::StartGetL(const TDesC8& aUrl)
	{
	TUriParser8 uri;
	User::LeaveIfError(uri.Parse(aUrl));

	RStringPool sp = iSession.StringPool();
	RStringF method = sp.StringF(HTTP::EGET, RHTTPSession::GetTable());

	iTransaction = iSession.OpenTransactionL(uri, *this, method);
	iTransactionActive = ETrue;
	iTransaction.SubmitL();
	}

void CTurboSoundDownloader::AppendBodyL(const TDesC8& aChunk)
	{
	if (!iBodyBuf)
		{
		iBodyBuf = HBufC8::NewL(aChunk.Length());
		iBodyBuf->Des().Copy(aChunk);
		return;
		}
	HBufC8* bigger = HBufC8::NewL(iBodyBuf->Length() + aChunk.Length());
	TPtr8 bp = bigger->Des();
	bp.Copy(*iBodyBuf);
	bp.Append(aChunk);
	delete iBodyBuf;
	iBodyBuf = bigger;
	}

void CTurboSoundDownloader::WriteBodyToFileL(const TDesC8& aChunk)
	{
	if (!iFileOpen)
		{
		return;
		}
	User::LeaveIfError(iFile.Write(aChunk));
	iBytesDone += aChunk.Length();
	iObserver.HandleDownloadProgress(iBytesDone, iContentLength);
	}

void CTurboSoundDownloader::ParseRemoteListAndNotifyL()
	{
	iRemoteTitles->Reset();
	iRemoteFilenames->Reset();
	iRemoteUrls->Reset();

	if (!iBodyBuf)
		{
		iObserver.HandleRemoteTracksReadyL(*iRemoteTitles);
		return;
		}

	TPtrC8 body = iBodyBuf->Des();
	TInt start = 0;
	while (start < body.Length())
		{
		TInt nl = body.Mid(start).Locate('\n');
		const TInt end = (nl == KErrNotFound) ? body.Length() : start + nl;
		TPtrC8 line = body.Mid(start, end - start);
		start = end + 1;

		// strip trailing CR
		while (line.Length() > 0 && line[line.Length() - 1] == '\r')
			{
			line.Set(line.Left(line.Length() - 1));
			}
		if (line.Length() == 0)
			{
			continue;
			}

		TInt p1 = line.Locate('|');
		if (p1 == KErrNotFound)
			{
			continue;
			}
		TPtrC8 a = line.Left(p1);
		TPtrC8 rest = line.Mid(p1 + 1);
		TInt p2 = rest.Locate('|');
		if (p2 == KErrNotFound)
			{
			continue;
			}
		TPtrC8 b = rest.Left(p2);
		TPtrC8 c = rest.Mid(p2 + 1);

		HBufC* title = HBufC::NewLC(a.Length());
		title->Des().Copy(a); // ASCII path
		HBufC* fname = HBufC::NewLC(b.Length());
		fname->Des().Copy(b);
		HBufC* url16 = HBufC::NewLC(c.Length());
		url16->Des().Copy(c);

		iRemoteTitles->AppendL(*title);
		iRemoteFilenames->AppendL(*fname);
		iRemoteUrls->AppendL(*url16);

		CleanupStack::PopAndDestroy(3); // url16, fname, title
		}

	iObserver.HandleRemoteTracksReadyL(*iRemoteTitles);
	}

void CTurboSoundDownloader::MHFRunL(RHTTPTransaction aTransaction, const THTTPEvent& aEvent)
	{
	switch (aEvent.iStatus)
		{
		case THTTPEvent::EGotResponseHeaders:
			{
			RHTTPResponse resp = aTransaction.Response();
			RStringPool sp = iSession.StringPool();
			RHTTPHeaders hdr = resp.GetHeaderCollection();
			THTTPHdrVal len;
			if (hdr.GetField(sp.StringF(HTTP::EContentLength, RHTTPSession::GetTable()), 0, len) == KErrNone)
				{
				if (len.Type() == THTTPHdrVal::KTIntVal)
					{
					iContentLength = len.Int();
					}
				}
			if (iMode == EDownloadFile)
				{
				iObserver.HandleDownloadProgress(0, iContentLength);
				}
			}
			break;

		case THTTPEvent::EGotResponseBodyData:
			{
			MHTTPDataSupplier* body = aTransaction.Response().Body();
			TPtrC8 ptr;
			body->GetNextDataPart(ptr);
			if (iMode == EFetchList || iMode == EFetchOnline)
				{
				AppendBodyL(ptr);
				}
			else if (iMode == EDownloadFile)
				{
				WriteBodyToFileL(ptr);
				}
			body->ReleaseData();
			}
			break;

		case THTTPEvent::EResponseComplete:
			break;

		case THTTPEvent::ESucceeded:
			{
			TMode finishedMode = iMode;
			HBufC* finishedPath = NULL;
			if (finishedMode == EDownloadFile && iLocalPath)
				{
				finishedPath = iLocalPath->AllocL();
				}

			if (finishedMode == EFetchList)
				{
				ParseRemoteListAndNotifyL();
				Cleanup();
				}
			else if (finishedMode == EFetchOnline)
				{
				// Hand the raw server text to the UI (it usually says
				// "Pobrano! Odswiez /music" on success or an error string).
				HBufC8* reply = NULL;
				if (iBodyBuf)
					{
					reply = iBodyBuf->AllocL();
					}
				Cleanup();
				if (reply)
					{
					CleanupStack::PushL(reply);
					iObserver.HandleOnlineFetchFinishedL(*reply, KErrNone);
					CleanupStack::PopAndDestroy(reply);
					}
				else
					{
					iObserver.HandleOnlineFetchFinishedL(KNullDesC8, KErrNone);
					}
				}
			else if (finishedMode == EDownloadFile)
				{
				if (iFileOpen)
					{
					iFile.Close();
					iFileOpen = EFalse;
					}
				Cleanup();
				if (finishedPath)
					{
					CleanupStack::PushL(finishedPath);
					iObserver.HandleDownloadFinishedL(*finishedPath, KErrNone);
					CleanupStack::PopAndDestroy(finishedPath);
					}
				}
			else
				{
				Cleanup();
				}
			}
			break;

		case THTTPEvent::EFailed:
			{
			TMode failedMode = iMode;

			// Try to surface the real reason instead of the opaque -2.
			// If the server actually replied, use its status code (e.g. 404,
			// 500); otherwise fall back to KErrGeneral.
			TInt err = KErrGeneral;
			TInt status = 0;
			TRAPD(rcErr, status = aTransaction.Response().StatusCode());
			if (rcErr == KErrNone && status > 0)
				{
				err = status;
				}

			Cleanup();
			if (failedMode == EDownloadFile)
				{
				iObserver.HandleDownloadFinishedL(KNullDesC, err);
				}
			else if (failedMode == EFetchOnline)
				{
				iObserver.HandleOnlineFetchFinishedL(KNullDesC8, err);
				}
			else
				{
				iObserver.HandleHttpError(err);
				}
			}
			break;

		default:
			break;
		}
	}

TInt CTurboSoundDownloader::MHFRunError(TInt aError, RHTTPTransaction /*aTransaction*/, const THTTPEvent& /*aEvent*/)
	{
	TMode failedMode = iMode;
	Cleanup();
	if (failedMode == EDownloadFile)
		{
		TRAP_IGNORE(iObserver.HandleDownloadFinishedL(KNullDesC, aError));
		}
	else if (failedMode == EFetchOnline)
		{
		TRAP_IGNORE(iObserver.HandleOnlineFetchFinishedL(KNullDesC8, aError));
		}
	else
		{
		iObserver.HandleHttpError(aError);
		}
	return KErrNone;
	}
