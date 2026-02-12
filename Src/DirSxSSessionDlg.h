/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSxSSessionDlg.h
 *
 * @brief Declaration of Session Settings dialog (6-tab property sheet)
 */
#pragma once

#include <afxdlgs.h>
#include <vector>
#include "UnicodeString.h"

class CDirSideBySideCoordinator;

/**
 * @brief Base class for session settings pages using in-memory dialog templates.
 */
class CSxSSessionPageBase : public CPropertyPage
{
protected:
	CSxSSessionPageBase();
	virtual ~CSxSSessionPageBase();

	// Each derived page builds its dialog template into this buffer
	BYTE m_dlgBuf[2048];
	DLGTEMPLATE* BuildTemplate(int width, int height, const wchar_t* title);
	WORD* AddControl(WORD* pw, DWORD style, int x, int y, int cx, int cy,
		WORD id, WORD classAtom, const wchar_t* text);
};

// --- Tab 1: Specs ---
class CSxSSpecsPage : public CSxSSessionPageBase
{
public:
	CSxSSpecsPage();
	String m_sLeftPath, m_sRightPath, m_sDescription;
	bool m_bLeftReadOnly, m_bRightReadOnly;
protected:
	BOOL OnInitDialog() override;
	void OnOK() override;
};

// --- Tab 2: Comparison ---
class CSxSComparisonPage : public CSxSSessionPageBase
{
public:
	CSxSComparisonPage();
	bool m_bCompareSize, m_bCompareTimestamps, m_bCompareContents;
	bool m_bCompareVersions, m_bCompareCase;
	int m_nContentsMode; // 0=CRC, 1=Binary, 2=Rules-based
	int m_nTimeTolerance;
	// File attribute comparison
	bool m_bAttrArchive, m_bAttrSystem, m_bAttrHidden, m_bAttrReadOnly;
	// Additional BC-style options
	bool m_bAlignDiffExt, m_bAlignUnicode, m_bSkipIfQuickSame, m_bOverrideQuick;
protected:
	BOOL OnInitDialog() override;
	void OnOK() override;
};

// --- Tab 3: Handling ---
class CSxSHandlingPage : public CSxSSessionPageBase
{
public:
	CSxSHandlingPage();
	bool m_bExpandOnLoad, m_bExpandDiffsOnly;
	int m_nArchiveMode; // 0=never, 1=once opened, 2=always
protected:
	BOOL OnInitDialog() override;
	void OnOK() override;
};

// --- Tab 4: Name Filters ---
class CSxSNameFiltersPage : public CSxSSessionPageBase
{
public:
	CSxSNameFiltersPage();
	String m_sIncludeFiles, m_sExcludeFiles;
	String m_sIncludeFolders, m_sExcludeFolders;
protected:
	BOOL OnInitDialog() override;
	void OnOK() override;
};

// --- Tab 5: Other Filters ---
class CSxSOtherFiltersPage : public CSxSSessionPageBase
{
public:
	CSxSOtherFiltersPage();
	bool m_bExcludeOsFiles;
	std::vector<String> m_filterRules;
protected:
	BOOL OnInitDialog() override;
	void OnOK() override;
};

// --- Tab 6: Misc ---
class CSxSMiscPage : public CSxSSessionPageBase
{
public:
	CSxSMiscPage();
	std::vector<String> m_alignmentOverrides;
protected:
	BOOL OnInitDialog() override;
	void OnOK() override;
};

/**
 * @brief Session Settings property sheet with 6 tabs.
 */
class CDirSxSSessionDlg : public CPropertySheet
{
public:
	CDirSxSSessionDlg(CWnd* pParent, CDirSideBySideCoordinator* pCoord);
	virtual ~CDirSxSSessionDlg();

	/** Load settings from options into page data members */
	void LoadFromOptions();
	/** Save page data back to options */
	void SaveToOptions();

	CDirSideBySideCoordinator* m_pCoordinator;
public:
	CSxSSpecsPage m_pageSpecs;
private:
	CSxSComparisonPage m_pageComparison;
	CSxSHandlingPage m_pageHandling;
	CSxSNameFiltersPage m_pageNameFilters;
	CSxSOtherFiltersPage m_pageOtherFilters;
	CSxSMiscPage m_pageMisc;
};
