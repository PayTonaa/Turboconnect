/*
 ============================================================================
 Name		: TurboSound.pan
 Author	  : Turbotoster
 Copyright   : Your copyright notice
 Description : This file contains panic codes.
 ============================================================================
 */

#ifndef __TURBOSOUND_PAN__
#define __TURBOSOUND_PAN__

/** TurboSound application panic codes */
enum TTurboSoundPanics
	{
	ETurboSoundUi = 1
	// add further panics here
	};

inline void Panic(TTurboSoundPanics aReason)
	{
	_LIT(applicationName, "TurboSound");
	User::Panic(applicationName, aReason);
	}

#endif // __TURBOSOUND_PAN__
