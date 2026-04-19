/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSxSLogPanel.h
 *
 * @brief Declaration of CDirSxSLogPanel class — bottom log panel for SxS mode.
 */
#pragma once

#include "UnicodeString.h"

/**
 * @brief Bottom log/console panel for side-by-side folder comparison.
 *
 * Displays timestamped messages about scan progress, file operations,
 * and errors. Implemented as a CControlBar with a multiline readonly CEdit.
 */
class CDirSxSLogPanel : public CControlBar
{
public:
	CDirSxSLogPanel();
	virtual ~CDirSxSLogPanel();

	BOOL Create(CWnd* pParentWnd);

	/** Append a timestamped message to the log */
	void AppendMessage(const String& sMessage);

	/** Clear all log messages */
	void ClearLog();

	// CControlBar overrides
	virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz) override;
	virtual void OnUpdateCmdUI(CFrameWnd* pTarget, BOOL bDisableIfNoHndler) override;

protected:
	CEdit m_editLog;
	CFont m_font;

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);

	DECLARE_MESSAGE_MAP()
};
