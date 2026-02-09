/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirPaneView.h
 *
 * @brief Declaration of CDirPaneView class for side-by-side folder comparison
 */
#pragma once

#include <afxcview.h>
#include <memory>
#include <vector>
#include <map>
#include "OptionsDirColors.h"
#include "SortHeaderCtrl.h"
#include "UnicodeString.h"
#include "IListCtrlImpl.h"

class CDirDoc;
class CDirFrame;
class CDiffContext;
class DirViewColItems;
class CDirSideBySideCoordinator;
class DIFFITEM;

/**
 * @brief Per-pane list view for side-by-side folder comparison.
 *
 * Each instance of this class shows one side (left or right) of the
 * folder comparison. Uses LVS_OWNERDATA virtual list mode.
 * Items missing on this side are shown as placeholder blank rows.
 */
class CDirPaneView : public CListView
{
	DECLARE_DYNCREATE(CDirPaneView)
protected:
	CDirPaneView();

public:
	virtual ~CDirPaneView();

	CDirDoc* GetDocument();
	const CDirDoc* GetDocument() const { return const_cast<CDirPaneView*>(this)->GetDocument(); }
	CDirFrame* GetParentFrame();

	/** Set the pane index (0=left, 1=right) */
	void SetPaneIndex(int nPane) { m_nThisPane = nPane; }
	int GetPaneIndex() const { return m_nThisPane; }

	/** Set the coordinator that manages cross-pane synchronization */
	void SetCoordinator(CDirSideBySideCoordinator *pCoordinator) { m_pCoordinator = pCoordinator; }

	/** Called by coordinator to update the display from the row mapping */
	void UpdateFromRowMapping();

	/** Delete all displayed items */
	void DeleteAllDisplayItems();

	/** Get the DIFFITEM key for a given list index */
	DIFFITEM* GetItemKey(int idx) const;

	const CDiffContext& GetDiffContext() const;
	CDiffContext& GetDiffContext();

	void SaveColumnState();

	/** Get the DirViewColItems for sort/column operations (used by coordinator) */
	DirViewColItems* GetColItems() const { return m_pColItems.get(); }

	/** Update the sort header arrow indicator */
	void UpdateSortHeaderIndicator();

// Overrides
public:
	virtual void OnInitialUpdate() override;
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;

protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg) override;
	virtual BOOL OnChildNotify(UINT, WPARAM, LPARAM, LRESULT*) override;

	int m_nThisPane;                     /**< 0=left, 1=right */
	CDirSideBySideCoordinator *m_pCoordinator;

	CSortHeaderCtrl m_ctlSortHeader;
	CImageList m_imageList;
	CListCtrl *m_pList;
	std::unique_ptr<DirViewColItems> m_pColItems;
	std::vector<ListViewOwnerDataItem> m_listViewItems;
	DIRCOLORSETTINGS m_cachedColors;
	bool m_bUseColors;
	CFont m_font;
	String m_sFindPattern;          /**< Last find filename pattern */
	bool m_bRowStripes;             /**< Alternating row stripe mode */

	/** Configurable key bindings: command ID -> (vkKey, modifiers) */
	struct KeyBinding { UINT vkKey; bool bCtrl; bool bShift; bool bAlt; };
	std::map<UINT, KeyBinding> m_keyBindings;
	void LoadKeyBindings();
	void SaveKeyBindings();

	void ReflectGetdispinfo(NMLVDISPINFO *pParam);
	void GetColors(int nRow, int nCol, COLORREF& clrBk, COLORREF& clrText) const;
	int GetPaneColImage(const DIFFITEM &di) const;

	void OpenSelectedItem();
	void OpenCrossComparison();

	// Tree mode expand/collapse
	void ExpandSubdir(int sel);
	void CollapseSubdir(int sel);
	void OnExpandAllSubdirs();
	void OnCollapseAllSubdirs();
	void ToggleExpandSubdir(int sel);

	// Message map
	afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnUpdateUIMessage(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSxsSwapSides();
	afx_msg void OnSxsCopy();
	afx_msg void OnSxsMove();
	afx_msg void OnSxsOpenCompare();
	afx_msg void OnSxsCrossCompare();
	afx_msg void OnUpdateSxsNeedSelection(CCmdUI* pCmdUI);
	afx_msg void OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnScroll(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnSxsToggleTree();
	afx_msg void OnSxsExpandAll();
	afx_msg void OnSxsCollapseAll();
	afx_msg void OnSxsFlattenMode();
	afx_msg void OnUpdateSxsToggleTree(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSxsFlattenMode(CCmdUI* pCmdUI);
	afx_msg void OnSxsRefresh();
	afx_msg void OnSxsRename();
	afx_msg void OnSxsFindFilename();
	afx_msg void OnEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSxsSelectAll();
	afx_msg void OnSxsSelectNewer();
	afx_msg void OnSxsSelectOrphans();
	afx_msg void OnSxsSelectDifferent();
	afx_msg void OnSxsInvertSelection();
	afx_msg void OnSxsNextDiff();
	afx_msg void OnSxsPrevDiff();
	afx_msg void OnSxsDelete();
	afx_msg void OnSxsUpdateLeft();
	afx_msg void OnSxsUpdateRight();
	afx_msg void OnSxsUpdateBoth();
	afx_msg void OnSxsMirrorLeft();
	afx_msg void OnSxsMirrorRight();
	afx_msg void OnSxsCompareContents();
	afx_msg void OnSxsCrcCompare();
	afx_msg void OnSxsTouchTimestamps();
	afx_msg void OnSxsShowLog();
	afx_msg void OnSxsGenerateReport();
	afx_msg void OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);

	// Navigation
	afx_msg void OnSxsNavBack();
	afx_msg void OnSxsNavForward();
	afx_msg void OnUpdateSxsNavBack(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSxsNavForward(CCmdUI* pCmdUI);
	afx_msg void OnSxsUpLevel();
	afx_msg void OnSxsSetBase();
	afx_msg void OnSxsSetBaseOther();
	afx_msg void OnSxsFindNext();
	afx_msg void OnSxsFindPrev();

	// File operations
	afx_msg void OnSxsCopyToFolder();
	afx_msg void OnSxsMoveToFolder();
	afx_msg void OnSxsNewFolder();
	afx_msg void OnSxsDeletePermanent();
	afx_msg void OnSxsExchange();
	afx_msg void OnSxsChangeAttributes();

	// Touch with options
	afx_msg void OnSxsTouchNow();
	afx_msg void OnSxsTouchSpecific();
	afx_msg void OnSxsTouchFromOther();

	// Advanced filter
	afx_msg void OnSxsAdvancedFilter();

	// Display modes
	afx_msg void OnSxsIgnoreStructure();
	afx_msg void OnUpdateSxsIgnoreStructure(CCmdUI* pCmdUI);
	afx_msg void OnSxsRowStripes();
	afx_msg void OnUpdateSxsRowStripes(CCmdUI* pCmdUI);

	// Exclude / clipboard / info
	afx_msg void OnSxsExcludePattern();
	afx_msg void OnSxsCompareInfo();
	afx_msg void OnSxsCopyPath();
	afx_msg void OnSxsCopyFilename();

	// Open with
	afx_msg void OnSxsOpenWithApp();
	afx_msg void OnSxsOpenWith();

	// Explorer context menu
	afx_msg void OnSxsExplorerMenu();

	// Side-specific selection
	afx_msg void OnSxsSelectLeftOnly();
	afx_msg void OnSxsSelectRightOnly();

	// Auto-expand
	afx_msg void OnSxsAutoExpandAll();
	afx_msg void OnSxsAutoExpandDiff();
	afx_msg void OnUpdateSxsAutoExpandAll(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSxsAutoExpandDiff(CCmdUI* pCmdUI);

	// Alignment
	afx_msg void OnSxsAlignWith();

	// Configurable keys
	afx_msg void OnSxsCustomizeKeys();

	// Report generation helper
	void GenerateHTMLReport(const String& filePath);
	void GenerateCSVReport(const String& filePath);

	// File attributes helper
	String GetItemAttributeString(const DIFFITEM& di) const;

	// Find helper
	bool FindFilename(const String& pattern, bool bForward, int startRow);

	// Explorer context menu helper
	void ShowExplorerContextMenu(const String& filePath, CPoint pt);

	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG
inline CDirDoc* CDirPaneView::GetDocument()
{ return reinterpret_cast<CDirDoc*>(m_pDocument); }
#endif
