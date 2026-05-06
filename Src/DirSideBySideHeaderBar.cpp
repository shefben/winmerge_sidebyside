/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997-2000  Thingamahoochie Software
//    Author: Dean Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSideBySideHeaderBar.cpp
 *
 * @brief Implementation of CDirSideBySideHeaderBar class
 */

#include "StdAfx.h"
#include "DirSideBySideHeaderBar.h"
#include "DarkModeLib.h"
#include "paths.h"
#include "resource.h"
#include <windowsx.h>
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static bool IsSystemDarkMode()
{
	COLORREF bg = ::GetSysColor(COLOR_WINDOW);
	int luminance = (GetRValue(bg) * 299 + GetGValue(bg) * 587 + GetBValue(bg) * 114) / 1000;
	return luminance < 128;
}

// Theme-aware colors for header bar
namespace BcHdr
{
	static COLORREF BG()        { return IsSystemDarkMode() ? RGB(43, 43, 43) : ::GetSysColor(COLOR_BTNFACE); }
	static COLORREF COMBO_BG()  { return IsSystemDarkMode() ? RGB(30, 30, 30) : ::GetSysColor(COLOR_WINDOW); }
	static COLORREF FG_TEXT()   { return IsSystemDarkMode() ? RGB(200, 200, 200) : ::GetSysColor(COLOR_WINDOWTEXT); }
	static COLORREF BTN_BG()   { return IsSystemDarkMode() ? RGB(50, 50, 50) : ::GetSysColor(COLOR_BTNFACE); }
	static COLORREF BTN_HOT()  { return IsSystemDarkMode() ? RGB(65, 65, 65) : ::GetSysColor(COLOR_BTNHIGHLIGHT); }
	static COLORREF BTN_PRESS(){ return IsSystemDarkMode() ? RGB(35, 35, 35) : ::GetSysColor(COLOR_BTNSHADOW); }
	static COLORREF BTN_BORDER(){ return IsSystemDarkMode() ? RGB(65, 65, 65) : ::GetSysColor(COLOR_BTNSHADOW); }
	static COLORREF ICON()     { return IsSystemDarkMode() ? RGB(170, 170, 170) : ::GetSysColor(COLOR_BTNTEXT); }
}

// Icon type constants for DrawIconButton
enum { ICON_BACK = 0, ICON_FORWARD = 1, ICON_UPLEVEL = 2, ICON_BROWSE = 3, ICON_UPLEVEL_BOTH = 4 };

// Control IDs
#define IDC_SXS_COMBO_LEFT     9801
#define IDC_SXS_COMBO_RIGHT    9802
#define IDC_SXS_BACK_LEFT      9803
#define IDC_SXS_BACK_RIGHT     9804
#define IDC_SXS_BROWSE_LEFT    9805
#define IDC_SXS_BROWSE_RIGHT   9806
#define IDC_SXS_UPLEVEL_LEFT   9807
#define IDC_SXS_UPLEVEL_RIGHT  9808
#define IDC_SXS_FORWARD_LEFT   9809
#define IDC_SXS_FORWARD_RIGHT  9810
#define IDC_SXS_UPLEVEL_BOTH   9811

// Bar height
static const int BAR_HEIGHT = 24;
// Button width
static const int BTN_W = 22;
// Vertical padding around combo
static const int PAD_Y = 2;

BEGIN_MESSAGE_MAP(CDirSideBySideHeaderBar, CDialogBar)
	ON_WM_ERASEBKGND()
	ON_WM_CTLCOLOR()
	ON_WM_DRAWITEM()
	ON_BN_CLICKED(IDC_SXS_BACK_LEFT,     OnBackLeft)
	ON_BN_CLICKED(IDC_SXS_BACK_RIGHT,    OnBackRight)
	ON_BN_CLICKED(IDC_SXS_FORWARD_LEFT,  OnForwardLeft)
	ON_BN_CLICKED(IDC_SXS_FORWARD_RIGHT, OnForwardRight)
	ON_BN_CLICKED(IDC_SXS_UPLEVEL_LEFT,  OnUpLevelLeft)
	ON_BN_CLICKED(IDC_SXS_UPLEVEL_RIGHT, OnUpLevelRight)
	ON_BN_CLICKED(IDC_SXS_BROWSE_LEFT,   OnBrowseLeft)
	ON_BN_CLICKED(IDC_SXS_BROWSE_RIGHT,  OnBrowseRight)
	ON_BN_CLICKED(IDC_SXS_UPLEVEL_BOTH,  OnUpLevelBoth)
	ON_CBN_SELCHANGE(IDC_SXS_COMBO_LEFT,  OnComboSelChangeLeft)
	ON_CBN_SELCHANGE(IDC_SXS_COMBO_RIGHT, OnComboSelChangeRight)
END_MESSAGE_MAP()

CDirSideBySideHeaderBar::CDirSideBySideHeaderBar()
	: m_nPanes(2)
	, m_nActivePane(-1)
	, m_hWndHotButton(nullptr)
{
	m_pDropHandlers[0] = nullptr;
	m_pDropHandlers[1] = nullptr;
	m_brDarkBg.CreateSolidBrush(BcHdr::BG());
	m_brDarkEdit.CreateSolidBrush(BcHdr::COMBO_BG());
}

CDirSideBySideHeaderBar::~CDirSideBySideHeaderBar()
{
	for (int pane = 0; pane < 2; pane++)
	{
		if (m_pDropHandlers[pane])
		{
			if (m_comboPath[pane].GetSafeHwnd())
				RevokeDragDrop(m_comboPath[pane].m_hWnd);
			m_pDropHandlers[pane]->Release();
			m_pDropHandlers[pane] = nullptr;
		}
	}
}

/**
 * @brief Draw a small icon inside an owner-draw button.
 * @param iconType  ICON_BACK, ICON_BROWSE, or ICON_UPLEVEL
 */
void CDirSideBySideHeaderBar::DrawIconButton(LPDRAWITEMSTRUCT lpDIS, int iconType)
{
	HDC hDC = lpDIS->hDC;
	RECT rc = lpDIS->rcItem;
	bool bPressed = (lpDIS->itemState & ODS_SELECTED) != 0;
	bool bHot = (lpDIS->hwndItem == m_hWndHotButton);

	// Background — 3-state: pressed > hover > normal
	COLORREF bg;
	if (bPressed)
		bg = BcHdr::BTN_PRESS();
	else if (bHot)
		bg = BcHdr::BTN_HOT();
	else
		bg = BcHdr::BTN_BG();
	HBRUSH hBr = CreateSolidBrush(bg);
	FillRect(hDC, &rc, hBr);
	DeleteObject(hBr);

	// Border
	HPEN hPen = CreatePen(PS_SOLID, 1, BcHdr::BTN_BORDER());
	HPEN hOld = (HPEN)SelectObject(hDC, hPen);
	HBRUSH hNull = (HBRUSH)GetStockObject(NULL_BRUSH);
	HBRUSH hOldBr = (HBRUSH)SelectObject(hDC, hNull);
	Rectangle(hDC, rc.left, rc.top, rc.right, rc.bottom);
	SelectObject(hDC, hOld);
	SelectObject(hDC, hOldBr);
	DeleteObject(hPen);

	// Draw icon using GDI
	int cx = rc.right - rc.left;
	int cy = rc.bottom - rc.top;
	int mx = rc.left + cx / 2;
	int my = rc.top + cy / 2;

	HPEN hIconPen = CreatePen(PS_SOLID, 2, BcHdr::ICON());
	SelectObject(hDC, hIconPen);

	switch (iconType)
	{
	case ICON_BACK:
		// Left-pointing arrow: < shape
		{
			int sz = 4;
			MoveToEx(hDC, mx + sz, my - sz, nullptr);
			LineTo(hDC, mx - sz + 1, my);
			MoveToEx(hDC, mx - sz + 1, my, nullptr);
			LineTo(hDC, mx + sz, my + sz);
		}
		break;

	case ICON_FORWARD:
		// Right-pointing arrow: > shape
		{
			int sz = 4;
			MoveToEx(hDC, mx - sz, my - sz, nullptr);
			LineTo(hDC, mx + sz - 1, my);
			MoveToEx(hDC, mx + sz - 1, my, nullptr);
			LineTo(hDC, mx - sz, my + sz);
		}
		break;

	case ICON_BROWSE:
		// Folder icon: simple folder outline
		{
			HPEN hFolderPen = CreatePen(PS_SOLID, 1, BcHdr::ICON());
			HPEN hPrev = (HPEN)SelectObject(hDC, hFolderPen);
			HBRUSH hFolderBr = CreateSolidBrush(RGB(180, 160, 80));
			HBRUSH hPrevBr = (HBRUSH)SelectObject(hDC, hFolderBr);
			// Folder body
			RECT rcFolder = { mx - 6, my - 2, mx + 6, my + 5 };
			Rectangle(hDC, rcFolder.left, rcFolder.top, rcFolder.right, rcFolder.bottom);
			// Tab
			RECT rcTab = { mx - 6, my - 5, mx - 1, my - 1 };
			Rectangle(hDC, rcTab.left, rcTab.top, rcTab.right, rcTab.bottom);
			SelectObject(hDC, hPrev);
			SelectObject(hDC, hPrevBr);
			DeleteObject(hFolderPen);
			DeleteObject(hFolderBr);
		}
		break;

	case ICON_UPLEVEL:
		// Up arrow: ^ shape with stem
		{
			int sz = 4;
			// Arrow head
			MoveToEx(hDC, mx - sz, my + 1, nullptr);
			LineTo(hDC, mx, my - sz + 1);
			MoveToEx(hDC, mx, my - sz + 1, nullptr);
			LineTo(hDC, mx + sz, my + 1);
			// Stem
			MoveToEx(hDC, mx, my - sz + 2, nullptr);
			LineTo(hDC, mx, my + sz);
		}
		break;

	case ICON_UPLEVEL_BOTH:
		// Folder with two up-arrows side by side; arrow tips above folder top edge.
		{
			HPEN hFolderPen = CreatePen(PS_SOLID, 1, BcHdr::ICON());
			HPEN hPrev = (HPEN)SelectObject(hDC, hFolderPen);
			HBRUSH hFolderBr = CreateSolidBrush(RGB(180, 160, 80));
			HBRUSH hPrevBr = (HBRUSH)SelectObject(hDC, hFolderBr);
			// Folder body (taller to fit arrows inside)
			RECT rcFolder = { mx - 7, my - 1, mx + 7, my + 6 };
			Rectangle(hDC, rcFolder.left, rcFolder.top, rcFolder.right, rcFolder.bottom);
			// Folder tab on top-left
			RECT rcTab = { mx - 7, my - 4, mx - 2, my };
			Rectangle(hDC, rcTab.left, rcTab.top, rcTab.right, rcTab.bottom);
			SelectObject(hDC, hPrev);
			SelectObject(hDC, hPrevBr);
			DeleteObject(hFolderPen);
			DeleteObject(hFolderBr);

			// Two solid black up-arrows; tips sit ABOVE the folder top (rcFolder.top = my - 1)
			HPEN hBlackPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
			HBRUSH hBlackBr = CreateSolidBrush(RGB(0, 0, 0));
			HPEN hPrevP = (HPEN)SelectObject(hDC, hBlackPen);
			HBRUSH hPrevB = (HBRUSH)SelectObject(hDC, hBlackBr);

			int folderTop = my - 1;
			int tipY = folderTop - 2;     // arrow tips above folder top
			int baseY = my + 5;           // arrow bases near folder bottom
			int leftCx = mx - 3;
			int rightCx = mx + 3;
			int halfW = 2;                // arrowhead half-width

			// Left arrow: triangular head + 1px shaft
			POINT tri1[3] = {
				{ leftCx - halfW, tipY + halfW + 1 },
				{ leftCx + halfW + 1, tipY + halfW + 1 },
				{ leftCx, tipY }
			};
			Polygon(hDC, tri1, 3);
			// Left shaft (1 px wide)
			RECT rcShaft1 = { leftCx, tipY + halfW + 1, leftCx + 1, baseY };
			FillRect(hDC, &rcShaft1, hBlackBr);

			// Right arrow
			POINT tri2[3] = {
				{ rightCx - halfW, tipY + halfW + 1 },
				{ rightCx + halfW + 1, tipY + halfW + 1 },
				{ rightCx, tipY }
			};
			Polygon(hDC, tri2, 3);
			RECT rcShaft2 = { rightCx, tipY + halfW + 1, rightCx + 1, baseY };
			FillRect(hDC, &rcShaft2, hBlackBr);

			SelectObject(hDC, hPrevP);
			SelectObject(hDC, hPrevB);
			DeleteObject(hBlackPen);
			DeleteObject(hBlackBr);
		}
		break;
	}

	SelectObject(hDC, GetStockObject(BLACK_PEN));
	DeleteObject(hIconPen);
}

BOOL CDirSideBySideHeaderBar::Create(CWnd* pParentWnd)
{
	if (!__super::Create(pParentWnd, CDirSideBySideHeaderBar::IDD,
		CBRS_ALIGN_TOP | CBRS_TOOLTIPS | CBRS_FLYBY, AFX_IDW_CONTROLBAR_FIRST + 28))
		return FALSE;

	// Reduce flicker during resize by clipping children
	ModifyStyle(0, WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

	// Hide the template CFilepathEdit controls — we create our own combos
	for (UINT id = IDC_STATIC_TITLE_PANE0; id <= IDC_STATIC_TITLE_PANE2; id++)
	{
		CWnd* pCtl = GetDlgItem(id);
		if (pCtl)
			pCtl->ShowWindow(SW_HIDE);
	}

	// Font for combo text
	NONCLIENTMETRICS ncm = { sizeof NONCLIENTMETRICS };
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof NONCLIENTMETRICS, &ncm, 0);
	m_font.CreateFontIndirect(&ncm.lfStatusFont);

	// Bold/larger font for button symbols (not used for drawing but kept for sizing)
	ncm.lfStatusFont.lfWeight = FW_BOLD;
	m_btnFont.CreateFontIndirect(&ncm.lfStatusFont);

	// Create combo boxes and buttons for each pane
	// Button order (BC layout): [Combo] [Back] [Forward] [Up] [Browse]
	UINT comboIDs[2]   = { IDC_SXS_COMBO_LEFT,   IDC_SXS_COMBO_RIGHT };
	UINT backIDs[2]    = { IDC_SXS_BACK_LEFT,    IDC_SXS_BACK_RIGHT };
	UINT forwardIDs[2] = { IDC_SXS_FORWARD_LEFT, IDC_SXS_FORWARD_RIGHT };
	UINT upIDs[2]      = { IDC_SXS_UPLEVEL_LEFT, IDC_SXS_UPLEVEL_RIGHT };
	UINT browseIDs[2]  = { IDC_SXS_BROWSE_LEFT,  IDC_SXS_BROWSE_RIGHT };

	for (int pane = 0; pane < 2; pane++)
	{
		// Combo box (CBS_DROPDOWN gives edit + dropdown button)
		m_comboPath[pane].Create(
			WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL,
			CRect(0, 0, 200, BAR_HEIGHT + 200), this, comboIDs[pane]);
		m_comboPath[pane].SetFont(&m_font);
		if (IsSystemDarkMode())
			SetWindowTheme(m_comboPath[pane].m_hWnd, L"", L"");

		// Back button (owner-drawn)
		m_btnBack[pane].Create(_T(""),
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
			CRect(0, 0, BTN_W, BAR_HEIGHT), this, backIDs[pane]);

		// Forward button (owner-drawn)
		m_btnForward[pane].Create(_T(""),
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
			CRect(0, 0, BTN_W, BAR_HEIGHT), this, forwardIDs[pane]);

		// Up-level button (owner-drawn)
		m_btnUpLevel[pane].Create(_T(""),
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
			CRect(0, 0, BTN_W, BAR_HEIGHT), this, upIDs[pane]);

		// Browse button (owner-drawn)
		m_btnBrowse[pane].Create(_T(""),
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
			CRect(0, 0, BTN_W, BAR_HEIGHT), this, browseIDs[pane]);
	}

	// Center "Up Both" button (between panes)
	m_btnUpLevelBoth.Create(_T(""),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
		CRect(0, 0, BTN_W, BAR_HEIGHT), this, IDC_SXS_UPLEVEL_BOTH);

	// Register drop targets on each combo's edit area
	for (int pane = 0; pane < 2; pane++)
	{
		m_pDropHandlers[pane] = new DropHandler(
			[this, pane](const std::vector<String>& files) { OnDropFiles(pane, files); });
		RegisterDragDrop(m_comboPath[pane].m_hWnd, m_pDropHandlers[pane]);
	}

	return TRUE;
}

CSize CDirSideBySideHeaderBar::CalcFixedLayout(BOOL bStretch, BOOL bHorz)
{
	return CSize(SHRT_MAX, BAR_HEIGHT + 2 * PAD_Y);
}

BOOL CDirSideBySideHeaderBar::OnEraseBkgnd(CDC* pDC)
{
	CRect rc;
	GetClientRect(&rc);
	pDC->FillSolidRect(&rc, BcHdr::BG());
	return TRUE;
}

HBRUSH CDirSideBySideHeaderBar::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	// Dark theme for the combo's edit child
	if (nCtlColor == CTLCOLOR_EDIT)
	{
		for (int pane = 0; pane < m_nPanes; pane++)
		{
			CWnd* pParent = pWnd->GetParent();
			if (pParent && pParent->GetSafeHwnd() == m_comboPath[pane].GetSafeHwnd())
			{
				pDC->SetBkColor(BcHdr::COMBO_BG());
				pDC->SetTextColor(BcHdr::FG_TEXT());
				return (HBRUSH)m_brDarkEdit.GetSafeHandle();
			}
		}
	}
	// Dark theme for the dropdown list
	if (nCtlColor == CTLCOLOR_LISTBOX)
	{
		pDC->SetBkColor(BcHdr::COMBO_BG());
		pDC->SetTextColor(BcHdr::FG_TEXT());
		return (HBRUSH)m_brDarkEdit.GetSafeHandle();
	}
	return __super::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CDirSideBySideHeaderBar::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS)
{
	switch (nIDCtl)
	{
	case IDC_SXS_BACK_LEFT:
	case IDC_SXS_BACK_RIGHT:
		DrawIconButton(lpDIS, ICON_BACK);
		return;
	case IDC_SXS_FORWARD_LEFT:
	case IDC_SXS_FORWARD_RIGHT:
		DrawIconButton(lpDIS, ICON_FORWARD);
		return;
	case IDC_SXS_BROWSE_LEFT:
	case IDC_SXS_BROWSE_RIGHT:
		DrawIconButton(lpDIS, ICON_BROWSE);
		return;
	case IDC_SXS_UPLEVEL_LEFT:
	case IDC_SXS_UPLEVEL_RIGHT:
		DrawIconButton(lpDIS, ICON_UPLEVEL);
		return;
	case IDC_SXS_UPLEVEL_BOTH:
		DrawIconButton(lpDIS, ICON_UPLEVEL_BOTH);
		return;
	}
	__super::OnDrawItem(nIDCtl, lpDIS);
}

/**
 * @brief Resize with no explicit widths — split evenly.
 */
void CDirSideBySideHeaderBar::Resize()
{
	if (m_hWnd == nullptr)
		return;
	CRect rc;
	GetClientRect(&rc);
	int half = rc.Width() / 2;
	int widths[2] = { half, rc.Width() - half };
	int offsets[2] = { 0, half };
	Resize(widths, offsets);
}

void CDirSideBySideHeaderBar::Resize(int widths[])
{
	int offsets[2] = { 0, widths[0] };
	Resize(widths, offsets);
}

/**
 * @brief Layout controls to match splitter column positions.
 *
 * Left pane layout:  [ComboBox][Back][Forward][Up][Browse][UpBoth]
 * Right pane layout: [ComboBox][Back][Forward][Up][Browse]
 * The UpBoth button sits at the right edge of the left pane,
 * visually between the two panes.
 */
void CDirSideBySideHeaderBar::Resize(int widths[], int offsets[])
{
	if (m_hWnd == nullptr)
		return;

	const int comboH = BAR_HEIGHT;
	const int btnCount = 4;  // Back, Forward, Up, Browse (per side)
	const int buttonsW = btnCount * BTN_W;
	const bool bHasUpBoth = (m_nPanes >= 2 && m_btnUpLevelBoth.GetSafeHwnd() != nullptr);

	HDWP hDWP = ::BeginDeferWindowPos(m_nPanes * 5 + 1);
	if (!hDWP)
		return;

	for (int pane = 0; pane < m_nPanes; pane++)
	{
		int x = offsets[pane];
		int w = widths[pane];
		// Left pane reserves an extra BTN_W for the UpBoth button
		int extra = (pane == 0 && bHasUpBoth) ? BTN_W : 0;
		int comboW = w - buttonsW - extra - 1;
		if (comboW < 80) comboW = 80;

		if (m_comboPath[pane].GetSafeHwnd())
			hDWP = ::DeferWindowPos(hDWP, m_comboPath[pane].m_hWnd, nullptr,
				x, PAD_Y, comboW, comboH + 200,
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);

		int bx = x + comboW + 1;
		if (m_btnBack[pane].GetSafeHwnd())
			hDWP = ::DeferWindowPos(hDWP, m_btnBack[pane].m_hWnd, nullptr,
				bx, PAD_Y, BTN_W, comboH,
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
		bx += BTN_W;
		if (m_btnForward[pane].GetSafeHwnd())
			hDWP = ::DeferWindowPos(hDWP, m_btnForward[pane].m_hWnd, nullptr,
				bx, PAD_Y, BTN_W, comboH,
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
		bx += BTN_W;
		if (m_btnUpLevel[pane].GetSafeHwnd())
			hDWP = ::DeferWindowPos(hDWP, m_btnUpLevel[pane].m_hWnd, nullptr,
				bx, PAD_Y, BTN_W, comboH,
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
		bx += BTN_W;
		if (m_btnBrowse[pane].GetSafeHwnd())
			hDWP = ::DeferWindowPos(hDWP, m_btnBrowse[pane].m_hWnd, nullptr,
				bx, PAD_Y, BTN_W, comboH,
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);

		// Place UpBoth at the right edge of the left pane
		if (pane == 0 && bHasUpBoth)
		{
			bx += BTN_W;
			hDWP = ::DeferWindowPos(hDWP, m_btnUpLevelBoth.m_hWnd, nullptr,
				bx, PAD_Y, BTN_W, comboH,
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
		}
	}

	::EndDeferWindowPos(hDWP);
}

// --- IHeaderBar implementation ---

String CDirSideBySideHeaderBar::GetCaption(int pane) const
{
	ASSERT(pane >= 0 && pane < 2);
	if (m_hWnd == nullptr || !m_comboPath[pane].GetSafeHwnd())
		return _T("");
	CString str;
	m_comboPath[pane].GetWindowText(str);
	return String(str);
}

void CDirSideBySideHeaderBar::SetCaption(int pane, const String& sCaption)
{
	ASSERT(pane >= 0 && pane < 2);
	if (m_hWnd == nullptr || !m_comboPath[pane].GetSafeHwnd())
		return;
	m_comboPath[pane].SetWindowText(sCaption.c_str());
	AddPathToHistory(pane, sCaption);
}

String CDirSideBySideHeaderBar::GetPath(int pane) const
{
	return GetCaption(pane);
}

void CDirSideBySideHeaderBar::SetPath(int pane, const String& sPath)
{
	SetCaption(pane, sPath);
}

int CDirSideBySideHeaderBar::GetActive() const
{
	return m_nActivePane;
}

void CDirSideBySideHeaderBar::SetActive(int pane, bool bActive)
{
	if (pane >= 0 && pane < 2)
	{
		if (bActive)
			m_nActivePane = pane;
		else if (m_nActivePane == pane)
			m_nActivePane = -1;
	}
}

void CDirSideBySideHeaderBar::EditActivePanePath()
{
	if (m_nActivePane >= 0 && m_nActivePane < 2)
		m_comboPath[m_nActivePane].SetFocus();
}

// --- Combo selection ---

void CDirSideBySideHeaderBar::OnComboSelChange(UINT id)
{
	int pane = (id == IDC_SXS_COMBO_LEFT) ? 0 : 1;
	int sel = m_comboPath[pane].GetCurSel();
	if (sel >= 0 && sel < static_cast<int>(m_pathHistory[pane].size()))
	{
		String selectedPath = m_pathHistory[pane][sel];
		if (m_folderSelectedCallbackfunc)
			m_folderSelectedCallbackfunc(pane, selectedPath);
	}
}

void CDirSideBySideHeaderBar::OnComboSelChangeLeft()  { OnComboSelChange(IDC_SXS_COMBO_LEFT); }
void CDirSideBySideHeaderBar::OnComboSelChangeRight() { OnComboSelChange(IDC_SXS_COMBO_RIGHT); }

// --- Button handlers ---

void CDirSideBySideHeaderBar::OnBackLeft()     { if (m_backCallbackfunc)    m_backCallbackfunc(0); }
void CDirSideBySideHeaderBar::OnBackRight()    { if (m_backCallbackfunc)    m_backCallbackfunc(1); }
void CDirSideBySideHeaderBar::OnForwardLeft()  { if (m_forwardCallbackfunc) m_forwardCallbackfunc(0); }
void CDirSideBySideHeaderBar::OnForwardRight() { if (m_forwardCallbackfunc) m_forwardCallbackfunc(1); }
void CDirSideBySideHeaderBar::OnBrowseLeft()   { if (m_browseCallbackfunc)  m_browseCallbackfunc(0); }
void CDirSideBySideHeaderBar::OnBrowseRight()  { if (m_browseCallbackfunc)  m_browseCallbackfunc(1); }
void CDirSideBySideHeaderBar::OnUpLevelLeft()  { if (m_upLevelCallbackfunc) m_upLevelCallbackfunc(0); }
void CDirSideBySideHeaderBar::OnUpLevelRight() { if (m_upLevelCallbackfunc) m_upLevelCallbackfunc(1); }

// --- Path history ---

void CDirSideBySideHeaderBar::AddPathToHistory(int pane, const String& sPath)
{
	if (pane < 0 || pane >= 2 || sPath.empty())
		return;

	auto& history = m_pathHistory[pane];

	// Remove existing duplicate (case-insensitive)
	for (auto it = history.begin(); it != history.end(); ++it)
	{
		if (_tcsicmp(it->c_str(), sPath.c_str()) == 0)
		{
			history.erase(it);
			break;
		}
	}

	history.insert(history.begin(), sPath);
	if (history.size() > 20)
		history.resize(20);

	// Update dropdown list without clearing the edit text
	if (m_comboPath[pane].GetSafeHwnd())
	{
		// Save the current edit text before resetting
		CString curText;
		m_comboPath[pane].GetWindowText(curText);
		m_comboPath[pane].ResetContent();
		for (const auto& path : history)
			m_comboPath[pane].AddString(path.c_str());
		// Restore the edit text (ResetContent clears it on CBS_DROPDOWN combos)
		m_comboPath[pane].SetWindowText(curText);
	}
}

// --- OnCommand: intercept BN_CLICKED before CControlBar routes to parent frame ---

BOOL CDirSideBySideHeaderBar::OnCommand(WPARAM wParam, LPARAM lParam)
{
	UINT nID = LOWORD(wParam);
	int nCode = HIWORD(wParam);

	// Control notification (lParam != 0 means WM_COMMAND came from a child control)
	if (lParam != 0)
	{
		if (nCode == BN_CLICKED)
		{
			switch (nID)
			{
			case IDC_SXS_BACK_LEFT:     OnBackLeft();     return TRUE;
			case IDC_SXS_BACK_RIGHT:    OnBackRight();    return TRUE;
			case IDC_SXS_FORWARD_LEFT:  OnForwardLeft();  return TRUE;
			case IDC_SXS_FORWARD_RIGHT: OnForwardRight(); return TRUE;
			case IDC_SXS_UPLEVEL_LEFT:  OnUpLevelLeft();  return TRUE;
			case IDC_SXS_UPLEVEL_RIGHT: OnUpLevelRight(); return TRUE;
			case IDC_SXS_BROWSE_LEFT:   OnBrowseLeft();   return TRUE;
			case IDC_SXS_BROWSE_RIGHT:  OnBrowseRight();  return TRUE;
			case IDC_SXS_UPLEVEL_BOTH:  OnUpLevelBoth();  return TRUE;
			}
		}
		else if (nCode == CBN_SELCHANGE)
		{
			if (nID == IDC_SXS_COMBO_LEFT || nID == IDC_SXS_COMBO_RIGHT)
			{
				OnComboSelChange(nID);
				return TRUE;
			}
		}
	}

	return __super::OnCommand(wParam, lParam);
}

// --- WindowProc: hover tracking for owner-draw buttons ---

LRESULT CDirSideBySideHeaderBar::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_MOUSEMOVE)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		HWND hChild = ::ChildWindowFromPoint(m_hWnd, pt);
		if (hChild != m_hWndHotButton)
		{
			if (m_hWndHotButton && ::IsWindow(m_hWndHotButton))
				::InvalidateRect(m_hWndHotButton, nullptr, FALSE);
			m_hWndHotButton = nullptr;
			if (hChild && hChild != m_hWnd)
			{
				int id = ::GetDlgCtrlID(hChild);
				if (id >= IDC_SXS_BACK_LEFT && id <= IDC_SXS_UPLEVEL_BOTH)
				{
					m_hWndHotButton = hChild;
					::InvalidateRect(hChild, nullptr, FALSE);
					TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
					TrackMouseEvent(&tme);
				}
			}
		}
	}
	if (message == WM_MOUSELEAVE)
	{
		if (m_hWndHotButton && ::IsWindow(m_hWndHotButton))
			::InvalidateRect(m_hWndHotButton, nullptr, FALSE);
		m_hWndHotButton = nullptr;
	}

	return __super::WindowProc(message, wParam, lParam);
}

// --- PreTranslateMessage: handle Enter key in combo edit ---

BOOL CDirSideBySideHeaderBar::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
	{
		// Check if the focused window is a combo edit child
		CWnd* pFocus = GetFocus();
		if (pFocus)
		{
			for (int pane = 0; pane < m_nPanes; pane++)
			{
				if (!m_comboPath[pane].GetSafeHwnd())
					continue;
				// CBS_DROPDOWN has a child Edit control
				CWnd* pEdit = m_comboPath[pane].GetWindow(GW_CHILD);
				if (pEdit && pFocus->GetSafeHwnd() == pEdit->GetSafeHwnd())
				{
					CString text;
					m_comboPath[pane].GetWindowText(text);
					String sPath(text.GetString());
					if (!sPath.empty() && m_folderSelectedCallbackfunc)
						m_folderSelectedCallbackfunc(pane, sPath);
					return TRUE;
				}
			}
		}
	}
	return __super::PreTranslateMessage(pMsg);
}

// --- Up Both handler ---

void CDirSideBySideHeaderBar::OnUpLevelBoth()
{
	// Navigate both sides up one level via dedicated callback
	if (m_upBothCallbackfunc)
		m_upBothCallbackfunc();
	else if (m_upLevelCallbackfunc)
		m_upLevelCallbackfunc(0); // fallback
}

// --- Drop handler ---

void CDirSideBySideHeaderBar::OnDropFiles(int pane, const std::vector<String>& files)
{
	if (files.empty() || pane < 0 || pane >= m_nPanes)
		return;

	String path = files[0];
	if (paths::DoesPathExist(path) == paths::IS_EXISTING_FILE)
		path = paths::GetParentPath(path);

	if (m_folderSelectedCallbackfunc)
		m_folderSelectedCallbackfunc(pane, path);
}
