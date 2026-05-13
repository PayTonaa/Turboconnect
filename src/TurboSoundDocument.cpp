/*
 ============================================================================
 Name		: TurboSoundDocument.cpp
 Author	  : Turbotoster
 Copyright   : Your copyright notice
 Description : CTurboSoundDocument implementation
 ============================================================================
 */

// INCLUDE FILES
#include "TurboSoundAppUi.h"
#include "TurboSoundDocument.h"

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CTurboSoundDocument::NewL()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CTurboSoundDocument* CTurboSoundDocument::NewL(CEikApplication& aApp)
	{
	CTurboSoundDocument* self = NewLC(aApp);
	CleanupStack::Pop(self);
	return self;
	}

// -----------------------------------------------------------------------------
// CTurboSoundDocument::NewLC()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CTurboSoundDocument* CTurboSoundDocument::NewLC(CEikApplication& aApp)
	{
	CTurboSoundDocument* self = new (ELeave) CTurboSoundDocument(aApp);

	CleanupStack::PushL(self);
	self->ConstructL();
	return self;
	}

// -----------------------------------------------------------------------------
// CTurboSoundDocument::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CTurboSoundDocument::ConstructL()
	{
	// No implementation required
	}

// -----------------------------------------------------------------------------
// CTurboSoundDocument::CTurboSoundDocument()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CTurboSoundDocument::CTurboSoundDocument(CEikApplication& aApp) :
	CAknDocument(aApp)
	{
	// No implementation required
	}

// ---------------------------------------------------------------------------
// CTurboSoundDocument::~CTurboSoundDocument()
// Destructor.
// ---------------------------------------------------------------------------
//
CTurboSoundDocument::~CTurboSoundDocument()
	{
	// No implementation required
	}

// ---------------------------------------------------------------------------
// CTurboSoundDocument::CreateAppUiL()
// Constructs CreateAppUi.
// ---------------------------------------------------------------------------
//
CEikAppUi* CTurboSoundDocument::CreateAppUiL()
	{
	// Create the application user interface, and return a pointer to it;
	// the framework takes ownership of this object
	return new (ELeave) CTurboSoundAppUi;
	}

// End of File
