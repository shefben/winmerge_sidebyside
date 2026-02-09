/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSideBySideCoordinator.h
 *
 * @brief Declaration of CDirSideBySideCoordinator class
 */
#pragma once

#include <vector>
#include <map>
#include <memory>
#include "UnicodeString.h"
#include "DirActions.h"
#include "OptionsDirColors.h"

class DirViewColItems;

class CDiffContext;
class CDirDoc;
class CDirPaneView;
class DIFFITEM;

/** Folder content status for side-by-side icons */
enum FolderContentStatus
{
	FOLDER_STATUS_UNKNOWN = 0,
	FOLDER_STATUS_ALL_SAME,         /**< All children identical */
	FOLDER_STATUS_ALL_DIFFERENT,    /**< All children differ */
	FOLDER_STATUS_UNIQUE_ONLY,      /**< Only unique items (no matches) */
	FOLDER_STATUS_MIXED,            /**< Mix of same, different, and/or unique */
};

/**
 * @brief Row mapping entry for side-by-side view.
 * Maps a visual row to a DIFFITEM and tracks whether each pane
 * has a real item or a placeholder at this row.
 */
struct SideBySideRowItem
{
	DIFFITEM *diffpos;         /**< Pointer to DIFFITEM in CDiffContext */
	bool existsOnLeft;         /**< true if item exists on left side */
	bool existsOnRight;        /**< true if item exists on right side */
	int indent;                /**< Indent level for tree mode */
};

/**
 * @brief Coordinates two CDirPaneView instances for side-by-side folder comparison.
 *
 * This class mediates between the two pane views and the shared CDiffContext.
 * It builds a synchronized row mapping so both panes always have the same
 * number of rows, with placeholder blank rows where items are missing on one side.
 */
class CDirSideBySideCoordinator
{
public:
	CDirSideBySideCoordinator(CDirDoc *pDoc);
	~CDirSideBySideCoordinator();

	void SetPaneViews(CDirPaneView *pLeftPane, CDirPaneView *pRightPane);

	/** Rebuild the row mapping from the CDiffContext and update both panes */
	void Redisplay();

	/** Walk the diff tree and build synchronized row indices */
	void BuildRowMapping();

	/** Swap left and right sides */
	void SwapSides();

	/** Get the row mapping */
	const std::vector<SideBySideRowItem>& GetRowMapping() const { return m_rowMapping; }

	/** Get number of synchronized rows */
	int GetRowCount() const { return static_cast<int>(m_rowMapping.size()); }

	/** Get the DIFFITEM for a given row */
	DIFFITEM* GetDiffItemAt(int row) const;

	/** Check if an item exists on the given pane (0=left, 1=right) */
	bool ItemExistsOnPane(int row, int pane) const;

	/** Get pane views */
	CDirPaneView* GetLeftPaneView() const { return m_pLeftPane; }
	CDirPaneView* GetRightPaneView() const { return m_pRightPane; }

	/** Get active pane index (0=left, 1=right) */
	int GetActivePane() const { return m_nActivePane; }
	void SetActivePane(int pane) { m_nActivePane = pane; }

	/** Compute folder content status for icon determination */
	FolderContentStatus ComputeFolderContentStatus(const DIFFITEM &di) const;

	/** Get pane-specific icon image index */
	int GetPaneColImage(const DIFFITEM &di, int pane) const;

	/** Get selected items from a pane */
	void GetSelectedItems(int pane, std::vector<DIFFITEM*>& items) const;

	/** Status counts for the status bar */
	struct StatusCounts
	{
		int nTotal;
		int nIdentical;
		int nDifferent;
		int nOrphanLeft;
		int nOrphanRight;
		int nNewerLeft;
		int nNewerRight;
		int nSkipped;
	};

	/** Get the current status counts */
	const StatusCounts& GetStatusCounts() const { return m_statusCounts; }

	/** Update status counts from the current row mapping */
	void UpdateStatusCounts();

	/** Format status string for the status bar */
	String FormatStatusString() const;

	/** Format a detail string for the selected item (filename, size, date) */
	String FormatSelectionDetailString(int nSelectedRow) const;

	/** Sort column management */
	void SetSortColumn(int nCol, bool bAscending);
	int GetSortColumn() const { return m_nSortColumn; }
	bool GetSortAscending() const { return m_bSortAscending; }

	/** Select a row in both panes */
	void SelectRowInBothPanes(int row);

	/** Bulk sync operations */
	void UpdateLeft();   // Copy newer + orphans from right to left
	void UpdateRight();  // Copy newer + orphans from left to right
	void UpdateBoth();   // Copy newer + orphans both ways
	void MirrorLeft();   // Make left match right (copy + delete)
	void MirrorRight();  // Make right match left (copy + delete)

	/** Name filter pattern (wildcard: *, ?) */
	void SetNameFilter(const String& filter);
	const String& GetNameFilter() const { return m_sNameFilter; }

	/** Get file attributes as a string (e.g. "RHSA") */
	static String GetFileAttributeString(const String& filePath);

	/** Log panel support */
	void LogOperation(const String& msg);
	const std::vector<String>& GetLogMessages() const { return m_logMessages; }
	void ClearLog() { m_logMessages.clear(); }

	/** CRC32 comparison for selected files */
	static DWORD ComputeCRC32(const String& filePath);

	/** Touch timestamps — copy modification time from source to destination */
	static bool TouchFileTimestamp(const String& srcPath, const String& dstPath);

	/** Touch timestamps — set to current time */
	static bool TouchToNow(const String& filePath);

	/** Touch timestamps — set to specific FILETIME */
	static bool TouchToSpecificTime(const String& filePath, const FILETIME& ft);

	/** Navigation history */
	void PushHistory(const String& leftPath, const String& rightPath);
	bool NavigateBack(String& leftPath, String& rightPath);
	bool NavigateForward(String& leftPath, String& rightPath);
	bool CanNavigateBack() const { return !m_historyBack.empty(); }
	bool CanNavigateForward() const { return !m_historyForward.empty(); }

	/** Navigate up one level */
	bool GetParentPaths(String& leftParent, String& rightParent) const;

	/** Set base folder (re-root comparison to subfolder) */
	bool SetBaseFolder(int pane, const String& subfolderPath);
	bool SetBaseFolderOtherSide(int pane, const String& subfolderPath);

	/** Exchange files between sides */
	void ExchangeFiles(const std::vector<DIFFITEM*>& items);

	/** Advanced filter settings */
	struct AdvancedFilter
	{
		String dateFrom;    /**< YYYY-MM-DD format or empty */
		String dateTo;      /**< YYYY-MM-DD format or empty */
		int sizeMin;        /**< Minimum size in bytes, -1 = disabled */
		int sizeMax;        /**< Maximum size in bytes, -1 = disabled */
		String attrMask;    /**< Attribute mask: R, H, S, A (include only) */
	};
	void SetAdvancedFilter(const AdvancedFilter& filter);
	const AdvancedFilter& GetAdvancedFilter() const { return m_advFilter; }
	bool PassesAdvancedFilter(const DIFFITEM& di) const;

	/** Ignore folder structure mode — align files by name only */
	void SetIgnoreFolderStructure(bool bIgnore);
	bool GetIgnoreFolderStructure() const { return m_bIgnoreFolderStructure; }

	/** Get file version string for PE files */
	static String GetFileVersionString(const String& filePath);

	/** Get file owner string (NTFS) */
	static String GetFileOwnerString(const String& filePath);

	/** Alignment overrides — force two items to align */
	void AddAlignmentOverride(DIFFITEM* leftItem, DIFFITEM* rightItem);
	void ClearAlignmentOverrides();
	const std::map<DIFFITEM*, DIFFITEM*>& GetAlignmentOverrides() const { return m_alignmentOverrides; }

	/** Auto-expand on load: 0=none, 1=all, 2=diff-only */
	void ApplyAutoExpand();

	/** Background scanning state */
	void SetScanningInProgress(bool bScanning) { m_bScanningInProgress = bScanning; }
	bool IsScanningInProgress() const { return m_bScanningInProgress; }

	/** Format comparison info string for info dialog */
	String FormatCompareInfoString() const;

private:
	void BuildRowMappingChildren(DIFFITEM *diffpos, int level);
	void BuildRowMappingIgnoreStructure();
	void SortRowMapping();

	CDirDoc *m_pDoc;
	CDirPaneView *m_pLeftPane;
	CDirPaneView *m_pRightPane;
	std::vector<SideBySideRowItem> m_rowMapping;
	int m_nActivePane;
	std::unique_ptr<DirViewFilterSettings> m_pDirFilter;
	StatusCounts m_statusCounts;
	int m_nSortColumn;                /**< Logical sort column (-1 = none) */
	bool m_bSortAscending;            /**< Sort direction */
	String m_sNameFilter;             /**< Wildcard name filter pattern */
	std::vector<String> m_logMessages; /**< Operation log messages */

	/** Navigation history */
	struct HistoryEntry { String leftPath; String rightPath; };
	std::vector<HistoryEntry> m_historyBack;
	std::vector<HistoryEntry> m_historyForward;

	/** Advanced filter */
	AdvancedFilter m_advFilter;

	/** Ignore folder structure mode */
	bool m_bIgnoreFolderStructure;

	/** Alignment overrides */
	std::map<DIFFITEM*, DIFFITEM*> m_alignmentOverrides;

	/** Background scanning state */
	bool m_bScanningInProgress;
};
