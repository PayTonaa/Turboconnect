/*
 ============================================================================
 Name		: TurboSoundDocument.h
 Author	  : Turbotoster
 Copyright   : Your copyright notice
 Description : Declares document class for application.
 ============================================================================
 */

#ifndef __TURBOSOUNDDOCUMENT_h__
#define __TURBOSOUNDDOCUMENT_h__

// INCLUDES
#include <akndoc.h>

// FORWARD DECLARATIONS
class CTurboSoundAppUi;
class CEikApplication;

// CLASS DECLARATION

/**
 * CTurboSoundDocument application class.
 * An instance of class CTurboSoundDocument is the Document part of the
 * AVKON application framework for the TurboSound example application.
 */
class CTurboSoundDocument : public CAknDocument
	{
public:
	// Constructors and destructor

	/**
	 * NewL.
	 * Two-phased constructor.
	 * Construct a CTurboSoundDocument for the AVKON application aApp
	 * using two phase construction, and return a pointer
	 * to the created object.
	 * @param aApp Application creating this document.
	 * @return A pointer to the created instance of CTurboSoundDocument.
	 */
	static CTurboSoundDocument* NewL(CEikApplication& aApp);

	/**
	 * NewLC.
	 * Two-phased constructor.
	 * Construct a CTurboSoundDocument for the AVKON application aApp
	 * using two phase construction, and return a pointer
	 * to the created object.
	 * @param aApp Application creating this document.
	 * @return A pointer to the created instance of CTurboSoundDocument.
	 */
	static CTurboSoundDocument* NewLC(CEikApplication& aApp);

	/**
	 * ~CTurboSoundDocument
	 * Virtual Destructor.
	 */
	virtual ~CTurboSoundDocument();

public:
	// Functions from base classes

	/**
	 * CreateAppUiL
	 * From CEikDocument, CreateAppUiL.
	 * Create a CTurboSoundAppUi object and return a pointer to it.
	 * The object returned is owned by the Uikon framework.
	 * @return Pointer to created instance of AppUi.
	 */
	CEikAppUi* CreateAppUiL();

private:
	// Constructors

	/**
	 * ConstructL
	 * 2nd phase constructor.
	 */
	void ConstructL();

	/**
	 * CTurboSoundDocument.
	 * C++ default constructor.
	 * @param aApp Application creating this document.
	 */
	CTurboSoundDocument(CEikApplication& aApp);

	};

#endif // __TURBOSOUNDDOCUMENT_h__
// End of File
