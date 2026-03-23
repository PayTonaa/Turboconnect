/*
 ============================================================================
 Name		: NokiaspotifyAppUi.cpp
 Author	  :
 Copyright   : Your copyright notice
 Description : CNokiaspotifyAppUi implementation
 ============================================================================
 */

// INCLUDE FILES
#include <avkon.hrh>
#include <aknmessagequerydialog.h>
#include <aknnotewrappers.h>
#include <stringloader.h>

#include <Nokiaspotify_0xE0A04525.rsg>

#ifdef _HELP_AVAILABLE_
//#include "Nokiaspotify_0xE0A04525.hlp.hrh"
#endif
#include "Nokiaspotify.hrh"
#include "Nokiaspotify.pan"
#include "NokiaspotifyApplication.h"
#include "NokiaspotifyAppUi.h"
#include "NokiaspotifyAppView.h"

_LIT(KinformationText,
	"Connect to internet, login yo your accont\n"
	"NOTE! Spotify premium is required");
_LIT(KinformationHead, "Spotify for Nokia");

// ============================ MEMBER FUNCTIONS ===============================


// -----------------------------------------------------------------------------
// CNokiaspotifyAppUi::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CNokiaspotifyAppUi::ConstructL()
	{
	// Initialise app UI with standard value.
	BaseConstructL(CAknAppUi::EAknEnableSkin);

	iAppView = CNokiaspotifyAppView::NewL(ClientRect());
	AddToStackL(iAppView);
	iAppView->SetFocus(ETrue);
	TRAP_IGNORE(ShowWelcomeDialogL());
	iAppView->DrawNow();
	}
// -----------------------------------------------------------------------------
// CNokiaspotifyAppUi::CNokiaspotifyAppUi()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CNokiaspotifyAppUi::CNokiaspotifyAppUi()
	{
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppUi::~CNokiaspotifyAppUi()
// Destructor.
// -----------------------------------------------------------------------------
//
CNokiaspotifyAppUi::~CNokiaspotifyAppUi()
	{
	if (iAppView)
		{
		RemoveFromStack(iAppView);
		delete iAppView;
		iAppView = NULL;
		}

	}


// -----------------------------------------------------------------------------
// CNokiaspotifyAppUi::HandleCommandL()
// Takes care of command handling.
// -----------------------------------------------------------------------------
//
void CNokiaspotifyAppUi::HandleCommandL(TInt aCommand)
	{
	switch (aCommand)
		{
		case EEikCmdExit:
		case EAknSoftkeyExit:
			Exit();
			break;

		case ECommand1:
			{

			// Load a string from the resource file and display it
			HBufC* textResource = StringLoader::LoadLC(R_COMMAND1_TEXT);
			CAknInformationNote* informationNote;

			informationNote = new (ELeave) CAknInformationNote;

			// Show the information Note with
			// textResource loaded with StringLoader.
			informationNote->ExecuteLD(*textResource);

			// Pop HBuf from CleanUpStack and Destroy it.
			CleanupStack::PopAndDestroy(textResource);
			}
			break;
		case ECommand2:
			{
			_LIT(KAuthors,
				"Turboconnect / Spotify for Nokia\n"
				"Author: Turbotoster\n"
				"Device: Nokia E75");
			CAknInformationNote* note = new (ELeave) CAknInformationNote;
			note->ExecuteLD(KAuthors);
			}
			break;
		case EHelp:
			ShowWelcomeDialogL();
			break;
		case ELogin:
			ShowLoginInfoL();
			break;
		case EAbout:
			{

			CAknMessageQueryDialog* dlg = new (ELeave) CAknMessageQueryDialog();
			dlg->PrepareLC(R_ABOUT_QUERY_DIALOG);
			HBufC* title = iEikonEnv->AllocReadResourceLC(R_ABOUT_DIALOG_TITLE);
			dlg->QueryHeading()->SetTextL(*title);
			CleanupStack::PopAndDestroy(); //title
			HBufC* msg = iEikonEnv->AllocReadResourceLC(R_ABOUT_DIALOG_TEXT);
			dlg->SetMessageTextL(*msg);
			CleanupStack::PopAndDestroy(); //msg
			dlg->RunLD();
			}
			break;
		default:
			// Do not Panic: the shell sends many command ids; unknown ones are OK.
			break;
		}
	}

void CNokiaspotifyAppUi::ShowWelcomeDialogL()
	{
	const TInt len = KinformationHead().Length() + KinformationText().Length() + 4;
	HBufC* msg = HBufC::NewLC(len);
	msg->Des().Copy(KinformationHead);
	msg->Des().Append(_L("\n\n"));
	msg->Des().Append(KinformationText);
	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	note->ExecuteLD(*msg);
	CleanupStack::PopAndDestroy(msg);
	}

void CNokiaspotifyAppUi::HandleLoginFromViewL()
	{
	TRAP_IGNORE(ShowLoginInfoL());
	}

void CNokiaspotifyAppUi::ShowLoginInfoL()
	{
	_LIT(KLoginInfo, "Log In: not wired yet — server OAuth next.");
	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	note->ExecuteLD(KLoginInfo);
	}
// -----------------------------------------------------------------------------
//  Called by the framework when the application status pane
//  size is changed.  Passes the new client rectangle to the
//  AppView
// -----------------------------------------------------------------------------
//
void CNokiaspotifyAppUi::HandleStatusPaneSizeChange()
	{
	if (iAppView)
		{
		iAppView->SetRect(ClientRect());
		}
	}

CArrayFix<TCoeHelpContext>* CNokiaspotifyAppUi::HelpContextL() const
	{
//#warning "Please see comment about help and UID3..."
	// Note: Help will not work if the application uid3 is not in the
	// protected range.  The default uid3 range for projects created
	// from this template (0xE0000000 - 0xEFFFFFFF) are not in the protected range so that they
	// can be self signed and installed on the device during testing.
	// Once you get your official uid3 from Symbian Ltd. and find/replace
	// all occurrences of uid3 in your project, the context help will
	// work. Alternatively, a patch now exists for the versions of
	// HTML help compiler in SDKs and can be found here along with an FAQ:
	// http://www3.symbian.com/faq.nsf/AllByDate/E9DF3257FD565A658025733900805EA2?OpenDocument
#ifdef _HELP_AVAILABLE_
	CArrayFixFlat<TCoeHelpContext>* array = new(ELeave)CArrayFixFlat<TCoeHelpContext>(1);
	CleanupStack::PushL(array);
//	array->AppendL(TCoeHelpContext(KUidNokiaspotifyApp, KGeneral_Information));
	CleanupStack::Pop(array);
	return array;
#else
	return NULL;
#endif
	}

// End of File
