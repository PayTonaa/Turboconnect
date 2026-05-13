/*
 ============================================================================
 Name		: TurboSoundAppUi.cpp
 ============================================================================
 */

#include <avkon.hrh>
#include <aknmessagequerydialog.h>
#include <aknnotewrappers.h>
#include <aknquerydialog.h>
#include <stringloader.h>
#include <coemain.h>
#include <f32file.h>
#include <s32file.h>
#include <badesca.h>
#include <e32keys.h>
#include <w32std.h>

#include <TurboSound_0xE650F19F.rsg>

#include "TurboSound.hrh"
#include "TurboSound.pan"
#include "TurboSoundApplication.h"
#include "TurboSoundAppUi.h"
#include "TurboSoundAppView.h"
#include "TurboSoundSimplePlayer.h"
#include "TurboSoundDownloader.h"
#include "TurboSoundRemCon.h"

// Domyslny folder z muzyka. Mozna nadpisac plikiem E:\\Muzyka\\turbosound.cfg
// (jedna linia ASCII/Unicode z bezwzgledna sciezka zakonczona '\\').
_LIT( KMusicFolderDefault, "E:\\Muzyka\\" );
_LIT( KMusicFolderLegacy,  "E:\\Sounds\\TurboSound\\" );
_LIT( KMusicConfigFile,    "E:\\Muzyka\\turbosound.cfg" );
_LIT( KMusicConfigFileAlt, "C:\\Data\\turbosound.cfg" );
// Plain HTTP on purpose. Even with CACerts-2021-update + networking_improvements
// installed, Nokia E75's TLS stack (9.2 FP2) cannot complete a TLS 1.2
// handshake against modern hosts (no SNI, no modern ciphers). Confirmed by
// the stock browser failing on https://turboconect.pl too. The Flask backend
// must be reachable on HTTP without forced redirect to HTTPS.
_LIT8( KServerBaseUrl, "http://turboconect.pl" );

_LIT( KSessionStore, "C:\\Data\\turbosound.sess" );
const TInt KSessionMagic =
		(TInt)('T' | ('S' << 8) | ('s' << 16) | ('2' << 24));
const TInt KSessionVer = 1;

static void AppendLe32(TDes8& aBuf, TInt aVal)
	{
	for (TInt i = 0; i < 4; i++)
		{
		aBuf.Append((TUint8)(aVal & 0xFF));
		aVal >>= 8;
		}
	}

static TInt ReadLe32(const TUint8* aBase, TInt aOff)
	{
	return (TInt)(
			(TUint32)aBase[aOff]
			| ((TUint32)aBase[aOff + 1] << 8)
			| ((TUint32)aBase[aOff + 2] << 16)
			| ((TUint32)aBase[aOff + 3] << 24));
	}

static TInt UsToSec(const TTimeIntervalMicroSeconds& aUs)
	{
	const TInt64 v = aUs.Int64();
	if (v <= 0)
		{
		return 0;
		}
	return static_cast<TInt>(v / 1000000);
	}

TInt CTurboSoundAppUi::PlayerUiTickStatic(TAny* aPtr)
	{
	CTurboSoundAppUi* self = reinterpret_cast<CTurboSoundAppUi*>(aPtr);
	TRAP_IGNORE(self->TickPlayerUiL());
	return 1;
	}

void CTurboSoundAppUi::ConstructL()
	{
	BaseConstructL(CAknAppUi::EAknEnableSkin);

	iPlayerUiTick = CPeriodic::NewL(CActive::EPriorityStandard);

	iMusic = CTurboSoundSimplePlayer::NewL();
	iMusic->SetObserver(this);
	iDownloader = CTurboSoundDownloader::NewL(*this);

	iAppView = CTurboSoundAppView::NewL(ClientRect(), *this);
	iAppView->SetNowPlayingKeysObserver(this);
	AddToStackL(iAppView);

	// Wymus dostarczanie bocznych Vol+/- do aplikacji (rozne S60 3rd FP2 routuja
	// te klawisze przez global key handler; CaptureKey przejmuje je na nasz wg).
	if (iCoeEnv)
		{
		TRAP_IGNORE(iCapVolUp = iCoeEnv->RootWin().CaptureKey(EKeyIncVolume, 0, 0));
		TRAP_IGNORE(iCapVolDn = iCoeEnv->RootWin().CaptureKey(EKeyDecVolume, 0, 0));
		}

	// Glowne zrodlo bocznych Vol+/- na Nokia E-series: RemConCoreApi (target).
	// Bez tego boczne klawisze nie dochodza do "zwyklych" aplikacji.
	TRAP_IGNORE(iRemCon = CTurboSoundRemCon::NewL(*this));

	SetMode(EModeLocal);
	TRAP_IGNORE(RefreshLocalListL());
	}

CTurboSoundAppUi::CTurboSoundAppUi()
	: iAppView(NULL), iMusic(NULL), iDownloader(NULL),
	  iMode(EModeLocal), iSearchFilter(NULL), iCurrentTrack(-1),
	  iPlayerUiTick(NULL), iNpUserDismissed(EFalse),
	  iResumeTrackIndex(-1), iResumePositionSec(0), iPendingSeekToSec(-1),
	  iCapVolUp(0), iCapVolDn(0), iRemCon(NULL)
	{
	}

CTurboSoundAppUi::~CTurboSoundAppUi()
	{
	delete iRemCon;
	iRemCon = NULL;
	if (iCoeEnv)
		{
		if (iCapVolUp > 0)
			{
			iCoeEnv->RootWin().CancelCaptureKey(iCapVolUp);
			iCapVolUp = 0;
			}
		if (iCapVolDn > 0)
			{
			iCoeEnv->RootWin().CancelCaptureKey(iCapVolDn);
			iCapVolDn = 0;
			}
		}
	StopPlayerUiTimer();
	delete iPlayerUiTick;
	iPlayerUiTick = NULL;
	if (iAppView)
		{
		RemoveFromStack(iAppView);
		delete iAppView;
		iAppView = NULL;
		}
	delete iDownloader;
	iDownloader = NULL;
	if (iMusic)
		{
		iMusic->SetObserver(NULL);
		TRAP_IGNORE(SaveSessionSnapshotL());
		iMusic->Stop();
		}
	delete iMusic;
	iMusic = NULL;
	delete iSearchFilter;
	iSearchFilter = NULL;
	iFilteredIndex.Close();
	}

void CTurboSoundAppUi::SetMode(TListMode aMode)
	{
	iMode = aMode;
	if (!iAppView)
		{
		return;
		}
	TRAP_IGNORE(iAppView->SetStatusL(
			aMode == EModeLocal ? _L("Local library") : _L("Server library")));
	}

static TBool ReadConfiguredFolderL(TDes& aOut)
	{
	aOut.Zero();
	RFs& fs = CCoeEnv::Static()->FsSession();
	const TDesC* candidates[] = { &KMusicConfigFile, &KMusicConfigFileAlt };
	for (TInt c = 0; c < (TInt) (sizeof(candidates) / sizeof(candidates[0])); ++c)
		{
		RFile f;
		if (f.Open(fs, *candidates[c], EFileRead | EFileShareReadersOnly) != KErrNone)
			{
			continue;
			}
		TBuf8<260> raw;
		TInt rerr = f.Read(raw);
		f.Close();
		if (rerr != KErrNone || raw.Length() == 0)
			{
			continue;
			}
		// strip BOM / CR / LF / whitespace
		if (raw.Length() >= 3
			&& (TUint8) raw[0] == 0xEF
			&& (TUint8) raw[1] == 0xBB
			&& (TUint8) raw[2] == 0xBF)
			{
			raw.Delete(0, 3);
			}
		for (TInt i = raw.Length() - 1; i >= 0; --i)
			{
			TChar ch(raw[i]);
			if (ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t')
				{
				raw.Delete(i, 1);
				}
			else
				{
				break;
				}
			}
		TInt cut = raw.Locate('\n');
		if (cut == KErrNotFound) cut = raw.Locate('\r');
		if (cut != KErrNotFound) raw.SetLength(cut);
		if (raw.Length() == 0 || raw.Length() > aOut.MaxLength())
			{
			continue;
			}
		aOut.Copy(raw);
		if (aOut.Length() > 0 && aOut[aOut.Length() - 1] != '\\')
			{
			if (aOut.Length() < aOut.MaxLength())
				{
				aOut.Append(_L("\\"));
				}
			}
		return ETrue;
		}
	return EFalse;
	}

void CTurboSoundAppUi::RefreshLocalListL()
	{
	if (!iMusic || !iAppView)
		{
		return;
		}

	TFileName folder;
	if (!ReadConfiguredFolderL(folder))
		{
		folder.Copy(KMusicFolderDefault);
		}

	iMusic->ScanFolderL(folder);
	if (iMusic->TrackCount() == 0)
		{
		// fallback: stary folder z TurboSound 1.x
		iMusic->ScanFolderL(KMusicFolderLegacy);
		}
	ApplyFilteredListL();
	SetMode(EModeLocal);
	TRAP_IGNORE(LoadResumeHintL());
	}

// Buduje filtrowana liste i wysyla ja do widoku. Jezeli iSearchFilter == NULL,
// pokazujemy wszystkie utwory.
void CTurboSoundAppUi::ApplyFilteredListL()
	{
	if (!iMusic || !iAppView)
		{
		return;
		}
	const MDesCArray& allNames = iMusic->TrackNames();
	const TInt total = allNames.MdcaCount();

	iFilteredIndex.Reset();

	CDesCArrayFlat* filteredNames = new (ELeave) CDesCArrayFlat(8);
	CleanupStack::PushL(filteredNames);

	if (iSearchFilter && iSearchFilter->Length() > 0)
		{
		TBuf<80> needle;
		needle.Copy(iSearchFilter->Left(Min(iSearchFilter->Length(),
		                                    needle.MaxLength())));
		needle.LowerCase();
		for (TInt i = 0; i < total; i++)
			{
			TBuf<256> name;
			const TPtrC src = allNames.MdcaPoint(i);
			const TInt safe = Min(src.Length(), name.MaxLength());
			name.Copy(src.Left(safe));
			name.LowerCase();
			if (name.Find(needle) != KErrNotFound)
				{
				filteredNames->AppendL(allNames.MdcaPoint(i));
				User::LeaveIfError(iFilteredIndex.Append(i));
				}
			}
		TBuf<80> info;
		info.Format(_L("Filtr: %S  (%d/%d)"), iSearchFilter,
		            filteredNames->MdcaCount(), total);
		iAppView->SetTrackNamesL(*filteredNames);
		iAppView->SetStatusL(info);
		}
	else
		{
		for (TInt i = 0; i < total; i++)
			{
			filteredNames->AppendL(allNames.MdcaPoint(i));
			User::LeaveIfError(iFilteredIndex.Append(i));
			}
		iAppView->SetTrackNamesL(allNames);
		}
	CleanupStack::PopAndDestroy(filteredNames);
	}

TInt CTurboSoundAppUi::MapFilteredToReal(TInt aFilteredIndex) const
	{
	if (aFilteredIndex < 0 || aFilteredIndex >= iFilteredIndex.Count())
		{
		return -1;
		}
	return iFilteredIndex[aFilteredIndex];
	}

TInt CTurboSoundAppUi::MapRealToFiltered(TInt aRealIndex) const
	{
	for (TInt i = 0; i < iFilteredIndex.Count(); i++)
		{
		if (iFilteredIndex[i] == aRealIndex)
			{
			return i;
			}
		}
	return -1;
	}

void CTurboSoundAppUi::TogglePlayPauseL()
	{
	if (!iMusic || iMode != EModeLocal)
		{
		return;
		}

	// Gra -> pauza (panel zostaje do momentu strzalki w gore)
	if (iMusic->IsPlaying())
		{
		iMusic->PauseL();
		if (iAppView)
			{
			iAppView->SetNowPlayingPausedL(ETrue);
			iAppView->SetStatusL(_L("Pauza"));
			if (!iNpUserDismissed)
				{
				if (!iAppView->NowPlayingActive() && iMusic->CurrentIndex() >= 0)
					{
					const MDesCArray& names = iMusic->TrackNames();
					const TInt ix = iMusic->CurrentIndex();
					if (ix >= 0 && ix < names.MdcaCount())
						{
						TRAP_IGNORE(iAppView->ShowNowPlayingL(names.MdcaPoint(ix)));
						}
					}
				StartPlayerUiTimer();
				}
			}
		return;
		}

	// Pauza -> wznow
	if (iMusic->IsPaused())
		{
		iMusic->ResumeL();
		if (iAppView)
			{
			iAppView->SetNowPlayingPausedL(EFalse);
			iAppView->SetStatusL(_L("Wznowiono"));
			if (!iNpUserDismissed)
				{
				StartPlayerUiTimer();
				}
			}
		return;
		}

	// Nic nie gra -> zagraj zaznaczony
	if (iFilteredIndex.Count() > 0)
		{
		TInt visIdx = (iAppView ? iAppView->CurrentItemIndex() : 0);
		if (visIdx < 0) visIdx = 0;
		const TInt realIdx = MapFilteredToReal(visIdx);
		if (realIdx >= 0)
			{
			iNpUserDismissed = EFalse;
			iCurrentTrack = realIdx;
			if (iAppView) iAppView->SetStatusL(_L("Ladowanie utworu..."));
			iPendingSeekToSec = -1;
			iMusic->PlayTrackL(realIdx);
			}
		}
	}

// --- callbacki playera ------------------------------------------------------

void CTurboSoundAppUi::HandlePlaybackStartedL(TInt aIndex, const TDesC& aDisplayName)
	{
	if (!iAppView) return;
	// Sync zaznaczenia listy z aktualnie granym utworem (jezeli widoczny).
	const TInt vis = MapRealToFiltered(aIndex);
	if (vis >= 0)
		{
		iAppView->SetCurrentItemIndex(vis);
		}
	TBuf<160> msg;
	msg.Append(_L("Gram: "));
	const TInt room = msg.MaxLength() - msg.Length();
	const TInt take = (aDisplayName.Length() < room) ? aDisplayName.Length() : room;
	msg.Append(aDisplayName.Left(take));
	iAppView->SetStatusL(msg);
	if (iAppView)
		{
		iAppView->SetNowPlayingPausedL(EFalse);
		}
	if (!iNpUserDismissed)
		{
		TRAP_IGNORE(iAppView->ShowNowPlayingL(aDisplayName));
		StartPlayerUiTimer();
		}
	if (iPendingSeekToSec >= 0 && iMusic && iMusic->HasActiveStream())
		{
		const TInt seekSec = iPendingSeekToSec;
		iPendingSeekToSec = -1;
		iMusic->SeekToSecondsL(seekSec);
		if (iAppView && iAppView->NowPlayingActive())
			{
			TTimeIntervalMicroSeconds pos(0);
			TTimeIntervalMicroSeconds dur(0);
			iMusic->GetPlaybackProgress(pos, dur);
			iAppView->SetNowPlayingProgressL(UsToSec(pos), UsToSec(dur));
			}
		}
	}

void CTurboSoundAppUi::HandlePlaybackCompletedL(TInt /*aIndex*/, TInt aError)
	{
	if (aError != KErrNone && aError != KErrUnderflow)
		{
		iNpUserDismissed = EFalse;
		HideNowPlayingUiL();
		if (iAppView)
			{
			TBuf<48> msg;
			msg.Format(_L("Blad odtwarzania (%d)"), aError);
			iAppView->SetStatusL(msg);
			}
		return;
		}
	// Utwor sie skonczyl -> autoplay kolejnego (jak na stronie)
	TRAP_IGNORE(PlayNeighbourL(+1));
	}

void CTurboSoundAppUi::HandlePlaybackErrorL(TInt aError)
	{
	iNpUserDismissed = EFalse;
	ClearSessionStoreL();
	HideNowPlayingUiL();
	if (iAppView)
		{
		TBuf<48> msg;
		msg.Format(_L("Blad audio (%d)"), aError);
		iAppView->SetStatusL(msg);
		}
	}

void CTurboSoundAppUi::TickPlayerUiL()
	{
	if (!iAppView || !iAppView->NowPlayingActive() || !iMusic
	    || !iMusic->HasActiveStream())
		{
		return;
		}
	TTimeIntervalMicroSeconds pos(0);
	TTimeIntervalMicroSeconds dur(0);
	iMusic->GetPlaybackProgress(pos, dur);
	iAppView->SetNowPlayingProgressL(UsToSec(pos), UsToSec(dur));
	}

void CTurboSoundAppUi::StartPlayerUiTimer()
	{
	if (!iPlayerUiTick)
		{
		return;
		}
	iPlayerUiTick->Cancel();
	iPlayerUiTick->Start(80000, 400000, TCallBack(CTurboSoundAppUi::PlayerUiTickStatic, this));
	}

void CTurboSoundAppUi::StopPlayerUiTimer()
	{
	if (iPlayerUiTick)
		{
		iPlayerUiTick->Cancel();
		}
	}

void CTurboSoundAppUi::HideNowPlayingUiL()
	{
	StopPlayerUiTimer();
	if (iAppView)
		{
		iAppView->HideNowPlayingL();
		}
	}

void CTurboSoundAppUi::HandleNpStopL()
	{
	ClearSessionStoreL();
	iNpUserDismissed = EFalse;
	if (iMusic)
		{
		iMusic->Stop();
		}
	HideNowPlayingUiL();
	if (iAppView)
		{
		iAppView->SetNowPlayingPausedL(EFalse);
		iAppView->SetStatusL(_L("Zatrzymano"));
		}
	}

void CTurboSoundAppUi::HandleNpPauseToggleL()
	{
	// Srodkowy klawisz na panelu: pauza / wznow. Panel NIE jest chowany.
	if (!iMusic || iMode != EModeLocal)
		{
		return;
		}
	if (iMusic->IsPlaying())
		{
		iMusic->PauseL();
		if (iAppView)
			{
			iAppView->SetNowPlayingPausedL(ETrue);
			iAppView->SetStatusL(_L("Pauza"));
			}
		return;
		}
	if (iMusic->IsPaused())
		{
		iMusic->ResumeL();
		if (iAppView)
			{
			iAppView->SetNowPlayingPausedL(EFalse);
			iAppView->SetStatusL(_L("Wznowiono"));
			StartPlayerUiTimer();
			}
		return;
		}
	// Nic nie gra: jak Play z listy.
	TRAP_IGNORE(TogglePlayPauseL());
	}

TBool CTurboSoundAppUi::HandleNpVolumeDeltaL(TInt aDelta)
	{
	if (!iMusic)
		{
		return EFalse;
		}
	if (!iMusic->HasActiveStream())
		{
		return EFalse;
		}
	const TInt mx = iMusic->PlaybackMaxVolume();
	if (mx <= 0)
		{
		return EFalse;
		}
	TInt v = iMusic->PlaybackVolume();
	v += aDelta;
	if (v < 0)
		{
		v = 0;
		}
	if (v > mx)
		{
		v = mx;
		}
	iMusic->SetPlaybackVolume(v);
	if (iAppView)
		{
		const TInt pct = (mx > 0) ? (v * 100) / mx : 0;
		TBuf<32> msg;
		msg.Format(_L("Glosnosc: %d%%"), pct);
		iAppView->SetStatusL(msg);
		}
	return ETrue;
	}

void CTurboSoundAppUi::HandleNpPrevL()
	{
	TRAP_IGNORE(PlayNeighbourL(-1));
	}

void CTurboSoundAppUi::HandleNpNextL()
	{
	TRAP_IGNORE(PlayNeighbourL(+1));
	}

void CTurboSoundAppUi::HandleNpSeekRelativeL(TInt aDeltaSeconds)
	{
	if (!iMusic || iMode != EModeLocal)
		{
		return;
		}
	if (!iMusic->HasActiveStream())
		{
		return;
		}
	iMusic->SeekRelativeSecondsL(aDeltaSeconds);
	if (iAppView && iAppView->NowPlayingActive())
		{
		TTimeIntervalMicroSeconds pos(0);
		TTimeIntervalMicroSeconds dur(0);
		iMusic->GetPlaybackProgress(pos, dur);
		iAppView->SetNowPlayingProgressL(UsToSec(pos), UsToSec(dur));
		}
	}

void CTurboSoundAppUi::HandleNpDismissL()
	{
	iNpUserDismissed = ETrue;
	HideNowPlayingUiL();
	if (iAppView && iMusic && (iMusic->IsPlaying() || iMusic->IsPaused()))
		{
		iAppView->SetStatusL(_L("Panel ukryty (odtwarzanie trwa)"));
		}
	else if (iAppView)
		{
		iAppView->SetStatusL(_L("Powrot do listy"));
		}
	}

void CTurboSoundAppUi::HandleForegroundEventL(TBool aForeground)
	{
	CAknAppUi::HandleForegroundEventL(aForeground);
	if (aForeground)
		{
		TRAP_IGNORE(TryShowNowPlayingAfterForegroundL());
		}
	else
		{
		TRAP_IGNORE(SaveSessionSnapshotL());
		}
	}

void CTurboSoundAppUi::SaveSessionSnapshotL()
	{
	if (!iMusic)
		{
		return;
		}
	RFs& fs = CCoeEnv::Static()->FsSession();
	if (!iMusic->HasActiveStream())
		{
		fs.Delete(KSessionStore);
		return;
		}
	const TInt idx = iMusic->CurrentIndex();
	if (idx < 0)
		{
		return;
		}
	TTimeIntervalMicroSeconds pos(0);
	TTimeIntervalMicroSeconds dur(0);
	iMusic->GetPlaybackProgress(pos, dur);
	const TInt posSec = UsToSec(pos);
	TBuf8<32> blob;
	AppendLe32(blob, KSessionMagic);
	AppendLe32(blob, KSessionVer);
	AppendLe32(blob, iMusic->TrackCount());
	AppendLe32(blob, idx);
	AppendLe32(blob, posSec);
	const TInt flags = iMusic->IsPlaying() ? 1 : (iMusic->IsPaused() ? 2 : 0);
	AppendLe32(blob, flags);
	RFile f;
	TInt err = f.Replace(fs, KSessionStore, EFileWrite | EFileShareExclusive);
	if (err != KErrNone)
		{
		return;
		}
	err = f.Write(blob);
	f.Close();
	}

void CTurboSoundAppUi::ClearSessionStoreL()
	{
	RFs& fs = CCoeEnv::Static()->FsSession();
	fs.Delete(KSessionStore);
	}

void CTurboSoundAppUi::LoadResumeHintL()
	{
	iResumeTrackIndex = -1;
	iResumePositionSec = 0;
	if (!iMusic || !iAppView)
		{
		return;
		}
	RFs& fs = CCoeEnv::Static()->FsSession();
	RFile f;
	if (f.Open(fs, KSessionStore, EFileRead | EFileShareReadersOnly) != KErrNone)
		{
		return;
		}
	TBuf8<64> buf;
	if (f.Read(buf) != KErrNone || buf.Length() < 24)
		{
		f.Close();
		return;
		}
	f.Close();
	const TUint8* d = buf.Ptr();
	if (ReadLe32(d, 0) != KSessionMagic || ReadLe32(d, 4) != KSessionVer)
		{
		fs.Delete(KSessionStore);
		return;
		}
	const TInt savedCount = ReadLe32(d, 8);
	const TInt idx = ReadLe32(d, 12);
	const TInt posSec = ReadLe32(d, 16);
	if (savedCount != iMusic->TrackCount() || idx < 0 || idx >= iMusic->TrackCount())
		{
		fs.Delete(KSessionStore);
		return;
		}
	iResumeTrackIndex = idx;
	iResumePositionSec = posSec;
	const MDesCArray& names = iMusic->TrackNames();
	if (idx < names.MdcaCount())
		{
		const TInt vis = MapRealToFiltered(idx);
		if (vis >= 0)
			{
			iAppView->SetCurrentItemIndex(vis);
			}
		TBuf<180> hint;
		hint.Copy(_L("Sesja: "));
		const TPtrC nm = names.MdcaPoint(idx);
		const TInt room = hint.MaxLength() - hint.Length() - 28;
		if (nm.Length() <= room)
			{
			hint.Append(nm);
			}
		else if (room > 1)
			{
			hint.Append(nm.Left(room));
			}
		hint.Append(_L(" — Opcje: Wznów z pozycji"));
		iAppView->SetStatusL(hint);
		}
	}

void CTurboSoundAppUi::TryShowNowPlayingAfterForegroundL()
	{
	if (!iAppView || !iMusic)
		{
		return;
		}
	if (!iMusic->HasActiveStream())
		{
		return;
		}
	if (!iMusic->IsPlaying() && !iMusic->IsPaused())
		{
		return;
		}
	iNpUserDismissed = EFalse;
	const TDesC* d = iMusic->CurrentDisplayName();
	if (d && d->Length() > 0)
		{
		TRAP_IGNORE(iAppView->ShowNowPlayingL(*d));
		}
	else
		{
		const TInt ix = iMusic->CurrentIndex();
		if (ix >= 0)
			{
			const MDesCArray& names = iMusic->TrackNames();
			if (ix < names.MdcaCount())
				{
				TRAP_IGNORE(iAppView->ShowNowPlayingL(names.MdcaPoint(ix)));
				}
			}
		}
	StartPlayerUiTimer();
	TRAP_IGNORE(TickPlayerUiL());
	}

void CTurboSoundAppUi::ShowNowPlayingPanelL()
	{
	if (!iAppView || !iMusic)
		{
		return;
		}
	if (!iMusic->HasActiveStream())
		{
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(_L("Brak aktywnego utworu."));
		return;
		}
	const TDesC* d = iMusic->CurrentDisplayName();
	if (d && d->Length() > 0)
		{
		iNpUserDismissed = EFalse;
		TRAP_IGNORE(iAppView->ShowNowPlayingL(*d));
		StartPlayerUiTimer();
		TRAP_IGNORE(TickPlayerUiL());
		return;
		}
	const TInt ix = iMusic->CurrentIndex();
	if (ix >= 0)
		{
		const MDesCArray& names = iMusic->TrackNames();
		if (ix < names.MdcaCount())
			{
			iNpUserDismissed = EFalse;
			TRAP_IGNORE(iAppView->ShowNowPlayingL(names.MdcaPoint(ix)));
			StartPlayerUiTimer();
			TRAP_IGNORE(TickPlayerUiL());
			return;
			}
		}
	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	note->ExecuteLD(_L("Brak aktywnego utworu."));
	}

void CTurboSoundAppUi::ResumeLastSessionL()
	{
	if (!iMusic || iMode != EModeLocal)
		{
		return;
		}
	if (iResumeTrackIndex < 0)
		{
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(_L("Brak zapisanej sesji."));
		return;
		}
	if (iResumeTrackIndex >= iMusic->TrackCount())
		{
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(_L("Sesja nie pasuje do biblioteki."));
		ClearSessionStoreL();
		iResumeTrackIndex = -1;
		return;
		}
	const TInt vis = MapRealToFiltered(iResumeTrackIndex);
	if (vis >= 0 && iAppView)
		{
		iAppView->SetCurrentItemIndex(vis);
		}
	iCurrentTrack = iResumeTrackIndex;
	iPendingSeekToSec = iResumePositionSec;
	iNpUserDismissed = EFalse;
	if (iAppView)
		{
		iAppView->SetStatusL(_L("Wznawianie sesji..."));
		}
	iMusic->PlayTrackL(iResumeTrackIndex);
	}

void CTurboSoundAppUi::SearchTracksL()
	{
	if (!iMusic || !iAppView)
		{
		return;
		}
	TBuf<60> query;
	if (iSearchFilter)
		{
		query.Copy(iSearchFilter->Left(Min(iSearchFilter->Length(),
		                                   query.MaxLength())));
		}
	CAknTextQueryDialog* dlg = CAknTextQueryDialog::NewL(query);
	dlg->PrepareLC(R_SEARCH_QUERY_DIALOG);
	TInt button = dlg->RunLD();
	if (button == 0)
		{
		return;
		}
	query.Trim();
	delete iSearchFilter;
	iSearchFilter = NULL;
	if (query.Length() > 0)
		{
		iSearchFilter = query.AllocL();
		}
	ApplyFilteredListL();
	}

void CTurboSoundAppUi::ClearSearchL()
	{
	if (!iSearchFilter)
		{
		return;
		}
	delete iSearchFilter;
	iSearchFilter = NULL;
	ApplyFilteredListL();
	if (iAppView)
		{
		iAppView->SetStatusL(_L("Filtr wyczyszczony"));
		}
	}

void CTurboSoundAppUi::PlayNeighbourL(TInt aDelta)
	{
	if (!iMusic || iFilteredIndex.Count() == 0)
		{
		return;
		}
	const TInt visibleCount = iFilteredIndex.Count();
	// Szukamy aktualnego utworu w widocznej liscie (po realnym indeksie)
	TInt visibleIdx = -1;
	if (iCurrentTrack >= 0)
		{
		for (TInt i = 0; i < visibleCount; i++)
			{
			if (iFilteredIndex[i] == iCurrentTrack)
				{
				visibleIdx = i;
				break;
				}
			}
		}
	if (visibleIdx < 0)
		{
		visibleIdx = (aDelta > 0) ? -1 : visibleCount;
		}
	TInt nextVisible = visibleIdx + aDelta;
	if (nextVisible < 0) nextVisible = visibleCount - 1;
	if (nextVisible >= visibleCount) nextVisible = 0;

	const TInt realIdx = iFilteredIndex[nextVisible];
	iCurrentTrack = realIdx;
	if (iAppView)
		{
		iAppView->SetCurrentItemIndex(nextVisible);
		iAppView->SetStatusL(_L("Ladowanie utworu..."));
		}
	iPendingSeekToSec = -1;
	iMusic->PlayTrackL(realIdx);
	}

void CTurboSoundAppUi::FetchServerListL()
	{
	if (!iDownloader || !iAppView)
		{
		return;
		}
	iAppView->SetStatusL(_L("Server library: click OK"));
	iDownloader->FetchTrackListL(KServerBaseUrl, KNullDesC8);
	}

void CTurboSoundAppUi::FetchOnlineL()
	{
	if (!iDownloader || !iAppView)
		{
		return;
		}

	TBuf<80> query;
	CAknTextQueryDialog* dlg = CAknTextQueryDialog::NewL(query);
	dlg->PrepareLC(R_FETCH_QUERY_DIALOG);
	TInt button = dlg->RunLD();
	if (button == 0)
		{
		return;
		}
	query.Trim();
	if (query.Length() == 0)
		{
		return;
		}

	iAppView->SetStatusL(_L("Pobieranie z YouTube..."));
	iDownloader->FetchOnlineL(KServerBaseUrl, query);
	}

void CTurboSoundAppUi::HandleNetStep(const TDesC& aMsg)
	{
	if (iAppView)
		{
		TRAP_IGNORE(iAppView->SetStatusL(aMsg));
		}

	// Best-effort line-log to E:\TurboSoundLog.txt so we still know what
	// happened even if the UI freezes or exits before showing the message.
	RFs& fs = CCoeEnv::Static()->FsSession();
	RFile f;
	TInt err = f.Open(fs, _L("E:\\TurboSoundLog.txt"),
			EFileWrite | EFileShareAny);
	if (err == KErrNotFound)
		{
		err = f.Create(fs, _L("E:\\TurboSoundLog.txt"),
				EFileWrite | EFileShareAny);
		}
	if (err != KErrNone)
		{
		return;
		}
	TInt size = 0;
	f.Size(size);
	f.Seek(ESeekStart, size);
	TBuf8<128> line;
	for (TInt i = 0; i < aMsg.Length() && line.Length() < 120; i++)
		{
		line.Append(static_cast<TUint8>(aMsg[i]));
		}
	line.Append(_L8("\r\n"));
	f.Write(line);
	f.Close();
	}

void CTurboSoundAppUi::HandleTrackSelectedL(TInt aIndex)
	{
	if (iMode == EModeLocal)
		{
		if (iMusic)
			{
			const TInt realIdx = MapFilteredToReal(aIndex);
			if (realIdx >= 0)
				{
				iNpUserDismissed = EFalse;
				iCurrentTrack = realIdx;
				if (iAppView)
					{
					iAppView->SetStatusL(_L("Ladowanie utworu..."));
					}
				iPendingSeekToSec = -1;
				iMusic->PlayTrackL(realIdx);
				}
			}
		return;
		}

	// Server mode: download chosen track to local folder.
	if (iDownloader)
		{
		iAppView->SetStatusL(_L("Starting download..."));
		TFileName targetFolder;
		if (!ReadConfiguredFolderL(targetFolder))
			{
			targetFolder.Copy(KMusicFolderDefault);
			}
		// upewnij sie, ze folder istnieje (downloader oczekuje gotowej sciezki)
		RFs& fs = CCoeEnv::Static()->FsSession();
		TInt mk = fs.MkDirAll(targetFolder);
		if (mk != KErrNone && mk != KErrAlreadyExists)
			{
			// jako fallback - stary folder TurboSound
			targetFolder.Copy(KMusicFolderLegacy);
			fs.MkDirAll(targetFolder);
			}
		TRAPD(err, iDownloader->DownloadByIndexL(aIndex, KServerBaseUrl, targetFolder));
		if (err != KErrNone)
			{
			iAppView->SetStatusL(_L("Download error"));
			}
		}
	}

void CTurboSoundAppUi::HandleRemoteTracksReadyL(const MDesCArray& aTitles)
	{
	if (!iAppView)
		{
		return;
		}
	iAppView->SetTrackNamesL(aTitles);
	SetMode(EModeServer);
	if (aTitles.MdcaCount() == 0)
		{
		iAppView->SetStatusL(_L("Server: no tracks"));
		}
	}

void CTurboSoundAppUi::HandleOnlineFetchFinishedL(const TDesC8& aResult, TInt aError)
	{
	if (!iAppView)
		{
		return;
		}
	if (aError != KErrNone)
		{
		TBuf<48> msg;
		msg.Format(_L("Fetch error %d"), aError);
		iAppView->SetStatusL(msg);
		return;
		}

	// Show the first ~40 chars of the server reply in the status bar.
	TBuf<64> short16;
	const TInt take = Min(aResult.Length(), 40);
	for (TInt i = 0; i < take; i++)
		{
		short16.Append(static_cast<TUint16>(aResult[i]));
		}
	if (short16.Length() == 0)
		{
		short16.Copy(_L("Pobrano. Odswiezam..."));
		}
	iAppView->SetStatusL(short16);

	// Auto-refresh the server list so the new track shows up.
	TRAP_IGNORE(FetchServerListL());
	}

void CTurboSoundAppUi::HandleDownloadProgress(TInt aBytesDone, TInt aBytesTotal)
	{
	if (!iAppView)
		{
		return;
		}
	TBuf<48> status;
	if (aBytesTotal > 0)
		{
		status.Format(_L("Downloading %d / %d B"), aBytesDone, aBytesTotal);
		}
	else
		{
		status.Format(_L("Downloading %d B"), aBytesDone);
		}
	TRAP_IGNORE(iAppView->SetStatusL(status));
	}

void CTurboSoundAppUi::HandleDownloadFinishedL(const TDesC& aLocalFile, TInt aError)
	{
	if (!iAppView)
		{
		return;
		}
	if (aError != KErrNone)
		{
		TBuf<48> msg;
		msg.Format(_L("Download error %d"), aError);
		iAppView->SetStatusL(msg);
		return;
		}
	(void)aLocalFile;
	iAppView->SetStatusL(_L("Saved. Refreshing..."));
	RefreshLocalListL();
	}

void CTurboSoundAppUi::HandleHttpError(TInt aError)
	{
	if (!iAppView)
		{
		return;
		}
	TBuf<48> msg;
	msg.Format(_L("HTTP error %d"), aError);
	TRAP_IGNORE(iAppView->SetStatusL(msg));
	}

void CTurboSoundAppUi::HandleCommandL(TInt aCommand)
	{
	switch (aCommand)
		{
		case EEikCmdExit:
		case EAknSoftkeyExit:
			Exit();
			break;

		case EAbout:
			{
			CAknMessageQueryDialog* dlg = new (ELeave) CAknMessageQueryDialog();
			dlg->PrepareLC(R_ABOUT_QUERY_DIALOG);
			HBufC* title = iEikonEnv->AllocReadResourceLC(R_ABOUT_DIALOG_TITLE);
			dlg->QueryHeading()->SetTextL(*title);
			CleanupStack::PopAndDestroy();
			HBufC* msg = iEikonEnv->AllocReadResourceLC(R_ABOUT_DIALOG_TEXT);
			dlg->SetMessageTextL(*msg);
			CleanupStack::PopAndDestroy();
			dlg->RunLD();
			}
			break;

		case ECmdRefresh:
			{
			TRAPD(err, RefreshLocalListL());
			if (err != KErrNone)
				{
				CAknInformationNote* note = new (ELeave) CAknInformationNote;
				note->ExecuteLD(_L("Cannot read folder"));
				}
			}
			break;

		case ECmdServerList:
			{
			TRAPD(err, FetchServerListL());
			if (err != KErrNone)
				{
				TBuf<64> msg;
				msg.Format(_L("Cannot start HTTP (%d)"), err);
				CAknInformationNote* note = new (ELeave) CAknInformationNote;
				note->ExecuteLD(msg);
				}
			}
			break;

		case ECmdFetchOnline:
			{
			TRAPD(err, FetchOnlineL());
			if (err != KErrNone)
				{
				TBuf<64> msg;
				msg.Format(_L("Fetch error (%d)"), err);
				CAknInformationNote* note = new (ELeave) CAknInformationNote;
				note->ExecuteLD(msg);
				}
			}
			break;

		case ECmdPlay:
			{
			TRAP_IGNORE(TogglePlayPauseL());
			}
			break;

		case ECmdShowNpPanel:
			TRAP_IGNORE(ShowNowPlayingPanelL());
			break;

		case ECmdResumeLast:
			TRAP_IGNORE(ResumeLastSessionL());
			break;

		case ECmdKeyDiag:
			if (iAppView)
				{
				iAppView->EnableKeyDiagnostic();
				}
			break;

		case ECmdStop:
			{
			ClearSessionStoreL();
			iNpUserDismissed = EFalse;
			if (iMusic)
				{
				iMusic->Stop();
				}
			HideNowPlayingUiL();
			if (iAppView) iAppView->SetStatusL(_L("Zatrzymano"));
			}
			break;

		case ECmdNext:
			TRAP_IGNORE(PlayNeighbourL(+1));
			break;

		case ECmdPrev:
			TRAP_IGNORE(PlayNeighbourL(-1));
			break;

		case ECmdSearch:
			{
			TRAPD(err, SearchTracksL());
			if (err != KErrNone && iAppView)
				{
				iAppView->SetStatusL(_L("Blad wyszukiwania"));
				}
			}
			break;

		case ECmdSearchClear:
			TRAP_IGNORE(ClearSearchL());
			break;

		default:
			break;
		}
	}

void CTurboSoundAppUi::HandleStatusPaneSizeChange()
	{
	if (iAppView)
		{
		iAppView->SetRect(ClientRect());
		}
	}

CArrayFix<TCoeHelpContext>* CTurboSoundAppUi::HelpContextL() const
	{
	return NULL;
	}
