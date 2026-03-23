/*
 ============================================================================
 Name		: NokiaspotifyAppUi.h
 Author	  : 
 Copyright   : Your copyright notice
 Description : Declares UI class for application.
 ============================================================================
 */

#ifndef __NOKIASPOTIFYAPPUI_h__
#define __NOKIASPOTIFYAPPUI_h__

// INCLUDES
#include <aknappui.h>

// FORWARD DECLARATIONS
class CNokiaspotifyAppView;

// CLASS DECLARATION
/**
 * CNokiaspotifyAppUi application UI class.
 * Interacts with the user through the UI and request message processing
 * from the handler class
 */
class CNokiaspotifyAppUi : public CAknAppUi
	{
public:
	// Constructors and destructor

	/**
	 * ConstructL.
	 * 2nd phase constructor.
	 */
	void ConstructL();

	/**
	 * CNokiaspotifyAppUi.
	 * C++ default constructor. This needs to be public due to
	 * the way the framework constructs the AppUi
	 */
	CNokiaspotifyAppUi();

	/**
	 * ~CNokiaspotifyAppUi.
	 * Virtual Destructor.
	 */
	virtual ~CNokiaspotifyAppUi();

private:
	// Functions from base classes

	/**
	 * From CEikAppUi, HandleCommandL.
	 * Takes care of command handling.
	 * @param aCommand Command to be handled.
	 */
	void HandleCommandL(TInt aCommand);

	/**
	 *  HandleStatusPaneSizeChange.
	 *  Called by the framework when the application status pane
	 *  size is changed.
	 */
	void HandleStatusPaneSizeChange();

	/**
	 *  From CCoeAppUi, HelpContextL.
	 *  Provides help context for the application.
	 *  size is changed.
	 */
	CArrayFix<TCoeHelpContext>* HelpContextL() const;

private:
	void ShowWelcomeDialogL();

private:
	// Data

	/**
	 * The application view
	 * Owned by CNokiaspotifyAppUi
	 */
	CNokiaspotifyAppView* iAppView;

	};

#endif // __NOKIASPOTIFYAPPUI_h__
// End of File
