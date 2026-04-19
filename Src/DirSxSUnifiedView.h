/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSxSUnifiedView.h
 *
 * @brief Declaration of CDirSxSUnifiedView class — single unified table
 *        for side-by-side folder comparison (Beyond Compare-style).
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
 * @brief Unified list view for side-by-side folder comparison.
 *
 * Replaces the two CDirPaneView + CDirGutterView splitter layout with a
 * single CListView containing 7 columns:
 *   Name (L) | Size (L) | Modified (L) | Cmp | Name (R) | Size (R) | Modified (R)
 *
 * The center "Cmp" column (~30px) shows a comparison indicator:
 *   "=" for identical, a not-equal sign for different, blank for folders/orphans.
 *
 * Uses LVS_OWNERDATA virtual list mode with per-subitem custom draw coloring.
 */
class CDirSxSUnifiedView : public CListView
{
	DECLARE_DYNCREATE(CDirSxSUnifiedView)
protected:
	CDirSxSUnifiedView();

public:
	virtual ~CDirSxSUnifiedView();

	CDirDoc* GetDocument();
	const CDirDoc* GetDocument() const { return const_cast<CDirSxSUnifiedView*>(this)->GetDocument(); }
	CDirFrame* GetParentFrame();

	/** Set the coordinator that manages row mapping */
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

	/** Get the DirViewColItems (used by coordinator for sort) */
	DirViewColItems* GetColItems() const { return m_pColItems.get(); }

	/** Update the sort header arrow indicator */
	void UpdateSortHeaderIndicator();

	/** Column indices */
	enum UnifiedCol {
		COL_LEFT_NAME = 0,
		COL_LEFT_SIZE,
		COL_LEFT_MODIFIED,
		COL_CMP,           // comparison indicator
		COL_RIGHT_NAME,
		COL_RIGHT_SIZE,
		COL_RIGHT_MODIFIED,
		COL_COUNT
	};

	/** Determine which "side" (0=left, 1=right, -1=center) a column belongs to */
	static int GetColumnSide(int col);

	/** Get pane index (always 0 for unified view) */
	int GetPaneIndex() const { return 0; }

	/** Get which side (0=left, 1=right) is currently active for selection */
	int GetActiveSide() const { return m_nActiveSide; }

// Overrides
public:
	virtual void OnInitialUpdate() override;
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;

protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg) override;
	virtual BOOL OnChildNotify(UINT, WPARAM, LPARAM, LRESULT*) override;

	CDirSideBySideCoordinator *m_pCoordinator;

	CSortHeaderCtrl m_ctlSortHeader;
	CImageList m_imageList;
	CListCtrl *m_pList;
	std::unique_ptr<DirViewColItems> m_pColItems;
	std::vector<ListViewOwnerDataItem> m_listViewItems;
	DIRCOLORSETTINGS m_cachedColors;
	bool m_bUseColors;
	CFont m_font;
	CFont m_boldFont;               /**< Bold font for directory names */
	String m_sFindPattern;          /**< Last find filename pattern */
	bool m_bRowStripes;             /**< Alternating row stripe mode */
	bool m_bResizing;               /**< Guard against recursive OnSize */
	int m_nCachedToleranceSecs;     /**< Cached tolerance for draw pass */
	int m_nContextSide;             /**< 0=left, 1=right — set on right-click */
	int m_nActiveSide;              /**< 0=left, 1=right — set on left-click for split selection */

	static const UINT_PTR TIMER_REDISPLAY = 100;

public:
	struct KeyBinding { UINT vkKey; bool bCtrl; bool bShift; bool bAlt; };
protected:
	std::map<UINT, KeyBinding> m_keyBindings;
	void LoadKeyBindings();
	void SaveKeyBindings();

	void ReflectGetdispinfo(NMLVDISPINFO *pParam);
	void GetColors(int nRow, int nCol, COLORREF& clrBk, COLORREF& clrText) const;
	int GetItemIcon(int nRow) const;
	int GetItemIconForSide(int nRow, int side) const;
	String GetComparisonSymbol(int nRow) const;
	String GetCellText(int nRow, int nCol) const;

	void OpenSelectedItem();
	void OpenCrossComparison();

	// Tree mode expand/collapse
	void CompareExpandedChildren(DIFFITEM &parentDi);
	void ExpandSubdir(int sel);
	void CollapseSubdir(int sel);
	void OnExpandAllSubdirs();
	void OnCollapseAllSubdirs();
	void ToggleExpandSubdir(int sel);

	// Message map
	afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnHeaderCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg LRESULT OnUpdateUIMessage(WPARAM wParam, LPARAM lParam);
	afx_msg void OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnClick(NMHDR* pNMHDR, LRESULT* pResult);

	// SxS commands
	afx_msg void OnSxsSwapSides();
	afx_msg void OnSxsCopy();
	afx_msg void OnSxsMove();
	afx_msg void OnSxsOpenCompare();
	afx_msg void OnSxsCrossCompare();
	afx_msg void OnUpdateSxsNeedSelection(CCmdUI* pCmdUI);
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

	void NavigateToPath(const String& sPath);

	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG
inline CDirDoc* CDirSxSUnifiedView::GetDocument()
{ return reinterpret_cast<CDirDoc*>(m_pDocument); }
#endif
