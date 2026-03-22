/*
 ============================================================================
 Name		: NokiaspotifyAppView.cpp
 Author	  : 
 Copyright   : Your copyright notice
 Description : Application view implementation
 ============================================================================
 */

// INCLUDE FILES
#include <coemain.h>
#include "NokiaspotifyAppView.h"

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::NewL()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CNokiaspotifyAppView* CNokiaspotifyAppView::NewL(const TRect& aRect)
	{
	CNokiaspotifyAppView* self = CNokiaspotifyAppView::NewLC(aRect);
	CleanupStack::Pop(self);
	return self;
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::NewLC()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CNokiaspotifyAppView* CNokiaspotifyAppView::NewLC(const TRect& aRect)
	{
	CNokiaspotifyAppView* self = new (ELeave) CNokiaspotifyAppView;
	CleanupStack::PushL(self);
	self->ConstructL(aRect);
	return self;
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CNokiaspotifyAppView::ConstructL(const TRect& aRect)
	{
	// Create a window for this application view
	CreateWindowL();

	// Set the windows size
	SetRect(aRect);

	// Activate the window, which makes it ready to be drawn
	ActivateL();
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::CNokiaspotifyAppView()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CNokiaspotifyAppView::CNokiaspotifyAppView()
	{
	// No implementation required
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::~CNokiaspotifyAppView()
// Destructor.
// -----------------------------------------------------------------------------
//
CNokiaspotifyAppView::~CNokiaspotifyAppView()
	{
	// No implementation required
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::Draw()
// Draws the display.
// -----------------------------------------------------------------------------
//
void CNokiaspotifyAppView::Draw(const TRect& /*aRect*/) const
	{
	// Get the standard graphics context
	CWindowGc& gc = SystemGc();

	// Gets the control's extent
	TRect drawRect(Rect());

	// Clears the screen
	gc.Clear(drawRect);

	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::SizeChanged()
// Called by framework when the view size is changed.
// -----------------------------------------------------------------------------
//
void CNokiaspotifyAppView::SizeChanged()
	{
	DrawNow();
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::HandlePointerEventL()
// Called by framework to handle pointer touch events.
// Note: although this method is compatible with earlier SDKs, 
// it will not be called in SDKs without Touch support.
// -----------------------------------------------------------------------------
//
void CNokiaspotifyAppView::HandlePointerEventL(
		const TPointerEvent& aPointerEvent)
	{

	// Call base class HandlePointerEventL()
	CCoeControl::HandlePointerEventL(aPointerEvent);
	}

// End of File
