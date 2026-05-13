/*
 ============================================================================
 Name        : TurboSoundSimplePlayer.h
 Description : Folder scanner + system player launcher.
 ============================================================================
 */

#ifndef TURBOSOUNDSIMPLEPLAYER_H
#define TURBOSOUNDSIMPLEPLAYER_H

#include <e32base.h>
#include <bamdesca.h>

class CTurboSoundPlayEngine;

/** Powiadomienia ze silnika playera (in-app, CMdaAudioPlayerUtility). */
class MTurboSoundPlaybackObserver
	{
public:
	virtual void HandlePlaybackStartedL(TInt aIndex, const TDesC& aDisplayName) = 0;
	virtual void HandlePlaybackCompletedL(TInt aIndex, TInt aError) = 0;
	virtual void HandlePlaybackErrorL(TInt aError) = 0;
	};

class CTurboSoundSimplePlayer : public CBase
	{
public:
	static CTurboSoundSimplePlayer* NewL();
	virtual ~CTurboSoundSimplePlayer();

	/** Skanuje aFolder (sciezka konczy sie '\\') szukajac plikow audio. */
	void ScanFolderL(const TDesC& aFolder);

	/** Tytuły z ostatniego skanu. */
	const MDesCArray& TrackNames() const;

	TInt TrackCount() const;

	/**
	 * Otwiera i zaczyna grac utwor aIndex W APLIKACJI (CMdaAudioPlayerUtility).
	 * Stary system "uruchom Fonoteke" zostal zastapiony - dzwiek leci wewnatrz Turboconnect.
	 */
	void PlayTrackL(TInt aIndex);

	/** Alias zachowany dla kompatybilnosci ze starym kodem. */
	void LaunchTrackL(TInt aIndex);

	/** Pauza / wznowienie / stop biezacego utworu. */
	void PauseL();
	void ResumeL();
	void Stop();

	TBool IsPlaying() const;
	TBool IsPaused() const;
	TInt CurrentIndex() const;

	/** Pozycja i dlugosc odtwarzania (microsekundy); ETrue gdy jest silnik. */
	TBool GetPlaybackProgress(TTimeIntervalMicroSeconds& aPos,
	                          TTimeIntervalMicroSeconds& aDur);

	/** Glosnosc strumienia MMF (0 .. MaxVolume); bez otwartego playera zwraca 0. */
	TInt PlaybackVolume() const;
	TInt PlaybackMaxVolume() const;
	void SetPlaybackVolume(TInt aVolume);

	/** Otwarty plik MMF (Play lub Pauza) — do glosnosci i panelu. */
	TBool HasActiveStream() const;

	/** Przewin o aDeltaSec sekund (ujemne = w tyl). */
	void SeekRelativeSecondsL(TInt aDeltaSec);

	/** Ustaw pozycje bezwzglednie (sekundy od poczatku), po otwarciu strumienia. */
	void SeekToSecondsL(TInt aSec);

	/** Nazwa wyswietlana biezacego utworu (NULL gdy brak). Wskaznik do wew. bufora — nie zwalniaj. */
	const TDesC* CurrentDisplayName() const;

	/** Ustawia obserwatora powiadomien (start/end/error). */
	void SetObserver(MTurboSoundPlaybackObserver* aObserver);

private:
	CTurboSoundSimplePlayer();
	void ConstructL();

private:
	CTurboSoundPlayEngine* iD;
	};

#endif
