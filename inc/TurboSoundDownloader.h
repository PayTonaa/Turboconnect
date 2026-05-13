/*
 ============================================================================
 Name        : TurboSoundDownloader.h
 Description : HTTP downloader for the turboconnect server. Two operations:
               (1) fetch list of remote tracks, (2) download one to disk.
 ============================================================================
 */

#ifndef TURBOSOUNDDOWNLOADER_H
#define TURBOSOUNDDOWNLOADER_H

#include <e32base.h>
#include <f32file.h>
#include <bamdesca.h>
#include <badesca.h>
#include <es_sock.h>
#include <in_sock.h>
#include <http/mhttptransactioncallback.h>
#include <http/rhttpsession.h>
#include <http/rhttptransaction.h>

class MTurboSoundDownloadObserver
	{
public:
	/** List of remote tracks ready (display titles). */
	virtual void HandleRemoteTracksReadyL(const MDesCArray& aTitles) = 0;

	/** /api/fetch finished. aResult is the raw server reply (UTF-8). */
	virtual void HandleOnlineFetchFinishedL(const TDesC8& aResult, TInt aError) = 0;

	/** Bytes received during download (aTotal == 0 if unknown). */
	virtual void HandleDownloadProgress(TInt aBytesDone, TInt aBytesTotal) = 0;

	/** Download finished. aError == KErrNone on success. */
	virtual void HandleDownloadFinishedL(const TDesC& aLocalFile, TInt aError) = 0;

	/** Generic transport error (DNS, connect, HTTP failed, etc.). */
	virtual void HandleHttpError(TInt aError) = 0;

	/** Diagnostic step from the network setup, shown in the status bar. */
	virtual void HandleNetStep(const TDesC& aMsg) = 0;
	};

NONSHARABLE_CLASS(CTurboSoundDownloader) : public CBase, public MHTTPTransactionCallback
	{
public:
	static CTurboSoundDownloader* NewL(MTurboSoundDownloadObserver& aObserver);
	~CTurboSoundDownloader();

	/** GET <aBaseUrl>/api/search_plain?q=<aQuery> and parse the lines. */
	void FetchTrackListL(const TDesC8& aBaseUrl, const TDesC8& aQuery);

	/**
	 * Ask the server to fetch a new track from the internet via yt-dlp.
	 * Issues GET <aBaseUrl>/api/fetch?q=<url-encoded aQuery>. On success the
	 * observer's HandleRemoteTracksReadyL is called with a fresh server list
	 * (auto-refresh of the library after download finishes server-side).
	 */
	void FetchOnlineL(const TDesC8& aBaseUrl, const TDesC& aQuery);

	/** Download remote track aIndex into aTargetFolder (folder must end with backslash). */
	void DownloadByIndexL(TInt aIndex, const TDesC8& aBaseUrl, const TDesC& aTargetFolder);

	/** Drop any in-flight transaction and free temporary buffers. */
	void CancelOperation();

	TInt RemoteTrackCount() const;
	const TDesC& RemoteFilename(TInt aIndex) const;

private: // MHTTPTransactionCallback
	void MHFRunL(RHTTPTransaction aTransaction, const THTTPEvent& aEvent);
	TInt MHFRunError(TInt aError, RHTTPTransaction aTransaction, const THTTPEvent& aEvent);

private:
	CTurboSoundDownloader(MTurboSoundDownloadObserver& aObserver);
	void ConstructL();

	void EnsureConnectedL();
	void StartGetL(const TDesC8& aUrl);
	void AppendBodyL(const TDesC8& aChunk);
	void WriteBodyToFileL(const TDesC8& aChunk);
	void ParseRemoteListAndNotifyL();
	void Cleanup();

private:
	enum TMode { EIdle, EFetchList, EDownloadFile, EFetchOnline };

	MTurboSoundDownloadObserver& iObserver;

	RSocketServ iSocketServ;
	TBool iSocketServOpen;
	RConnection iConnection;
	TBool iConnectionOpen;

	RHTTPSession iSession;
	TBool iSessionOpen;
	RHTTPTransaction iTransaction;
	TBool iTransactionActive;

	TMode iMode;

	/** List mode: accumulate response body. */
	HBufC8* iBodyBuf;

	/** Download mode: open file, byte counters and target path. */
	RFile iFile;
	TBool iFileOpen;
	HBufC* iLocalPath;
	TInt iBytesDone;
	TInt iContentLength;

	/** Last fetched remote list. */
	CDesCArrayFlat* iRemoteTitles;
	CDesCArrayFlat* iRemoteFilenames;
	CDesCArrayFlat* iRemoteUrls; // 8-bit stored as 16-bit for simplicity (ASCII)
	};

#endif
