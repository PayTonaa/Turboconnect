/*
 ============================================================================
 Name		: TurboSoundAppUi.h
 ============================================================================
 */

#ifndef __TURBOSOUNDAPPUI_h__
#define __TURBOSOUNDAPPUI_h__

#include <e32base.h>
#include <aknappui.h>
#include "TurboSoundAppView.h"
#include "TurboSoundDownloader.h"
#include "TurboSoundSimplePlayer.h"

class CTurboSoundRemCon;

class CTurboSoundAppUi
	: public CAknAppUi,
	  public MTurboSoundTrackObserver,
	  public MTurboSoundDownloadObserver,
	  public MTurboSoundPlaybackObserver,
	  public MTurboSoundNowPlayingKeys
	{
public:
	void ConstructL();
	CTurboSoundAppUi();
	virtual ~CTurboSoundAppUi();

private:
	enum TListMode { EModeLocal, EModeServer };

	// from CEikAppUi
	void HandleCommandL(TInt aCommand);
	void HandleStatusPaneSizeChange();
	void HandleForegroundEventL(TBool aForeground);
	CArrayFix<TCoeHelpContext>* HelpContextL() const;

	// from MTurboSoundTrackObserver
	void HandleTrackSelectedL(TInt aIndex);

	// from MTurboSoundDownloadObserver
	void HandleRemoteTracksReadyL(const MDesCArray& aTitles);
	void HandleOnlineFetchFinishedL(const TDesC8& aResult, TInt aError);
	void HandleDownloadProgress(TInt aBytesDone, TInt aBytesTotal);
	void HandleDownloadFinishedL(const TDesC& aLocalFile, TInt aError);
	void HandleHttpError(TInt aError);
	void HandleNetStep(const TDesC& aMsg);

	void RefreshLocalListL();
	void FetchServerListL();
	void FetchOnlineL();
	void SetMode(TListMode aMode);

	void SearchTracksL();
	void ClearSearchL();
	void ApplyFilteredListL();
	void PlayNeighbourL(TInt aDelta);
	void TogglePlayPauseL();
	TInt MapFilteredToReal(TInt aFilteredIndex) const;
	TInt MapRealToFiltered(TInt aRealIndex) const;

	// from MTurboSoundPlaybackObserver
	void HandlePlaybackStartedL(TInt aIndex, const TDesC& aDisplayName);
	void HandlePlaybackCompletedL(TInt aIndex, TInt aError);
	void HandlePlaybackErrorL(TInt aError);

	// from MTurboSoundNowPlayingKeys
	void HandleNpStopL();
	void HandleNpPauseToggleL();
	void HandleNpPrevL();
	void HandleNpNextL();
	void HandleNpSeekRelativeL(TInt aDeltaSeconds);
	void HandleNpDismissL();
	TBool HandleNpVolumeDeltaL(TInt aDelta);

	void TickPlayerUiL();
	void StartPlayerUiTimer();
	void StopPlayerUiTimer();
	void HideNowPlayingUiL();

	void SaveSessionSnapshotL();
	void ClearSessionStoreL();
	void LoadResumeHintL();
	void TryShowNowPlayingAfterForegroundL();
	void ShowNowPlayingPanelL();
	void ResumeLastSessionL();

	static TInt PlayerUiTickStatic(TAny* aPtr);

private:
	CTurboSoundAppView* iAppView;
	CTurboSoundSimplePlayer* iMusic;
	CTurboSoundDownloader* iDownloader;

	TListMode iMode;

	HBufC* iSearchFilter;          // aktualny filtr (lowercase), NULL = brak filtra
	RArray<TInt> iFilteredIndex;   // mapa: indeks w widoku -> indeks w iMusic
	TInt iCurrentTrack;            // ostatnio uruchomiony utwor (realny indeks w iMusic)

	CPeriodic* iPlayerUiTick;      // odswiezanie paska postepu panelu "Teraz odtwarzane"
	TBool iNpUserDismissed;        // uzytkownik schowal panel (strzalka gora) — nie pokazuj przy autoplay

	TInt iResumeTrackIndex;        // z pliku sesji, -1 = brak
	TInt iResumePositionSec;
	TInt iPendingSeekToSec;        // >=0: przewin po starcie; -1: brak

	// Window-group capture dla bocznych Vol+/- (E75): zeby trafialy do aplikacji.
	TInt32 iCapVolUp;
	TInt32 iCapVolDn;

	// RemCon target — gdzie naprawde leca boczne Vol+/- z E75.
	CTurboSoundRemCon* iRemCon;
	};

#endif // __TURBOSOUNDAPPUI_h__
