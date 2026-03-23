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
#include <gdi.h>
#include <eikenv.h>
#include <e32keys.h>
#include <eikappui.h>
#include "NokiaspotifyAppView.h"
#include "NokiaspotifyAppUi.h"

_LIT(KLoginText, "Log In");
_LIT(KIntroText, "Spotify for Nokia (E75). Center key: Log In.");

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
	CreateWindowL();
	SetRect(aRect);
	ActivateL();
	}
// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::CNokiaspotifyAppView()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CNokiaspotifyAppView::CNokiaspotifyAppView()
	: iLoginButtonDown(EFalse)
	, iLoginKeyFocus(ETrue)
	{
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::LoginButtonRect()
// -----------------------------------------------------------------------------
//
TRect CNokiaspotifyAppView::LoginButtonRect()
	{
	return TRect(70, 140, 170, 170);
	}

TRect CNokiaspotifyAppView::IntroduceTextBox(
		const TRect& aClient,
		const TRect& aLoginButton)
	{
	const TInt kOuterMargin = 8;
	const TInt kGapAboveButton = 8;
	const TInt left = aClient.iTl.iX + kOuterMargin;
	const TInt top = aClient.iTl.iY + kOuterMargin;
	const TInt right = aClient.iBr.iX - kOuterMargin;
	const TInt bottom = aLoginButton.iTl.iY - kGapAboveButton;
	return TRect(left, top, right, bottom);
	}

TSize CNokiaspotifyAppView::ClassicRound()
	{
	return TSize(15, 15);
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
	CWindowGc& gc = SystemGc();
	const TRect r(Rect());
	gc.SetPenStyle(CGraphicsContext::ENullPen);
	gc.SetBrushColor(TRgb(0,0,0));
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.DrawRect(r);

	const TRect spotifybutton(LoginButtonRect());
	const TRect introduce_box(IntroduceTextBox(r, spotifybutton));
	const TInt kTextPad = 6;
	const TRect textInner(
		introduce_box.iTl.iX + kTextPad,
		introduce_box.iTl.iY + kTextPad,
		introduce_box.iBr.iX - kTextPad,
		introduce_box.iBr.iY - kTextPad);

	const CFont* fontMessage = CEikonEnv::Static()->DenseFont();
	gc.UseFont(fontMessage);
	gc.SetPenStyle(CGraphicsContext::ESolidPen);
	gc.SetPenColor(TRgb(220, 220, 220));
	if (textInner.Height() > 0 && textInner.Width() > 0)
		{
		gc.SetClippingRect(textInner);
		const TInt baseline = textInner.iTl.iY + 14;
		gc.DrawText(KIntroText, TPoint(textInner.iTl.iX, baseline));
		gc.CancelClippingRect();
		}
	gc.DiscardFont();

	const TRgb kGreen(30, 215, 96);
	const TRgb kGreenHi(80, 240, 150);
	gc.SetPenStyle(CGraphicsContext::ESolidPen);
	gc.SetPenColor(iLoginButtonDown ? TRgb(255, 255, 255) : kGreen);
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.SetBrushColor(iLoginButtonDown ? kGreenHi : kGreen);
	gc.DrawRoundRect(spotifybutton, ClassicRound());

	gc.SetBrushStyle(CGraphicsContext::ENullBrush);
	const CFont* font = CEikonEnv::Static()->NormalFont();
	gc.UseFont(font);
	const TInt text_offset = 20;
	gc.SetPenStyle(CGraphicsContext::ESolidPen);
	gc.SetPenColor(TRgb(255, 255, 255));
	gc.DrawText(KLoginText, spotifybutton, text_offset, CGraphicsContext::ECenter);
	gc.DiscardFont();

	if (iLoginKeyFocus)
		{
		TRect focusRing(spotifybutton);
		focusRing.iTl.iX -= 2;
		focusRing.iTl.iY -= 2;
		focusRing.iBr.iX += 2;
		focusRing.iBr.iY += 2;
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(TRgb(255, 255, 255));
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		gc.DrawRoundRect(focusRing, ClassicRound());
		}

	gc.SetPenStyle(CGraphicsContext::ENullPen);
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
	const TRect btn(LoginButtonRect());
	const TBool inside = btn.Contains(aPointerEvent.iPosition);

	switch (aPointerEvent.iType)
		{
		case TPointerEvent::EButton1Down:
			if (inside)
				{
				iLoginButtonDown = ETrue;
				DrawNow();
				}
			break;
		case TPointerEvent::EButton1Up:
			{
			const TBool wasDown = iLoginButtonDown;
			iLoginButtonDown = EFalse;
			if (wasDown && inside)
				{
				CEikAppUi* ui = CEikonEnv::Static()->EikAppUi();
				CNokiaspotifyAppUi* appUi = STATIC_CAST(CNokiaspotifyAppUi*, ui);
				appUi->HandleLoginFromViewL();
				}
			DrawNow();
			}
			break;
		default:
			break;
		}

	CCoeControl::HandlePointerEventL(aPointerEvent);
	}

TKeyResponse CNokiaspotifyAppView::OfferKeyEventL(
		const TKeyEvent& aKeyEvent,
		TEventCode aType)
	{
	if (aType == EEventKey && aKeyEvent.iRepeats == 0)
		{
		const TInt c = static_cast<TInt>(aKeyEvent.iCode);
		if (c == EKeyEnter || c == EStdKeyDevice3)
			{
			CEikAppUi* ui = CEikonEnv::Static()->EikAppUi();
			CNokiaspotifyAppUi* appUi = STATIC_CAST(CNokiaspotifyAppUi*, ui);
			appUi->HandleLoginFromViewL();
			return EKeyWasConsumed;
			}
		}
	return CCoeControl::OfferKeyEventL(aKeyEvent, aType);
	}

// End of File