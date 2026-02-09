/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSideBySideFilterBar.h
 *
 * @brief Declaration of CDirSideBySideFilterBar class
 */
#pragma once

#include <afxext.h>

class CDirSideBySideCoordinator;

/**
 * @brief Filter toggle toolbar for side-by-side folder comparison mode.
 *
 * Displays a row of toggle buttons: All | Different | Identical |
 * Orphans Left | Orphans Right | Newer Left | Newer Right | Skipped |
 * Suppress Filters.
 * Each button toggles the corresponding display filter and triggers redisplay.
 */
class CDirSideBySideFilterBar : public CToolBar
{
	DECLARE_DYNAMIC(CDirSideBySideFilterBar)
public:
	CDirSideBySideFilterBar();
	virtual ~CDirSideBySideFilterBar();

	BOOL Create(CWnd* pParentWnd);

	void SetCoordinator(CDirSideBySideCoordinator* pCoordinator) { m_pCoordinator = pCoordinator; }

	void UpdateButtonStates();

protected:
	CDirSideBySideCoordinator* m_pCoordinator;
	CEdit m_editNameFilter;           /**< Name filter pattern edit control */
	CStatic m_labelNameFilter;        /**< Label for name filter edit */
	CFont m_editFont;                 /**< Font for the edit control */

	afx_msg void OnFilterAll();
	afx_msg void OnFilterDifferent();
	afx_msg void OnFilterIdentical();
	afx_msg void OnFilterOrphansL();
	afx_msg void OnFilterOrphansR();
	afx_msg void OnFilterNewerL();
	afx_msg void OnFilterNewerR();
	afx_msg void OnFilterSkipped();
	afx_msg void OnSuppressFilters();
	afx_msg void OnNameFilterChanged();

	afx_msg void OnUpdateFilterAll(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFilterDifferent(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFilterIdentical(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFilterOrphansL(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFilterOrphansR(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFilterNewerL(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFilterNewerR(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFilterSkipped(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSuppressFilters(CCmdUI* pCmdUI);

	DECLARE_MESSAGE_MAP()
};
