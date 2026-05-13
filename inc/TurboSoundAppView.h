/*
 ============================================================================
 Name		: TurboSoundAppView.h
 Description : Track list with status bar (Avkon single-style listbox).
 ============================================================================
 */

#ifndef __TURBOSOUNDAPPVIEW_h__
#define __TURBOSOUNDAPPVIEW_h__

#include <coecntrl.h>
#include <bamdesca.h>

class CAknSingleStyleListBox;

class MTurboSoundTrackObserver
	{
public:
	virtual void HandleTrackSelectedL(TInt aIndex) = 0;
	};

/** Sterowanie panelem "Teraz odtwarzane" (klawisze). */
class MTurboSoundNowPlayingKeys
	{
public:
	virtual void HandleNpStopL() = 0;
	/** Srodkowy klawisz / Enter: pauza / wznow. Panel NIE jest chowany. */
	virtual void HandleNpPauseToggleL() = 0;
	/** Strzalki L/R: poprzedni / nastepny utwor. */
	virtual void HandleNpPrevL() = 0;
	virtual void HandleNpNextL() = 0;
	/** Skok w sekundach (np. klawisze 4/6 na panelu). */
	virtual void HandleNpSeekRelativeL(TInt aDeltaSeconds) = 0;
	virtual void HandleNpDismissL() = 0;
	/** Zmiana glosnosci o aDelta (np. +/-1). ETrue gdy zastosowano. */
	virtual TBool HandleNpVolumeDeltaL(TInt aDelta) = 0;
	};

class CTurboSoundAppView : public CCoeControl
	{
public:
	static CTurboSoundAppView* NewL(const TRect& aRect, MTurboSoundTrackObserver& aObserver);
	static CTurboSoundAppView* NewLC(const TRect& aRect, MTurboSoundTrackObserver& aObserver);
	virtual ~CTurboSoundAppView();

	void SetTrackNamesL(const MDesCArray& aNames);
	void SetStatusL(const TDesC& aText);
	void SetCurrentItemIndex(TInt aIndex);
	TInt CurrentItemIndex() const;

	/** Po wywolaniu, NASTEPNY klawisz wyswietli na pasku statusu swoj iCode / iScanCode /
	 * typ zdarzenia. Pomocne do ustalenia jakie kody wysyla bocznym Vol+/- na E75. */
	void EnableKeyDiagnostic();

	void SetNowPlayingKeysObserver(MTurboSoundNowPlayingKeys* aKeys);
	void ShowNowPlayingL(const TDesC& aDisplayName);
	void HideNowPlayingL();
	void SetNowPlayingProgressL(TInt aCurSec, TInt aTotalSec);
	/** Pauza wizualna na panelu (tekst PAUZA). */
	void SetNowPlayingPausedL(TBool aPaused);
	TBool NowPlayingActive() const;

public: // CCoeControl
	void Draw(const TRect& aRect) const;
	void SizeChanged();
	TInt CountComponentControls() const;
	CCoeControl* ComponentControl(TInt aIndex) const;
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);
	void HandlePointerEventL(const TPointerEvent& aPointerEvent);

private:
	void ConstructL(const TRect& aRect);
	CTurboSoundAppView(MTurboSoundTrackObserver& aObserver);
	TRect ListAreaRect() const;
	void DrawNowPlayingPanel(CWindowGc& aGc, const TRect& aArea) const;

private:
	MTurboSoundTrackObserver& iObserver;
	MTurboSoundNowPlayingKeys* iNpKeys;
	CAknSingleStyleListBox* iListBox;
	HBufC* iStatus;
	TInt iTrackCount;

	TBool iNpActive;
	TBool iNpPaused;
	HBufC* iNpTitle;
	HBufC* iNpArtist;
	TInt iNpCurSec;
	TInt iNpTotalSec;

	TBool iKeyDiagOnce;            // zalogowac kolejny klawisz do statusu i wylaczyc
	};

#endif // __TURBOSOUNDAPPVIEW_h__
