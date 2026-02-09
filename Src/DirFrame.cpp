/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997-2000  Thingamahoochie Software
//    Author: Dean Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  DirFrame.cpp
 *
 * @brief Implementation file for CDirFrame
 *
 */

#include "stdafx.h"
#include "DirFrame.h"
#include "DirSideBySideHeaderBar.h"
#include "DirSideBySideFilterBar.h"
#include "DirPaneView.h"
#include "DirGutterView.h"
#include "DirSideBySideCoordinator.h"
#include "DirDoc.h"
#include "DiffContext.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "OptionsDirColors.h"
#include "resource.h"
#include "MainFrm.h"
#include "paths.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/**
 * @brief Statusbar pane indexes
 */
enum
{
	PANE_FILTER = 1,
	PANE_COMPMETHOD,
	PANE_LEFT_RO,
	PANE_MIDDLE_RO,
	PANE_RIGHT_RO,
};

/**
 * @brief Width of compare method name pane in statusbar
 */
const int COMPMETHOD_PANEL_WIDTH = 100;
/**
 * @brief Width of center gutter column in SxS mode
 */
const int GUTTER_COL_WIDTH = 24;
/**
 * @brief Width of filter name pane in statusbar
 */
const int FILTER_PANEL_WIDTH = 200;

/**
 * @brief Bottom statusbar panels and indicators
 */
static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_SEPARATOR,
	ID_SEPARATOR,
	ID_SEPARATOR,
	ID_SEPARATOR,
	ID_SEPARATOR,
};

/**
 * @brief RO status panel width
 */
static UINT RO_PANEL_WIDTH = 30;

/////////////////////////////////////////////////////////////////////////////
// CDirFrame

IMPLEMENT_DYNCREATE(CDirFrame, CMergeFrameCommon)

CDirFrame::CDirFrame()
: CMergeFrameCommon(IDI_EQUALFOLDER, IDI_NOTEQUALFOLDER)
, m_bSideBySideMode(false)
, m_pLeftPaneView(nullptr)
, m_pRightPaneView(nullptr)
, m_pGutterView(nullptr)
{
}

CDirFrame::~CDirFrame()
{
}

BEGIN_MESSAGE_MAP(CDirFrame, CMergeFrameCommon)
	//{{AFX_MSG_MAP(CDirFrame)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_SIZE()
	ON_MESSAGE_VOID(WM_IDLEUPDATECMDUI, OnIdleUpdateCmdUI)
	ON_COMMAND(ID_VIEW_DISPLAY_FILTER_BAR_MENU, OnViewDisplayFilterBar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_DISPLAY_FILTER_BAR_MENU, OnUpdateDisplayViewFilterBar)
	ON_COMMAND(IDCANCEL, OnDisplayFilterBarClose)
	ON_COMMAND(IDC_FILTERFILE_MASK_MENU, OnDisplayFilterBarMaskMenu)
	ON_COMMAND(ID_VIEW_DIR_SIDEBYSIDE, OnViewSideBySide)
	ON_UPDATE_COMMAND_UI(ID_VIEW_DIR_SIDEBYSIDE, OnUpdateViewSideBySide)
	ON_COMMAND(ID_DIR_SXS_SWAP_SIDES, OnSxsSwapSides)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_SWAP_SIDES, OnUpdateSxsCommand)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_CROSS_COMPARE, OnUpdateSxsCommand)
	ON_COMMAND(ID_DIR_SXS_LEGEND, OnSxsLegend)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_LEGEND, OnUpdateSxsLegend)
	ON_WM_ACTIVATEAPP()
	ON_COMMAND(ID_DIR_SXS_SESSION_SAVE, OnSxsSessionSave)
	ON_COMMAND(ID_DIR_SXS_SESSION_LOAD, OnSxsSessionLoad)
	ON_COMMAND(ID_DIR_SXS_WORKSPACE_SAVE, OnSxsWorkspaceSave)
	ON_COMMAND(ID_DIR_SXS_WORKSPACE_LOAD, OnSxsWorkspaceLoad)
	ON_COMMAND(ID_DIR_SXS_NAV_BACK, OnSxsNavBack)
	ON_COMMAND(ID_DIR_SXS_NAV_FORWARD, OnSxsNavForward)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_NAV_BACK, OnUpdateSxsNavBack)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_NAV_FORWARD, OnUpdateSxsNavForward)
	ON_COMMAND(ID_DIR_SXS_UP_LEVEL, OnSxsUpLevel)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDirFrame message handlers

/**
 * @brief Create statusbar
 */
int CDirFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (__super::OnCreate(lpCreateStruct) == -1)
		return -1;

	EnableDocking(CBRS_ALIGN_TOP);

	// Dir frame has a header bar at top
	if (!m_wndFilePathBar.Create(this))
	{
		TRACE0("Failed to create header bar\n");
		return -1;      // fail to create
	}

	// Create the side-by-side header bar (initially hidden; shown when SxS mode is active)
	if (!m_wndSxSHeaderBar.Create(this))
	{
		TRACE0("Failed to create SxS header bar\n");
		return -1;      // fail to create
	}

	// Create the side-by-side filter bar (initially hidden; shown when SxS mode is active)
	if (!m_wndSxSFilterBar.Create(this))
	{
		TRACE0("Failed to create SxS filter bar\n");
		return -1;      // fail to create
	}
	ShowControlBar(&m_wndSxSFilterBar, FALSE, FALSE);

	// Directory frame has a status bar
	if (!m_wndStatusBar.Create(this, WS_CHILD | WS_VISIBLE | CBRS_BOTTOM, AFX_IDW_CONTROLBAR_FIRST+30) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}	
	
	String sText = _("RO");
	const int lpx = CClientDC(this).GetDeviceCaps(LOGPIXELSX);
	auto pointToPixel = [lpx](int point) { return MulDiv(point, lpx, 72); };
	m_wndStatusBar.SetPaneInfo(0, 0, SBPS_STRETCH | SBPS_NOBORDERS, 0);
	m_wndStatusBar.SetPaneInfo(PANE_FILTER, ID_STATUS_FILTER, SBPS_CLICKABLE, pointToPixel(FILTER_PANEL_WIDTH));
	m_wndStatusBar.SetPaneInfo(PANE_COMPMETHOD, ID_STATUS_FILTER, SBPS_CLICKABLE, pointToPixel(COMPMETHOD_PANEL_WIDTH));
	m_wndStatusBar.SetPaneInfo(PANE_LEFT_RO, ID_STATUS_LEFTDIR_RO, SBPS_CLICKABLE, pointToPixel(RO_PANEL_WIDTH));
	m_wndStatusBar.SetPaneInfo(PANE_MIDDLE_RO, ID_STATUS_MIDDLEDIR_RO, SBPS_CLICKABLE, pointToPixel(RO_PANEL_WIDTH));
	m_wndStatusBar.SetPaneInfo(PANE_RIGHT_RO, ID_STATUS_RIGHTDIR_RO, SBPS_CLICKABLE, pointToPixel(RO_PANEL_WIDTH));
	m_wndStatusBar.SetPaneText(PANE_LEFT_RO, sText.c_str(), TRUE); 
	m_wndStatusBar.SetPaneText(PANE_MIDDLE_RO, sText.c_str(), TRUE); 
	m_wndStatusBar.SetPaneText(PANE_RIGHT_RO, sText.c_str(), TRUE);

	// load docking positions and sizes
	CDockState dockState;
	dockState.LoadState(_T("Settings-DirFrame"));
	SetDockState(dockState);

	return 0;
}

/**
 * @brief Set statusbar text
 */
void CDirFrame::SetStatus(const tchar_t* szStatus)
{
	m_wndStatusBar.SetPaneText(0, szStatus);
}

/**
 * @brief Set current compare method name to statusbar
 * @param [in] nCompMethod compare method to show
 */
void CDirFrame::SetCompareMethodStatusDisplay(int nCompMethod)
{
	m_wndStatusBar.SetPaneText(PANE_COMPMETHOD, I18n::LoadString(IDS_COMPMETHOD_FULL_CONTENTS + nCompMethod).c_str());
}

/**
 * @brief Set active filter name to statusbar
 * @param [in] szFilter Filtername to show
 */
void CDirFrame::SetFilterStatusDisplay(const tchar_t* szFilter)
{
	m_wndStatusBar.SetPaneText(PANE_FILTER, szFilter);
}

/**
 * @brief Restore maximized state of directory compare window
 */
void CDirFrame::ActivateFrame(int nCmdShow) 
{
	__super::ActivateFrame(nCmdShow);
}

/**
 * @brief Update any resources necessary after a GUI language change
 */
void CDirFrame::UpdateResources()
{
}

void CDirFrame::OnClose() 
{	
	__super::OnClose();
}

/**
 * @brief Save maximized state before destroying window
 */
BOOL CDirFrame::DestroyWindow()
{
	HideProgressBar();
	HideFilterBar();
	// save docking positions and sizes
	CDockState dockState;
	GetDockState(dockState);
	dockState.SaveState(_T("Settings-DirFrame"));
	SaveWindowState();

	// Save SxS state
	if (m_bSideBySideMode)
	{
		if (m_pLeftPaneView)
			m_pLeftPaneView->SaveColumnState();
		if (m_pRightPaneView)
			m_pRightPaneView->SaveColumnState();

		// Save splitter position
		if (::IsWindow(m_wndSplitter.m_hWnd))
		{
			int wLeft, wMin;
			m_wndSplitter.GetColumnInfo(0, wLeft, wMin);
			GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_SPLITTER_POS, wLeft);
		}
	}

	return __super::DestroyWindow();
}

void CDirFrame::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	if (m_bSideBySideMode)
		UpdateHeaderSizes();
	else
		m_wndFilePathBar.Resize();
}

void CDirFrame::ShowProgressBar()
{
	if (m_pCmpProgressBar == nullptr)
		m_pCmpProgressBar.reset(new DirCompProgressBar());

	if (!::IsWindow(m_pCmpProgressBar->GetSafeHwnd()))
		m_pCmpProgressBar->Create(this);

	ShowControlBar(m_pCmpProgressBar.get(), TRUE, FALSE);
}

void CDirFrame::HideProgressBar()
{
	if (m_pCmpProgressBar != nullptr && ::IsWindow(m_pCmpProgressBar->GetSafeHwnd()))
	{
		ShowControlBar(m_pCmpProgressBar.get(), FALSE, FALSE);
		m_pCmpProgressBar->DestroyWindow();
	}
	m_pCmpProgressBar.reset();
}

void CDirFrame::OnViewDisplayFilterBar()
{
	if (!m_pDirFilterBar)
		ShowFilterBar();
	else
		HideFilterBar();
}

void CDirFrame::OnUpdateDisplayViewFilterBar(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(TRUE);
	pCmdUI->SetCheck(m_pDirFilterBar != nullptr);
}

void CDirFrame::OnDisplayFilterBarClose()
{
	HideFilterBar();
	GetActiveView()->SetFocus();
}

void CDirFrame::OnDisplayFilterBarMaskMenu()
{
	m_pDirFilterBar->ShowFilterMaskMenu();
}

void CDirFrame::ShowFilterBar()
{
	if (!m_pDirFilterBar)
		m_pDirFilterBar.reset(new CDirFilterBar());
	if (!::IsWindow(m_pDirFilterBar->GetSafeHwnd()) && !m_pDirFilterBar->Create(this))
	{
		TRACE0("Failed to create filter bar\n");
		m_pDirFilterBar.reset();
		return;
	}
	ShowControlBar(m_pDirFilterBar.get(), TRUE, FALSE);
}

void CDirFrame::HideFilterBar()
{
	if (m_pDirFilterBar != nullptr && ::IsWindow(m_pDirFilterBar->GetSafeHwnd()))
	{
		ShowControlBar(m_pDirFilterBar.get(), FALSE, FALSE);
		m_pDirFilterBar->DestroyWindow();
	}
	m_pDirFilterBar.reset();
}

/**
 * @brief Create the client area — in SxS mode, create a 1x2 splitter with two CDirPaneView instances.
 */
BOOL CDirFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
	m_bSideBySideMode = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SIDEBYSIDE_MODE);

	if (!m_bSideBySideMode)
	{
		// Hide the SxS header bar, show the standard one
		ShowControlBar(&m_wndSxSHeaderBar, FALSE, FALSE);
		return __super::OnCreateClient(lpcs, pContext);
	}

	// SxS mode: hide the standard header bar, show the SxS one and filter bar
	ShowControlBar(&m_wndFilePathBar, FALSE, FALSE);
	ShowControlBar(&m_wndSxSHeaderBar, TRUE, FALSE);
	if (GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_SHOW_FILTER_BAR))
		ShowControlBar(&m_wndSxSFilterBar, TRUE, FALSE);

	bool bShowGutter = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_SHOW_GUTTER);
	int nCols = bShowGutter ? 3 : 2;

	// Create a 1-row, N-column splitter (2 panes + optional gutter)
	m_wndSplitter.HideBorders(true);
	if (!m_wndSplitter.CreateStatic(this, 1, nCols))
	{
		TRACE0("Failed to create SxS splitter\n");
		return FALSE;
	}

	// Create left pane (column 0)
	if (!m_wndSplitter.CreateView(0, 0, RUNTIME_CLASS(CDirPaneView),
		CSize(100, 100), pContext))
	{
		TRACE0("Failed to create left pane view\n");
		return FALSE;
	}

	if (bShowGutter)
	{
		// Create center gutter (column 1) — narrow, ~24px
		if (!m_wndSplitter.CreateView(0, 1, RUNTIME_CLASS(CDirGutterView),
			CSize(GUTTER_COL_WIDTH, 100), pContext))
		{
			TRACE0("Failed to create gutter view\n");
			return FALSE;
		}

		// Create right pane (column 2)
		if (!m_wndSplitter.CreateView(0, 2, RUNTIME_CLASS(CDirPaneView),
			CSize(100, 100), pContext))
		{
			TRACE0("Failed to create right pane view\n");
			return FALSE;
		}

		m_pLeftPaneView = static_cast<CDirPaneView*>(m_wndSplitter.GetPane(0, 0));
		m_pGutterView = static_cast<CDirGutterView*>(m_wndSplitter.GetPane(0, 1));
		m_pRightPaneView = static_cast<CDirPaneView*>(m_wndSplitter.GetPane(0, 2));

		// Lock the gutter column width
		m_wndSplitter.SetColumnInfo(1, GUTTER_COL_WIDTH, GUTTER_COL_WIDTH);
	}
	else
	{
		// Create right pane (column 1) — no gutter
		if (!m_wndSplitter.CreateView(0, 1, RUNTIME_CLASS(CDirPaneView),
			CSize(100, 100), pContext))
		{
			TRACE0("Failed to create right pane view\n");
			return FALSE;
		}

		m_pLeftPaneView = static_cast<CDirPaneView*>(m_wndSplitter.GetPane(0, 0));
		m_pRightPaneView = static_cast<CDirPaneView*>(m_wndSplitter.GetPane(0, 1));
	}

	m_pLeftPaneView->SetPaneIndex(0);
	m_pRightPaneView->SetPaneIndex(1);

	// Create the coordinator
	CDirDoc *pDoc = dynamic_cast<CDirDoc*>(pContext->m_pCurrentDoc);
	m_pCoordinator.reset(new CDirSideBySideCoordinator(pDoc));
	m_pCoordinator->SetPaneViews(m_pLeftPaneView, m_pRightPaneView);

	m_pLeftPaneView->SetCoordinator(m_pCoordinator.get());
	m_pRightPaneView->SetCoordinator(m_pCoordinator.get());

	if (m_pGutterView)
		m_pGutterView->SetCoordinator(m_pCoordinator.get());

	// Connect coordinator to the document and filter bar
	pDoc->SetSideBySideMode(true);
	pDoc->SetCoordinator(m_pCoordinator.get());
	m_wndSxSFilterBar.SetCoordinator(m_pCoordinator.get());

	// Wire folder selection callback — when user browses to a new folder via header bar
	// or drops a folder onto a path edit, open a new comparison with the updated path
	m_wndSxSHeaderBar.SetOnFolderSelectedCallback(
		[this, pDoc](int pane, const String& sFolderpath)
		{
			if (!pDoc->HasDiffs())
				return;
			const CDiffContext &ctxt = pDoc->GetDiffContext();
			PathContext paths = ctxt.GetNormalizedPaths();
			if (pane >= 0 && pane < paths.GetSize())
				paths.SetPath(pane, sFolderpath);
			fileopenflags_t dwFlags[3] = {};
			GetMainFrame()->DoFileOrFolderOpen(&paths, dwFlags, nullptr, _T(""),
				ctxt.m_bRecursive, nullptr);
		});

	// Equalize the pane columns (not the gutter)
	m_wndSplitter.EqualizeCols();

	// Restore splitter position if saved
	int nSplitterPos = GetOptionsMgr()->GetInt(OPT_DIRVIEW_SXS_SPLITTER_POS);
	if (nSplitterPos > 0)
	{
		CRect rc;
		GetClientRect(&rc);
		if (nSplitterPos < rc.Width() - 50)
		{
			m_wndSplitter.SetColumnInfo(0, nSplitterPos, 50);
			if (bShowGutter)
			{
				int rightWidth = rc.Width() - nSplitterPos - GUTTER_COL_WIDTH;
				if (rightWidth > 50)
					m_wndSplitter.SetColumnInfo(2, rightWidth, 50);
			}
			m_wndSplitter.RecalcLayout();
		}
	}

	return TRUE;
}

/**
 * @brief Get the interface to the header (path) bar.
 * Returns the SxS header bar in side-by-side mode, otherwise the standard one.
 */
IHeaderBar * CDirFrame::GetHeaderInterface()
{
	if (m_bSideBySideMode)
		return &m_wndSxSHeaderBar;
	return &m_wndFilePathBar;
}

/**
 * @brief Sync header bar widths with the splitter column widths.
 * In 3-column mode (with gutter), the header gets left pane width and
 * right pane width, skipping the gutter column.
 */
void CDirFrame::UpdateHeaderSizes()
{
	if (!m_bSideBySideMode)
		return;
	if (!::IsWindow(m_wndSxSHeaderBar.m_hWnd) || !::IsWindow(m_wndSplitter.m_hWnd))
		return;

	int nCols = m_wndSplitter.GetColumnCount();
	int w[2] = { 1, 1 };

	if (nCols == 3)
	{
		// 3-column: left pane (col 0), gutter (col 1), right pane (col 2)
		int wmin;
		m_wndSplitter.GetColumnInfo(0, w[0], wmin);
		m_wndSplitter.GetColumnInfo(2, w[1], wmin);
		// Add half the gutter width to each side for header alignment
		int wGutter;
		m_wndSplitter.GetColumnInfo(1, wGutter, wmin);
		w[0] += wGutter / 2;
		w[1] += (wGutter + 1) / 2;
	}
	else
	{
		for (int pane = 0; pane < nCols && pane < 2; pane++)
		{
			int wmin;
			m_wndSplitter.GetColumnInfo(pane, w[pane], wmin);
		}
	}

	if (w[0] < 1) w[0] = 1;
	if (w[1] < 1) w[1] = 1;

	m_wndSxSHeaderBar.Resize(w);

	// Update the gutter display
	if (m_pGutterView && m_pGutterView->GetSafeHwnd())
		m_pGutterView->UpdateDisplay();
}

void CDirFrame::OnIdleUpdateCmdUI()
{
	if (m_bSideBySideMode)
		UpdateHeaderSizes();
}

void CDirFrame::OnViewSideBySide()
{
	// Toggle the option — requires reopening the folder comparison to take effect
	bool bCurrent = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SIDEBYSIDE_MODE);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SIDEBYSIDE_MODE, !bCurrent);
}

void CDirFrame::OnUpdateViewSideBySide(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_DIRVIEW_SIDEBYSIDE_MODE));
}

void CDirFrame::OnSxsSwapSides()
{
	if (m_pCoordinator)
		m_pCoordinator->SwapSides();
}

void CDirFrame::OnUpdateSxsCommand(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_bSideBySideMode);
}

/**
 * @brief Handle app activation — auto-refresh when regaining focus.
 */
void CDirFrame::OnActivateApp(BOOL bActive, DWORD dwThreadID)
{
	__super::OnActivateApp(bActive, dwThreadID);

	if (bActive && m_bSideBySideMode &&
		GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_AUTO_REFRESH))
	{
		// Post refresh command to avoid issues during activation
		if (m_pLeftPaneView && m_pLeftPaneView->GetSafeHwnd())
			m_pLeftPaneView->PostMessage(WM_COMMAND, ID_DIR_SXS_REFRESH);
	}
}

/**
 * @brief Dialog proc for the Color Legend dialog.
 */
static INT_PTR CALLBACK LegendDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		SetWindowLongPtr(hDlg, DWLP_USER, lParam);
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, IDOK);
			return TRUE;
		}
		break;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hDlg, &ps);

			// Load actual color settings from options
			DIRCOLORSETTINGS colors = {};
			Options::DirColors::Load(GetOptionsMgr(), colors);

			// Draw color swatches with labels using actual configured colors
			struct { COLORREF clrBg; COLORREF clrText; const wchar_t* label; } items[] = {
				{ colors.clrDirItemNewer,      colors.clrDirItemNewerText,      L"Newer (this side is newer)" },
				{ colors.clrDirItemOlder,      colors.clrDirItemOlderText,      L"Older (this side is older)" },
				{ colors.clrDirItemDiff,       colors.clrDirItemDiffText,       L"Different (same timestamp)" },
				{ colors.clrDirItemOrphan,     colors.clrDirItemOrphanText,     L"Orphan (unique to one side)" },
				{ colors.clrDirItemEqual,      colors.clrDirItemEqualText,      L"Identical" },
				{ colors.clrDirItemSuppressed, colors.clrDirItemSuppressedText, L"Suppressed filter item" },
				{ colors.clrDirItemFiltered,   colors.clrDirItemFilteredText,   L"Filtered / Skipped" },
			};

			int y = 10;
			const int swatchW = 24, swatchH = 18, textX = 40, lineH = 26;

			HFONT hFont = (HFONT)SendMessage(hDlg, WM_GETFONT, 0, 0);
			if (!hFont)
				hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
			HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

			for (const auto& item : items)
			{
				// Draw swatch with both background and text color sample
				RECT rcSwatch = { 10, y, 10 + swatchW, y + swatchH };
				HBRUSH hBrush = CreateSolidBrush(item.clrBg);
				FillRect(hdc, &rcSwatch, hBrush);
				DeleteObject(hBrush);
				FrameRect(hdc, &rcSwatch, (HBRUSH)GetStockObject(GRAY_BRUSH));

				// Draw "Ab" inside the swatch with the text color
				SetBkMode(hdc, TRANSPARENT);
				SetTextColor(hdc, item.clrText);
				DrawTextW(hdc, L"Ab", -1, &rcSwatch, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

				// Draw label
				SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
				RECT rcText = { textX, y, 350, y + swatchH };
				DrawTextW(hdc, item.label, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

				y += lineH;
			}

			SelectObject(hdc, hOldFont);
			EndPaint(hDlg, &ps);
		}
		return TRUE;
	}
	return FALSE;
}

/**
 * @brief Build an in-memory dialog template for the Legend dialog.
 */
static DLGTEMPLATE* BuildLegendDlgTemplate(BYTE* buffer, size_t bufSize)
{
	memset(buffer, 0, bufSize);
	const int DLG_W = 240, DLG_H = 220;

	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)buffer;
	pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
	pDlg->cdit = 1; // Just the OK button
	pDlg->x = 0; pDlg->y = 0;
	pDlg->cx = DLG_W; pDlg->cy = DLG_H;

	WORD* pw = (WORD*)(pDlg + 1);
	*pw++ = 0; // menu
	*pw++ = 0; // class
	const wchar_t dlgTitle[] = L"Color Legend";
	memcpy(pw, dlgTitle, sizeof(dlgTitle));
	pw += _countof(dlgTitle);
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	// OK button
	DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
	pItem->x = DLG_W / 2 - 30; pItem->y = DLG_H - 20; pItem->cx = 60; pItem->cy = 14;
	pItem->id = IDOK;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0080;
	const wchar_t ok[] = L"OK";
	memcpy(pw, ok, sizeof(ok));
	pw += _countof(ok);
	*pw++ = 0;

	return pDlg;
}

void CDirFrame::OnSxsLegend()
{
	BYTE dlgBuf[512];
	DLGTEMPLATE* pDlgTmpl = BuildLegendDlgTemplate(dlgBuf, sizeof(dlgBuf));
	DialogBoxIndirectParam(AfxGetInstanceHandle(), pDlgTmpl,
		m_hWnd, LegendDlgProc, 0);
}

void CDirFrame::OnUpdateSxsLegend(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_bSideBySideMode);
}

/**
 * @brief Save the current SxS comparison session to an INI-style file.
 * Stores left/right paths, recursive flag, filter, and SxS mode settings.
 */
void CDirFrame::OnSxsSessionSave()
{
	CDirDoc *pDoc = dynamic_cast<CDirDoc*>(GetActiveDocument());
	if (!pDoc || !pDoc->HasDiffs())
	{
		AfxMessageBox(_T("No active comparison to save."), MB_ICONINFORMATION);
		return;
	}

	CFileDialog dlg(FALSE, _T("wmses"), _T("session.wmses"),
		OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
		_T("WinMerge SxS Session (*.wmses)|*.wmses|All Files (*.*)|*.*||"));
	if (dlg.DoModal() != IDOK)
		return;

	String sPath = dlg.GetPathName().GetString();

	const CDiffContext &ctxt = pDoc->GetDiffContext();
	PathContext paths = ctxt.GetNormalizedPaths();

	WritePrivateProfileString(_T("Session"), _T("LeftPath"),
		paths.GetLeft().c_str(), sPath.c_str());
	WritePrivateProfileString(_T("Session"), _T("RightPath"),
		paths.GetRight().c_str(), sPath.c_str());
	WritePrivateProfileString(_T("Session"), _T("Recursive"),
		ctxt.m_bRecursive ? _T("1") : _T("0"), sPath.c_str());

	String filter = GetOptionsMgr()->GetString(OPT_FILEFILTER_CURRENT);
	WritePrivateProfileString(_T("Session"), _T("Filter"),
		filter.c_str(), sPath.c_str());

	WritePrivateProfileString(_T("Session"), _T("SideBySideMode"),
		m_bSideBySideMode ? _T("1") : _T("0"), sPath.c_str());
	WritePrivateProfileString(_T("Session"), _T("TreeMode"),
		GetOptionsMgr()->GetBool(OPT_TREE_MODE) ? _T("1") : _T("0"), sPath.c_str());
	WritePrivateProfileString(_T("Session"), _T("FlattenMode"),
		GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_FLATTEN_MODE) ? _T("1") : _T("0"), sPath.c_str());

	if (m_pCoordinator)
		m_pCoordinator->LogOperation(_T("Session saved to: ") + sPath);
}

/**
 * @brief Load a saved SxS comparison session from an INI-style file.
 * Reads paths and settings, then opens a new comparison.
 */
void CDirFrame::OnSxsSessionLoad()
{
	CFileDialog dlg(TRUE, _T("wmses"), nullptr,
		OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		_T("WinMerge SxS Session (*.wmses)|*.wmses|All Files (*.*)|*.*||"));
	if (dlg.DoModal() != IDOK)
		return;

	String sPath = dlg.GetPathName().GetString();

	tchar_t szLeft[MAX_PATH] = {};
	tchar_t szRight[MAX_PATH] = {};
	tchar_t szRecurse[8] = {};
	tchar_t szFilter[MAX_PATH] = {};
	tchar_t szSxS[8] = {};
	tchar_t szTree[8] = {};
	tchar_t szFlatten[8] = {};

	GetPrivateProfileString(_T("Session"), _T("LeftPath"), _T(""), szLeft, MAX_PATH, sPath.c_str());
	GetPrivateProfileString(_T("Session"), _T("RightPath"), _T(""), szRight, MAX_PATH, sPath.c_str());
	GetPrivateProfileString(_T("Session"), _T("Recursive"), _T("0"), szRecurse, 8, sPath.c_str());
	GetPrivateProfileString(_T("Session"), _T("Filter"), _T("*.*"), szFilter, MAX_PATH, sPath.c_str());
	GetPrivateProfileString(_T("Session"), _T("SideBySideMode"), _T("1"), szSxS, 8, sPath.c_str());
	GetPrivateProfileString(_T("Session"), _T("TreeMode"), _T("0"), szTree, 8, sPath.c_str());
	GetPrivateProfileString(_T("Session"), _T("FlattenMode"), _T("0"), szFlatten, 8, sPath.c_str());

	if (szLeft[0] == _T('\0') || szRight[0] == _T('\0'))
	{
		AfxMessageBox(_T("Invalid session file: missing paths."), MB_ICONERROR);
		return;
	}

	// Apply settings
	bool bSxS = (_ttoi(szSxS) != 0);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SIDEBYSIDE_MODE, bSxS);
	GetOptionsMgr()->SaveOption(OPT_TREE_MODE, _ttoi(szTree) != 0);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_FLATTEN_MODE, _ttoi(szFlatten) != 0);
	if (szFilter[0] != _T('\0'))
		GetOptionsMgr()->SaveOption(OPT_FILEFILTER_CURRENT, String(szFilter));

	bool bRecurse = (_ttoi(szRecurse) != 0);

	PathContext pathCtx;
	pathCtx.SetLeft(szLeft);
	pathCtx.SetRight(szRight);
	fileopenflags_t dwFlags[3] = {};
	GetMainFrame()->DoFileOrFolderOpen(&pathCtx, dwFlags, nullptr, _T(""),
		bRecurse, nullptr);
}

/**
 * @brief Save the full workspace state (window position, splitter, column widths, paths, filter, tree mode).
 */
void CDirFrame::OnSxsWorkspaceSave()
{
	CDirDoc *pDoc = dynamic_cast<CDirDoc*>(GetActiveDocument());
	if (!pDoc || !pDoc->HasDiffs())
	{
		AfxMessageBox(_T("No active comparison to save."), MB_ICONINFORMATION);
		return;
	}

	CFileDialog dlg(FALSE, _T("wmwks"), _T("workspace.wmwks"),
		OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
		_T("WinMerge SxS Workspace (*.wmwks)|*.wmwks|All Files (*.*)|*.*||"));
	if (dlg.DoModal() != IDOK)
		return;

	String sPath = dlg.GetPathName().GetString();

	const CDiffContext &ctxt = pDoc->GetDiffContext();
	PathContext paths = ctxt.GetNormalizedPaths();

	// Session data
	WritePrivateProfileString(_T("Session"), _T("LeftPath"),
		paths.GetLeft().c_str(), sPath.c_str());
	WritePrivateProfileString(_T("Session"), _T("RightPath"),
		paths.GetRight().c_str(), sPath.c_str());
	WritePrivateProfileString(_T("Session"), _T("Recursive"),
		ctxt.m_bRecursive ? _T("1") : _T("0"), sPath.c_str());

	String filter = GetOptionsMgr()->GetString(OPT_FILEFILTER_CURRENT);
	WritePrivateProfileString(_T("Session"), _T("Filter"),
		filter.c_str(), sPath.c_str());
	WritePrivateProfileString(_T("Session"), _T("SideBySideMode"),
		m_bSideBySideMode ? _T("1") : _T("0"), sPath.c_str());
	WritePrivateProfileString(_T("Session"), _T("TreeMode"),
		GetOptionsMgr()->GetBool(OPT_TREE_MODE) ? _T("1") : _T("0"), sPath.c_str());
	WritePrivateProfileString(_T("Session"), _T("FlattenMode"),
		GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_FLATTEN_MODE) ? _T("1") : _T("0"), sPath.c_str());

	// Window position
	WINDOWPLACEMENT wp = {};
	wp.length = sizeof(wp);
	GetWindowPlacement(&wp);
	String sWinPos = strutils::format(_T("%d,%d,%d,%d,%d"),
		wp.rcNormalPosition.left, wp.rcNormalPosition.top,
		wp.rcNormalPosition.right, wp.rcNormalPosition.bottom,
		wp.showCmd);
	WritePrivateProfileString(_T("Workspace"), _T("WindowPlacement"),
		sWinPos.c_str(), sPath.c_str());

	// Splitter position
	if (m_bSideBySideMode && ::IsWindow(m_wndSplitter.m_hWnd))
	{
		int wLeft, wMin;
		m_wndSplitter.GetColumnInfo(0, wLeft, wMin);
		String sSplitter = strutils::format(_T("%d"), wLeft);
		WritePrivateProfileString(_T("Workspace"), _T("SplitterPos"),
			sSplitter.c_str(), sPath.c_str());
	}

	// Column widths
	if (m_pLeftPaneView)
	{
		m_pLeftPaneView->SaveColumnState();
		WritePrivateProfileString(_T("Workspace"), _T("LeftColumnWidths"),
			GetOptionsMgr()->GetString(OPT_DIRVIEW_SXS_LEFT_COLUMN_WIDTHS).c_str(), sPath.c_str());
		WritePrivateProfileString(_T("Workspace"), _T("LeftColumnOrders"),
			GetOptionsMgr()->GetString(OPT_DIRVIEW_SXS_LEFT_COLUMN_ORDERS).c_str(), sPath.c_str());
	}
	if (m_pRightPaneView)
	{
		m_pRightPaneView->SaveColumnState();
		WritePrivateProfileString(_T("Workspace"), _T("RightColumnWidths"),
			GetOptionsMgr()->GetString(OPT_DIRVIEW_SXS_RIGHT_COLUMN_WIDTHS).c_str(), sPath.c_str());
		WritePrivateProfileString(_T("Workspace"), _T("RightColumnOrders"),
			GetOptionsMgr()->GetString(OPT_DIRVIEW_SXS_RIGHT_COLUMN_ORDERS).c_str(), sPath.c_str());
	}

	if (m_pCoordinator)
		m_pCoordinator->LogOperation(_T("Workspace saved to: ") + sPath);
}

/**
 * @brief Load a saved workspace and restore all settings.
 * Reads paths, window position, splitter pos, column widths, and reopens the comparison.
 */
void CDirFrame::OnSxsWorkspaceLoad()
{
	CFileDialog dlg(TRUE, _T("wmwks"), nullptr,
		OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		_T("WinMerge SxS Workspace (*.wmwks)|*.wmwks|All Files (*.*)|*.*||"));
	if (dlg.DoModal() != IDOK)
		return;

	String sPath = dlg.GetPathName().GetString();

	tchar_t szLeft[MAX_PATH] = {};
	tchar_t szRight[MAX_PATH] = {};
	tchar_t szRecurse[8] = {};
	tchar_t szFilter[MAX_PATH] = {};
	tchar_t szSxS[8] = {};
	tchar_t szTree[8] = {};
	tchar_t szFlatten[8] = {};

	GetPrivateProfileString(_T("Session"), _T("LeftPath"), _T(""), szLeft, MAX_PATH, sPath.c_str());
	GetPrivateProfileString(_T("Session"), _T("RightPath"), _T(""), szRight, MAX_PATH, sPath.c_str());
	GetPrivateProfileString(_T("Session"), _T("Recursive"), _T("0"), szRecurse, 8, sPath.c_str());
	GetPrivateProfileString(_T("Session"), _T("Filter"), _T("*.*"), szFilter, MAX_PATH, sPath.c_str());
	GetPrivateProfileString(_T("Session"), _T("SideBySideMode"), _T("1"), szSxS, 8, sPath.c_str());
	GetPrivateProfileString(_T("Session"), _T("TreeMode"), _T("0"), szTree, 8, sPath.c_str());
	GetPrivateProfileString(_T("Session"), _T("FlattenMode"), _T("0"), szFlatten, 8, sPath.c_str());

	if (szLeft[0] == _T('\0') || szRight[0] == _T('\0'))
	{
		AfxMessageBox(_T("Invalid workspace file: missing paths."), MB_ICONERROR);
		return;
	}

	// Apply settings
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SIDEBYSIDE_MODE, _ttoi(szSxS) != 0);
	GetOptionsMgr()->SaveOption(OPT_TREE_MODE, _ttoi(szTree) != 0);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_FLATTEN_MODE, _ttoi(szFlatten) != 0);
	if (szFilter[0] != _T('\0'))
		GetOptionsMgr()->SaveOption(OPT_FILEFILTER_CURRENT, String(szFilter));

	// Restore workspace-specific settings (column widths, splitter pos)
	tchar_t szBuf[512] = {};
	GetPrivateProfileString(_T("Workspace"), _T("SplitterPos"), _T("0"), szBuf, 512, sPath.c_str());
	int nSplitterPos = _ttoi(szBuf);
	if (nSplitterPos > 0)
		GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_SPLITTER_POS, nSplitterPos);

	GetPrivateProfileString(_T("Workspace"), _T("LeftColumnWidths"), _T(""), szBuf, 512, sPath.c_str());
	if (szBuf[0] != _T('\0'))
		GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_LEFT_COLUMN_WIDTHS, String(szBuf));

	GetPrivateProfileString(_T("Workspace"), _T("LeftColumnOrders"), _T(""), szBuf, 512, sPath.c_str());
	if (szBuf[0] != _T('\0'))
		GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_LEFT_COLUMN_ORDERS, String(szBuf));

	GetPrivateProfileString(_T("Workspace"), _T("RightColumnWidths"), _T(""), szBuf, 512, sPath.c_str());
	if (szBuf[0] != _T('\0'))
		GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_RIGHT_COLUMN_WIDTHS, String(szBuf));

	GetPrivateProfileString(_T("Workspace"), _T("RightColumnOrders"), _T(""), szBuf, 512, sPath.c_str());
	if (szBuf[0] != _T('\0'))
		GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_RIGHT_COLUMN_ORDERS, String(szBuf));

	bool bRecurse = (_ttoi(szRecurse) != 0);

	PathContext pathCtx;
	pathCtx.SetLeft(szLeft);
	pathCtx.SetRight(szRight);
	fileopenflags_t dwFlags[3] = {};
	GetMainFrame()->DoFileOrFolderOpen(&pathCtx, dwFlags, nullptr, _T(""),
		bRecurse, nullptr);

	// Restore window placement after reopening
	tchar_t szWinPos[256] = {};
	GetPrivateProfileString(_T("Workspace"), _T("WindowPlacement"), _T(""), szWinPos, 256, sPath.c_str());
	if (szWinPos[0] != _T('\0'))
	{
		int l, t, r, b, sc;
		if (_stscanf_s(szWinPos, _T("%d,%d,%d,%d,%d"), &l, &t, &r, &b, &sc) == 5)
		{
			WINDOWPLACEMENT wp = {};
			wp.length = sizeof(wp);
			wp.rcNormalPosition = { l, t, r, b };
			wp.showCmd = sc;
			SetWindowPlacement(&wp);
		}
	}
}

/**
 * @brief Navigate back in folder comparison history.
 */
void CDirFrame::OnSxsNavBack()
{
	if (!m_bSideBySideMode || !m_pCoordinator)
		return;
	String leftPath, rightPath;
	if (m_pCoordinator->NavigateBack(leftPath, rightPath))
	{
		// Re-open comparison with these paths
		CDirDoc* pDoc = dynamic_cast<CDirDoc*>(GetActiveDocument());
		if (pDoc)
		{
			PathContext pathCtx;
			pathCtx.SetLeft(leftPath.c_str());
			pathCtx.SetRight(rightPath.c_str());
			const CDiffContext& ctxt = pDoc->GetDiffContext();
			fileopenflags_t dwFlags[3] = {};
			GetMainFrame()->DoFileOrFolderOpen(&pathCtx, dwFlags, nullptr, _T(""),
				ctxt.m_bRecursive, nullptr);
		}
	}
}

/**
 * @brief Navigate forward in folder comparison history.
 */
void CDirFrame::OnSxsNavForward()
{
	if (!m_bSideBySideMode || !m_pCoordinator)
		return;
	String leftPath, rightPath;
	if (m_pCoordinator->NavigateForward(leftPath, rightPath))
	{
		// Re-open comparison with these paths
		CDirDoc* pDoc = dynamic_cast<CDirDoc*>(GetActiveDocument());
		if (pDoc)
		{
			PathContext pathCtx;
			pathCtx.SetLeft(leftPath.c_str());
			pathCtx.SetRight(rightPath.c_str());
			const CDiffContext& ctxt = pDoc->GetDiffContext();
			fileopenflags_t dwFlags[3] = {};
			GetMainFrame()->DoFileOrFolderOpen(&pathCtx, dwFlags, nullptr, _T(""),
				ctxt.m_bRecursive, nullptr);
		}
	}
}

/**
 * @brief Update UI for back navigation — enable if history is available.
 */
void CDirFrame::OnUpdateSxsNavBack(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_bSideBySideMode && m_pCoordinator && m_pCoordinator->CanNavigateBack());
}

/**
 * @brief Update UI for forward navigation — enable if forward history is available.
 */
void CDirFrame::OnUpdateSxsNavForward(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_bSideBySideMode && m_pCoordinator && m_pCoordinator->CanNavigateForward());
}

/**
 * @brief Navigate up one level in the folder hierarchy.
 */
void CDirFrame::OnSxsUpLevel()
{
	if (!m_bSideBySideMode || !m_pCoordinator)
		return;
	String leftParent, rightParent;
	if (m_pCoordinator->GetParentPaths(leftParent, rightParent))
	{
		// Push current paths to history
		CDirDoc* pDoc = dynamic_cast<CDirDoc*>(GetActiveDocument());
		if (pDoc)
		{
			const CDiffContext& ctxt = pDoc->GetDiffContext();
			m_pCoordinator->PushHistory(ctxt.GetLeftPath(), ctxt.GetRightPath());

			PathContext pathCtx;
			pathCtx.SetLeft(leftParent.c_str());
			pathCtx.SetRight(rightParent.c_str());
			fileopenflags_t dwFlags[3] = {};
			GetMainFrame()->DoFileOrFolderOpen(&pathCtx, dwFlags, nullptr, _T(""),
				ctxt.m_bRecursive, nullptr);
		}
	}
}

