/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSideBySideCoordinator.cpp
 *
 * @brief Implementation of CDirSideBySideCoordinator class
 */

#include "StdAfx.h"
#include "DirSideBySideCoordinator.h"
#include "DirPaneView.h"
#include "DirDoc.h"
#include "DirFrame.h"
#include "DiffContext.h"
#include "DiffItem.h"
#include "DirActions.h"
#include "DirViewColItems.h"
#include "DiffThread.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "ShellFileOperations.h"
#include "paths.h"
#include <Shlwapi.h>
#include <Poco/DateTime.h>
#include <algorithm>
#include <unordered_map>
#include <Aclapi.h>

#pragma comment(lib, "version.lib")
#pragma comment(lib, "Advapi32.lib")

CDirSideBySideCoordinator::CDirSideBySideCoordinator(CDirDoc *pDoc)
	: m_pDoc(pDoc)
	, m_pLeftPane(nullptr)
	, m_pRightPane(nullptr)
	, m_nActivePane(0)
	, m_statusCounts{}
	, m_nSortColumn(-1)
	, m_bSortAscending(true)
	, m_bAlwaysShowFolders(true)
	, m_bIgnoreFolderStructure(false)
	, m_bAutoExpandApplied(false)
	, m_bScanningInProgress(false)
	, m_advFilter{ _T(""), _T(""), -1, -1, _T("") }
{
}

CDirSideBySideCoordinator::~CDirSideBySideCoordinator()
{
}

void CDirSideBySideCoordinator::SetPaneViews(CDirPaneView *pLeftPane, CDirPaneView *pRightPane)
{
	m_pLeftPane = pLeftPane;
	m_pRightPane = pRightPane;
}

/**
 * @brief Get the DIFFITEM for a given row index
 */
DIFFITEM* CDirSideBySideCoordinator::GetDiffItemAt(int row) const
{
	if (row < 0 || row >= static_cast<int>(m_rowMapping.size()))
		return nullptr;
	return m_rowMapping[row].diffpos;
}

/**
 * @brief Check if an item exists on the given pane
 */
bool CDirSideBySideCoordinator::ItemExistsOnPane(int row, int pane) const
{
	if (row < 0 || row >= static_cast<int>(m_rowMapping.size()))
		return false;
	return pane == 0 ? m_rowMapping[row].existsOnLeft : m_rowMapping[row].existsOnRight;
}

/**
 * @brief Walk the diff context tree and build the synchronized row mapping.
 *
 * Items that exist on both sides get the same row.
 * Items unique to one side get a placeholder on the other side.
 * This ensures both panes always have the same row count.
 */
void CDirSideBySideCoordinator::BuildRowMapping()
{
	m_rowMapping.clear();

	if (!m_pDoc || !m_pDoc->HasDiffs())
		return;

	const CDiffContext &ctxt = m_pDoc->GetDiffContext();

	// Refresh filter settings
	m_pDirFilter.reset(new DirViewFilterSettings([](const String& name) { return GetOptionsMgr()->GetBool(name); }));

	if (m_bIgnoreFolderStructure)
	{
		BuildRowMappingIgnoreStructure();
	}
	else
	{
		DIFFITEM *diffpos = ctxt.GetFirstDiffPosition();
		BuildRowMappingChildren(diffpos, 0);
	}

	// Apply sort if a sort column is set
	SortRowMapping();
}

/**
 * @brief Recursively build row mapping for children.
 * In flatten mode, recurse into all subdirectories regardless of EXPANDED flag,
 * set indent=0, and skip directory entries (show only leaf files).
 */
void CDirSideBySideCoordinator::BuildRowMappingChildren(DIFFITEM *diffpos, int level)
{
	const CDiffContext &ctxt = m_pDoc->GetDiffContext();
	bool bFlattenMode = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_FLATTEN_MODE);
	bool bSuppressFilters = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_SUPPRESS_FILTERS);
	bool bHasNameFilter = !m_sNameFilter.empty();
	// Include/exclude filter patterns from session settings
	String sIncludeFiles = GetOptionsMgr()->GetString(OPT_DIRVIEW_SXS_INCLUDE_FILES);
	String sExcludeFiles = GetOptionsMgr()->GetString(OPT_DIRVIEW_SXS_EXCLUDE_FILES);
	String sIncludeFolders = GetOptionsMgr()->GetString(OPT_DIRVIEW_SXS_INCLUDE_FOLDERS);
	String sExcludeFolders = GetOptionsMgr()->GetString(OPT_DIRVIEW_SXS_EXCLUDE_FOLDERS);
	bool bHasIncExcFilter = !sExcludeFiles.empty() || !sExcludeFolders.empty()
		|| (sIncludeFiles != _T("*.*") && !sIncludeFiles.empty())
		|| (sIncludeFolders != _T("*") && !sIncludeFolders.empty());

	while (diffpos != nullptr)
	{
		DIFFITEM *curdiffpos = diffpos;
		const DIFFITEM &di = ctxt.GetNextSiblingDiffPosition(diffpos);

		// In suppress-filters mode, show everything; otherwise use normal filter
		if (!bSuppressFilters && !IsShowable(ctxt, di, *m_pDirFilter))
			continue;

		// Apply name filter (wildcard matching) — skip non-matching files
		// Directories are always shown so their children can be traversed
		if (bHasNameFilter && !di.diffcode.isDirectory())
		{
			// Get the filename from whichever side exists
			String filename;
			for (int s = 0; s < ctxt.GetCompareDirs(); s++)
			{
				if (di.diffcode.exists(s))
				{
					filename = di.diffFileInfo[s].filename;
					break;
				}
			}
			if (!filename.empty() && !PathMatchSpec(filename.c_str(), m_sNameFilter.c_str()))
				continue;
		}

		// Apply advanced filter — skip non-matching files (directories always pass)
		if (!di.diffcode.isDirectory() && !PassesAdvancedFilter(di))
			continue;

		// Apply include/exclude patterns from Name Filters tab
		if (!bSuppressFilters && bHasIncExcFilter)
		{
			String filename;
			for (int s = 0; s < ctxt.GetCompareDirs(); s++)
			{
				if (di.diffcode.exists(s))
				{
					filename = di.diffFileInfo[s].filename;
					break;
				}
			}
			if (!filename.empty())
			{
				if (di.diffcode.isDirectory())
				{
					// Check folder include/exclude
					if (!sIncludeFolders.empty() && sIncludeFolders != _T("*"))
					{
						if (!PathMatchSpec(filename.c_str(), sIncludeFolders.c_str()))
							continue;
					}
					if (!sExcludeFolders.empty())
					{
						if (PathMatchSpec(filename.c_str(), sExcludeFolders.c_str()))
							continue;
					}
				}
				else
				{
					// Check file include/exclude
					if (!sIncludeFiles.empty() && sIncludeFiles != _T("*.*"))
					{
						if (!PathMatchSpec(filename.c_str(), sIncludeFiles.c_str()))
							continue;
					}
					if (!sExcludeFiles.empty())
					{
						if (PathMatchSpec(filename.c_str(), sExcludeFiles.c_str()))
							continue;
					}
				}
			}
		}

		if (bFlattenMode)
		{
			// In flatten mode: skip directories, recurse into all children
			if (di.diffcode.isDirectory())
			{
				if (di.HasChildren())
					BuildRowMappingChildren(ctxt.GetFirstChildDiffPosition(curdiffpos), 0);
				continue;
			}

			// Show file with indent=0
			SideBySideRowItem rowItem;
			rowItem.diffpos = curdiffpos;
			rowItem.existsOnLeft = di.diffcode.exists(0);
			rowItem.existsOnRight = di.diffcode.exists(ctxt.GetCompareDirs() - 1);
			rowItem.indent = 0;
			m_rowMapping.push_back(rowItem);
		}
		else
		{
			// Normal mode
			// Show directories that exist on all sides (they are navigable tree nodes)
			// Only skip if "Always Show Folders" is off AND we're not in recursive mode
			// AND the directory has no children to display
			if (!ctxt.m_bRecursive && di.diffcode.isDirectory() && di.diffcode.existAll()
				&& !m_bAlwaysShowFolders && !di.HasChildren())
				continue;

			SideBySideRowItem rowItem;
			rowItem.diffpos = curdiffpos;
			rowItem.existsOnLeft = di.diffcode.exists(0);
			rowItem.existsOnRight = di.diffcode.exists(ctxt.GetCompareDirs() - 1);
			rowItem.indent = level;

			m_rowMapping.push_back(rowItem);

			// In tree mode, recurse into children if expanded
			if (di.HasChildren() && (di.customFlags & ViewCustomFlags::EXPANDED))
			{
				BuildRowMappingChildren(ctxt.GetFirstChildDiffPosition(curdiffpos), level + 1);
			}
		}
	}
}

/**
 * @brief Rebuild the display in both panes from the current diff context.
 */
void CDirSideBySideCoordinator::Redisplay()
{
	InvalidateFolderStatusCache();
	BuildRowMapping();
	UpdateStatusCounts();

	if (m_pLeftPane)
		m_pLeftPane->UpdateFromRowMapping();
	if (m_pRightPane)
		m_pRightPane->UpdateFromRowMapping();

	// Update status bar with counts
	if (m_pLeftPane)
	{
		CDirFrame *pFrame = m_pLeftPane->GetParentFrame();
		if (pFrame)
			pFrame->SetStatus(FormatStatusString().c_str());
	}
}

/**
 * @brief Swap left and right sides by delegating to CDirDoc::Swap
 */
void CDirSideBySideCoordinator::SwapSides()
{
	if (m_pDoc)
	{
		m_pDoc->Swap(0, m_pDoc->m_nDirs - 1);
		Redisplay();
	}
}

/**
 * @brief Compute the content status of a folder item.
 * Recursively scans children to determine if they are all same,
 * all different, unique-only, or mixed.
 */
FolderContentStatus CDirSideBySideCoordinator::ComputeFolderContentStatus(const DIFFITEM &di) const
{
	if (!di.HasChildren() || !m_pDoc || !m_pDoc->HasDiffs())
		return FOLDER_STATUS_UNKNOWN;

	// Check cache first — avoids expensive recursive tree walks on every draw
	auto it = m_folderStatusCache.find(&di);
	if (it != m_folderStatusCache.end())
		return it->second;

	const CDiffContext &ctxt = m_pDoc->GetDiffContext();

	bool hasSame = false;
	bool hasDiff = false;
	bool hasUnique = false;

	DIFFITEM *childpos = ctxt.GetFirstChildDiffPosition(const_cast<DIFFITEM*>(&di));
	while (childpos != nullptr)
	{
		const DIFFITEM &child = ctxt.GetNextSiblingDiffPosition(childpos);

		if (child.diffcode.isResultFiltered())
			continue;

		if (!IsItemExistAll(ctxt, child))
			hasUnique = true;
		else if (child.diffcode.isResultSame())
			hasSame = true;
		else if (child.diffcode.isResultDiff())
			hasDiff = true;

		// Recurse into subfolders
		if (child.diffcode.isDirectory() && child.HasChildren())
		{
			FolderContentStatus childStatus = ComputeFolderContentStatus(child);
			switch (childStatus)
			{
			case FOLDER_STATUS_ALL_SAME:
				hasSame = true;
				break;
			case FOLDER_STATUS_ALL_DIFFERENT:
				hasDiff = true;
				break;
			case FOLDER_STATUS_UNIQUE_ONLY:
				hasUnique = true;
				break;
			case FOLDER_STATUS_MIXED:
				hasSame = true;
				hasDiff = true;
				hasUnique = true;
				break;
			default:
				break;
			}
		}
	}

	int flags = (hasSame ? 1 : 0) | (hasDiff ? 2 : 0) | (hasUnique ? 4 : 0);
	FolderContentStatus result;
	switch (flags)
	{
	case 0: result = FOLDER_STATUS_UNKNOWN; break;
	case 1: result = FOLDER_STATUS_ALL_SAME; break;
	case 2: result = FOLDER_STATUS_ALL_DIFFERENT; break;
	case 4: result = FOLDER_STATUS_UNIQUE_ONLY; break;
	default: result = FOLDER_STATUS_MIXED; break;
	}

	m_folderStatusCache[&di] = result;
	return result;
}

/**
 * @brief Get pane-specific icon image index for a diff item.
 * Uses the same icon set as the unified CDirView but applies
 * folder content status awareness.
 */
int CDirSideBySideCoordinator::GetPaneColImage(const DIFFITEM &di, int pane) const
{
	// For non-directory items, delegate to the standard function
	if (!di.diffcode.isDirectory())
		return GetColImage(di);

	// For directories, use content status to choose icon
	if (di.diffcode.isResultError())
		return DIFFIMG_ERROR;
	if (di.diffcode.isResultAbort())
		return DIFFIMG_ABORT;
	if (di.diffcode.isResultFiltered())
		return DIFFIMG_DIRSKIP;

	// Unique directory: show folder icon only on the side that has it
	if (!IsItemExistAll(m_pDoc->GetDiffContext(), di))
	{
		if (di.diffcode.isSideFirstOnly())
			return DIFFIMG_LDIRUNIQUE;
		else
			return DIFFIMG_RDIRUNIQUE;
	}

	// Directory present on both sides - check content status
	FolderContentStatus status = ComputeFolderContentStatus(di);
	switch (status)
	{
	case FOLDER_STATUS_ALL_SAME:
		return DIFFIMG_DIRSAME;
	case FOLDER_STATUS_ALL_DIFFERENT:
		return DIFFIMG_DIRDIFF;
	case FOLDER_STATUS_UNIQUE_ONLY:
	case FOLDER_STATUS_MIXED:
		return DIFFIMG_DIRDIFF;
	default:
		return DIFFIMG_DIR;
	}
}

/**
 * @brief Get selected DIFFITEM pointers from the given pane.
 */
void CDirSideBySideCoordinator::GetSelectedItems(int pane, std::vector<DIFFITEM*>& items) const
{
	items.clear();
	CDirPaneView *pView = (pane == 0) ? m_pLeftPane : m_pRightPane;
	if (!pView)
		return;

	CListCtrl &list = pView->GetListCtrl();
	int nItem = -1;
	while ((nItem = list.GetNextItem(nItem, LVNI_SELECTED)) != -1)
	{
		DIFFITEM *key = pView->GetItemKey(nItem);
		if (key != nullptr)
			items.push_back(key);
	}
}

/**
 * @brief Update status counts from the current row mapping.
 * Iterates all visible rows and counts items by category.
 */
void CDirSideBySideCoordinator::UpdateStatusCounts()
{
	m_statusCounts = {};

	if (!m_pDoc || !m_pDoc->HasDiffs())
		return;

	const CDiffContext &ctxt = m_pDoc->GetDiffContext();
	int toleranceSecs = GetOptionsMgr()->GetInt(OPT_CMP_IGNORE_SMALL_FILETIME_SECS);

	for (const auto &row : m_rowMapping)
	{
		if (!row.diffpos)
			continue;

		const DIFFITEM &di = ctxt.GetDiffAt(row.diffpos);
		if (di.isEmpty() || di.diffcode.isDirectory())
			continue;

		m_statusCounts.nTotal++;

		if (di.diffcode.isResultFiltered())
		{
			m_statusCounts.nSkipped++;
		}
		else if (!IsItemExistAll(ctxt, di))
		{
			if (di.diffcode.exists(0) && !di.diffcode.exists(ctxt.GetCompareDirs() - 1))
				m_statusCounts.nOrphanLeft++;
			else
				m_statusCounts.nOrphanRight++;
		}
		else if (di.diffcode.isResultSame())
		{
			m_statusCounts.nIdentical++;
		}
		else if (di.diffcode.isResultDiff())
		{
			m_statusCounts.nDifferent++;

			// Check for newer left/right
			const auto &leftTime = di.diffFileInfo[0].mtime;
			const auto &rightTime = di.diffFileInfo[ctxt.GetCompareDirs() - 1].mtime;
			Poco::Timestamp::TimeDiff diff = leftTime - rightTime;
			Poco::Timestamp::TimeDiff toleranceUs = static_cast<Poco::Timestamp::TimeDiff>(toleranceSecs) * Poco::Timestamp::resolution();

			if (diff > toleranceUs)
				m_statusCounts.nNewerLeft++;
			else if (diff < -toleranceUs)
				m_statusCounts.nNewerRight++;
		}
	}
}

/**
 * @brief Format a status string for the status bar.
 */
String CDirSideBySideCoordinator::FormatStatusString() const
{
	return strutils::format(
		_T("Total: %d | Identical: %d | Different: %d | Orphan L: %d | Orphan R: %d | Newer L: %d | Newer R: %d"),
		m_statusCounts.nTotal,
		m_statusCounts.nIdentical,
		m_statusCounts.nDifferent,
		m_statusCounts.nOrphanLeft,
		m_statusCounts.nOrphanRight,
		m_statusCounts.nNewerLeft,
		m_statusCounts.nNewerRight);
}

/**
 * @brief Sort the row mapping using the current sort column and direction.
 * Uses DirViewColItems::ColSort from the left pane's column items to
 * compare DIFFITEM entries. Does not sort while a comparison is in progress.
 */
void CDirSideBySideCoordinator::SortRowMapping()
{
	if (m_nSortColumn < 0 || m_rowMapping.empty())
		return;

	if (!m_pDoc || !m_pDoc->HasDiffs())
		return;

	// Do not sort while comparing -- results are updated asynchronously
	// and may violate strict weak ordering required by std::sort.
	if (m_pDoc->m_diffThread.GetThreadState() == CDiffThread::THREAD_COMPARING)
		return;

	// We need a DirViewColItems instance for the sort comparison.
	// Use the left pane's column items (both panes should have the same logical columns).
	DirViewColItems *pColItems = nullptr;
	if (m_pLeftPane)
		pColItems = m_pLeftPane->GetColItems();
	if (!pColItems)
		return;

	if (m_nSortColumn >= pColItems->GetColCount())
		return;

	const CDiffContext &ctxt = m_pDoc->GetDiffContext();
	const int sortCol = m_nSortColumn;
	const bool bAscending = m_bSortAscending;

	std::stable_sort(m_rowMapping.begin(), m_rowMapping.end(),
		[&ctxt, pColItems, sortCol, bAscending](const SideBySideRowItem &a, const SideBySideRowItem &b) -> bool
		{
			// Null diffpos items (should not happen) sort last
			if (!a.diffpos)
				return false;
			if (!b.diffpos)
				return true;

			const DIFFITEM &ldi = ctxt.GetDiffAt(a.diffpos);
			const DIFFITEM &rdi = ctxt.GetDiffAt(b.diffpos);

			int retVal = pColItems->ColSort(&ctxt, sortCol, ldi, rdi, false);
			return bAscending ? (retVal < 0) : (retVal > 0);
		});
}

/**
 * @brief Set the sort column and direction, then re-sort and refresh both panes.
 */
void CDirSideBySideCoordinator::SetSortColumn(int nCol, bool bAscending)
{
	m_nSortColumn = nCol;
	m_bSortAscending = bAscending;
	Redisplay();
}

/**
 * @brief Format a detail string for the selected item showing filename, size, and date.
 * Used by the status bar when a single item is selected.
 */
String CDirSideBySideCoordinator::FormatSelectionDetailString(int nSelectedRow) const
{
	if (nSelectedRow < 0 || nSelectedRow >= static_cast<int>(m_rowMapping.size()))
		return String();

	const SideBySideRowItem &row = m_rowMapping[nSelectedRow];
	if (!row.diffpos)
		return String();

	if (!m_pDoc || !m_pDoc->HasDiffs())
		return String();

	const CDiffContext &ctxt = m_pDoc->GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(row.diffpos);

	// Determine which side to show info from (prefer the active pane side)
	int side = m_nActivePane;
	if (side >= ctxt.GetCompareDirs())
		side = ctxt.GetCompareDirs() - 1;
	if (!di.diffcode.exists(side))
	{
		// Fall back to the other side
		side = (side == 0) ? (ctxt.GetCompareDirs() - 1) : 0;
		if (!di.diffcode.exists(side))
			return String();
	}

	String filename = di.diffFileInfo[side].filename;

	// Format size
	String sizeStr;
	if (di.diffFileInfo[side].size != DirItem::FILE_SIZE_NONE && !di.diffcode.isDirectory())
	{
		Poco::File::FileSize fileSize = di.diffFileInfo[side].size;
		if (fileSize < 1024)
			sizeStr = strutils::format(_T("%llu B"), static_cast<unsigned long long>(fileSize));
		else if (fileSize < 1024 * 1024)
			sizeStr = strutils::format(_T("%.1f KB"), static_cast<double>(fileSize) / 1024.0);
		else
			sizeStr = strutils::format(_T("%.1f MB"), static_cast<double>(fileSize) / (1024.0 * 1024.0));
	}

	// Format date
	String dateStr;
	if (di.diffFileInfo[side].mtime != Poco::Timestamp::TIMEVAL_MIN)
	{
		try
		{
			Poco::DateTime dt(di.diffFileInfo[side].mtime);
			dateStr = strutils::format(_T("%04d-%02d-%02d %02d:%02d:%02d"),
				dt.year(), dt.month(), dt.day(),
				dt.hour(), dt.minute(), dt.second());
		}
		catch (...)
		{
			// If timestamp conversion fails, skip the date
		}
	}

	// Build the detail string
	String detail = filename;
	if (!sizeStr.empty())
		detail += _T("  |  ") + sizeStr;
	if (!dateStr.empty())
		detail += _T("  |  ") + dateStr;

	return detail;
}

/**
 * @brief Select a row in both pane list controls.
 * Used by gutter clicks to synchronize selection.
 */
void CDirSideBySideCoordinator::SelectRowInBothPanes(int row)
{
	if (row < 0 || row >= static_cast<int>(m_rowMapping.size()))
		return;

	auto selectRow = [row](CDirPaneView *pPane)
	{
		if (!pPane)
			return;
		CListCtrl &list = pPane->GetListCtrl();
		// Clear existing selection
		int nItem = -1;
		while ((nItem = list.GetNextItem(nItem, LVNI_SELECTED)) != -1)
			list.SetItemState(nItem, 0, LVIS_SELECTED | LVIS_FOCUSED);
		// Select the target row
		list.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		list.EnsureVisible(row, FALSE);
	};

	selectRow(m_pLeftPane);
	selectRow(m_pRightPane);
}

void CDirSideBySideCoordinator::UpdateRight()
{
	if (!m_pDoc || !m_pDoc->HasDiffs())
		return;

	const CDiffContext &ctxt = m_pDoc->GetDiffContext();
	int leftSide = 0;
	int rightSide = ctxt.GetCompareDirs() - 1;
	int toleranceSecs = GetOptionsMgr()->GetInt(OPT_CMP_IGNORE_SMALL_FILETIME_SECS);

	ShellFileOperations fileOps;
	for (const auto &row : m_rowMapping)
	{
		if (!row.diffpos)
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(row.diffpos);
		if (di.diffcode.isDirectory() || di.diffcode.isResultFiltered())
			continue;

		bool shouldCopy = false;
		if (di.diffcode.exists(leftSide) && !di.diffcode.exists(rightSide))
		{
			shouldCopy = true; // Orphan on left — copy to right
		}
		else if (IsItemExistAll(ctxt, di) && di.diffcode.isResultDiff())
		{
			Poco::Timestamp::TimeDiff diff = di.diffFileInfo[leftSide].mtime - di.diffFileInfo[rightSide].mtime;
			Poco::Timestamp::TimeDiff toleranceUs = static_cast<Poco::Timestamp::TimeDiff>(toleranceSecs) * Poco::Timestamp::resolution();
			if (diff > toleranceUs)
				shouldCopy = true; // Left is newer — copy to right
		}

		if (shouldCopy)
		{
			String srcPath = di.getFilepath(leftSide, ctxt.GetPath(leftSide));
			String relPath = di.diffFileInfo[leftSide].path;
			String dstDir = paths::ConcatPath(ctxt.GetPath(rightSide), relPath);
			String dstPath = paths::ConcatPath(dstDir, di.diffFileInfo[leftSide].filename);
			fileOps.AddSourceAndDestination(srcPath, dstPath);
		}
	}

	HWND hParent = m_pLeftPane ? m_pLeftPane->GetSafeHwnd() : nullptr;
	fileOps.SetOperation(FO_COPY, FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR, hParent);
	if (fileOps.Run() && !fileOps.IsCanceled())
		m_pDoc->Rescan();
}

void CDirSideBySideCoordinator::UpdateLeft()
{
	if (!m_pDoc || !m_pDoc->HasDiffs())
		return;

	const CDiffContext &ctxt = m_pDoc->GetDiffContext();
	int leftSide = 0;
	int rightSide = ctxt.GetCompareDirs() - 1;
	int toleranceSecs = GetOptionsMgr()->GetInt(OPT_CMP_IGNORE_SMALL_FILETIME_SECS);

	ShellFileOperations fileOps;
	for (const auto &row : m_rowMapping)
	{
		if (!row.diffpos)
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(row.diffpos);
		if (di.diffcode.isDirectory() || di.diffcode.isResultFiltered())
			continue;

		bool shouldCopy = false;
		if (!di.diffcode.exists(leftSide) && di.diffcode.exists(rightSide))
		{
			shouldCopy = true; // Orphan on right — copy to left
		}
		else if (IsItemExistAll(ctxt, di) && di.diffcode.isResultDiff())
		{
			Poco::Timestamp::TimeDiff diff = di.diffFileInfo[rightSide].mtime - di.diffFileInfo[leftSide].mtime;
			Poco::Timestamp::TimeDiff toleranceUs = static_cast<Poco::Timestamp::TimeDiff>(toleranceSecs) * Poco::Timestamp::resolution();
			if (diff > toleranceUs)
				shouldCopy = true; // Right is newer — copy to left
		}

		if (shouldCopy)
		{
			String srcPath = di.getFilepath(rightSide, ctxt.GetPath(rightSide));
			String relPath = di.diffFileInfo[rightSide].path;
			String dstDir = paths::ConcatPath(ctxt.GetPath(leftSide), relPath);
			String dstPath = paths::ConcatPath(dstDir, di.diffFileInfo[rightSide].filename);
			fileOps.AddSourceAndDestination(srcPath, dstPath);
		}
	}

	HWND hParent = m_pLeftPane ? m_pLeftPane->GetSafeHwnd() : nullptr;
	fileOps.SetOperation(FO_COPY, FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR, hParent);
	if (fileOps.Run() && !fileOps.IsCanceled())
		m_pDoc->Rescan();
}

void CDirSideBySideCoordinator::UpdateBoth()
{
	if (!m_pDoc || !m_pDoc->HasDiffs())
		return;

	const CDiffContext &ctxt = m_pDoc->GetDiffContext();
	int leftSide = 0;
	int rightSide = ctxt.GetCompareDirs() - 1;
	int toleranceSecs = GetOptionsMgr()->GetInt(OPT_CMP_IGNORE_SMALL_FILETIME_SECS);

	ShellFileOperations fileOps;
	for (const auto &row : m_rowMapping)
	{
		if (!row.diffpos)
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(row.diffpos);
		if (di.diffcode.isDirectory() || di.diffcode.isResultFiltered())
			continue;

		int srcSide = -1, dstSide = -1;
		if (di.diffcode.exists(leftSide) && !di.diffcode.exists(rightSide))
		{
			srcSide = leftSide; dstSide = rightSide;
		}
		else if (!di.diffcode.exists(leftSide) && di.diffcode.exists(rightSide))
		{
			srcSide = rightSide; dstSide = leftSide;
		}
		else if (IsItemExistAll(ctxt, di) && di.diffcode.isResultDiff())
		{
			Poco::Timestamp::TimeDiff diff = di.diffFileInfo[leftSide].mtime - di.diffFileInfo[rightSide].mtime;
			Poco::Timestamp::TimeDiff toleranceUs = static_cast<Poco::Timestamp::TimeDiff>(toleranceSecs) * Poco::Timestamp::resolution();
			if (diff > toleranceUs)
			{ srcSide = leftSide; dstSide = rightSide; }
			else if (diff < -toleranceUs)
			{ srcSide = rightSide; dstSide = leftSide; }
		}

		if (srcSide >= 0 && dstSide >= 0)
		{
			String srcPath = di.getFilepath(srcSide, ctxt.GetPath(srcSide));
			String relPath = di.diffFileInfo[srcSide].path;
			String dstDir = paths::ConcatPath(ctxt.GetPath(dstSide), relPath);
			String dstPath = paths::ConcatPath(dstDir, di.diffFileInfo[srcSide].filename);
			fileOps.AddSourceAndDestination(srcPath, dstPath);
		}
	}

	HWND hParent = m_pLeftPane ? m_pLeftPane->GetSafeHwnd() : nullptr;
	fileOps.SetOperation(FO_COPY, FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR, hParent);
	if (fileOps.Run() && !fileOps.IsCanceled())
		m_pDoc->Rescan();
}

void CDirSideBySideCoordinator::MirrorRight()
{
	if (!m_pDoc || !m_pDoc->HasDiffs())
		return;

	const CDiffContext &ctxt = m_pDoc->GetDiffContext();
	int leftSide = 0;
	int rightSide = ctxt.GetCompareDirs() - 1;

	// First copy all left items to right
	ShellFileOperations copyOps;
	ShellFileOperations deleteOps;

	for (const auto &row : m_rowMapping)
	{
		if (!row.diffpos)
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(row.diffpos);
		if (di.diffcode.isDirectory() || di.diffcode.isResultFiltered())
			continue;

		if (di.diffcode.exists(leftSide))
		{
			if (!di.diffcode.exists(rightSide) || di.diffcode.isResultDiff())
			{
				String srcPath = di.getFilepath(leftSide, ctxt.GetPath(leftSide));
				String relPath = di.diffFileInfo[leftSide].path;
				String dstDir = paths::ConcatPath(ctxt.GetPath(rightSide), relPath);
				String dstPath = paths::ConcatPath(dstDir, di.diffFileInfo[leftSide].filename);
				copyOps.AddSourceAndDestination(srcPath, dstPath);
			}
		}
		else if (di.diffcode.exists(rightSide))
		{
			// Orphan on right — delete
			String path = di.getFilepath(rightSide, ctxt.GetPath(rightSide));
			deleteOps.AddSource(path);
		}
	}

	HWND hParent = m_pLeftPane ? m_pLeftPane->GetSafeHwnd() : nullptr;
	bool changed = false;

	copyOps.SetOperation(FO_COPY, FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR, hParent);
	if (copyOps.Run() && !copyOps.IsCanceled())
		changed = true;

	deleteOps.SetOperation(FO_DELETE, FOF_ALLOWUNDO, hParent);
	if (deleteOps.Run() && !deleteOps.IsCanceled())
		changed = true;

	if (changed)
		m_pDoc->Rescan();
}

void CDirSideBySideCoordinator::MirrorLeft()
{
	if (!m_pDoc || !m_pDoc->HasDiffs())
		return;

	const CDiffContext &ctxt = m_pDoc->GetDiffContext();
	int leftSide = 0;
	int rightSide = ctxt.GetCompareDirs() - 1;

	ShellFileOperations copyOps;
	ShellFileOperations deleteOps;

	for (const auto &row : m_rowMapping)
	{
		if (!row.diffpos)
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(row.diffpos);
		if (di.diffcode.isDirectory() || di.diffcode.isResultFiltered())
			continue;

		if (di.diffcode.exists(rightSide))
		{
			if (!di.diffcode.exists(leftSide) || di.diffcode.isResultDiff())
			{
				String srcPath = di.getFilepath(rightSide, ctxt.GetPath(rightSide));
				String relPath = di.diffFileInfo[rightSide].path;
				String dstDir = paths::ConcatPath(ctxt.GetPath(leftSide), relPath);
				String dstPath = paths::ConcatPath(dstDir, di.diffFileInfo[rightSide].filename);
				copyOps.AddSourceAndDestination(srcPath, dstPath);
			}
		}
		else if (di.diffcode.exists(leftSide))
		{
			String path = di.getFilepath(leftSide, ctxt.GetPath(leftSide));
			deleteOps.AddSource(path);
		}
	}

	HWND hParent = m_pLeftPane ? m_pLeftPane->GetSafeHwnd() : nullptr;
	bool changed = false;

	copyOps.SetOperation(FO_COPY, FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR, hParent);
	if (copyOps.Run() && !copyOps.IsCanceled())
		changed = true;

	deleteOps.SetOperation(FO_DELETE, FOF_ALLOWUNDO, hParent);
	if (deleteOps.Run() && !deleteOps.IsCanceled())
		changed = true;

	if (changed)
		m_pDoc->Rescan();
}

/**
 * @brief Set the name filter pattern for wildcard matching.
 * Triggers a redisplay to apply the filter. An empty string clears the filter.
 */
void CDirSideBySideCoordinator::SetNameFilter(const String& filter)
{
	if (m_sNameFilter == filter)
		return;
	m_sNameFilter = filter;
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_NAME_FILTER, filter);
	Redisplay();
}

/**
 * @brief Get file attributes as a compact string.
 * Returns a string like "RHSA" indicating ReadOnly, Hidden, System, Archive flags.
 */
String CDirSideBySideCoordinator::GetFileAttributeString(const String& filePath)
{
	DWORD attrs = GetFileAttributes(filePath.c_str());
	if (attrs == INVALID_FILE_ATTRIBUTES)
		return _T("");

	String result;
	if (attrs & FILE_ATTRIBUTE_READONLY)
		result += _T('R');
	if (attrs & FILE_ATTRIBUTE_HIDDEN)
		result += _T('H');
	if (attrs & FILE_ATTRIBUTE_SYSTEM)
		result += _T('S');
	if (attrs & FILE_ATTRIBUTE_ARCHIVE)
		result += _T('A');
	return result;
}

/**
 * @brief Append a log message with timestamp.
 */
void CDirSideBySideCoordinator::LogOperation(const String& msg)
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	String timestamp = strutils::format(_T("[%02d:%02d:%02d] "), st.wHour, st.wMinute, st.wSecond);
	m_logMessages.push_back(timestamp + msg);
}

/**
 * @brief Compute CRC32 of a file.
 * Uses a standard CRC32 lookup table for efficient computation.
 */
DWORD CDirSideBySideCoordinator::ComputeCRC32(const String& filePath)
{
	// Build CRC32 lookup table (standard polynomial 0xEDB88320)
	static DWORD crc32Table[256] = {};
	static bool tableInit = false;
	if (!tableInit)
	{
		for (DWORD i = 0; i < 256; i++)
		{
			DWORD crc = i;
			for (int j = 0; j < 8; j++)
			{
				if (crc & 1)
					crc = (crc >> 1) ^ 0xEDB88320UL;
				else
					crc >>= 1;
			}
			crc32Table[i] = crc;
		}
		tableInit = true;
	}

	HANDLE hFile = CreateFile(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return 0;

	DWORD crc = 0xFFFFFFFF;
	BYTE buffer[8192];
	DWORD bytesRead = 0;
	while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0)
	{
		for (DWORD i = 0; i < bytesRead; i++)
			crc = crc32Table[(crc ^ buffer[i]) & 0xFF] ^ (crc >> 8);
	}
	CloseHandle(hFile);
	return crc ^ 0xFFFFFFFF;
}

/**
 * @brief Copy the modification timestamp from srcPath to dstPath.
 * Uses Win32 SetFileTime API.
 */
bool CDirSideBySideCoordinator::TouchFileTimestamp(const String& srcPath, const String& dstPath)
{
	HANDLE hSrc = CreateFile(srcPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hSrc == INVALID_HANDLE_VALUE)
		return false;

	FILETIME ftCreate, ftAccess, ftWrite;
	BOOL bGot = GetFileTime(hSrc, &ftCreate, &ftAccess, &ftWrite);
	CloseHandle(hSrc);
	if (!bGot)
		return false;

	HANDLE hDst = CreateFile(dstPath.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hDst == INVALID_HANDLE_VALUE)
		return false;

	BOOL bSet = SetFileTime(hDst, nullptr, nullptr, &ftWrite);
	CloseHandle(hDst);
	return bSet != FALSE;
}

/**
 * @brief Set a file's modification time to the current time.
 * Uses GetSystemTimeAsFileTime + SetFileTime.
 */
bool CDirSideBySideCoordinator::TouchToNow(const String& filePath)
{
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);

	HANDLE hFile = CreateFile(filePath.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	BOOL bSet = SetFileTime(hFile, nullptr, nullptr, &ft);
	CloseHandle(hFile);
	return bSet != FALSE;
}

/**
 * @brief Set a file's modification time to a specific FILETIME value.
 */
bool CDirSideBySideCoordinator::TouchToSpecificTime(const String& filePath, const FILETIME& ft)
{
	HANDLE hFile = CreateFile(filePath.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	BOOL bSet = SetFileTime(hFile, nullptr, nullptr, &ft);
	CloseHandle(hFile);
	return bSet != FALSE;
}

/**
 * @brief Push current paths onto the back history stack, clear forward history.
 */
void CDirSideBySideCoordinator::PushHistory(const String& leftPath, const String& rightPath)
{
	HistoryEntry entry;
	entry.leftPath = leftPath;
	entry.rightPath = rightPath;
	m_historyBack.push_back(entry);
	m_historyForward.clear();
}

/**
 * @brief Navigate back in history.
 * Pops from m_historyBack into out-params, pushes current paths to m_historyForward.
 * @return false if history is empty
 */
bool CDirSideBySideCoordinator::NavigateBack(String& leftPath, String& rightPath)
{
	if (m_historyBack.empty())
		return false;

	// Push current paths to forward stack
	if (m_pDoc && m_pDoc->HasDiffs())
	{
		const CDiffContext &ctxt = m_pDoc->GetDiffContext();
		HistoryEntry current;
		current.leftPath = ctxt.GetPath(0);
		current.rightPath = ctxt.GetPath(ctxt.GetCompareDirs() - 1);
		m_historyForward.push_back(current);
	}

	// Pop from back stack
	HistoryEntry entry = m_historyBack.back();
	m_historyBack.pop_back();
	leftPath = entry.leftPath;
	rightPath = entry.rightPath;
	return true;
}

/**
 * @brief Navigate forward in history.
 * Pops from m_historyForward into out-params, pushes current paths to m_historyBack.
 * @return false if forward history is empty
 */
bool CDirSideBySideCoordinator::NavigateForward(String& leftPath, String& rightPath)
{
	if (m_historyForward.empty())
		return false;

	// Push current paths to back stack
	if (m_pDoc && m_pDoc->HasDiffs())
	{
		const CDiffContext &ctxt = m_pDoc->GetDiffContext();
		HistoryEntry current;
		current.leftPath = ctxt.GetPath(0);
		current.rightPath = ctxt.GetPath(ctxt.GetCompareDirs() - 1);
		m_historyBack.push_back(current);
	}

	// Pop from forward stack
	HistoryEntry entry = m_historyForward.back();
	m_historyForward.pop_back();
	leftPath = entry.leftPath;
	rightPath = entry.rightPath;
	return true;
}

/**
 * @brief Get parent directories of the current left/right base folders.
 * @return false if already at root (no parent available)
 */
bool CDirSideBySideCoordinator::GetParentPaths(String& leftParent, String& rightParent) const
{
	if (!m_pDoc || !m_pDoc->HasDiffs())
		return false;

	const CDiffContext &ctxt = m_pDoc->GetDiffContext();
	String leftPath = ctxt.GetPath(0);
	String rightPath = ctxt.GetPath(ctxt.GetCompareDirs() - 1);

	leftParent = paths::GetParentPath(leftPath);
	rightParent = paths::GetParentPath(rightPath);

	// If GetParentPath returns the same path, we are at root
	if (leftParent == leftPath || rightParent == rightPath)
		return false;

	return true;
}

/**
 * @brief Set the base folder on the specified pane and trigger re-comparison.
 * Pushes history, then stores the new base path and calls Redisplay.
 * Full re-scan integration with DirDoc is deferred.
 */
bool CDirSideBySideCoordinator::SetBaseFolder(int pane, const String& subfolderPath)
{
	if (!m_pDoc || !m_pDoc->HasDiffs())
		return false;

	const CDiffContext &ctxt = m_pDoc->GetDiffContext();
	String leftPath = ctxt.GetPath(0);
	String rightPath = ctxt.GetPath(ctxt.GetCompareDirs() - 1);

	// Push current paths to history
	PushHistory(leftPath, rightPath);

	// Replace the specified pane's path
	if (pane == 0)
		leftPath = subfolderPath;
	else
		rightPath = subfolderPath;

	// Store and redisplay - actual re-scan needs DirDoc integration
	Redisplay();
	return true;
}

/**
 * @brief Set the base folder on the opposite pane and trigger re-comparison.
 * Same as SetBaseFolder but applies to the other side.
 */
bool CDirSideBySideCoordinator::SetBaseFolderOtherSide(int pane, const String& subfolderPath)
{
	// Apply to the opposite pane
	int otherPane = (pane == 0) ? 1 : 0;
	return SetBaseFolder(otherPane, subfolderPath);
}

/**
 * @brief Exchange (swap) files between left and right sides.
 * For each DIFFITEM that exists on both sides, swaps the files using
 * a rename-to-temp, rename-left-to-right, rename-temp-to-left pattern.
 */
void CDirSideBySideCoordinator::ExchangeFiles(const std::vector<DIFFITEM*>& items)
{
	if (!m_pDoc || !m_pDoc->HasDiffs())
		return;

	const CDiffContext &ctxt = m_pDoc->GetDiffContext();
	int leftSide = 0;
	int rightSide = ctxt.GetCompareDirs() - 1;

	for (DIFFITEM *pos : items)
	{
		if (!pos)
			continue;

		const DIFFITEM &di = ctxt.GetDiffAt(pos);

		// Can only exchange files that exist on both sides
		if (!di.diffcode.exists(leftSide) || !di.diffcode.exists(rightSide))
			continue;
		if (di.diffcode.isDirectory())
			continue;

		String leftFilePath = di.getFilepath(leftSide, ctxt.GetPath(leftSide));
		String rightFilePath = di.getFilepath(rightSide, ctxt.GetPath(rightSide));

		// Create a temp name by appending ".exchange_tmp" to the left file
		String tempPath = leftFilePath + _T(".exchange_tmp");

		LogOperation(strutils::format(_T("Exchange: %s <-> %s"), leftFilePath.c_str(), rightFilePath.c_str()));

		// Step 1: Rename left to temp
		if (!MoveFile(leftFilePath.c_str(), tempPath.c_str()))
		{
			LogOperation(strutils::format(_T("  Failed to rename left to temp: %s"), leftFilePath.c_str()));
			continue;
		}

		// Step 2: Rename right to left
		if (!MoveFile(rightFilePath.c_str(), leftFilePath.c_str()))
		{
			LogOperation(strutils::format(_T("  Failed to rename right to left: %s"), rightFilePath.c_str()));
			// Try to restore left from temp
			MoveFile(tempPath.c_str(), leftFilePath.c_str());
			continue;
		}

		// Step 3: Rename temp to right
		if (!MoveFile(tempPath.c_str(), rightFilePath.c_str()))
		{
			LogOperation(strutils::format(_T("  Failed to rename temp to right: %s"), tempPath.c_str()));
			continue;
		}

		LogOperation(_T("  Exchange completed successfully"));
	}

	// Rescan to pick up changes
	m_pDoc->Rescan();
}

/**
 * @brief Store the advanced filter settings and apply them.
 * Saves to options and triggers redisplay.
 */
void CDirSideBySideCoordinator::SetAdvancedFilter(const AdvancedFilter& filter)
{
	m_advFilter = filter;
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_ADV_FILTER_DATE_FROM, m_advFilter.dateFrom);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_ADV_FILTER_DATE_TO, m_advFilter.dateTo);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_ADV_FILTER_SIZE_MIN, m_advFilter.sizeMin);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_ADV_FILTER_SIZE_MAX, m_advFilter.sizeMax);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_ADV_FILTER_ATTR, m_advFilter.attrMask);
	Redisplay();
}

/**
 * @brief Check if a DIFFITEM passes the advanced filter.
 * Checks date range, size range, and attribute mask.
 * Returns true if all checks pass or if the filter is effectively disabled
 * (empty strings / -1 values).
 */
bool CDirSideBySideCoordinator::PassesAdvancedFilter(const DIFFITEM& di) const
{
	// Date range check — parse YYYY-MM-DD and compare against file mtime
	if (!m_advFilter.dateFrom.empty())
	{
		int year = 0, month = 0, day = 0;
		if (_stscanf_s(m_advFilter.dateFrom.c_str(), _T("%d-%d-%d"), &year, &month, &day) == 3)
		{
			try
			{
				Poco::DateTime dtFrom(year, month, day);
				Poco::Timestamp tsFrom = dtFrom.timestamp();

				// Check all sides that exist
				bool anyPasses = false;
				for (int s = 0; s < 3; s++)
				{
					if (di.diffcode.exists(s) && di.diffFileInfo[s].mtime != Poco::Timestamp::TIMEVAL_MIN)
					{
						if (di.diffFileInfo[s].mtime >= tsFrom)
							anyPasses = true;
					}
				}
				// If no side exists, let it pass
				if (di.diffcode.exists(0) || di.diffcode.exists(1) || di.diffcode.exists(2))
				{
					if (!anyPasses)
						return false;
				}
			}
			catch (...)
			{
				// Invalid date format — ignore this filter
			}
		}
	}

	if (!m_advFilter.dateTo.empty())
	{
		int year = 0, month = 0, day = 0;
		if (_stscanf_s(m_advFilter.dateTo.c_str(), _T("%d-%d-%d"), &year, &month, &day) == 3)
		{
			try
			{
				// Use end of day (23:59:59) for the "to" date
				Poco::DateTime dtTo(year, month, day, 23, 59, 59);
				Poco::Timestamp tsTo = dtTo.timestamp();

				bool anyPasses = false;
				for (int s = 0; s < 3; s++)
				{
					if (di.diffcode.exists(s) && di.diffFileInfo[s].mtime != Poco::Timestamp::TIMEVAL_MIN)
					{
						if (di.diffFileInfo[s].mtime <= tsTo)
							anyPasses = true;
					}
				}
				if (di.diffcode.exists(0) || di.diffcode.exists(1) || di.diffcode.exists(2))
				{
					if (!anyPasses)
						return false;
				}
			}
			catch (...)
			{
				// Invalid date format — ignore this filter
			}
		}
	}

	// Size range check
	if (m_advFilter.sizeMin >= 0)
	{
		bool anyPasses = false;
		for (int s = 0; s < 3; s++)
		{
			if (di.diffcode.exists(s) && di.diffFileInfo[s].size != DirItem::FILE_SIZE_NONE)
			{
				if (static_cast<int>(di.diffFileInfo[s].size) >= m_advFilter.sizeMin)
					anyPasses = true;
			}
		}
		if (di.diffcode.exists(0) || di.diffcode.exists(1) || di.diffcode.exists(2))
		{
			if (!anyPasses)
				return false;
		}
	}

	if (m_advFilter.sizeMax >= 0)
	{
		bool anyPasses = false;
		for (int s = 0; s < 3; s++)
		{
			if (di.diffcode.exists(s) && di.diffFileInfo[s].size != DirItem::FILE_SIZE_NONE)
			{
				if (static_cast<int>(di.diffFileInfo[s].size) <= m_advFilter.sizeMax)
					anyPasses = true;
			}
		}
		if (di.diffcode.exists(0) || di.diffcode.exists(1) || di.diffcode.exists(2))
		{
			if (!anyPasses)
				return false;
		}
	}

	// Attribute mask check — file must have all specified attributes
	if (!m_advFilter.attrMask.empty())
	{
		bool anyPasses = false;
		for (int s = 0; s < 3; s++)
		{
			if (!di.diffcode.exists(s))
				continue;

			// Use the raw attributes stored in FileFlags
			DWORD flags = di.diffFileInfo[s].flags.attributes;

			bool sideOk = true;
			for (size_t i = 0; i < m_advFilter.attrMask.length(); i++)
			{
				tchar_t ch = m_advFilter.attrMask[i];
				switch (ch)
				{
				case _T('R'):
				case _T('r'):
					if (!(flags & FILE_ATTRIBUTE_READONLY))
						sideOk = false;
					break;
				case _T('H'):
				case _T('h'):
					if (!(flags & FILE_ATTRIBUTE_HIDDEN))
						sideOk = false;
					break;
				case _T('S'):
				case _T('s'):
					if (!(flags & FILE_ATTRIBUTE_SYSTEM))
						sideOk = false;
					break;
				case _T('A'):
				case _T('a'):
					if (!(flags & FILE_ATTRIBUTE_ARCHIVE))
						sideOk = false;
					break;
				}
			}
			if (sideOk)
				anyPasses = true;
		}
		if (!anyPasses)
			return false;
	}

	return true;
}

/**
 * @brief Set the ignore-folder-structure mode.
 * When enabled, files are matched by name only, ignoring directory hierarchy.
 */
void CDirSideBySideCoordinator::SetIgnoreFolderStructure(bool bIgnore)
{
	m_bIgnoreFolderStructure = bIgnore;
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_IGNORE_FOLDER_STRUCTURE, bIgnore);
	Redisplay();
}

/**
 * @brief Build a flat row mapping that ignores directory structure.
 * Collects ALL files from the DIFFITEM tree, then matches them by filename
 * across left and right sides. Files with the same name are aligned together.
 * Unmatched files become orphans.
 */
void CDirSideBySideCoordinator::BuildRowMappingIgnoreStructure()
{
	if (!m_pDoc || !m_pDoc->HasDiffs())
		return;

	const CDiffContext &ctxt = m_pDoc->GetDiffContext();
	int leftSide = 0;
	int rightSide = ctxt.GetCompareDirs() - 1;

	// Collect all files by walking the entire tree using GetNextDiffPosition
	// which does a depth-first traversal
	struct FileEntry
	{
		DIFFITEM *diffpos;
		String filename;
		int side; // 0 = left, 1 = right, 2 = both
	};

	std::vector<FileEntry> leftFiles;
	std::vector<FileEntry> rightFiles;
	std::vector<DIFFITEM*> bothFiles;

	DIFFITEM *diffpos = ctxt.GetFirstDiffPosition();
	while (diffpos != nullptr)
	{
		DIFFITEM *curdiffpos = diffpos;
		const DIFFITEM &di = ctxt.GetNextDiffPosition(diffpos);

		// Skip directories — we only want leaf files
		if (di.diffcode.isDirectory())
			continue;

		// Skip filtered items
		if (di.diffcode.isResultFiltered())
			continue;

		// Apply advanced filter
		if (!PassesAdvancedFilter(di))
			continue;

		bool onLeft = di.diffcode.exists(leftSide);
		bool onRight = di.diffcode.exists(rightSide);

		if (onLeft && onRight)
		{
			// File exists on both sides — keep as matched pair
			bothFiles.push_back(curdiffpos);
		}
		else if (onLeft)
		{
			FileEntry entry;
			entry.diffpos = curdiffpos;
			entry.filename = di.diffFileInfo[leftSide].filename;
			entry.side = 0;
			leftFiles.push_back(entry);
		}
		else if (onRight)
		{
			FileEntry entry;
			entry.diffpos = curdiffpos;
			entry.filename = di.diffFileInfo[rightSide].filename;
			entry.side = 1;
			rightFiles.push_back(entry);
		}
	}

	// First, add all files that already exist on both sides
	for (DIFFITEM *pos : bothFiles)
	{
		SideBySideRowItem rowItem;
		rowItem.diffpos = pos;
		rowItem.existsOnLeft = true;
		rowItem.existsOnRight = true;
		rowItem.indent = 0;
		m_rowMapping.push_back(rowItem);
	}

	// Match left-only and right-only files by filename using hash map (O(n) instead of O(n²))
	std::unordered_map<String, size_t> rightByName;
	rightByName.reserve(rightFiles.size());
	for (size_t ri = 0; ri < rightFiles.size(); ri++)
		rightByName[rightFiles[ri].filename] = ri;

	std::vector<bool> rightMatched(rightFiles.size(), false);

	for (size_t li = 0; li < leftFiles.size(); li++)
	{
		auto it = rightByName.find(leftFiles[li].filename);
		if (it != rightByName.end() && !rightMatched[it->second])
		{
			size_t ri = it->second;
			// Match found — add both as a paired row
			SideBySideRowItem rowItem;
			rowItem.diffpos = leftFiles[li].diffpos;
			rowItem.existsOnLeft = true;
			rowItem.existsOnRight = false; // Still orphan in the DIFFITEM sense
			rowItem.indent = 0;
			m_rowMapping.push_back(rowItem);

			SideBySideRowItem rowItemRight;
			rowItemRight.diffpos = rightFiles[ri].diffpos;
			rowItemRight.existsOnLeft = false;
			rowItemRight.existsOnRight = true;
			rowItemRight.indent = 0;
			m_rowMapping.push_back(rowItemRight);

			rightMatched[ri] = true;
		}
		else
		{
			// Left-only orphan
			SideBySideRowItem rowItem;
			rowItem.diffpos = leftFiles[li].diffpos;
			rowItem.existsOnLeft = true;
			rowItem.existsOnRight = false;
			rowItem.indent = 0;
			m_rowMapping.push_back(rowItem);
		}
	}

	// Add unmatched right-only orphans
	for (size_t ri = 0; ri < rightFiles.size(); ri++)
	{
		if (!rightMatched[ri])
		{
			SideBySideRowItem rowItem;
			rowItem.diffpos = rightFiles[ri].diffpos;
			rowItem.existsOnLeft = false;
			rowItem.existsOnRight = true;
			rowItem.indent = 0;
			m_rowMapping.push_back(rowItem);
		}
	}
}

/**
 * @brief Get the file version string from a PE file's version resource.
 * Uses GetFileVersionInfoSize/GetFileVersionInfo/VerQueryValue to extract FileVersion.
 * @return Version string or empty string on failure.
 */
String CDirSideBySideCoordinator::GetFileVersionString(const String& filePath)
{
	DWORD dwHandle = 0;
	DWORD dwSize = GetFileVersionInfoSize(filePath.c_str(), &dwHandle);
	if (dwSize == 0)
		return _T("");

	std::vector<BYTE> versionData(dwSize);
	if (!GetFileVersionInfo(filePath.c_str(), dwHandle, dwSize, versionData.data()))
		return _T("");

	VS_FIXEDFILEINFO *pFileInfo = nullptr;
	UINT uLen = 0;
	if (!VerQueryValue(versionData.data(), _T("\\"), reinterpret_cast<LPVOID*>(&pFileInfo), &uLen))
		return _T("");

	if (uLen == 0 || pFileInfo == nullptr)
		return _T("");

	return strutils::format(_T("%d.%d.%d.%d"),
		HIWORD(pFileInfo->dwFileVersionMS),
		LOWORD(pFileInfo->dwFileVersionMS),
		HIWORD(pFileInfo->dwFileVersionLS),
		LOWORD(pFileInfo->dwFileVersionLS));
}

/**
 * @brief Get the file owner string for an NTFS file.
 * Uses GetNamedSecurityInfo to get the owner SID, then LookupAccountSid
 * to resolve the owner name.
 * @return Owner string in "DOMAIN\\User" format or empty string on failure.
 */
String CDirSideBySideCoordinator::GetFileOwnerString(const String& filePath)
{
	PSID pSidOwner = nullptr;
	PSECURITY_DESCRIPTOR pSD = nullptr;

	DWORD dwRes = GetNamedSecurityInfo(
		filePath.c_str(),
		SE_FILE_OBJECT,
		OWNER_SECURITY_INFORMATION,
		&pSidOwner,
		nullptr,
		nullptr,
		nullptr,
		&pSD);

	if (dwRes != ERROR_SUCCESS)
		return _T("");

	tchar_t szAccountName[256] = {};
	tchar_t szDomainName[256] = {};
	DWORD dwAccountLen = _countof(szAccountName);
	DWORD dwDomainLen = _countof(szDomainName);
	SID_NAME_USE sidUse;

	BOOL bResult = LookupAccountSid(
		nullptr,
		pSidOwner,
		szAccountName,
		&dwAccountLen,
		szDomainName,
		&dwDomainLen,
		&sidUse);

	String result;
	if (bResult)
	{
		if (szDomainName[0] != _T('\0'))
		{
			result = szDomainName;
			result += _T("\\");
			result += szAccountName;
		}
		else
		{
			result = szAccountName;
		}
	}

	if (pSD)
		LocalFree(pSD);

	return result;
}

/**
 * @brief Add an alignment override to force two items to be aligned together.
 */
void CDirSideBySideCoordinator::AddAlignmentOverride(DIFFITEM* leftItem, DIFFITEM* rightItem)
{
	m_alignmentOverrides[leftItem] = rightItem;
	Redisplay();
}

/**
 * @brief Clear all alignment overrides.
 */
void CDirSideBySideCoordinator::ClearAlignmentOverrides()
{
	m_alignmentOverrides.clear();
	Redisplay();
}

/**
 * @brief Apply auto-expand mode based on the OPT_DIRVIEW_SXS_AUTO_EXPAND_MODE option.
 * Mode 0: no auto-expand. Mode 1: expand all folders. Mode 2: expand only
 * folders containing differences.
 */
void CDirSideBySideCoordinator::ApplyAutoExpand()
{
	if (!m_pDoc || !m_pDoc->HasDiffs())
		return;

	int mode = GetOptionsMgr()->GetInt(OPT_DIRVIEW_SXS_AUTO_EXPAND_MODE);
	if (mode == 0)
		return;

	CDiffContext &ctxt = m_pDoc->GetDiffContext();

	// Walk the entire tree using depth-first traversal
	DIFFITEM *diffpos = ctxt.GetFirstDiffPosition();
	while (diffpos != nullptr)
	{
		DIFFITEM *curdiffpos = diffpos;
		DIFFITEM &di = ctxt.GetNextDiffRefPosition(diffpos);

		if (!di.diffcode.isDirectory())
			continue;

		if (mode == 1)
		{
			// Expand all folders
			di.customFlags |= ViewCustomFlags::EXPANDED;
		}
		else if (mode == 2)
		{
			// Expand only folders containing differences
			FolderContentStatus status = ComputeFolderContentStatus(di);
			if (status == FOLDER_STATUS_ALL_DIFFERENT ||
				status == FOLDER_STATUS_MIXED ||
				status == FOLDER_STATUS_UNIQUE_ONLY)
			{
				di.customFlags |= ViewCustomFlags::EXPANDED;
			}
			else
			{
				di.customFlags &= ~ViewCustomFlags::EXPANDED;
			}
		}
	}
}

/**
 * @brief Format a detailed multi-line comparison information string.
 * Shows total files, identical count with percentage, different count with
 * percentage, orphan counts, newer counts, and skipped count.
 */
String CDirSideBySideCoordinator::FormatCompareInfoString() const
{
	int pctIdentical = 0;
	int pctDifferent = 0;
	if (m_statusCounts.nTotal > 0)
	{
		pctIdentical = (m_statusCounts.nIdentical * 100) / m_statusCounts.nTotal;
		pctDifferent = (m_statusCounts.nDifferent * 100) / m_statusCounts.nTotal;
	}

	return strutils::format(
		_T("Comparison Information\n\nTotal files: %d\nIdentical: %d (%d%%)\nDifferent: %d (%d%%)\nOrphan Left: %d\nOrphan Right: %d\nNewer Left: %d\nNewer Right: %d\nSkipped: %d"),
		m_statusCounts.nTotal,
		m_statusCounts.nIdentical, pctIdentical,
		m_statusCounts.nDifferent, pctDifferent,
		m_statusCounts.nOrphanLeft,
		m_statusCounts.nOrphanRight,
		m_statusCounts.nNewerLeft,
		m_statusCounts.nNewerRight,
		m_statusCounts.nSkipped);
}
