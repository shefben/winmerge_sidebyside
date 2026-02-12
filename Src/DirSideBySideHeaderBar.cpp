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
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Beyond Compare dark theme colors
namespace BcHdr
{
	static const COLORREF BG        = RGB(43, 43, 43);   // toolbar background
	static const COLORREF COMBO_BG  = RGB(30, 30, 30);   // combo edit background
	static const COLORREF TEXT      = RGB(200, 200, 200); // text
	static const COLORREF BTN_BG   = RGB(50, 50, 50);    // button face
	static const COLORREF BTN_HOT  = RGB(65, 65, 65);    // button hover
	static const COLORREF BTN_PRESS= RGB(35, 35, 35);    // button pressed
	static const COLORREF BTN_BORDER = RGB(65, 65, 65);  // button border
	static const COLORREF ICON     = RGB(170, 170, 170);  // icon lines
}

// Icon type constants for DrawIconButton
enum { ICON_BACK = 0, ICON_BROWSE = 1, ICON_UPLEVEL = 2 };

// Control IDs
#define IDC_SXS_COMBO_LEFT     9801
#define IDC_SXS_COMBO_RIGHT    9802
#define IDC_SXS_BACK_LEFT      9803
#define IDC_SXS_BACK_RIGHT     9804
#define IDC_SXS_BROWSE_LEFT    9805
#define IDC_SXS_BROWSE_RIGHT   9806
#define IDC_SXS_UPLEVEL_LEFT   9807
#define IDC_SXS_UPLEVEL_RIGHT  9808

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
	ON_CONTROL_RANGE(CBN_SELCHANGE, IDC_SXS_COMBO_LEFT, IDC_SXS_COMBO_RIGHT, OnComboSelChange)
	ON_BN_CLICKED(IDC_SXS_BACK_LEFT, OnBackLeft)
	ON_BN_CLICKED(IDC_SXS_BACK_RIGHT, OnBackRight)
	ON_BN_CLICKED(IDC_SXS_BROWSE_LEFT, OnBrowseLeft)
	ON_BN_CLICKED(IDC_SXS_BROWSE_RIGHT, OnBrowseRight)
	ON_BN_CLICKED(IDC_SXS_UPLEVEL_LEFT, OnUpLevelLeft)
	ON_BN_CLICKED(IDC_SXS_UPLEVEL_RIGHT, OnUpLevelRight)
END_MESSAGE_MAP()

CDirSideBySideHeaderBar::CDirSideBySideHeaderBar()
	: m_nPanes(2)
	, m_nActivePane(-1)
{
	m_pDropHandlers[0] = nullptr;
	m_pDropHandlers[1] = nullptr;
	m_brDarkBg.CreateSolidBrush(BcHdr::BG);
	m_brDarkEdit.CreateSolidBrush(BcHdr::COMBO_BG);
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

	// Background
	COLORREF bg = bPressed ? BcHdr::BTN_PRESS : BcHdr::BTN_BG;
	HBRUSH hBr = CreateSolidBrush(bg);
	FillRect(hDC, &rc, hBr);
	DeleteObject(hBr);

	// Border
	HPEN hPen = CreatePen(PS_SOLID, 1, BcHdr::BTN_BORDER);
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

	HPEN hIconPen = CreatePen(PS_SOLID, 2, BcHdr::ICON);
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

	case ICON_BROWSE:
		// Folder icon: simple folder outline
		{
			HPEN hFolderPen = CreatePen(PS_SOLID, 1, BcHdr::ICON);
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
	}

	SelectObject(hDC, GetStockObject(BLACK_PEN));
	DeleteObject(hIconPen);
}

BOOL CDirSideBySideHeaderBar::Create(CWnd* pParentWnd)
{
	if (!__super::Create(pParentWnd, CDirSideBySideHeaderBar::IDD,
		CBRS_ALIGN_TOP | CBRS_TOOLTIPS | CBRS_FLYBY, AFX_IDW_CONTROLBAR_FIRST + 28))
		return FALSE;

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
	UINT comboIDs[2] = { IDC_SXS_COMBO_LEFT, IDC_SXS_COMBO_RIGHT };
	UINT backIDs[2]  = { IDC_SXS_BACK_LEFT,  IDC_SXS_BACK_RIGHT };
	UINT browseIDs[2] = { IDC_SXS_BROWSE_LEFT, IDC_SXS_BROWSE_RIGHT };
	UINT upIDs[2]    = { IDC_SXS_UPLEVEL_LEFT, IDC_SXS_UPLEVEL_RIGHT };

	for (int pane = 0; pane < 2; pane++)
	{
		// Combo box (CBS_DROPDOWN gives edit + dropdown button)
		m_comboPath[pane].Create(
			WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL,
			CRect(0, 0, 200, BAR_HEIGHT + 200), this, comboIDs[pane]);
		m_comboPath[pane].SetFont(&m_font);
		SetWindowTheme(m_comboPath[pane].m_hWnd, L"", L"");

		// Back button (owner-drawn)
		m_btnBack[pane].Create(_T(""),
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
			CRect(0, 0, BTN_W, BAR_HEIGHT), this, backIDs[pane]);
		SetWindowTheme(m_btnBack[pane].m_hWnd, L"", L"");

		// Browse button (owner-drawn)
		m_btnBrowse[pane].Create(_T(""),
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
			CRect(0, 0, BTN_W, BAR_HEIGHT), this, browseIDs[pane]);
		SetWindowTheme(m_btnBrowse[pane].m_hWnd, L"", L"");

		// Up-level button (owner-drawn)
		m_btnUpLevel[pane].Create(_T(""),
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
			CRect(0, 0, BTN_W, BAR_HEIGHT), this, upIDs[pane]);
		SetWindowTheme(m_btnUpLevel[pane].m_hWnd, L"", L"");
	}

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
	pDC->FillSolidRect(&rc, BcHdr::BG);
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
				pDC->SetBkColor(BcHdr::COMBO_BG);
				pDC->SetTextColor(BcHdr::TEXT);
				return (HBRUSH)m_brDarkEdit.GetSafeHandle();
			}
		}
	}
	// Dark theme for the dropdown list
	if (nCtlColor == CTLCOLOR_LISTBOX)
	{
		pDC->SetBkColor(BcHdr::COMBO_BG);
		pDC->SetTextColor(BcHdr::TEXT);
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
	case IDC_SXS_BROWSE_LEFT:
	case IDC_SXS_BROWSE_RIGHT:
		DrawIconButton(lpDIS, ICON_BROWSE);
		return;
	case IDC_SXS_UPLEVEL_LEFT:
	case IDC_SXS_UPLEVEL_RIGHT:
		DrawIconButton(lpDIS, ICON_UPLEVEL);
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
 * Per-pane layout: [ComboBox][Back btn][Browse btn][Up btn]
 * The combo takes all available width minus 3*BTN_W for buttons.
 */
void CDirSideBySideHeaderBar::Resize(int widths[], int offsets[])
{
	if (m_hWnd == nullptr)
		return;

	const int comboH = BAR_HEIGHT;
	const int btnCount = 3;
	const int buttonsW = btnCount * BTN_W;

	for (int pane = 0; pane < m_nPanes; pane++)
	{
		int x = offsets[pane];
		int w = widths[pane];
		int comboW = w - buttonsW - 1;
		if (comboW < 80) comboW = 80;

		if (m_comboPath[pane].GetSafeHwnd())
			m_comboPath[pane].SetWindowPos(nullptr, x, PAD_Y, comboW, comboH + 200,
				SWP_NOZORDER | SWP_NOACTIVATE);

		int bx = x + comboW + 1;
		if (m_btnBack[pane].GetSafeHwnd())
			m_btnBack[pane].SetWindowPos(nullptr, bx, PAD_Y, BTN_W, comboH,
				SWP_NOZORDER | SWP_NOACTIVATE);
		bx += BTN_W;
		if (m_btnBrowse[pane].GetSafeHwnd())
			m_btnBrowse[pane].SetWindowPos(nullptr, bx, PAD_Y, BTN_W, comboH,
				SWP_NOZORDER | SWP_NOACTIVATE);
		bx += BTN_W;
		if (m_btnUpLevel[pane].GetSafeHwnd())
			m_btnUpLevel[pane].SetWindowPos(nullptr, bx, PAD_Y, BTN_W, comboH,
				SWP_NOZORDER | SWP_NOACTIVATE);
	}

	InvalidateRect(nullptr, FALSE);
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

// --- Button handlers ---

void CDirSideBySideHeaderBar::OnBackLeft()   { if (m_backCallbackfunc)    m_backCallbackfunc(0); }
void CDirSideBySideHeaderBar::OnBackRight()  { if (m_backCallbackfunc)    m_backCallbackfunc(1); }
void CDirSideBySideHeaderBar::OnBrowseLeft() { if (m_browseCallbackfunc)  m_browseCallbackfunc(0); }
void CDirSideBySideHeaderBar::OnBrowseRight(){ if (m_browseCallbackfunc)  m_browseCallbackfunc(1); }
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

	// Update dropdown list
	if (m_comboPath[pane].GetSafeHwnd())
	{
		m_comboPath[pane].ResetContent();
		for (const auto& path : history)
			m_comboPath[pane].AddString(path.c_str());
	}
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
