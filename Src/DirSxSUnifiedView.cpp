/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSxSUnifiedView.cpp
 *
 * @brief Implementation of CDirSxSUnifiedView — single unified 7-column list
 *        for side-by-side folder comparison (Beyond Compare-style).
 *
 * Columns: Name(L) | Size(L) | Modified(L) | Cmp | Name(R) | Size(R) | Modified(R)
 */

#include "StdAfx.h"
#include "DirSxSUnifiedView.h"
#include "DirDoc.h"
#include "DirFrame.h"
#include "DirSideBySideCoordinator.h"
#include "DiffThread.h"
#include "DirViewColItems.h"
#include "DiffContext.h"
#include "DiffItem.h"
#include "Merge.h"
#include "SyntaxColors.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "OptionsDirColors.h"
#include "resource.h"
#include "DirActions.h"
#include "IListCtrlImpl.h"
#include "MainFrm.h"
#include "FileLocation.h"
#include "FileTransform.h"
#include "paths.h"
#include "ShellFileOperations.h"
#include "MergeApp.h"
#include "FolderCmp.h"
#include "CompareStats.h"
#include <afxole.h>
#include <Shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <Shlwapi.h>
#include <fstream>
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "version.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Beyond Compare dark theme color palette
namespace BcColors
{
	// Core dark theme backgrounds
	static const COLORREF BG_DARK      = RGB(30, 33, 33);    // List even-row bg
	static const COLORREF BG_ALT       = RGB(38, 42, 42);    // List odd-row bg (stripe)
	static const COLORREF TOOLBAR_BG   = RGB(45, 48, 50);    // Toolbar/filter bar bg
	static const COLORREF HEADER_BG    = RGB(50, 55, 58);    // Header bar bg
	static const COLORREF COLHDR_BG    = RGB(35, 40, 42);    // Column header bg
	static const COLORREF GUTTER_BG    = RGB(45, 48, 50);    // Gutter bg
	static const COLORREF BORDER       = RGB(70, 75, 75);    // Subtle borders

	// Text colors — red=different, purple=orphan, white=same
	static const COLORREF TEXT_NORMAL   = RGB(255, 255, 255); // Same/identical file text (white in dark mode)
	static const COLORREF TEXT_ORPHAN   = RGB(150, 100, 220); // Orphan files (purple)
	static const COLORREF TEXT_DIFF     = RGB(220, 60, 60);   // Different files (red)
	static const COLORREF TEXT_FILTERED = RGB(100, 100, 100); // Filtered (dim gray)
	static const COLORREF TEXT_HEADER   = RGB(200, 200, 200); // Header/column text
	static const COLORREF TEXT_DIMMED   = RGB(60, 60, 60);    // Dimmed text for missing side

	// Folder text colors — same scheme: red=different, purple=orphan, white=same
	static const COLORREF FOLDER_IDENTICAL = RGB(255, 255, 255); // All children identical (white)
	static const COLORREF FOLDER_DIFFERENT = RGB(220, 60, 60);   // Contains differences (red)
	static const COLORREF FOLDER_ORPHAN    = RGB(150, 100, 220); // Orphan folder (purple)
	static const COLORREF FOLDER_MIXED     = RGB(220, 60, 60);   // Mixed diffs+orphans (red)
	static const COLORREF FOLDER_UNKNOWN   = RGB(200, 180, 60);  // Unknown/unscanned (yellow)

	// Folder icon fill colors — same scheme: red=different, purple=orphan, gray=same
	static const COLORREF ICON_FOLDER_IDENTICAL = RGB(180, 180, 180); // Gray folder (same)
	static const COLORREF ICON_FOLDER_DIFFERENT = RGB(220, 50, 50);   // Red folder (different)
	static const COLORREF ICON_FOLDER_ORPHAN    = RGB(140, 95, 210);  // Purple folder (orphan)
	static const COLORREF ICON_FOLDER_MIXED     = RGB(220, 50, 50);   // Red folder (mixed diffs)
	static const COLORREF ICON_FOLDER_UNKNOWN   = RGB(200, 180, 50);  // Yellow folder (unknown)

	// Dimmed background for missing-side columns
	static const COLORREF BG_MISSING    = RGB(25, 27, 27);
	static const COLORREF BG_MISSING_ALT= RGB(32, 35, 35);
}

// Default column width
constexpr int DefColumnWidth = 111;

// BC-style colored folder icon indices (appended after standard icons)
enum BcFolderIcon
{
	BCFOLDER_IDENTICAL = 0, // Gray folder — all children same
	BCFOLDER_DIFFERENT,     // Red folder — contains differences
	BCFOLDER_ORPHAN,        // Purple folder — orphan (one side only)
	BCFOLDER_MIXED,         // Split folder — left red (diffs), right purple (orphans)
	BCFOLDER_UNKNOWN,       // Yellow folder — unscanned / unknown
	BCFOLDER_PENDING,       // Gray outlined folder — awaiting scan
	BCFOLDER_COUNT
};

// Base index in the image list where BC folder icons start
static int s_nBcFolderIconBase = -1;

/**
 * @brief Draw a simple folder icon shape filled with a given color.
 */
static void DrawColoredFolderIcon(CDC &dc, int cx, int cy, COLORREF fillColor)
{
	CBrush brush(fillColor);
	CPen pen(PS_SOLID, 1, RGB(GetRValue(fillColor) * 2 / 3,
		GetGValue(fillColor) * 2 / 3, GetBValue(fillColor) * 2 / 3));
	CBrush* pOldBrush = dc.SelectObject(&brush);
	CPen* pOldPen = dc.SelectObject(&pen);

	int tabW = cx * 5 / 12;
	int tabH = cy / 5;
	dc.Rectangle(1, 1, tabW, 1 + tabH);

	int bodyTop = 1 + tabH - 1;
	dc.Rectangle(1, bodyTop, cx - 1, cy - 1);

	dc.SelectObject(pOldBrush);
	dc.SelectObject(pOldPen);
}

/**
 * @brief Draw a split-color folder icon — left half one color, right half another.
 * Used for BCFOLDER_MIXED: left=red (differences), right=purple (orphans).
 */
static void DrawSplitColorFolderIcon(CDC &dc, int cx, int cy,
	COLORREF leftColor, COLORREF rightColor)
{
	int midX = cx / 2;

	// Left half
	CRgn rgnLeft;
	rgnLeft.CreateRectRgn(0, 0, midX, cy);
	dc.SelectClipRgn(&rgnLeft);
	DrawColoredFolderIcon(dc, cx, cy, leftColor);

	// Right half
	CRgn rgnRight;
	rgnRight.CreateRectRgn(midX, 0, cx, cy);
	dc.SelectClipRgn(&rgnRight);
	DrawColoredFolderIcon(dc, cx, cy, rightColor);

	dc.SelectClipRgn(nullptr);
}

/**
 * @brief Draw an outlined (hollow) folder icon for "awaiting scan" state.
 */
static void DrawOutlinedFolderIcon(CDC &dc, int cx, int cy, COLORREF borderColor)
{
	CBrush* pOldBrush = (CBrush*)dc.SelectStockObject(NULL_BRUSH);
	CPen pen(PS_SOLID, 1, borderColor);
	CPen* pOldPen = dc.SelectObject(&pen);

	int tabW = cx * 5 / 12;
	int tabH = cy / 5;
	dc.Rectangle(1, 1, tabW, 1 + tabH);
	int bodyTop = 1 + tabH - 1;
	dc.Rectangle(1, bodyTop, cx - 1, cy - 1);

	dc.SelectObject(pOldBrush);
	dc.SelectObject(pOldPen);
}

// Text buffer for LVN_GETDISPINFO
static String s_rgUnifiedDispinfoText[2];

static tchar_t* NTAPI AllocUnifiedDispinfoText(const String &s)
{
	static int i = 0;
	const tchar_t* pszText = (s_rgUnifiedDispinfoText[i] = s).c_str();
	i ^= 1;
	return (tchar_t*)pszText;
}

/////////////////////////////////////////////////////////////////////////////
// CDirSxSUnifiedView

IMPLEMENT_DYNCREATE(CDirSxSUnifiedView, CListView)

CDirSxSUnifiedView::CDirSxSUnifiedView()
	: m_pCoordinator(nullptr)
	, m_pList(nullptr)
	, m_bUseColors(true)
	, m_bRowStripes(false)
	, m_bResizing(false)
	, m_nCachedToleranceSecs(-1)
	, m_nContextSide(0)
	, m_nActiveSide(0)
{
	m_cachedColors = {};
}

CDirSxSUnifiedView::~CDirSxSUnifiedView()
{
}

#ifdef _DEBUG
CDirDoc* CDirSxSUnifiedView::GetDocument()
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CDirDoc)));
	return (CDirDoc*)m_pDocument;
}
#endif

CDirFrame* CDirSxSUnifiedView::GetParentFrame()
{
	return static_cast<CDirFrame*>(CListView::GetParentFrame());
}

const CDiffContext& CDirSxSUnifiedView::GetDiffContext() const
{
	return GetDocument()->GetDiffContext();
}

CDiffContext& CDirSxSUnifiedView::GetDiffContext()
{
	return GetDocument()->GetDiffContext();
}

/**
 * @brief Determine which "side" a column belongs to.
 * @return 0 for left (cols 0-2), -1 for center (col 3), 1 for right (cols 4-6).
 */
int CDirSxSUnifiedView::GetColumnSide(int col)
{
	if (col <= COL_LEFT_MODIFIED)
		return 0;
	if (col == COL_CMP)
		return -1;
	return 1;
}

/////////////////////////////////////////////////////////////////////////////
// Message map

BEGIN_MESSAGE_MAP(CDirSxSUnifiedView, CListView)
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnCustomDraw)
	ON_NOTIFY(NM_CUSTOMDRAW, 0, OnHeaderCustomDraw)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnDblClick)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_KEYDOWN()
	ON_WM_CONTEXTMENU()
	ON_WM_SIZE()
	ON_WM_TIMER()
	ON_MESSAGE(MSG_UI_UPDATE, OnUpdateUIMessage)
	ON_COMMAND(ID_DIR_SXS_SWAP_SIDES, OnSxsSwapSides)
	ON_COMMAND(ID_DIR_SXS_COPY, OnSxsCopy)
	ON_COMMAND(ID_DIR_SXS_MOVE, OnSxsMove)
	ON_COMMAND(ID_DIR_SXS_OPEN_COMPARE, OnSxsOpenCompare)
	ON_COMMAND(ID_DIR_SXS_CROSS_COMPARE, OnSxsCrossCompare)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_COPY, OnUpdateSxsNeedSelection)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_MOVE, OnUpdateSxsNeedSelection)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_OPEN_COMPARE, OnUpdateSxsNeedSelection)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_CROSS_COMPARE, OnUpdateSxsNeedSelection)
	ON_NOTIFY_REFLECT(NM_CLICK, OnClick)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnColumnClick)
	ON_NOTIFY_REFLECT(LVN_ITEMCHANGED, OnItemChanged)
	ON_COMMAND(ID_DIR_SXS_TOGGLE_TREE, OnSxsToggleTree)
	ON_COMMAND(ID_DIR_SXS_EXPAND_ALL, OnSxsExpandAll)
	ON_COMMAND(ID_DIR_SXS_COLLAPSE_ALL, OnSxsCollapseAll)
	ON_COMMAND(ID_DIR_SXS_FLATTEN_MODE, OnSxsFlattenMode)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_TOGGLE_TREE, OnUpdateSxsToggleTree)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_FLATTEN_MODE, OnUpdateSxsFlattenMode)
	ON_COMMAND(ID_DIR_SXS_REFRESH, OnSxsRefresh)
	ON_COMMAND(ID_DIR_SXS_RENAME, OnSxsRename)
	ON_COMMAND(ID_DIR_SXS_FIND_FILENAME, OnSxsFindFilename)
	ON_NOTIFY_REFLECT(LVN_ENDLABELEDIT, OnEndLabelEdit)
	ON_COMMAND(ID_DIR_SXS_SELECT_ALL, OnSxsSelectAll)
	ON_COMMAND(ID_DIR_SXS_SELECT_NEWER, OnSxsSelectNewer)
	ON_COMMAND(ID_DIR_SXS_SELECT_ORPHANS, OnSxsSelectOrphans)
	ON_COMMAND(ID_DIR_SXS_SELECT_DIFFERENT, OnSxsSelectDifferent)
	ON_COMMAND(ID_DIR_SXS_INVERT_SELECTION, OnSxsInvertSelection)
	ON_COMMAND(ID_DIR_SXS_NEXT_DIFF, OnSxsNextDiff)
	ON_COMMAND(ID_DIR_SXS_PREV_DIFF, OnSxsPrevDiff)
	ON_COMMAND(ID_DIR_SXS_DELETE, OnSxsDelete)
	ON_COMMAND(ID_DIR_SXS_UPDATE_LEFT, OnSxsUpdateLeft)
	ON_COMMAND(ID_DIR_SXS_UPDATE_RIGHT, OnSxsUpdateRight)
	ON_COMMAND(ID_DIR_SXS_UPDATE_BOTH, OnSxsUpdateBoth)
	ON_COMMAND(ID_DIR_SXS_MIRROR_LEFT, OnSxsMirrorLeft)
	ON_COMMAND(ID_DIR_SXS_MIRROR_RIGHT, OnSxsMirrorRight)
	ON_COMMAND(ID_DIR_SXS_COMPARE_CONTENTS, OnSxsCompareContents)
	ON_COMMAND(ID_DIR_SXS_CRC_COMPARE, OnSxsCrcCompare)
	ON_COMMAND(ID_DIR_SXS_TOUCH_TIMESTAMPS, OnSxsTouchTimestamps)
	ON_COMMAND(ID_DIR_SXS_SHOW_LOG, OnSxsShowLog)
	ON_COMMAND(ID_DIR_SXS_GENERATE_REPORT, OnSxsGenerateReport)
	ON_NOTIFY_REFLECT(LVN_BEGINDRAG, OnBeginDrag)
	ON_COMMAND(ID_DIR_SXS_NAV_BACK, OnSxsNavBack)
	ON_COMMAND(ID_DIR_SXS_NAV_FORWARD, OnSxsNavForward)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_NAV_BACK, OnUpdateSxsNavBack)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_NAV_FORWARD, OnUpdateSxsNavForward)
	ON_COMMAND(ID_DIR_SXS_UP_LEVEL, OnSxsUpLevel)
	ON_COMMAND(ID_DIR_SXS_SET_BASE, OnSxsSetBase)
	ON_COMMAND(ID_DIR_SXS_SET_BASE_OTHER, OnSxsSetBaseOther)
	ON_COMMAND(ID_DIR_SXS_FIND_NEXT, OnSxsFindNext)
	ON_COMMAND(ID_DIR_SXS_FIND_PREV, OnSxsFindPrev)
	ON_COMMAND(ID_DIR_SXS_COPY_TO_FOLDER, OnSxsCopyToFolder)
	ON_COMMAND(ID_DIR_SXS_MOVE_TO_FOLDER, OnSxsMoveToFolder)
	ON_COMMAND(ID_DIR_SXS_NEW_FOLDER, OnSxsNewFolder)
	ON_COMMAND(ID_DIR_SXS_DELETE_PERMANENT, OnSxsDeletePermanent)
	ON_COMMAND(ID_DIR_SXS_EXCHANGE, OnSxsExchange)
	ON_COMMAND(ID_DIR_SXS_CHANGE_ATTRIBUTES, OnSxsChangeAttributes)
	ON_COMMAND(ID_DIR_SXS_TOUCH_NOW, OnSxsTouchNow)
	ON_COMMAND(ID_DIR_SXS_TOUCH_SPECIFIC, OnSxsTouchSpecific)
	ON_COMMAND(ID_DIR_SXS_TOUCH_FROM_OTHER, OnSxsTouchFromOther)
	ON_COMMAND(ID_DIR_SXS_ADV_FILTER, OnSxsAdvancedFilter)
	ON_COMMAND(ID_DIR_SXS_IGNORE_STRUCTURE, OnSxsIgnoreStructure)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_IGNORE_STRUCTURE, OnUpdateSxsIgnoreStructure)
	ON_COMMAND(ID_DIR_SXS_ROW_STRIPES, OnSxsRowStripes)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_ROW_STRIPES, OnUpdateSxsRowStripes)
	ON_COMMAND(ID_DIR_SXS_EXCLUDE_PATTERN, OnSxsExcludePattern)
	ON_COMMAND(ID_DIR_SXS_COMPARE_INFO, OnSxsCompareInfo)
	ON_COMMAND(ID_DIR_SXS_COPY_PATH, OnSxsCopyPath)
	ON_COMMAND(ID_DIR_SXS_COPY_FILENAME, OnSxsCopyFilename)
	ON_COMMAND(ID_DIR_SXS_OPEN_WITH_APP, OnSxsOpenWithApp)
	ON_COMMAND(ID_DIR_SXS_OPEN_WITH, OnSxsOpenWith)
	ON_COMMAND(ID_DIR_SXS_EXPLORER_MENU, OnSxsExplorerMenu)
	ON_COMMAND(ID_DIR_SXS_SELECT_LEFT_ONLY, OnSxsSelectLeftOnly)
	ON_COMMAND(ID_DIR_SXS_SELECT_RIGHT_ONLY, OnSxsSelectRightOnly)
	ON_COMMAND(ID_DIR_SXS_AUTO_EXPAND_ALL, OnSxsAutoExpandAll)
	ON_COMMAND(ID_DIR_SXS_AUTO_EXPAND_DIFF, OnSxsAutoExpandDiff)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_AUTO_EXPAND_ALL, OnUpdateSxsAutoExpandAll)
	ON_UPDATE_COMMAND_UI(ID_DIR_SXS_AUTO_EXPAND_DIFF, OnUpdateSxsAutoExpandDiff)
	ON_COMMAND(ID_DIR_SXS_ALIGN_WITH, OnSxsAlignWith)
	ON_COMMAND(ID_DIR_SXS_CUSTOMIZE_KEYS, OnSxsCustomizeKeys)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Initialization

BOOL CDirSxSUnifiedView::PreCreateWindow(CREATESTRUCT& cs)
{
	__super::PreCreateWindow(cs);
	cs.style |= LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_EDITLABELS;
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	return TRUE;
}

/**
 * @brief Initialize the unified view with 7 columns.
 */
void CDirSxSUnifiedView::OnInitialUpdate()
{
	const int iconCX = []() {
		const int cx = GetSystemMetrics(SM_CXSMICON);
		if (cx < 24) return 16;
		if (cx < 32) return 24;
		if (cx < 48) return 32;
		return 48;
	}();
	const int iconCY = iconCX;

	__super::OnInitialUpdate();
	m_pList = &GetListCtrl();

	// Load color settings
	m_bUseColors = GetOptionsMgr()->GetBool(OPT_DIRCLR_USE_COLORS);
	if (m_bUseColors)
		Options::DirColors::Load(GetOptionsMgr(), m_cachedColors);

	CDirDoc* pDoc = GetDocument();

	auto properties = strutils::split<std::vector<String>>(GetOptionsMgr()->GetString(OPT_ADDITIONAL_PROPERTIES), ' ');
	m_pColItems.reset(new DirViewColItems(pDoc->m_nDirs, properties));

	m_pList->SendMessage(CCM_SETUNICODEFORMAT, TRUE, 0);

	// Load user-selected font
	if (GetOptionsMgr()->GetBool(OPT_FONT_DIRCMP + OPT_FONT_USECUSTOM))
	{
		m_font.CreateFontIndirect(&theApp.m_lfDir);
		CWnd::SetFont(&m_font, TRUE);
	}

	// Create bold font for directory names
	{
		LOGFONT lf = {};
		if (m_font.GetSafeHandle())
			m_font.GetLogFont(&lf);
		else
		{
			NONCLIENTMETRICS ncm = { sizeof NONCLIENTMETRICS };
			SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof NONCLIENTMETRICS, &ncm, 0);
			lf = ncm.lfMessageFont;
		}
		lf.lfWeight = FW_BOLD;
		m_boldFont.CreateFontIndirect(&lf);
	}

	// Set dark theme background colors
	m_pList->SetBkColor(BcColors::BG_DARK);
	m_pList->SetTextBkColor(BcColors::BG_DARK);
	m_pList->SetTextColor(BcColors::TEXT_NORMAL);

	// Replace standard header with sort header
	HWND hWnd = ListView_GetHeader(m_pList->m_hWnd);
	if (hWnd != nullptr)
		m_ctlSortHeader.SubclassWindow(hWnd);

	// Load icons
	VERIFY(m_imageList.Create(iconCX, iconCY, ILC_COLOR32 | ILC_MASK, 15, 1));
	int icon_ids[] = {
		IDI_LFILE, IDI_MFILE, IDI_RFILE,
		IDI_MRFILE, IDI_LRFILE, IDI_LMFILE,
		IDI_NOTEQUALFILE, IDI_EQUALFILE, IDI_FILE,
		IDI_EQUALBINARY, IDI_BINARYDIFF,
		IDI_LFOLDER, IDI_MFOLDER, IDI_RFOLDER,
		IDI_MRFOLDER, IDI_LRFOLDER, IDI_LMFOLDER,
		IDI_FILESKIP, IDI_FOLDERSKIP,
		IDI_NOTEQUALFOLDER, IDI_EQUALFOLDER, IDI_FOLDER,
		IDI_COMPARE_ERROR,
		IDI_FOLDERUP, IDI_FOLDERUP_DISABLE,
		IDI_COMPARE_ABORTED,
		IDI_NOTEQUALTEXTFILE, IDI_EQUALTEXTFILE,
		IDI_NOTEQUALIMAGE, IDI_EQUALIMAGE,
	};
	for (auto id : icon_ids)
		VERIFY(-1 != m_imageList.Add((HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, iconCX, iconCY, 0)));

	// Append BC-style colored folder icons (GDI-drawn)
	{
		s_nBcFolderIconBase = m_imageList.GetImageCount();
		COLORREF folderColors[BCFOLDER_COUNT] = {
			BcColors::ICON_FOLDER_IDENTICAL,
			BcColors::ICON_FOLDER_DIFFERENT,
			BcColors::ICON_FOLDER_ORPHAN,
			RGB(0, 0, 0), // BCFOLDER_MIXED — handled specially below
			BcColors::ICON_FOLDER_UNKNOWN,
			RGB(120, 120, 130), // BCFOLDER_PENDING — gray outline
		};
		CDC dcMem;
		dcMem.CreateCompatibleDC(nullptr);
		for (int fi = 0; fi < BCFOLDER_COUNT; fi++)
		{
			CBitmap bmpColor, bmpMask;

			bmpColor.CreateBitmap(iconCX, iconCY, 1, 32, nullptr);
			CBitmap* pOld = dcMem.SelectObject(&bmpColor);
			dcMem.FillSolidRect(0, 0, iconCX, iconCY, RGB(0, 0, 0));
			if (fi == BCFOLDER_MIXED)
				DrawSplitColorFolderIcon(dcMem, iconCX, iconCY,
					BcColors::ICON_FOLDER_DIFFERENT, BcColors::ICON_FOLDER_ORPHAN);
			else if (fi == BCFOLDER_PENDING)
				DrawOutlinedFolderIcon(dcMem, iconCX, iconCY, RGB(120, 120, 130));
			else
				DrawColoredFolderIcon(dcMem, iconCX, iconCY, folderColors[fi]);
			dcMem.SelectObject(pOld);

			bmpMask.CreateBitmap(iconCX, iconCY, 1, 1, nullptr);
			pOld = dcMem.SelectObject(&bmpMask);
			dcMem.FillSolidRect(0, 0, iconCX, iconCY, RGB(255, 255, 255));

			CBrush black(RGB(0, 0, 0));
			CPen blackPen(PS_SOLID, 1, RGB(0, 0, 0));
			CBrush* pOldBr = dcMem.SelectObject(&black);
			CPen* pOldPen = dcMem.SelectObject(&blackPen);
			int tabW = iconCX * 5 / 12;
			int tabH = iconCY / 5;
			dcMem.Rectangle(1, 1, tabW, 1 + tabH);
			dcMem.Rectangle(1, tabH, iconCX - 1, iconCY - 1);
			dcMem.SelectObject(pOldBr);
			dcMem.SelectObject(pOldPen);
			dcMem.SelectObject(pOld);

			m_imageList.Add(&bmpColor, &bmpMask);
		}
	}

	m_pList->SetImageList(&m_imageList, LVSIL_SMALL);

	// Insert 7 columns
	m_pList->SetRedraw(FALSE);

	const int dpi = CClientDC(this).GetDeviceCaps(LOGPIXELSX);
	auto px = [dpi](int pt) { return MulDiv(pt, dpi, 72); };

	m_pList->InsertColumn(COL_LEFT_NAME,     _T("Name"),     LVCFMT_LEFT,  px(180));
	m_pList->InsertColumn(COL_LEFT_SIZE,     _T("Size"),     LVCFMT_RIGHT, px(70));
	m_pList->InsertColumn(COL_LEFT_MODIFIED, _T("Modified"), LVCFMT_LEFT,  px(120));
	m_pList->InsertColumn(COL_CMP,           _T("Cmp"),      LVCFMT_CENTER, px(30));
	m_pList->InsertColumn(COL_RIGHT_NAME,    _T("Name"),     LVCFMT_LEFT,  px(180));
	m_pList->InsertColumn(COL_RIGHT_SIZE,    _T("Size"),     LVCFMT_RIGHT, px(70));
	m_pList->InsertColumn(COL_RIGHT_MODIFIED,_T("Modified"), LVCFMT_LEFT,  px(120));

	// Extended styles
	DWORD exstyle = LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP
		| LVS_EX_DOUBLEBUFFER | LVS_EX_SUBITEMIMAGES;
	m_pList->SetExtendedStyle(exstyle);

	m_pList->SetRedraw(TRUE);

	// Initialize row stripes setting
	m_bRowStripes = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_ROW_STRIPES);

	// Load configurable key bindings
	LoadKeyBindings();
}

/////////////////////////////////////////////////////////////////////////////
// PreTranslateMessage — keyboard shortcuts

BOOL CDirSxSUnifiedView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		bool bCtrl = (GetKeyState(VK_CONTROL) < 0);
		bool bShift = (GetKeyState(VK_SHIFT) < 0);
		bool bAlt = (GetKeyState(VK_MENU) < 0);

		// Check configurable key bindings first
		for (const auto& kv : m_keyBindings)
		{
			const KeyBinding& kb = kv.second;
			if (pMsg->wParam == kb.vkKey && bCtrl == kb.bCtrl && bShift == kb.bShift && bAlt == kb.bAlt)
			{
				SendMessage(WM_COMMAND, kv.first, 0);
				return TRUE;
			}
		}

		switch (pMsg->wParam)
		{
		case VK_F5:
			OnSxsRefresh();
			return TRUE;
		case VK_F2:
			OnSxsRename();
			return TRUE;
		case VK_F3:
			if (bShift)
				OnSxsFindPrev();
			else
				OnSxsFindNext();
			return TRUE;
		case 'F':
			if (bCtrl)
			{
				OnSxsFindFilename();
				return TRUE;
			}
			break;
		case 'N':
			if (bCtrl)
			{
				if (bShift)
					OnSxsPrevDiff();
				else
					OnSxsNextDiff();
				return TRUE;
			}
			break;
		case 'I':
			if (bCtrl && !bShift && !bAlt)
			{
				OnSxsCompareInfo();
				return TRUE;
			}
			break;
		case 'C':
			if (bCtrl && bShift && !bAlt)
			{
				OnSxsCopyPath();
				return TRUE;
			}
			break;
		case VK_LEFT:
			if (bAlt && !bCtrl && !bShift)
			{
				OnSxsNavBack();
				return TRUE;
			}
			break;
		case VK_RIGHT:
			if (bAlt && !bCtrl && !bShift)
			{
				OnSxsNavForward();
				return TRUE;
			}
			break;
		case VK_BACK:
			if (!bCtrl && !bShift && !bAlt)
			{
				OnSxsUpLevel();
				return TRUE;
			}
			break;
		case VK_INSERT:
			if (!bCtrl && !bShift && !bAlt)
			{
				OnSxsNewFolder();
				return TRUE;
			}
			break;
		case VK_DELETE:
			if (bShift && !bCtrl && !bAlt)
			{
				OnSxsDeletePermanent();
				return TRUE;
			}
			OnSxsDelete();
			return TRUE;
		}
	}
	return __super::PreTranslateMessage(pMsg);
}

/////////////////////////////////////////////////////////////////////////////
// Child notify — dispatch LVN_GETDISPINFO

BOOL CDirSxSUnifiedView::OnChildNotify(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	if (uMsg == WM_NOTIFY)
	{
		NMHDR *pNMHDR = (NMHDR *)lParam;
		if (pNMHDR->code == LVN_GETDISPINFO)
		{
			ReflectGetdispinfo((NMLVDISPINFO *)lParam);
			return TRUE;
		}
	}
	return __super::OnChildNotify(uMsg, wParam, lParam, pResult);
}

/////////////////////////////////////////////////////////////////////////////
// Cell text formatting helpers

/**
 * @brief Format a file size with thousands separators (e.g. "1,234,567").
 */
static String FormatFileSize(int64_t size)
{
	if (size < 0)
		return _T("");
	String raw = strutils::format(_T("%lld"), size);
	String result;
	int len = static_cast<int>(raw.length());
	for (int i = 0; i < len; i++)
	{
		if (i > 0 && (len - i) % 3 == 0)
			result += _T(',');
		result += raw[i];
	}
	return result;
}

/**
 * @brief Format a Poco::Timestamp as "YYYY-MM-DD HH:MM:SS" in local time.
 */
static String FormatTimestamp(const DiffFileInfo &fi)
{
	if (fi.mtime == 0)
		return _T("");
	int64_t epochUs = fi.mtime.epochMicroseconds();
	int64_t ft100ns = epochUs * 10 + 116444736000000000LL;
	FILETIME ft;
	ft.dwLowDateTime = static_cast<DWORD>(ft100ns);
	ft.dwHighDateTime = static_cast<DWORD>(ft100ns >> 32);
	SYSTEMTIME stUtc, stLocal;
	FileTimeToSystemTime(&ft, &stUtc);
	SystemTimeToTzSpecificLocalTime(nullptr, &stUtc, &stLocal);
	return strutils::format(_T("%04d-%02d-%02d %02d:%02d:%02d"),
		stLocal.wYear, stLocal.wMonth, stLocal.wDay,
		stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
}

/**
 * @brief Get the comparison symbol for a row.
 * "=" for identical, "\u2260" for different, blank for folders/orphans.
 */
String CDirSxSUnifiedView::GetComparisonSymbol(int nRow) const
{
	if (nRow < 0 || nRow >= static_cast<int>(m_listViewItems.size()))
		return _T("");

	DIFFITEM *key = reinterpret_cast<DIFFITEM*>(m_listViewItems[nRow].lParam);
	if (!key)
		return _T("");

	if (!GetDocument()->HasDiffs())
		return _T("");

	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);

	if (di.diffcode.isDirectory())
	{
		// Show hourglass if this directory is actively being scanned
		DIFFITEM *pActive = ctxt.m_pActiveScanParent.load(std::memory_order_relaxed);
		if (pActive == key)
			return _T("\u231B"); // Hourglass
		return _T("");
	}
	if (!IsItemExistAll(ctxt, di))
		return _T("");  // Orphan — blank

	// Use raw compare flags to handle all result states including
	// files in subdirectories that may not match isResultDiff()'s
	// strict existAll() requirement.
	if (di.diffcode.isResultFiltered())
		return _T("~");
	unsigned cmpResult = di.diffcode.diffcode & DIFFCODE::COMPAREFLAGS;
	if (cmpResult == DIFFCODE::SAME)
		return _T("=");
	if (cmpResult == DIFFCODE::DIFF)
		return _T("\u2260");
	if (cmpResult == DIFFCODE::CMPERR || cmpResult == DIFFCODE::CMPABORT)
		return _T("!");
	// NOCMP (0) — not yet compared
	return _T("?");
}

/**
 * @brief Get cell text for a given row and column in the unified view.
 *
 * Columns 0-2 get data from the left side (diffFileInfo[0]).
 * Column 3 returns the comparison symbol.
 * Columns 4-6 get data from the right side (diffFileInfo[nDirs-1]).
 */
String CDirSxSUnifiedView::GetCellText(int nRow, int nCol) const
{
	if (nRow < 0 || nRow >= static_cast<int>(m_listViewItems.size()))
		return _T("");

	DIFFITEM *key = reinterpret_cast<DIFFITEM*>(m_listViewItems[nRow].lParam);
	if (!key)
		return _T("");

	if (!GetDocument()->HasDiffs())
		return _T("");

	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);
	int nDirs = ctxt.GetCompareDirs();

	if (nCol == COL_CMP)
		return GetComparisonSymbol(nRow);

	// Determine which side this column maps to
	int side = (nCol <= COL_LEFT_MODIFIED) ? 0 : (nDirs - 1);

	// Check if item exists on this side
	if (!di.diffcode.exists(side))
		return _T("");

	// Determine the "type" of column within the side (0=Name, 1=Size, 2=Modified)
	int colType = (nCol <= COL_LEFT_MODIFIED) ? nCol : (nCol - COL_RIGHT_NAME);

	switch (colType)
	{
	case 0: // Name
		return di.diffFileInfo[side].filename;
	case 1: // Size
		if (di.diffcode.isDirectory())
			return _T("");
		return FormatFileSize(di.diffFileInfo[side].size);
	case 2: // Modified
		return FormatTimestamp(di.diffFileInfo[side]);
	default:
		return _T("");
	}
}

/////////////////////////////////////////////////////////////////////////////
// LVN_GETDISPINFO

void CDirSxSUnifiedView::ReflectGetdispinfo(NMLVDISPINFO *pParam)
{
	int nIdx = pParam->item.iItem;
	if (nIdx < 0 || nIdx >= static_cast<int>(m_listViewItems.size()))
		return;

	DIFFITEM *key = reinterpret_cast<DIFFITEM*>(m_listViewItems[nIdx].lParam);
	if (key == nullptr)
	{
		if (pParam->item.mask & LVIF_TEXT)
			pParam->item.pszText = _T("");
		if (pParam->item.mask & LVIF_IMAGE)
			pParam->item.iImage = -1;
		return;
	}

	if (!GetDocument()->HasDiffs())
		return;

	if (pParam->item.mask & LVIF_TEXT)
	{
		String s = GetCellText(nIdx, pParam->item.iSubItem);
		pParam->item.pszText = AllocUnifiedDispinfoText(s);
	}
	if (pParam->item.mask & LVIF_IMAGE)
	{
		int nSubItem = pParam->item.iSubItem;
		if (nSubItem == COL_LEFT_NAME || nSubItem == COL_RIGHT_NAME)
		{
			const CDiffContext &ctxt = GetDiffContext();
			const DIFFITEM &di = ctxt.GetDiffAt(key);
			int side = GetColumnSide(nSubItem);
			bool bExists = (side == 0)
				? di.diffcode.existsFirst()
				: di.diffcode.existsSecond();
			// Use per-side icon so folder icons reflect orphans on each side independently
			pParam->item.iImage = bExists ? GetItemIconForSide(nIdx, side) : -1;
		}
		else
		{
			pParam->item.iImage = -1;
		}
	}
	if (pParam->item.mask & LVIF_INDENT)
	{
		pParam->item.iIndent = m_listViewItems[nIdx].iIndent;
	}
}

/**
 * @brief Get icon for an item row (used for column 0 only).
 * Directories use BC-style colored folder icons based on content status.
 */
int CDirSxSUnifiedView::GetItemIcon(int nRow) const
{
	if (nRow < 0 || nRow >= static_cast<int>(m_listViewItems.size()))
		return -1;

	DIFFITEM *key = reinterpret_cast<DIFFITEM*>(m_listViewItems[nRow].lParam);
	if (!key)
		return -1;

	if (!GetDocument()->HasDiffs())
		return -1;

	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);

	if (!di.diffcode.isDirectory() || s_nBcFolderIconBase < 0)
	{
		// Non-directory — use standard icon via coordinator
		int icon;
		if (m_pCoordinator)
			icon = m_pCoordinator->GetPaneColImage(di, 0);
		else
			icon = GetColImage(di);

		// SxS mode: remap NOCMP items to plain file icon instead of "?"
		if ((di.diffcode.diffcode & DIFFCODE::COMPAREFLAGS) == DIFFCODE::NOCMP)
			icon = DIFFIMG_FILE;

		// SxS mode: remap orphan-unique icons to plain file icon
		// (which pane the file is on is already clear from placement)
		switch (icon)
		{
		case DIFFIMG_LUNIQUE:
		case DIFFIMG_RUNIQUE:
		case DIFFIMG_MUNIQUE:
			icon = DIFFIMG_FILE;
			break;
		case DIFFIMG_LDIRUNIQUE:
		case DIFFIMG_RDIRUNIQUE:
		case DIFFIMG_MDIRUNIQUE:
			icon = DIFFIMG_DIR;
			break;
		}
		return icon;
	}

	// Directory: use BC-style colored folder icons
	if (di.diffcode.isResultError())
		return DIFFIMG_ERROR;
	if (di.diffcode.isResultAbort())
		return DIFFIMG_ABORT;
	if (di.diffcode.isResultFiltered())
		return DIFFIMG_DIRSKIP;

	// Orphan folder
	if (!IsItemExistAll(ctxt, di))
		return s_nBcFolderIconBase + BCFOLDER_ORPHAN;

	// Folder on both sides — check content status
	if (m_pCoordinator)
	{
		// If folder hasn't been scanned yet (no children AND no compare flags set),
		// show pending icon. Scanned-but-empty folders will have SAME or DIFF set.
		if (!di.HasChildren() &&
			(di.diffcode.diffcode & DIFFCODE::COMPAREFLAGS) == DIFFCODE::NOCMP)
		{
			return s_nBcFolderIconBase + BCFOLDER_PENDING;
		}

		// For scanned-but-empty folders, use their compare flags directly
		if (!di.HasChildren())
		{
			if (di.diffcode.isResultSame())
				return s_nBcFolderIconBase + BCFOLDER_IDENTICAL;
			else if (di.diffcode.isResultDiff())
				return s_nBcFolderIconBase + BCFOLDER_DIFFERENT;
			else
				return s_nBcFolderIconBase + BCFOLDER_UNKNOWN;
		}

		FolderContentStatus status = m_pCoordinator->ComputeFolderContentStatus(di);
		switch (status)
		{
		case FOLDER_STATUS_ALL_SAME:
			return s_nBcFolderIconBase + BCFOLDER_IDENTICAL;
		case FOLDER_STATUS_ALL_DIFFERENT:
			return s_nBcFolderIconBase + BCFOLDER_DIFFERENT;
		case FOLDER_STATUS_UNIQUE_ONLY:
			return s_nBcFolderIconBase + BCFOLDER_ORPHAN;
		case FOLDER_STATUS_MIXED:
			return s_nBcFolderIconBase + BCFOLDER_MIXED;
		case FOLDER_STATUS_UNKNOWN:
			// Unknown status with children means still scanning — show pending
			return s_nBcFolderIconBase + BCFOLDER_PENDING;
		default:
			return s_nBcFolderIconBase + BCFOLDER_UNKNOWN;
		}
	}

	return s_nBcFolderIconBase + BCFOLDER_UNKNOWN;
}

/**
 * @brief Get icon for a specific side of a row.
 *
 * For non-directory items, returns the same as GetItemIcon.
 * For directories present on both sides, uses per-side status:
 * - Only shows BCFOLDER_MIXED if THIS side has both orphans and diffs.
 * - A left-only orphan counts as an orphan on the left side only.
 * - A right-only orphan counts as an orphan on the right side only.
 * - Diffs (files on both sides with different content) count for both sides.
 *
 * @param side 0=left, 1=right
 */
int CDirSxSUnifiedView::GetItemIconForSide(int nRow, int side) const
{
	if (nRow < 0 || nRow >= static_cast<int>(m_listViewItems.size()))
		return -1;

	DIFFITEM *key = reinterpret_cast<DIFFITEM*>(m_listViewItems[nRow].lParam);
	if (!key)
		return -1;

	if (!GetDocument()->HasDiffs())
		return -1;

	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);

	// Non-directory or no BC icons: same as GetItemIcon
	if (!di.diffcode.isDirectory() || s_nBcFolderIconBase < 0)
		return GetItemIcon(nRow);

	// Directory special cases
	if (di.diffcode.isResultError())
		return DIFFIMG_ERROR;
	if (di.diffcode.isResultAbort())
		return DIFFIMG_ABORT;
	if (di.diffcode.isResultFiltered())
		return DIFFIMG_DIRSKIP;

	// Orphan folder (doesn't exist on both sides)
	if (!IsItemExistAll(ctxt, di))
		return s_nBcFolderIconBase + BCFOLDER_ORPHAN;

	// Folder on both sides — check per-side content status
	if (m_pCoordinator)
	{
		// Only show PENDING if not yet scanned (no children AND no compare flags)
		if (!di.HasChildren() &&
			(di.diffcode.diffcode & DIFFCODE::COMPAREFLAGS) == DIFFCODE::NOCMP)
		{
			return s_nBcFolderIconBase + BCFOLDER_PENDING;
		}

		// Scanned-but-empty folder: use compare flags directly
		if (!di.HasChildren())
		{
			if (di.diffcode.isResultSame())
				return s_nBcFolderIconBase + BCFOLDER_IDENTICAL;
			else if (di.diffcode.isResultDiff())
				return s_nBcFolderIconBase + BCFOLDER_DIFFERENT;
			else
				return s_nBcFolderIconBase + BCFOLDER_UNKNOWN;
		}

		FolderContentStatus status = m_pCoordinator->ComputeFolderContentStatusForSide(di, side);
		switch (status)
		{
		case FOLDER_STATUS_ALL_SAME:
			return s_nBcFolderIconBase + BCFOLDER_IDENTICAL;
		case FOLDER_STATUS_ALL_DIFFERENT:
			return s_nBcFolderIconBase + BCFOLDER_DIFFERENT;
		case FOLDER_STATUS_UNIQUE_ONLY:
			return s_nBcFolderIconBase + BCFOLDER_ORPHAN;
		case FOLDER_STATUS_MIXED:
			return s_nBcFolderIconBase + BCFOLDER_MIXED;
		case FOLDER_STATUS_UNKNOWN:
			return s_nBcFolderIconBase + BCFOLDER_PENDING;
		default:
			return s_nBcFolderIconBase + BCFOLDER_UNKNOWN;
		}
	}

	return s_nBcFolderIconBase + BCFOLDER_UNKNOWN;
}

/////////////////////////////////////////////////////////////////////////////
// Custom draw — per-subitem coloring

void CDirSxSUnifiedView::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (!m_bUseColors)
		return;

	*pResult = CDRF_DODEFAULT;
	LPNMLVCUSTOMDRAW lpC = (LPNMLVCUSTOMDRAW)pNMHDR;

	if (lpC->nmcd.dwDrawStage == CDDS_PREPAINT)
	{
		*pResult = CDRF_NOTIFYITEMDRAW;
		return;
	}
	if (lpC->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
	{
		int nRow = static_cast<int>(lpC->nmcd.dwItemSpec);
		if (nRow >= 0 && nRow < static_cast<int>(m_listViewItems.size()))
		{
			DIFFITEM *key = reinterpret_cast<DIFFITEM*>(m_listViewItems[nRow].lParam);
			if (key && GetDocument()->HasDiffs())
			{
				const CDiffContext &ctxt = GetDiffContext();
				const DIFFITEM &di = ctxt.GetDiffAt(key);
				if (di.diffcode.isDirectory())
				{
					if (m_boldFont.GetSafeHandle())
						SelectObject(lpC->nmcd.hdc, m_boldFont.GetSafeHandle());

					// BC-style: folder text color based on content status
					if (di.diffcode.isResultFiltered())
						lpC->clrText = BcColors::TEXT_FILTERED;
					else if (!IsItemExistAll(ctxt, di))
						lpC->clrText = BcColors::FOLDER_ORPHAN;
					else if (m_pCoordinator)
					{
						FolderContentStatus status = m_pCoordinator->ComputeFolderContentStatus(di);
						switch (status)
						{
						case FOLDER_STATUS_ALL_SAME:
							lpC->clrText = BcColors::FOLDER_IDENTICAL;
							break;
						case FOLDER_STATUS_ALL_DIFFERENT:
							lpC->clrText = BcColors::FOLDER_DIFFERENT;
							break;
						case FOLDER_STATUS_UNIQUE_ONLY:
							lpC->clrText = BcColors::FOLDER_ORPHAN;
							break;
						case FOLDER_STATUS_MIXED:
							lpC->clrText = BcColors::FOLDER_MIXED;
							break;
						default:
							lpC->clrText = BcColors::FOLDER_UNKNOWN;
							break;
						}
					}
					else
						lpC->clrText = BcColors::FOLDER_UNKNOWN;

					*pResult = CDRF_NOTIFYSUBITEMDRAW | CDRF_NEWFONT;
					return;
				}
			}
		}
		*pResult = CDRF_NOTIFYSUBITEMDRAW;
		return;
	}
	if (lpC->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM))
	{
		int nRow = static_cast<int>(lpC->nmcd.dwItemSpec);
		int nSubItem = lpC->iSubItem;
		COLORREF clrBk, clrText;
		GetColors(nRow, nSubItem, clrBk, clrText);

		bool bSelected = (m_pList->GetItemState(nRow, LVIS_SELECTED) & LVIS_SELECTED) != 0;

		// ---------------------------------------------------------------
		// SELECTED ROWS: We must take full control of drawing (CDRF_SKIPDEFAULT)
		// because LVS_EX_FULLROWSELECT + Windows themes paint the selection
		// highlight across the entire row, ignoring clrTextBk overrides.
		// We paint each cell ourselves: highlight on the active side,
		// normal background on the inactive side.
		// ---------------------------------------------------------------
		if (bSelected)
		{
			bool bFocused = (GetFocus() == m_pList);
			int colSide = GetColumnSide(nSubItem);
			bool bOnActiveSide = (colSide == m_nActiveSide) || (colSide == -1);

			COLORREF bgColor, textColor;
			if (bOnActiveSide)
			{
				bgColor = bFocused ? GetSysColor(COLOR_HIGHLIGHT) : GetSysColor(COLOR_BTNFACE);
				textColor = bFocused ? GetSysColor(COLOR_HIGHLIGHTTEXT) : GetSysColor(COLOR_BTNTEXT);
			}
			else
			{
				bgColor = clrBk;
				textColor = clrText;
			}

			// Get the cell rectangle
			RECT rc;
			if (nSubItem == 0)
			{
				// Column 0: GetSubItemRect returns the entire row; clip to column width
				ListView_GetItemRect(m_pList->m_hWnd, nRow, &rc, LVIR_BOUNDS);
				rc.right = rc.left + m_pList->GetColumnWidth(0);
			}
			else
			{
				ListView_GetSubItemRect(m_pList->m_hWnd, nRow, nSubItem, LVIR_BOUNDS, &rc);
			}

			// Fill background — paints over any theme-drawn selection highlight
			HBRUSH hBr = CreateSolidBrush(bgColor);
			FillRect(lpC->nmcd.hdc, &rc, hBr);
			DeleteObject(hBr);

			int iconW = GetSystemMetrics(SM_CXSMICON);
			int indent = (nRow >= 0 && nRow < (int)m_listViewItems.size())
				? m_listViewItems[nRow].iIndent : 0;
			int indentPx = indent * iconW;

			// Name columns: draw icon + indented text
			if (nSubItem == COL_LEFT_NAME || nSubItem == COL_RIGHT_NAME)
			{
				int side = (nSubItem == COL_LEFT_NAME) ? 0 : 1;
				DIFFITEM *key = (nRow >= 0 && nRow < (int)m_listViewItems.size())
					? reinterpret_cast<DIFFITEM*>(m_listViewItems[nRow].lParam) : nullptr;
				if (key && GetDocument()->HasDiffs())
				{
					const CDiffContext &ctxt = GetDiffContext();
					const DIFFITEM &di = ctxt.GetDiffAt(key);
					int sideIdx = (side == 0) ? 0 : (ctxt.GetCompareDirs() - 1);
					if (di.diffcode.exists(sideIdx))
					{
						int iImage = GetItemIconForSide(nRow, side);
						if (iImage >= 0)
						{
							ImageList_Draw(m_imageList.GetSafeHandle(), iImage,
								lpC->nmcd.hdc,
								rc.left + indentPx + 2,
								rc.top + (rc.bottom - rc.top - iconW) / 2,
								ILD_TRANSPARENT);
						}
					}
				}

				RECT rcText = rc;
				rcText.left += indentPx + iconW + 4;
				String text = GetCellText(nRow, nSubItem);
				if (!text.empty())
				{
					SetBkMode(lpC->nmcd.hdc, TRANSPARENT);
					SetTextColor(lpC->nmcd.hdc, textColor);
					DrawText(lpC->nmcd.hdc, text.c_str(), (int)text.length(),
						&rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
				}
			}
			else
			{
				// Size / Modified / Cmp columns: text only
				String text = GetCellText(nRow, nSubItem);
				if (!text.empty())
				{
					RECT rcText = rc;
					rcText.left += 4;
					rcText.right -= 2;

					UINT fmt = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS;
					if (nSubItem == COL_LEFT_SIZE || nSubItem == COL_RIGHT_SIZE)
						fmt |= DT_RIGHT;
					else if (nSubItem == COL_CMP)
						fmt |= DT_CENTER;
					else
						fmt |= DT_LEFT;

					SetBkMode(lpC->nmcd.hdc, TRANSPARENT);
					SetTextColor(lpC->nmcd.hdc, textColor);
					DrawText(lpC->nmcd.hdc, text.c_str(), (int)text.length(),
						&rcText, fmt);
				}
			}

			*pResult = CDRF_SKIPDEFAULT;
			return;
		}

		// ---------------------------------------------------------------
		// NON-SELECTED ROWS: let the listview draw with our colors,
		// except COL_RIGHT_NAME which needs custom indent drawing.
		// ---------------------------------------------------------------
		lpC->clrTextBk = clrBk;
		lpC->clrText = clrText;

		if (nSubItem == COL_RIGHT_NAME && nRow >= 0 && nRow < (int)m_listViewItems.size())
		{
			int indent = m_listViewItems[nRow].iIndent;
			if (indent > 0)
			{
				int iconW = GetSystemMetrics(SM_CXSMICON);
				int indentPx = indent * iconW;

				RECT rc;
				ListView_GetSubItemRect(m_pList->m_hWnd, nRow, nSubItem, LVIR_BOUNDS, &rc);

				COLORREF bgColor = lpC->clrTextBk;
				COLORREF textColor = lpC->clrText;

				HBRUSH hBr = CreateSolidBrush(bgColor);
				FillRect(lpC->nmcd.hdc, &rc, hBr);
				DeleteObject(hBr);

				DIFFITEM *key = reinterpret_cast<DIFFITEM*>(m_listViewItems[nRow].lParam);
				if (key && GetDocument()->HasDiffs())
				{
					const CDiffContext &ctxt = GetDiffContext();
					const DIFFITEM &di = ctxt.GetDiffAt(key);
					int side = ctxt.GetCompareDirs() - 1;
					if (di.diffcode.exists(side))
					{
						int iImage = GetItemIconForSide(nRow, 1);
						if (iImage >= 0)
						{
							ImageList_Draw(m_imageList.GetSafeHandle(), iImage,
								lpC->nmcd.hdc,
								rc.left + indentPx + 2,
								rc.top + (rc.bottom - rc.top - iconW) / 2,
								ILD_TRANSPARENT);
						}

						RECT rcText = rc;
						rcText.left += indentPx + iconW + 4;
						String text = GetCellText(nRow, nSubItem);
						SetBkMode(lpC->nmcd.hdc, TRANSPARENT);
						SetTextColor(lpC->nmcd.hdc, textColor);
						DrawText(lpC->nmcd.hdc, text.c_str(), (int)text.length(),
							&rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
					}
				}

				*pResult = CDRF_SKIPDEFAULT;
				return;
			}
		}
	}
}

/**
 * @brief Custom draw handler for the column header control (dark theme).
 */
void CDirSxSUnifiedView::OnHeaderCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMCUSTOMDRAW lpCD = (LPNMCUSTOMDRAW)pNMHDR;
	*pResult = CDRF_DODEFAULT;

	if (!m_pList || lpCD->hdr.hwndFrom != ListView_GetHeader(m_pList->m_hWnd))
		return;

	if (lpCD->dwDrawStage == CDDS_PREPAINT)
	{
		*pResult = CDRF_NOTIFYITEMDRAW;
		return;
	}
	if (lpCD->dwDrawStage == CDDS_ITEMPREPAINT)
	{
		::FillRect(lpCD->hdc, &lpCD->rc, (HBRUSH)::CreateSolidBrush(BcColors::COLHDR_BG));

		HPEN hPen = ::CreatePen(PS_SOLID, 1, BcColors::BORDER);
		HPEN hOld = (HPEN)::SelectObject(lpCD->hdc, hPen);
		::MoveToEx(lpCD->hdc, lpCD->rc.left, lpCD->rc.bottom - 1, nullptr);
		::LineTo(lpCD->hdc, lpCD->rc.right, lpCD->rc.bottom - 1);
		::SelectObject(lpCD->hdc, hOld);
		::DeleteObject(hPen);

		HDITEM hdi = {};
		tchar_t szText[128] = {};
		hdi.mask = HDI_TEXT;
		hdi.pszText = szText;
		hdi.cchTextMax = _countof(szText);
		Header_GetItem(lpCD->hdr.hwndFrom, (int)lpCD->dwItemSpec, &hdi);

		::SetBkMode(lpCD->hdc, TRANSPARENT);
		::SetTextColor(lpCD->hdc, BcColors::TEXT_HEADER);
		CRect rcText = lpCD->rc;
		rcText.DeflateRect(4, 0);

		// Center-align the Cmp column header
		UINT fmt = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
		if ((int)lpCD->dwItemSpec == COL_CMP)
			fmt |= DT_CENTER;
		else
			fmt |= DT_LEFT;

		::DrawText(lpCD->hdc, szText, -1, &rcText, fmt);

		*pResult = CDRF_SKIPDEFAULT;
		return;
	}
}

/**
 * @brief Get colors for an item at a specific subitem (column).
 *
 * Key difference from CDirPaneView: per-subitem coloring.
 * - Left columns (0-2): colored by the left-side file status.
 * - Right columns (4-6): colored by the right-side file status.
 * - Center column (3): neutral.
 * - Missing side: dimmed background, no text.
 */
void CDirSxSUnifiedView::GetColors(int nRow, int nCol, COLORREF& clrBk, COLORREF& clrText) const
{
	// Dark alternating rows (only when row stripes enabled)
	bool bOddRow = m_bRowStripes && (nRow & 1) != 0;
	clrBk = bOddRow ? BcColors::BG_ALT : BcColors::BG_DARK;
	clrText = BcColors::TEXT_NORMAL;

	if (nRow < 0 || nRow >= static_cast<int>(m_listViewItems.size()))
		return;

	DIFFITEM *key = reinterpret_cast<DIFFITEM*>(m_listViewItems[nRow].lParam);
	if (key == nullptr)
	{
		clrText = clrBk;
		return;
	}

	if (!GetDocument()->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);
	int nDirs = ctxt.GetCompareDirs();

	// Determine which side this column belongs to
	int colSide = GetColumnSide(nCol);

	// For the center "Cmp" column, use neutral colors based on overall status
	if (colSide == -1)
	{
		if (di.isEmpty() || di.diffcode.isDirectory())
			return;

		if (di.diffcode.isResultFiltered())
			clrText = BcColors::TEXT_FILTERED;
		else if (!IsItemExistAll(ctxt, di))
			clrText = BcColors::TEXT_ORPHAN;
		else if (di.diffcode.isResultDiff())
			clrText = BcColors::TEXT_DIFF;
		// else: identical — keep TEXT_NORMAL
		return;
	}

	// Map column side (0=left, 1=right) to DIFFITEM side index
	int side = (colSide == 0) ? 0 : (nDirs - 1);

	// Check if item exists on this side
	if (!di.diffcode.exists(side))
	{
		// Missing side — dimmed background, invisible text
		clrBk = bOddRow ? BcColors::BG_MISSING_ALT : BcColors::BG_MISSING;
		clrText = clrBk; // invisible
		return;
	}

	// Item exists on this side — color by status
	if (di.isEmpty())
		return;

	if (di.diffcode.isResultFiltered())
	{
		clrText = BcColors::TEXT_FILTERED;
	}
	else if (!IsItemExistAll(ctxt, di))
	{
		// Orphan — this side exists but the other doesn't
		clrText = BcColors::TEXT_ORPHAN;
	}
	else if (di.diffcode.isResultDiff())
	{
		clrText = BcColors::TEXT_DIFF;
	}
	// else: identical — keep TEXT_NORMAL (white)
}

/////////////////////////////////////////////////////////////////////////////
// Data management

DIFFITEM* CDirSxSUnifiedView::GetItemKey(int idx) const
{
	if (idx < 0 || idx >= static_cast<int>(m_listViewItems.size()))
		return nullptr;
	return reinterpret_cast<DIFFITEM*>(m_listViewItems[idx].lParam);
}

void CDirSxSUnifiedView::DeleteAllDisplayItems()
{
	m_listViewItems.clear();
	if (m_pList && m_pList->GetSafeHwnd())
	{
		m_pList->DeleteAllItems();
		m_pList->SetItemCount(0);
	}
}

/**
 * @brief Update the unified view from the coordinator's row mapping.
 * Every row has a DIFFITEM pointer (no placeholders in unified view
 * since both sides are shown in the same row).
 */
void CDirSxSUnifiedView::UpdateFromRowMapping()
{
	if (!m_pCoordinator || !m_pList)
		return;

	m_nCachedToleranceSecs = -1;

	m_pList->SetRedraw(FALSE);
	m_listViewItems.clear();

	const auto& rowMapping = m_pCoordinator->GetRowMapping();
	for (int i = 0; i < static_cast<int>(rowMapping.size()); ++i)
	{
		const auto& row = rowMapping[i];
		ListViewOwnerDataItem item;

		// In unified view, every row has the DIFFITEM (no placeholders)
		item.lParam = reinterpret_cast<LPARAM>(row.diffpos);
		item.iImage = I_IMAGECALLBACK;
		item.iIndent = row.indent;

		m_listViewItems.push_back(item);
	}

	m_pList->SetItemCount(static_cast<int>(m_listViewItems.size()));
	m_pList->SetRedraw(TRUE);
	m_pList->Invalidate();
}

/////////////////////////////////////////////////////////////////////////////
// Event handlers — double-click, key, size, timer

void CDirSxSUnifiedView::OnDblClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMIA = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	*pResult = 0;

	int nItem = pNMIA->iItem;
	if (nItem < 0)
		return;

	DIFFITEM *key = GetItemKey(nItem);
	if (key == nullptr)
		return;

	const DIFFITEM &di = GetDiffContext().GetDiffAt(key);
	if (di.diffcode.isDirectory())
	{
		ToggleExpandSubdir(nItem);
		return;
	}
	OpenSelectedItem();
}

void CDirSxSUnifiedView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	LVHITTESTINFO lvhti;
	lvhti.pt = point;
	m_pList->SubItemHitTest(&lvhti);
	if (lvhti.iItem >= 0)
	{
		DIFFITEM *key = GetItemKey(lvhti.iItem);
		if (key != nullptr)
		{
			const DIFFITEM &di = GetDiffContext().GetDiffAt(key);
			if (di.diffcode.isDirectory())
			{
				ToggleExpandSubdir(lvhti.iItem);
				return;
			}
			OpenSelectedItem();
		}
	}
}

void CDirSxSUnifiedView::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	if (!m_pList || !m_pList->GetSafeHwnd() || m_bResizing)
		return;

	CRect rc;
	m_pList->GetClientRect(&rc);
	int totalW = rc.Width();
	if (totalW < 100)
		return;

	m_bResizing = true;

	int cmpW = 30;
	int halfW = (totalW - cmpW) / 2;
	int nameW = halfW * 50 / 100;
	int sizeW = halfW * 20 / 100;
	int modW  = halfW - nameW - sizeW;

	m_pList->SetColumnWidth(COL_LEFT_NAME, nameW);
	m_pList->SetColumnWidth(COL_LEFT_SIZE, sizeW);
	m_pList->SetColumnWidth(COL_LEFT_MODIFIED, modW);
	m_pList->SetColumnWidth(COL_CMP, cmpW);
	m_pList->SetColumnWidth(COL_RIGHT_NAME, nameW);
	m_pList->SetColumnWidth(COL_RIGHT_SIZE, sizeW);
	m_pList->SetColumnWidth(COL_RIGHT_MODIFIED, modW);

	m_bResizing = false;
}

/**
 * @brief Handle UI update messages from the diff thread.
 */
LRESULT CDirSxSUnifiedView::OnUpdateUIMessage(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	CDirDoc *pDoc = GetDocument();
	if (pDoc == nullptr || m_pCoordinator == nullptr)
		return 0;

	if (wParam == CDiffThread::EVENT_COMPARE_COMPLETED)
	{
		pDoc->CompareReady();
		if (!pDoc->GetGeneratingReport())
			m_pCoordinator->Redisplay();
		m_pCoordinator->SetScanningInProgress(false);
		CDirFrame *pFrame = GetParentFrame();
		if (pFrame)
		{
			pFrame->HideScanProgressBar();
			// Log completion with summary
			const auto& counts = m_pCoordinator->GetStatusCounts();
			String msg = strutils::format(
				_T("Comparison complete. %d items: %d identical, %d different, %d left orphans, %d right orphans."),
				counts.nTotal, counts.nIdentical, counts.nDifferent,
				counts.nOrphanLeft, counts.nOrphanRight);
			pFrame->GetLogPanel().AppendMessage(msg);
			pFrame->SetStatus(m_pCoordinator->FormatStatusString().c_str());
		}
	}
	else if (wParam == CDiffThread::EVENT_COMPARE_PROGRESSED)
	{
		// Throttle progress updates — redisplay at most every 500ms
		SetTimer(TIMER_REDISPLAY, 500, nullptr);
		CDirFrame *pFrame = GetParentFrame();
		if (pFrame)
		{
			pFrame->UpdateScanProgressBar();
			// Update status text with progress
			const CDiffContext &ctxt = pDoc->GetDiffContext();
			if (ctxt.m_pCompareStats)
			{
				int nDone = ctxt.m_pCompareStats->GetComparedItems();
				int nTotal = ctxt.m_pCompareStats->GetTotalItems();
				if (nTotal > 0)
				{
					String statusMsg = strutils::format(_T("Comparing items... %d / %d"), nDone, nTotal);
					pFrame->SetStatus(statusMsg.c_str());
				}
			}
		}
	}
	else if (wParam == CDiffThread::EVENT_COLLECT_COMPLETED)
	{
		m_pCoordinator->SetScanningInProgress(true);
		m_pCoordinator->Redisplay();
		CDirFrame *pFrame = GetParentFrame();
		if (pFrame)
		{
			// Switch from marquee (scan phase) to determinate (compare phase)
			pFrame->SetScanProgressDeterminate();
			pFrame->GetLogPanel().AppendMessage(_T("Scanning complete. Starting comparison..."));
			pFrame->SetStatus(_T("Scanning complete. Comparing items..."));
		}
	}

	return 0;
}

void CDirSxSUnifiedView::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == TIMER_REDISPLAY)
	{
		KillTimer(TIMER_REDISPLAY);
		if (m_pCoordinator)
			m_pCoordinator->Redisplay();
	}
	else
	{
		CListView::OnTimer(nIDEvent);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Keyboard handler

void CDirSxSUnifiedView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_RETURN)
	{
		int nItem = m_pList->GetNextItem(-1, LVNI_FOCUSED);
		if (nItem >= 0)
		{
			DIFFITEM *key = GetItemKey(nItem);
			if (key)
			{
				const DIFFITEM &di = GetDiffContext().GetDiffAt(key);
				if (di.diffcode.isDirectory())
				{
					ToggleExpandSubdir(nItem);
					return;
				}
			}
		}
		OpenSelectedItem();
		return;
	}
	// Tree mode: Left collapses, Right expands
	if (nChar == VK_LEFT || nChar == VK_RIGHT)
	{
		int nItem = m_pList->GetNextItem(-1, LVNI_FOCUSED);
		if (nItem >= 0)
		{
			DIFFITEM *key = GetItemKey(nItem);
			if (key)
			{
				const DIFFITEM &di = GetDiffContext().GetDiffAt(key);
				if (di.diffcode.isDirectory())
				{
					if (nChar == VK_RIGHT)
						ExpandSubdir(nItem);
					else
						CollapseSubdir(nItem);
					return;
				}
			}
		}
	}
	__super::OnKeyDown(nChar, nRepCnt, nFlags);
}

/////////////////////////////////////////////////////////////////////////////
// Context menu — side-aware based on hit-tested column

void CDirSxSUnifiedView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (m_pList->GetItemCount() == 0)
		return;

	GetParentFrame()->ActivateFrame();

	// Determine which side was right-clicked based on column hit-test
	if (point.x != -1 && point.y != -1)
	{
		CPoint clientPt = point;
		m_pList->ScreenToClient(&clientPt);

		LVHITTESTINFO lvhti = {};
		lvhti.pt = clientPt;
		m_pList->SubItemHitTest(&lvhti);

		int colSide = GetColumnSide(lvhti.iSubItem);
		if (colSide == 0)
			m_nContextSide = 0;
		else if (colSide == 1)
			m_nContextSide = 1;
		// For center column, keep previous context side
	}

	CMenu menu;
	menu.CreatePopupMenu();

	menu.AppendMenu(MF_STRING, ID_DIR_SXS_OPEN_COMPARE, _("&Open Comparison").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_CROSS_COMPARE, _("Cross-&Compare Selected").c_str());
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_COPY, _("Cop&y to Other Side").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_MOVE, _("Mo&ve to Other Side").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_DELETE, _("&Delete").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_RENAME, _("Re&name\tF2").c_str());
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_SWAP_SIDES, _("S&wap Sides").c_str());
	menu.AppendMenu(MF_SEPARATOR);

	// Sync operations submenu
	CMenu syncMenu;
	syncMenu.CreatePopupMenu();
	syncMenu.AppendMenu(MF_STRING, ID_DIR_SXS_UPDATE_LEFT, _("Update &Left").c_str());
	syncMenu.AppendMenu(MF_STRING, ID_DIR_SXS_UPDATE_RIGHT, _("Update &Right").c_str());
	syncMenu.AppendMenu(MF_STRING, ID_DIR_SXS_UPDATE_BOTH, _("Update &Both").c_str());
	syncMenu.AppendMenu(MF_SEPARATOR);
	syncMenu.AppendMenu(MF_STRING, ID_DIR_SXS_MIRROR_LEFT, _("Mirror to Le&ft").c_str());
	syncMenu.AppendMenu(MF_STRING, ID_DIR_SXS_MIRROR_RIGHT, _("Mirror to Ri&ght").c_str());
	menu.AppendMenu(MF_POPUP, (UINT_PTR)syncMenu.m_hMenu, _("S&ynchronize").c_str());
	syncMenu.Detach();
	menu.AppendMenu(MF_SEPARATOR);

	menu.AppendMenu(MF_STRING, ID_DIR_SXS_EXCHANGE, _("E&xchange Sides").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_CHANGE_ATTRIBUTES, _("Change &Attributes...").c_str());
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_COMPARE_CONTENTS, _("Compare &Contents").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_CRC_COMPARE, _("CRC C&ompare").c_str());

	// Touch submenu
	CMenu touchMenu;
	touchMenu.CreatePopupMenu();
	touchMenu.AppendMenu(MF_STRING, ID_DIR_SXS_TOUCH_TIMESTAMPS, _("Copy to Other S&ide").c_str());
	touchMenu.AppendMenu(MF_STRING, ID_DIR_SXS_TOUCH_NOW, _("Set to &Now").c_str());
	touchMenu.AppendMenu(MF_STRING, ID_DIR_SXS_TOUCH_SPECIFIC, _("Set to S&pecific Time...").c_str());
	touchMenu.AppendMenu(MF_STRING, ID_DIR_SXS_TOUCH_FROM_OTHER, _("Copy &From Other Side").c_str());
	menu.AppendMenu(MF_POPUP, (UINT_PTR)touchMenu.m_hMenu, _("&Touch Timestamps").c_str());
	touchMenu.Detach();
	menu.AppendMenu(MF_SEPARATOR);

	// File operations
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_COPY_TO_FOLDER, _("Copy to &Folder...").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_MOVE_TO_FOLDER, _("Move to Fo&lder...").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_NEW_FOLDER, _("New Fol&der...\tInsert").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_DELETE_PERMANENT, _("Delete &Permanently\tShift+Del").c_str());
	menu.AppendMenu(MF_SEPARATOR);

	// Clipboard and info
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_COPY_PATH, _("Copy Pat&h\tCtrl+Shift+C").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_COPY_FILENAME, _("Copy File&name").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_COMPARE_INFO, _("Compare &Info...\tCtrl+I").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_EXCLUDE_PATTERN, _("E&xclude Pattern").c_str());
	menu.AppendMenu(MF_SEPARATOR);

	// Open with
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_OPEN_WITH_APP, _("Open with &App").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_OPEN_WITH, _("Open &With...").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_EXPLORER_MENU, _("Explorer Conte&xt Menu").c_str());
	menu.AppendMenu(MF_SEPARATOR);

	// Navigation
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_SET_BASE, _("Set as &Base").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_SET_BASE_OTHER, _("Set as Base (&Other Side)").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_ALIGN_WITH, _("Ali&gn With...").c_str());
	menu.AppendMenu(MF_SEPARATOR);

	// Advanced filter
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_ADV_FILTER, _("Advanced Fi&lter...").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_IGNORE_STRUCTURE, _("Ignore Folder St&ructure").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_ROW_STRIPES, _("Row Stri&pes").c_str());
	menu.AppendMenu(MF_SEPARATOR);

	// Session/Workspace submenu
	CMenu sessionMenu;
	sessionMenu.CreatePopupMenu();
	sessionMenu.AppendMenu(MF_STRING, ID_DIR_SXS_SESSION_SAVE, _("Save S&ession...").c_str());
	sessionMenu.AppendMenu(MF_STRING, ID_DIR_SXS_SESSION_LOAD, _("&Load Session...").c_str());
	sessionMenu.AppendMenu(MF_SEPARATOR);
	sessionMenu.AppendMenu(MF_STRING, ID_DIR_SXS_WORKSPACE_SAVE, _("Save &Workspace...").c_str());
	sessionMenu.AppendMenu(MF_STRING, ID_DIR_SXS_WORKSPACE_LOAD, _("Load W&orkspace...").c_str());
	menu.AppendMenu(MF_POPUP, (UINT_PTR)sessionMenu.m_hMenu, _("Session/Wor&kspace").c_str());
	sessionMenu.Detach();
	menu.AppendMenu(MF_SEPARATOR);

	menu.AppendMenu(MF_STRING, ID_DIR_SXS_SHOW_LOG, _("Show &Log...").c_str());
	menu.AppendMenu(MF_SEPARATOR);

	// Selection submenu
	CMenu selMenu;
	selMenu.CreatePopupMenu();
	selMenu.AppendMenu(MF_STRING, ID_DIR_SXS_SELECT_ALL, _("Select &All").c_str());
	selMenu.AppendMenu(MF_STRING, ID_DIR_SXS_SELECT_NEWER, _("Select &Newer").c_str());
	selMenu.AppendMenu(MF_STRING, ID_DIR_SXS_SELECT_ORPHANS, _("Select &Orphans").c_str());
	selMenu.AppendMenu(MF_STRING, ID_DIR_SXS_SELECT_DIFFERENT, _("Select &Different").c_str());
	selMenu.AppendMenu(MF_STRING, ID_DIR_SXS_INVERT_SELECTION, _("&Invert Selection").c_str());
	selMenu.AppendMenu(MF_SEPARATOR);
	selMenu.AppendMenu(MF_STRING, ID_DIR_SXS_SELECT_LEFT_ONLY, _("Select &Left Only").c_str());
	selMenu.AppendMenu(MF_STRING, ID_DIR_SXS_SELECT_RIGHT_ONLY, _("Select &Right Only").c_str());
	menu.AppendMenu(MF_POPUP, (UINT_PTR)selMenu.m_hMenu, _("Se&lection").c_str());
	selMenu.Detach();

	menu.AppendMenu(MF_SEPARATOR);

	// Auto-expand submenu
	CMenu autoExpandMenu;
	autoExpandMenu.CreatePopupMenu();
	autoExpandMenu.AppendMenu(MF_STRING, ID_DIR_SXS_AUTO_EXPAND_ALL, _("Expand &All").c_str());
	autoExpandMenu.AppendMenu(MF_STRING, ID_DIR_SXS_AUTO_EXPAND_DIFF, _("Expand &Differences Only").c_str());
	int autoExpandMode = GetOptionsMgr()->GetInt(OPT_DIRVIEW_SXS_AUTO_EXPAND_MODE);
	autoExpandMenu.CheckMenuRadioItem(ID_DIR_SXS_AUTO_EXPAND_ALL, ID_DIR_SXS_AUTO_EXPAND_DIFF,
		(autoExpandMode == 1) ? ID_DIR_SXS_AUTO_EXPAND_ALL : ID_DIR_SXS_AUTO_EXPAND_DIFF, MF_BYCOMMAND);
	menu.AppendMenu(MF_POPUP, (UINT_PTR)autoExpandMenu.m_hMenu, _("Auto-E&xpand").c_str());
	autoExpandMenu.Detach();
	menu.AppendMenu(MF_SEPARATOR);

	// Tree mode items
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_TOGGLE_TREE, _("&Tree Mode").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_EXPAND_ALL, _("E&xpand All").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_COLLAPSE_ALL, _("Co&llapse All").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_FLATTEN_MODE, _("&Flatten Mode").c_str());
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_REFRESH, _("Re&fresh\tF5").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_FIND_FILENAME, _("F&ind Filename...\tCtrl+F").c_str());
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_NEXT_DIFF, _("&Next Difference\tCtrl+N").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_PREV_DIFF, _("P&revious Difference\tCtrl+Shift+N").c_str());
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_GENERATE_REPORT, _("Generate &Report...").c_str());
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_FIND_NEXT, _("Find Ne&xt\tF3").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_FIND_PREV, _("Find Pre&vious\tShift+F3").c_str());
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_NAV_BACK, _("Navigate &Back\tAlt+Left").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_NAV_FORWARD, _("Navigate F&orward\tAlt+Right").c_str());
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_UP_LEVEL, _("Up &Level\tBackspace").c_str());
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_DIR_SXS_CUSTOMIZE_KEYS, _("Customize Ke&ys...").c_str());

	// Check marks for toggle items
	if (GetOptionsMgr()->GetBool(OPT_TREE_MODE))
		menu.CheckMenuItem(ID_DIR_SXS_TOGGLE_TREE, MF_CHECKED);
	if (GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_FLATTEN_MODE))
		menu.CheckMenuItem(ID_DIR_SXS_FLATTEN_MODE, MF_CHECKED);
	if (GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_IGNORE_FOLDER_STRUCTURE))
		menu.CheckMenuItem(ID_DIR_SXS_IGNORE_STRUCTURE, MF_CHECKED);
	if (m_bRowStripes)
		menu.CheckMenuItem(ID_DIR_SXS_ROW_STRIPES, MF_CHECKED);

	// Enable/disable navigation items
	if (m_pCoordinator)
	{
		if (!m_pCoordinator->CanNavigateBack())
			menu.EnableMenuItem(ID_DIR_SXS_NAV_BACK, MF_GRAYED);
		if (!m_pCoordinator->CanNavigateForward())
			menu.EnableMenuItem(ID_DIR_SXS_NAV_FORWARD, MF_GRAYED);
	}

	// Enable/disable items based on selection
	int nSel = m_pList->GetSelectedCount();
	if (nSel == 0)
	{
		menu.EnableMenuItem(ID_DIR_SXS_OPEN_COMPARE, MF_GRAYED);
		menu.EnableMenuItem(ID_DIR_SXS_CROSS_COMPARE, MF_GRAYED);
		menu.EnableMenuItem(ID_DIR_SXS_COPY, MF_GRAYED);
		menu.EnableMenuItem(ID_DIR_SXS_MOVE, MF_GRAYED);
		menu.EnableMenuItem(ID_DIR_SXS_DELETE, MF_GRAYED);
		menu.EnableMenuItem(ID_DIR_SXS_RENAME, MF_GRAYED);
		menu.EnableMenuItem(ID_DIR_SXS_CRC_COMPARE, MF_GRAYED);
		menu.EnableMenuItem(ID_DIR_SXS_TOUCH_TIMESTAMPS, MF_GRAYED);
	}
	if (nSel != 1)
		menu.EnableMenuItem(ID_DIR_SXS_RENAME, MF_GRAYED);

	if (point.x == -1 && point.y == -1)
	{
		CRect rect;
		GetClientRect(rect);
		ClientToScreen(rect);
		point = rect.TopLeft();
		point.Offset(5, 5);
	}

	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
}

/////////////////////////////////////////////////////////////////////////////
// Open comparison

void CDirSxSUnifiedView::OpenSelectedItem()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();

	int nItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
	if (nItem < 0)
		return;

	DIFFITEM *key = GetItemKey(nItem);
	if (!key)
		return;

	const DIFFITEM &di = ctxt.GetDiffAt(key);

	if (di.diffcode.isDirectory())
	{
		ToggleExpandSubdir(nItem);
		return;
	}

	PathContext paths = GetItemFileNames(ctxt, di);
	int nDirs = ctxt.GetCompareDirs();

	const String sUntitled[] = { _("Untitled Left"),
		nDirs < 3 ? _("Untitled Right") : _("Untitled Middle"),
		_("Untitled Right") };
	PathContext filteredPaths;
	FileLocation fileloc[3];
	String strDesc[3];
	fileopenflags_t dwFlags[3] = {};

	for (int i = 0; i < nDirs; i++)
	{
		dwFlags[i] = FFILEOPEN_NOMRU | (pDoc->GetReadOnly(i) ? FFILEOPEN_READONLY : 0);
		if (di.diffcode.exists(i) && paths::DoesPathExist(paths[i]) != paths::DOES_NOT_EXIST)
		{
			fileloc[i].setPath(paths[i]);
			fileloc[i].encoding = di.diffFileInfo[i].encoding;
			filteredPaths.SetPath(filteredPaths.GetSize(), paths[i], false);
		}
		else
		{
			strDesc[i] = sUntitled[i];
			filteredPaths.SetPath(filteredPaths.GetSize(), paths::NATIVE_NULL_DEVICE_NAME, false);
		}
	}

	PackingInfo *infoUnpacker = nullptr;
	PrediffingInfo *infoPrediffer = nullptr;
	String filteredFilenames = CDiffContext::GetFilteredFilenames(filteredPaths);
	GetDiffContext().FetchPluginInfos(filteredFilenames, &infoUnpacker, &infoPrediffer);

	GetMainFrame()->ShowAutoMergeDoc(0, pDoc, nDirs, fileloc,
		dwFlags, strDesc, _T(""), infoUnpacker, infoPrediffer);
}

void CDirSxSUnifiedView::OpenCrossComparison()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();

	std::vector<DIFFITEM*> leftItems, rightItems;
	m_pCoordinator->GetSelectedItems(0, leftItems);
	m_pCoordinator->GetSelectedItems(1, rightItems);

	if (leftItems.empty() || rightItems.empty())
		return;

	const DIFFITEM &diLeft = ctxt.GetDiffAt(leftItems[0]);
	const DIFFITEM &diRight = ctxt.GetDiffAt(rightItems[0]);

	PathContext paths;
	int leftSide = 0;
	int rightSide = ctxt.GetCompareDirs() - 1;

	String leftPath = diLeft.getFilepath(leftSide, ctxt.GetPath(leftSide));
	String rightPath = diRight.getFilepath(rightSide, ctxt.GetPath(rightSide));

	paths.SetPath(0, leftPath);
	paths.SetPath(1, rightPath);

	fileopenflags_t dwFlags[3] = {};
	GetMainFrame()->DoFileOrFolderOpen(&paths, dwFlags, nullptr, _T(""), false, nullptr);
}

/////////////////////////////////////////////////////////////////////////////
// Command handlers — swap, copy, move, open

void CDirSxSUnifiedView::OnSxsSwapSides()
{
	if (m_pCoordinator)
		m_pCoordinator->SwapSides();
}

void CDirSxSUnifiedView::OnSxsCopy()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	std::vector<DIFFITEM*> items;
	m_pCoordinator->GetSelectedItems(m_nContextSide, items);
	if (items.empty())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int srcSide = m_nContextSide;
	int dstSide = (m_nContextSide == 0) ? (ctxt.GetCompareDirs() - 1) : 0;

	ShellFileOperations fileOps;
	for (auto *key : items)
	{
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (!di.diffcode.exists(srcSide))
			continue;

		String srcPath = di.getFilepath(srcSide, ctxt.GetPath(srcSide));
		String dstDir;
		if (di.diffcode.exists(dstSide))
			dstDir = paths::GetParentPath(di.getFilepath(dstSide, ctxt.GetPath(dstSide)));
		else
		{
			String relPath = di.diffFileInfo[srcSide].path;
			dstDir = paths::ConcatPath(ctxt.GetPath(dstSide), relPath);
		}
		String dstPath = paths::ConcatPath(dstDir, di.diffFileInfo[srcSide].filename);
		fileOps.AddSourceAndDestination(srcPath, dstPath);
	}

	fileOps.SetOperation(FO_COPY, FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR, GetSafeHwnd());
	if (fileOps.Run() && !fileOps.IsCanceled())
	{
		if (m_pCoordinator)
			m_pCoordinator->LogOperation(strutils::format(_T("Copied %d item(s) to other side"), static_cast<int>(items.size())));
		pDoc->Rescan();
	}
}

void CDirSxSUnifiedView::OnSxsMove()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	std::vector<DIFFITEM*> items;
	m_pCoordinator->GetSelectedItems(m_nContextSide, items);
	if (items.empty())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int srcSide = m_nContextSide;
	int dstSide = (m_nContextSide == 0) ? (ctxt.GetCompareDirs() - 1) : 0;

	ShellFileOperations fileOps;
	for (auto *key : items)
	{
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (!di.diffcode.exists(srcSide))
			continue;

		String srcPath = di.getFilepath(srcSide, ctxt.GetPath(srcSide));
		String dstDir;
		if (di.diffcode.exists(dstSide))
			dstDir = paths::GetParentPath(di.getFilepath(dstSide, ctxt.GetPath(dstSide)));
		else
		{
			String relPath = di.diffFileInfo[srcSide].path;
			dstDir = paths::ConcatPath(ctxt.GetPath(dstSide), relPath);
		}
		String dstPath = paths::ConcatPath(dstDir, di.diffFileInfo[srcSide].filename);
		fileOps.AddSourceAndDestination(srcPath, dstPath);
	}

	fileOps.SetOperation(FO_MOVE, FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR, GetSafeHwnd());
	if (fileOps.Run() && !fileOps.IsCanceled())
	{
		if (m_pCoordinator)
			m_pCoordinator->LogOperation(strutils::format(_T("Moved %d item(s) to other side"), static_cast<int>(items.size())));
		pDoc->Rescan();
	}
}

void CDirSxSUnifiedView::OnSxsOpenCompare()
{
	OpenSelectedItem();
}

void CDirSxSUnifiedView::OnSxsCrossCompare()
{
	OpenCrossComparison();
}

void CDirSxSUnifiedView::OnUpdateSxsNeedSelection(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_pList && m_pList->GetSelectedCount() > 0);
}

/////////////////////////////////////////////////////////////////////////////
// Column header click — sort

void CDirSxSUnifiedView::OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	if (!m_pCoordinator || !m_pColItems)
		return;

	NM_LISTVIEW *pNMListView = (NM_LISTVIEW *)pNMHDR;
	int clickedCol = pNMListView->iSubItem;

	// Map unified column to logical sort column
	// For unified view, sort by the underlying property (Name, Size, Modified)
	// Map columns 0,4 -> Name(0), 1,5 -> Size(2), 2,6 -> Modified(3), 3 -> ignore
	int sortcol = -1;
	switch (clickedCol)
	{
	case COL_LEFT_NAME:
	case COL_RIGHT_NAME:
		sortcol = 0; // Name
		break;
	case COL_LEFT_SIZE:
	case COL_RIGHT_SIZE:
		sortcol = 2; // Size
		break;
	case COL_LEFT_MODIFIED:
	case COL_RIGHT_MODIFIED:
		sortcol = 3; // Modified
		break;
	case COL_CMP:
		return; // Can't sort by comparison indicator
	}

	if (sortcol < 0 || sortcol >= m_pColItems->GetColCount())
		return;

	int oldSortCol = m_pCoordinator->GetSortColumn();
	bool bAscending;
	if (sortcol == oldSortCol)
	{
		bAscending = !m_pCoordinator->GetSortAscending();
	}
	else
	{
		bAscending = m_pColItems->IsDefaultSortAscending(sortcol);
	}

	m_pCoordinator->SetSortColumn(sortcol, bAscending);
	UpdateSortHeaderIndicator();
}

void CDirSxSUnifiedView::UpdateSortHeaderIndicator()
{
	if (!m_pCoordinator || !m_pColItems)
		return;

	int sortCol = m_pCoordinator->GetSortColumn();
	if (sortCol < 0)
	{
		m_ctlSortHeader.SetSortImage(-1, true);
		return;
	}

	// Map logical sort column back to unified column for the header arrow
	int physCol = -1;
	switch (sortCol)
	{
	case 0: physCol = COL_LEFT_NAME; break;
	case 2: physCol = COL_LEFT_SIZE; break;
	case 3: physCol = COL_LEFT_MODIFIED; break;
	default: physCol = -1; break;
	}

	m_ctlSortHeader.SetSortImage(physCol, m_pCoordinator->GetSortAscending());
}

/////////////////////////////////////////////////////////////////////////////
// Selection change — update status bar

void CDirSxSUnifiedView::OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLISTVIEW *pNMLV = (NMLISTVIEW *)pNMHDR;
	*pResult = 0;

	if (!(pNMLV->uChanged & LVIF_STATE))
		return;
	if ((pNMLV->uNewState & LVIS_SELECTED) == (pNMLV->uOldState & LVIS_SELECTED))
		return;

	// Update status bar
	CDirFrame *pFrame = GetParentFrame();
	if (m_pCoordinator && m_pList && pFrame)
	{
		int nSelCount = m_pList->GetSelectedCount();
		if (nSelCount == 1)
		{
			int nSelItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
			String detail = m_pCoordinator->FormatSelectionDetailString(nSelItem);
			if (!detail.empty())
				pFrame->SetStatus(detail.c_str());
			else
				pFrame->SetStatus(m_pCoordinator->FormatStatusString().c_str());
		}
		else
		{
			pFrame->SetStatus(m_pCoordinator->FormatStatusString().c_str());
		}
	}
}

/**
 * @brief Handle NM_CLICK to track which side (left/right) was clicked.
 *
 * Uses SubItemHitTest to determine which column was clicked, then sets
 * m_nActiveSide accordingly. This drives the split-selection highlight
 * in OnCustomDraw — only the active side's columns show the selection bar.
 */
void CDirSxSUnifiedView::OnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMITEMACTIVATE *pNMIA = (NMITEMACTIVATE *)pNMHDR;
	*pResult = 0;

	LVHITTESTINFO lvhti = {};
	lvhti.pt = pNMIA->ptAction;
	m_pList->SubItemHitTest(&lvhti);

	int colSide = GetColumnSide(lvhti.iSubItem);
	if (colSide == 0)
		m_nActiveSide = 0;
	else if (colSide == 1)
		m_nActiveSide = 1;
	// Center column: keep previous active side

	// Also update context side to match (for operations triggered from toolbar etc.)
	m_nContextSide = m_nActiveSide;

	// Force redraw so split selection highlight updates
	m_pList->Invalidate(FALSE);
}

/////////////////////////////////////////////////////////////////////////////
// Tree mode expand/collapse

/**
 * @brief Compare any NOCMP children of a folder inline.
 * When expanding a folder whose children were discovered but not compared
 * (NOCMP status), this performs the comparison so the Cmp column updates.
 */
void CDirSxSUnifiedView::CompareExpandedChildren(DIFFITEM &parentDi)
{
	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;
	if (!parentDi.HasChildren())
		return;

	CDiffContext &ctxt = GetDiffContext();
	FolderCmp fc(&ctxt);

	DIFFITEM *childPos = ctxt.GetFirstChildDiffPosition(&parentDi);
	while (childPos != nullptr)
	{
		DIFFITEM &child = ctxt.GetNextSiblingDiffRefPosition(childPos);
		if (child.diffcode.isDirectory())
			continue;
		// Only compare items that haven't been compared yet
		unsigned cmpFlags = child.diffcode.diffcode & DIFFCODE::COMPAREFLAGS;
		if (cmpFlags != DIFFCODE::NOCMP)
			continue;
		// Only compare items that exist on both sides
		if (!child.diffcode.existAll())
			continue;

		child.diffcode.diffcode &= ~DIFFCODE::NEEDSCAN;
		if (!child.diffcode.isResultFiltered())
		{
			child.diffcode.diffcode |= fc.prepAndCompareFiles(child);
			child.nsdiffs = fc.m_ndiffs;
			child.nidiffs = fc.m_ntrivialdiffs;
			int nDirs = ctxt.GetCompareDirs();
			for (int i = 0; i < nDirs; ++i)
			{
				if (child.diffcode.exists(i))
				{
					child.diffFileInfo[i].m_textStats = fc.m_diffFileData.m_textStats[i];
					child.diffFileInfo[i].encoding = fc.m_diffFileData.m_FileLocation[i].encoding;
				}
			}
		}
	}
}

void CDirSxSUnifiedView::ExpandSubdir(int sel)
{
	if (!m_pCoordinator)
		return;
	DIFFITEM *key = GetItemKey(sel);
	if (!key)
		return;
	DIFFITEM &di = GetDiffContext().GetDiffRefAt(key);
	if (di.diffcode.isDirectory())
	{
		di.customFlags |= ViewCustomFlags::EXPANDED;
		CDirDoc *pDoc = GetDocument();
		if (pDoc && !di.HasChildren())
		{
			auto state = pDoc->m_diffThread.GetThreadState();
			if (state == CDiffThread::THREAD_COMPARING)
			{
				pDoc->m_diffThread.RequestPriorityScan(key);
			}
			else if (state == CDiffThread::THREAD_COMPLETED)
			{
				pDoc->Rescan();
				return;
			}
		}
		// Compare any NOCMP children so Cmp column updates
		CompareExpandedChildren(di);
		m_pCoordinator->InvalidateFolderStatusCacheFor(key);
		m_pCoordinator->Redisplay();
	}
}

void CDirSxSUnifiedView::CollapseSubdir(int sel)
{
	if (!m_pCoordinator)
		return;
	DIFFITEM *key = GetItemKey(sel);
	if (!key)
		return;
	DIFFITEM &di = GetDiffContext().GetDiffRefAt(key);
	if (di.diffcode.isDirectory())
	{
		di.customFlags &= ~ViewCustomFlags::EXPANDED;
		m_pCoordinator->Redisplay();
	}
}

void CDirSxSUnifiedView::ToggleExpandSubdir(int sel)
{
	if (!m_pCoordinator)
		return;
	DIFFITEM *key = GetItemKey(sel);
	if (!key)
		return;
	DIFFITEM &di = GetDiffContext().GetDiffRefAt(key);
	if (!di.diffcode.isDirectory())
		return;
	if (di.customFlags & ViewCustomFlags::EXPANDED)
		di.customFlags &= ~ViewCustomFlags::EXPANDED;
	else
	{
		di.customFlags |= ViewCustomFlags::EXPANDED;
		CDirDoc *pDoc = GetDocument();
		if (pDoc && !di.HasChildren())
		{
			auto state = pDoc->m_diffThread.GetThreadState();
			if (state == CDiffThread::THREAD_COMPARING)
			{
				pDoc->m_diffThread.RequestPriorityScan(key);
			}
			else if (state == CDiffThread::THREAD_COMPLETED)
			{
				pDoc->Rescan();
				return;
			}
		}
		// Compare any NOCMP children so Cmp column updates
		CompareExpandedChildren(di);
		m_pCoordinator->InvalidateFolderStatusCacheFor(key);
	}
	m_pCoordinator->Redisplay();
}

void CDirSxSUnifiedView::OnExpandAllSubdirs()
{
	if (!m_pCoordinator)
		return;
	CDiffContext &ctxt = GetDiffContext();
	DIFFITEM *pos = ctxt.GetFirstDiffPosition();
	while (pos != nullptr)
	{
		DIFFITEM &di = ctxt.GetNextDiffRefPosition(pos);
		if (di.HasChildren())
			di.customFlags |= ViewCustomFlags::EXPANDED;
	}
	m_pCoordinator->Redisplay();
}

void CDirSxSUnifiedView::OnCollapseAllSubdirs()
{
	if (!m_pCoordinator)
		return;
	CDiffContext &ctxt = GetDiffContext();
	DIFFITEM *pos = ctxt.GetFirstDiffPosition();
	while (pos != nullptr)
	{
		DIFFITEM &di = ctxt.GetNextDiffRefPosition(pos);
		if (di.HasChildren())
			di.customFlags &= ~ViewCustomFlags::EXPANDED;
	}
	m_pCoordinator->Redisplay();
}

/////////////////////////////////////////////////////////////////////////////
// Tree/flatten mode toggles

void CDirSxSUnifiedView::OnSxsToggleTree()
{
	bool bCurrent = GetOptionsMgr()->GetBool(OPT_TREE_MODE);
	GetOptionsMgr()->SaveOption(OPT_TREE_MODE, !bCurrent);
	if (m_pCoordinator)
		m_pCoordinator->Redisplay();
}

void CDirSxSUnifiedView::OnSxsExpandAll()
{
	OnExpandAllSubdirs();
}

void CDirSxSUnifiedView::OnSxsCollapseAll()
{
	OnCollapseAllSubdirs();
}

void CDirSxSUnifiedView::OnSxsFlattenMode()
{
	bool bCurrent = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_FLATTEN_MODE);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_FLATTEN_MODE, !bCurrent);
	if (m_pCoordinator)
		m_pCoordinator->Redisplay();
}

void CDirSxSUnifiedView::OnUpdateSxsToggleTree(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_TREE_MODE));
}

void CDirSxSUnifiedView::OnUpdateSxsFlattenMode(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_FLATTEN_MODE));
}

/////////////////////////////////////////////////////////////////////////////
// Refresh, rename, find

void CDirSxSUnifiedView::OnSxsRefresh()
{
	CDirDoc *pDoc = GetDocument();
	if (pDoc)
		pDoc->Rescan();
}

void CDirSxSUnifiedView::OnSxsRename()
{
	if (!m_pList)
		return;
	int nItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
	if (nItem >= 0)
	{
		DIFFITEM *key = GetItemKey(nItem);
		if (key != nullptr)
			m_pList->EditLabel(nItem);
	}
}

void CDirSxSUnifiedView::OnEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVDISPINFO *pDispInfo = (NMLVDISPINFO *)pNMHDR;
	*pResult = FALSE;

	if (pDispInfo->item.pszText == nullptr)
		return;

	int nItem = pDispInfo->item.iItem;
	DIFFITEM *key = GetItemKey(nItem);
	if (!key)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	CDiffContext &ctxt = GetDiffContext();
	DIFFITEM &di = ctxt.GetDiffRefAt(key);

	String newName = pDispInfo->item.pszText;
	if (newName.empty())
		return;

	// Rename on the context side
	int side = m_nContextSide;
	if (side >= ctxt.GetCompareDirs())
		side = ctxt.GetCompareDirs() - 1;

	if (!di.diffcode.exists(side))
		return;

	String oldPath = di.getFilepath(side, ctxt.GetPath(side));
	String dir = paths::GetParentPath(oldPath);
	String newPath = paths::ConcatPath(dir, newName);

	if (MoveFile(oldPath.c_str(), newPath.c_str()))
	{
		*pResult = TRUE;
		pDoc->Rescan();
	}
	else
	{
		String msg = strutils::format(_T("Failed to rename '%s' to '%s'"),
			oldPath.c_str(), newPath.c_str());
		AfxMessageBox(msg.c_str(), MB_ICONERROR);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Find filename dialog helpers (shared with DirPaneView)

static INT_PTR CALLBACK UnifiedFindFilenameDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			SetWindowLongPtr(hDlg, DWLP_USER, lParam);
			tchar_t *pBuf = reinterpret_cast<tchar_t*>(lParam);
			if (pBuf && pBuf[0])
			{
				::SetDlgItemTextW(hDlg, 1001, pBuf);
				SendDlgItemMessage(hDlg, 1001, EM_SETSEL, 0, -1);
			}
			SetFocus(GetDlgItem(hDlg, 1001));
		}
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				tchar_t *pBuf = reinterpret_cast<tchar_t*>(GetWindowLongPtr(hDlg, DWLP_USER));
				if (pBuf)
					::GetDlgItemTextW(hDlg, 1001, pBuf, MAX_PATH);
			}
			EndDialog(hDlg, IDOK);
			return TRUE;
		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static DLGTEMPLATE* BuildUnifiedFindDlgTemplate(BYTE* buffer, size_t bufSize)
{
	memset(buffer, 0, bufSize);
	const int DLG_W = 260, DLG_H = 75;

	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)buffer;
	pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
	pDlg->cdit = 4;
	pDlg->x = 0; pDlg->y = 0;
	pDlg->cx = DLG_W; pDlg->cy = DLG_H;

	WORD* pw = (WORD*)(pDlg + 1);
	*pw++ = 0;
	*pw++ = 0;
	const wchar_t dlgTitle[] = L"Find Filename";
	memcpy(pw, dlgTitle, sizeof(dlgTitle));
	pw += _countof(dlgTitle);
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	// Static label
	DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
	pItem->x = 7; pItem->y = 7; pItem->cx = DLG_W - 14; pItem->cy = 10;
	pItem->id = 0xFFFF;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0082;
	const wchar_t label[] = L"Filename:";
	memcpy(pw, label, sizeof(label));
	pw += _countof(label);
	*pw++ = 0;
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	// Edit control (id=1001)
	pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
	pItem->x = 7; pItem->y = 20; pItem->cx = DLG_W - 14; pItem->cy = 14;
	pItem->id = 1001;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0081;
	*pw++ = 0;
	*pw++ = 0;
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	// OK button
	pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
	pItem->x = DLG_W - 120; pItem->y = DLG_H - 20; pItem->cx = 50; pItem->cy = 14;
	pItem->id = IDOK;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0080;
	const wchar_t ok[] = L"OK";
	memcpy(pw, ok, sizeof(ok));
	pw += _countof(ok);
	*pw++ = 0;
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	// Cancel button
	pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
	pItem->x = DLG_W - 60; pItem->y = DLG_H - 20; pItem->cx = 50; pItem->cy = 14;
	pItem->id = IDCANCEL;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0080;
	const wchar_t cancel[] = L"Cancel";
	memcpy(pw, cancel, sizeof(cancel));
	pw += _countof(cancel);
	*pw++ = 0;

	return pDlg;
}

void CDirSxSUnifiedView::OnSxsFindFilename()
{
	if (!m_pCoordinator || !m_pList)
		return;

	static String s_lastSearch;
	tchar_t szInput[MAX_PATH] = {};
	if (!s_lastSearch.empty())
		_tcsncpy_s(szInput, s_lastSearch.c_str(), _TRUNCATE);

	BYTE dlgBuf[1024];
	DLGTEMPLATE* pDlgTmpl = BuildUnifiedFindDlgTemplate(dlgBuf, sizeof(dlgBuf));

	INT_PTR nResult = DialogBoxIndirectParam(AfxGetInstanceHandle(), pDlgTmpl,
		m_hWnd, UnifiedFindFilenameDlgProc, reinterpret_cast<LPARAM>(szInput));

	if (nResult != IDOK)
		return;

	String searchText = szInput;
	if (searchText.empty())
		return;
	s_lastSearch = searchText;
	m_sFindPattern = searchText;

	CDiffContext &ctxt = GetDiffContext();
	String searchLower = searchText;
	CharLower(&searchLower[0]);

	int nStart = m_pList->GetNextItem(-1, LVNI_FOCUSED);
	if (nStart < 0) nStart = 0;
	int nCount = static_cast<int>(m_listViewItems.size());

	for (int offset = 1; offset <= nCount; offset++)
	{
		int i = (nStart + offset) % nCount;
		DIFFITEM *key = GetItemKey(i);
		if (!key)
			continue;

		const DIFFITEM &di = ctxt.GetDiffAt(key);

		// Search both sides for matching filename
		for (int side = 0; side < ctxt.GetCompareDirs(); side++)
		{
			if (!di.diffcode.exists(side))
				continue;

			String filename = String(di.diffFileInfo[side].filename);
			CharLower(&filename[0]);

			if (filename.find(searchLower) != String::npos)
			{
				m_pList->SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
				m_pList->SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				m_pList->EnsureVisible(i, FALSE);
				return;
			}
		}
	}

	AfxMessageBox(_("Filename not found.").c_str(), MB_ICONINFORMATION);
}

/////////////////////////////////////////////////////////////////////////////
// Column state save

void CDirSxSUnifiedView::SaveColumnState()
{
	// For unified view, save column widths under a unified option
	// (Not using per-pane options since there's only one view)
	if (!m_pList)
		return;

	String sWidths;
	for (int i = 0; i < COL_COUNT; i++)
	{
		if (i > 0) sWidths += _T(" ");
		sWidths += strutils::format(_T("%d"), m_pList->GetColumnWidth(i));
	}
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_LEFT_COLUMN_WIDTHS, sWidths);
}

/////////////////////////////////////////////////////////////////////////////
// Selection commands

void CDirSxSUnifiedView::OnSxsSelectAll()
{
	if (!m_pList)
		return;
	for (int i = 0; i < static_cast<int>(m_listViewItems.size()); i++)
	{
		DIFFITEM *key = GetItemKey(i);
		if (key != nullptr)
			m_pList->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
	}
}

void CDirSxSUnifiedView::OnSxsSelectNewer()
{
	if (!m_pList || !GetDocument() || !GetDocument()->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int leftSide = 0;
	int rightSide = ctxt.GetCompareDirs() - 1;
	int toleranceSecs = GetOptionsMgr()->GetInt(OPT_CMP_IGNORE_SMALL_FILETIME_SECS);

	m_pList->SetItemState(-1, 0, LVIS_SELECTED);
	for (int i = 0; i < static_cast<int>(m_listViewItems.size()); i++)
	{
		DIFFITEM *key = GetItemKey(i);
		if (!key)
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (!IsItemExistAll(ctxt, di) || !di.diffcode.isResultDiff())
			continue;

		// Select items where the context side is newer
		int thisSide = m_nContextSide;
		int otherSide = (thisSide == 0) ? rightSide : leftSide;
		Poco::Timestamp::TimeDiff diff = di.diffFileInfo[thisSide].mtime - di.diffFileInfo[otherSide].mtime;
		Poco::Timestamp::TimeDiff toleranceUs = static_cast<Poco::Timestamp::TimeDiff>(toleranceSecs) * Poco::Timestamp::resolution();
		if (diff > toleranceUs)
			m_pList->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
	}
}

void CDirSxSUnifiedView::OnSxsSelectOrphans()
{
	if (!m_pList || !GetDocument() || !GetDocument()->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	m_pList->SetItemState(-1, 0, LVIS_SELECTED);
	for (int i = 0; i < static_cast<int>(m_listViewItems.size()); i++)
	{
		DIFFITEM *key = GetItemKey(i);
		if (!key)
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (!IsItemExistAll(ctxt, di))
			m_pList->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
	}
}

void CDirSxSUnifiedView::OnSxsSelectDifferent()
{
	if (!m_pList || !GetDocument() || !GetDocument()->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	m_pList->SetItemState(-1, 0, LVIS_SELECTED);
	for (int i = 0; i < static_cast<int>(m_listViewItems.size()); i++)
	{
		DIFFITEM *key = GetItemKey(i);
		if (!key)
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (di.diffcode.isResultDiff())
			m_pList->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
	}
}

void CDirSxSUnifiedView::OnSxsInvertSelection()
{
	if (!m_pList)
		return;
	for (int i = 0; i < static_cast<int>(m_listViewItems.size()); i++)
	{
		DIFFITEM *key = GetItemKey(i);
		if (!key)
			continue;
		UINT state = m_pList->GetItemState(i, LVIS_SELECTED);
		m_pList->SetItemState(i, state ? 0 : LVIS_SELECTED, LVIS_SELECTED);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Next/Previous difference navigation

void CDirSxSUnifiedView::OnSxsNextDiff()
{
	if (!m_pCoordinator || !m_pList)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int nStart = m_pList->GetNextItem(-1, LVNI_FOCUSED);
	int nCount = static_cast<int>(m_listViewItems.size());

	for (int offset = 1; offset <= nCount; offset++)
	{
		int i = (nStart + offset) % nCount;
		DIFFITEM *key = GetItemKey(i);
		if (!key)
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (di.diffcode.isResultDiff() || !IsItemExistAll(ctxt, di))
		{
			m_pList->SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
			m_pList->SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			m_pList->EnsureVisible(i, FALSE);
			return;
		}
	}
}

void CDirSxSUnifiedView::OnSxsPrevDiff()
{
	if (!m_pCoordinator || !m_pList)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int nStart = m_pList->GetNextItem(-1, LVNI_FOCUSED);
	int nCount = static_cast<int>(m_listViewItems.size());
	if (nStart < 0) nStart = 0;

	for (int offset = 1; offset <= nCount; offset++)
	{
		int i = (nStart - offset + nCount) % nCount;
		DIFFITEM *key = GetItemKey(i);
		if (!key)
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (di.diffcode.isResultDiff() || !IsItemExistAll(ctxt, di))
		{
			m_pList->SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
			m_pList->SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			m_pList->EnsureVisible(i, FALSE);
			return;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// Delete

void CDirSxSUnifiedView::OnSxsDelete()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	std::vector<DIFFITEM*> items;
	m_pCoordinator->GetSelectedItems(m_nContextSide, items);
	if (items.empty())
		return;

	String msg = strutils::format(_T("Delete %d selected item(s) from %s side?"),
		static_cast<int>(items.size()),
		m_nContextSide == 0 ? _T("left") : _T("right"));
	if (AfxMessageBox(msg.c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int side = m_nContextSide;

	ShellFileOperations fileOps;
	for (auto *key : items)
	{
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (!di.diffcode.exists(side))
			continue;
		String path = di.getFilepath(side, ctxt.GetPath(side));
		fileOps.AddSource(path);
	}

	fileOps.SetOperation(FO_DELETE, FOF_ALLOWUNDO, GetSafeHwnd());
	if (fileOps.Run() && !fileOps.IsCanceled())
	{
		if (m_pCoordinator)
			m_pCoordinator->LogOperation(strutils::format(_T("Deleted %d item(s)"), static_cast<int>(items.size())));
		pDoc->Rescan();
	}
}

/////////////////////////////////////////////////////////////////////////////
// Sync operations

void CDirSxSUnifiedView::OnSxsUpdateLeft()
{
	if (m_pCoordinator)
	{
		if (AfxMessageBox(_T("Update Left: Copy newer and orphan files from right to left?"), MB_YESNO | MB_ICONQUESTION) == IDYES)
			m_pCoordinator->UpdateLeft();
	}
}

void CDirSxSUnifiedView::OnSxsUpdateRight()
{
	if (m_pCoordinator)
	{
		if (AfxMessageBox(_T("Update Right: Copy newer and orphan files from left to right?"), MB_YESNO | MB_ICONQUESTION) == IDYES)
			m_pCoordinator->UpdateRight();
	}
}

void CDirSxSUnifiedView::OnSxsUpdateBoth()
{
	if (m_pCoordinator)
	{
		if (AfxMessageBox(_T("Update Both: Copy newer and orphan files in both directions?"), MB_YESNO | MB_ICONQUESTION) == IDYES)
			m_pCoordinator->UpdateBoth();
	}
}

void CDirSxSUnifiedView::OnSxsMirrorLeft()
{
	if (m_pCoordinator)
	{
		if (AfxMessageBox(_T("Mirror to Left: Make left side identical to right side?\nThis will copy different files and delete left-only orphans."), MB_YESNO | MB_ICONWARNING) == IDYES)
			m_pCoordinator->MirrorLeft();
	}
}

void CDirSxSUnifiedView::OnSxsMirrorRight()
{
	if (m_pCoordinator)
	{
		if (AfxMessageBox(_T("Mirror to Right: Make right side identical to left side?\nThis will copy different files and delete right-only orphans."), MB_YESNO | MB_ICONWARNING) == IDYES)
			m_pCoordinator->MirrorRight();
	}
}

void CDirSxSUnifiedView::OnSxsCompareContents()
{
	CDirDoc *pDoc = GetDocument();
	if (pDoc)
		pDoc->Rescan();
}

/////////////////////////////////////////////////////////////////////////////
// CRC Compare

void CDirSxSUnifiedView::OnSxsCrcCompare()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	std::vector<DIFFITEM*> items;
	m_pCoordinator->GetSelectedItems(m_nContextSide, items);
	if (items.empty())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int leftSide = 0;
	int rightSide = ctxt.GetCompareDirs() - 1;

	String result;
	int nMatch = 0, nDiffer = 0, nSingleSide = 0;

	for (auto *key : items)
	{
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (di.diffcode.isDirectory())
			continue;

		String filename = di.diffFileInfo[di.diffcode.exists(leftSide) ? leftSide : rightSide].filename;

		if (di.diffcode.exists(leftSide) && di.diffcode.exists(rightSide))
		{
			String leftPath = di.getFilepath(leftSide, ctxt.GetPath(leftSide));
			String rightPath = di.getFilepath(rightSide, ctxt.GetPath(rightSide));

			DWORD crcLeft = CDirSideBySideCoordinator::ComputeCRC32(leftPath);
			DWORD crcRight = CDirSideBySideCoordinator::ComputeCRC32(rightPath);

			bool bMatch = (crcLeft == crcRight);
			if (bMatch) nMatch++; else nDiffer++;

			result += strutils::format(_T("%s: L=%08X  R=%08X  %s\r\n"),
				filename.c_str(), crcLeft, crcRight,
				bMatch ? _T("[MATCH]") : _T("[DIFFER]"));
		}
		else
		{
			nSingleSide++;
			int side = di.diffcode.exists(leftSide) ? leftSide : rightSide;
			String filePath = di.getFilepath(side, ctxt.GetPath(side));
			DWORD crc = CDirSideBySideCoordinator::ComputeCRC32(filePath);
			result += strutils::format(_T("%s: CRC=%08X  (%s only)\r\n"),
				filename.c_str(), crc,
				side == leftSide ? _T("Left") : _T("Right"));
		}
	}

	String summary = strutils::format(_T("\r\n--- Summary: %d match, %d differ, %d single-side ---"),
		nMatch, nDiffer, nSingleSide);
	result += summary;

	if (m_pCoordinator)
		m_pCoordinator->LogOperation(strutils::format(_T("CRC Compare: %d items, %d match, %d differ"),
			static_cast<int>(items.size()), nMatch, nDiffer));

	AfxMessageBox(result.c_str(), MB_ICONINFORMATION);
}

/////////////////////////////////////////////////////////////////////////////
// Touch Timestamps

void CDirSxSUnifiedView::OnSxsTouchTimestamps()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	std::vector<DIFFITEM*> items;
	m_pCoordinator->GetSelectedItems(m_nContextSide, items);
	if (items.empty())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int srcSide = m_nContextSide;
	int dstSide = (m_nContextSide == 0) ? (ctxt.GetCompareDirs() - 1) : 0;

	String msg = strutils::format(
		_T("Copy modification timestamps from %s side to %s side for %d selected item(s)?"),
		srcSide == 0 ? _T("Left") : _T("Right"),
		dstSide == 0 ? _T("Left") : _T("Right"),
		static_cast<int>(items.size()));
	if (AfxMessageBox(msg.c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;

	int nSuccess = 0, nFailed = 0;
	for (auto *key : items)
	{
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (di.diffcode.isDirectory())
			continue;
		if (!di.diffcode.exists(srcSide) || !di.diffcode.exists(dstSide))
			continue;

		String srcPath = di.getFilepath(srcSide, ctxt.GetPath(srcSide));
		String dstPath = di.getFilepath(dstSide, ctxt.GetPath(dstSide));

		if (CDirSideBySideCoordinator::TouchFileTimestamp(srcPath, dstPath))
			nSuccess++;
		else
			nFailed++;
	}

	if (m_pCoordinator)
		m_pCoordinator->LogOperation(strutils::format(
			_T("Touch Timestamps: %d succeeded, %d failed"), nSuccess, nFailed));

	AfxMessageBox(strutils::format(
		_T("Touch Timestamps complete.\nSucceeded: %d\nFailed: %d"), nSuccess, nFailed).c_str(),
		MB_ICONINFORMATION);

	pDoc->Rescan();
}

/////////////////////////////////////////////////////////////////////////////
// Show Log

static INT_PTR CALLBACK UnifiedLogDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			const tchar_t* pszLog = reinterpret_cast<const tchar_t*>(lParam);
			HWND hEdit = GetDlgItem(hDlg, 1001);
			if (hEdit && pszLog)
				SetWindowText(hEdit, pszLog);
			return TRUE;
		}
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		if (LOWORD(wParam) == 1002)
		{
			HWND hEdit = GetDlgItem(hDlg, 1001);
			if (hEdit) SetWindowText(hEdit, _T(""));
			EndDialog(hDlg, 1002);
			return TRUE;
		}
		break;
	case WM_SIZE:
		{
			int cx = LOWORD(lParam);
			int cy = HIWORD(lParam);
			HWND hEdit = GetDlgItem(hDlg, 1001);
			if (hEdit) MoveWindow(hEdit, 5, 5, cx - 10, cy - 40, TRUE);
			HWND hOk = GetDlgItem(hDlg, IDOK);
			if (hOk) MoveWindow(hOk, cx / 2 - 80, cy - 30, 70, 24, TRUE);
			HWND hClear = GetDlgItem(hDlg, 1002);
			if (hClear) MoveWindow(hClear, cx / 2 + 10, cy - 30, 70, 24, TRUE);
		}
		return TRUE;
	}
	return FALSE;
}

static DLGTEMPLATE* BuildUnifiedLogDlgTemplate(BYTE* buffer, size_t bufSize)
{
	memset(buffer, 0, bufSize);
	const int DLG_W = 350, DLG_H = 250;

	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)buffer;
	pDlg->style = DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
	pDlg->cdit = 3;
	pDlg->x = 0; pDlg->y = 0;
	pDlg->cx = DLG_W; pDlg->cy = DLG_H;

	WORD* pw = (WORD*)(pDlg + 1);
	*pw++ = 0; *pw++ = 0;
	const wchar_t dlgTitle[] = L"Operation Log";
	memcpy(pw, dlgTitle, sizeof(dlgTitle));
	pw += _countof(dlgTitle);
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL |
		ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY;
	pItem->x = 5; pItem->y = 5;
	pItem->cx = DLG_W - 10; pItem->cy = DLG_H - 30;
	pItem->id = 1001;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0081;
	*pw++ = 0; *pw++ = 0;
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
	pItem->x = DLG_W / 2 - 80; pItem->y = DLG_H - 20;
	pItem->cx = 60; pItem->cy = 14;
	pItem->id = IDOK;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0080;
	const wchar_t ok[] = L"OK";
	memcpy(pw, ok, sizeof(ok));
	pw += _countof(ok);
	*pw++ = 0;
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
	pItem->x = DLG_W / 2 + 10; pItem->y = DLG_H - 20;
	pItem->cx = 60; pItem->cy = 14;
	pItem->id = 1002;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0080;
	const wchar_t clear[] = L"Clear";
	memcpy(pw, clear, sizeof(clear));
	pw += _countof(clear);
	*pw++ = 0;

	return pDlg;
}

void CDirSxSUnifiedView::OnSxsShowLog()
{
	if (!m_pCoordinator)
		return;

	const auto& messages = m_pCoordinator->GetLogMessages();
	String logText;
	if (messages.empty())
		logText = _T("No operations logged yet.");
	else
	{
		for (const auto& msg : messages)
		{
			logText += msg;
			logText += _T("\r\n");
		}
	}

	BYTE dlgBuf[1024];
	DLGTEMPLATE* pDlgTmpl = BuildUnifiedLogDlgTemplate(dlgBuf, sizeof(dlgBuf));
	INT_PTR nResult = DialogBoxIndirectParam(AfxGetInstanceHandle(), pDlgTmpl,
		m_hWnd, UnifiedLogDlgProc, reinterpret_cast<LPARAM>(logText.c_str()));

	if (nResult == 1002)
		m_pCoordinator->ClearLog();
}

/////////////////////////////////////////////////////////////////////////////
// Report generation

String CDirSxSUnifiedView::GetItemAttributeString(const DIFFITEM& di) const
{
	if (!GetDocument() || !GetDocument()->HasDiffs())
		return _T("");

	const CDiffContext &ctxt = GetDiffContext();
	int side = m_nContextSide;
	if (!di.diffcode.exists(side))
		return _T("");

	String filePath = di.getFilepath(side, ctxt.GetPath(side));
	return CDirSideBySideCoordinator::GetFileAttributeString(filePath);
}

void CDirSxSUnifiedView::GenerateHTMLReport(const String& filePath)
{
	if (!m_pCoordinator || !GetDocument() || !GetDocument()->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	const auto& rowMapping = m_pCoordinator->GetRowMapping();
	int leftSide = 0;
	int rightSide = ctxt.GetCompareDirs() - 1;

	DIRCOLORSETTINGS colors = {};
	Options::DirColors::Load(GetOptionsMgr(), colors);

	auto colorToHex = [](COLORREF clr) -> String
	{
		return strutils::format(_T("#%02X%02X%02X"),
			GetRValue(clr), GetGValue(clr), GetBValue(clr));
	};

	std::basic_ofstream<tchar_t> f(filePath.c_str());
	if (!f.is_open())
	{
		AfxMessageBox(_T("Failed to create report file."), MB_ICONERROR);
		return;
	}

	f << _T("<!DOCTYPE html>\n<html>\n<head>\n");
	f << _T("<meta charset=\"utf-8\">\n");
	f << _T("<title>WinMerge Side-by-Side Comparison Report</title>\n");
	f << _T("<style>\n");
	f << _T("body { font-family: Segoe UI, Tahoma, sans-serif; margin: 20px; }\n");
	f << _T("h1 { color: #333; }\n");
	f << _T("table { border-collapse: collapse; width: 100%; }\n");
	f << _T("th { background: #4472C4; color: white; padding: 8px; text-align: left; }\n");
	f << _T("td { padding: 6px 8px; border: 1px solid #ddd; }\n");
	f << _T("tr:hover { opacity: 0.9; }\n");
	f << _T(".identical { background: ") << colorToHex(colors.clrDirItemEqual) << _T("; color: ") << colorToHex(colors.clrDirItemEqualText) << _T("; }\n");
	f << _T(".different { background: ") << colorToHex(colors.clrDirItemDiff) << _T("; color: ") << colorToHex(colors.clrDirItemDiffText) << _T("; }\n");
	f << _T(".orphan { background: ") << colorToHex(colors.clrDirItemOrphan) << _T("; color: ") << colorToHex(colors.clrDirItemOrphanText) << _T("; }\n");
	f << _T(".filtered { background: ") << colorToHex(colors.clrDirItemFiltered) << _T("; color: ") << colorToHex(colors.clrDirItemFilteredText) << _T("; }\n");
	f << _T("</style>\n</head>\n<body>\n");
	f << _T("<h1>WinMerge Unified Folder Comparison Report</h1>\n");
	f << _T("<p><strong>Left:</strong> ") << ctxt.GetPath(leftSide) << _T("</p>\n");
	f << _T("<p><strong>Right:</strong> ") << ctxt.GetPath(rightSide) << _T("</p>\n");
	f << _T("<p><strong>Generated:</strong> ");

	SYSTEMTIME st;
	GetLocalTime(&st);
	f << strutils::format(_T("%04d-%02d-%02d %02d:%02d:%02d"),
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	f << _T("</p>\n");

	f << _T("<table>\n<tr><th>Filename</th><th>Cmp</th>");
	f << _T("<th>Size Left</th><th>Size Right</th><th>Date Left</th><th>Date Right</th></tr>\n");

	for (const auto& row : rowMapping)
	{
		if (!row.diffpos)
			continue;

		const DIFFITEM &di = ctxt.GetDiffAt(row.diffpos);
		if (di.isEmpty() || di.diffcode.isDirectory())
			continue;

		String cssClass;
		String cmpSymbol;

		if (di.diffcode.isResultFiltered())
		{
			cssClass = _T("filtered");
			cmpSymbol = _T("~");
		}
		else if (!IsItemExistAll(ctxt, di))
		{
			cssClass = _T("orphan");
			cmpSymbol = _T("");
		}
		else if (di.diffcode.isResultSame())
		{
			cssClass = _T("identical");
			cmpSymbol = _T("=");
		}
		else if (di.diffcode.isResultDiff())
		{
			cssClass = _T("different");
			cmpSymbol = _T("\u2260");
		}

		String filename;
		for (int s = 0; s < ctxt.GetCompareDirs(); s++)
		{
			if (di.diffcode.exists(s))
			{
				String relPath = di.diffFileInfo[s].path;
				filename = di.diffFileInfo[s].filename;
				if (!relPath.empty())
					filename = relPath + _T("\\") + filename;
				break;
			}
		}

		String sizeLeft = di.diffcode.exists(leftSide) ?
			FormatFileSize(di.diffFileInfo[leftSide].size) : _T("-");
		String sizeRight = di.diffcode.exists(rightSide) ?
			FormatFileSize(di.diffFileInfo[rightSide].size) : _T("-");
		String dateLeft = di.diffcode.exists(leftSide) ? FormatTimestamp(di.diffFileInfo[leftSide]) : _T("-");
		String dateRight = di.diffcode.exists(rightSide) ? FormatTimestamp(di.diffFileInfo[rightSide]) : _T("-");

		f << _T("<tr class=\"") << cssClass << _T("\">");
		f << _T("<td>") << filename << _T("</td>");
		f << _T("<td>") << cmpSymbol << _T("</td>");
		f << _T("<td>") << sizeLeft << _T("</td>");
		f << _T("<td>") << sizeRight << _T("</td>");
		f << _T("<td>") << dateLeft << _T("</td>");
		f << _T("<td>") << dateRight << _T("</td>");
		f << _T("</tr>\n");
	}

	f << _T("</table>\n");

	const auto &counts = m_pCoordinator->GetStatusCounts();
	f << _T("<h2>Summary</h2>\n<ul>\n");
	f << _T("<li>Total files: ") << counts.nTotal << _T("</li>\n");
	f << _T("<li>Identical: ") << counts.nIdentical << _T("</li>\n");
	f << _T("<li>Different: ") << counts.nDifferent << _T("</li>\n");
	f << _T("<li>Left only: ") << counts.nOrphanLeft << _T("</li>\n");
	f << _T("<li>Right only: ") << counts.nOrphanRight << _T("</li>\n");
	f << _T("<li>Skipped: ") << counts.nSkipped << _T("</li>\n");
	f << _T("</ul>\n</body>\n</html>\n");
	f.close();
}

void CDirSxSUnifiedView::GenerateCSVReport(const String& filePath)
{
	if (!m_pCoordinator || !GetDocument() || !GetDocument()->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	const auto& rowMapping = m_pCoordinator->GetRowMapping();
	int leftSide = 0;
	int rightSide = ctxt.GetCompareDirs() - 1;

	std::basic_ofstream<tchar_t> f(filePath.c_str());
	if (!f.is_open())
	{
		AfxMessageBox(_T("Failed to create report file."), MB_ICONERROR);
		return;
	}

	f << _T("Filename,Comparison,Size Left,Size Right,Date Left,Date Right\n");

	for (const auto& row : rowMapping)
	{
		if (!row.diffpos)
			continue;

		const DIFFITEM &di = ctxt.GetDiffAt(row.diffpos);
		if (di.isEmpty() || di.diffcode.isDirectory())
			continue;

		String cmpSymbol;
		if (di.diffcode.isResultFiltered())
			cmpSymbol = _T("Filtered");
		else if (!IsItemExistAll(ctxt, di))
			cmpSymbol = _T("Orphan");
		else if (di.diffcode.isResultSame())
			cmpSymbol = _T("Identical");
		else if (di.diffcode.isResultDiff())
			cmpSymbol = _T("Different");

		String filename;
		for (int s = 0; s < ctxt.GetCompareDirs(); s++)
		{
			if (di.diffcode.exists(s))
			{
				String relPath = di.diffFileInfo[s].path;
				filename = di.diffFileInfo[s].filename;
				if (!relPath.empty())
					filename = relPath + _T("\\") + filename;
				break;
			}
		}

		if (filename.find(_T(',')) != String::npos || filename.find(_T('"')) != String::npos)
		{
			String escaped;
			for (auto ch : filename)
			{
				if (ch == _T('"')) escaped += _T('"');
				escaped += ch;
			}
			filename = _T("\"") + escaped + _T("\"");
		}

		String sizeLeft = di.diffcode.exists(leftSide) ?
			strutils::format(_T("%lld"), di.diffFileInfo[leftSide].size) : _T("");
		String sizeRight = di.diffcode.exists(rightSide) ?
			strutils::format(_T("%lld"), di.diffFileInfo[rightSide].size) : _T("");
		String dateLeft = di.diffcode.exists(leftSide) ? FormatTimestamp(di.diffFileInfo[leftSide]) : _T("");
		String dateRight = di.diffcode.exists(rightSide) ? FormatTimestamp(di.diffFileInfo[rightSide]) : _T("");

		f << filename << _T(",");
		f << cmpSymbol << _T(",");
		f << sizeLeft << _T(",") << sizeRight << _T(",");
		f << dateLeft << _T(",") << dateRight << _T("\n");
	}

	f.close();
}

void CDirSxSUnifiedView::OnSxsGenerateReport()
{
	if (!m_pCoordinator || !GetDocument() || !GetDocument()->HasDiffs())
		return;

	CFileDialog dlg(FALSE, _T("html"), _T("ComparisonReport"),
		OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
		_T("HTML Files (*.html)|*.html|CSV Files (*.csv)|*.csv||"),
		this);

	if (dlg.DoModal() != IDOK)
		return;

	String outputPath = String(dlg.GetPathName());
	String ext = String(dlg.GetFileExt());

	if (!ext.empty())
		CharLower(&ext[0]);

	if (ext == _T("csv"))
		GenerateCSVReport(outputPath);
	else
		GenerateHTMLReport(outputPath);

	m_pCoordinator->LogOperation(strutils::format(_T("Generated report: %s"), outputPath.c_str()));
	ShellExecute(GetSafeHwnd(), _T("open"), outputPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

/////////////////////////////////////////////////////////////////////////////
// Drag-Drop

void CDirSxSUnifiedView::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	if (!m_pCoordinator || !GetDocument() || !GetDocument()->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int side = m_nContextSide;

	std::vector<String> filePaths;
	int nItem = -1;
	while ((nItem = m_pList->GetNextItem(nItem, LVNI_SELECTED)) != -1)
	{
		DIFFITEM *key = GetItemKey(nItem);
		if (!key)
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (!di.diffcode.exists(side))
			continue;
		String path = di.getFilepath(side, ctxt.GetPath(side));
		filePaths.push_back(path);
	}

	if (filePaths.empty())
		return;

	size_t totalLen = sizeof(DROPFILES);
	for (const auto& fp : filePaths)
		totalLen += (fp.length() + 1) * sizeof(tchar_t);
	totalLen += sizeof(tchar_t);

	HGLOBAL hGlobal = GlobalAlloc(GHND, totalLen);
	if (!hGlobal)
		return;

	DROPFILES *pDropFiles = static_cast<DROPFILES*>(GlobalLock(hGlobal));
	if (!pDropFiles)
	{
		GlobalFree(hGlobal);
		return;
	}

	pDropFiles->pFiles = sizeof(DROPFILES);
	pDropFiles->pt.x = 0;
	pDropFiles->pt.y = 0;
	pDropFiles->fNC = FALSE;
	pDropFiles->fWide = TRUE;

	tchar_t *pData = reinterpret_cast<tchar_t*>(reinterpret_cast<BYTE*>(pDropFiles) + sizeof(DROPFILES));
	for (const auto& fp : filePaths)
	{
		memcpy(pData, fp.c_str(), (fp.length() + 1) * sizeof(tchar_t));
		pData += fp.length() + 1;
	}
	*pData = _T('\0');

	GlobalUnlock(hGlobal);

	COleDataSource dataSource;
	STGMEDIUM stgmed = {};
	stgmed.tymed = TYMED_HGLOBAL;
	stgmed.hGlobal = hGlobal;

	dataSource.CacheData(CF_HDROP, &stgmed);
	DROPEFFECT dwEffect = dataSource.DoDragDrop(DROPEFFECT_COPY | DROPEFFECT_MOVE);

	if (dwEffect == DROPEFFECT_MOVE)
	{
		CDirDoc *pDoc = GetDocument();
		if (pDoc) pDoc->Rescan();
	}
}

/////////////////////////////////////////////////////////////////////////////
// Navigation handlers

void CDirSxSUnifiedView::OnSxsNavBack()
{
	if (!m_pCoordinator)
		return;
	String leftPath, rightPath;
	if (m_pCoordinator->NavigateBack(leftPath, rightPath))
	{
		CDirFrame *pFrame = GetParentFrame();
		if (pFrame)
			pFrame->OnSxsNavBack();
	}
}

void CDirSxSUnifiedView::OnSxsNavForward()
{
	if (!m_pCoordinator)
		return;
	String leftPath, rightPath;
	if (m_pCoordinator->NavigateForward(leftPath, rightPath))
	{
		CDirFrame *pFrame = GetParentFrame();
		if (pFrame)
			pFrame->OnSxsNavForward();
	}
}

void CDirSxSUnifiedView::OnUpdateSxsNavBack(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_pCoordinator && m_pCoordinator->CanNavigateBack());
}

void CDirSxSUnifiedView::OnUpdateSxsNavForward(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_pCoordinator && m_pCoordinator->CanNavigateForward());
}

void CDirSxSUnifiedView::OnSxsUpLevel()
{
	if (!m_pCoordinator)
		return;
	String leftParent, rightParent;
	if (m_pCoordinator->GetParentPaths(leftParent, rightParent))
	{
		const CDiffContext &ctxt = GetDiffContext();
		m_pCoordinator->PushHistory(ctxt.GetLeftPath(), ctxt.GetRightPath());
		CDirFrame *pFrame = GetParentFrame();
		if (pFrame)
			pFrame->OnSxsUpLevel();
	}
}

void CDirSxSUnifiedView::OnSxsSetBase()
{
	if (!m_pCoordinator || !m_pList)
		return;
	int nItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
	if (nItem < 0)
		return;
	DIFFITEM *key = GetItemKey(nItem);
	if (!key)
		return;
	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);
	if (!di.diffcode.isDirectory())
		return;
	int side = m_nContextSide;
	if (!di.diffcode.exists(side))
		return;
	String subPath = di.getFilepath(side, ctxt.GetPath(side));
	m_pCoordinator->SetBaseFolder(side, subPath);
}

void CDirSxSUnifiedView::OnSxsSetBaseOther()
{
	if (!m_pCoordinator || !m_pList)
		return;
	int nItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
	if (nItem < 0)
		return;
	DIFFITEM *key = GetItemKey(nItem);
	if (!key)
		return;
	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);
	if (!di.diffcode.isDirectory())
		return;
	int otherSide = (m_nContextSide == 0) ? (ctxt.GetCompareDirs() - 1) : 0;
	if (!di.diffcode.exists(otherSide))
		return;
	String subPath = di.getFilepath(otherSide, ctxt.GetPath(otherSide));
	m_pCoordinator->SetBaseFolderOtherSide(otherSide, subPath);
}

/////////////////////////////////////////////////////////////////////////////
// Find Next / Find Prev

bool CDirSxSUnifiedView::FindFilename(const String& pattern, bool bForward, int startRow)
{
	int nCount = m_pList->GetItemCount();
	if (nCount == 0 || pattern.empty())
		return false;
	for (int i = 1; i <= nCount; ++i)
	{
		int idx = bForward ? (startRow + i) % nCount : (startRow - i + nCount) % nCount;
		DIFFITEM *di = GetItemKey(idx);
		if (!di)
			continue;
		const CDiffContext &ctxt = GetDiffContext();
		const DIFFITEM &item = ctxt.GetDiffAt(di);
		// Search both sides
		for (int s = 0; s < ctxt.GetCompareDirs(); s++)
		{
			if (!item.diffcode.exists(s))
				continue;
			const String& name = item.diffFileInfo[s].filename;
			if (PathMatchSpec(name.c_str(), pattern.c_str()))
			{
				m_pList->SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
				m_pList->SetItemState(idx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				m_pList->EnsureVisible(idx, FALSE);
				return true;
			}
		}
	}
	return false;
}

void CDirSxSUnifiedView::OnSxsFindNext()
{
	if (!m_pCoordinator || !m_pList)
		return;
	if (m_sFindPattern.empty())
	{
		OnSxsFindFilename();
		return;
	}
	int nStart = m_pList->GetNextItem(-1, LVNI_FOCUSED);
	if (nStart < 0) nStart = 0;
	if (!FindFilename(m_sFindPattern, true, nStart))
		AfxMessageBox(_("No more matches found.").c_str(), MB_ICONINFORMATION);
}

void CDirSxSUnifiedView::OnSxsFindPrev()
{
	if (!m_pCoordinator || !m_pList)
		return;
	if (m_sFindPattern.empty())
	{
		OnSxsFindFilename();
		return;
	}
	int nStart = m_pList->GetNextItem(-1, LVNI_FOCUSED);
	if (nStart < 0) nStart = 0;
	if (!FindFilename(m_sFindPattern, false, nStart))
		AfxMessageBox(_("No more matches found.").c_str(), MB_ICONINFORMATION);
}

/////////////////////////////////////////////////////////////////////////////
// Copy to Folder / Move to Folder

void CDirSxSUnifiedView::OnSxsCopyToFolder()
{
	if (!m_pCoordinator || !m_pList)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	CFolderPickerDialog dlg(nullptr, 0, this);
	if (dlg.DoModal() != IDOK)
		return;

	String destFolder = String(dlg.GetPathName().GetString());
	const CDiffContext &ctxt = GetDiffContext();

	std::vector<String> srcPaths;
	int sel = -1;
	while ((sel = m_pList->GetNextItem(sel, LVNI_SELECTED)) != -1)
	{
		DIFFITEM *key = GetItemKey(sel);
		if (!key)
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (!di.diffcode.exists(m_nContextSide))
			continue;
		String srcPath = di.getFilepath(m_nContextSide, ctxt.GetPath(m_nContextSide));
		srcPaths.push_back(srcPath);
	}
	if (srcPaths.empty())
		return;

	String srcStr;
	for (const auto& p : srcPaths)
	{
		srcStr += p;
		srcStr += _T('\0');
	}
	srcStr += _T('\0');

	String destStr = destFolder + _T('\0');

	SHFILEOPSTRUCT shfop = {};
	shfop.hwnd = m_hWnd;
	shfop.wFunc = FO_COPY;
	shfop.pFrom = srcStr.c_str();
	shfop.pTo = destStr.c_str();
	shfop.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR;
	SHFileOperation(&shfop);

	m_pCoordinator->LogOperation(_T("Copied files to: ") + destFolder);
}

void CDirSxSUnifiedView::OnSxsMoveToFolder()
{
	if (!m_pCoordinator || !m_pList)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	CFolderPickerDialog dlg(nullptr, 0, this);
	if (dlg.DoModal() != IDOK)
		return;

	String destFolder = String(dlg.GetPathName().GetString());
	const CDiffContext &ctxt = GetDiffContext();

	std::vector<String> srcPaths;
	int sel = -1;
	while ((sel = m_pList->GetNextItem(sel, LVNI_SELECTED)) != -1)
	{
		DIFFITEM *key = GetItemKey(sel);
		if (!key)
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (!di.diffcode.exists(m_nContextSide))
			continue;
		String srcPath = di.getFilepath(m_nContextSide, ctxt.GetPath(m_nContextSide));
		srcPaths.push_back(srcPath);
	}
	if (srcPaths.empty())
		return;

	String srcStr;
	for (const auto& p : srcPaths)
	{
		srcStr += p;
		srcStr += _T('\0');
	}
	srcStr += _T('\0');

	String destStr = destFolder + _T('\0');

	SHFILEOPSTRUCT shfop = {};
	shfop.hwnd = m_hWnd;
	shfop.wFunc = FO_MOVE;
	shfop.pFrom = srcStr.c_str();
	shfop.pTo = destStr.c_str();
	shfop.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR;
	SHFileOperation(&shfop);

	m_pCoordinator->LogOperation(_T("Moved files to: ") + destFolder);
	pDoc->Rescan();
}

/////////////////////////////////////////////////////////////////////////////
// New Folder

static INT_PTR CALLBACK UnifiedNewFolderDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		SetWindowLongPtr(hDlg, DWLP_USER, lParam);
		SetFocus(GetDlgItem(hDlg, 1001));
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				tchar_t *pBuf = reinterpret_cast<tchar_t*>(GetWindowLongPtr(hDlg, DWLP_USER));
				if (pBuf) ::GetDlgItemTextW(hDlg, 1001, pBuf, MAX_PATH);
			}
			EndDialog(hDlg, IDOK);
			return TRUE;
		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static DLGTEMPLATE* BuildUnifiedNewFolderDlgTemplate(BYTE* buffer, size_t bufSize)
{
	memset(buffer, 0, bufSize);
	const int DLG_W = 260, DLG_H = 75;

	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)buffer;
	pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
	pDlg->cdit = 4;
	pDlg->x = 0; pDlg->y = 0;
	pDlg->cx = DLG_W; pDlg->cy = DLG_H;

	WORD* pw = (WORD*)(pDlg + 1);
	*pw++ = 0; *pw++ = 0;
	const wchar_t dlgTitle[] = L"New Folder";
	memcpy(pw, dlgTitle, sizeof(dlgTitle));
	pw += _countof(dlgTitle);
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
	pItem->x = 7; pItem->y = 7; pItem->cx = DLG_W - 14; pItem->cy = 10;
	pItem->id = 0xFFFF;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0082;
	const wchar_t label[] = L"Folder Name:";
	memcpy(pw, label, sizeof(label));
	pw += _countof(label);
	*pw++ = 0;
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
	pItem->x = 7; pItem->y = 20; pItem->cx = DLG_W - 14; pItem->cy = 14;
	pItem->id = 1001;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0081;
	*pw++ = 0; *pw++ = 0;
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
	pItem->x = DLG_W - 120; pItem->y = DLG_H - 20; pItem->cx = 50; pItem->cy = 14;
	pItem->id = IDOK;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0080;
	const wchar_t ok[] = L"OK";
	memcpy(pw, ok, sizeof(ok));
	pw += _countof(ok);
	*pw++ = 0;
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
	pItem->x = DLG_W - 60; pItem->y = DLG_H - 20; pItem->cx = 50; pItem->cy = 14;
	pItem->id = IDCANCEL;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0080;
	const wchar_t cancel[] = L"Cancel";
	memcpy(pw, cancel, sizeof(cancel));
	pw += _countof(cancel);
	*pw++ = 0;

	return pDlg;
}

void CDirSxSUnifiedView::OnSxsNewFolder()
{
	if (!m_pCoordinator)
		return;

	tchar_t szInput[MAX_PATH] = {};

	BYTE dlgBuf[1024];
	DLGTEMPLATE* pDlgTmpl = BuildUnifiedNewFolderDlgTemplate(dlgBuf, sizeof(dlgBuf));

	INT_PTR nResult = DialogBoxIndirectParam(AfxGetInstanceHandle(), pDlgTmpl,
		m_hWnd, UnifiedNewFolderDlgProc, reinterpret_cast<LPARAM>(szInput));

	if (nResult != IDOK)
		return;

	String folderName = szInput;
	if (folderName.empty())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	String basePath = (m_nContextSide == 0) ? ctxt.GetLeftPath() : ctxt.GetRightPath();
	String newPath = paths::ConcatPath(basePath, folderName);

	if (CreateDirectory(newPath.c_str(), nullptr))
	{
		m_pCoordinator->LogOperation(_T("Created folder: ") + newPath);
		CDirDoc *pDoc = GetDocument();
		if (pDoc)
			pDoc->Rescan();
	}
	else
	{
		AfxMessageBox(strutils::format(_T("Failed to create folder: %s"), newPath.c_str()).c_str(), MB_ICONERROR);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Delete Permanently

void CDirSxSUnifiedView::OnSxsDeletePermanent()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	std::vector<DIFFITEM*> items;
	m_pCoordinator->GetSelectedItems(m_nContextSide, items);
	if (items.empty())
		return;

	String msg = strutils::format(_T("PERMANENTLY delete %d selected item(s) from %s side?\nThis cannot be undone!"),
		static_cast<int>(items.size()),
		m_nContextSide == 0 ? _T("left") : _T("right"));
	if (AfxMessageBox(msg.c_str(), MB_YESNO | MB_ICONWARNING) != IDYES)
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int side = m_nContextSide;

	ShellFileOperations fileOps;
	for (auto *key : items)
	{
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (!di.diffcode.exists(side))
			continue;
		String path = di.getFilepath(side, ctxt.GetPath(side));
		fileOps.AddSource(path);
	}

	fileOps.SetOperation(FO_DELETE, 0, GetSafeHwnd());
	if (fileOps.Run() && !fileOps.IsCanceled())
	{
		m_pCoordinator->LogOperation(strutils::format(_T("Permanently deleted %d item(s)"),
			static_cast<int>(items.size())));
		pDoc->Rescan();
	}
}

/////////////////////////////////////////////////////////////////////////////
// Exchange

void CDirSxSUnifiedView::OnSxsExchange()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	std::vector<DIFFITEM*> items;
	m_pCoordinator->GetSelectedItems(m_nContextSide, items);
	if (items.empty())
		return;

	String msg = strutils::format(_T("Exchange %d selected item(s) between left and right sides?"),
		static_cast<int>(items.size()));
	if (AfxMessageBox(msg.c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;

	m_pCoordinator->ExchangeFiles(items);
	pDoc->Rescan();
}

/////////////////////////////////////////////////////////////////////////////
// Change Attributes

static INT_PTR CALLBACK UnifiedChangeAttrDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			SetWindowLongPtr(hDlg, DWLP_USER, lParam);
			DWORD *pAttrs = reinterpret_cast<DWORD*>(lParam);
			if (pAttrs)
			{
				CheckDlgButton(hDlg, 1001, (*pAttrs & FILE_ATTRIBUTE_READONLY) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hDlg, 1002, (*pAttrs & FILE_ATTRIBUTE_HIDDEN) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hDlg, 1003, (*pAttrs & FILE_ATTRIBUTE_SYSTEM) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hDlg, 1004, (*pAttrs & FILE_ATTRIBUTE_ARCHIVE) ? BST_CHECKED : BST_UNCHECKED);
			}
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				DWORD *pAttrs = reinterpret_cast<DWORD*>(GetWindowLongPtr(hDlg, DWLP_USER));
				if (pAttrs)
				{
					*pAttrs = 0;
					if (IsDlgButtonChecked(hDlg, 1001) == BST_CHECKED) *pAttrs |= FILE_ATTRIBUTE_READONLY;
					if (IsDlgButtonChecked(hDlg, 1002) == BST_CHECKED) *pAttrs |= FILE_ATTRIBUTE_HIDDEN;
					if (IsDlgButtonChecked(hDlg, 1003) == BST_CHECKED) *pAttrs |= FILE_ATTRIBUTE_SYSTEM;
					if (IsDlgButtonChecked(hDlg, 1004) == BST_CHECKED) *pAttrs |= FILE_ATTRIBUTE_ARCHIVE;
				}
			}
			EndDialog(hDlg, IDOK);
			return TRUE;
		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static DLGTEMPLATE* BuildUnifiedChangeAttrDlgTemplate(BYTE* buffer, size_t bufSize)
{
	memset(buffer, 0, bufSize);
	const int DLG_W = 200, DLG_H = 120;

	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)buffer;
	pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
	pDlg->cdit = 6;
	pDlg->x = 0; pDlg->y = 0;
	pDlg->cx = DLG_W; pDlg->cy = DLG_H;

	WORD* pw = (WORD*)(pDlg + 1);
	*pw++ = 0; *pw++ = 0;
	const wchar_t dlgTitle[] = L"Change Attributes";
	memcpy(pw, dlgTitle, sizeof(dlgTitle));
	pw += _countof(dlgTitle);
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	struct CheckboxDef { WORD id; const wchar_t *label; int labelLen; int y; };
	const wchar_t lbl_r[] = L"Read-only";
	const wchar_t lbl_h[] = L"Hidden";
	const wchar_t lbl_s[] = L"System";
	const wchar_t lbl_a[] = L"Archive";
	CheckboxDef cbs[] = {
		{ 1001, lbl_r, _countof(lbl_r), 7 },
		{ 1002, lbl_h, _countof(lbl_h), 22 },
		{ 1003, lbl_s, _countof(lbl_s), 37 },
		{ 1004, lbl_a, _countof(lbl_a), 52 },
	};

	for (auto &cb : cbs)
	{
		DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)pw;
		pItem->style = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP;
		pItem->x = 10; pItem->y = (short)cb.y; pItem->cx = DLG_W - 20; pItem->cy = 12;
		pItem->id = cb.id;
		pw = (WORD*)(pItem + 1);
		*pw++ = 0xFFFF; *pw++ = 0x0080;
		memcpy(pw, cb.label, cb.labelLen * sizeof(wchar_t));
		pw += cb.labelLen;
		*pw++ = 0;
		pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);
	}

	DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
	pItem->x = DLG_W / 2 - 60; pItem->y = DLG_H - 22; pItem->cx = 50; pItem->cy = 14;
	pItem->id = IDOK;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0080;
	const wchar_t ok[] = L"OK";
	memcpy(pw, ok, sizeof(ok));
	pw += _countof(ok);
	*pw++ = 0;
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
	pItem->x = DLG_W / 2 + 10; pItem->y = DLG_H - 22; pItem->cx = 50; pItem->cy = 14;
	pItem->id = IDCANCEL;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0080;
	const wchar_t cancel[] = L"Cancel";
	memcpy(pw, cancel, sizeof(cancel));
	pw += _countof(cancel);
	*pw++ = 0;

	return pDlg;
}

void CDirSxSUnifiedView::OnSxsChangeAttributes()
{
	if (!m_pCoordinator || !m_pList)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	int nItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
	if (nItem < 0)
		return;

	DIFFITEM *key = GetItemKey(nItem);
	if (!key)
		return;

	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);
	if (!di.diffcode.exists(m_nContextSide))
		return;

	String filePath = di.getFilepath(m_nContextSide, ctxt.GetPath(m_nContextSide));
	DWORD attrs = GetFileAttributes(filePath.c_str());
	if (attrs == INVALID_FILE_ATTRIBUTES)
		return;

	BYTE dlgBuf[2048];
	DLGTEMPLATE* pDlgTmpl = BuildUnifiedChangeAttrDlgTemplate(dlgBuf, sizeof(dlgBuf));

	INT_PTR nResult = DialogBoxIndirectParam(AfxGetInstanceHandle(), pDlgTmpl,
		m_hWnd, UnifiedChangeAttrDlgProc, reinterpret_cast<LPARAM>(&attrs));

	if (nResult != IDOK)
		return;

	int nSuccess = 0, nFailed = 0;
	int sel = -1;
	while ((sel = m_pList->GetNextItem(sel, LVNI_SELECTED)) != -1)
	{
		DIFFITEM *selKey = GetItemKey(sel);
		if (!selKey)
			continue;
		const DIFFITEM &selDi = ctxt.GetDiffAt(selKey);
		if (!selDi.diffcode.exists(m_nContextSide))
			continue;
		String selPath = selDi.getFilepath(m_nContextSide, ctxt.GetPath(m_nContextSide));
		if (SetFileAttributes(selPath.c_str(), attrs))
			nSuccess++;
		else
			nFailed++;
	}

	m_pCoordinator->LogOperation(strutils::format(
		_T("Changed attributes: %d succeeded, %d failed"), nSuccess, nFailed));

	if (nFailed > 0)
		AfxMessageBox(strutils::format(_T("Attribute change: %d succeeded, %d failed"),
			nSuccess, nFailed).c_str(), MB_ICONWARNING);

	pDoc->Rescan();
}

/////////////////////////////////////////////////////////////////////////////
// Touch Now / Touch Specific / Touch From Other

void CDirSxSUnifiedView::OnSxsTouchNow()
{
	if (!m_pCoordinator || !m_pList)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int nSuccess = 0, nFailed = 0;
	int sel = -1;
	while ((sel = m_pList->GetNextItem(sel, LVNI_SELECTED)) != -1)
	{
		DIFFITEM *key = GetItemKey(sel);
		if (!key)
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (di.diffcode.isDirectory())
			continue;
		if (!di.diffcode.exists(m_nContextSide))
			continue;
		String filePath = di.getFilepath(m_nContextSide, ctxt.GetPath(m_nContextSide));
		if (CDirSideBySideCoordinator::TouchToNow(filePath))
			nSuccess++;
		else
			nFailed++;
	}

	m_pCoordinator->LogOperation(strutils::format(
		_T("Touch Now: %d succeeded, %d failed"), nSuccess, nFailed));

	AfxMessageBox(strutils::format(
		_T("Touch Now complete.\nSucceeded: %d\nFailed: %d"), nSuccess, nFailed).c_str(),
		MB_ICONINFORMATION);

	pDoc->Rescan();
}

static INT_PTR CALLBACK UnifiedTouchSpecificDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			SetWindowLongPtr(hDlg, DWLP_USER, lParam);
			SYSTEMTIME st;
			GetLocalTime(&st);
			tchar_t buf[64];
			_stprintf_s(buf, _T("%04d-%02d-%02d %02d:%02d:%02d"),
				st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
			::SetDlgItemTextW(hDlg, 1001, buf);
			SetFocus(GetDlgItem(hDlg, 1001));
		}
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				tchar_t *pBuf = reinterpret_cast<tchar_t*>(GetWindowLongPtr(hDlg, DWLP_USER));
				if (pBuf) ::GetDlgItemTextW(hDlg, 1001, pBuf, 64);
			}
			EndDialog(hDlg, IDOK);
			return TRUE;
		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static DLGTEMPLATE* BuildUnifiedTouchSpecificDlgTemplate(BYTE* buffer, size_t bufSize)
{
	memset(buffer, 0, bufSize);
	const int DLG_W = 260, DLG_H = 75;

	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)buffer;
	pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
	pDlg->cdit = 4;
	pDlg->x = 0; pDlg->y = 0;
	pDlg->cx = DLG_W; pDlg->cy = DLG_H;

	WORD* pw = (WORD*)(pDlg + 1);
	*pw++ = 0; *pw++ = 0;
	const wchar_t dlgTitle[] = L"Touch to Specific Time";
	memcpy(pw, dlgTitle, sizeof(dlgTitle));
	pw += _countof(dlgTitle);
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
	pItem->x = 7; pItem->y = 7; pItem->cx = DLG_W - 14; pItem->cy = 10;
	pItem->id = 0xFFFF;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0082;
	const wchar_t label[] = L"Date/Time (YYYY-MM-DD HH:MM:SS):";
	memcpy(pw, label, sizeof(label));
	pw += _countof(label);
	*pw++ = 0;
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
	pItem->x = 7; pItem->y = 20; pItem->cx = DLG_W - 14; pItem->cy = 14;
	pItem->id = 1001;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0081;
	*pw++ = 0; *pw++ = 0;
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
	pItem->x = DLG_W - 120; pItem->y = DLG_H - 20; pItem->cx = 50; pItem->cy = 14;
	pItem->id = IDOK;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0080;
	const wchar_t ok[] = L"OK";
	memcpy(pw, ok, sizeof(ok));
	pw += _countof(ok);
	*pw++ = 0;
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
	pItem->x = DLG_W - 60; pItem->y = DLG_H - 20; pItem->cx = 50; pItem->cy = 14;
	pItem->id = IDCANCEL;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0080;
	const wchar_t cancel[] = L"Cancel";
	memcpy(pw, cancel, sizeof(cancel));
	pw += _countof(cancel);
	*pw++ = 0;

	return pDlg;
}

void CDirSxSUnifiedView::OnSxsTouchSpecific()
{
	if (!m_pCoordinator || !m_pList)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	tchar_t szInput[64] = {};

	BYTE dlgBuf[1024];
	DLGTEMPLATE* pDlgTmpl = BuildUnifiedTouchSpecificDlgTemplate(dlgBuf, sizeof(dlgBuf));

	INT_PTR nResult = DialogBoxIndirectParam(AfxGetInstanceHandle(), pDlgTmpl,
		m_hWnd, UnifiedTouchSpecificDlgProc, reinterpret_cast<LPARAM>(szInput));

	if (nResult != IDOK)
		return;

	SYSTEMTIME st = {};
	if (_stscanf_s(szInput, _T("%hd-%hd-%hd %hd:%hd:%hd"),
		&st.wYear, &st.wMonth, &st.wDay,
		&st.wHour, &st.wMinute, &st.wSecond) < 6)
	{
		AfxMessageBox(_T("Invalid date/time format. Use YYYY-MM-DD HH:MM:SS"), MB_ICONERROR);
		return;
	}

	SYSTEMTIME stUtc;
	TzSpecificLocalTimeToSystemTime(nullptr, &st, &stUtc);
	FILETIME ft;
	SystemTimeToFileTime(&stUtc, &ft);

	const CDiffContext &ctxt = GetDiffContext();
	int nSuccess = 0, nFailed = 0;
	int sel = -1;
	while ((sel = m_pList->GetNextItem(sel, LVNI_SELECTED)) != -1)
	{
		DIFFITEM *key = GetItemKey(sel);
		if (!key) continue;
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (di.diffcode.isDirectory()) continue;
		if (!di.diffcode.exists(m_nContextSide)) continue;
		String filePath = di.getFilepath(m_nContextSide, ctxt.GetPath(m_nContextSide));
		if (CDirSideBySideCoordinator::TouchToSpecificTime(filePath, ft))
			nSuccess++;
		else
			nFailed++;
	}

	m_pCoordinator->LogOperation(strutils::format(
		_T("Touch Specific: %d succeeded, %d failed"), nSuccess, nFailed));

	AfxMessageBox(strutils::format(
		_T("Touch Specific complete.\nSucceeded: %d\nFailed: %d"), nSuccess, nFailed).c_str(),
		MB_ICONINFORMATION);

	pDoc->Rescan();
}

void CDirSxSUnifiedView::OnSxsTouchFromOther()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	std::vector<DIFFITEM*> items;
	m_pCoordinator->GetSelectedItems(m_nContextSide, items);
	if (items.empty())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int dstSide = m_nContextSide;
	int srcSide = (m_nContextSide == 0) ? (ctxt.GetCompareDirs() - 1) : 0;

	String msg = strutils::format(
		_T("Copy modification timestamps from %s side to %s side for %d selected item(s)?"),
		srcSide == 0 ? _T("Left") : _T("Right"),
		dstSide == 0 ? _T("Left") : _T("Right"),
		static_cast<int>(items.size()));
	if (AfxMessageBox(msg.c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;

	int nSuccess = 0, nFailed = 0;
	for (auto *key : items)
	{
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (di.diffcode.isDirectory()) continue;
		if (!di.diffcode.exists(srcSide) || !di.diffcode.exists(dstSide)) continue;

		String srcPath = di.getFilepath(srcSide, ctxt.GetPath(srcSide));
		String dstPath = di.getFilepath(dstSide, ctxt.GetPath(dstSide));

		if (CDirSideBySideCoordinator::TouchFileTimestamp(srcPath, dstPath))
			nSuccess++;
		else
			nFailed++;
	}

	m_pCoordinator->LogOperation(strutils::format(
		_T("Touch From Other: %d succeeded, %d failed"), nSuccess, nFailed));

	AfxMessageBox(strutils::format(
		_T("Touch From Other complete.\nSucceeded: %d\nFailed: %d"), nSuccess, nFailed).c_str(),
		MB_ICONINFORMATION);

	pDoc->Rescan();
}

/////////////////////////////////////////////////////////////////////////////
// Advanced Filter

static INT_PTR CALLBACK UnifiedAdvFilterDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			SetWindowLongPtr(hDlg, DWLP_USER, lParam);
			auto *pFilter = reinterpret_cast<CDirSideBySideCoordinator::AdvancedFilter*>(lParam);
			if (pFilter)
			{
				::SetDlgItemTextW(hDlg, 1001, pFilter->dateFrom.c_str());
				::SetDlgItemTextW(hDlg, 1002, pFilter->dateTo.c_str());
				if (pFilter->sizeMin >= 0)
					::SetDlgItemInt(hDlg, 1003, pFilter->sizeMin, FALSE);
				if (pFilter->sizeMax >= 0)
					::SetDlgItemInt(hDlg, 1004, pFilter->sizeMax, FALSE);
				::SetDlgItemTextW(hDlg, 1005, pFilter->attrMask.c_str());
			}
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				auto *pFilter = reinterpret_cast<CDirSideBySideCoordinator::AdvancedFilter*>(
					GetWindowLongPtr(hDlg, DWLP_USER));
				if (pFilter)
				{
					tchar_t buf[MAX_PATH];
					::GetDlgItemTextW(hDlg, 1001, buf, MAX_PATH);
					pFilter->dateFrom = buf;
					::GetDlgItemTextW(hDlg, 1002, buf, MAX_PATH);
					pFilter->dateTo = buf;
					::GetDlgItemTextW(hDlg, 1003, buf, MAX_PATH);
					pFilter->sizeMin = (buf[0] != 0) ? _ttoi(buf) : -1;
					::GetDlgItemTextW(hDlg, 1004, buf, MAX_PATH);
					pFilter->sizeMax = (buf[0] != 0) ? _ttoi(buf) : -1;
					::GetDlgItemTextW(hDlg, 1005, buf, MAX_PATH);
					pFilter->attrMask = buf;
				}
			}
			EndDialog(hDlg, IDOK);
			return TRUE;
		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static DLGTEMPLATE* BuildUnifiedAdvFilterDlgTemplate(BYTE* buffer, size_t bufSize)
{
	memset(buffer, 0, bufSize);
	const int DLG_W = 300, DLG_H = 160;

	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)buffer;
	pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
	pDlg->cdit = 12;
	pDlg->x = 0; pDlg->y = 0;
	pDlg->cx = DLG_W; pDlg->cy = DLG_H;

	WORD* pw = (WORD*)(pDlg + 1);
	*pw++ = 0; *pw++ = 0;
	const wchar_t dlgTitle[] = L"Advanced Filter";
	memcpy(pw, dlgTitle, sizeof(dlgTitle));
	pw += _countof(dlgTitle);
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	struct FieldDef { const wchar_t *label; int labelLen; WORD editId; int y; };
	const wchar_t lbl1[] = L"Date From (YYYY-MM-DD):";
	const wchar_t lbl2[] = L"Date To (YYYY-MM-DD):";
	const wchar_t lbl3[] = L"Min Size (bytes):";
	const wchar_t lbl4[] = L"Max Size (bytes):";
	const wchar_t lbl5[] = L"Attributes (RHSA):";
	FieldDef fields[] = {
		{ lbl1, _countof(lbl1), 1001, 7 },
		{ lbl2, _countof(lbl2), 1002, 32 },
		{ lbl3, _countof(lbl3), 1003, 57 },
		{ lbl4, _countof(lbl4), 1004, 82 },
		{ lbl5, _countof(lbl5), 1005, 107 },
	};

	for (auto &f : fields)
	{
		DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)pw;
		pItem->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
		pItem->x = 7; pItem->y = (short)f.y; pItem->cx = 100; pItem->cy = 10;
		pItem->id = 0xFFFF;
		pw = (WORD*)(pItem + 1);
		*pw++ = 0xFFFF; *pw++ = 0x0082;
		memcpy(pw, f.label, f.labelLen * sizeof(wchar_t));
		pw += f.labelLen;
		*pw++ = 0;
		pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

		pItem = (DLGITEMTEMPLATE*)pw;
		pItem->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
		pItem->x = 120; pItem->y = (short)f.y; pItem->cx = DLG_W - 130; pItem->cy = 14;
		pItem->id = f.editId;
		pw = (WORD*)(pItem + 1);
		*pw++ = 0xFFFF; *pw++ = 0x0081;
		*pw++ = 0; *pw++ = 0;
		pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);
	}

	DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
	pItem->x = DLG_W / 2 - 60; pItem->y = DLG_H - 22; pItem->cx = 50; pItem->cy = 14;
	pItem->id = IDOK;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0080;
	const wchar_t ok[] = L"OK";
	memcpy(pw, ok, sizeof(ok));
	pw += _countof(ok);
	*pw++ = 0;
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
	pItem->x = DLG_W / 2 + 10; pItem->y = DLG_H - 22; pItem->cx = 50; pItem->cy = 14;
	pItem->id = IDCANCEL;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0080;
	const wchar_t cancel[] = L"Cancel";
	memcpy(pw, cancel, sizeof(cancel));
	pw += _countof(cancel);
	*pw++ = 0;

	return pDlg;
}

void CDirSxSUnifiedView::OnSxsAdvancedFilter()
{
	if (!m_pCoordinator)
		return;

	CDirSideBySideCoordinator::AdvancedFilter filter = m_pCoordinator->GetAdvancedFilter();

	BYTE dlgBuf[4096];
	DLGTEMPLATE* pDlgTmpl = BuildUnifiedAdvFilterDlgTemplate(dlgBuf, sizeof(dlgBuf));

	INT_PTR nResult = DialogBoxIndirectParam(AfxGetInstanceHandle(), pDlgTmpl,
		m_hWnd, UnifiedAdvFilterDlgProc, reinterpret_cast<LPARAM>(&filter));

	if (nResult != IDOK)
		return;

	m_pCoordinator->SetAdvancedFilter(filter);
	m_pCoordinator->LogOperation(_T("Advanced filter updated"));
	m_pCoordinator->Redisplay();
}

/////////////////////////////////////////////////////////////////////////////
// Ignore Structure / Row Stripes

void CDirSxSUnifiedView::OnSxsIgnoreStructure()
{
	bool bCurrent = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_IGNORE_FOLDER_STRUCTURE);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_IGNORE_FOLDER_STRUCTURE, !bCurrent);
	if (m_pCoordinator)
	{
		m_pCoordinator->SetIgnoreFolderStructure(!bCurrent);
		m_pCoordinator->Redisplay();
	}
}

void CDirSxSUnifiedView::OnUpdateSxsIgnoreStructure(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_IGNORE_FOLDER_STRUCTURE));
}

void CDirSxSUnifiedView::OnSxsRowStripes()
{
	m_bRowStripes = !m_bRowStripes;
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_ROW_STRIPES, m_bRowStripes);
	if (m_pList)
		m_pList->InvalidateRect(nullptr);
}

void CDirSxSUnifiedView::OnUpdateSxsRowStripes(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_bRowStripes);
}

/////////////////////////////////////////////////////////////////////////////
// Exclude Pattern

void CDirSxSUnifiedView::OnSxsExcludePattern()
{
	if (!m_pCoordinator || !m_pList)
		return;

	int nItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
	if (nItem < 0)
		return;

	DIFFITEM *key = GetItemKey(nItem);
	if (!key)
		return;

	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);

	// Find a side that exists
	int side = m_nContextSide;
	if (!di.diffcode.exists(side))
		side = (side == 0) ? (ctxt.GetCompareDirs() - 1) : 0;
	if (!di.diffcode.exists(side))
		return;

	String filename = di.diffFileInfo[side].filename;
	String::size_type dotPos = filename.rfind(_T('.'));
	String pattern;
	if (dotPos != String::npos)
		pattern = _T("-*.") + filename.substr(dotPos + 1);
	else
		pattern = _T("-") + filename;

	String currentFilter = m_pCoordinator->GetNameFilter();
	if (!currentFilter.empty())
		currentFilter += _T(" ");
	currentFilter += pattern;
	m_pCoordinator->SetNameFilter(currentFilter);
	m_pCoordinator->Redisplay();

	m_pCoordinator->LogOperation(_T("Added exclude pattern: ") + pattern);
}

/////////////////////////////////////////////////////////////////////////////
// Compare Info

void CDirSxSUnifiedView::OnSxsCompareInfo()
{
	if (!m_pCoordinator)
		return;
	String info = m_pCoordinator->FormatCompareInfoString();
	AfxMessageBox(info.c_str(), MB_ICONINFORMATION);
}

/////////////////////////////////////////////////////////////////////////////
// Copy Path / Copy Filename

void CDirSxSUnifiedView::OnSxsCopyPath()
{
	if (!m_pList)
		return;

	int nItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
	if (nItem < 0)
		return;

	DIFFITEM *key = GetItemKey(nItem);
	if (!key)
		return;

	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);
	if (!di.diffcode.exists(m_nContextSide))
		return;

	String fullPath = di.getFilepath(m_nContextSide, ctxt.GetPath(m_nContextSide));

	if (OpenClipboard())
	{
		EmptyClipboard();
		size_t len = (fullPath.length() + 1) * sizeof(tchar_t);
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
		if (hMem)
		{
			tchar_t *pData = static_cast<tchar_t*>(GlobalLock(hMem));
			if (pData)
			{
				memcpy(pData, fullPath.c_str(), len);
				GlobalUnlock(hMem);
				SetClipboardData(CF_UNICODETEXT, hMem);
			}
		}
		CloseClipboard();
	}
}

void CDirSxSUnifiedView::OnSxsCopyFilename()
{
	if (!m_pList)
		return;

	int nItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
	if (nItem < 0)
		return;

	DIFFITEM *key = GetItemKey(nItem);
	if (!key)
		return;

	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);
	int side = m_nContextSide;
	if (!di.diffcode.exists(side))
		return;

	String filename = di.diffFileInfo[side].filename;

	if (OpenClipboard())
	{
		EmptyClipboard();
		size_t len = (filename.length() + 1) * sizeof(tchar_t);
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
		if (hMem)
		{
			tchar_t *pData = static_cast<tchar_t*>(GlobalLock(hMem));
			if (pData)
			{
				memcpy(pData, filename.c_str(), len);
				GlobalUnlock(hMem);
				SetClipboardData(CF_UNICODETEXT, hMem);
			}
		}
		CloseClipboard();
	}
}

/////////////////////////////////////////////////////////////////////////////
// Open With App / Open With...

void CDirSxSUnifiedView::OnSxsOpenWithApp()
{
	if (!m_pList)
		return;
	int nItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
	if (nItem < 0)
		return;
	DIFFITEM *key = GetItemKey(nItem);
	if (!key)
		return;
	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);
	if (!di.diffcode.exists(m_nContextSide))
		return;
	String filePath = di.getFilepath(m_nContextSide, ctxt.GetPath(m_nContextSide));
	ShellExecute(GetSafeHwnd(), _T("open"), filePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void CDirSxSUnifiedView::OnSxsOpenWith()
{
	if (!m_pList)
		return;
	int nItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
	if (nItem < 0)
		return;
	DIFFITEM *key = GetItemKey(nItem);
	if (!key)
		return;
	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);
	if (!di.diffcode.exists(m_nContextSide))
		return;
	String filePath = di.getFilepath(m_nContextSide, ctxt.GetPath(m_nContextSide));
	String param = _T("shell32.dll,OpenAs_RunDLL ") + filePath;
	ShellExecute(GetSafeHwnd(), _T("open"), _T("rundll32.exe"), param.c_str(), nullptr, SW_SHOWNORMAL);
}

/////////////////////////////////////////////////////////////////////////////
// Explorer Context Menu

void CDirSxSUnifiedView::ShowExplorerContextMenu(const String& filePath, CPoint pt)
{
	PIDLIST_ABSOLUTE pidlFolder = nullptr;
	LPCITEMIDLIST pidlChild = nullptr;
	IShellFolder *pShellFolder = nullptr;

	if (SUCCEEDED(SHParseDisplayName(filePath.c_str(), nullptr, &pidlFolder, 0, nullptr)))
	{
		if (SUCCEEDED(SHBindToParent(pidlFolder, IID_IShellFolder, (void**)&pShellFolder, &pidlChild)))
		{
			IContextMenu *pContextMenu = nullptr;
			if (SUCCEEDED(pShellFolder->GetUIObjectOf(m_hWnd, 1, &pidlChild, IID_IContextMenu, nullptr, (void**)&pContextMenu)))
			{
				HMENU hMenu = CreatePopupMenu();
				if (hMenu)
				{
					pContextMenu->QueryContextMenu(hMenu, 0, 1, 0x7FFF, CMF_NORMAL);
					int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN,
						pt.x, pt.y, 0, m_hWnd, nullptr);
					if (cmd > 0)
					{
						CMINVOKECOMMANDINFO cmi = {};
						cmi.cbSize = sizeof(cmi);
						cmi.hwnd = m_hWnd;
						cmi.lpVerb = MAKEINTRESOURCEA(cmd - 1);
						pContextMenu->InvokeCommand(&cmi);
					}
					DestroyMenu(hMenu);
				}
				pContextMenu->Release();
			}
			pShellFolder->Release();
		}
		CoTaskMemFree(pidlFolder);
	}
}

void CDirSxSUnifiedView::OnSxsExplorerMenu()
{
	if (!m_pList)
		return;
	int nItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
	if (nItem < 0)
		return;
	DIFFITEM *key = GetItemKey(nItem);
	if (!key)
		return;
	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);
	if (!di.diffcode.exists(m_nContextSide))
		return;
	String filePath = di.getFilepath(m_nContextSide, ctxt.GetPath(m_nContextSide));

	CPoint pt;
	GetCursorPos(&pt);
	ShowExplorerContextMenu(filePath, pt);
}

/////////////////////////////////////////////////////////////////////////////
// Select Left Only / Select Right Only

void CDirSxSUnifiedView::OnSxsSelectLeftOnly()
{
	if (!m_pList || !GetDocument() || !GetDocument()->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	m_pList->SetItemState(-1, 0, LVIS_SELECTED);
	for (int i = 0; i < static_cast<int>(m_listViewItems.size()); i++)
	{
		DIFFITEM *key = GetItemKey(i);
		if (!key) continue;
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (di.diffcode.isSideFirstOnly())
			m_pList->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
	}
}

void CDirSxSUnifiedView::OnSxsSelectRightOnly()
{
	if (!m_pList || !GetDocument() || !GetDocument()->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	m_pList->SetItemState(-1, 0, LVIS_SELECTED);
	for (int i = 0; i < static_cast<int>(m_listViewItems.size()); i++)
	{
		DIFFITEM *key = GetItemKey(i);
		if (!key) continue;
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (di.diffcode.isSideSecondOnly())
			m_pList->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Auto-expand

void CDirSxSUnifiedView::OnSxsAutoExpandAll()
{
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_AUTO_EXPAND_MODE, 1);
	if (m_pCoordinator)
	{
		m_pCoordinator->ApplyAutoExpand();
		m_pCoordinator->Redisplay();
	}
}

void CDirSxSUnifiedView::OnSxsAutoExpandDiff()
{
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_AUTO_EXPAND_MODE, 2);
	if (m_pCoordinator)
	{
		m_pCoordinator->ApplyAutoExpand();
		m_pCoordinator->Redisplay();
	}
}

void CDirSxSUnifiedView::OnUpdateSxsAutoExpandAll(CCmdUI* pCmdUI)
{
	pCmdUI->SetRadio(GetOptionsMgr()->GetInt(OPT_DIRVIEW_SXS_AUTO_EXPAND_MODE) == 1);
}

void CDirSxSUnifiedView::OnUpdateSxsAutoExpandDiff(CCmdUI* pCmdUI)
{
	pCmdUI->SetRadio(GetOptionsMgr()->GetInt(OPT_DIRVIEW_SXS_AUTO_EXPAND_MODE) == 2);
}

/////////////////////////////////////////////////////////////////////////////
// Align With

void CDirSxSUnifiedView::OnSxsAlignWith()
{
	if (!m_pCoordinator || !m_pList)
		return;

	// In unified view, alignment works differently since there's a single list.
	// The user selects two items and we align them across sides.
	int nCount = m_pList->GetSelectedCount();
	if (nCount < 2)
	{
		AfxMessageBox(_T("Please select two items to align with each other."), MB_ICONINFORMATION);
		return;
	}

	// Get first two selected items
	int nFirst = m_pList->GetNextItem(-1, LVNI_SELECTED);
	int nSecond = m_pList->GetNextItem(nFirst, LVNI_SELECTED);

	DIFFITEM *firstKey = GetItemKey(nFirst);
	DIFFITEM *secondKey = GetItemKey(nSecond);
	if (!firstKey || !secondKey)
		return;

	// First selected = left item, second = right item
	m_pCoordinator->AddAlignmentOverride(firstKey, secondKey);
	m_pCoordinator->Redisplay();

	m_pCoordinator->LogOperation(_T("Added alignment override"));
}

/////////////////////////////////////////////////////////////////////////////
// Customize Keys

static INT_PTR CALLBACK UnifiedCustomizeKeysDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			SetWindowLongPtr(hDlg, DWLP_USER, lParam);
			HWND hList = GetDlgItem(hDlg, 1001);
			if (hList)
			{
				auto *pBindings = reinterpret_cast<std::map<UINT, CDirSxSUnifiedView::KeyBinding>*>(lParam);
				if (pBindings)
				{
					for (auto &kv : *pBindings)
					{
						String desc = strutils::format(_T("Command %u: VK=%u Ctrl=%d Shift=%d Alt=%d"),
							kv.first, kv.second.vkKey,
							kv.second.bCtrl ? 1 : 0,
							kv.second.bShift ? 1 : 0,
							kv.second.bAlt ? 1 : 0);
						SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)desc.c_str());
					}
				}
			}
		}
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static DLGTEMPLATE* BuildUnifiedCustomizeKeysDlgTemplate(BYTE* buffer, size_t bufSize)
{
	memset(buffer, 0, bufSize);
	const int DLG_W = 350, DLG_H = 250;

	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)buffer;
	pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
	pDlg->cdit = 2;
	pDlg->x = 0; pDlg->y = 0;
	pDlg->cx = DLG_W; pDlg->cy = DLG_H;

	WORD* pw = (WORD*)(pDlg + 1);
	*pw++ = 0; *pw++ = 0;
	const wchar_t dlgTitle[] = L"Customize Key Bindings";
	memcpy(pw, dlgTitle, sizeof(dlgTitle));
	pw += _countof(dlgTitle);
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOINTEGRALHEIGHT;
	pItem->x = 5; pItem->y = 5;
	pItem->cx = DLG_W - 10; pItem->cy = DLG_H - 35;
	pItem->id = 1001;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0083;
	*pw++ = 0; *pw++ = 0;
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
	pItem->x = DLG_W / 2 - 25; pItem->y = DLG_H - 22; pItem->cx = 50; pItem->cy = 14;
	pItem->id = IDOK;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0080;
	const wchar_t ok[] = L"OK";
	memcpy(pw, ok, sizeof(ok));
	pw += _countof(ok);
	*pw++ = 0;

	return pDlg;
}

void CDirSxSUnifiedView::OnSxsCustomizeKeys()
{
	BYTE dlgBuf[1024];
	DLGTEMPLATE* pDlgTmpl = BuildUnifiedCustomizeKeysDlgTemplate(dlgBuf, sizeof(dlgBuf));

	DialogBoxIndirectParam(AfxGetInstanceHandle(), pDlgTmpl,
		m_hWnd, UnifiedCustomizeKeysDlgProc, reinterpret_cast<LPARAM>(&m_keyBindings));
}

/////////////////////////////////////////////////////////////////////////////
// Load / Save Key Bindings

void CDirSxSUnifiedView::LoadKeyBindings()
{
	m_keyBindings.clear();
	String bindings = GetOptionsMgr()->GetString(OPT_DIRVIEW_SXS_KEY_BINDINGS);
	if (bindings.empty())
		return;

	String::size_type pos = 0;
	while (pos < bindings.length())
	{
		String::size_type semi = bindings.find(_T(';'), pos);
		if (semi == String::npos)
			semi = bindings.length();

		String entry = bindings.substr(pos, semi - pos);
		pos = semi + 1;

		UINT cmdId = 0, vk = 0;
		int ctrl = 0, shift = 0, alt = 0;
		if (_stscanf_s(entry.c_str(), _T("%u:%u:%d:%d:%d"), &cmdId, &vk, &ctrl, &shift, &alt) == 5)
		{
			KeyBinding kb;
			kb.vkKey = vk;
			kb.bCtrl = (ctrl != 0);
			kb.bShift = (shift != 0);
			kb.bAlt = (alt != 0);
			m_keyBindings[cmdId] = kb;
		}
	}
}

void CDirSxSUnifiedView::SaveKeyBindings()
{
	String result;
	for (const auto& kv : m_keyBindings)
	{
		if (!result.empty())
			result += _T(';');
		result += strutils::format(_T("%u:%u:%d:%d:%d"),
			kv.first, kv.second.vkKey,
			kv.second.bCtrl ? 1 : 0,
			kv.second.bShift ? 1 : 0,
			kv.second.bAlt ? 1 : 0);
	}
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_KEY_BINDINGS, result);
}

/////////////////////////////////////////////////////////////////////////////
// Navigate to path

void CDirSxSUnifiedView::NavigateToPath(const String& sPath)
{
	CDirDoc* pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	const CDiffContext& ctxt = pDoc->GetDiffContext();
	PathContext paths = ctxt.GetNormalizedPaths();
	if (m_nContextSide >= 0 && m_nContextSide < paths.GetSize())
		paths.SetPath(m_nContextSide, sPath);

	fileopenflags_t dwFlags[3] = {};
	GetMainFrame()->DoFileOrFolderOpen(&paths, dwFlags, nullptr, _T(""),
		ctxt.m_bRecursive, nullptr);
}
