/*
 ============================================================================
 Name        : TurboSoundRemCon.h
 Description : RemConCoreApi target — boczne Vol+/- (i klawisze multimediow
               z zestawu sluchawkowego) sa dostarczane przez framework RemCon,
               nie przez window server. Bez tego E75 nie da nam tych klawiszy.
 ============================================================================
 */

#ifndef TURBOSOUND_REMCON_H
#define TURBOSOUND_REMCON_H

#include <e32base.h>
#include <remconcoreapitargetobserver.h>

class CRemConInterfaceSelector;
class CRemConCoreApiTarget;
class MTurboSoundNowPlayingKeys;

class CTurboSoundRemCon : public CBase, public MRemConCoreApiTargetObserver
	{
public:
	static CTurboSoundRemCon* NewL(MTurboSoundNowPlayingKeys& aObserver);
	virtual ~CTurboSoundRemCon();

public: // MRemConCoreApiTargetObserver
	void MrccatoCommand(TRemConCoreApiOperationId aOperationId,
	                    TRemConCoreApiButtonAction aButtonAct);
	void MrccatoPlay(TRemConCoreApiPlaybackSpeed aSpeed,
	                 TRemConCoreApiButtonAction aButtonAct);
	void MrccatoTuneFunction(TBool aTwoPart,
	                         TUint aMajorChannel,
	                         TUint aMinorChannel,
	                         TRemConCoreApiButtonAction aButtonAct);
	void MrccatoSelectDiskFunction(TUint aDisk,
	                               TRemConCoreApiButtonAction aButtonAct);
	void MrccatoSelectAvInputFunction(TUint8 aAvInputSignalNumber,
	                                  TRemConCoreApiButtonAction aButtonAct);
	void MrccatoSelectBroadcastChannelFunction(TUint aChannel,
	                                           TRemConCoreApiButtonAction aButtonAct);

private:
	CTurboSoundRemCon(MTurboSoundNowPlayingKeys& aObserver);
	void ConstructL();
	void DispatchL(TRemConCoreApiOperationId aOp,
	               TRemConCoreApiButtonAction aBtn);

private:
	MTurboSoundNowPlayingKeys& iObs;
	CRemConInterfaceSelector* iSelector;
	CRemConCoreApiTarget* iTarget;
	};

#endif
