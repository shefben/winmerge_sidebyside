/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirGutterView.cpp
 *
 * @brief Implementation of CDirGutterView class
 */

#include "StdAfx.h"
#include "DirGutterView.h"
#include "DirSideBySideCoordinator.h"
#include "DirPaneView.h"
#include "DirDoc.h"
#include "DiffContext.h"
#include "DiffItem.h"
#include "DirActions.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "OptionsDirColors.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Gutter width in pixels
static const int GUTTER_WIDTH = 24;

IMPLEMENT_DYNCREATE(CDirGutterView, CView)

CDirGutterView::CDirGutterView()
	: m_pCoordinator(nullptr)
	, m_nItemHeight(18)
	, m_nTopIndex(0)
{
	m_scanBrush.CreateHatchBrush(HS_BDIAGONAL, RGB(192, 192, 192));
}

CDirGutterView::~CDirGutterView()
{
}

BEGIN_MESSAGE_MAP(CDirGutterView, CView)
	ON_WM_LBUTTONDOWN()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

BOOL CDirGutterView::PreCreateWindow(CREATESTRUCT& cs)
{
	cs.style &= ~WS_BORDER;
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	return CView::PreCreateWindow(cs);
}

/**
 * @brief Set the top visible index, syncing with list controls.
 */
void CDirGutterView::SetScrollPos(int nTopIndex)
{
	if (m_nTopIndex != nTopIndex)
	{
		m_nTopIndex = nTopIndex;
		Invalidate(FALSE);
	}
}

/**
 * @brief Trigger a repaint after data changes.
 */
void CDirGutterView::UpdateDisplay()
{
	if (GetSafeHwnd())
		Invalidate(FALSE);
}

/**
 * @brief Draw the gutter symbols for visible rows.
 */
void CDirGutterView::OnDraw(CDC* pDC)
{
	if (!m_pCoordinator)
		return;

	CRect rcClient;
	GetClientRect(&rcClient);

	// Background
	COLORREF clrBg = GetSysColor(COLOR_3DFACE);
	pDC->FillSolidRect(&rcClient, clrBg);

	// Get item height from left pane list control if not set
	CDirPaneView *pLeftPane = m_pCoordinator->GetLeftPaneView();
	if (pLeftPane && pLeftPane->GetListCtrl().GetItemCount() > 0)
	{
		CRect rcItem;
		if (pLeftPane->GetListCtrl().GetItemRect(0, &rcItem, LVIR_BOUNDS))
			m_nItemHeight = rcItem.Height();
		m_nTopIndex = pLeftPane->GetListCtrl().GetTopIndex();
	}

	if (m_nItemHeight < 1)
		m_nItemHeight = 18;

	// Create a font for symbols
	if (!m_font.GetSafeHandle())
	{
		LOGFONT lf = {};
		lf.lfHeight = -MulDiv(8, pDC->GetDeviceCaps(LOGPIXELSY), 72);
		lf.lfWeight = FW_BOLD;
		lf.lfCharSet = DEFAULT_CHARSET;
		_tcscpy_s(lf.lfFaceName, _T("Consolas"));
		m_font.CreateFontIndirect(&lf);
	}

	CFont *pOldFont = pDC->SelectObject(&m_font);
	pDC->SetBkMode(TRANSPARENT);

	const CDiffContext *pCtxt = nullptr;
	CDirDoc *pDoc = nullptr;
	if (pLeftPane)
	{
		pDoc = pLeftPane->GetDocument();
		if (pDoc && pDoc->HasDiffs())
			pCtxt = &pDoc->GetDiffContext();
	}

	int nRowCount = m_pCoordinator->GetRowCount();
	int nVisibleRows = rcClient.Height() / m_nItemHeight + 1;

	int toleranceSecs = GetOptionsMgr()->GetInt(OPT_CMP_IGNORE_SMALL_FILETIME_SECS);

	for (int i = 0; i < nVisibleRows && (m_nTopIndex + i) < nRowCount; i++)
	{
		int row = m_nTopIndex + i;
		int y = i * m_nItemHeight;

		DIFFITEM *diffpos = m_pCoordinator->GetDiffItemAt(row);
		if (!diffpos || !pCtxt)
		{
			// Placeholder - draw light separator
			continue;
		}

		const DIFFITEM &di = pCtxt->GetDiffAt(diffpos);

		// Determine symbol and color
		const tchar_t *symbol = _T("");
		COLORREF clrSymbol = RGB(0, 0, 0);

		if (di.isEmpty())
		{
			continue;
		}
		else if (di.diffcode.isResultFiltered())
		{
			symbol = _T("~");
			clrSymbol = RGB(128, 128, 128);
		}
		else if (!IsItemExistAll(*pCtxt, di))
		{
			// Orphan - arrow pointing to the side that has it
			if (di.diffcode.exists(0) && !di.diffcode.exists(pCtxt->GetCompareDirs() - 1))
			{
				symbol = _T("\x2192"); // rightward arrow (exists on left only)
				clrSymbol = RGB(128, 0, 128);
			}
			else
			{
				symbol = _T("\x2190"); // leftward arrow (exists on right only)
				clrSymbol = RGB(128, 0, 128);
			}
		}
		else if (di.diffcode.isResultSame())
		{
			symbol = _T("=");
			clrSymbol = RGB(0, 128, 0);
		}
		else if (di.diffcode.isResultDiff())
		{
			// Check newer/older
			const auto &leftTime = di.diffFileInfo[0].mtime;
			const auto &rightTime = di.diffFileInfo[pCtxt->GetCompareDirs() - 1].mtime;
			Poco::Timestamp::TimeDiff diff = leftTime - rightTime;
			Poco::Timestamp::TimeDiff toleranceUs = static_cast<Poco::Timestamp::TimeDiff>(toleranceSecs) * Poco::Timestamp::resolution();

			if (diff > toleranceUs)
			{
				symbol = _T("\x226B"); // much greater than (newer left)
				clrSymbol = RGB(192, 0, 0);
			}
			else if (diff < -toleranceUs)
			{
				symbol = _T("\x226A"); // much less than (newer right)
				clrSymbol = RGB(192, 0, 0);
			}
			else
			{
				symbol = _T("\x2260"); // not equal
				clrSymbol = RGB(192, 0, 0);
			}
		}

		pDC->SetTextColor(clrSymbol);
		CRect rcRow(rcClient.left, y, rcClient.right, y + m_nItemHeight);
		pDC->DrawText(symbol, -1, &rcRow, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

		// Draw scanning indicator for folders being scanned
		if (m_pCoordinator && m_pCoordinator->IsScanningInProgress())
		{
			if (di.diffcode.isDirectory())
			{
				// Check if this folder hasn't been fully compared yet
				if (di.diffcode.diffcode == 0)  // Not yet compared
				{
					// Draw a hatched/hollow rectangle to indicate scanning
					CPen pen(PS_DOT, 1, RGB(128, 128, 128));
					CPen* pOldPen = pDC->SelectObject(&pen);
					CBrush* pOldBrush = (CBrush*)pDC->SelectStockObject(HOLLOW_BRUSH);
					pDC->Rectangle(rcRow);
					pDC->SelectObject(pOldPen);
					pDC->SelectObject(pOldBrush);
				}
			}
		}
	}

	pDC->SelectObject(pOldFont);
}

/**
 * @brief Handle click — select the corresponding row in both panes.
 */
void CDirGutterView::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (!m_pCoordinator || m_nItemHeight < 1)
		return;

	int row = m_nTopIndex + point.y / m_nItemHeight;
	m_pCoordinator->SelectRowInBothPanes(row);

	CView::OnLButtonDown(nFlags, point);
}

BOOL CDirGutterView::OnEraseBkgnd(CDC* pDC)
{
	return TRUE; // OnDraw handles background
}

void CDirGutterView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	Invalidate(FALSE);
}

/**
 * @brief Forward mouse wheel to the left pane list control.
 */
BOOL CDirGutterView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (m_pCoordinator)
	{
		CDirPaneView *pLeftPane = m_pCoordinator->GetLeftPaneView();
		if (pLeftPane && pLeftPane->GetListCtrl().GetSafeHwnd())
		{
			pLeftPane->GetListCtrl().SendMessage(WM_MOUSEWHEEL,
				MAKEWPARAM(nFlags, zDelta), MAKELPARAM(pt.x, pt.y));
			return TRUE;
		}
	}
	return CView::OnMouseWheel(nFlags, zDelta, pt);
}
