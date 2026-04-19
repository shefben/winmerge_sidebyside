/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997-2000  Thingamahoochie Software
//    Author: Dean Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSxSLogPanel.cpp
 *
 * @brief Implementation of CDirSxSLogPanel class
 */

#include "StdAfx.h"
#include "DirSxSLogPanel.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static const int LOG_PANEL_HEIGHT = 80;

BEGIN_MESSAGE_MAP(CDirSxSLogPanel, CControlBar)
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

CDirSxSLogPanel::CDirSxSLogPanel()
{
}

CDirSxSLogPanel::~CDirSxSLogPanel()
{
}

BOOL CDirSxSLogPanel::Create(CWnd* pParentWnd)
{
	// Create control bar at the bottom
	DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | CBRS_BOTTOM;
	if (!CControlBar::Create(nullptr, _T("Log"), dwStyle, CRect(0, 0, 100, LOG_PANEL_HEIGHT),
		pParentWnd, AFX_IDW_CONTROLBAR_FIRST + 32))
		return FALSE;

	// Create font for log text
	NONCLIENTMETRICS ncm = { sizeof NONCLIENTMETRICS };
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof NONCLIENTMETRICS, &ncm, 0);
	ncm.lfStatusFont.lfHeight = -11;
	_tcscpy_s(ncm.lfStatusFont.lfFaceName, _T("Consolas"));
	m_font.CreateFontIndirect(&ncm.lfStatusFont);

	// Create multiline readonly scrollable edit control
	CRect rc;
	GetClientRect(&rc);
	m_editLog.Create(
		WS_CHILD | WS_VISIBLE | WS_VSCROLL |
		ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
		rc, this, 1);
	m_editLog.SetFont(&m_font);

	// Dark theme colors
	COLORREF bg = ::GetSysColor(COLOR_WINDOW);
	int luminance = (GetRValue(bg) * 299 + GetGValue(bg) * 587 + GetBValue(bg) * 114) / 1000;
	if (luminance < 128)
	{
		// Dark mode: set colors via subclassing not possible for readonly edit,
		// but the system dark mode will handle it if enabled.
	}

	return TRUE;
}

CSize CDirSxSLogPanel::CalcFixedLayout(BOOL bStretch, BOOL bHorz)
{
	return CSize(bStretch ? SHRT_MAX : 100, LOG_PANEL_HEIGHT);
}

void CDirSxSLogPanel::OnUpdateCmdUI(CFrameWnd* /*pTarget*/, BOOL /*bDisableIfNoHndler*/)
{
	// Nothing to update
}

void CDirSxSLogPanel::OnSize(UINT nType, int cx, int cy)
{
	CControlBar::OnSize(nType, cx, cy);
	if (m_editLog.GetSafeHwnd())
	{
		CRect rc;
		GetClientRect(&rc);
		m_editLog.MoveWindow(&rc);
	}
}

void CDirSxSLogPanel::OnPaint()
{
	// Let the edit control handle painting
	Default();
}

BOOL CDirSxSLogPanel::OnEraseBkgnd(CDC* pDC)
{
	CRect rc;
	GetClientRect(&rc);
	pDC->FillSolidRect(&rc, ::GetSysColor(COLOR_WINDOW));
	return TRUE;
}

void CDirSxSLogPanel::AppendMessage(const String& sMessage)
{
	if (!m_editLog.GetSafeHwnd())
		return;

	// Get current timestamp
	SYSTEMTIME st;
	GetLocalTime(&st);
	CString sTimestamp;
	sTimestamp.Format(_T("[%02d:%02d:%02d] "), st.wHour, st.wMinute, st.wSecond);

	// Build the line
	CString sLine;
	sLine.Format(_T("%s%s\r\n"), (LPCTSTR)sTimestamp, sMessage.c_str());

	// Append to end
	int nLen = m_editLog.GetWindowTextLength();
	m_editLog.SetSel(nLen, nLen);
	m_editLog.ReplaceSel(sLine);

	// Auto-scroll to bottom
	m_editLog.SendMessage(EM_SCROLLCARET);
}

void CDirSxSLogPanel::ClearLog()
{
	if (m_editLog.GetSafeHwnd())
		m_editLog.SetWindowText(_T(""));
}
