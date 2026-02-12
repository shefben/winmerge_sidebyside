/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSxSToolBar.h
 *
 * @brief Declaration of CDirSxSToolBar class
 */
#pragma once

#include <afxext.h>

/**
 * @brief BC-style icon+text toolbar for side-by-side folder comparison.
 *
 * Displays buttons with 16x16 GDI-drawn icons and text labels.
 * Supports dropdown menus for Diffs, Sessions, and Structure buttons.
 */
class CDirSxSToolBar : public CToolBar
{
	DECLARE_DYNAMIC(CDirSxSToolBar)
public:
	CDirSxSToolBar();
	virtual ~CDirSxSToolBar();

	BOOL Create(CWnd* pParentWnd);

protected:
	CImageList m_imageList;

	void CreateToolbarIcons();
	static HBITMAP CreateIcon16(int iconType);

	afx_msg void OnDropDown(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
	void ShowDiffsDropdown();
	void ShowStructureDropdown();
	void ShowSessionsDropdown();

	DECLARE_MESSAGE_MAP()
};
