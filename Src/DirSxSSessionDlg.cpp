/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSxSSessionDlg.cpp
 *
 * @brief Implementation of Session Settings dialog (6-tab property sheet)
 */

#include "StdAfx.h"
#include "DirSxSSessionDlg.h"
#include "DirSideBySideCoordinator.h"
#include "MergeApp.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Internal control IDs for the in-memory dialog controls
enum {
	IDC_SXS_LEFT_PATH = 7001,
	IDC_SXS_RIGHT_PATH,
	IDC_SXS_LEFT_READONLY,
	IDC_SXS_RIGHT_READONLY,
	IDC_SXS_DESCRIPTION,
	IDC_SXS_COMPARE_SIZE,
	IDC_SXS_COMPARE_TIMESTAMPS,
	IDC_SXS_TIME_TOLERANCE,
	IDC_SXS_COMPARE_CONTENTS,
	IDC_SXS_CONTENTS_CRC,
	IDC_SXS_CONTENTS_BINARY,
	IDC_SXS_CONTENTS_RULES,
	IDC_SXS_COMPARE_VERSIONS,
	IDC_SXS_COMPARE_CASE,
	IDC_SXS_EXPAND_ON_LOAD,
	IDC_SXS_EXPAND_DIFFS_ONLY,
	IDC_SXS_ARCHIVE_MODE,
	IDC_SXS_INCLUDE_FILES,
	IDC_SXS_EXCLUDE_FILES,
	IDC_SXS_INCLUDE_FOLDERS,
	IDC_SXS_EXCLUDE_FOLDERS,
	// Comparison tab: file attributes
	IDC_SXS_ATTR_ARCHIVE,
	IDC_SXS_ATTR_SYSTEM,
	IDC_SXS_ATTR_HIDDEN,
	IDC_SXS_ATTR_READONLY,
	IDC_SXS_ALIGN_DIFF_EXT,
	IDC_SXS_ALIGN_UNICODE,
	IDC_SXS_SKIP_IF_QUICK_SAME,
	IDC_SXS_OVERRIDE_QUICK,
	// Other Filters tab
	IDC_SXS_FILTER_RULES_LIST,
	IDC_SXS_FILTER_ADD,
	IDC_SXS_FILTER_REMOVE,
	IDC_SXS_EXCLUDE_OS_FILES,
	// Misc tab
	IDC_SXS_ALIGN_OVERRIDES_LIST,
	IDC_SXS_ALIGN_ADD,
	IDC_SXS_ALIGN_REMOVE,
	IDC_SXS_FILE_FORMATS_LIST,
};

// ============================================================================
// CSxSSessionPageBase
// ============================================================================

CSxSSessionPageBase::CSxSSessionPageBase()
{
	memset(m_dlgBuf, 0, sizeof(m_dlgBuf));
}

CSxSSessionPageBase::~CSxSSessionPageBase()
{
}

/**
 * @brief Build a minimal in-memory dialog template.
 */
DLGTEMPLATE* CSxSSessionPageBase::BuildTemplate(int width, int height, const wchar_t* title)
{
	memset(m_dlgBuf, 0, sizeof(m_dlgBuf));
	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)m_dlgBuf;
	pDlg->style = DS_SETFONT | WS_CHILD | WS_VISIBLE;
	pDlg->cdit = 0;
	pDlg->x = 0; pDlg->y = 0;
	pDlg->cx = (short)width; pDlg->cy = (short)height;

	WORD* pw = (WORD*)(pDlg + 1);
	*pw++ = 0; // menu
	*pw++ = 0; // class
	// title
	if (title)
	{
		size_t len = wcslen(title) + 1;
		memcpy(pw, title, len * sizeof(wchar_t));
		pw += len;
	}
	else
		*pw++ = 0;

	// Font (DS_SETFONT)
	*pw++ = 8; // point size
	const wchar_t fontName[] = L"MS Shell Dlg";
	memcpy(pw, fontName, sizeof(fontName));
	pw += _countof(fontName);

	return pDlg;
}

WORD* CSxSSessionPageBase::AddControl(WORD* pw, DWORD style, int x, int y, int cx, int cy,
	WORD id, WORD classAtom, const wchar_t* text)
{
	// Align to DWORD
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = style;
	pItem->x = (short)x; pItem->y = (short)y;
	pItem->cx = (short)cx; pItem->cy = (short)cy;
	pItem->id = id;

	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; // predefined class atom
	*pw++ = classAtom; // 0x0080=Button, 0x0081=Edit, 0x0082=Static, 0x0085=ComboBox
	// text
	if (text)
	{
		size_t len = wcslen(text) + 1;
		memcpy(pw, text, len * sizeof(wchar_t));
		pw += len;
	}
	else
		*pw++ = 0;
	*pw++ = 0; // creation data

	// Increment control count
	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)m_dlgBuf;
	pDlg->cdit++;

	return pw;
}

// ============================================================================
// Tab 1: Specs
// ============================================================================

CSxSSpecsPage::CSxSSpecsPage()
	: m_bLeftReadOnly(false), m_bRightReadOnly(false)
{
	m_psp.dwFlags |= PSP_DLGINDIRECT;
	BuildTemplate(300, 200, L"Specs");
	m_psp.pResource = (DLGTEMPLATE*)m_dlgBuf;
	m_psp.pszTitle = _T("Specs");
	m_psp.dwFlags |= PSP_USETITLE;
}

BOOL CSxSSpecsPage::OnInitDialog()
{
	CSxSSessionPageBase::OnInitDialog();

	// Create controls programmatically
	CRect rc;
	GetClientRect(&rc);
	int x = 10, y = 10, w = rc.Width() - 20;

	CStatic* pLabel;
	pLabel = new CStatic;
	pLabel->Create(_T("Left folder:"), WS_CHILD | WS_VISIBLE, CRect(x, y, x + 80, y + 14), this);
	y += 16;

	CEdit* pEditLeft = new CEdit;
	pEditLeft->Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(x, y, x + w, y + 18), this, IDC_SXS_LEFT_PATH);
	pEditLeft->SetWindowText(m_sLeftPath.c_str());
	y += 22;

	CButton* pChkLeftRO = new CButton;
	pChkLeftRO->Create(_T("Disable editing (read-only)"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x, y, x + w, y + 14), this, IDC_SXS_LEFT_READONLY);
	pChkLeftRO->SetCheck(m_bLeftReadOnly ? BST_CHECKED : BST_UNCHECKED);
	y += 24;

	pLabel = new CStatic;
	pLabel->Create(_T("Right folder:"), WS_CHILD | WS_VISIBLE, CRect(x, y, x + 80, y + 14), this);
	y += 16;

	CEdit* pEditRight = new CEdit;
	pEditRight->Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(x, y, x + w, y + 18), this, IDC_SXS_RIGHT_PATH);
	pEditRight->SetWindowText(m_sRightPath.c_str());
	y += 22;

	CButton* pChkRightRO = new CButton;
	pChkRightRO->Create(_T("Disable editing (read-only)"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x, y, x + w, y + 14), this, IDC_SXS_RIGHT_READONLY);
	pChkRightRO->SetCheck(m_bRightReadOnly ? BST_CHECKED : BST_UNCHECKED);
	y += 24;

	pLabel = new CStatic;
	pLabel->Create(_T("Description:"), WS_CHILD | WS_VISIBLE, CRect(x, y, x + 80, y + 14), this);
	y += 16;

	CEdit* pEditDesc = new CEdit;
	pEditDesc->Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
		CRect(x, y, x + w, y + 50), this, IDC_SXS_DESCRIPTION);
	pEditDesc->SetWindowText(m_sDescription.c_str());

	return TRUE;
}

void CSxSSpecsPage::OnOK()
{
	CString text;
	GetDlgItemText(IDC_SXS_LEFT_PATH, text);
	m_sLeftPath = (LPCTSTR)text;
	GetDlgItemText(IDC_SXS_RIGHT_PATH, text);
	m_sRightPath = (LPCTSTR)text;
	GetDlgItemText(IDC_SXS_DESCRIPTION, text);
	m_sDescription = (LPCTSTR)text;
	m_bLeftReadOnly = (IsDlgButtonChecked(IDC_SXS_LEFT_READONLY) == BST_CHECKED);
	m_bRightReadOnly = (IsDlgButtonChecked(IDC_SXS_RIGHT_READONLY) == BST_CHECKED);
	CSxSSessionPageBase::OnOK();
}

// ============================================================================
// Tab 2: Comparison
// ============================================================================

CSxSComparisonPage::CSxSComparisonPage()
	: m_bCompareSize(true), m_bCompareTimestamps(true)
	, m_bCompareContents(false), m_bCompareVersions(false)
	, m_bCompareCase(false), m_nContentsMode(0), m_nTimeTolerance(2)
	, m_bAttrArchive(true), m_bAttrSystem(true), m_bAttrHidden(true), m_bAttrReadOnly(true)
	, m_bAlignDiffExt(false), m_bAlignUnicode(true), m_bSkipIfQuickSame(false), m_bOverrideQuick(true)
{
	m_psp.dwFlags |= PSP_DLGINDIRECT;
	BuildTemplate(300, 200, L"Comparison");
	m_psp.pResource = (DLGTEMPLATE*)m_dlgBuf;
	m_psp.pszTitle = _T("Comparison");
	m_psp.dwFlags |= PSP_USETITLE;
}

BOOL CSxSComparisonPage::OnInitDialog()
{
	CSxSSessionPageBase::OnInitDialog();

	CRect rc;
	GetClientRect(&rc);
	int x = 10, y = 10, w = rc.Width() - 20;
	int halfW = (rc.Width() - 30) / 2;  // half-width for two-column layout
	int x2 = x + halfW + 10;            // right column x origin

	// --- Left column: Quick tests ---
	CStatic* pGroup = new CStatic;
	pGroup->Create(_T("Quick tests:"), WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(x, y, x + halfW, y + 14), this);

	// --- Right column: File attributes (same starting y) ---
	CStatic* pAttrGroup = new CStatic;
	pAttrGroup->Create(_T("Compare file attributes:"), WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(x2, y, x2 + halfW, y + 14), this);
	y += 18;

	CButton* pChk;
	pChk = new CButton;
	pChk->Create(_T("Compare file size"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x + 10, y, x + halfW, y + 14), this, IDC_SXS_COMPARE_SIZE);
	pChk->SetCheck(m_bCompareSize ? BST_CHECKED : BST_UNCHECKED);

	pChk = new CButton;
	pChk->Create(_T("Archive"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x2 + 10, y, x2 + halfW, y + 14), this, IDC_SXS_ATTR_ARCHIVE);
	pChk->SetCheck(m_bAttrArchive ? BST_CHECKED : BST_UNCHECKED);
	y += 18;

	pChk = new CButton;
	pChk->Create(_T("Compare timestamps"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x + 10, y, x + halfW, y + 14), this, IDC_SXS_COMPARE_TIMESTAMPS);
	pChk->SetCheck(m_bCompareTimestamps ? BST_CHECKED : BST_UNCHECKED);

	pChk = new CButton;
	pChk->Create(_T("System"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x2 + 10, y, x2 + halfW, y + 14), this, IDC_SXS_ATTR_SYSTEM);
	pChk->SetCheck(m_bAttrSystem ? BST_CHECKED : BST_UNCHECKED);
	y += 18;

	CStatic* pLabel = new CStatic;
	pLabel->Create(_T("Time tolerance"), WS_CHILD | WS_VISIBLE, CRect(x + 20, y, x + 110, y + 14), this);
	CEdit* pEdit = new CEdit;
	pEdit->Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, CRect(x + 115, y, x + 155, y + 16), this, IDC_SXS_TIME_TOLERANCE);
	CString sTol;
	sTol.Format(_T("%d"), m_nTimeTolerance);
	pEdit->SetWindowText(sTol);

	pChk = new CButton;
	pChk->Create(_T("Hidden"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x2 + 10, y, x2 + halfW, y + 14), this, IDC_SXS_ATTR_HIDDEN);
	pChk->SetCheck(m_bAttrHidden ? BST_CHECKED : BST_UNCHECKED);
	y += 20;

	pChk = new CButton;
	pChk->Create(_T("Compare filename case"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x + 10, y, x + halfW, y + 14), this, IDC_SXS_COMPARE_CASE);
	pChk->SetCheck(m_bCompareCase ? BST_CHECKED : BST_UNCHECKED);

	pChk = new CButton;
	pChk->Create(_T("Read-only"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x2 + 10, y, x2 + halfW, y + 14), this, IDC_SXS_ATTR_READONLY);
	pChk->SetCheck(m_bAttrReadOnly ? BST_CHECKED : BST_UNCHECKED);
	y += 24;

	// --- Requires opening files section (full width, below both columns) ---
	pGroup = new CStatic;
	pGroup->Create(_T("Requires opening files:"), WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(x, y, x + w, y + 14), this);
	y += 18;

	pChk = new CButton;
	pChk->Create(_T("Compare contents"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x + 10, y, x + w, y + 14), this, IDC_SXS_COMPARE_CONTENTS);
	pChk->SetCheck(m_bCompareContents ? BST_CHECKED : BST_UNCHECKED);
	y += 18;

	// Radio buttons for contents mode
	CButton* pRadio;
	pRadio = new CButton;
	pRadio->Create(_T("CRC"), WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, CRect(x + 20, y, x + 80, y + 14), this, IDC_SXS_CONTENTS_CRC);
	pRadio->SetCheck(m_nContentsMode == 0 ? BST_CHECKED : BST_UNCHECKED);
	pRadio = new CButton;
	pRadio->Create(_T("Binary"), WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, CRect(x + 85, y, x + 150, y + 14), this, IDC_SXS_CONTENTS_BINARY);
	pRadio->SetCheck(m_nContentsMode == 1 ? BST_CHECKED : BST_UNCHECKED);
	pRadio = new CButton;
	pRadio->Create(_T("Rules-based"), WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, CRect(x + 155, y, x + 250, y + 14), this, IDC_SXS_CONTENTS_RULES);
	pRadio->SetCheck(m_nContentsMode == 2 ? BST_CHECKED : BST_UNCHECKED);
	y += 22;

	pChk = new CButton;
	pChk->Create(_T("Compare versions"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x + 10, y, x + w, y + 14), this, IDC_SXS_COMPARE_VERSIONS);
	pChk->SetCheck(m_bCompareVersions ? BST_CHECKED : BST_UNCHECKED);
	y += 28;

	// --- Additional options (full width) ---
	pChk = new CButton;
	pChk->Create(_T("Align filenames with different extensions"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x, y, x + w, y + 14), this, IDC_SXS_ALIGN_DIFF_EXT);
	pChk->SetCheck(m_bAlignDiffExt ? BST_CHECKED : BST_UNCHECKED);
	y += 18;

	pChk = new CButton;
	pChk->Create(_T("Align filenames with different Unicode normalization forms"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x, y, x + w, y + 14), this, IDC_SXS_ALIGN_UNICODE);
	pChk->SetCheck(m_bAlignUnicode ? BST_CHECKED : BST_UNCHECKED);
	y += 18;

	pChk = new CButton;
	pChk->Create(_T("Skip if quick tests indicate files are the same"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x, y, x + w, y + 14), this, IDC_SXS_SKIP_IF_QUICK_SAME);
	pChk->SetCheck(m_bSkipIfQuickSame ? BST_CHECKED : BST_UNCHECKED);
	y += 18;

	pChk = new CButton;
	pChk->Create(_T("Override quick test results"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x, y, x + w, y + 14), this, IDC_SXS_OVERRIDE_QUICK);
	pChk->SetCheck(m_bOverrideQuick ? BST_CHECKED : BST_UNCHECKED);

	return TRUE;
}

void CSxSComparisonPage::OnOK()
{
	m_bCompareSize = (IsDlgButtonChecked(IDC_SXS_COMPARE_SIZE) == BST_CHECKED);
	m_bCompareTimestamps = (IsDlgButtonChecked(IDC_SXS_COMPARE_TIMESTAMPS) == BST_CHECKED);
	m_bCompareContents = (IsDlgButtonChecked(IDC_SXS_COMPARE_CONTENTS) == BST_CHECKED);
	m_bCompareVersions = (IsDlgButtonChecked(IDC_SXS_COMPARE_VERSIONS) == BST_CHECKED);
	m_bCompareCase = (IsDlgButtonChecked(IDC_SXS_COMPARE_CASE) == BST_CHECKED);

	if (IsDlgButtonChecked(IDC_SXS_CONTENTS_CRC) == BST_CHECKED) m_nContentsMode = 0;
	else if (IsDlgButtonChecked(IDC_SXS_CONTENTS_BINARY) == BST_CHECKED) m_nContentsMode = 1;
	else m_nContentsMode = 2;

	CString sTol;
	GetDlgItemText(IDC_SXS_TIME_TOLERANCE, sTol);
	m_nTimeTolerance = _ttoi(sTol);

	// File attribute checkboxes
	m_bAttrArchive = (IsDlgButtonChecked(IDC_SXS_ATTR_ARCHIVE) == BST_CHECKED);
	m_bAttrSystem = (IsDlgButtonChecked(IDC_SXS_ATTR_SYSTEM) == BST_CHECKED);
	m_bAttrHidden = (IsDlgButtonChecked(IDC_SXS_ATTR_HIDDEN) == BST_CHECKED);
	m_bAttrReadOnly = (IsDlgButtonChecked(IDC_SXS_ATTR_READONLY) == BST_CHECKED);

	// Additional BC-style options
	m_bAlignDiffExt = (IsDlgButtonChecked(IDC_SXS_ALIGN_DIFF_EXT) == BST_CHECKED);
	m_bAlignUnicode = (IsDlgButtonChecked(IDC_SXS_ALIGN_UNICODE) == BST_CHECKED);
	m_bSkipIfQuickSame = (IsDlgButtonChecked(IDC_SXS_SKIP_IF_QUICK_SAME) == BST_CHECKED);
	m_bOverrideQuick = (IsDlgButtonChecked(IDC_SXS_OVERRIDE_QUICK) == BST_CHECKED);

	CSxSSessionPageBase::OnOK();
}

// ============================================================================
// Tab 3: Handling
// ============================================================================

CSxSHandlingPage::CSxSHandlingPage()
	: m_bExpandOnLoad(true), m_bExpandDiffsOnly(false), m_nArchiveMode(0)
{
	m_psp.dwFlags |= PSP_DLGINDIRECT;
	BuildTemplate(300, 200, L"Handling");
	m_psp.pResource = (DLGTEMPLATE*)m_dlgBuf;
	m_psp.pszTitle = _T("Handling");
	m_psp.dwFlags |= PSP_USETITLE;
}

BOOL CSxSHandlingPage::OnInitDialog()
{
	CSxSSessionPageBase::OnInitDialog();

	CRect rc;
	GetClientRect(&rc);
	int x = 10, y = 10, w = rc.Width() - 20;

	CStatic* pGroup = new CStatic;
	pGroup->Create(_T("Folder handling:"), WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(x, y, x + w, y + 14), this);
	y += 18;

	CButton* pChk;
	pChk = new CButton;
	pChk->Create(_T("Expand on load"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x + 10, y, x + w, y + 14), this, IDC_SXS_EXPAND_ON_LOAD);
	pChk->SetCheck(m_bExpandOnLoad ? BST_CHECKED : BST_UNCHECKED);
	y += 18;

	pChk = new CButton;
	pChk->Create(_T("Only expand folders with differences"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(x + 20, y, x + w, y + 14), this, IDC_SXS_EXPAND_DIFFS_ONLY);
	pChk->SetCheck(m_bExpandDiffsOnly ? BST_CHECKED : BST_UNCHECKED);
	y += 28;

	pGroup = new CStatic;
	pGroup->Create(_T("Archive handling:"), WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(x, y, x + w, y + 14), this);
	y += 18;

	CComboBox* pCombo = new CComboBox;
	pCombo->Create(WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST, CRect(x + 10, y, x + 220, y + 100), this, IDC_SXS_ARCHIVE_MODE);
	pCombo->AddString(_T("Never handle archives"));
	pCombo->AddString(_T("As folders once opened"));
	pCombo->AddString(_T("Always treat as folders"));
	pCombo->SetCurSel(m_nArchiveMode);

	return TRUE;
}

void CSxSHandlingPage::OnOK()
{
	m_bExpandOnLoad = (IsDlgButtonChecked(IDC_SXS_EXPAND_ON_LOAD) == BST_CHECKED);
	m_bExpandDiffsOnly = (IsDlgButtonChecked(IDC_SXS_EXPAND_DIFFS_ONLY) == BST_CHECKED);

	CComboBox* pCombo = (CComboBox*)GetDlgItem(IDC_SXS_ARCHIVE_MODE);
	if (pCombo)
		m_nArchiveMode = pCombo->GetCurSel();

	CSxSSessionPageBase::OnOK();
}

// ============================================================================
// Tab 4: Name Filters
// ============================================================================

CSxSNameFiltersPage::CSxSNameFiltersPage()
{
	m_psp.dwFlags |= PSP_DLGINDIRECT;
	BuildTemplate(300, 200, L"Name Filters");
	m_psp.pResource = (DLGTEMPLATE*)m_dlgBuf;
	m_psp.pszTitle = _T("Name Filters");
	m_psp.dwFlags |= PSP_USETITLE;
}

BOOL CSxSNameFiltersPage::OnInitDialog()
{
	CSxSSessionPageBase::OnInitDialog();

	CRect rc;
	GetClientRect(&rc);
	int x = 10, y = 10;
	int halfW = (rc.Width() - 30) / 2;
	int editH = 50;

	// Include files (top-left)
	CStatic* pLabel;
	pLabel = new CStatic;
	pLabel->Create(_T("Include files:"), WS_CHILD | WS_VISIBLE, CRect(x, y, x + halfW, y + 14), this);
	y += 16;

	CEdit* pEdit;
	pEdit = new CEdit;
	pEdit->Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
		CRect(x, y, x + halfW, y + editH), this, IDC_SXS_INCLUDE_FILES);
	pEdit->SetWindowText(m_sIncludeFiles.c_str());

	// Exclude files (top-right)
	int x2 = x + halfW + 10;
	pLabel = new CStatic;
	pLabel->Create(_T("Exclude files:"), WS_CHILD | WS_VISIBLE, CRect(x2, y - 16, x2 + halfW, y - 2), this);

	pEdit = new CEdit;
	pEdit->Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
		CRect(x2, y, x2 + halfW, y + editH), this, IDC_SXS_EXCLUDE_FILES);
	pEdit->SetWindowText(m_sExcludeFiles.c_str());
	y += editH + 16;

	// Include folders (bottom-left)
	pLabel = new CStatic;
	pLabel->Create(_T("Include folders:"), WS_CHILD | WS_VISIBLE, CRect(x, y, x + halfW, y + 14), this);
	y += 16;

	pEdit = new CEdit;
	pEdit->Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
		CRect(x, y, x + halfW, y + editH), this, IDC_SXS_INCLUDE_FOLDERS);
	pEdit->SetWindowText(m_sIncludeFolders.c_str());

	// Exclude folders (bottom-right)
	pLabel = new CStatic;
	pLabel->Create(_T("Exclude folders:"), WS_CHILD | WS_VISIBLE, CRect(x2, y - 16, x2 + halfW, y - 2), this);

	pEdit = new CEdit;
	pEdit->Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
		CRect(x2, y, x2 + halfW, y + editH), this, IDC_SXS_EXCLUDE_FOLDERS);
	pEdit->SetWindowText(m_sExcludeFolders.c_str());

	return TRUE;
}

void CSxSNameFiltersPage::OnOK()
{
	CString text;
	GetDlgItemText(IDC_SXS_INCLUDE_FILES, text);
	m_sIncludeFiles = (LPCTSTR)text;
	GetDlgItemText(IDC_SXS_EXCLUDE_FILES, text);
	m_sExcludeFiles = (LPCTSTR)text;
	GetDlgItemText(IDC_SXS_INCLUDE_FOLDERS, text);
	m_sIncludeFolders = (LPCTSTR)text;
	GetDlgItemText(IDC_SXS_EXCLUDE_FOLDERS, text);
	m_sExcludeFolders = (LPCTSTR)text;
	CSxSSessionPageBase::OnOK();
}

// ============================================================================
// Tab 5: Other Filters
// ============================================================================

CSxSOtherFiltersPage::CSxSOtherFiltersPage()
	: m_bExcludeOsFiles(true)
{
	m_psp.dwFlags |= PSP_DLGINDIRECT;
	BuildTemplate(300, 200, L"Other Filters");
	m_psp.pResource = (DLGTEMPLATE*)m_dlgBuf;
	m_psp.pszTitle = _T("Other Filters");
	m_psp.dwFlags |= PSP_USETITLE;
}

BOOL CSxSOtherFiltersPage::OnInitDialog()
{
	CSxSSessionPageBase::OnInitDialog();

	CRect rc;
	GetClientRect(&rc);
	int x = 10, y = 10, w = rc.Width() - 20;

	// Label
	CStatic* pLabel = new CStatic;
	pLabel->Create(_T("Filter rules:"), WS_CHILD | WS_VISIBLE | SS_LEFT,
		CRect(x, y, x + w, y + 14), this);
	y += 18;

	// Listbox for filter rules
	int listH = rc.Height() - 90;
	CListBox* pList = new CListBox;
	pList->Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
		CRect(x, y, x + w, y + listH), this, IDC_SXS_FILTER_RULES_LIST);

	// Populate with existing rules
	for (const auto& rule : m_filterRules)
		pList->AddString(rule.c_str());

	y += listH + 4;

	// Add / Remove buttons
	int btnW = 60;
	CButton* pBtn = new CButton;
	pBtn->Create(_T("Add..."), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		CRect(x, y, x + btnW, y + 20), this, IDC_SXS_FILTER_ADD);

	pBtn = new CButton;
	pBtn->Create(_T("Remove"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		CRect(x + btnW + 6, y, x + 2 * btnW + 6, y + 20), this, IDC_SXS_FILTER_REMOVE);

	y += 26;

	// Exclude OS files checkbox
	CButton* pChk = new CButton;
	pChk->Create(_T("Exclude protected operating system files (Recommended)"),
		WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
		CRect(x, y, x + w, y + 14), this, IDC_SXS_EXCLUDE_OS_FILES);
	pChk->SetCheck(m_bExcludeOsFiles ? BST_CHECKED : BST_UNCHECKED);

	return TRUE;
}

void CSxSOtherFiltersPage::OnOK()
{
	m_bExcludeOsFiles = (IsDlgButtonChecked(IDC_SXS_EXCLUDE_OS_FILES) == BST_CHECKED);

	// Read rules from listbox
	CListBox* pList = (CListBox*)GetDlgItem(IDC_SXS_FILTER_RULES_LIST);
	if (pList)
	{
		m_filterRules.clear();
		int count = pList->GetCount();
		for (int i = 0; i < count; i++)
		{
			CString text;
			pList->GetText(i, text);
			m_filterRules.push_back((LPCTSTR)text);
		}
	}

	CSxSSessionPageBase::OnOK();
}

// ============================================================================
// Tab 6: Misc
// ============================================================================

CSxSMiscPage::CSxSMiscPage()
{
	m_psp.dwFlags |= PSP_DLGINDIRECT;
	BuildTemplate(300, 200, L"Misc");
	m_psp.pResource = (DLGTEMPLATE*)m_dlgBuf;
	m_psp.pszTitle = _T("Misc");
	m_psp.dwFlags |= PSP_USETITLE;
}

BOOL CSxSMiscPage::OnInitDialog()
{
	CSxSSessionPageBase::OnInitDialog();

	CRect rc;
	GetClientRect(&rc);
	int x = 10, y = 10, w = rc.Width() - 20;

	// --- Top half: Alignment overrides ---
	CStatic* pLabel = new CStatic;
	pLabel->Create(_T("Alignment overrides:"), WS_CHILD | WS_VISIBLE | SS_LEFT,
		CRect(x, y, x + w, y + 14), this);
	y += 18;

	int halfH = (rc.Height() - 40) / 2 - 30;
	CListBox* pAlignList = new CListBox;
	pAlignList->Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
		CRect(x, y, x + w, y + halfH), this, IDC_SXS_ALIGN_OVERRIDES_LIST);

	// Populate with existing alignment overrides
	for (const auto& entry : m_alignmentOverrides)
		pAlignList->AddString(entry.c_str());

	y += halfH + 4;

	// Add / Remove buttons for alignment overrides
	int btnW = 60;
	CButton* pBtn = new CButton;
	pBtn->Create(_T("Add..."), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		CRect(x, y, x + btnW, y + 20), this, IDC_SXS_ALIGN_ADD);

	pBtn = new CButton;
	pBtn->Create(_T("Remove"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		CRect(x + btnW + 6, y, x + 2 * btnW + 6, y + 20), this, IDC_SXS_ALIGN_REMOVE);

	y += 28;

	// --- Bottom half: Enabled file formats ---
	pLabel = new CStatic;
	pLabel->Create(_T("Enabled file formats:"), WS_CHILD | WS_VISIBLE | SS_LEFT,
		CRect(x, y, x + w, y + 14), this);
	y += 18;

	int remainH = rc.Height() - y - 10;
	CListCtrl* pFmtList = new CListCtrl;
	pFmtList->Create(WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
		CRect(x, y, x + w, y + remainH), this, IDC_SXS_FILE_FORMATS_LIST);

	pFmtList->SetExtendedStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
	pFmtList->InsertColumn(0, _T("Name"), LVCFMT_LEFT, w / 2 - 10);
	pFmtList->InsertColumn(1, _T("Mask"), LVCFMT_LEFT, w / 2 - 10);

	// Predefined file format categories
	struct { const TCHAR* name; const TCHAR* mask; } formats[] = {
		{ _T("C/C++/C#/ObjC Source"), _T("*.c;*.cc;*.cpp;*.cs;*.h;*.hpp;*.m") },
		{ _T("Java Source"), _T("*.jav;*.java") },
		{ _T("JavaScript"), _T("*.js;*.jsx;*.ts;*.tsx") },
		{ _T("Python Source"), _T("*.py;*.pyw") },
		{ _T("HTML/Web"), _T("*.htm;*.html;*.asp;*.aspx;*.ascx") },
		{ _T("CSS/SCSS/LESS"), _T("*.css;*.scss;*.less;*.sass") },
		{ _T("XML/XSLT"), _T("*.xml;*.xsl;*.xslt;*.xsd") },
		{ _T("JSON/YAML"), _T("*.json;*.yaml;*.yml") },
		{ _T("SQL"), _T("*.sql;*.ddl;*.dml") },
		{ _T("Shell Scripts"), _T("*.sh;*.bash;*.bat;*.cmd;*.ps1") },
		{ _T("Rust Source"), _T("*.rs") },
		{ _T("Go Source"), _T("*.go") },
		{ _T("Ruby Source"), _T("*.rb;*.rake") },
		{ _T("PHP Source"), _T("*.php;*.phtml") },
		{ _T("INI/Config"), _T("*.ini;*.cfg;*.conf;*.properties") },
	};

	for (int i = 0; i < _countof(formats); i++)
	{
		int idx = pFmtList->InsertItem(i, formats[i].name);
		pFmtList->SetItemText(idx, 1, formats[i].mask);
		pFmtList->SetCheck(idx, TRUE); // All enabled by default
	}

	return TRUE;
}

void CSxSMiscPage::OnOK()
{
	// Read alignment overrides from listbox
	CListBox* pList = (CListBox*)GetDlgItem(IDC_SXS_ALIGN_OVERRIDES_LIST);
	if (pList)
	{
		m_alignmentOverrides.clear();
		int count = pList->GetCount();
		for (int i = 0; i < count; i++)
		{
			CString text;
			pList->GetText(i, text);
			m_alignmentOverrides.push_back((LPCTSTR)text);
		}
	}

	CSxSSessionPageBase::OnOK();
}

// ============================================================================
// CDirSxSSessionDlg
// ============================================================================

CDirSxSSessionDlg::CDirSxSSessionDlg(CWnd* pParent, CDirSideBySideCoordinator* pCoord)
	: CPropertySheet(_T("Session Settings - Folder Compare"), pParent)
	, m_pCoordinator(pCoord)
{
	AddPage(&m_pageSpecs);
	AddPage(&m_pageComparison);
	AddPage(&m_pageHandling);
	AddPage(&m_pageNameFilters);
	AddPage(&m_pageOtherFilters);
	AddPage(&m_pageMisc);
}

CDirSxSSessionDlg::~CDirSxSSessionDlg()
{
}

void CDirSxSSessionDlg::LoadFromOptions()
{
	// Specs
	m_pageSpecs.m_bLeftReadOnly = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_LEFT_READONLY);
	m_pageSpecs.m_bRightReadOnly = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_RIGHT_READONLY);
	m_pageSpecs.m_sDescription = GetOptionsMgr()->GetString(OPT_DIRVIEW_SXS_DESCRIPTION);

	// Comparison
	m_pageComparison.m_bCompareSize = GetOptionsMgr()->GetBool(OPT_CMP_SXS_COMPARE_SIZE);
	m_pageComparison.m_bCompareTimestamps = GetOptionsMgr()->GetBool(OPT_CMP_SXS_COMPARE_TIMESTAMPS);
	m_pageComparison.m_bCompareContents = GetOptionsMgr()->GetBool(OPT_CMP_SXS_COMPARE_CONTENTS);
	m_pageComparison.m_nContentsMode = GetOptionsMgr()->GetInt(OPT_CMP_SXS_CONTENTS_MODE);
	m_pageComparison.m_bCompareVersions = GetOptionsMgr()->GetBool(OPT_CMP_SXS_COMPARE_VERSIONS);
	m_pageComparison.m_bCompareCase = GetOptionsMgr()->GetBool(OPT_CMP_SXS_COMPARE_CASE);
	m_pageComparison.m_nTimeTolerance = GetOptionsMgr()->GetInt(OPT_CMP_IGNORE_SMALL_FILETIME_SECS);

	// Comparison: file attributes
	m_pageComparison.m_bAttrArchive = GetOptionsMgr()->GetBool(OPT_CMP_SXS_ATTR_ARCHIVE);
	m_pageComparison.m_bAttrSystem = GetOptionsMgr()->GetBool(OPT_CMP_SXS_ATTR_SYSTEM);
	m_pageComparison.m_bAttrHidden = GetOptionsMgr()->GetBool(OPT_CMP_SXS_ATTR_HIDDEN);
	m_pageComparison.m_bAttrReadOnly = GetOptionsMgr()->GetBool(OPT_CMP_SXS_ATTR_READONLY);

	// Comparison: additional BC-style options
	m_pageComparison.m_bAlignDiffExt = GetOptionsMgr()->GetBool(OPT_CMP_SXS_ALIGN_DIFF_EXT);
	m_pageComparison.m_bAlignUnicode = GetOptionsMgr()->GetBool(OPT_CMP_SXS_ALIGN_UNICODE);
	m_pageComparison.m_bSkipIfQuickSame = GetOptionsMgr()->GetBool(OPT_CMP_SXS_SKIP_IF_QUICK_SAME);
	m_pageComparison.m_bOverrideQuick = GetOptionsMgr()->GetBool(OPT_CMP_SXS_OVERRIDE_QUICK);

	// Handling
	m_pageHandling.m_bExpandOnLoad = GetOptionsMgr()->GetBool(OPT_CMP_SXS_EXPAND_ON_LOAD);
	m_pageHandling.m_bExpandDiffsOnly = GetOptionsMgr()->GetBool(OPT_CMP_SXS_EXPAND_DIFFS_ONLY);
	m_pageHandling.m_nArchiveMode = GetOptionsMgr()->GetInt(OPT_CMP_SXS_ARCHIVE_MODE);

	// Name filters
	m_pageNameFilters.m_sIncludeFiles = GetOptionsMgr()->GetString(OPT_DIRVIEW_SXS_INCLUDE_FILES);
	m_pageNameFilters.m_sExcludeFiles = GetOptionsMgr()->GetString(OPT_DIRVIEW_SXS_EXCLUDE_FILES);
	m_pageNameFilters.m_sIncludeFolders = GetOptionsMgr()->GetString(OPT_DIRVIEW_SXS_INCLUDE_FOLDERS);
	m_pageNameFilters.m_sExcludeFolders = GetOptionsMgr()->GetString(OPT_DIRVIEW_SXS_EXCLUDE_FOLDERS);

	// Other filters
	m_pageOtherFilters.m_bExcludeOsFiles = GetOptionsMgr()->GetBool(OPT_CMP_SXS_EXCLUDE_OS_FILES);
}

void CDirSxSSessionDlg::SaveToOptions()
{
	// Specs
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_LEFT_READONLY, m_pageSpecs.m_bLeftReadOnly);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_RIGHT_READONLY, m_pageSpecs.m_bRightReadOnly);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_DESCRIPTION, m_pageSpecs.m_sDescription);

	// Comparison
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_COMPARE_SIZE, m_pageComparison.m_bCompareSize);
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_COMPARE_TIMESTAMPS, m_pageComparison.m_bCompareTimestamps);
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_COMPARE_CONTENTS, m_pageComparison.m_bCompareContents);
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_CONTENTS_MODE, m_pageComparison.m_nContentsMode);
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_COMPARE_VERSIONS, m_pageComparison.m_bCompareVersions);
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_COMPARE_CASE, m_pageComparison.m_bCompareCase);
	GetOptionsMgr()->SaveOption(OPT_CMP_IGNORE_SMALL_FILETIME_SECS, m_pageComparison.m_nTimeTolerance);

	// Comparison: file attributes
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_ATTR_ARCHIVE, m_pageComparison.m_bAttrArchive);
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_ATTR_SYSTEM, m_pageComparison.m_bAttrSystem);
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_ATTR_HIDDEN, m_pageComparison.m_bAttrHidden);
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_ATTR_READONLY, m_pageComparison.m_bAttrReadOnly);

	// Comparison: additional BC-style options
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_ALIGN_DIFF_EXT, m_pageComparison.m_bAlignDiffExt);
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_ALIGN_UNICODE, m_pageComparison.m_bAlignUnicode);
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_SKIP_IF_QUICK_SAME, m_pageComparison.m_bSkipIfQuickSame);
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_OVERRIDE_QUICK, m_pageComparison.m_bOverrideQuick);

	// Handling
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_EXPAND_ON_LOAD, m_pageHandling.m_bExpandOnLoad);
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_EXPAND_DIFFS_ONLY, m_pageHandling.m_bExpandDiffsOnly);
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_ARCHIVE_MODE, m_pageHandling.m_nArchiveMode);

	// Name filters
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_INCLUDE_FILES, m_pageNameFilters.m_sIncludeFiles);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_EXCLUDE_FILES, m_pageNameFilters.m_sExcludeFiles);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_INCLUDE_FOLDERS, m_pageNameFilters.m_sIncludeFolders);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_EXCLUDE_FOLDERS, m_pageNameFilters.m_sExcludeFolders);

	// Other filters
	GetOptionsMgr()->SaveOption(OPT_CMP_SXS_EXCLUDE_OS_FILES, m_pageOtherFilters.m_bExcludeOsFiles);
}
