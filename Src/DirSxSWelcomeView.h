/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSxSWelcomeView.h
 *
 * @brief Declaration of CDirSxSWelcomeView — BC-style startup welcome screen.
 */
#pragma once

#include <afxwin.h>
#include <vector>
#include "UnicodeString.h"

/**
 * @brief Owner-drawn welcome view for SxS mode startup.
 *
 * Shows a dark-themed welcome screen with large icon buttons:
 *  - New Folder Comparison
 *  - New File Comparison
 *  - Open Session
 *  - Recent Sessions list
 */
class CDirSxSWelcomeView : public CView
{
	DECLARE_DYNCREATE(CDirSxSWelcomeView)
protected:
	CDirSxSWelcomeView();

public:
	virtual ~CDirSxSWelcomeView();

	virtual void OnDraw(CDC* pDC) override;
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;
	virtual void OnInitialUpdate() override;

protected:
	/** Button rectangle and action */
	struct WelcomeButton
	{
		CRect rc;
		String label;
		String description;
		int iconType; // 0=folder, 1=file, 2=session, 3=recent
	};

	void LayoutButtons(int cx, int cy);
	void DrawButton(CDC* pDC, const WelcomeButton& btn, bool bHover);
	void DrawFolderIcon(CDC* pDC, const CRect& rc, COLORREF color);
	void DrawFileIcon(CDC* pDC, const CRect& rc, COLORREF color);
	void DrawSessionIcon(CDC* pDC, const CRect& rc, COLORREF color);
	void DrawRecentIcon(CDC* pDC, const CRect& rc, COLORREF color);
	int HitTestButton(CPoint pt) const;

	std::vector<WelcomeButton> m_buttons;
	std::vector<String> m_recentPaths;
	std::vector<CRect> m_recentRects;
	int m_nHoverButton;
	int m_nHoverRecent;
	CFont m_fontTitle;
	CFont m_fontButton;
	CFont m_fontDesc;
	CFont m_fontRecent;
	bool m_bLayoutDone;

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnMouseLeave();

	DECLARE_MESSAGE_MAP()
};
