/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSxSToolBar.cpp
 *
 * @brief Implementation of CDirSxSToolBar class
 */

#include "StdAfx.h"
#include "DirSxSToolBar.h"
#include "resource.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Beyond Compare dark theme colors for toolbar
namespace BcToolbarColors
{
	static const COLORREF BG       = RGB(45, 48, 50);
	static const COLORREF TEXT     = RGB(200, 200, 200);
	static const COLORREF BORDER   = RGB(70, 75, 75);
}

IMPLEMENT_DYNAMIC(CDirSxSToolBar, CToolBar)

// Icon type indices for CreateIcon16()
enum SxsIconType
{
	ICON_HOME = 0,
	ICON_SESSIONS,
	ICON_DIFFS,
	ICON_ALL,
	ICON_SAME,
	ICON_STRUCTURE,
	ICON_MINOR,
	ICON_RULES,
	ICON_COPY,
	ICON_MOVE,
	ICON_EXPAND,
	ICON_COLLAPSE,
	ICON_SELECT,
	ICON_FILES,
	ICON_REFRESH,
	ICON_SWAP,
	ICON_STOP,
	ICON_COUNT
};

// Button definitions
static const struct {
	UINT nID;
	int iImage;
	BYTE fsStyle;
	const tchar_t* pszText;
} SxsButtons[] = {
	{ ID_DIR_SXS_HOME,             ICON_HOME,      TBSTYLE_BUTTON,                            _T("Home") },
	{ ID_DIR_SXS_SESSIONS,         ICON_SESSIONS,  TBSTYLE_BUTTON | BTNS_DROPDOWN,            _T("Sessions") },
	{ ID_DIR_SXS_NEXT_DIFF,        ICON_DIFFS,     TBSTYLE_BUTTON | BTNS_DROPDOWN,            _T("Diffs") },
	{ ID_DIR_SXS_FILTER_ALL,       ICON_ALL,       TBSTYLE_BUTTON,                            _T("All") },
	{ ID_DIR_SXS_FILTER_IDENTICAL, ICON_SAME,      TBSTYLE_BUTTON,                            _T("Same") },
	{ ID_DIR_SXS_STRUCTURE,        ICON_STRUCTURE,  TBSTYLE_BUTTON | BTNS_DROPDOWN,            _T("Structure") },
	{ ID_DIR_SXS_SHOW_MINOR,       ICON_MINOR,     TBSTYLE_BUTTON,                            _T("Minor") },
	{ ID_DIR_SXS_SESSION_SETTINGS, ICON_RULES,     TBSTYLE_BUTTON,                            _T("Rules") },
	{ 0,                            0,              TBSTYLE_SEP,                               nullptr },
	{ ID_DIR_SXS_COPY_TO_FOLDER,   ICON_COPY,      TBSTYLE_BUTTON,                            _T("Copy") },
	{ ID_DIR_SXS_MOVE_TO_FOLDER,   ICON_MOVE,      TBSTYLE_BUTTON,                            _T("Move") },
	{ 0,                            0,              TBSTYLE_SEP,                               nullptr },
	{ ID_DIR_SXS_EXPAND_ALL,       ICON_EXPAND,    TBSTYLE_BUTTON,                            _T("Expand") },
	{ ID_DIR_SXS_COLLAPSE_ALL,     ICON_COLLAPSE,  TBSTYLE_BUTTON,                            _T("Collapse") },
	{ ID_DIR_SXS_SELECT_ALL,       ICON_SELECT,    TBSTYLE_BUTTON,                            _T("Select") },
	{ ID_DIR_SXS_FILES_BUTTON,     ICON_FILES,     TBSTYLE_BUTTON,                            _T("Files") },
	{ 0,                            0,              TBSTYLE_SEP,                               nullptr },
	{ ID_DIR_SXS_REFRESH,          ICON_REFRESH,   TBSTYLE_BUTTON,                            _T("Refresh") },
	{ ID_DIR_SXS_SWAP_SIDES,       ICON_SWAP,      TBSTYLE_BUTTON,                            _T("Swap") },
	{ ID_DIR_SXS_STOP,             ICON_STOP,      TBSTYLE_BUTTON,                            _T("Stop") },
};

static const int NUM_BUTTONS = _countof(SxsButtons);

CDirSxSToolBar::CDirSxSToolBar()
{
}

CDirSxSToolBar::~CDirSxSToolBar()
{
}

BEGIN_MESSAGE_MAP(CDirSxSToolBar, CToolBar)
	ON_NOTIFY_REFLECT(TBN_DROPDOWN, OnDropDown)
	ON_WM_ERASEBKGND()
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnCustomDraw)
END_MESSAGE_MAP()

BOOL CDirSxSToolBar::OnEraseBkgnd(CDC* pDC)
{
	CRect rc;
	GetClientRect(&rc);
	pDC->FillSolidRect(&rc, BcToolbarColors::BG);
	return TRUE;
}

void CDirSxSToolBar::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTBCUSTOMDRAW lpTBCD = (LPNMTBCUSTOMDRAW)pNMHDR;
	*pResult = CDRF_DODEFAULT;

	if (lpTBCD->nmcd.dwDrawStage == CDDS_PREPAINT)
	{
		*pResult = CDRF_NOTIFYITEMDRAW;
		return;
	}
	if (lpTBCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
	{
		HDC hDC = lpTBCD->nmcd.hdc;
		RECT rc = lpTBCD->nmcd.rc;
		bool bHot = (lpTBCD->nmcd.uItemState & CDIS_HOT) != 0;
		bool bSelected = (lpTBCD->nmcd.uItemState & CDIS_SELECTED) != 0;

		// Draw button background manually for proper dark theme
		COLORREF clrBg = bSelected ? RGB(35, 38, 40) :
			(bHot ? RGB(65, 70, 72) : BcToolbarColors::BG);
		HBRUSH hBrush = CreateSolidBrush(clrBg);
		FillRect(hDC, &rc, hBrush);
		DeleteObject(hBrush);

		// Draw subtle border on hover/pressed
		if (bHot || bSelected)
		{
			HPEN hPen = CreatePen(PS_SOLID, 1, BcToolbarColors::BORDER);
			HPEN hOldPen = (HPEN)SelectObject(hDC, hPen);
			HBRUSH hNull = (HBRUSH)GetStockObject(NULL_BRUSH);
			HBRUSH hOldBr = (HBRUSH)SelectObject(hDC, hNull);
			Rectangle(hDC, rc.left, rc.top, rc.right, rc.bottom);
			SelectObject(hDC, hOldPen);
			SelectObject(hDC, hOldBr);
			DeleteObject(hPen);
		}

		lpTBCD->clrText = BcToolbarColors::TEXT;
		lpTBCD->clrBtnFace = BcToolbarColors::BG;
		lpTBCD->clrBtnHighlight = RGB(60, 65, 68);
		*pResult = TBCDRF_USECDCOLORS | TBCDRF_NOBACKGROUND;
		return;
	}
}

/**
 * @brief Create a 16x16 icon bitmap for the toolbar using GDI drawing.
 */
HBITMAP CDirSxSToolBar::CreateIcon16(int iconType)
{
	const int sz = 20;
	// Create a 32-bit DIB section
	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = sz;
	bmi.bmiHeader.biHeight = -sz; // top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void* pBits = nullptr;
	HDC hScreenDC = ::GetDC(nullptr);
	HBITMAP hBmp = CreateDIBSection(hScreenDC, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
	HDC hMemDC = CreateCompatibleDC(hScreenDC);
	::ReleaseDC(nullptr, hScreenDC);

	HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hBmp);

	// Fill with dark bg (transparent key)
	COLORREF clrTransparent = BcToolbarColors::BG;
	HBRUSH hBrush = CreateSolidBrush(clrTransparent);
	RECT rcFull = { 0, 0, sz, sz };
	FillRect(hMemDC, &rcFull, hBrush);
	DeleteObject(hBrush);

	// Helper macros
	auto FillBox = [&](int x, int y, int w, int h, COLORREF clr) {
		HBRUSH hb = CreateSolidBrush(clr);
		RECT r = { x, y, x + w, y + h };
		FillRect(hMemDC, &r, hb);
		DeleteObject(hb);
	};
	auto DrawLine = [&](int x1, int y1, int x2, int y2, COLORREF clr) {
		HPEN hPen = CreatePen(PS_SOLID, 1, clr);
		HPEN hOld = (HPEN)SelectObject(hMemDC, hPen);
		MoveToEx(hMemDC, x1, y1, nullptr);
		LineTo(hMemDC, x2, y2);
		SelectObject(hMemDC, hOld);
		DeleteObject(hPen);
	};

	switch (iconType)
	{
	case ICON_HOME: // House: green
		{
			COLORREF c = RGB(0, 200, 0);
			// Roof triangle (simplified as lines)
			DrawLine(8, 1, 1, 7, c);
			DrawLine(8, 1, 15, 7, c);
			DrawLine(1, 7, 15, 7, c);
			// Body
			FillBox(3, 7, 10, 8, c);
			// Door
			FillBox(6, 10, 4, 5, RGB(0, 140, 0));
		}
		break;
	case ICON_SESSIONS: // Two stacked pages: blue
		{
			COLORREF c = RGB(70, 130, 220);
			FillBox(4, 1, 9, 11, c);
			FillBox(2, 3, 9, 11, RGB(100, 160, 240));
			// Page fold
			FillBox(4, 1, 3, 2, RGB(50, 100, 180));
		}
		break;
	case ICON_DIFFS: // Two vertical bars: red+green
		FillBox(2, 2, 4, 12, RGB(220, 60, 60));
		FillBox(10, 2, 4, 12, RGB(0, 200, 0));
		break;
	case ICON_ALL: // Grid of 4 squares: green
		{
			COLORREF c = RGB(0, 180, 0);
			FillBox(1, 1, 6, 6, c);
			FillBox(9, 1, 6, 6, c);
			FillBox(1, 9, 6, 6, c);
			FillBox(9, 9, 6, 6, c);
		}
		break;
	case ICON_SAME: // Equals sign: blue
		{
			COLORREF c = RGB(70, 130, 220);
			FillBox(2, 4, 12, 3, c);
			FillBox(2, 9, 12, 3, c);
		}
		break;
	case ICON_STRUCTURE: // Folder with tree lines: yellow
		{
			COLORREF c = RGB(220, 200, 50);
			FillBox(1, 2, 6, 2, c);  // tab
			FillBox(1, 4, 14, 10, c); // body
			// Tree lines inside
			DrawLine(4, 7, 12, 7, RGB(160, 140, 20));
			DrawLine(4, 10, 12, 10, RGB(160, 140, 20));
		}
		break;
	case ICON_MINOR: // Small circle: orange
		{
			COLORREF c = RGB(220, 160, 0);
			HBRUSH hb = CreateSolidBrush(c);
			HPEN hPen = CreatePen(PS_SOLID, 1, c);
			HBRUSH hOldBr = (HBRUSH)SelectObject(hMemDC, hb);
			HPEN hOldPn = (HPEN)SelectObject(hMemDC, hPen);
			Ellipse(hMemDC, 4, 4, 12, 12);
			SelectObject(hMemDC, hOldBr);
			SelectObject(hMemDC, hOldPn);
			DeleteObject(hb);
			DeleteObject(hPen);
		}
		break;
	case ICON_RULES: // Gear/wrench: gray
		{
			COLORREF c = RGB(180, 180, 180);
			// Simplified gear shape
			FillBox(5, 1, 6, 2, c);
			FillBox(5, 13, 6, 2, c);
			FillBox(1, 5, 2, 6, c);
			FillBox(13, 5, 2, 6, c);
			FillBox(4, 4, 8, 8, c);
			// Center hole
			FillBox(6, 6, 4, 4, clrTransparent);
		}
		break;
	case ICON_COPY: // Two overlapping rectangles: cyan
		{
			COLORREF c1 = RGB(80, 180, 220);
			COLORREF c2 = RGB(60, 140, 180);
			FillBox(4, 1, 10, 10, c1);
			FillBox(1, 4, 10, 10, c2);
		}
		break;
	case ICON_MOVE: // Rectangle with arrow: cyan+green
		{
			FillBox(1, 3, 8, 10, RGB(80, 180, 220));
			// Right arrow
			COLORREF ca = RGB(0, 180, 0);
			FillBox(9, 7, 5, 2, ca);
			DrawLine(12, 4, 15, 8, ca);
			DrawLine(12, 11, 15, 8, ca);
		}
		break;
	case ICON_EXPAND: // Plus in box: green
		{
			COLORREF c = RGB(0, 180, 0);
			// Box outline
			FillBox(1, 1, 14, 1, c);
			FillBox(1, 14, 14, 1, c);
			FillBox(1, 1, 1, 14, c);
			FillBox(14, 1, 1, 14, c);
			// Plus
			FillBox(7, 4, 2, 8, c);
			FillBox(4, 7, 8, 2, c);
		}
		break;
	case ICON_COLLAPSE: // Minus in box: red
		{
			COLORREF c = RGB(200, 60, 60);
			FillBox(1, 1, 14, 1, c);
			FillBox(1, 14, 14, 1, c);
			FillBox(1, 1, 1, 14, c);
			FillBox(14, 1, 1, 14, c);
			FillBox(4, 7, 8, 2, c);
		}
		break;
	case ICON_SELECT: // Checkmark in box: green
		{
			COLORREF c = RGB(0, 200, 0);
			FillBox(1, 1, 14, 1, c);
			FillBox(1, 14, 14, 1, c);
			FillBox(1, 1, 1, 14, c);
			FillBox(14, 1, 1, 14, c);
			DrawLine(4, 8, 7, 12, c);
			DrawLine(7, 12, 12, 4, c);
		}
		break;
	case ICON_FILES: // Single document page: white/light
		{
			COLORREF c = RGB(220, 220, 220);
			FillBox(3, 1, 10, 14, c);
			// Page fold
			FillBox(10, 1, 3, 3, RGB(180, 180, 180));
			// Lines
			DrawLine(5, 5, 11, 5, RGB(150, 150, 150));
			DrawLine(5, 7, 11, 7, RGB(150, 150, 150));
			DrawLine(5, 9, 11, 9, RGB(150, 150, 150));
		}
		break;
	case ICON_REFRESH: // Two curved arrows: green
		{
			COLORREF c = RGB(0, 200, 0);
			// Simplified circular arrows
			HPEN hPen = CreatePen(PS_SOLID, 2, c);
			HPEN hOld = (HPEN)SelectObject(hMemDC, hPen);
			HBRUSH hNull = (HBRUSH)GetStockObject(NULL_BRUSH);
			HBRUSH hOldBr = (HBRUSH)SelectObject(hMemDC, hNull);
			Arc(hMemDC, 2, 2, 14, 14, 8, 2, 2, 8);
			Arc(hMemDC, 2, 2, 14, 14, 2, 14, 14, 2);
			SelectObject(hMemDC, hOld);
			SelectObject(hMemDC, hOldBr);
			DeleteObject(hPen);
			// Arrow tips
			FillBox(6, 1, 3, 2, c);
			FillBox(7, 13, 3, 2, c);
		}
		break;
	case ICON_SWAP: // Two bidirectional arrows: yellow
		{
			COLORREF c = RGB(220, 200, 50);
			// Left arrow
			FillBox(1, 4, 10, 2, c);
			DrawLine(1, 5, 4, 2, c);
			DrawLine(1, 5, 4, 8, c);
			// Right arrow
			FillBox(5, 10, 10, 2, c);
			DrawLine(15, 11, 12, 8, c);
			DrawLine(15, 11, 12, 14, c);
		}
		break;
	case ICON_STOP: // Red X in circle
		{
			COLORREF c = RGB(220, 50, 50);
			HBRUSH hb = CreateSolidBrush(c);
			HPEN hPen = CreatePen(PS_SOLID, 1, c);
			HBRUSH hOldBr = (HBRUSH)SelectObject(hMemDC, hb);
			HPEN hOldPn = (HPEN)SelectObject(hMemDC, hPen);
			Ellipse(hMemDC, 1, 1, 15, 15);
			SelectObject(hMemDC, hOldBr);
			SelectObject(hMemDC, hOldPn);
			DeleteObject(hb);
			DeleteObject(hPen);
			// White X
			HPEN hW = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
			HPEN hO2 = (HPEN)SelectObject(hMemDC, hW);
			MoveToEx(hMemDC, 4, 4, nullptr); LineTo(hMemDC, 12, 12);
			MoveToEx(hMemDC, 12, 4, nullptr); LineTo(hMemDC, 4, 12);
			SelectObject(hMemDC, hO2);
			DeleteObject(hW);
		}
		break;
	}

	SelectObject(hMemDC, hOldBmp);
	DeleteDC(hMemDC);
	return hBmp;
}

/**
 * @brief Build the image list with all 17 GDI-drawn icons.
 */
void CDirSxSToolBar::CreateToolbarIcons()
{
	m_imageList.Create(20, 20, ILC_COLOR32 | ILC_MASK, ICON_COUNT, 0);
	for (int i = 0; i < ICON_COUNT; i++)
	{
		CBitmap bmp;
		bmp.Attach(CreateIcon16(i));
		m_imageList.Add(&bmp, BcToolbarColors::BG);
	}
}

/**
 * @brief Create the toolbar with icon+text buttons (BC-style).
 */
BOOL CDirSxSToolBar::Create(CWnd* pParentWnd)
{
	if (!CToolBar::CreateEx(pParentWnd,
		TBSTYLE_FLAT | TBSTYLE_TOOLTIPS,
		WS_CHILD | CBRS_TOP | CBRS_TOOLTIPS | CBRS_FLYBY,
		CRect(0, 0, 0, 0), AFX_IDW_CONTROLBAR_FIRST + 29))
	{
		return FALSE;
	}

	// Enable dropdown arrows
	GetToolBarCtrl().SetExtendedStyle(
		GetToolBarCtrl().GetExtendedStyle() | TBSTYLE_EX_DRAWDDARROWS);

	// Create and attach the image list
	CreateToolbarIcons();
	GetToolBarCtrl().SetImageList(&m_imageList);

	// Build the TBBUTTON array
	TBBUTTON tbButtons[NUM_BUTTONS];
	for (int i = 0; i < NUM_BUTTONS; i++)
	{
		memset(&tbButtons[i], 0, sizeof(TBBUTTON));
		if (SxsButtons[i].fsStyle & TBSTYLE_SEP)
		{
			tbButtons[i].iBitmap = 8;
			tbButtons[i].idCommand = 0;
			tbButtons[i].fsState = 0;
			tbButtons[i].fsStyle = TBSTYLE_SEP;
			tbButtons[i].iString = -1;
		}
		else
		{
			tbButtons[i].iBitmap = SxsButtons[i].iImage;
			tbButtons[i].idCommand = SxsButtons[i].nID;
			tbButtons[i].fsState = TBSTATE_ENABLED;
			tbButtons[i].fsStyle = SxsButtons[i].fsStyle;
			tbButtons[i].iString = GetToolBarCtrl().AddStrings(SxsButtons[i].pszText);
		}
	}

	GetToolBarCtrl().AddButtons(NUM_BUTTONS, tbButtons);

	// Set button + bitmap sizes for icon-beside-text layout (BC style: 20x20 icons, ~55px wide)
	GetToolBarCtrl().SetBitmapSize(CSize(20, 20));
	GetToolBarCtrl().SetButtonSize(CSize(55, 38));

	SetBarStyle(GetBarStyle() & ~CBRS_BORDER_ANY);

	// Disable visual styles so custom draw colors take full effect
	SetWindowTheme(m_hWnd, L"", L"");

	return TRUE;
}

/**
 * @brief Handle TBN_DROPDOWN notifications for dropdown buttons.
 */
void CDirSxSToolBar::OnDropDown(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMTOOLBAR* pTB = (NMTOOLBAR*)pNMHDR;
	*pResult = TBDDRET_DEFAULT;

	switch (pTB->iItem)
	{
	case ID_DIR_SXS_NEXT_DIFF:
		ShowDiffsDropdown();
		*pResult = TBDDRET_DEFAULT;
		break;
	case ID_DIR_SXS_STRUCTURE:
		ShowStructureDropdown();
		*pResult = TBDDRET_DEFAULT;
		break;
	case ID_DIR_SXS_SESSIONS:
		ShowSessionsDropdown();
		*pResult = TBDDRET_DEFAULT;
		break;
	}
}

/**
 * @brief Show the Diffs dropdown with 10 BC-style presets.
 */
void CDirSxSToolBar::ShowDiffsDropdown()
{
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_DIFFS_SHOW_DIFFS,          _T("Show Differences"));
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_DIFFS_NO_ORPHANS,          _T("Show No Orphans"));
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_DIFFS_NO_ORPHANS_DIFF,     _T("Show Differences but No Orphans"));
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_DIFFS_ORPHANS,             _T("Show Orphans"));
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_DIFFS_LEFT_NEWER,          _T("Show Left Newer"));
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_DIFFS_RIGHT_NEWER,         _T("Show Right Newer"));
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_DIFFS_LEFT_NEWER_ORPHANS,  _T("Show Left Newer and Left Orphans"));
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_DIFFS_RIGHT_NEWER_ORPHANS, _T("Show Right Newer and Right Orphans"));
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_DIFFS_LEFT_ORPHANS,        _T("Show Left Orphans"));
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_DIFFS_RIGHT_ORPHANS,       _T("Show Right Orphans"));

	// Position below the button
	CRect rc;
	GetToolBarCtrl().GetItemRect(GetToolBarCtrl().CommandToIndex(ID_DIR_SXS_NEXT_DIFF), &rc);
	ClientToScreen(&rc);
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_TOPALIGN, rc.left, rc.bottom, GetParentFrame());
}

/**
 * @brief Show the Structure dropdown with 4 modes.
 */
void CDirSxSToolBar::ShowStructureDropdown()
{
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_STRUCT_ALWAYS_FOLDERS,     _T("Always Show Folders"));
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_STRUCT_FILES_AND_FOLDERS,  _T("Compare Files and Folder Structure"));
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_STRUCT_ONLY_FILES,         _T("Only Compare Files"));
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_STRUCT_IGNORE_STRUCTURE,   _T("Ignore Folder Structure"));

	CRect rc;
	GetToolBarCtrl().GetItemRect(GetToolBarCtrl().CommandToIndex(ID_DIR_SXS_STRUCTURE), &rc);
	ClientToScreen(&rc);
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_TOPALIGN, rc.left, rc.bottom, GetParentFrame());
}

/**
 * @brief Show the Sessions dropdown.
 */
void CDirSxSToolBar::ShowSessionsDropdown()
{
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_SESSION_SAVE, _T("Save Session..."));
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_SESSION_LOAD, _T("Load Session..."));
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_WORKSPACE_SAVE, _T("Save Workspace..."));
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_WORKSPACE_LOAD, _T("Load Workspace..."));

	CRect rc;
	GetToolBarCtrl().GetItemRect(GetToolBarCtrl().CommandToIndex(ID_DIR_SXS_SESSIONS), &rc);
	ClientToScreen(&rc);
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_TOPALIGN, rc.left, rc.bottom, GetParentFrame());
}
