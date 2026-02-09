/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSideBySideFilterBar.cpp
 *
 * @brief Implementation of CDirSideBySideFilterBar class
 */

#include "StdAfx.h"
#include "DirSideBySideFilterBar.h"
#include "DirSideBySideCoordinator.h"
#include "DirPaneView.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CDirSideBySideFilterBar, CToolBar)

// Button definitions for the SxS filter toolbar
static const UINT SxsFilterButtons[] =
{
	ID_DIR_SXS_FILTER_ALL,
	ID_SEPARATOR,
	ID_DIR_SXS_FILTER_DIFFERENT,
	ID_DIR_SXS_FILTER_IDENTICAL,
	ID_SEPARATOR,
	ID_DIR_SXS_FILTER_ORPHANS_L,
	ID_DIR_SXS_FILTER_ORPHANS_R,
	ID_SEPARATOR,
	ID_DIR_SXS_FILTER_NEWER_L,
	ID_DIR_SXS_FILTER_NEWER_R,
	ID_SEPARATOR,
	ID_DIR_SXS_FILTER_SKIPPED,
	ID_SEPARATOR,
	ID_DIR_SXS_SUPPRESS_FILTERS,
};

CDirSideBySideFilterBar::CDirSideBySideFilterBar()
	: m_pCoordinator(nullptr)
{
}

CDirSideBySideFilterBar::~CDirSideBySideFilterBar()
{
}

BEGIN_MESSAGE_MAP(CDirSideBySideFilterBar, CToolBar)
	ON_COMMAND(ID_DIR_SXS_FILTER_ALL, OnFilterAll)
	ON_COMMAND(ID_DIR_SXS_FILTER_DIFFERENT, OnFilterDifferent)
	ON_COMMAND(ID_DIR_SXS_FILTER_IDENTICAL, OnFilterIdentical)
	ON_COMMAND(ID_DIR_SXS_FILTER_ORPHANS_L, OnFilterOrphansL)
	ON_COMMAND(ID_DIR_SXS_FILTER_ORPHANS_R, OnFilterOrphansR)
	ON_COMMAND(ID_DIR_SXS_FILTER_NEWER_L, OnFilterNewerL)
	ON_COMMAND(ID_DIR_SXS_FILTER_NEWER_R, OnFilterNewerR)
	ON_COMMAND(ID_DIR_SXS_FILTER_SKIPPED, OnFilterSkipped)
	ON_COMMAND(ID_DIR_SXS_SUPPRESS_FILTERS, OnSuppressFilters)
	ON_EN_KILLFOCUS(ID_DIR_SXS_NAME_FILTER_EDIT, OnNameFilterChanged)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_ALL, OnUpdateFilterAll)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_DIFFERENT, OnUpdateFilterDifferent)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_IDENTICAL, OnUpdateFilterIdentical)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_ORPHANS_L, OnUpdateFilterOrphansL)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_ORPHANS_R, OnUpdateFilterOrphansR)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_NEWER_L, OnUpdateFilterNewerL)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_NEWER_R, OnUpdateFilterNewerR)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FILTER_SKIPPED, OnUpdateFilterSkipped)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_SUPPRESS_FILTERS, OnUpdateSuppressFilters)
	ON_COMMAND(ID_DIR_SXS_ADV_FILTER, OnAdvancedFilter)
END_MESSAGE_MAP()

/**
 * @brief Create the filter toolbar with text buttons.
 */
BOOL CDirSideBySideFilterBar::Create(CWnd* pParentWnd)
{
	if (!CToolBar::CreateEx(pParentWnd, TBSTYLE_FLAT | TBSTYLE_LIST,
		WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS | CBRS_FLYBY,
		CRect(0, 0, 0, 0), AFX_IDW_CONTROLBAR_FIRST + 27))
	{
		return FALSE;
	}

	GetToolBarCtrl().SetButtonSize(CSize(90, 22));

	// Set buttons
	SetButtons(SxsFilterButtons, _countof(SxsFilterButtons));

	// Set text labels for each button
	struct { UINT id; const tchar_t* text; } btnText[] = {
		{ ID_DIR_SXS_FILTER_ALL,       _T("All") },
		{ ID_DIR_SXS_FILTER_DIFFERENT, _T("Different") },
		{ ID_DIR_SXS_FILTER_IDENTICAL, _T("Identical") },
		{ ID_DIR_SXS_FILTER_ORPHANS_L, _T("Orphans L") },
		{ ID_DIR_SXS_FILTER_ORPHANS_R, _T("Orphans R") },
		{ ID_DIR_SXS_FILTER_NEWER_L,   _T("Newer L") },
		{ ID_DIR_SXS_FILTER_NEWER_R,   _T("Newer R") },
		{ ID_DIR_SXS_FILTER_SKIPPED,   _T("Skipped") },
		{ ID_DIR_SXS_SUPPRESS_FILTERS, _T("Suppress") },
	};

	// Add text to each button
	CToolBarCtrl& tbCtrl = GetToolBarCtrl();
	for (const auto& btn : btnText)
	{
		int idx = CommandToIndex(btn.id);
		if (idx >= 0)
		{
			SetButtonText(idx, btn.text);
			// Make checkable
			UINT nStyle = GetButtonStyle(idx);
			SetButtonStyle(idx, nStyle | TBBS_CHECKBOX);
		}
	}

	// Recalculate sizes based on text
	CSize sizeButton(0, 0);
	CSize sizeImage(0, 0);
	SetSizes(CSize(90, 22), CSize(1, 1));

	SetBarStyle(GetBarStyle() & ~CBRS_BORDER_ANY);

	// Create name filter label and edit control at the right end of the toolbar
	NONCLIENTMETRICS ncm = { sizeof NONCLIENTMETRICS };
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof NONCLIENTMETRICS, &ncm, 0);
	m_editFont.CreateFontIndirect(&ncm.lfStatusFont);

	// Calculate position after last button
	int nButtons = GetToolBarCtrl().GetButtonCount();
	CRect rcLastBtn;
	GetToolBarCtrl().GetItemRect(nButtons - 1, &rcLastBtn);

	int labelX = rcLastBtn.right + 8;
	int editX = labelX + 42;
	int editY = 2;
	int editH = 18;
	int editW = 150;

	m_labelNameFilter.Create(_T("Filter:"), WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
		CRect(labelX, editY, labelX + 40, editY + editH), this);
	m_labelNameFilter.SetFont(&m_editFont);

	m_editNameFilter.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
		CRect(editX, editY, editX + editW, editY + editH), this, ID_DIR_SXS_NAME_FILTER_EDIT);
	m_editNameFilter.SetFont(&m_editFont);

	// Restore saved filter pattern
	String savedFilter = GetOptionsMgr()->GetString(OPT_DIRVIEW_SXS_NAME_FILTER);
	if (!savedFilter.empty())
		m_editNameFilter.SetWindowText(savedFilter.c_str());

	// Set cue banner text (placeholder)
	m_editNameFilter.SetCueBanner(_T("e.g. *.cpp;*.h"));

	// Add Advanced Filter button
	{
		TBBUTTON btn = {};
		btn.iBitmap = I_IMAGENONE;
		btn.idCommand = ID_DIR_SXS_ADV_FILTER;
		btn.fsState = TBSTATE_ENABLED;
		btn.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT;
		btn.iString = (INT_PTR)_T("Filters...");
		GetToolBarCtrl().AddButtons(1, &btn);
	}

	return TRUE;
}

void CDirSideBySideFilterBar::UpdateButtonStates()
{
	if (GetSafeHwnd())
		Invalidate();
}

// Helper to toggle a boolean option and redisplay
static void ToggleOption(const String& optName, CDirSideBySideCoordinator* pCoord)
{
	bool bCurrent = GetOptionsMgr()->GetBool(optName);
	GetOptionsMgr()->SaveOption(optName, !bCurrent);
	if (pCoord)
		pCoord->Redisplay();
}

void CDirSideBySideFilterBar::OnFilterAll()
{
	// Turn on all display filters
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT, true);
	GetOptionsMgr()->SaveOption(OPT_SHOW_IDENTICAL, true);
	GetOptionsMgr()->SaveOption(OPT_SHOW_UNIQUE_LEFT, true);
	GetOptionsMgr()->SaveOption(OPT_SHOW_UNIQUE_RIGHT, true);
	GetOptionsMgr()->SaveOption(OPT_SHOW_SKIPPED, true);
	if (m_pCoordinator)
		m_pCoordinator->Redisplay();
}

void CDirSideBySideFilterBar::OnFilterDifferent()
{
	ToggleOption(OPT_SHOW_DIFFERENT, m_pCoordinator);
}

void CDirSideBySideFilterBar::OnFilterIdentical()
{
	ToggleOption(OPT_SHOW_IDENTICAL, m_pCoordinator);
}

void CDirSideBySideFilterBar::OnFilterOrphansL()
{
	ToggleOption(OPT_SHOW_UNIQUE_LEFT, m_pCoordinator);
}

void CDirSideBySideFilterBar::OnFilterOrphansR()
{
	ToggleOption(OPT_SHOW_UNIQUE_RIGHT, m_pCoordinator);
}

void CDirSideBySideFilterBar::OnFilterNewerL()
{
	ToggleOption(OPT_SHOW_DIFFERENT_LEFT_ONLY, m_pCoordinator);
}

void CDirSideBySideFilterBar::OnFilterNewerR()
{
	ToggleOption(OPT_SHOW_DIFFERENT_RIGHT_ONLY, m_pCoordinator);
}

void CDirSideBySideFilterBar::OnFilterSkipped()
{
	ToggleOption(OPT_SHOW_SKIPPED, m_pCoordinator);
}

void CDirSideBySideFilterBar::OnSuppressFilters()
{
	ToggleOption(OPT_DIRVIEW_SXS_SUPPRESS_FILTERS, m_pCoordinator);
}

// Update UI handlers — set check state based on current option values
void CDirSideBySideFilterBar::OnUpdateFilterAll(CCmdUI* pCmdUI)
{
	bool allOn = GetOptionsMgr()->GetBool(OPT_SHOW_DIFFERENT) &&
		GetOptionsMgr()->GetBool(OPT_SHOW_IDENTICAL) &&
		GetOptionsMgr()->GetBool(OPT_SHOW_UNIQUE_LEFT) &&
		GetOptionsMgr()->GetBool(OPT_SHOW_UNIQUE_RIGHT) &&
		GetOptionsMgr()->GetBool(OPT_SHOW_SKIPPED);
	pCmdUI->SetCheck(allOn);
}

void CDirSideBySideFilterBar::OnUpdateFilterDifferent(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_SHOW_DIFFERENT));
}

void CDirSideBySideFilterBar::OnUpdateFilterIdentical(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_SHOW_IDENTICAL));
}

void CDirSideBySideFilterBar::OnUpdateFilterOrphansL(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_SHOW_UNIQUE_LEFT));
}

void CDirSideBySideFilterBar::OnUpdateFilterOrphansR(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_SHOW_UNIQUE_RIGHT));
}

void CDirSideBySideFilterBar::OnUpdateFilterNewerL(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_SHOW_DIFFERENT_LEFT_ONLY));
}

void CDirSideBySideFilterBar::OnUpdateFilterNewerR(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_SHOW_DIFFERENT_RIGHT_ONLY));
}

void CDirSideBySideFilterBar::OnUpdateFilterSkipped(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_SHOW_SKIPPED));
}

void CDirSideBySideFilterBar::OnUpdateSuppressFilters(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_SUPPRESS_FILTERS));
}

/**
 * @brief Handle name filter edit losing focus — apply the filter pattern.
 * Also handles Enter key via PreTranslateMessage in the toolbar.
 */
void CDirSideBySideFilterBar::OnNameFilterChanged()
{
	if (!m_editNameFilter.GetSafeHwnd() || !m_pCoordinator)
		return;

	CString text;
	m_editNameFilter.GetWindowText(text);
	m_pCoordinator->SetNameFilter(String(text));
}

/**
 * @brief Open the advanced filter dialog by forwarding to the left pane view.
 */
void CDirSideBySideFilterBar::OnAdvancedFilter()
{
	// Forward to pane view which shows the dialog
	if (m_pCoordinator)
	{
		CDirPaneView* pPane = m_pCoordinator->GetLeftPaneView();
		if (pPane)
			pPane->SendMessage(WM_COMMAND, ID_DIR_SXS_ADV_FILTER);
	}
}
