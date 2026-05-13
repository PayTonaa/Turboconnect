/*
 ============================================================================
 Name        : TurboSoundRemCon.cpp
 Description : Glowny target RemCon — dostarcza boczne Vol+/- (i klawisze
               multimediow) aplikacji. Nokia E75 routuje te klawisze
               wlasnie przez RemCon, nie przez window server.
 ============================================================================
 */

#include "TurboSoundRemCon.h"

#include <remconinterfaceselector.h>
#include <remconcoreapitarget.h>
#include <remconcoreapi.h>

#include "TurboSoundAppView.h"  // dla MTurboSoundNowPlayingKeys

CTurboSoundRemCon* CTurboSoundRemCon::NewL(MTurboSoundNowPlayingKeys& aObserver)
	{
	CTurboSoundRemCon* self = new (ELeave) CTurboSoundRemCon(aObserver);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

CTurboSoundRemCon::CTurboSoundRemCon(MTurboSoundNowPlayingKeys& aObserver)
	: iObs(aObserver), iSelector(NULL), iTarget(NULL)
	{
	}

void CTurboSoundRemCon::ConstructL()
	{
	iSelector = CRemConInterfaceSelector::NewL();
	iTarget = CRemConCoreApiTarget::NewL(*iSelector, *this);
	iSelector->OpenTargetL();
	}

CTurboSoundRemCon::~CTurboSoundRemCon()
	{
	delete iSelector;
	iSelector = NULL;
	iTarget = NULL;  // wlasnoscia iSelector
	}

void CTurboSoundRemCon::DispatchL(TRemConCoreApiOperationId aOp,
                                  TRemConCoreApiButtonAction aBtn)
	{
	// Reagujemy na press i click (kazde wcisniecie). Release / hold-up ignorujemy,
	// ale RemCon i tak zwykle przysyla "click" przy szybkim wcisnieciu.
	if (aBtn != ERemConCoreApiButtonClick
	    && aBtn != ERemConCoreApiButtonPress)
		{
		return;
		}
	switch (aOp)
		{
		case ERemConCoreApiVolumeUp:
			iObs.HandleNpVolumeDeltaL(+1);
			break;
		case ERemConCoreApiVolumeDown:
			iObs.HandleNpVolumeDeltaL(-1);
			break;
		case ERemConCoreApiPausePlayFunction:
		case ERemConCoreApiPlay:
			iObs.HandleNpPauseToggleL();
			break;
		case ERemConCoreApiPause:
			iObs.HandleNpPauseToggleL();
			break;
		case ERemConCoreApiStop:
			iObs.HandleNpStopL();
			break;
		case ERemConCoreApiForward:
			iObs.HandleNpNextL();
			break;
		case ERemConCoreApiBackward:
			iObs.HandleNpPrevL();
			break;
		case ERemConCoreApiRewind:
			iObs.HandleNpSeekRelativeL(-10);
			break;
		case ERemConCoreApiFastForward:
			iObs.HandleNpSeekRelativeL(+10);
			break;
		default:
			break;
		}
	}

void CTurboSoundRemCon::MrccatoCommand(TRemConCoreApiOperationId aOperationId,
                                       TRemConCoreApiButtonAction aButtonAct)
	{
	TRAP_IGNORE(DispatchL(aOperationId, aButtonAct));
	}

void CTurboSoundRemCon::MrccatoPlay(TRemConCoreApiPlaybackSpeed /*aSpeed*/,
                                    TRemConCoreApiButtonAction aButtonAct)
	{
	TRAP_IGNORE(DispatchL(ERemConCoreApiPlay, aButtonAct));
	}

void CTurboSoundRemCon::MrccatoTuneFunction(TBool /*aTwoPart*/,
                                            TUint /*aMajorChannel*/,
                                            TUint /*aMinorChannel*/,
                                            TRemConCoreApiButtonAction /*aButtonAct*/)
	{
	}

void CTurboSoundRemCon::MrccatoSelectDiskFunction(TUint /*aDisk*/,
                                                  TRemConCoreApiButtonAction /*aButtonAct*/)
	{
	}

void CTurboSoundRemCon::MrccatoSelectAvInputFunction(TUint8 /*aAvInputSignalNumber*/,
                                                     TRemConCoreApiButtonAction /*aButtonAct*/)
	{
	}

void CTurboSoundRemCon::MrccatoSelectBroadcastChannelFunction(TUint /*aChannel*/,
                                                              TRemConCoreApiButtonAction /*aButtonAct*/)
	{
	}
