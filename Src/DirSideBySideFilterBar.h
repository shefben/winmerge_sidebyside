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
 * @brief BC-style filter bar for side-by-side folder comparison mode.
 *
 * Displays a clean text filter field with a "Filters" dropdown button
 * (popup menu with checkable filter options) and optional "Peek" button.
 * Replaces the old colored toggle button toolbar with a cleaner UI.
 */
class CDirSideBySideFilterBar : public CControlBar
{
	DECLARE_DYNAMIC(CDirSideBySideFilterBar)
public:
	CDirSideBySideFilterBar();
	virtual ~CDirSideBySideFilterBar();

	BOOL Create(CWnd* pParentWnd);
	CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz) override;
	void OnUpdateCmdUI(CFrameWnd* pTarget, BOOL bDisableIfNoHndler) override {}

	void SetCoordinator(CDirSideBySideCoordinator* pCoordinator) { m_pCoordinator = pCoordinator; }

	void UpdateButtonStates();

protected:
	CDirSideBySideCoordinator* m_pCoordinator;
	CStatic m_labelFilter;            /**< "Filter:" label */
	CEdit m_editFilter;               /**< Filter pattern edit control */
	CButton m_btnFilters;             /**< "Filters" dropdown button */
	CButton m_btnPeek;                /**< "Peek" toggle button */
	CFont m_editFont;                 /**< Font for the controls */
	CBrush m_brDarkBg;                /**< Dark background brush */
	CBrush m_brDarkEdit;              /**< Dark edit background brush */

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS);
	afx_msg void OnPeek();
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
	afx_msg void OnAdvancedFilter();
	afx_msg void OnFiltersDropdown();

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
