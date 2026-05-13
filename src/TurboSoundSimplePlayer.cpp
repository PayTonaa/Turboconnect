/*
 ============================================================================
 Name        : TurboSoundSimplePlayer.cpp
 Description : Skanowanie folderu + odtwarzanie audio W APLIKACJI
               (CMdaAudioPlayerUtility - bez przekierowania do Fonoteki).
 ============================================================================
*/

#include "TurboSoundSimplePlayer.h"

#include <eikenv.h>
#include <badesca.h>
#include <bamdesca.h>
#include <f32file.h>
#include <coemain.h>
#include <mdaaudiosampleplayer.h>

class CTurboSoundPlayEngine : public CBase, public MMdaAudioPlayerCallback
	{
public:
	static CTurboSoundPlayEngine* NewL();
	~CTurboSoundPlayEngine();

	void ScanFolderL(const TDesC& aFolder);
	const MDesCArray& Names() const;
	TInt Count() const;

	void PlayTrackL(TInt aIndex);
	void PauseL();
	void ResumeL();
	void Stop();
	TBool IsPlaying() const { return iPlaying; }
	TBool IsPaused() const  { return iPaused;  }
	TInt CurrentIndex() const { return iCurrentIndex; }

	void SetObserver(MTurboSoundPlaybackObserver* aObserver) { iObserver = aObserver; }

	TInt VolumeLevel() const;
	TInt MaxVolumeLevel() const;
	void SetVolumeLevel(TInt aVolume);

	TBool HasActiveStream() const;
	void SeekRelativeSecondsL(TInt aDeltaSec);
	void SeekToSecondsL(TInt aSec);
	const TDesC* CurrentDisplayName() const;

	void GetPlaybackProgressL(TTimeIntervalMicroSeconds& aPos,
	                          TTimeIntervalMicroSeconds& aDur);

public: // MMdaAudioPlayerCallback
	void MapcInitComplete(TInt aError, const TTimeIntervalMicroSeconds& aDuration);
	void MapcPlayComplete(TInt aError);

private:
	CTurboSoundPlayEngine();
	void ConstructL();
	TBool IsAudioFile(const TDesC& aFileName) const;
	void ReleasePlayer();
	void NotifyError(TInt aError);
	/** MMF czasem zwraca MaxVolume()==0 mimo dzialajacego SetVolume — uzywamy skali zastepczej. */
	TInt EffectiveMaxVolume() const;

private:
	CDesCArrayFlat* iNames;
	CDesCArrayFlat* iPaths;

	CMdaAudioPlayerUtility* iPlayer;
	TBool iPrimed;        // czy plik jest otwarty i gotowy do play
	TBool iPlaying;       // czy aktualnie leci dzwiek
	TBool iPaused;        // czy stoi na pauzie
	TInt  iCurrentIndex;  // -1 = nic
	HBufC* iCurrentName;  // wyswietlana nazwa biezacego utworu (na potrzeby callback)
	TTimeIntervalMicroSeconds iDurationUs;

	MTurboSoundPlaybackObserver* iObserver;

	TInt iStreamVolume;       // MMF nie ma Volume() — trzymamy ustawiana wartosc
	TInt iPersistedVolume;    // ostatnia wartosc ustawiona przez uzytkownika (trzyma sie przez kolejne utwory)
	TBool iVolumePersisted;   // EFalse = jeszcze nie ustawiona (uzyj domyslnej startowej)
	};

// ---------------------------------------------------------------------------
//                       silnik (in-app playback)
// ---------------------------------------------------------------------------

CTurboSoundPlayEngine* CTurboSoundPlayEngine::NewL()
	{
	CTurboSoundPlayEngine* self = new (ELeave) CTurboSoundPlayEngine;
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

CTurboSoundPlayEngine::CTurboSoundPlayEngine()
	: iNames(NULL), iPaths(NULL), iPlayer(NULL),
	  iPrimed(EFalse), iPlaying(EFalse), iPaused(EFalse),
	  iCurrentIndex(-1), iCurrentName(NULL), iDurationUs(0), iObserver(NULL),
	  iStreamVolume(0), iPersistedVolume(0), iVolumePersisted(EFalse)
	{
	}

void CTurboSoundPlayEngine::ConstructL()
	{
	iNames = new (ELeave) CDesCArrayFlat(8);
	iPaths = new (ELeave) CDesCArrayFlat(8);
	}

CTurboSoundPlayEngine::~CTurboSoundPlayEngine()
	{
	ReleasePlayer();
	delete iCurrentName;
	delete iNames;
	delete iPaths;
	}

void CTurboSoundPlayEngine::ReleasePlayer()
	{
	if (iPlayer)
		{
		iPlayer->Stop();
		iPlayer->Close();
		delete iPlayer;
		iPlayer = NULL;
		}
	iPrimed = EFalse;
	iPlaying = EFalse;
	iPaused = EFalse;
	iDurationUs = 0;
	iStreamVolume = 0;
	}

const MDesCArray& CTurboSoundPlayEngine::Names() const
	{
	return *iNames;
	}

TInt CTurboSoundPlayEngine::Count() const
	{
	return iNames ? iNames->MdcaCount() : 0;
	}

void CTurboSoundPlayEngine::ScanFolderL(const TDesC& aFolder)
	{
	RFs& fs = CCoeEnv::Static()->FsSession();
	TInt mk = fs.MkDirAll(aFolder);
	if ((mk != KErrNone) && (mk != KErrAlreadyExists))
		{
		// ignore - folder moze juz istniec
		}

	TFileName pattern;
	pattern.Copy(aFolder);
	pattern.Append(_L("*"));

	CDir* dir = NULL;
	User::LeaveIfError(fs.GetDir(pattern, KEntryAttNormal, ESortByName, dir));
	CleanupStack::PushL(dir);

	CDesCArrayFlat* newNames = new (ELeave) CDesCArrayFlat(8);
	CleanupStack::PushL(newNames);
	CDesCArrayFlat* newPaths = new (ELeave) CDesCArrayFlat(8);
	CleanupStack::PushL(newPaths);

	for (TInt i = 0; i < dir->Count(); i++)
		{
		const TEntry& e = (*dir)[i];
		if (e.IsDir())
			{
			continue;
			}
		if (!IsAudioFile(e.iName))
			{
			continue;
			}
		newNames->AppendL(e.iName);

		TFileName full;
		full.Copy(aFolder);
		full.Append(e.iName);
		newPaths->AppendL(full);
		}

	CleanupStack::Pop(newPaths);
	CleanupStack::Pop(newNames);
	CleanupStack::PopAndDestroy(dir);

	delete iNames;
	iNames = newNames;
	delete iPaths;
	iPaths = newPaths;
	}

TBool CTurboSoundPlayEngine::IsAudioFile(const TDesC& aFileName) const
	{
	TParse parse;
	if (parse.Set(aFileName, NULL, NULL) != KErrNone)
		{
		return EFalse;
		}
	const TPtrC ext = parse.Ext();
	return (ext.CompareF(_L(".mp3")) == 0)
		|| (ext.CompareF(_L(".aac")) == 0)
		|| (ext.CompareF(_L(".m4a")) == 0)
		|| (ext.CompareF(_L(".wma")) == 0);
	}

void CTurboSoundPlayEngine::PlayTrackL(TInt aIndex)
	{
	if (!iPaths || aIndex < 0 || aIndex >= iPaths->MdcaCount())
		{
		return;
		}
	// Zamknij stary player przed otwarciem nowego pliku.
	ReleasePlayer();

	delete iCurrentName;
	iCurrentName = NULL;
	if (iNames && aIndex < iNames->MdcaCount())
		{
		iCurrentName = iNames->MdcaPoint(aIndex).AllocL();
		}
	iCurrentIndex = aIndex;

	const TPtrC path = iPaths->MdcaPoint(aIndex);
	// Priorytet "Music Player" wg konwencji S60 3rd FP2. Zapewnia, ze:
	//   1) AudioPolicy systemu prawidlowo obsluzy boczne Vol+/- (popup glosnosci)
	//   2) Nasze audio nie jest traktowane jak alarmowy / wysokopriorytetowy stream.
	// W razie braku stalej w SDK definiujemy wartosc lokalnie.
	#ifndef KAudioPriorityMusicPlayer
	const TInt KAudioPriorityMusicPlayer = 78;
	#endif
	iPlayer = CMdaAudioPlayerUtility::NewFilePlayerL(
			path, *this,
			KAudioPriorityMusicPlayer,
			(TMdaPriorityPreference) EMdaPriorityPreferenceTime);
	}

void CTurboSoundPlayEngine::PauseL()
	{
	if (iPlayer && iPlaying)
		{
		iPlayer->Pause();
		iPaused = ETrue;
		iPlaying = EFalse;
		}
	}

void CTurboSoundPlayEngine::ResumeL()
	{
	if (iPlayer && iPaused && iPrimed)
		{
		iPlayer->Play();
		iPaused = EFalse;
		iPlaying = ETrue;
		}
	}

void CTurboSoundPlayEngine::Stop()
	{
	ReleasePlayer();
	iCurrentIndex = -1;
	delete iCurrentName;
	iCurrentName = NULL;
	}

void CTurboSoundPlayEngine::NotifyError(TInt aError)
	{
	if (iObserver)
		{
		TRAP_IGNORE(iObserver->HandlePlaybackErrorL(aError));
		}
	}

TInt CTurboSoundPlayEngine::EffectiveMaxVolume() const
	{
	if (!iPlayer)
		{
		return 0;
		}
	const TInt mx = iPlayer->MaxVolume();
	return (mx > 0) ? mx : 10;
	}

void CTurboSoundPlayEngine::GetPlaybackProgressL(
		TTimeIntervalMicroSeconds& aPos,
		TTimeIntervalMicroSeconds& aDur)
	{
	aPos = 0;
	aDur = iDurationUs;
	if (!iPlayer || !iPrimed)
		{
		return;
		}
	iPlayer->GetPosition(aPos);
	if (iDurationUs.Int64() <= 0 && iPlayer)
		{
		aDur = iPlayer->Duration();
		}
	}

TInt CTurboSoundPlayEngine::VolumeLevel() const
	{
	return iStreamVolume;
	}

TInt CTurboSoundPlayEngine::MaxVolumeLevel() const
	{
	return EffectiveMaxVolume();
	}

void CTurboSoundPlayEngine::SetVolumeLevel(TInt aVolume)
	{
	if (!iPlayer)
		{
		return;
		}
	const TInt mx = EffectiveMaxVolume();
	if (aVolume < 0)
		{
		aVolume = 0;
		}
	if (mx > 0 && aVolume > mx)
		{
		aVolume = mx;
		}
	iPlayer->SetVolume(aVolume);
	iStreamVolume = aVolume;
	iPersistedVolume = aVolume;
	iVolumePersisted = ETrue;
	}

TBool CTurboSoundPlayEngine::HasActiveStream() const
	{
	return (iPlayer != NULL) && iPrimed;
	}

void CTurboSoundPlayEngine::SeekRelativeSecondsL(TInt aDeltaSec)
	{
	if (!iPlayer || !iPrimed)
		{
		return;
		}

	const TBool wasPlaying = iPlaying;

	TTimeIntervalMicroSeconds pos(0);
	iPlayer->GetPosition(pos);

	TTimeIntervalMicroSeconds dur = iDurationUs;
	if (iDurationUs.Int64() <= 0)
		{
		dur = iPlayer->Duration();
		}

	TInt64 np = pos.Int64() + (TInt64)aDeltaSec * (TInt64)1000000;
	if (np < 0)
		{
		np = 0;
		}
	const TInt64 dmax = dur.Int64();
	if (np > dmax)
		{
		np = dmax;
		}

	if (wasPlaying)
		{
		iPlayer->Pause();
		iPlaying = EFalse;
		iPaused = ETrue;
		}

	TTimeIntervalMicroSeconds newPos;
	newPos = np;
	TRAP_IGNORE(iPlayer->SetPosition(newPos));

	if (wasPlaying)
		{
		iPlayer->Play();
		iPlaying = ETrue;
		iPaused = EFalse;
		}
	}

void CTurboSoundPlayEngine::SeekToSecondsL(TInt aSec)
	{
	if (!iPlayer || !iPrimed)
		{
		return;
		}
	if (aSec < 0)
		{
		aSec = 0;
		}

	const TBool wasPlaying = iPlaying;

	TTimeIntervalMicroSeconds dur = iDurationUs;
	if (iDurationUs.Int64() <= 0)
		{
		dur = iPlayer->Duration();
		}
	TInt64 np = (TInt64)aSec * (TInt64)1000000;
	const TInt64 dmax = dur.Int64();
	if (dmax > 0 && np > dmax)
		{
		np = dmax;
		}

	if (wasPlaying)
		{
		iPlayer->Pause();
		iPlaying = EFalse;
		iPaused = ETrue;
		}

	TTimeIntervalMicroSeconds newPos;
	newPos = np;
	TRAP_IGNORE(iPlayer->SetPosition(newPos));

	if (wasPlaying)
		{
		iPlayer->Play();
		iPlaying = ETrue;
		iPaused = EFalse;
		}
	}

const TDesC* CTurboSoundPlayEngine::CurrentDisplayName() const
	{
	return iCurrentName;
	}

// --- callbacki z CMdaAudioPlayerUtility ------------------------------------

void CTurboSoundPlayEngine::MapcInitComplete(
		TInt aError, const TTimeIntervalMicroSeconds& aDuration)
	{
	if (aError != KErrNone)
		{
		ReleasePlayer();
		NotifyError(aError);
		return;
		}
	iDurationUs = aDuration;
	if (iDurationUs.Int64() <= 0 && iPlayer)
		{
		iDurationUs = iPlayer->Duration();
		}
	iPrimed = ETrue;
	if (iPlayer)
		{
		iPlayer->Play();
		iPlaying = ETrue;
		iPaused = EFalse;
		const TInt mx = EffectiveMaxVolume();
		if (mx > 0)
			{
			// Strumien MMF zostawiamy na MAX — boczny Vol+/- systemu (AudioPolicy)
			// reguluje "system music volume", co dziala globalnie miedzy aplikacjami.
			// Per-stream wartosc moze byc zmieniana przez nasze klawisze 2/8 (panel).
			TInt v = iVolumePersisted ? iPersistedVolume : mx;
			if (v < 0) v = 0;
			if (v > mx) v = mx;
			iPlayer->SetVolume(v);
			iStreamVolume = v;
			if (!iVolumePersisted)
				{
				iPersistedVolume = v;
				iVolumePersisted = ETrue;
				}
			}
		if (iObserver && iCurrentName)
			{
			TRAP_IGNORE(iObserver->HandlePlaybackStartedL(iCurrentIndex, *iCurrentName));
			}
		}
	}

void CTurboSoundPlayEngine::MapcPlayComplete(TInt aError)
	{
	const TInt finishedIndex = iCurrentIndex;
	iPlaying = EFalse;
	iPaused = EFalse;
	// Nie zwalniamy iPlayer od razu - obserwator moze chciec od razu zagrac kolejny
	// utwor (i wtedy ReleasePlayer zrobi to czysto przez PlayTrackL -> ReleasePlayer).
	if (iObserver)
		{
		TRAP_IGNORE(iObserver->HandlePlaybackCompletedL(finishedIndex, aError));
		}
	}

// ---------------------------------------------------------------------------
//                  wrapper (zachowany dotychczasowy interfejs)
// ---------------------------------------------------------------------------

CTurboSoundSimplePlayer* CTurboSoundSimplePlayer::NewL()
	{
	CTurboSoundSimplePlayer* self = new (ELeave) CTurboSoundSimplePlayer;
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

CTurboSoundSimplePlayer::CTurboSoundSimplePlayer()
	: iD(NULL)
	{
	}

void CTurboSoundSimplePlayer::ConstructL()
	{
	iD = CTurboSoundPlayEngine::NewL();
	}

CTurboSoundSimplePlayer::~CTurboSoundSimplePlayer()
	{
	delete iD;
	iD = NULL;
	}

void CTurboSoundSimplePlayer::ScanFolderL(const TDesC& aFolder)
	{
	if (iD)
		{
		iD->ScanFolderL(aFolder);
		}
	}

const MDesCArray& CTurboSoundSimplePlayer::TrackNames() const
	{
	return iD->Names();
	}

TInt CTurboSoundSimplePlayer::TrackCount() const
	{
	return iD ? iD->Count() : 0;
	}

void CTurboSoundSimplePlayer::PlayTrackL(TInt aIndex)
	{
	if (iD)
		{
		iD->PlayTrackL(aIndex);
		}
	}

void CTurboSoundSimplePlayer::LaunchTrackL(TInt aIndex)
	{
	PlayTrackL(aIndex);
	}

void CTurboSoundSimplePlayer::PauseL()
	{
	if (iD)
		{
		iD->PauseL();
		}
	}

void CTurboSoundSimplePlayer::ResumeL()
	{
	if (iD)
		{
		iD->ResumeL();
		}
	}

void CTurboSoundSimplePlayer::Stop()
	{
	if (iD)
		{
		iD->Stop();
		}
	}

TBool CTurboSoundSimplePlayer::IsPlaying() const
	{
	return iD ? iD->IsPlaying() : EFalse;
	}

TBool CTurboSoundSimplePlayer::IsPaused() const
	{
	return iD ? iD->IsPaused() : EFalse;
	}

TInt CTurboSoundSimplePlayer::CurrentIndex() const
	{
	return iD ? iD->CurrentIndex() : -1;
	}

void CTurboSoundSimplePlayer::SetObserver(MTurboSoundPlaybackObserver* aObserver)
	{
	if (iD)
		{
		iD->SetObserver(aObserver);
		}
	}

TBool CTurboSoundSimplePlayer::GetPlaybackProgress(
		TTimeIntervalMicroSeconds& aPos,
		TTimeIntervalMicroSeconds& aDur)
	{
	if (!iD)
		{
		return EFalse;
		}
	iD->GetPlaybackProgressL(aPos, aDur);
	return ETrue;
	}

TInt CTurboSoundSimplePlayer::PlaybackVolume() const
	{
	return iD ? iD->VolumeLevel() : 0;
	}

TInt CTurboSoundSimplePlayer::PlaybackMaxVolume() const
	{
	return iD ? iD->MaxVolumeLevel() : 0;
	}

void CTurboSoundSimplePlayer::SetPlaybackVolume(TInt aVolume)
	{
	if (iD)
		{
		iD->SetVolumeLevel(aVolume);
		}
	}

TBool CTurboSoundSimplePlayer::HasActiveStream() const
	{
	return iD ? iD->HasActiveStream() : EFalse;
	}

void CTurboSoundSimplePlayer::SeekRelativeSecondsL(TInt aDeltaSec)
	{
	if (iD)
		{
		iD->SeekRelativeSecondsL(aDeltaSec);
		}
	}

void CTurboSoundSimplePlayer::SeekToSecondsL(TInt aSec)
	{
	if (iD)
		{
		iD->SeekToSecondsL(aSec);
		}
	}

const TDesC* CTurboSoundSimplePlayer::CurrentDisplayName() const
	{
	return iD ? iD->CurrentDisplayName() : NULL;
	}
