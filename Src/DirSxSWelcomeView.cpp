/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSxSWelcomeView.cpp
 *
 * @brief Implementation of CDirSxSWelcomeView — BC-style startup welcome screen.
 */

#include "StdAfx.h"
#include "DirSxSWelcomeView.h"
#include "Merge.h"
#include "MainFrm.h"
#include "PathContext.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "paths.h"
#include <Shlobj.h>
#include <shobjidl.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Dark theme colors (matching BC / DirSxSUnifiedView palette)
namespace WelcomeColors
{
	static const COLORREF BG           = RGB(30, 33, 33);
	static const COLORREF PANEL_BG     = RGB(40, 44, 44);
	static const COLORREF BUTTON_BG    = RGB(50, 55, 58);
	static const COLORREF BUTTON_HOVER = RGB(65, 72, 78);
	static const COLORREF BUTTON_BORDER= RGB(80, 88, 92);
	static const COLORREF TEXT_TITLE   = RGB(200, 210, 220);
	static const COLORREF TEXT_BUTTON  = RGB(230, 235, 240);
	static const COLORREF TEXT_DESC    = RGB(140, 150, 160);
	static const COLORREF TEXT_RECENT  = RGB(180, 190, 200);
	static const COLORREF RECENT_HOVER = RGB(55, 62, 65);
	static const COLORREF ICON_FOLDER  = RGB(200, 180, 80);
	static const COLORREF ICON_FILE    = RGB(100, 160, 220);
	static const COLORREF ICON_SESSION = RGB(140, 200, 120);
	static const COLORREF ICON_RECENT  = RGB(180, 140, 200);
}

/////////////////////////////////////////////////////////////////////////////
// CDirSxSWelcomeView

IMPLEMENT_DYNCREATE(CDirSxSWelcomeView, CView)

BEGIN_MESSAGE_MAP(CDirSxSWelcomeView, CView)
	ON_WM_SIZE()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_ERASEBKGND()
	ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()

CDirSxSWelcomeView::CDirSxSWelcomeView()
	: m_nHoverButton(-1)
	, m_nHoverRecent(-1)
	, m_bLayoutDone(false)
{
}

CDirSxSWelcomeView::~CDirSxSWelcomeView()
{
}

BOOL CDirSxSWelcomeView::PreCreateWindow(CREATESTRUCT& cs)
{
	cs.style &= ~WS_BORDER;
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	return CView::PreCreateWindow(cs);
}

void CDirSxSWelcomeView::OnInitialUpdate()
{
	CView::OnInitialUpdate();

	// Create fonts
	int dpi = CClientDC(this).GetDeviceCaps(LOGPIXELSY);

	LOGFONT lf = {};
	lf.lfHeight = -MulDiv(22, dpi, 72);
	lf.lfWeight = FW_BOLD;
	_tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
	m_fontTitle.CreateFontIndirect(&lf);

	lf.lfHeight = -MulDiv(13, dpi, 72);
	lf.lfWeight = FW_SEMIBOLD;
	m_fontButton.CreateFontIndirect(&lf);

	lf.lfHeight = -MulDiv(9, dpi, 72);
	lf.lfWeight = FW_NORMAL;
	m_fontDesc.CreateFontIndirect(&lf);

	lf.lfHeight = -MulDiv(10, dpi, 72);
	lf.lfWeight = FW_NORMAL;
	m_fontRecent.CreateFontIndirect(&lf);

	// Recent files are not directly accessible from CWinApp (protected member).
	// The recent list will be populated lazily via WM_INITMENUPOPUP-style MRU
	// or we can populate from the MFC app's recent file list at draw time.

	// Layout will happen on first OnSize or OnDraw
}

void CDirSxSWelcomeView::LayoutButtons(int cx, int cy)
{
	m_buttons.clear();
	m_recentRects.clear();

	int dpi = CClientDC(this).GetDeviceCaps(LOGPIXELSY);
	auto px = [dpi](int pt) { return MulDiv(pt, dpi, 72); };

	int btnW = px(200);
	int btnH = px(80);
	int gap = px(20);
	int totalW = btnW * 2 + gap;
	int totalH = btnH * 2 + gap;
	int startX = (cx - totalW) / 2;
	int startY = cy / 4; // Start at 25% height

	// 4 buttons in 2x2 grid
	WelcomeButton btn;

	// Top-left: New Folder Comparison
	btn.rc = CRect(startX, startY, startX + btnW, startY + btnH);
	btn.label = _T("New Folder Comparison");
	btn.description = _T("Compare two folders side by side");
	btn.iconType = 0;
	m_buttons.push_back(btn);

	// Top-right: New File Comparison
	btn.rc = CRect(startX + btnW + gap, startY, startX + totalW, startY + btnH);
	btn.label = _T("New File Comparison");
	btn.description = _T("Compare two files");
	btn.iconType = 1;
	m_buttons.push_back(btn);

	// Bottom-left: Open Session
	btn.rc = CRect(startX, startY + btnH + gap, startX + btnW, startY + totalH);
	btn.label = _T("Open Session...");
	btn.description = _T("Load a saved .WinMerge session");
	btn.iconType = 2;
	m_buttons.push_back(btn);

	// Bottom-right: Recent Sessions
	btn.rc = CRect(startX + btnW + gap, startY + btnH + gap, startX + totalW, startY + totalH);
	btn.label = _T("Recent Sessions");
	btn.description = _T("Open a recent comparison");
	btn.iconType = 3;
	m_buttons.push_back(btn);

	// Recent paths list below buttons
	int recentY = startY + totalH + px(30);
	int recentH = px(22);
	for (size_t i = 0; i < m_recentPaths.size(); i++)
	{
		CRect rc(startX, recentY + (int)i * (recentH + 4),
			startX + totalW, recentY + (int)i * (recentH + 4) + recentH);
		m_recentRects.push_back(rc);
	}

	m_bLayoutDone = true;
}

BOOL CDirSxSWelcomeView::OnEraseBkgnd(CDC* pDC)
{
	CRect rc;
	GetClientRect(&rc);
	pDC->FillSolidRect(&rc, WelcomeColors::BG);
	return TRUE;
}

void CDirSxSWelcomeView::OnDraw(CDC* pDC)
{
	CRect rcClient;
	GetClientRect(&rcClient);

	if (rcClient.Width() <= 0 || rcClient.Height() <= 0)
		return;

	if (!m_bLayoutDone)
		LayoutButtons(rcClient.Width(), rcClient.Height());

	// Double-buffer
	CDC dcMem;
	dcMem.CreateCompatibleDC(pDC);
	CBitmap bmpMem;
	bmpMem.CreateCompatibleBitmap(pDC, rcClient.Width(), rcClient.Height());
	CBitmap* pOldBmp = dcMem.SelectObject(&bmpMem);

	dcMem.FillSolidRect(&rcClient, WelcomeColors::BG);

	// Title
	dcMem.SetBkMode(TRANSPARENT);
	CFont* pOldFont = dcMem.SelectObject(&m_fontTitle);
	dcMem.SetTextColor(WelcomeColors::TEXT_TITLE);
	CRect rcTitle(0, rcClient.Height() / 8, rcClient.Width(), rcClient.Height() / 4);
	dcMem.DrawText(_T("WinMerge — Side by Side"), -1, &rcTitle,
		DT_CENTER | DT_SINGLELINE | DT_VCENTER);

	// Buttons
	for (int i = 0; i < (int)m_buttons.size(); i++)
		DrawButton(&dcMem, m_buttons[i], i == m_nHoverButton);

	// Recent paths header
	if (!m_recentPaths.empty() && !m_recentRects.empty())
	{
		dcMem.SelectObject(&m_fontDesc);
		dcMem.SetTextColor(WelcomeColors::TEXT_DESC);
		CRect rcHdr(m_recentRects[0].left, m_recentRects[0].top - 20,
			m_recentRects[0].right, m_recentRects[0].top);
		dcMem.DrawText(_T("Recent Comparisons:"), -1, &rcHdr, DT_LEFT | DT_SINGLELINE);
	}

	// Recent paths
	dcMem.SelectObject(&m_fontRecent);
	for (int i = 0; i < (int)m_recentPaths.size(); i++)
	{
		bool bHover = (i == m_nHoverRecent);
		if (bHover)
			dcMem.FillSolidRect(&m_recentRects[i], WelcomeColors::RECENT_HOVER);

		dcMem.SetTextColor(bHover ? WelcomeColors::TEXT_BUTTON : WelcomeColors::TEXT_RECENT);
		CRect rcText = m_recentRects[i];
		rcText.left += 8;
		dcMem.DrawText(m_recentPaths[i].c_str(), -1, &rcText,
			DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
	}

	dcMem.SelectObject(pOldFont);

	// Blit
	pDC->BitBlt(0, 0, rcClient.Width(), rcClient.Height(), &dcMem, 0, 0, SRCCOPY);
	dcMem.SelectObject(pOldBmp);
}

void CDirSxSWelcomeView::DrawButton(CDC* pDC, const WelcomeButton& btn, bool bHover)
{
	COLORREF bgColor = bHover ? WelcomeColors::BUTTON_HOVER : WelcomeColors::BUTTON_BG;

	// Button background with rounded-ish border
	pDC->FillSolidRect(&btn.rc, bgColor);
	CPen pen(PS_SOLID, 1, WelcomeColors::BUTTON_BORDER);
	CPen* pOld = pDC->SelectObject(&pen);
	CBrush* pOldBr = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
	pDC->Rectangle(&btn.rc);
	pDC->SelectObject(pOld);
	pDC->SelectObject(pOldBr);

	// Icon area (left side of button)
	int iconSize = (btn.rc.Height() - 20) * 2 / 3;
	CRect rcIcon(btn.rc.left + 12, btn.rc.top + (btn.rc.Height() - iconSize) / 2,
		btn.rc.left + 12 + iconSize, btn.rc.top + (btn.rc.Height() + iconSize) / 2);

	switch (btn.iconType)
	{
	case 0: DrawFolderIcon(pDC, rcIcon, WelcomeColors::ICON_FOLDER); break;
	case 1: DrawFileIcon(pDC, rcIcon, WelcomeColors::ICON_FILE); break;
	case 2: DrawSessionIcon(pDC, rcIcon, WelcomeColors::ICON_SESSION); break;
	case 3: DrawRecentIcon(pDC, rcIcon, WelcomeColors::ICON_RECENT); break;
	}

	// Label text
	int textLeft = rcIcon.right + 10;
	CFont* pOldFont = pDC->SelectObject(&m_fontButton);
	pDC->SetTextColor(WelcomeColors::TEXT_BUTTON);
	CRect rcLabel(textLeft, btn.rc.top + 12, btn.rc.right - 8, btn.rc.top + btn.rc.Height() / 2 + 4);
	pDC->DrawText(btn.label.c_str(), -1, &rcLabel, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

	// Description text
	pDC->SelectObject(&m_fontDesc);
	pDC->SetTextColor(WelcomeColors::TEXT_DESC);
	CRect rcDesc(textLeft, btn.rc.top + btn.rc.Height() / 2, btn.rc.right - 8, btn.rc.bottom - 8);
	pDC->DrawText(btn.description.c_str(), -1, &rcDesc, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

	pDC->SelectObject(pOldFont);
}

void CDirSxSWelcomeView::DrawFolderIcon(CDC* pDC, const CRect& rc, COLORREF color)
{
	CBrush brush(color);
	CPen pen(PS_SOLID, 1, RGB(GetRValue(color) * 2 / 3, GetGValue(color) * 2 / 3, GetBValue(color) * 2 / 3));
	CBrush* pOldBr = pDC->SelectObject(&brush);
	CPen* pOldPen = pDC->SelectObject(&pen);

	int tabW = rc.Width() * 5 / 12;
	int tabH = rc.Height() / 5;
	pDC->Rectangle(rc.left + 1, rc.top + 1, rc.left + tabW, rc.top + 1 + tabH);
	int bodyTop = rc.top + tabH;
	pDC->Rectangle(rc.left + 1, bodyTop, rc.right - 1, rc.bottom - 1);

	pDC->SelectObject(pOldBr);
	pDC->SelectObject(pOldPen);
}

void CDirSxSWelcomeView::DrawFileIcon(CDC* pDC, const CRect& rc, COLORREF color)
{
	CBrush brush(color);
	CPen pen(PS_SOLID, 1, RGB(GetRValue(color) * 2 / 3, GetGValue(color) * 2 / 3, GetBValue(color) * 2 / 3));
	CBrush* pOldBr = pDC->SelectObject(&brush);
	CPen* pOldPen = pDC->SelectObject(&pen);

	// File shape with folded corner
	int cornerSize = rc.Width() / 4;
	POINT pts[6] = {
		{ rc.left + 2, rc.top + 2 },
		{ rc.right - 2 - cornerSize, rc.top + 2 },
		{ rc.right - 2, rc.top + 2 + cornerSize },
		{ rc.right - 2, rc.bottom - 2 },
		{ rc.left + 2, rc.bottom - 2 },
		{ rc.left + 2, rc.top + 2 },
	};
	pDC->Polygon(pts, 6);

	// Fold triangle
	POINT fold[3] = {
		{ rc.right - 2 - cornerSize, rc.top + 2 },
		{ rc.right - 2 - cornerSize, rc.top + 2 + cornerSize },
		{ rc.right - 2, rc.top + 2 + cornerSize },
	};
	CBrush darkBrush(RGB(GetRValue(color) * 3 / 4, GetGValue(color) * 3 / 4, GetBValue(color) * 3 / 4));
	pDC->SelectObject(&darkBrush);
	pDC->Polygon(fold, 3);

	pDC->SelectObject(pOldBr);
	pDC->SelectObject(pOldPen);
}

void CDirSxSWelcomeView::DrawSessionIcon(CDC* pDC, const CRect& rc, COLORREF color)
{
	// Simple disk/save icon
	CBrush brush(color);
	CPen pen(PS_SOLID, 1, RGB(GetRValue(color) * 2 / 3, GetGValue(color) * 2 / 3, GetBValue(color) * 2 / 3));
	CBrush* pOldBr = pDC->SelectObject(&brush);
	CPen* pOldPen = pDC->SelectObject(&pen);

	pDC->Rectangle(rc.left + 2, rc.top + 2, rc.right - 2, rc.bottom - 2);

	// Top slot
	CBrush dark(RGB(GetRValue(color) * 2 / 3, GetGValue(color) * 2 / 3, GetBValue(color) * 2 / 3));
	pDC->SelectObject(&dark);
	int slotH = rc.Height() / 4;
	pDC->Rectangle(rc.left + rc.Width() / 4, rc.top + 2,
		rc.right - rc.Width() / 4, rc.top + 2 + slotH);

	// Bottom label area
	CBrush light(RGB(255, 255, 255));
	pDC->SelectObject(&light);
	pDC->Rectangle(rc.left + rc.Width() / 5, rc.bottom - rc.Height() * 2 / 5,
		rc.right - rc.Width() / 5, rc.bottom - 2);

	pDC->SelectObject(pOldBr);
	pDC->SelectObject(pOldPen);
}

void CDirSxSWelcomeView::DrawRecentIcon(CDC* pDC, const CRect& rc, COLORREF color)
{
	// Clock icon
	CPen pen(PS_SOLID, 2, color);
	CPen* pOldPen = pDC->SelectObject(&pen);
	CBrush* pOldBr = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);

	int cx = (rc.left + rc.right) / 2;
	int cy = (rc.top + rc.bottom) / 2;
	int r = min(rc.Width(), rc.Height()) / 2 - 2;
	pDC->Ellipse(cx - r, cy - r, cx + r, cy + r);

	// Clock hands
	pDC->MoveTo(cx, cy);
	pDC->LineTo(cx, cy - r * 2 / 3); // 12 o'clock
	pDC->MoveTo(cx, cy);
	pDC->LineTo(cx + r / 2, cy + r / 4); // ~4 o'clock

	pDC->SelectObject(pOldBr);
	pDC->SelectObject(pOldPen);
}

int CDirSxSWelcomeView::HitTestButton(CPoint pt) const
{
	for (int i = 0; i < (int)m_buttons.size(); i++)
	{
		if (m_buttons[i].rc.PtInRect(pt))
			return i;
	}
	return -1;
}

void CDirSxSWelcomeView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	if (cx > 0 && cy > 0)
	{
		LayoutButtons(cx, cy);
		Invalidate(FALSE);
	}
}

void CDirSxSWelcomeView::OnMouseMove(UINT nFlags, CPoint point)
{
	// Track mouse for hover effects
	TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
	TrackMouseEvent(&tme);

	int nOldHover = m_nHoverButton;
	int nOldRecent = m_nHoverRecent;
	m_nHoverButton = HitTestButton(point);
	m_nHoverRecent = -1;
	for (int i = 0; i < (int)m_recentRects.size(); i++)
	{
		if (m_recentRects[i].PtInRect(point))
		{
			m_nHoverRecent = i;
			break;
		}
	}

	if (m_nHoverButton != nOldHover || m_nHoverRecent != nOldRecent)
		Invalidate(FALSE);

	CView::OnMouseMove(nFlags, point);
}

void CDirSxSWelcomeView::OnMouseLeave()
{
	if (m_nHoverButton >= 0 || m_nHoverRecent >= 0)
	{
		m_nHoverButton = -1;
		m_nHoverRecent = -1;
		Invalidate(FALSE);
	}
}

void CDirSxSWelcomeView::OnLButtonUp(UINT nFlags, CPoint point)
{
	CMainFrame *pMainFrame = GetMainFrame();

	int nBtn = HitTestButton(point);
	if (nBtn == 0)
	{
		// New Folder Comparison — browse for two folders
		CFolderPickerDialog dlgLeft(nullptr, 0, this);
		dlgLeft.m_ofn.lpstrTitle = _T("Select Left Folder");
		if (dlgLeft.DoModal() == IDOK)
		{
			CFolderPickerDialog dlgRight(nullptr, 0, this);
			dlgRight.m_ofn.lpstrTitle = _T("Select Right Folder");
			if (dlgRight.DoModal() == IDOK)
			{
				PathContext pathCtx;
				pathCtx.SetLeft(CString(dlgLeft.GetPathName()).GetString());
				pathCtx.SetRight(CString(dlgRight.GetPathName()).GetString());
				fileopenflags_t dwFlags[3] = {};
				pMainFrame->DoFileOrFolderOpen(&pathCtx, dwFlags, nullptr, _T(""),
					true, nullptr);
			}
		}
	}
	else if (nBtn == 1)
	{
		// New File Comparison — use standard open dialog
		pMainFrame->DoFileOrFolderOpen();
	}
	else if (nBtn == 2)
	{
		// Open Session — file picker for .WinMerge files
		CFileDialog dlg(TRUE, _T("WinMerge"), nullptr, OFN_FILEMUSTEXIST,
			_T("WinMerge Session Files (*.WinMerge)|*.WinMerge|All Files (*.*)|*.*||"), this);
		if (dlg.DoModal() == IDOK)
		{
			String sPath(dlg.GetPathName().GetString());
			theApp.LoadAndOpenProjectFile(sPath);
		}
	}
	else if (nBtn == 3)
	{
		// Recent Sessions — just scroll down to the list
		// (clicking in the list is handled below)
	}

	// Check recent path clicks
	for (int i = 0; i < (int)m_recentRects.size(); i++)
	{
		if (m_recentRects[i].PtInRect(point) && i < (int)m_recentPaths.size())
		{
			String sPath = m_recentPaths[i];
			theApp.LoadAndOpenProjectFile(sPath);
			break;
		}
	}

	CView::OnLButtonUp(nFlags, point);
}
