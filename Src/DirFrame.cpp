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
#include "DirSxSToolBar.h"
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
#include "DirSxSSessionDlg.h"
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
 * @brief Width of center gutter column in SxS mode (thin=4px, classic=24px)
 */
static int GetGutterColWidth()
{
	return GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_THIN_GUTTER) ? 4 : 24;
}
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
, m_bSplitterCreated(false)
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
	ON_UPDATE_COMMAND_UI_RANGE(ID_DIR_SXS_HOME, ID_DIR_SXS_STRUCTURE, OnUpdateSxsRange)
	ON_UPDATE_COMMAND_UI_RANGE(ID_DIR_SXS_SHOW_MINOR, ID_DIR_SXS_STOP, OnUpdateSxsRange)
	ON_UPDATE_COMMAND_UI_RANGE(ID_DIR_SXS_STRUCT_ALWAYS_FOLDERS, ID_DIR_SXS_DIFFS_RIGHT_ORPHANS, OnUpdateSxsRange)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_ALL, OnUpdateSxsRange)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_IDENTICAL, OnUpdateSxsRange)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_NEXT_DIFF, OnUpdateSxsRange)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_EXPAND_ALL, OnUpdateSxsRange)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_COLLAPSE_ALL, OnUpdateSxsRange)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_SELECT_ALL, OnUpdateSxsRange)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_COPY_TO_FOLDER, OnUpdateSxsRange)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_MOVE_TO_FOLDER, OnUpdateSxsRange)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_REFRESH, OnUpdateSxsRange)
	ON_COMMAND(ID_DIR_SXS_DIFFS_SHOW_DIFFS, OnSxsDiffsShowDiffs)
	ON_COMMAND(ID_DIR_SXS_DIFFS_NO_ORPHANS, OnSxsDiffsNoOrphans)
	ON_COMMAND(ID_DIR_SXS_DIFFS_NO_ORPHANS_DIFF, OnSxsDiffsNoOrphansDiff)
	ON_COMMAND(ID_DIR_SXS_DIFFS_ORPHANS, OnSxsDiffsOrphans)
	ON_COMMAND(ID_DIR_SXS_DIFFS_LEFT_NEWER, OnSxsDiffsLeftNewer)
	ON_COMMAND(ID_DIR_SXS_DIFFS_RIGHT_NEWER, OnSxsDiffsRightNewer)
	ON_COMMAND(ID_DIR_SXS_DIFFS_LEFT_NEWER_ORPHANS, OnSxsDiffsLeftNewerOrphans)
	ON_COMMAND(ID_DIR_SXS_DIFFS_RIGHT_NEWER_ORPHANS, OnSxsDiffsRightNewerOrphans)
	ON_COMMAND(ID_DIR_SXS_DIFFS_LEFT_ORPHANS, OnSxsDiffsLeftOrphans)
	ON_COMMAND(ID_DIR_SXS_DIFFS_RIGHT_ORPHANS, OnSxsDiffsRightOrphans)
	ON_COMMAND(ID_DIR_SXS_STRUCT_ALWAYS_FOLDERS, OnSxsStructAlwaysFolders)
	ON_COMMAND(ID_DIR_SXS_STRUCT_FILES_AND_FOLDERS, OnSxsStructFilesAndFolders)
	ON_COMMAND(ID_DIR_SXS_STRUCT_ONLY_FILES, OnSxsStructOnlyFiles)
	ON_COMMAND(ID_DIR_SXS_STRUCT_IGNORE_STRUCTURE, OnSxsStructIgnoreStructure)
	ON_COMMAND(ID_DIR_SXS_SESSION_SETTINGS, OnSxsSessionSettings)
	ON_COMMAND(ID_DIR_SXS_HOME, OnSxsHome)
	// Forward standard WinMerge commands to active SxS pane
	ON_COMMAND(ID_DIR_COPY_LEFT_TO_RIGHT, OnFwdCopyLR)
	ON_COMMAND(ID_DIR_COPY_RIGHT_TO_LEFT, OnFwdCopyRL)
	ON_COMMAND(ID_DIR_DEL_LEFT, OnFwdDelLeft)
	ON_COMMAND(ID_DIR_DEL_RIGHT, OnFwdDelRight)
	ON_COMMAND(ID_DIR_DEL_BOTH, OnFwdDelBoth)
	ON_COMMAND(ID_REFRESH, OnFwdRefresh)
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnFwdSelectAll)
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

	// Create the SxS toolbar (initially hidden; shown when SxS mode is active)
	if (!m_wndSxSToolBar.Create(this))
	{
		TRACE0("Failed to create SxS toolbar\n");
		return -1;
	}
	ShowControlBar(&m_wndSxSToolBar, FALSE, FALSE);

	// Now that all bars are created, apply deferred SxS visibility
	// (OnCreateClient ran before these bars existed, so ShowControlBar was skipped)
	if (m_bSideBySideMode)
	{
		ShowControlBar(&m_wndFilePathBar, FALSE, FALSE);
		ShowControlBar(&m_wndSxSToolBar, TRUE, FALSE);
		ShowControlBar(&m_wndSxSHeaderBar, TRUE, FALSE);
		if (GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_SHOW_FILTER_BAR))
			ShowControlBar(&m_wndSxSFilterBar, TRUE, FALSE);
	}
	else
	{
		ShowControlBar(&m_wndSxSHeaderBar, FALSE, FALSE);
	}

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

		// Clear coordinator pointers BEFORE child windows are destroyed.
		// __super::DestroyWindow() will destroy the CDirPaneView instances
		// (CView::PostNcDestroy → delete this), leaving raw pointers dangling.
		// The CDirDoc also holds a raw pointer to the coordinator — clear it
		// so the DiffThread callback and other code paths don't dereference freed memory.
		if (m_pLeftPaneView)
			m_pLeftPaneView->SetCoordinator(nullptr);
		if (m_pRightPaneView)
			m_pRightPaneView->SetCoordinator(nullptr);
		if (m_pGutterView)
			m_pGutterView->SetCoordinator(nullptr);
		m_wndSxSFilterBar.SetCoordinator(nullptr);

		CDirDoc *pDoc = dynamic_cast<CDirDoc*>(GetActiveDocument());
		if (pDoc)
			pDoc->SetCoordinator(nullptr);
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

	TRACE(_T("CDirFrame::OnCreateClient — SxS mode = %d\n"), (int)m_bSideBySideMode);
	OutputDebugString(m_bSideBySideMode
		? _T("WinMerge: OnCreateClient -> SxS mode ENABLED\n")
		: _T("WinMerge: OnCreateClient -> SxS mode DISABLED (unified)\n"));

	if (!m_bSideBySideMode)
	{
		// Hide the SxS header bar, show the standard one (only if already created)
		if (::IsWindow(m_wndSxSHeaderBar.m_hWnd))
			ShowControlBar(&m_wndSxSHeaderBar, FALSE, FALSE);
		return __super::OnCreateClient(lpcs, pContext);
	}

	// SxS mode: hide the standard header bar (path combos are now in each pane)
	// Note: bars may not be created yet if OnCreateClient is called from __super::OnCreate
	// before the bars are created in CDirFrame::OnCreate. Guard with IsWindow checks.
	if (::IsWindow(m_wndFilePathBar.m_hWnd))
		ShowControlBar(&m_wndFilePathBar, FALSE, FALSE);
	if (::IsWindow(m_wndSxSHeaderBar.m_hWnd))
		ShowControlBar(&m_wndSxSHeaderBar, FALSE, FALSE);
	if (::IsWindow(m_wndSxSFilterBar.m_hWnd) && GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_SHOW_FILTER_BAR))
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
			CSize(GetGutterColWidth(), 100), pContext))
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

	// SxS mode must always scan recursively (like Beyond Compare) so that
	// directories have children and the tree can be expanded / collapsed.
	GetOptionsMgr()->SaveOption(OPT_CMP_INCLUDE_SUBDIRS, true);

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

	// Wire the header bar callbacks
	m_wndSxSHeaderBar.SetPaneCount(2);
	m_wndSxSHeaderBar.SetOnBackCallback([this](int pane) {
		OnSxsNavBack();
	});
	m_wndSxSHeaderBar.SetOnBrowseCallback([this](int pane) {
		CFolderPickerDialog dlg(nullptr, 0, this);
		if (dlg.DoModal() == IDOK)
		{
			String newPath(dlg.GetPathName().GetString());
			CDirDoc* pDoc = dynamic_cast<CDirDoc*>(GetActiveDocument());
			if (pDoc)
			{
				const CDiffContext& ctxt = pDoc->GetDiffContext();
				m_pCoordinator->PushHistory(ctxt.GetLeftPath(), ctxt.GetRightPath());
				PathContext pathCtx;
				if (pane == 0)
				{
					pathCtx.SetLeft(newPath.c_str());
					pathCtx.SetRight(ctxt.GetRightPath().c_str());
				}
				else
				{
					pathCtx.SetLeft(ctxt.GetLeftPath().c_str());
					pathCtx.SetRight(newPath.c_str());
				}
				fileopenflags_t dwFlags[3] = {};
				GetMainFrame()->DoFileOrFolderOpen(&pathCtx, dwFlags, nullptr, _T(""),
					ctxt.m_bRecursive, nullptr);
			}
		}
	});
	m_wndSxSHeaderBar.SetOnUpLevelCallback([this](int pane) {
		OnSxsUpLevel();
	});

	// Set proper column widths: split available width between left and right panes,
	// keeping gutter at fixed GetGutterColWidth().  Do NOT use EqualizeCols() because
	// it distributes width equally across ALL columns including the gutter.
	{
		CRect rc;
		GetClientRect(&rc);
		int totalWidth = rc.Width();
		if (totalWidth <= 0)
			totalWidth = 800; // fallback if client rect not yet valid

		int nSplitterPos = GetOptionsMgr()->GetInt(OPT_DIRVIEW_SXS_SPLITTER_POS);

		if (bShowGutter)
		{
			int gutterW = GetGutterColWidth();
			int paneSpace = totalWidth - gutterW;
			if (paneSpace < 100) paneSpace = 100;

			int leftW, rightW;
			if (nSplitterPos > 0 && nSplitterPos < paneSpace - 50)
			{
				leftW = nSplitterPos;
				rightW = paneSpace - leftW;
			}
			else
			{
				leftW = paneSpace / 2;
				rightW = paneSpace - leftW;
			}

			m_wndSplitter.SetColumnInfo(0, leftW, 50);
			m_wndSplitter.SetColumnInfo(1, gutterW, gutterW);
			m_wndSplitter.SetColumnInfo(2, rightW, 50);
		}
		else
		{
			int leftW, rightW;
			if (nSplitterPos > 0 && nSplitterPos < totalWidth - 50)
			{
				leftW = nSplitterPos;
				rightW = totalWidth - leftW;
			}
			else
			{
				leftW = totalWidth / 2;
				rightW = totalWidth - leftW;
			}

			m_wndSplitter.SetColumnInfo(0, leftW, 50);
			m_wndSplitter.SetColumnInfo(1, rightW, 50);
		}

		m_wndSplitter.RecalcLayout();
	}

	m_bSplitterCreated = true;
	TRACE(_T("CDirFrame::OnCreateClient — SxS splitter created OK, %d cols\n"), nCols);
	return TRUE;
}

/**
 * @brief Get the interface to the header (path) bar.
 * In SxS mode, returns the SxS header bar. Otherwise returns the standard file path bar.
 */
IHeaderBar * CDirFrame::GetHeaderInterface()
{
	if (m_bSideBySideMode)
		return &m_wndSxSHeaderBar;
	return &m_wndFilePathBar;
}

/**
 * @brief Sync header bar widths with the splitter column widths.
 */
void CDirFrame::UpdateHeaderSizes()
{
	if (!m_bSideBySideMode)
		return;
	if (!m_bSplitterCreated)
		return;
	if (!::IsWindow(m_wndSplitter.m_hWnd))
		return;

	int nCols = m_wndSplitter.GetColumnCount();
	int w[2] = { 1, 1 };
	int offsets[2] = { 0, 0 };

	if (nCols == 3)
	{
		// 3-column: left pane (col 0), gutter (col 1), right pane (col 2)
		int wmin, wGutter;
		m_wndSplitter.GetColumnInfo(0, w[0], wmin);
		m_wndSplitter.GetColumnInfo(1, wGutter, wmin);
		m_wndSplitter.GetColumnInfo(2, w[1], wmin);
		// Left pane starts at 0, right pane starts after left + gutter + splitter bars
		offsets[0] = 0;
		offsets[1] = w[0] + wGutter + m_wndSplitter.GetColumnCount(); // account for splitter borders
		// Get actual right pane position from the splitter
		CRect rcRight;
		m_wndSplitter.GetPane(0, 2)->GetWindowRect(&rcRight);
		CRect rcSplitter;
		m_wndSplitter.GetWindowRect(&rcSplitter);
		offsets[1] = rcRight.left - rcSplitter.left;
	}
	else
	{
		for (int pane = 0; pane < nCols && pane < 2; pane++)
		{
			int wmin;
			m_wndSplitter.GetColumnInfo(pane, w[pane], wmin);
		}
		offsets[0] = 0;
		// Get actual right pane position from the splitter
		CRect rcRight;
		m_wndSplitter.GetPane(0, 1)->GetWindowRect(&rcRight);
		CRect rcSplitter;
		m_wndSplitter.GetWindowRect(&rcSplitter);
		offsets[1] = rcRight.left - rcSplitter.left;
	}

	if (w[0] < 1) w[0] = 1;
	if (w[1] < 1) w[1] = 1;

	// Resize the header bar to match splitter column widths
	m_wndSxSHeaderBar.Resize(w, offsets);

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
	// Toggle the option
	bool bCurrent = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SIDEBYSIDE_MODE);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SIDEBYSIDE_MODE, !bCurrent);

	// Reopen the comparison in the new mode.
	// We must close this frame and open a fresh one because the splitter/views
	// are created once during OnCreateClient and cannot be rebuilt in-place.
	CDirDoc *pDoc = dynamic_cast<CDirDoc*>(GetActiveDocument());
	if (pDoc && pDoc->HasDiffs())
	{
		const CDiffContext &ctxt = pDoc->GetDiffContext();
		PathContext paths = ctxt.GetNormalizedPaths();
		bool bRecursive = ctxt.m_bRecursive;
		fileopenflags_t dwFlags[3] = {};

		// Open the new comparison first (creates a new frame in the new mode)
		GetMainFrame()->DoFileOrFolderOpen(&paths, dwFlags, nullptr, _T(""),
			bRecursive, nullptr);

		// Close this old frame
		PostMessage(WM_CLOSE);
	}
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
 * @brief Enable/disable range handler for all SxS toolbar commands.
 */
void CDirFrame::OnUpdateSxsRange(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_bSideBySideMode);
}

// --- Diffs dropdown preset handlers ---
static void ApplyDiffsPreset(bool bDiff, bool bSame, bool bOrpL, bool bOrpR, bool bNewerL, bool bNewerR,
	CDirSideBySideCoordinator* pCoord)
{
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT, bDiff);
	GetOptionsMgr()->SaveOption(OPT_SHOW_IDENTICAL, bSame);
	GetOptionsMgr()->SaveOption(OPT_SHOW_UNIQUE_LEFT, bOrpL);
	GetOptionsMgr()->SaveOption(OPT_SHOW_UNIQUE_RIGHT, bOrpR);
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT_LEFT_ONLY, bNewerL);
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT_RIGHT_ONLY, bNewerR);
	if (pCoord)
		pCoord->Redisplay();
}

void CDirFrame::OnSxsDiffsShowDiffs()
{
	ApplyDiffsPreset(true, false, true, true, true, true, m_pCoordinator.get());
}
void CDirFrame::OnSxsDiffsNoOrphans()
{
	ApplyDiffsPreset(true, true, false, false, true, true, m_pCoordinator.get());
}
void CDirFrame::OnSxsDiffsNoOrphansDiff()
{
	ApplyDiffsPreset(true, false, false, false, true, true, m_pCoordinator.get());
}
void CDirFrame::OnSxsDiffsOrphans()
{
	ApplyDiffsPreset(false, false, true, true, false, false, m_pCoordinator.get());
}
void CDirFrame::OnSxsDiffsLeftNewer()
{
	ApplyDiffsPreset(false, false, false, false, true, false, m_pCoordinator.get());
}
void CDirFrame::OnSxsDiffsRightNewer()
{
	ApplyDiffsPreset(false, false, false, false, false, true, m_pCoordinator.get());
}
void CDirFrame::OnSxsDiffsLeftNewerOrphans()
{
	ApplyDiffsPreset(false, false, true, false, true, false, m_pCoordinator.get());
}
void CDirFrame::OnSxsDiffsRightNewerOrphans()
{
	ApplyDiffsPreset(false, false, false, true, false, true, m_pCoordinator.get());
}
void CDirFrame::OnSxsDiffsLeftOrphans()
{
	ApplyDiffsPreset(false, false, true, false, false, false, m_pCoordinator.get());
}
void CDirFrame::OnSxsDiffsRightOrphans()
{
	ApplyDiffsPreset(false, false, false, true, false, false, m_pCoordinator.get());
}

// --- Structure dropdown handlers ---
void CDirFrame::OnSxsStructAlwaysFolders()
{
	if (m_pCoordinator)
	{
		m_pCoordinator->SetAlwaysShowFolders(true);
		m_pCoordinator->SetIgnoreFolderStructure(false);
		GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_FLATTEN_MODE, false);
		m_pCoordinator->Redisplay();
	}
}
void CDirFrame::OnSxsStructFilesAndFolders()
{
	if (m_pCoordinator)
	{
		m_pCoordinator->SetAlwaysShowFolders(false);
		m_pCoordinator->SetIgnoreFolderStructure(false);
		GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_FLATTEN_MODE, false);
		m_pCoordinator->Redisplay();
	}
}
void CDirFrame::OnSxsStructOnlyFiles()
{
	if (m_pCoordinator)
	{
		m_pCoordinator->SetAlwaysShowFolders(false);
		m_pCoordinator->SetIgnoreFolderStructure(false);
		GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_FLATTEN_MODE, true);
		m_pCoordinator->Redisplay();
	}
}
void CDirFrame::OnSxsStructIgnoreStructure()
{
	if (m_pCoordinator)
	{
		m_pCoordinator->SetAlwaysShowFolders(false);
		m_pCoordinator->SetIgnoreFolderStructure(true);
		GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_FLATTEN_MODE, false);
		m_pCoordinator->Redisplay();
	}
}

// --- Session Settings dialog ---
void CDirFrame::OnSxsSessionSettings()
{
	CDirSxSSessionDlg dlg(this, m_pCoordinator.get());

	// Populate paths from current comparison
	CDirDoc* pDoc = dynamic_cast<CDirDoc*>(GetActiveDocument());
	if (pDoc && pDoc->HasDiffs())
	{
		const CDiffContext &ctxt = pDoc->GetDiffContext();
		PathContext paths = ctxt.GetNormalizedPaths();
		dlg.m_pageSpecs.m_sLeftPath = paths.GetLeft();
		dlg.m_pageSpecs.m_sRightPath = paths.GetRight();
	}

	dlg.LoadFromOptions();

	if (dlg.DoModal() == IDOK)
	{
		dlg.SaveToOptions();
		if (m_pCoordinator)
			m_pCoordinator->Redisplay();
	}
}

// --- Home button ---
void CDirFrame::OnSxsHome()
{
	// Navigate to top-level / close current comparison
	GetMainFrame()->PostMessage(WM_COMMAND, ID_FILE_OPEN);
}

// --- Forward standard WinMerge commands to the active SxS pane ---

void CDirFrame::OnFwdCopyLR()
{
	if (m_bSideBySideMode && m_pLeftPaneView)
		m_pLeftPaneView->SendMessage(WM_COMMAND, ID_DIR_SXS_COPY);
}

void CDirFrame::OnFwdCopyRL()
{
	if (m_bSideBySideMode && m_pRightPaneView)
		m_pRightPaneView->SendMessage(WM_COMMAND, ID_DIR_SXS_COPY);
}

void CDirFrame::OnFwdDelLeft()
{
	if (m_bSideBySideMode && m_pLeftPaneView)
		m_pLeftPaneView->SendMessage(WM_COMMAND, ID_DIR_SXS_DELETE);
}

void CDirFrame::OnFwdDelRight()
{
	if (m_bSideBySideMode && m_pRightPaneView)
		m_pRightPaneView->SendMessage(WM_COMMAND, ID_DIR_SXS_DELETE);
}

void CDirFrame::OnFwdDelBoth()
{
	if (m_bSideBySideMode)
	{
		if (m_pLeftPaneView)
			m_pLeftPaneView->SendMessage(WM_COMMAND, ID_DIR_SXS_DELETE);
		if (m_pRightPaneView)
			m_pRightPaneView->SendMessage(WM_COMMAND, ID_DIR_SXS_DELETE);
	}
}

void CDirFrame::OnFwdRefresh()
{
	if (m_bSideBySideMode && m_pLeftPaneView)
		m_pLeftPaneView->SendMessage(WM_COMMAND, ID_DIR_SXS_REFRESH);
}

void CDirFrame::OnFwdSelectAll()
{
	if (m_bSideBySideMode && m_pLeftPaneView)
		m_pLeftPaneView->SendMessage(WM_COMMAND, ID_DIR_SXS_SELECT_ALL);
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

