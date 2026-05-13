/*
 ============================================================================
 Name		: TurboSoundApplication.cpp
 Author	  : Turbotoster
 Copyright   : Your copyright notice
 Description : Main application class
 ============================================================================
 */

// INCLUDE FILES
#include "TurboSound.hrh"
#include "TurboSoundDocument.h"
#include "TurboSoundApplication.h"

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CTurboSoundApplication::CreateDocumentL()
// Creates CApaDocument object
// -----------------------------------------------------------------------------
//
CApaDocument* CTurboSoundApplication::CreateDocumentL()
	{
	// Create an TurboSound document, and return a pointer to it
	return CTurboSoundDocument::NewL(*this);
	}

// -----------------------------------------------------------------------------
// CTurboSoundApplication::AppDllUid()
// Returns application UID
// -----------------------------------------------------------------------------
//
TUid CTurboSoundApplication::AppDllUid() const
	{
	// Return the UID for the TurboSound application
	return KUidTurboSoundApp;
	}

// End of File
