/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSideBySideFilterBar.cpp
 *
 * @brief Implementation of CDirSideBySideFilterBar class
 */

#include "StdAfx.h"
#include "DirSideBySideFilterBar.h"
#include "DirSideBySideCoordinator.h"
#include "DirPaneView.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "resource.h"
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Beyond Compare dark theme colors for filter bar
namespace BcFilterColors
{
	static const COLORREF BG       = RGB(45, 48, 50);
	static const COLORREF EDIT_BG  = RGB(35, 38, 40);
	static const COLORREF TEXT     = RGB(200, 200, 200);
	static const COLORREF BTN_BG  = RGB(55, 60, 62);
}

// Internal control IDs for the in-memory dialog children
static const UINT IDC_FILTER_LABEL = 5001;
static const UINT IDC_FILTER_EDIT  = 5002;
static const UINT IDC_FILTER_BTN   = 5003;
static const UINT IDC_PEEK_BTN     = 5004;

// Popup menu item IDs (base + offset)
static const UINT ID_FILTER_POPUP_ALL        = 6001;
static const UINT ID_FILTER_POPUP_DIFFERENT  = 6002;
static const UINT ID_FILTER_POPUP_IDENTICAL  = 6003;
static const UINT ID_FILTER_POPUP_ORPHANS_L  = 6004;
static const UINT ID_FILTER_POPUP_ORPHANS_R  = 6005;
static const UINT ID_FILTER_POPUP_NEWER_L    = 6006;
static const UINT ID_FILTER_POPUP_NEWER_R    = 6007;
static const UINT ID_FILTER_POPUP_SKIPPED    = 6008;
static const UINT ID_FILTER_POPUP_SUPPRESS   = 6009;
static const UINT ID_FILTER_POPUP_ADVANCED   = 6010;

IMPLEMENT_DYNAMIC(CDirSideBySideFilterBar, CControlBar)

CDirSideBySideFilterBar::CDirSideBySideFilterBar()
	: m_pCoordinator(nullptr)
{
	m_brDarkBg.CreateSolidBrush(BcFilterColors::BG);
	m_brDarkEdit.CreateSolidBrush(BcFilterColors::EDIT_BG);
}

CDirSideBySideFilterBar::~CDirSideBySideFilterBar()
{
}

BEGIN_MESSAGE_MAP(CDirSideBySideFilterBar, CControlBar)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CTLCOLOR()
	ON_WM_DRAWITEM()
	ON_BN_CLICKED(IDC_PEEK_BTN, OnPeek)
	ON_COMMAND(ID_DIR_SXS_FILTER_ALL, OnFilterAll)
	ON_COMMAND(ID_DIR_SXS_FILTER_DIFFERENT, OnFilterDifferent)
	ON_COMMAND(ID_DIR_SXS_FILTER_IDENTICAL, OnFilterIdentical)
	ON_COMMAND(ID_DIR_SXS_FILTER_ORPHANS_L, OnFilterOrphansL)
	ON_COMMAND(ID_DIR_SXS_FILTER_ORPHANS_R, OnFilterOrphansR)
	ON_COMMAND(ID_DIR_SXS_FILTER_NEWER_L, OnFilterNewerL)
	ON_COMMAND(ID_DIR_SXS_FILTER_NEWER_R, OnFilterNewerR)
	ON_COMMAND(ID_DIR_SXS_FILTER_SKIPPED, OnFilterSkipped)
	ON_COMMAND(ID_DIR_SXS_SUPPRESS_FILTERS, OnSuppressFilters)
	ON_EN_KILLFOCUS(IDC_FILTER_EDIT, OnNameFilterChanged)
	ON_BN_CLICKED(IDC_FILTER_BTN, OnFiltersDropdown)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_ALL, OnUpdateFilterAll)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_DIFFERENT, OnUpdateFilterDifferent)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_IDENTICAL, OnUpdateFilterIdentical)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_ORPHANS_L, OnUpdateFilterOrphansL)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_ORPHANS_R, OnUpdateFilterOrphansR)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_NEWER_L, OnUpdateFilterNewerL)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_NEWER_R, OnUpdateFilterNewerR)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_SKIPPED, OnUpdateFilterSkipped)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_SUPPRESS_FILTERS, OnUpdateSuppressFilters)
	ON_COMMAND(ID_DIR_SXS_ADV_FILTER, OnAdvancedFilter)
END_MESSAGE_MAP()

/**
 * @brief Create the BC-style filter bar with a text field and Filters button.
 * Uses CControlBar::Create for a simple child window, then creates controls manually.
 */
BOOL CDirSideBySideFilterBar::Create(CWnd* pParentWnd)
{
	// Create as a simple control bar window
	if (!CControlBar::Create(nullptr, _T("SxSFilterBar"),
		WS_CHILD | WS_VISIBLE | CBRS_TOP, CRect(0, 0, 0, 0), pParentWnd,
		AFX_IDW_CONTROLBAR_FIRST + 30))
	{
		return FALSE;
	}

	// Apply control bar style
	SetBarStyle(GetBarStyle() | CBRS_TOP | CBRS_TOOLTIPS | CBRS_FLYBY);
	SetBarStyle(GetBarStyle() & ~CBRS_BORDER_ANY);

	// Determine font
	NONCLIENTMETRICS ncm = { sizeof NONCLIENTMETRICS };
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof NONCLIENTMETRICS, &ncm, 0);
	m_editFont.CreateFontIndirect(&ncm.lfStatusFont);

	// Compute dimensions
	CClientDC dc(this);
	const int dpi = dc.GetDeviceCaps(LOGPIXELSX);
	auto px = [dpi](int pt) { return MulDiv(pt, dpi, 72); };
	int barH = px(22);
	int editH = px(16);
	int y = (barH - editH) / 2;
	int x = px(4);

	// Create "Filter:" label
	CRect rcLabel(x, y, x + px(36), y + editH);
	m_labelFilter.Create(_T("Filter:"), WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
		rcLabel, this, IDC_FILTER_LABEL);
	m_labelFilter.SetFont(&m_editFont);
	x = rcLabel.right + px(4);

	// Create wide filter edit
	int editW = px(250);
	CRect rcEdit(x, y, x + editW, y + editH);
	m_editFilter.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
		rcEdit, this, IDC_FILTER_EDIT);
	m_editFilter.SetFont(&m_editFont);
	m_editFilter.SetCueBanner(_T("e.g. *.cpp;*.h"));
	x = rcEdit.right + px(6);

	// Restore saved filter pattern
	String savedFilter = GetOptionsMgr()->GetString(OPT_DIRVIEW_SXS_NAME_FILTER);
	if (!savedFilter.empty())
		m_editFilter.SetWindowText(savedFilter.c_str());

	// Create "Filters" dropdown button (owner-drawn for dark theme)
	int btnW = px(52);
	CRect rcBtn(x, y, x + btnW, y + editH);
	m_btnFilters.Create(_T("Filters..."), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
		rcBtn, this, IDC_FILTER_BTN);
	m_btnFilters.SetFont(&m_editFont);
	SetWindowTheme(m_btnFilters.m_hWnd, L"", L"");
	x = rcBtn.right + px(4);

	// Create "Peek" toggle button (owner-drawn for dark theme)
	int peekW = px(40);
	CRect rcPeek(x, y, x + peekW, y + editH);
	m_btnPeek.Create(_T("Peek"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
		rcPeek, this, IDC_PEEK_BTN);
	m_btnPeek.SetFont(&m_editFont);
	SetWindowTheme(m_btnPeek.m_hWnd, L"", L"");

	// Update peek button state from saved option
	if (GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_SUPPRESS_FILTERS))
		m_btnPeek.SetWindowText(_T("Peek*"));

	return TRUE;
}

void CDirSideBySideFilterBar::UpdateButtonStates()
{
	if (GetSafeHwnd())
		Invalidate();
}

CSize CDirSideBySideFilterBar::CalcFixedLayout(BOOL bStretch, BOOL bHorz)
{
	CClientDC dc(const_cast<CDirSideBySideFilterBar*>(this));
	const int dpi = dc.GetDeviceCaps(LOGPIXELSY);
	int barH = MulDiv(24, dpi, 72);
	return CSize(bStretch ? SHRT_MAX : 0, barH);
}

// Helper to toggle a boolean option and redisplay
static void ToggleOption(const String& optName, CDirSideBySideCoordinator* pCoord)
{
	bool bCurrent = GetOptionsMgr()->GetBool(optName);
	GetOptionsMgr()->SaveOption(optName, !bCurrent);
	if (pCoord)
		pCoord->Redisplay();
}

BOOL CDirSideBySideFilterBar::OnEraseBkgnd(CDC* pDC)
{
	CRect rc;
	GetClientRect(&rc);
	pDC->FillSolidRect(&rc, BcFilterColors::BG);
	return TRUE;
}

HBRUSH CDirSideBySideFilterBar::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	if (nCtlColor == CTLCOLOR_EDIT)
	{
		pDC->SetBkColor(BcFilterColors::EDIT_BG);
		pDC->SetTextColor(BcFilterColors::TEXT);
		return (HBRUSH)m_brDarkEdit.GetSafeHandle();
	}
	if (nCtlColor == CTLCOLOR_STATIC)
	{
		pDC->SetBkMode(TRANSPARENT);
		pDC->SetTextColor(BcFilterColors::TEXT);
		return (HBRUSH)m_brDarkBg.GetSafeHandle();
	}
	if (nCtlColor == CTLCOLOR_BTN)
	{
		pDC->SetBkColor(BcFilterColors::BG);
		pDC->SetTextColor(BcFilterColors::TEXT);
		return (HBRUSH)m_brDarkBg.GetSafeHandle();
	}
	return CControlBar::OnCtlColor(pDC, pWnd, nCtlColor);
}

/**
 * @brief Owner-draw handler for dark-themed filter bar buttons.
 */
void CDirSideBySideFilterBar::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS)
{
	if (nIDCtl == IDC_FILTER_BTN || nIDCtl == IDC_PEEK_BTN)
	{
		HDC hDC = lpDIS->hDC;
		RECT rc = lpDIS->rcItem;
		bool bPressed = (lpDIS->itemState & ODS_SELECTED) != 0;

		// Background
		COLORREF bg = bPressed ? RGB(35, 38, 40) : BcFilterColors::BTN_BG;
		HBRUSH hBrush = CreateSolidBrush(bg);
		FillRect(hDC, &rc, hBrush);
		DeleteObject(hBrush);

		// Border
		HPEN hPen = CreatePen(PS_SOLID, 1, RGB(70, 75, 78));
		HPEN hOldPen = (HPEN)SelectObject(hDC, hPen);
		HBRUSH hNull = (HBRUSH)GetStockObject(NULL_BRUSH);
		HBRUSH hOldBr = (HBRUSH)SelectObject(hDC, hNull);
		Rectangle(hDC, rc.left, rc.top, rc.right, rc.bottom);
		SelectObject(hDC, hOldPen);
		SelectObject(hDC, hOldBr);
		DeleteObject(hPen);

		// Text
		SetBkMode(hDC, TRANSPARENT);
		SetTextColor(hDC, BcFilterColors::TEXT);
		HFONT hOldFont = nullptr;
		if (m_editFont.GetSafeHandle())
			hOldFont = (HFONT)SelectObject(hDC, m_editFont.GetSafeHandle());

		TCHAR szText[64] = {};
		::GetWindowText(lpDIS->hwndItem, szText, _countof(szText));
		DrawText(hDC, szText, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

		if (hOldFont)
			SelectObject(hDC, hOldFont);
		return;
	}
	CControlBar::OnDrawItem(nIDCtl, lpDIS);
}

/**
 * @brief Show the Filters dropdown popup menu.
 */
void CDirSideBySideFilterBar::OnFiltersDropdown()
{
	CMenu menu;
	menu.CreatePopupMenu();

	// Build checkable items
	bool allOn = GetOptionsMgr()->GetBool(OPT_SHOW_DIFFERENT) &&
		GetOptionsMgr()->GetBool(OPT_SHOW_IDENTICAL) &&
		GetOptionsMgr()->GetBool(OPT_SHOW_UNIQUE_LEFT) &&
		GetOptionsMgr()->GetBool(OPT_SHOW_UNIQUE_RIGHT) &&
		GetOptionsMgr()->GetBool(OPT_SHOW_SKIPPED);

	menu.AppendMenu(MF_STRING | (allOn ? MF_CHECKED : 0), ID_FILTER_POPUP_ALL, _T("Show All"));
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING | (GetOptionsMgr()->GetBool(OPT_SHOW_DIFFERENT) ? MF_CHECKED : 0),
		ID_FILTER_POPUP_DIFFERENT, _T("Show Different"));
	menu.AppendMenu(MF_STRING | (GetOptionsMgr()->GetBool(OPT_SHOW_IDENTICAL) ? MF_CHECKED : 0),
		ID_FILTER_POPUP_IDENTICAL, _T("Show Identical"));
	menu.AppendMenu(MF_STRING | (GetOptionsMgr()->GetBool(OPT_SHOW_UNIQUE_LEFT) ? MF_CHECKED : 0),
		ID_FILTER_POPUP_ORPHANS_L, _T("Show Orphans Left"));
	menu.AppendMenu(MF_STRING | (GetOptionsMgr()->GetBool(OPT_SHOW_UNIQUE_RIGHT) ? MF_CHECKED : 0),
		ID_FILTER_POPUP_ORPHANS_R, _T("Show Orphans Right"));
	menu.AppendMenu(MF_STRING | (GetOptionsMgr()->GetBool(OPT_SHOW_DIFFERENT_LEFT_ONLY) ? MF_CHECKED : 0),
		ID_FILTER_POPUP_NEWER_L, _T("Show Newer Left"));
	menu.AppendMenu(MF_STRING | (GetOptionsMgr()->GetBool(OPT_SHOW_DIFFERENT_RIGHT_ONLY) ? MF_CHECKED : 0),
		ID_FILTER_POPUP_NEWER_R, _T("Show Newer Right"));
	menu.AppendMenu(MF_STRING | (GetOptionsMgr()->GetBool(OPT_SHOW_SKIPPED) ? MF_CHECKED : 0),
		ID_FILTER_POPUP_SKIPPED, _T("Show Skipped"));
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING | (GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_SUPPRESS_FILTERS) ? MF_CHECKED : 0),
		ID_FILTER_POPUP_SUPPRESS, _T("Suppress Filters"));
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_FILTER_POPUP_ADVANCED, _T("Advanced Filters..."));

	// Show below the Filters button
	CRect rcBtn;
	m_btnFilters.GetWindowRect(&rcBtn);
	UINT cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
		rcBtn.left, rcBtn.bottom, this);

	// Handle the selected command
	switch (cmd)
	{
	case ID_FILTER_POPUP_ALL:        OnFilterAll(); break;
	case ID_FILTER_POPUP_DIFFERENT:  OnFilterDifferent(); break;
	case ID_FILTER_POPUP_IDENTICAL:  OnFilterIdentical(); break;
	case ID_FILTER_POPUP_ORPHANS_L:  OnFilterOrphansL(); break;
	case ID_FILTER_POPUP_ORPHANS_R:  OnFilterOrphansR(); break;
	case ID_FILTER_POPUP_NEWER_L:    OnFilterNewerL(); break;
	case ID_FILTER_POPUP_NEWER_R:    OnFilterNewerR(); break;
	case ID_FILTER_POPUP_SKIPPED:    OnFilterSkipped(); break;
	case ID_FILTER_POPUP_SUPPRESS:   OnSuppressFilters(); break;
	case ID_FILTER_POPUP_ADVANCED:   OnAdvancedFilter(); break;
	}
}

/**
 * @brief Handle WM_SIZE — stretch the filter edit to fill available width.
 */
void CDirSideBySideFilterBar::OnSize(UINT nType, int cx, int cy)
{
	CControlBar::OnSize(nType, cx, cy);

	if (!m_editFilter.GetSafeHwnd() || !m_btnFilters.GetSafeHwnd() || !m_btnPeek.GetSafeHwnd())
		return;

	CClientDC dc(this);
	const int dpi = dc.GetDeviceCaps(LOGPIXELSX);
	auto px = [dpi](int pt) { return MulDiv(pt, dpi, 72); };
	int editH = px(16);
	int barH = px(22);
	int y = (barH - editH) / 2;

	// Get label right edge
	CRect rcLabel;
	m_labelFilter.GetWindowRect(&rcLabel);
	ScreenToClient(&rcLabel);
	int xAfterLabel = rcLabel.right + px(4);

	// Place Peek button at right edge
	int peekW = px(40);
	int peekX = cx - peekW - px(4);
	m_btnPeek.MoveWindow(peekX, y, peekW, editH);

	// Place Filters button before Peek
	int btnW = px(52);
	int btnX = peekX - btnW - px(4);
	m_btnFilters.MoveWindow(btnX, y, btnW, editH);

	// Stretch edit to fill between label and Filters button
	int editW = btnX - xAfterLabel - px(4);
	if (editW < px(50))
		editW = px(50);
	m_editFilter.MoveWindow(xAfterLabel, y, editW, editH);
}

/**
 * @brief Toggle filter suppression (Peek mode).
 */
void CDirSideBySideFilterBar::OnPeek()
{
	bool bCurrent = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_SUPPRESS_FILTERS);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_SUPPRESS_FILTERS, !bCurrent);
	m_btnPeek.SetWindowText(!bCurrent ? _T("Peek*") : _T("Peek"));
	if (m_pCoordinator)
		m_pCoordinator->Redisplay();
}

void CDirSideBySideFilterBar::OnFilterAll()
{
	// Turn on all display filters
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT, true);
	GetOptionsMgr()->SaveOption(OPT_SHOW_IDENTICAL, true);
	GetOptionsMgr()->SaveOption(OPT_SHOW_UNIQUE_LEFT, true);
	GetOptionsMgr()->SaveOption(OPT_SHOW_UNIQUE_RIGHT, true);
	GetOptionsMgr()->SaveOption(OPT_SHOW_SKIPPED, true);
	if (m_pCoordinator)
		m_pCoordinator->Redisplay();
}

void CDirSideBySideFilterBar::OnFilterDifferent()
{
	ToggleOption(OPT_SHOW_DIFFERENT, m_pCoordinator);
}

void CDirSideBySideFilterBar::OnFilterIdentical()
{
	ToggleOption(OPT_SHOW_IDENTICAL, m_pCoordinator);
}

void CDirSideBySideFilterBar::OnFilterOrphansL()
{
	ToggleOption(OPT_SHOW_UNIQUE_LEFT, m_pCoordinator);
}

void CDirSideBySideFilterBar::OnFilterOrphansR()
{
	ToggleOption(OPT_SHOW_UNIQUE_RIGHT, m_pCoordinator);
}

void CDirSideBySideFilterBar::OnFilterNewerL()
{
	ToggleOption(OPT_SHOW_DIFFERENT_LEFT_ONLY, m_pCoordinator);
}

void CDirSideBySideFilterBar::OnFilterNewerR()
{
	ToggleOption(OPT_SHOW_DIFFERENT_RIGHT_ONLY, m_pCoordinator);
}

void CDirSideBySideFilterBar::OnFilterSkipped()
{
	ToggleOption(OPT_SHOW_SKIPPED, m_pCoordinator);
}

void CDirSideBySideFilterBar::OnSuppressFilters()
{
	ToggleOption(OPT_DIRVIEW_SXS_SUPPRESS_FILTERS, m_pCoordinator);
}

// Update UI handlers — set check state based on current option values
void CDirSideBySideFilterBar::OnUpdateFilterAll(CCmdUI* pCmdUI)
{
	bool allOn = GetOptionsMgr()->GetBool(OPT_SHOW_DIFFERENT) &&
		GetOptionsMgr()->GetBool(OPT_SHOW_IDENTICAL) &&
		GetOptionsMgr()->GetBool(OPT_SHOW_UNIQUE_LEFT) &&
		GetOptionsMgr()->GetBool(OPT_SHOW_UNIQUE_RIGHT) &&
		GetOptionsMgr()->GetBool(OPT_SHOW_SKIPPED);
	pCmdUI->SetCheck(allOn);
}

void CDirSideBySideFilterBar::OnUpdateFilterDifferent(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_SHOW_DIFFERENT));
}

void CDirSideBySideFilterBar::OnUpdateFilterIdentical(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_SHOW_IDENTICAL));
}

void CDirSideBySideFilterBar::OnUpdateFilterOrphansL(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_SHOW_UNIQUE_LEFT));
}

void CDirSideBySideFilterBar::OnUpdateFilterOrphansR(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_SHOW_UNIQUE_RIGHT));
}

void CDirSideBySideFilterBar::OnUpdateFilterNewerL(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_SHOW_DIFFERENT_LEFT_ONLY));
}

void CDirSideBySideFilterBar::OnUpdateFilterNewerR(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_SHOW_DIFFERENT_RIGHT_ONLY));
}

void CDirSideBySideFilterBar::OnUpdateFilterSkipped(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_SHOW_SKIPPED));
}

void CDirSideBySideFilterBar::OnUpdateSuppressFilters(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_SUPPRESS_FILTERS));
}

/**
 * @brief Handle name filter edit losing focus — apply the filter pattern.
 */
void CDirSideBySideFilterBar::OnNameFilterChanged()
{
	if (!m_editFilter.GetSafeHwnd() || !m_pCoordinator)
		return;

	CString text;
	m_editFilter.GetWindowText(text);
	m_pCoordinator->SetNameFilter(String(text));
}

/**
 * @brief Open the advanced filter dialog by forwarding to the left pane view.
 */
void CDirSideBySideFilterBar::OnAdvancedFilter()
{
	// Forward to pane view which shows the dialog
	if (m_pCoordinator)
	{
		CDirPaneView* pPane = m_pCoordinator->GetLeftPaneView();
		if (pPane)
			pPane->SendMessage(WM_COMMAND, ID_DIR_SXS_ADV_FILTER);
	}
}
