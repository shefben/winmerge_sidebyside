/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirGutterView.h
 *
 * @brief Declaration of CDirGutterView class
 */
#pragma once

#include <afxwin.h>

class CDirSideBySideCoordinator;

/**
 * @brief Center gutter view between the two side-by-side panes.
 *
 * Displays a narrow column (~24px) with comparison result icons:
 * = for same, != for different, <- for orphan right, -> for orphan left,
 * < for newer left, > for newer right.
 * Clicking a row selects it in both panes.
 * Vertical scroll is synced with the list controls.
 */
class CDirGutterView : public CView
{
	DECLARE_DYNCREATE(CDirGutterView)
protected:
	CDirGutterView();

public:
	virtual ~CDirGutterView();

	void SetCoordinator(CDirSideBySideCoordinator* pCoordinator) { m_pCoordinator = pCoordinator; }

	/** Invalidate and repaint after row mapping changes */
	void UpdateDisplay();

	/** Set the item height to match list control row height */
	void SetItemHeight(int nHeight) { m_nItemHeight = nHeight; }

	/** Set scroll position to match list control */
	void SetScrollPos(int nTopIndex);

protected:
	virtual void OnDraw(CDC* pDC) override;
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;

	CDirSideBySideCoordinator* m_pCoordinator;
	int m_nItemHeight;      /**< Height of each row in pixels */
	int m_nTopIndex;        /**< First visible row index (synced with list) */
	CFont m_font;           /**< Font for gutter symbols */
	CBrush m_scanBrush;     /**< Hatched brush for scanning-in-progress folders */

	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

	DECLARE_MESSAGE_MAP()
};
