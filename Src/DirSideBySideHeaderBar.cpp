/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997-2000  Thingamahoochie Software
//    Author: Dean Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSideBySideHeaderBar.cpp
 *
 * @brief Implementation of CDirSideBySideHeaderBar class
 */

#include "StdAfx.h"
#include "DirSideBySideHeaderBar.h"
#include "RoundedRectWithShadow.h"
#include "cecolor.h"
#include "DarkModeLib.h"
#include "paths.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

constexpr int SXS_RR_RADIUS = 3;
constexpr int SXS_RR_PADDING = 3;
constexpr int SXS_RR_SHADOWWIDTH = 3;

BEGIN_MESSAGE_MAP(CDirSideBySideHeaderBar, CDialogBar)
	ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
	ON_CONTROL_RANGE(EN_SETFOCUS, IDC_STATIC_TITLE_PANE0, IDC_STATIC_TITLE_PANE2, OnSetFocusEdit)
	ON_CONTROL_RANGE(EN_KILLFOCUS, IDC_STATIC_TITLE_PANE0, IDC_STATIC_TITLE_PANE2, OnKillFocusEdit)
	ON_CONTROL_RANGE(EN_USER_CAPTION_CHANGED, IDC_STATIC_TITLE_PANE0, IDC_STATIC_TITLE_PANE2, OnChangeEdit)
	ON_CONTROL_RANGE(EN_USER_FILE_SELECTED, IDC_STATIC_TITLE_PANE0, IDC_STATIC_TITLE_PANE2, OnSelectEdit)
END_MESSAGE_MAP()

CDirSideBySideHeaderBar::CDirSideBySideHeaderBar()
	: m_nPanes(2)
{
	m_pDropHandlers[0] = nullptr;
	m_pDropHandlers[1] = nullptr;
}

CDirSideBySideHeaderBar::~CDirSideBySideHeaderBar()
{
	for (int pane = 0; pane < 2; pane++)
	{
		if (m_pDropHandlers[pane])
		{
			RevokeDragDrop(m_Edit[pane].m_hWnd);
			m_pDropHandlers[pane]->Release();
			m_pDropHandlers[pane] = nullptr;
		}
	}
}

/**
 * @brief Create the header bar.
 * Reuses IDD_EDITOR_HEADERBAR template which has 3 edit controls.
 * We only use pane 0 and 1 for left/right.
 */
BOOL CDirSideBySideHeaderBar::Create(CWnd* pParentWnd)
{
	if (!__super::Create(pParentWnd, CDirSideBySideHeaderBar::IDD,
		CBRS_ALIGN_TOP | CBRS_TOOLTIPS | CBRS_FLYBY, AFX_IDW_CONTROLBAR_FIRST + 28))
		return FALSE;

	NONCLIENTMETRICS ncm = { sizeof NONCLIENTMETRICS };
	if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof NONCLIENTMETRICS, &ncm, 0))
	{
		ncm.lfStatusFont.lfWeight = FW_BOLD;
		m_font.CreateFontIndirect(&ncm.lfStatusFont);
	}

	// Subclass the two path edit controls (left=pane0, right=pane1)
	for (int pane = 0; pane < 2; pane++)
	{
		m_Edit[pane].SubClassEdit(IDC_STATIC_TITLE_PANE0 + pane, this);
		m_Edit[pane].SetFont(&m_font);
		m_Edit[pane].SetMargins(0, std::abs(ncm.lfStatusFont.lfHeight));
	}

	// Hide the third pane edit (pane2) since SxS mode is always 2-pane
	CWnd* pPane2 = GetDlgItem(IDC_STATIC_TITLE_PANE2);
	if (pPane2)
		pPane2->ShowWindow(SW_HIDE);

	// Register drop targets on each path edit control
	for (int pane = 0; pane < 2; pane++)
	{
		m_pDropHandlers[pane] = new DropHandler(
			[this, pane](const std::vector<String>& files) { OnDropFiles(pane, files); });
		RegisterDragDrop(m_Edit[pane].m_hWnd, m_pDropHandlers[pane]);
	}

	return TRUE;
}

CSize CDirSideBySideHeaderBar::CalcFixedLayout(BOOL bStretch, BOOL bHorz)
{
	TEXTMETRIC tm;
	CClientDC dc(this);
	CFont *pOldFont = dc.SelectObject(&m_font);
	dc.GetTextMetrics(&tm);
	dc.SelectObject(pOldFont);
	const int lpx = dc.GetDeviceCaps(LOGPIXELSX);
	auto pointToPixel = [lpx](int point) { return MulDiv(point, lpx, 72); };
	int cy = pointToPixel(3 + SXS_RR_SHADOWWIDTH + SXS_RR_PADDING);
	return CSize(SHRT_MAX, 1 + tm.tmHeight + cy);
}

/**
 * @brief Resize both edit controls to equal widths.
 * Called when no explicit widths are available.
 */
void CDirSideBySideHeaderBar::Resize()
{
	if (m_hWnd == nullptr)
		return;

	WINDOWPLACEMENT infoBar = {};
	GetWindowPlacement(&infoBar);

	int widths[2] = {};
	for (int pane = 0; pane < m_nPanes; pane++)
		widths[pane] = (infoBar.rcNormalPosition.right / m_nPanes) - ((pane == 0) ? 7 : 5);
	Resize(widths);
}

/**
 * @brief Resize edit controls to match splitter column widths.
 * @param [in] widths Array of column widths from the splitter.
 */
void CDirSideBySideHeaderBar::Resize(int widths[])
{
	if (m_hWnd == nullptr)
		return;

	CRect rc;
	int x = 0;
	GetClientRect(&rc);
	bool resized = false;
	for (int pane = 0; pane < m_nPanes; pane++)
	{
		CRect rcOld;
		m_Edit[pane].GetClientRect(&rcOld);
		rc.left = x;
		rc.right = x + widths[pane] + (pane == 0 ? 5 : 7);
		x = rc.right;
		if (rcOld.Width() != rc.Width())
		{
			CClientDC dc(this);
			const int lpx = dc.GetDeviceCaps(LOGPIXELSX);
			auto pointToPixel = [lpx](int point) { return MulDiv(point, lpx, 72); };
			const int sw = pointToPixel(SXS_RR_SHADOWWIDTH);
			CRect rc2 = rc;
			rc2.DeflateRect(sw + sw, sw);
			m_Edit[pane].MoveWindow(&rc2);
			m_Edit[pane].RefreshDisplayText();
			resized = true;
		}
	}
	if (resized)
		InvalidateRect(nullptr, false);
}

void CDirSideBySideHeaderBar::DoPaint(CDC* pDC)
{
	const int lpx = pDC->GetDeviceCaps(LOGPIXELSX);
	auto pointToPixel = [lpx](int point) { return MulDiv(point, lpx, 72); };
	const int r = pointToPixel(SXS_RR_RADIUS);
	const int sw = pointToPixel(SXS_RR_SHADOWWIDTH);
	CRect rcBar;
	GetWindowRect(&rcBar);
	const COLORREF clrBarBackcolor = GetSysColor(COLOR_3DFACE);
	for (int pane = 0; pane < m_nPanes; pane++)
	{
		CRect rc;
		m_Edit[pane].GetWindowRect(&rc);
		const COLORREF clrBackcolor = m_Edit[pane].GetBackColor();
		const COLORREF clrShadow =
			CEColor::GetIntermediateColor(clrBarBackcolor, GetSysColor(COLOR_3DSHADOW), m_Edit[pane].GetActive() ? 0.5f : 0.8f);
		rc.OffsetRect(-rcBar.left, -rcBar.top);
		DrawRoundedRectWithShadow(pDC->m_hDC, rc.left - sw, rc.top, rc.right - rc.left + 2 * sw, rc.bottom - rc.top, r, sw,
			clrBackcolor, clrShadow, clrBarBackcolor);
		if (pane == m_nPanes - 1)
		{
			CRect rc2{ rc.right + sw + sw, 0, rcBar.Width(), rcBar.Height() };
			pDC->FillSolidRect(&rc2, clrBarBackcolor);
		}
	}
	__super::DoPaint(pDC);
}

BOOL CDirSideBySideHeaderBar::OnToolTipNotify(UINT id, NMHDR * pTTTStruct, LRESULT * pResult)
{
	if (m_hWnd == nullptr)
		return FALSE;

	TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pTTTStruct;
	if (pTTT->uFlags & TTF_IDISHWND)
	{
		int nID = ::GetDlgCtrlID((HWND)pTTTStruct->idFrom);
		if (nID == IDC_STATIC_TITLE_PANE0 || nID == IDC_STATIC_TITLE_PANE1)
		{
			CRect rect;
			GetWindowRect(rect);
			int maxWidth = (int)(rect.Width() * .97);
			CRect rectScreen;
			SystemParametersInfo(SPI_GETWORKAREA, 0, rectScreen, 0);
			if (rectScreen.Width() * .8 > maxWidth)
				maxWidth = (int)(rectScreen.Width() * .8);

			HANDLE hFont = (HANDLE)::SendMessage(pTTTStruct->hwndFrom, WM_GETFONT, 0, 0);
			CClientDC tempDC(this);
			HANDLE hOldFont = ::SelectObject(tempDC.GetSafeHdc(), hFont);

			CFilepathEdit * pItem = static_cast<CFilepathEdit*>(GetDlgItem(nID));
			pTTT->lpszText = const_cast<tchar_t *>(pItem->GetUpdatedTipText(&tempDC, maxWidth).c_str());

			if (hOldFont != nullptr)
				::SelectObject(tempDC.GetSafeHdc(), hOldFont);

			::SendMessage(pTTTStruct->hwndFrom, TTM_SETMAXTIPWIDTH, 0, 5000);
			SetToolTipsFirstTime(pTTTStruct->hwndFrom);
			return TRUE;
		}
	}
	return FALSE;
}

void CDirSideBySideHeaderBar::OnSetFocusEdit(UINT id)
{
	const int pane = id - IDC_STATIC_TITLE_PANE0;
	if (pane < 0 || pane >= m_nPanes)
		return;
	InvalidateRect(nullptr, false);
	if (m_setFocusCallbackfunc)
		m_setFocusCallbackfunc(pane);
}

void CDirSideBySideHeaderBar::OnKillFocusEdit(UINT id)
{
	InvalidateRect(nullptr, false);
}

void CDirSideBySideHeaderBar::OnChangeEdit(UINT id)
{
	const int pane = id - IDC_STATIC_TITLE_PANE0;
	if (pane < 0 || pane >= m_nPanes)
		return;
	InvalidateRect(nullptr, false);
	if (m_captionChangedCallbackfunc)
	{
		CString text;
		m_Edit[pane].GetWindowText(text);
		m_captionChangedCallbackfunc(pane, (const tchar_t*)text);
	}
}

void CDirSideBySideHeaderBar::OnSelectEdit(UINT id)
{
	const int pane = id - IDC_STATIC_TITLE_PANE0;
	if (pane < 0 || pane >= m_nPanes)
		return;
	InvalidateRect(nullptr, false);
	(m_fileSelectedCallbackfunc ? m_fileSelectedCallbackfunc : m_folderSelectedCallbackfunc)
		(pane, m_Edit[pane].GetSelectedPath());
}

String CDirSideBySideHeaderBar::GetCaption(int pane) const
{
	ASSERT(pane >= 0 && pane < 2);
	if (m_hWnd == nullptr)
		return _T("");

	CString str;
	m_Edit[pane].GetWindowText(str);
	return String(str);
}

void CDirSideBySideHeaderBar::SetCaption(int pane, const String& sString)
{
	ASSERT(pane >= 0 && pane < 2);
	if (m_hWnd == nullptr)
		return;

	m_Edit[pane].SetOriginalText(sString);
}

String CDirSideBySideHeaderBar::GetPath(int pane) const
{
	ASSERT(pane >= 0 && pane < 2);
	if (m_hWnd == nullptr)
		return _T("");

	return m_Edit[pane].GetPath();
}

void CDirSideBySideHeaderBar::SetPath(int pane, const String& sString)
{
	ASSERT(pane >= 0 && pane < 2);
	if (m_hWnd == nullptr)
		return;

	m_Edit[pane].SetPath(sString);
}

int CDirSideBySideHeaderBar::GetActive() const
{
	for (int pane = 0; pane < m_nPanes; pane++)
	{
		if (m_Edit[pane].GetActive())
			return pane;
	}
	return -1;
}

void CDirSideBySideHeaderBar::SetActive(int pane, bool bActive)
{
	ASSERT(pane >= 0 && pane < 2);
	if (m_hWnd == nullptr)
		return;

	if (bActive != m_Edit[pane].GetActive())
		InvalidateRect(nullptr, false);
	m_Edit[pane].SetActive(bActive);
}

void CDirSideBySideHeaderBar::SetToolTipsFirstTime(HWND hTip)
{
	if (m_Tips.find(hTip) == m_Tips.end())
	{
		m_Tips.insert(hTip);
		DarkMode::setDarkTooltips(hTip, static_cast<int>(DarkMode::ToolTipsType::tooltip));
	}
}

void CDirSideBySideHeaderBar::EditActivePanePath()
{
	const int pane = GetActive();
	if (pane >= 0)
		m_Edit[pane].PostMessage(WM_COMMAND, ID_EDITOR_EDIT_PATH, 0);
}

/**
 * @brief Handle dropped files on a pane's path edit.
 * If a folder is dropped, treat it as the new path for that pane's side
 * by invoking the folder-selected callback.
 */
void CDirSideBySideHeaderBar::OnDropFiles(int pane, const std::vector<String>& files)
{
	if (files.empty() || pane < 0 || pane >= m_nPanes)
		return;

	// Use the first dropped path (folder or file's parent folder)
	String path = files[0];
	if (paths::DoesPathExist(path) == paths::IS_EXISTING_FILE)
		path = paths::GetParentPath(path);

	if (m_folderSelectedCallbackfunc)
		m_folderSelectedCallbackfunc(pane, path);
}
