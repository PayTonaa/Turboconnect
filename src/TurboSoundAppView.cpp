/*
 ============================================================================
 Name		: TurboSoundAppView.cpp
 ============================================================================
 */
#include <coemain.h>
#include <gdi.h>
#include <eikenv.h>
#include <aknlists.h>
#include <eikclbd.h>
#include <eikscrlb.h>
#include <badesca.h>
#include <bamdesca.h>
#include <aknutils.h>
#include <w32std.h>
#include <e32keys.h>
#include "TurboSoundAppView.h"
const TInt KHeaderHeight = 32;
const TInt KStatusBarHeight = 22;
const TInt KSeekStepSeconds = 10;
_LIT(KLabelPauza, "PAUZA");
// Chrome (gora/dol): granat + cyjan jak na stronie. Panel "Teraz odtwarzane"
// pozostaje ciemny (bez cyjanowego tla).
static const TRgb KBgDeep      (0x0B, 0x10, 0x26);  // tlo glowne (granat)
static const TRgb KBgPanel     (0x12, 0x1B, 0x35);  // header + status bg
static const TRgb KCyanPrimary (0x00, 0xF0, 0xFF);  // akcent chrome
static const TRgb KFontMain    (0xE6, 0xF7, 0xFF);

// Panel odtwarzania (ciemny):
static const TRgb KNpBgTop     (0x06, 0x06, 0x06);
static const TRgb KNpBgBottom  (0x12, 0x12, 0x12);
static const TRgb KNpLine      (0x38, 0x38, 0x38);
static const TRgb KNpTextMuted (0x8A, 0x8A, 0x8A);
static const TRgb KNpBarFill   (0x4A, 0x4A, 0x4A);
static const TRgb KAccentWarn  (0xFF, 0x9D, 0x00);
static void SplitTrackMetaL(const TDesC& aFileName, HBufC*& aTitle, HBufC*& aArtist)
	{
	delete aTitle;
	aTitle = NULL;
	delete aArtist;
	aArtist = NULL;
	TFileName base;
	base.Copy(aFileName);
	base.Trim();
	const TInt dot = base.LocateReverse('.');
	if (dot > 0)
		{
		base.SetLength(dot);
		}
	_LIT(KSep, " - ");
	const TInt sep = base.Find(KSep);
	if (sep >= 1 && (sep + KSep().Length()) < base.Length())
		{
		aArtist = base.Left(sep).AllocL();
		aArtist->Des().Trim();
		aTitle = base.Mid(sep + KSep().Length()).AllocL();
		aTitle->Des().Trim();
		}
	else
		{
		aTitle = base.AllocL();
		aTitle->Des().Trim();
		aArtist = HBufC::NewL(32);
		aArtist->Des().Copy(_L("Turboconnect"));
		}
	}
static void AppendMmSs(TDes& aBuf, TInt aTotalSec)
	{
	aBuf.Zero();
	if (aTotalSec < 0)
		{
		aTotalSec = 0;
		}
	const TInt m = aTotalSec / 60;
	const TInt s = aTotalSec % 60;
	aBuf.AppendNum(m / 10);
	aBuf.AppendNum(m % 10);
	aBuf.Append(_L(":"));
	aBuf.AppendNum(s / 10);
	aBuf.AppendNum(s % 10);
	}
CTurboSoundAppView* CTurboSoundAppView::NewL(const TRect& aRect, MTurboSoundTrackObserver& aObserver)
	{
	CTurboSoundAppView* self = NewLC(aRect, aObserver);
	CleanupStack::Pop(self);
	return self;
	}
CTurboSoundAppView* CTurboSoundAppView::NewLC(const TRect& aRect, MTurboSoundTrackObserver& aObserver)
	{
	CTurboSoundAppView* self = new (ELeave) CTurboSoundAppView(aObserver);
	CleanupStack::PushL(self);
	self->ConstructL(aRect);
	return self;
	}
CTurboSoundAppView::CTurboSoundAppView(MTurboSoundTrackObserver& aObserver)
	: iObserver(aObserver), iNpKeys(NULL), iListBox(NULL), iStatus(NULL),
	  iTrackCount(0), iNpActive(EFalse), iNpPaused(EFalse), iNpTitle(NULL), iNpArtist(NULL),
	  iNpCurSec(0), iNpTotalSec(0), iKeyDiagOnce(EFalse)
	{
	}

void CTurboSoundAppView::EnableKeyDiagnostic()
	{
	iKeyDiagOnce = ETrue;
	if (iStatus)
		{
		TRAP_IGNORE(SetStatusL(_L("Diagnostyka: wcisnij dowolny klawisz")));
		}
	}
CTurboSoundAppView::~CTurboSoundAppView()
	{
	delete iListBox;
	iListBox = NULL;
	delete iStatus;
	iStatus = NULL;
	delete iNpTitle;
	iNpTitle = NULL;
	delete iNpArtist;
	iNpArtist = NULL;
	}
void CTurboSoundAppView::ConstructL(const TRect& aRect)
	{
	CreateWindowL();
	iStatus = KNullDesC().AllocL();
	iListBox = new (ELeave) CAknSingleStyleListBox;
	iListBox->SetContainerWindowL(*this);
	iListBox->ConstructL(this, EAknListBoxSelectionList);
	iListBox->CreateScrollBarFrameL(ETrue);
	iListBox->ScrollBarFrame()->SetScrollBarVisibilityL(
			CEikScrollBarFrame::EOff, CEikScrollBarFrame::EAuto);

	SetRect(aRect);
	ActivateL();
	}
void CTurboSoundAppView::SetNowPlayingKeysObserver(MTurboSoundNowPlayingKeys* aKeys)
	{
	iNpKeys = aKeys;
	}
void CTurboSoundAppView::ShowNowPlayingL(const TDesC& aDisplayName)
	{
	HBufC* title = NULL;
	HBufC* artist = NULL;
	SplitTrackMetaL(aDisplayName, title, artist);
	delete iNpTitle;
	delete iNpArtist;
	iNpTitle = title;
	iNpArtist = artist;
	iNpCurSec = 0;
	iNpTotalSec = 0;
	iNpPaused = EFalse;
	iNpActive = ETrue;
	if (iListBox)
		{
		iListBox->MakeVisible(EFalse);
		}
	DrawDeferred();
	}
void CTurboSoundAppView::HideNowPlayingL()
	{
	if (!iNpActive)
		{
		return;
		}
	iNpActive = EFalse;
	iNpPaused = EFalse;
	delete iNpTitle;
	iNpTitle = NULL;
	delete iNpArtist;
	iNpArtist = NULL;
	if (iListBox)
		{
		iListBox->MakeVisible(ETrue);
		}
	DrawDeferred();
	}
void CTurboSoundAppView::SetNowPlayingProgressL(TInt aCurSec, TInt aTotalSec)
	{
	if (!iNpActive)
		{
		return;
		}
	iNpCurSec = aCurSec;
	if (aTotalSec > 0)
		{
		iNpTotalSec = aTotalSec;
		}
	DrawDeferred();
	}
TBool CTurboSoundAppView::NowPlayingActive() const
	{
	return iNpActive;
	}
void CTurboSoundAppView::SetNowPlayingPausedL(TBool aPaused)
	{
	if (!iNpActive)
		{
		return;
		}
	iNpPaused = aPaused;
	DrawDeferred();
	}
void CTurboSoundAppView::SetTrackNamesL(const MDesCArray& aNames)
	{
	if (!iListBox)
		{
		return;
		}
	CDesCArray* items = static_cast<CDesCArray*>(iListBox->Model()->ItemTextArray());
	items->Reset();
	const TInt count = aNames.MdcaCount();
	for (TInt i = 0; i < count; i++)
		{
		TBuf<256> row;
		row.Append('\t');
		const TPtrC name = aNames.MdcaPoint(i);
		const TInt safe = Min(name.Length(), row.MaxLength() - row.Length());
		row.Append(name.Left(safe));
		items->AppendL(row);
		}
	iListBox->HandleItemAdditionL();
	if (count > 0)
		{
		iListBox->SetCurrentItemIndex(0);
		}
	iTrackCount = count;
	DrawDeferred();
	}
void CTurboSoundAppView::SetCurrentItemIndex(TInt aIndex)
	{
	if (iListBox && aIndex >= 0 && aIndex < iTrackCount)
		{
		iListBox->SetCurrentItemIndex(aIndex);
		iListBox->DrawNow();
		}
	}
TInt CTurboSoundAppView::CurrentItemIndex() const
	{
	return iListBox ? iListBox->CurrentItemIndex() : -1;
	}
void CTurboSoundAppView::SetStatusL(const TDesC& aText)
	{
	HBufC* copy = aText.AllocL();
	delete iStatus;
	iStatus = copy;
	DrawNow();
	if (iCoeEnv)
		{
		iCoeEnv->WsSession().Flush();
		}
	}
TRect CTurboSoundAppView::ListAreaRect() const
	{
	TRect r(Rect());
	r.iTl.iY += KHeaderHeight;
	r.iBr.iY -= KStatusBarHeight;
	return r;
	}
void CTurboSoundAppView::DrawNowPlayingPanel(CWindowGc& aGc, const TRect& aArea) const
	{
	// Tlo panelu: gradient czerni / ciemnoszarego (bez cyjanu)
	const TInt h = aArea.Height();
	for (TInt y = 0; y < h; y++)
		{
		const TInt t = (y * 255) / Max(1, h - 1);
		const TInt r = KNpBgTop.Red()   + ((KNpBgBottom.Red()   - KNpBgTop.Red())   * t) / 255;
		const TInt g = KNpBgTop.Green() + ((KNpBgBottom.Green() - KNpBgTop.Green()) * t) / 255;
		const TInt b = KNpBgTop.Blue()  + ((KNpBgBottom.Blue()  - KNpBgTop.Blue())  * t) / 255;
		aGc.SetPenColor(TRgb(r, g, b));
		aGc.DrawLine(TPoint(aArea.iTl.iX, aArea.iTl.iY + y),
		             TPoint(aArea.iBr.iX - 1, aArea.iTl.iY + y));
		}
	aGc.SetPenStyle(CGraphicsContext::ESolidPen);
	aGc.SetPenColor(KNpLine);
	TRect rOuter = aArea;
	rOuter.Shrink(1, 1);
	aGc.DrawRect(rOuter);
	aGc.SetPenColor(KNpBarFill);
	TRect rInner = aArea;
	rInner.Shrink(3, 3);
	aGc.DrawRect(rInner);
	const CFont* fontTitle = CEikonEnv::Static()->TitleFont();
	const CFont* fontLeg = CEikonEnv::Static()->LegendFont();
	TInt y = aArea.iTl.iY + 8;
	aGc.UseFont(fontLeg);
	aGc.SetPenColor(KNpTextMuted);
	aGc.DrawText(_L("TERAZ ODTWARZANE"), TPoint(aArea.iTl.iX + 10, y + fontLeg->AscentInPixels()));
	if (iNpPaused)
		{
		const TInt pw = fontLeg->TextWidthInPixels(KLabelPauza()) + 10;
		TRect badge(aArea.iBr.iX - pw - 10, y,
		            aArea.iBr.iX - 6, y + fontLeg->HeightInPixels() + 4);
		aGc.SetBrushColor(KNpBgTop);
		aGc.SetPenColor(KAccentWarn);
		aGc.DrawRect(badge);
		TRect badgeInner = badge;
		badgeInner.Shrink(1, 1);
		aGc.DrawRect(badgeInner);
		aGc.SetPenColor(KFontMain);
		aGc.DrawText(KLabelPauza(), TPoint(badge.iTl.iX + 5, y + fontLeg->AscentInPixels() + 2));
		}
	y += fontLeg->HeightInPixels() + 6;
	aGc.DiscardFont();
	TRect art(aArea);
	art.iTl.iX = aArea.iTl.iX + (aArea.Width() - 96) / 2;
	art.iBr.iX = art.iTl.iX + 96;
	art.iTl.iY = y;
	art.iBr.iY = y + 72;
	for (TInt row = 0; row < art.Height(); row++)
		{
		const TInt tt = (row * 120) / Max(1, art.Height() - 1);
		const TInt vv = 0x14 + ((0x22 - 0x14) * tt) / 120;
		aGc.SetPenColor(TRgb(vv, vv, vv));
		aGc.DrawLine(TPoint(art.iTl.iX, art.iTl.iY + row),
		             TPoint(art.iBr.iX - 1, art.iTl.iY + row));
		}
	aGc.SetPenColor(KNpLine);
	aGc.DrawRect(art);
	aGc.UseFont(fontTitle);
	aGc.SetPenColor(KFontMain);
	aGc.DrawText(_L("TC"), TPoint(art.iTl.iX + 34, art.iTl.iY + 28 + fontTitle->AscentInPixels() / 2));
	aGc.DiscardFont();
	y = art.iBr.iY + 10;
	aGc.UseFont(fontTitle);
	aGc.SetPenColor(KFontMain);
	if (iNpTitle && iNpTitle->Length() > 0)
		{
		TBuf<120> line;
		line.Copy(iNpTitle->Left(Min(iNpTitle->Length(), line.MaxLength())));
		while (line.Length() > 0
		       && fontTitle->TextWidthInPixels(line) > aArea.Width() - 20)
			{
			line.SetLength(line.Length() - 1);
			}
		aGc.DrawText(line, TPoint(aArea.iTl.iX + 10, y + fontTitle->AscentInPixels()));
		}
	y += fontTitle->HeightInPixels() + 2;
	aGc.DiscardFont();
	aGc.UseFont(fontLeg);
	aGc.SetPenColor(KNpTextMuted);
	if (iNpArtist && iNpArtist->Length() > 0)
		{
		TBuf<100> al;
		al.Copy(iNpArtist->Left(Min(iNpArtist->Length(), al.MaxLength())));
		while (al.Length() > 0
		       && fontLeg->TextWidthInPixels(al) > aArea.Width() - 20)
			{
			al.SetLength(al.Length() - 1);
			}
		aGc.DrawText(al, TPoint(aArea.iTl.iX + 10, y + fontLeg->AscentInPixels()));
		}
	y += fontLeg->HeightInPixels() + 10;
	aGc.DiscardFont();
	const TInt barH = 16;
	const TInt barY = y;
	TRect barBg(aArea.iTl.iX + 10, barY, aArea.iBr.iX - 10, barY + barH);
	aGc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	aGc.SetBrushColor(KNpBgTop);
	aGc.SetPenColor(KNpTextMuted);
	aGc.DrawRect(barBg);
	TRect barInner = barBg;
	barInner.Shrink(2, 2);
	aGc.SetBrushColor(TRgb(0x0A, 0x0A, 0x0A));
	aGc.SetPenColor(TRgb(0x0A, 0x0A, 0x0A));
	aGc.DrawRect(barInner);
	TInt fillW = 0;
	if (iNpTotalSec > 0 && iNpCurSec >= 0)
		{
		fillW = (barInner.Width() * iNpCurSec) / iNpTotalSec;
		if (fillW > barInner.Width())
			{
			fillW = barInner.Width();
			}
		}
	if (fillW > 0)
		{
		TRect barFill(barInner.iTl.iX, barInner.iTl.iY,
		              barInner.iTl.iX + fillW, barInner.iBr.iY);
		aGc.SetBrushColor(KNpBarFill);
		aGc.SetPenColor(KNpBarFill);
		aGc.DrawRect(barFill);
		aGc.SetPenColor(KFontMain);
		aGc.DrawLine(TPoint(barFill.iTl.iX, barFill.iBr.iY - 1),
		             TPoint(barFill.iBr.iX - 1, barFill.iBr.iY - 1));
		}
	if (fillW > 0 && barInner.Width() > 0)
		{
		const TInt hx = barInner.iTl.iX + fillW;
		TRect head(hx - 2, barInner.iTl.iY - 2, hx + 2, barInner.iBr.iY + 2);
		aGc.SetBrushColor(KFontMain);
		aGc.SetPenColor(KNpBgTop);
		aGc.DrawRect(head);
		}
	y = barBg.iBr.iY + 6;
	TBuf<16> tL;
	TBuf<16> tR;
	AppendMmSs(tL, iNpCurSec);
	AppendMmSs(tR, iNpTotalSec);
	aGc.UseFont(fontLeg);
	aGc.SetPenColor(KFontMain);
	aGc.DrawText(tL, TPoint(barInner.iTl.iX, y + fontLeg->AscentInPixels()));
	const TInt tw = fontLeg->TextWidthInPixels(tR);
	aGc.DrawText(tR, TPoint(barInner.iBr.iX - tw, y + fontLeg->AscentInPixels()));
	y += fontLeg->HeightInPixels() + 6;
	aGc.SetPenColor(KNpTextMuted);
	aGc.DrawText(_L("Enter = pauza/wznow   < > = utwory   4/6 = -/+10s"), TPoint(aArea.iTl.iX + 10, y + fontLeg->AscentInPixels()));
	y += fontLeg->HeightInPixels();
	aGc.DrawText(_L("2/8 = glosnosc   Gora = ukryj panel"), TPoint(aArea.iTl.iX + 10, y + fontLeg->AscentInPixels()));
	aGc.DiscardFont();
	}
void CTurboSoundAppView::Draw(const TRect& /*aRect*/) const
	{
	CWindowGc& gc = SystemGc();
	const TRect drawRect(Rect());
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.SetPenStyle(CGraphicsContext::ESolidPen);
	gc.SetBrushColor(KBgDeep);
	gc.SetPenColor(KBgDeep);
	gc.Clear(drawRect);
	TRect header = drawRect;
	header.iBr.iY = header.iTl.iY + KHeaderHeight;
	gc.SetBrushColor(KBgPanel);
	gc.SetPenColor(KBgPanel);
	gc.DrawRect(header);
	gc.SetPenColor(KCyanPrimary);
	gc.DrawLine(TPoint(header.iTl.iX, header.iBr.iY - 1),
	            TPoint(header.iBr.iX, header.iBr.iY - 1));
	const CFont* fontBig = CEikonEnv::Static()->TitleFont();
	const CFont* fontSmall = CEikonEnv::Static()->LegendFont();
	gc.UseFont(fontBig);
	gc.SetPenColor(KFontMain);
	const TInt titleBaseY = header.iTl.iY + (KHeaderHeight / 2) + (fontBig->AscentInPixels() / 2) - 2;
	gc.DrawText(_L("TURBOCONNECT.E75"), TPoint(header.iTl.iX + 6, titleBaseY));
	gc.DiscardFont();
	gc.UseFont(fontSmall);
	gc.SetPenColor(KCyanPrimary);
	TBuf<24> counter;
	counter.Format(_L("%d TRACK(S)"), iTrackCount);
	const TInt counterWidth = fontSmall->TextWidthInPixels(counter);
	gc.DrawText(counter,
	            TPoint(header.iBr.iX - counterWidth - 6,
	                   header.iTl.iY + (KHeaderHeight / 2) + (fontSmall->AscentInPixels() / 2) - 2));
	gc.DiscardFont();
	TRect status = drawRect;
	status.iTl.iY = status.iBr.iY - KStatusBarHeight;
	gc.SetBrushColor(KBgPanel);
	gc.SetPenColor(KBgPanel);
	gc.DrawRect(status);
	gc.SetPenColor(KCyanPrimary);
	gc.DrawLine(TPoint(status.iTl.iX, status.iTl.iY),
	            TPoint(status.iBr.iX, status.iTl.iY));
	TRect accent(status.iTl.iX, status.iTl.iY + 2,
	             status.iTl.iX + 3, status.iBr.iY - 2);
	gc.SetBrushColor(KCyanPrimary);
	gc.SetPenColor(KCyanPrimary);
	gc.DrawRect(accent);
	if (iStatus && iStatus->Length() > 0)
		{
		gc.UseFont(fontSmall);
		gc.SetPenColor(KFontMain);
		const TInt baseLine = status.iBr.iY - 5;
		const TInt maxW = status.Width() - 14;
		TBuf<160> shortStatus;
		shortStatus.Copy(iStatus->Left(Min(iStatus->Length(),
		                                   shortStatus.MaxLength())));
		while (shortStatus.Length() > 0
		       && fontSmall->TextWidthInPixels(shortStatus) > maxW)
			{
			shortStatus.SetLength(shortStatus.Length() - 1);
			}
		gc.DrawText(shortStatus, TPoint(status.iTl.iX + 8, baseLine));
		gc.DiscardFont();
		}
	if (iNpActive)
		{
		DrawNowPlayingPanel(gc, ListAreaRect());
		}
	}
void CTurboSoundAppView::SizeChanged()
	{
	if (iListBox)
		{
		TRect listRect = ListAreaRect();
		iListBox->SetRect(listRect);
		}
	}
TInt CTurboSoundAppView::CountComponentControls() const
	{
	return iListBox ? 1 : 0;
	}
CCoeControl* CTurboSoundAppView::ComponentControl(TInt aIndex) const
	{
	if (aIndex == 0)
		{
		return iListBox;
		}
	return NULL;
	}
TKeyResponse CTurboSoundAppView::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
	{
	const TUint code = aKeyEvent.iCode;
	const TInt scan  = aKeyEvent.iScanCode;

	if (iKeyDiagOnce && aType == EEventKey)
		{
		iKeyDiagOnce = EFalse;
		TBuf<80> dbg;
		dbg.Format(_L("KEY: code=0x%X (%d)  scan=0x%X (%d)  type=%d"),
		           (TInt)code, (TInt)code, scan, scan, (TInt)aType);
		TRAP_IGNORE(SetStatusL(dbg));
		return EKeyWasConsumed;
		}

	// Klawisze glosnosci dzialaja zawsze, jezeli mamy obserwatora i strumien.
	// Akceptujemy zarowno znane kody MMF, jak i typowe skany E-series oraz
	// klawisze 2/8 z klawiatury numerycznej kiedy panel jest aktywny.
	if ((aType == EEventKey || aType == EEventKeyRepeat) && iNpKeys)
		{
		const TBool isVolUp =
				(code == EKeyIncVolume)
				|| (scan == EStdKeyIncVolume)
				|| (scan == 0xED)
				|| (scan == 0xE5)
				|| (iNpActive && code == '8')
				|| (iNpActive && scan == EStdKeyUpArrow && code == 0);
		const TBool isVolDn =
				(code == EKeyDecVolume)
				|| (scan == EStdKeyDecVolume)
				|| (scan == 0xEE)
				|| (scan == 0xE4)
				|| (iNpActive && code == '2');
		if (isVolUp && iNpKeys->HandleNpVolumeDeltaL(+1))
			{
			return EKeyWasConsumed;
			}
		if (isVolDn && iNpKeys->HandleNpVolumeDeltaL(-1))
			{
			return EKeyWasConsumed;
			}
		}

	if (iNpActive && iNpKeys)
		{
		const TBool isCenterKey =
				code == EKeyEnter
				|| code == EKeyOK
				|| code == EKeyDevice3
				|| scan == EStdKeyEnter
				|| scan == EStdKeyDevice3;
		const TBool isLeft =
				code == EKeyLeftArrow
				|| scan == EStdKeyLeftArrow;
		const TBool isRight =
				code == EKeyRightArrow
				|| scan == EStdKeyRightArrow;
		const TBool isUp =
				code == EKeyUpArrow
				|| scan == EStdKeyUpArrow;
		const TBool isSeekBack = (code == '4');
		const TBool isSeekFwd  = (code == '6');

		if (aType == EEventKey)
			{
			if (isCenterKey)
				{
				// Srodkowy klawisz to PAUZA / WZNOW. Panel zostaje na ekranie.
				TRAP_IGNORE(iNpKeys->HandleNpPauseToggleL());
				return EKeyWasConsumed;
				}
			if (isUp)
				{
				TRAP_IGNORE(iNpKeys->HandleNpDismissL());
				return EKeyWasConsumed;
				}
			if (isLeft)
				{
				TRAP_IGNORE(iNpKeys->HandleNpPrevL());
				return EKeyWasConsumed;
				}
			if (isRight)
				{
				TRAP_IGNORE(iNpKeys->HandleNpNextL());
				return EKeyWasConsumed;
				}
			if (isSeekBack)
				{
				TRAP_IGNORE(iNpKeys->HandleNpSeekRelativeL(-KSeekStepSeconds));
				return EKeyWasConsumed;
				}
			if (isSeekFwd)
				{
				TRAP_IGNORE(iNpKeys->HandleNpSeekRelativeL(KSeekStepSeconds));
				return EKeyWasConsumed;
				}
			return EKeyWasConsumed;
			}

		if (aType == EEventKeyRepeat)
			{
			if (isSeekBack)
				{
				TRAP_IGNORE(iNpKeys->HandleNpSeekRelativeL(-KSeekStepSeconds));
				return EKeyWasConsumed;
				}
			if (isSeekFwd)
				{
				TRAP_IGNORE(iNpKeys->HandleNpSeekRelativeL(KSeekStepSeconds));
				return EKeyWasConsumed;
				}
			return EKeyWasConsumed;
			}

		return EKeyWasConsumed;
		}
	if (!iListBox)
		{
		return EKeyWasNotConsumed;
		}
	if (aType == EEventKey)
		{
		const TBool isSelect =
				code == EKeyEnter
				|| code == EKeyOK
				|| code == EKeyDevice3
				|| scan == EStdKeyEnter
				|| scan == EStdKeyDevice3;
		if (isSelect)
			{
			const TInt idx = iListBox->CurrentItemIndex();
			if (idx >= 0)
				{
				iObserver.HandleTrackSelectedL(idx);
				}
			return EKeyWasConsumed;
			}
		}
	return iListBox->OfferKeyEventL(aKeyEvent, aType);
	}
void CTurboSoundAppView::HandlePointerEventL(const TPointerEvent& aPointerEvent)
	{
	CCoeControl::HandlePointerEventL(aPointerEvent);
	}
