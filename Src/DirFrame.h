/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  DirFrame.h
 *
 * @brief Declaration file for CDirFrame
 *
 */
#pragma once

#include "EditorFilepathBar.h"
#include "DirSideBySideHeaderBar.h"
#include "DirSideBySideFilterBar.h"
#include "DirSxSToolBar.h"
#include "BasicFlatStatusBar.h"
#include "DirCompProgressBar.h"
#include "DirFilterBar.h"
#include "MergeFrameCommon.h"
#include "Common/SplitterWndEx.h"
#include <memory>

class CDirPaneView;
class CDirSideBySideCoordinator;
class CDirGutterView;

/////////////////////////////////////////////////////////////////////////////
// CDirFrame frame

/**
 * @brief Frame window for Directory Compare window
 */
class CDirFrame : public CMergeFrameCommon
{
	DECLARE_DYNCREATE(CDirFrame)
protected:
	CDirFrame();           // protected constructor used by dynamic creation

// Attributes
public:

private:

// Operations
public:
	void SetStatus(const tchar_t* szStatus);
	void SetCompareMethodStatusDisplay(int nCompMethod);
	void SetFilterStatusDisplay(const tchar_t* szFilter);
	CBasicFlatStatusBar m_wndStatusBar;
	IHeaderBar * GetHeaderInterface() override;
	void UpdateResources();
	void ShowProgressBar();
	void HideProgressBar();
	void ShowFilterBar();
	void HideFilterBar();
	DirCompProgressBar* GetCompProgressBar() { return m_pCmpProgressBar.get(); }
	CDirFilterBar* GetFilterBar() { return m_pDirFilterBar.get(); }

	bool IsSideBySideMode() const { return m_bSideBySideMode; }
	CDirSideBySideCoordinator* GetCoordinator() { return m_pCoordinator.get(); }
	CDirPaneView* GetLeftPaneView() { return m_pLeftPaneView; }
	CDirPaneView* GetRightPaneView() { return m_pRightPaneView; }
	CDirGutterView* GetGutterView() { return m_pGutterView; }

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDirFrame)
	public:
	virtual void ActivateFrame(int nCmdShow = -1);
	virtual BOOL DestroyWindow();
	virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
	//}}AFX_VIRTUAL

protected:
	CEditorFilePathBar m_wndFilePathBar;
	CDirSideBySideHeaderBar m_wndSxSHeaderBar;
	CDirSideBySideFilterBar m_wndSxSFilterBar;
	CDirSxSToolBar m_wndSxSToolBar;
	std::unique_ptr<DirCompProgressBar> m_pCmpProgressBar;
	std::unique_ptr<CDirFilterBar> m_pDirFilterBar;

	// Side-by-side mode
	bool m_bSideBySideMode;
	bool m_bSplitterCreated;
	CSplitterWndEx m_wndSplitter;
	CDirPaneView *m_pLeftPaneView;
	CDirPaneView *m_pRightPaneView;
	CDirGutterView *m_pGutterView;
	std::unique_ptr<CDirSideBySideCoordinator> m_pCoordinator;

	virtual ~CDirFrame();

	// Generated message map functions
	//{{AFX_MSG(CDirFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClose();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnViewDisplayFilterBar();
	afx_msg void OnUpdateDisplayViewFilterBar(CCmdUI* pCmdUI);
	afx_msg void OnDisplayFilterBarClose();
	afx_msg void OnDisplayFilterBarMaskMenu();
	afx_msg void OnViewSideBySide();
	afx_msg void OnUpdateViewSideBySide(CCmdUI* pCmdUI);
	afx_msg void OnIdleUpdateCmdUI();
	afx_msg void OnSxsSwapSides();
	afx_msg void OnUpdateSxsCommand(CCmdUI* pCmdUI);
	afx_msg void OnSxsLegend();
	afx_msg void OnUpdateSxsLegend(CCmdUI* pCmdUI);
	afx_msg void OnActivateApp(BOOL bActive, DWORD dwThreadID);
	afx_msg void OnSxsSessionSave();
	afx_msg void OnSxsSessionLoad();
	afx_msg void OnSxsWorkspaceSave();
	afx_msg void OnSxsWorkspaceLoad();
public:
	afx_msg void OnSxsNavBack();
	afx_msg void OnSxsNavForward();
	afx_msg void OnSxsUpLevel();
	afx_msg void OnUpdateSxsRange(CCmdUI* pCmdUI);
	// Diffs dropdown presets
	afx_msg void OnSxsDiffsShowDiffs();
	afx_msg void OnSxsDiffsNoOrphans();
	afx_msg void OnSxsDiffsNoOrphansDiff();
	afx_msg void OnSxsDiffsOrphans();
	afx_msg void OnSxsDiffsLeftNewer();
	afx_msg void OnSxsDiffsRightNewer();
	afx_msg void OnSxsDiffsLeftNewerOrphans();
	afx_msg void OnSxsDiffsRightNewerOrphans();
	afx_msg void OnSxsDiffsLeftOrphans();
	afx_msg void OnSxsDiffsRightOrphans();
	// Structure dropdown
	afx_msg void OnSxsStructAlwaysFolders();
	afx_msg void OnSxsStructFilesAndFolders();
	afx_msg void OnSxsStructOnlyFiles();
	afx_msg void OnSxsStructIgnoreStructure();
	// Session settings dialog
	afx_msg void OnSxsSessionSettings();
	// Home button
	afx_msg void OnSxsHome();
	// Forward native WinMerge commands to SxS panes
	afx_msg void OnFwdCopyLR();
	afx_msg void OnFwdCopyRL();
	afx_msg void OnFwdDelLeft();
	afx_msg void OnFwdDelRight();
	afx_msg void OnFwdDelBoth();
	afx_msg void OnFwdRefresh();
	afx_msg void OnFwdSelectAll();
protected:
	afx_msg void OnUpdateSxsNavBack(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSxsNavForward(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	void UpdateHeaderSizes();
};


