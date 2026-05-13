/*
 ============================================================================
 Name		: TurboSoundApplication.h
 Author	  : Turbotoster
 Copyright   : Your copyright notice
 Description : Declares main application class.
 ============================================================================
 */

#ifndef __TURBOSOUNDAPPLICATION_H__
#define __TURBOSOUNDAPPLICATION_H__

// INCLUDES
#include <aknapp.h>
#include "TurboSound.hrh"

// UID for the application;
// this should correspond to the uid defined in the mmp file
const TUid KUidTurboSoundApp =
	{
	_UID3
	};

// CLASS DECLARATION

/**
 * CTurboSoundApplication application class.
 * Provides factory to create concrete document object.
 * An instance of CTurboSoundApplication is the application part of the
 * AVKON application framework for the TurboSound example application.
 */
class CTurboSoundApplication : public CAknApplication
	{
public:
	// Functions from base classes

	/**
	 * From CApaApplication, AppDllUid.
	 * @return Application's UID (KUidTurboSoundApp).
	 */
	TUid AppDllUid() const;

protected:
	// Functions from base classes

	/**
	 * From CApaApplication, CreateDocumentL.
	 * Creates CTurboSoundDocument document object. The returned
	 * pointer in not owned by the CTurboSoundApplication object.
	 * @return A pointer to the created document object.
	 */
	CApaDocument* CreateDocumentL();
	};

#endif // __TURBOSOUNDAPPLICATION_H__
// End of File
