/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirPaneView.cpp
 *
 * @brief Implementation of CDirPaneView class for side-by-side folder comparison
 */

#include "StdAfx.h"
#include "DirPaneView.h"
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
#include "DirGutterView.h"
#include "MainFrm.h"
#include "FileLocation.h"
#include "FileTransform.h"
#include "paths.h"
#include "ShellFileOperations.h"
#include <afxole.h>
#include <Shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <Shlwapi.h>
#include <fstream>
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

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
}

// Default column width (same value as CDirView)
constexpr int DefColumnWidth = 111;

// BC-style colored folder icon indices (appended after standard icons)
enum BcFolderIcon
{
	BCFOLDER_IDENTICAL = 0, // Gray folder — all children same
	BCFOLDER_DIFFERENT,     // Red folder — contains differences
	BCFOLDER_ORPHAN,        // Purple folder — orphan (one side only)
	BCFOLDER_MIXED,         // Red-ish folder — mixed diffs + orphans
	BCFOLDER_UNKNOWN,       // Yellow folder — unscanned / unknown
	BCFOLDER_COUNT
};

// Base index in the image list where BC folder icons start
static int s_nBcFolderIconBase = -1;

/**
 * @brief Draw a simple folder icon shape filled with a given color.
 * The folder is drawn as: a tab on top-left, then a rectangle body.
 */
static void DrawColoredFolderIcon(CDC &dc, int cx, int cy, COLORREF fillColor)
{
	// Background: transparent (already cleared)
	CBrush brush(fillColor);
	CPen pen(PS_SOLID, 1, RGB(GetRValue(fillColor) * 2 / 3,
		GetGValue(fillColor) * 2 / 3, GetBValue(fillColor) * 2 / 3));
	CBrush* pOldBrush = dc.SelectObject(&brush);
	CPen* pOldPen = dc.SelectObject(&pen);

	// Tab portion (top-left): small rectangle
	int tabW = cx * 5 / 12;
	int tabH = cy / 5;
	dc.Rectangle(1, 1, tabW, 1 + tabH);

	// Body: main folder rectangle below the tab
	int bodyTop = 1 + tabH - 1;
	dc.Rectangle(1, bodyTop, cx - 1, cy - 1);

	dc.SelectObject(pOldBrush);
	dc.SelectObject(pOldPen);
}

// Text buffer for LVN_GETDISPINFO
static String s_rgDispinfoText[2];

static tchar_t* NTAPI AllocPaneDispinfoText(const String &s)
{
	static int i = 0;
	const tchar_t* pszText = (s_rgDispinfoText[i] = s).c_str();
	i ^= 1;
	return (tchar_t*)pszText;
}

/////////////////////////////////////////////////////////////////////////////
// CDirPaneView

IMPLEMENT_DYNCREATE(CDirPaneView, CListView)

CDirPaneView::CDirPaneView()
	: m_nThisPane(0)
	, m_pCoordinator(nullptr)
	, m_pList(nullptr)
	, m_bUseColors(true)
	, m_bRowStripes(false)
	, m_bRedisplayPending(false)
	, m_nCachedToleranceSecs(-1)
{
	m_cachedColors = {};
}

CDirPaneView::~CDirPaneView()
{
	// Kill any pending redisplay timer to prevent post-destruction callback
	if (m_bRedisplayPending)
		KillTimer(TIMER_REDISPLAY);
}

#ifdef _DEBUG
CDirDoc* CDirPaneView::GetDocument()
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CDirDoc)));
	return (CDirDoc*)m_pDocument;
}
#endif

CDirFrame* CDirPaneView::GetParentFrame()
{
	return static_cast<CDirFrame*>(CListView::GetParentFrame());
}

const CDiffContext& CDirPaneView::GetDiffContext() const
{
	return GetDocument()->GetDiffContext();
}

CDiffContext& CDirPaneView::GetDiffContext()
{
	return GetDocument()->GetDiffContext();
}

BEGIN_MESSAGE_MAP(CDirPaneView, CListView)
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
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnColumnClick)
	ON_NOTIFY_REFLECT(LVN_ITEMCHANGED, OnItemChanged)
	ON_NOTIFY_REFLECT(LVN_ENDSCROLL, OnScroll)
	ON_WM_MOUSEWHEEL()
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

BOOL CDirPaneView::PreCreateWindow(CREATESTRUCT& cs)
{
	__super::PreCreateWindow(cs);
	cs.style |= LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_EDITLABELS;
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	return TRUE;
}

/**
 * @brief Initialize the pane view.
 * Sets up the list control, image list, columns, and colors.
 */
void CDirPaneView::OnInitialUpdate()
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

	// In SxS mode, restrict columns to Name/Ext/Size/Modified for this pane
	m_pColItems->SetSxSPaneColumns(m_nThisPane);

	m_pList->SendMessage(CCM_SETUNICODEFORMAT, TRUE, 0);

	// Load user-selected font
	if (GetOptionsMgr()->GetBool(OPT_FONT_DIRCMP + OPT_FONT_USECUSTOM))
	{
		m_font.CreateFontIndirect(&theApp.m_lfDir);
		CWnd::SetFont(&m_font, TRUE);
	}

	// Create bold font for directory names (Phase 2: tree polish)
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

	// Load icons - same set as CDirView
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
			BcColors::ICON_FOLDER_MIXED,
			BcColors::ICON_FOLDER_UNKNOWN,
		};
		CDC dcMem;
		dcMem.CreateCompatibleDC(nullptr);
		for (int fi = 0; fi < BCFOLDER_COUNT; fi++)
		{
			CBitmap bmpColor, bmpMask;

			// Create 32-bit color bitmap
			bmpColor.CreateBitmap(iconCX, iconCY, 1, 32, nullptr);
			CBitmap* pOld = dcMem.SelectObject(&bmpColor);

			// Clear to transparent (black = masked out)
			dcMem.FillSolidRect(0, 0, iconCX, iconCY, RGB(0, 0, 0));
			DrawColoredFolderIcon(dcMem, iconCX, iconCY, folderColors[fi]);
			dcMem.SelectObject(pOld);

			// Create mask bitmap (black = opaque, white = transparent)
			bmpMask.CreateBitmap(iconCX, iconCY, 1, 1, nullptr);
			pOld = dcMem.SelectObject(&bmpMask);
			dcMem.FillSolidRect(0, 0, iconCX, iconCY, RGB(255, 255, 255));

			// Draw the same folder shape in black on the mask (opaque area)
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

	// Load columns — SxS mode uses fixed 4-column layout
	m_pList->SetRedraw(FALSE);

	// DPI-aware default widths
	const int dpi = CClientDC(this).GetDeviceCaps(LOGPIXELSX);
	auto px = [dpi](int pt) { return MulDiv(pt, dpi, 72); };

	// Insert the 4 BC-style column headers
	m_pList->InsertColumn(0, _T("Name"),     LVCFMT_LEFT,  px(200));
	m_pList->InsertColumn(1, _T("Ext"),      LVCFMT_LEFT,  px(50));
	m_pList->InsertColumn(2, _T("Size"),     LVCFMT_RIGHT, px(70));
	m_pList->InsertColumn(3, _T("Modified"), LVCFMT_LEFT,  px(130));

	// Load saved column widths if available
	const String& colWidthOpt = (m_nThisPane == 0) ? OPT_DIRVIEW_SXS_LEFT_COLUMN_WIDTHS :
		OPT_DIRVIEW_SXS_RIGHT_COLUMN_WIDTHS;
	String colWidths = GetOptionsMgr()->GetString(colWidthOpt);
	if (!colWidths.empty())
	{
		m_pColItems->LoadColumnWidths(
			colWidths,
			std::bind(&CListCtrl::SetColumnWidth, m_pList, std::placeholders::_1, std::placeholders::_2),
			px(DefColumnWidth));
	}

	// Extended styles
	DWORD exstyle = LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP | LVS_EX_INFOTIP | LVS_EX_DOUBLEBUFFER;
	m_pList->SetExtendedStyle(exstyle);

	m_pList->SetRedraw(TRUE);

	// Initialize row stripes setting
	m_bRowStripes = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_ROW_STRIPES);

	// Load configurable key bindings
	LoadKeyBindings();
}

BOOL CDirPaneView::PreTranslateMessage(MSG* pMsg)
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

BOOL CDirPaneView::OnChildNotify(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
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

/**
 * @brief Respond to LVN_GETDISPINFO message for this pane
 */
void CDirPaneView::ReflectGetdispinfo(NMLVDISPINFO *pParam)
{
	int nIdx = pParam->item.iItem;
	if (nIdx < 0 || nIdx >= static_cast<int>(m_listViewItems.size()))
		return;

	DIFFITEM *key = reinterpret_cast<DIFFITEM*>(m_listViewItems[nIdx].lParam);
	if (key == nullptr)
	{
		// Placeholder row - show empty
		if (pParam->item.mask & LVIF_TEXT)
			pParam->item.pszText = _T("");
		if (pParam->item.mask & LVIF_IMAGE)
			pParam->item.iImage = -1; // No image for placeholder
		return;
	}

	if (!GetDocument()->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);

	int i = m_pColItems->ColPhysToLog(pParam->item.iSubItem);

	if (pParam->item.mask & LVIF_TEXT)
	{
		String s = m_pColItems->ColGetTextToDisplay(&ctxt, i, di);
		pParam->item.pszText = AllocPaneDispinfoText(s);
	}
	if (pParam->item.mask & LVIF_IMAGE)
	{
		pParam->item.iImage = GetPaneColImage(di);
	}
	if (pParam->item.mask & LVIF_INDENT)
	{
		pParam->item.iIndent = m_listViewItems[nIdx].iIndent;
	}
}

/**
 * @brief Get the icon image index for an item in this pane.
 * For directories, uses BC-style colored folder icons based on content status.
 * For files, delegates to the standard GetColImage.
 */
int CDirPaneView::GetPaneColImage(const DIFFITEM &di) const
{
	if (!di.diffcode.isDirectory() || s_nBcFolderIconBase < 0)
	{
		// Non-directory or BC icons not loaded — use standard icons
		if (m_pCoordinator)
			return m_pCoordinator->GetPaneColImage(di, m_nThisPane);
		return GetColImage(di);
	}

	// Directory: use BC-style colored folder icons
	if (di.diffcode.isResultError())
		return DIFFIMG_ERROR;
	if (di.diffcode.isResultAbort())
		return DIFFIMG_ABORT;
	if (di.diffcode.isResultFiltered())
		return DIFFIMG_DIRSKIP;

	const CDiffContext &ctxt = GetDiffContext();

	// Orphan folder
	if (!IsItemExistAll(ctxt, di))
		return s_nBcFolderIconBase + BCFOLDER_ORPHAN;

	// Folder on both sides — check content status
	if (m_pCoordinator)
	{
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
		default:
			return s_nBcFolderIconBase + BCFOLDER_UNKNOWN;
		}
	}

	return s_nBcFolderIconBase + BCFOLDER_UNKNOWN;
}

/**
 * @brief Custom draw handler for row coloring
 */
void CDirPaneView::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
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
		// Check if item is a directory — use bold font + status-based color
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
		GetColors(static_cast<int>(lpC->nmcd.dwItemSpec), lpC->iSubItem, lpC->clrTextBk, lpC->clrText);
	}
}

/**
 * @brief Custom draw handler for the column header control (dark theme).
 */
void CDirPaneView::OnHeaderCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMCUSTOMDRAW lpCD = (LPNMCUSTOMDRAW)pNMHDR;
	*pResult = CDRF_DODEFAULT;

	// Only handle the header control (child of the list)
	if (!m_pList || lpCD->hdr.hwndFrom != ListView_GetHeader(m_pList->m_hWnd))
		return;

	if (lpCD->dwDrawStage == CDDS_PREPAINT)
	{
		*pResult = CDRF_NOTIFYITEMDRAW;
		return;
	}
	if (lpCD->dwDrawStage == CDDS_ITEMPREPAINT)
	{
		// Fill background with dark color
		::FillRect(lpCD->hdc, &lpCD->rc, (HBRUSH)::CreateSolidBrush(BcColors::COLHDR_BG));

		// Draw bottom border
		HPEN hPen = ::CreatePen(PS_SOLID, 1, BcColors::BORDER);
		HPEN hOld = (HPEN)::SelectObject(lpCD->hdc, hPen);
		::MoveToEx(lpCD->hdc, lpCD->rc.left, lpCD->rc.bottom - 1, nullptr);
		::LineTo(lpCD->hdc, lpCD->rc.right, lpCD->rc.bottom - 1);
		::SelectObject(lpCD->hdc, hOld);
		::DeleteObject(hPen);

		// Get header item text
		HDITEM hdi = {};
		tchar_t szText[128] = {};
		hdi.mask = HDI_TEXT;
		hdi.pszText = szText;
		hdi.cchTextMax = _countof(szText);
		Header_GetItem(lpCD->hdr.hwndFrom, (int)lpCD->dwItemSpec, &hdi);

		// Draw text with light color
		::SetBkMode(lpCD->hdc, TRANSPARENT);
		::SetTextColor(lpCD->hdc, BcColors::TEXT_HEADER);
		CRect rcText = lpCD->rc;
		rcText.DeflateRect(4, 0);
		::DrawText(lpCD->hdc, szText, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

		*pResult = CDRF_SKIPDEFAULT;
		return;
	}
}

/**
 * @brief Get colors for an item row — Beyond Compare dark theme.
 * Dark alternating row backgrounds with status-colored text.
 */
void CDirPaneView::GetColors(int nRow, int nCol, COLORREF& clrBk, COLORREF& clrText) const
{
	// Dark alternating rows
	clrBk = (nRow & 1) ? BcColors::BG_ALT : BcColors::BG_DARK;
	clrText = BcColors::TEXT_NORMAL;

	if (nRow < 0 || nRow >= static_cast<int>(m_listViewItems.size()))
		return;

	DIFFITEM *key = reinterpret_cast<DIFFITEM*>(m_listViewItems[nRow].lParam);
	if (key == nullptr)
	{
		// Placeholder row — invisible text
		clrText = clrBk;
		return;
	}

	if (!GetDocument()->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);

	if (di.isEmpty())
	{
		// Empty item — default colors
	}
	else if (di.diffcode.isResultFiltered())
	{
		clrText = BcColors::TEXT_FILTERED;
	}
	else if (!IsItemExistAll(ctxt, di))
	{
		// Orphan (left-only or right-only)
		clrText = BcColors::TEXT_ORPHAN;
	}
	else if (di.diffcode.isResultDiff())
	{
		// Different files — all red regardless of timestamp
		clrText = BcColors::TEXT_DIFF;
	}
	// else: identical — keep TEXT_NORMAL (white)
}

/**
 * @brief Get the DIFFITEM key for a given list index
 */
DIFFITEM* CDirPaneView::GetItemKey(int idx) const
{
	if (idx < 0 || idx >= static_cast<int>(m_listViewItems.size()))
		return nullptr;
	return reinterpret_cast<DIFFITEM*>(m_listViewItems[idx].lParam);
}

/**
 * @brief Delete all display items from the list
 */
void CDirPaneView::DeleteAllDisplayItems()
{
	m_listViewItems.clear();
	if (m_pList && m_pList->GetSafeHwnd())
	{
		m_pList->DeleteAllItems();
		m_pList->SetItemCount(0);
	}
}

/**
 * @brief Called by coordinator to update this pane's display from the row mapping.
 */
void CDirPaneView::UpdateFromRowMapping()
{
	if (!m_pCoordinator || !m_pList)
		return;

	// Invalidate per-draw caches
	m_nCachedToleranceSecs = -1;

	m_pList->SetRedraw(FALSE);
	m_listViewItems.clear();

	const auto& rowMapping = m_pCoordinator->GetRowMapping();
	for (int i = 0; i < static_cast<int>(rowMapping.size()); ++i)
	{
		const auto& row = rowMapping[i];
		ListViewOwnerDataItem item;
		bool existsOnThisPane = (m_nThisPane == 0) ? row.existsOnLeft : row.existsOnRight;

		if (existsOnThisPane)
		{
			item.lParam = reinterpret_cast<LPARAM>(row.diffpos);
			item.iImage = I_IMAGECALLBACK;
			item.iIndent = row.indent;
		}
		else
		{
			// Placeholder row
			item.lParam = 0;
			item.iImage = -1;
			item.iIndent = 0;
		}
		m_listViewItems.push_back(item);
	}

	m_pList->SetItemCount(static_cast<int>(m_listViewItems.size()));
	m_pList->SetRedraw(TRUE);
	m_pList->Invalidate();
}

/**
 * @brief Handle NM_DBLCLK notification for double-click on list items.
 * This is the primary handler for double-clicks — more reliable than
 * WM_LBUTTONDBLCLK for CListView with LVS_OWNERDATA.
 */
void CDirPaneView::OnDblClick(NMHDR* pNMHDR, LRESULT* pResult)
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

/**
 * @brief Fallback handler for WM_LBUTTONDBLCLK.
 */
void CDirPaneView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	// NM_DBLCLK handles the main logic; this is a fallback
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

void CDirPaneView::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
}

/**
 * @brief Handle UI update messages from the diff thread.
 * Only the left pane (pane 0) processes these messages to avoid
 * double-processing. It triggers redisplay through the coordinator.
 */
LRESULT CDirPaneView::OnUpdateUIMessage(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	// Only process on the left pane to avoid double-processing
	if (m_nThisPane != 0)
		return 0;

	CDirDoc *pDoc = GetDocument();
	if (pDoc == nullptr || m_pCoordinator == nullptr)
		return 0;

	if (wParam == CDiffThread::EVENT_COMPARE_COMPLETED)
	{
		pDoc->CompareReady();

		if (!pDoc->GetGeneratingReport())
			m_pCoordinator->Redisplay();
	}
	else if (wParam == CDiffThread::EVENT_COMPARE_PROGRESSED)
	{
		// Throttle progress updates — at most once per 500ms to avoid
		// full tree rebuild on every 2-second progress event
		if (!m_bRedisplayPending)
		{
			m_bRedisplayPending = true;
			SetTimer(TIMER_REDISPLAY, 500, nullptr);
		}
	}
	else if (wParam == CDiffThread::EVENT_COLLECT_COMPLETED)
	{
		m_pCoordinator->Redisplay();
	}

	return 0;
}

/**
 * @brief Timer handler for throttled redisplay during comparison progress.
 */
void CDirPaneView::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == TIMER_REDISPLAY)
	{
		KillTimer(TIMER_REDISPLAY);
		m_bRedisplayPending = false;
		if (m_pCoordinator)
			m_pCoordinator->Redisplay();
	}
	else
	{
		CListView::OnTimer(nIDEvent);
	}
}

/**
 * @brief Open comparison for the first selected item on this pane.
 * Uses GetMainFrame()->DoFileOrFolderOpen() to open files or subfolders.
 */
void CDirPaneView::OpenSelectedItem()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();

	// Find selected item
	int nItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
	if (nItem < 0)
		return;

	DIFFITEM *key = GetItemKey(nItem);
	if (!key)
		return;

	const DIFFITEM &di = ctxt.GetDiffAt(key);

	if (di.diffcode.isDirectory())
	{
		// Directory: toggle expand/collapse
		ToggleExpandSubdir(nItem);
		return;
	}

	// Build full paths for all sides (same as CDirView::OpenSelection)
	PathContext paths = GetItemFileNames(ctxt, di);
	int nDirs = ctxt.GetCompareDirs();

	// For orphan files (only on one side), use NUL device for the missing side
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

/**
 * @brief Open cross-comparison: compare one selected file from each pane.
 */
void CDirPaneView::OpenCrossComparison()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();

	// Get selections from both panes
	std::vector<DIFFITEM*> leftItems, rightItems;
	m_pCoordinator->GetSelectedItems(0, leftItems);
	m_pCoordinator->GetSelectedItems(1, rightItems);

	if (leftItems.empty() || rightItems.empty())
		return;

	const DIFFITEM &diLeft = ctxt.GetDiffAt(leftItems[0]);
	const DIFFITEM &diRight = ctxt.GetDiffAt(rightItems[0]);

	// Build paths: left item's left-side path vs right item's right-side path
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

/**
 * @brief Handle keyboard shortcuts in the pane view.
 * Tab switches between panes, Enter opens comparison.
 */
void CDirPaneView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_TAB && m_pCoordinator)
	{
		// Switch focus to the other pane
		CDirPaneView *pOtherPane = (m_nThisPane == 0) ?
			m_pCoordinator->GetRightPaneView() : m_pCoordinator->GetLeftPaneView();
		if (pOtherPane)
		{
			m_pCoordinator->SetActivePane(pOtherPane->GetPaneIndex());
			pOtherPane->SetFocus();
		}
		return;
	}
	if (nChar == VK_RETURN)
	{
		// If focused item is a directory, toggle expand; otherwise open file comparison
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

/**
 * @brief Display context menu for the pane view.
 */
void CDirPaneView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (m_pList->GetItemCount() == 0)
		return;

	GetParentFrame()->ActivateFrame();

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

// Phase 4: Swap sides handler
void CDirPaneView::OnSxsSwapSides()
{
	if (m_pCoordinator)
		m_pCoordinator->SwapSides();
}

// Phase 4: Copy to other side handler — native SxS file operation
void CDirPaneView::OnSxsCopy()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	std::vector<DIFFITEM*> items;
	m_pCoordinator->GetSelectedItems(m_nThisPane, items);
	if (items.empty())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int srcSide = m_nThisPane;
	int dstSide = (m_nThisPane == 0) ? (ctxt.GetCompareDirs() - 1) : 0;

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
			// Build destination directory from source relative path
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

// Phase 4: Move to other side handler — native SxS file operation
void CDirPaneView::OnSxsMove()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	std::vector<DIFFITEM*> items;
	m_pCoordinator->GetSelectedItems(m_nThisPane, items);
	if (items.empty())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int srcSide = m_nThisPane;
	int dstSide = (m_nThisPane == 0) ? (ctxt.GetCompareDirs() - 1) : 0;

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

// Phase 5: Open comparison for selected item
void CDirPaneView::OnSxsOpenCompare()
{
	OpenSelectedItem();
}

// Phase 5: Cross-compare selected items from each pane
void CDirPaneView::OnSxsCrossCompare()
{
	OpenCrossComparison();
}

// Phase 4: Enable commands only when items are selected
void CDirPaneView::OnUpdateSxsNeedSelection(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_pList && m_pList->GetSelectedCount() > 0);
}

/**
 * @brief Handle column header click -- sort both panes by the clicked column.
 * When the same column is clicked again, the sort direction is toggled.
 * For a new column, the default sort direction from DirViewColItems is used.
 * The coordinator stores the sort state and applies it during BuildRowMapping.
 */
void CDirPaneView::OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	if (!m_pCoordinator || !m_pColItems)
		return;

	NM_LISTVIEW *pNMListView = (NM_LISTVIEW *)pNMHDR;
	int sortcol = m_pColItems->ColPhysToLog(pNMListView->iSubItem);
	if (sortcol < 0 || sortcol >= m_pColItems->GetColCount())
		return;

	int oldSortCol = m_pCoordinator->GetSortColumn();
	bool bAscending;
	if (sortcol == oldSortCol)
	{
		// Same column -- toggle direction
		bAscending = !m_pCoordinator->GetSortAscending();
	}
	else
	{
		// New column -- use default direction (most columns ascending, dates descending)
		bAscending = m_pColItems->IsDefaultSortAscending(sortcol);
	}

	// Tell the coordinator to sort both panes (triggers Redisplay)
	m_pCoordinator->SetSortColumn(sortcol, bAscending);

	// Update sort header indicators on both panes
	UpdateSortHeaderIndicator();
	CDirPaneView *pOtherPane = (m_nThisPane == 0) ?
		m_pCoordinator->GetRightPaneView() : m_pCoordinator->GetLeftPaneView();
	if (pOtherPane)
		pOtherPane->UpdateSortHeaderIndicator();
}

/**
 * @brief Update the sort header arrow indicator to match the coordinator's sort state.
 * Maps the coordinator's logical sort column to the physical column index
 * in this pane's column layout, then updates the CSortHeaderCtrl.
 */
void CDirPaneView::UpdateSortHeaderIndicator()
{
	if (!m_pCoordinator || !m_pColItems)
		return;

	int sortCol = m_pCoordinator->GetSortColumn();
	if (sortCol < 0)
	{
		m_ctlSortHeader.SetSortImage(-1, true);
		return;
	}

	int physCol = m_pColItems->ColLogToPhys(sortCol);
	m_ctlSortHeader.SetSortImage(physCol, m_pCoordinator->GetSortAscending());
}

/**
 * @brief Handle selection change -- sync selection to opposite pane, update gutter, and update status bar.
 */
void CDirPaneView::OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLISTVIEW *pNMLV = (NMLISTVIEW *)pNMHDR;
	*pResult = 0;

	if (!(pNMLV->uChanged & LVIF_STATE))
		return;
	if ((pNMLV->uNewState & LVIS_SELECTED) == (pNMLV->uOldState & LVIS_SELECTED))
		return;

	// Sync selection to the other pane (guard against recursion)
	static bool bSyncing = false;
	if (!bSyncing && m_pCoordinator)
	{
		bSyncing = true;
		CDirPaneView *pOtherPane = (m_nThisPane == 0) ?
			m_pCoordinator->GetRightPaneView() : m_pCoordinator->GetLeftPaneView();
		if (pOtherPane)
		{
			CListCtrl &otherList = pOtherPane->GetListCtrl();
			if (otherList.GetSafeHwnd())
			{
				int nItem = pNMLV->iItem;
				if (pNMLV->uNewState & LVIS_SELECTED)
					otherList.SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);
				else
					otherList.SetItemState(nItem, 0, LVIS_SELECTED);
			}
		}
		bSyncing = false;
	}

	// Sync focus too
	if ((pNMLV->uNewState & LVIS_FOCUSED) != (pNMLV->uOldState & LVIS_FOCUSED))
	{
		if (!bSyncing && m_pCoordinator)
		{
			bSyncing = true;
			CDirPaneView *pOtherPane = (m_nThisPane == 0) ?
				m_pCoordinator->GetRightPaneView() : m_pCoordinator->GetLeftPaneView();
			if (pOtherPane)
			{
				CListCtrl &otherList = pOtherPane->GetListCtrl();
				if (otherList.GetSafeHwnd())
				{
					int nItem = pNMLV->iItem;
					if (pNMLV->uNewState & LVIS_FOCUSED)
						otherList.SetItemState(nItem, LVIS_FOCUSED, LVIS_FOCUSED);
					else
						otherList.SetItemState(nItem, 0, LVIS_FOCUSED);
				}
			}
			bSyncing = false;
		}
	}

	// Update gutter display on selection change
	CDirFrame *pFrame = GetParentFrame();
	if (pFrame)
	{
		CDirGutterView *pGutter = pFrame->GetGutterView();
		if (pGutter)
			pGutter->UpdateDisplay();
	}

	// Update status bar: show detail for single selection, summary counts otherwise
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
			// Multiple or no selection: show summary counts
			pFrame->SetStatus(m_pCoordinator->FormatStatusString().c_str());
		}
	}
}

/**
 * @brief Handle scroll events — sync other pane and gutter view scroll position.
 */
void CDirPaneView::OnScroll(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	if (!m_pCoordinator || !m_pList)
		return;

	int nTopIndex = m_pList->GetTopIndex();

	// Sync the other pane
	CDirPaneView *pOtherPane = (m_nThisPane == 0) ?
		m_pCoordinator->GetRightPaneView() : m_pCoordinator->GetLeftPaneView();
	if (pOtherPane)
	{
		CListCtrl &otherList = pOtherPane->GetListCtrl();
		if (otherList.GetSafeHwnd() && otherList.GetTopIndex() != nTopIndex)
		{
			otherList.EnsureVisible(nTopIndex + otherList.GetCountPerPage() - 1, FALSE);
			otherList.EnsureVisible(nTopIndex, FALSE);
		}
	}

	// Sync gutter
	CDirFrame *pFrame = GetParentFrame();
	if (pFrame)
	{
		CDirGutterView *pGutter = pFrame->GetGutterView();
		if (pGutter)
			pGutter->SetScrollPos(nTopIndex);
	}
}

/**
 * @brief Handle mouse wheel — sync other pane and gutter after scrolling.
 */
BOOL CDirPaneView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	BOOL bResult = __super::OnMouseWheel(nFlags, zDelta, pt);

	// After our scroll, sync other pane and gutter
	if (m_pCoordinator && m_pList)
	{
		int nTopIndex = m_pList->GetTopIndex();

		CDirPaneView *pOtherPane = (m_nThisPane == 0) ?
			m_pCoordinator->GetRightPaneView() : m_pCoordinator->GetLeftPaneView();
		if (pOtherPane)
		{
			CListCtrl &otherList = pOtherPane->GetListCtrl();
			if (otherList.GetSafeHwnd() && otherList.GetTopIndex() != nTopIndex)
			{
				otherList.EnsureVisible(nTopIndex + otherList.GetCountPerPage() - 1, FALSE);
				otherList.EnsureVisible(nTopIndex, FALSE);
			}
		}

		CDirFrame *pFrame = GetParentFrame();
		if (pFrame)
		{
			CDirGutterView *pGutter = pFrame->GetGutterView();
			if (pGutter)
				pGutter->SetScrollPos(nTopIndex);
		}
	}

	return bResult;
}

// --- Tree mode expand/collapse ---

/**
 * @brief Expand a folder item in tree mode.
 */
void CDirPaneView::ExpandSubdir(int sel)
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
		m_pCoordinator->Redisplay();
	}
}

/**
 * @brief Collapse a folder item in tree mode.
 */
void CDirPaneView::CollapseSubdir(int sel)
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

/**
 * @brief Toggle expand/collapse of a folder item.
 */
void CDirPaneView::ToggleExpandSubdir(int sel)
{
	if (!m_pCoordinator)
		return;
	DIFFITEM *key = GetItemKey(sel);
	if (!key)
		return;
	DIFFITEM &di = GetDiffContext().GetDiffRefAt(key);
	if (!di.diffcode.isDirectory())
		return;
	// Toggle expand/collapse — even if HasChildren() is false (scan may
	// still be running), we set the flag so children appear once available.
	if (di.customFlags & ViewCustomFlags::EXPANDED)
		di.customFlags &= ~ViewCustomFlags::EXPANDED;
	else
		di.customFlags |= ViewCustomFlags::EXPANDED;
	m_pCoordinator->Redisplay();
}

/**
 * @brief Expand all subdirectories in tree mode.
 */
void CDirPaneView::OnExpandAllSubdirs()
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

/**
 * @brief Collapse all subdirectories in tree mode.
 */
void CDirPaneView::OnCollapseAllSubdirs()
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

// Tree mode command handlers
void CDirPaneView::OnSxsToggleTree()
{
	bool bCurrent = GetOptionsMgr()->GetBool(OPT_TREE_MODE);
	GetOptionsMgr()->SaveOption(OPT_TREE_MODE, !bCurrent);
	if (m_pCoordinator)
		m_pCoordinator->Redisplay();
}

void CDirPaneView::OnSxsExpandAll()
{
	OnExpandAllSubdirs();
}

void CDirPaneView::OnSxsCollapseAll()
{
	OnCollapseAllSubdirs();
}

void CDirPaneView::OnSxsFlattenMode()
{
	bool bCurrent = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_FLATTEN_MODE);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_FLATTEN_MODE, !bCurrent);
	if (m_pCoordinator)
		m_pCoordinator->Redisplay();
}

void CDirPaneView::OnUpdateSxsToggleTree(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_TREE_MODE));
}

void CDirPaneView::OnUpdateSxsFlattenMode(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_FLATTEN_MODE));
}

// --- Batch 5: Navigation & Operations ---

/**
 * @brief Refresh (F5) — rescan the folder comparison.
 */
void CDirPaneView::OnSxsRefresh()
{
	CDirDoc *pDoc = GetDocument();
	if (pDoc)
		pDoc->Rescan();
}

/**
 * @brief Rename in-place (F2) — begin label edit on the selected item.
 */
void CDirPaneView::OnSxsRename()
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

/**
 * @brief Handle end of label edit — perform the actual rename.
 */
void CDirPaneView::OnEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVDISPINFO *pDispInfo = (NMLVDISPINFO *)pNMHDR;
	*pResult = FALSE;

	if (pDispInfo->item.pszText == nullptr)
		return; // Edit was cancelled

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

	// Rename on the side this pane represents
	int side = m_nThisPane;
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
		// Rescan to pick up the change
		pDoc->Rescan();
	}
	else
	{
		String msg = strutils::format(_T("Failed to rename '%s' to '%s'"),
			oldPath.c_str(), newPath.c_str());
		AfxMessageBox(msg.c_str(), MB_ICONERROR);
	}
}

/**
 * @brief Dialog proc for the simple Find Filename dialog.
 * The dialog is created from a memory template with: static label, edit, OK, Cancel.
 */
static INT_PTR CALLBACK FindFilenameDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
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

/**
 * @brief Build an in-memory dialog template for the Find Filename dialog.
 * Returns the dialog template pointer within the provided buffer.
 */
static DLGTEMPLATE* BuildFindDlgTemplate(BYTE* buffer, size_t bufSize)
{
	memset(buffer, 0, bufSize);
	const int DLG_W = 260, DLG_H = 75;

	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)buffer;
	pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
	pDlg->cdit = 4;
	pDlg->x = 0; pDlg->y = 0;
	pDlg->cx = DLG_W; pDlg->cy = DLG_H;

	WORD* pw = (WORD*)(pDlg + 1);
	*pw++ = 0; // menu
	*pw++ = 0; // class
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

/**
 * @brief Find filename (Ctrl+F) — prompt for filename and scroll to match.
 */
void CDirPaneView::OnSxsFindFilename()
{
	if (!m_pCoordinator || !m_pList)
		return;

	static String s_lastSearch;
	tchar_t szInput[MAX_PATH] = {};
	if (!s_lastSearch.empty())
		_tcsncpy_s(szInput, s_lastSearch.c_str(), _TRUNCATE);

	BYTE dlgBuf[1024];
	DLGTEMPLATE* pDlgTmpl = BuildFindDlgTemplate(dlgBuf, sizeof(dlgBuf));

	INT_PTR nResult = DialogBoxIndirectParam(AfxGetInstanceHandle(), pDlgTmpl,
		m_hWnd, FindFilenameDlgProc, reinterpret_cast<LPARAM>(szInput));

	if (nResult != IDOK)
		return;

	String searchText = szInput;
	if (searchText.empty())
		return;
	s_lastSearch = searchText;
	m_sFindPattern = searchText;

	// Search through items for matching filename (case-insensitive substring)
	CDiffContext &ctxt = GetDiffContext();
	String searchLower = searchText;
	CharLower(&searchLower[0]);

	int nStart = m_pList->GetNextItem(-1, LVNI_FOCUSED);
	if (nStart < 0) nStart = 0;
	int nCount = static_cast<int>(m_listViewItems.size());

	// Search from current position, wrapping around
	for (int offset = 1; offset <= nCount; offset++)
	{
		int i = (nStart + offset) % nCount;
		DIFFITEM *key = GetItemKey(i);
		if (!key)
			continue;

		const DIFFITEM &di = ctxt.GetDiffAt(key);
		int side = m_nThisPane;
		if (side >= ctxt.GetCompareDirs())
			side = ctxt.GetCompareDirs() - 1;
		if (!di.diffcode.exists(side))
			continue;

		String filename = String(di.diffFileInfo[side].filename);
		CharLower(&filename[0]);

		if (filename.find(searchLower) != String::npos)
		{
			// Found — select and scroll to this item
			m_pList->SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
			m_pList->SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			m_pList->EnsureVisible(i, FALSE);
			return;
		}
	}

	AfxMessageBox(_("Filename not found.").c_str(), MB_ICONINFORMATION);
}

/**
 * @brief Save column widths and orders to the registry/INI for this pane.
 */
void CDirPaneView::SaveColumnState()
{
	if (!m_pList || !m_pColItems)
		return;

	// Save column widths
	const String& colWidthOpt = (m_nThisPane == 0) ? OPT_DIRVIEW_SXS_LEFT_COLUMN_WIDTHS :
		OPT_DIRVIEW_SXS_RIGHT_COLUMN_WIDTHS;
	String sWidths = m_pColItems->SaveColumnWidths(
		std::bind(&CListCtrl::GetColumnWidth, m_pList, std::placeholders::_1));
	GetOptionsMgr()->SaveOption(colWidthOpt, sWidths);

	// Save column orders
	const String& colOrderOpt = (m_nThisPane == 0) ? OPT_DIRVIEW_SXS_LEFT_COLUMN_ORDERS :
		OPT_DIRVIEW_SXS_RIGHT_COLUMN_ORDERS;
	String sOrders = m_pColItems->SaveColumnOrders();
	GetOptionsMgr()->SaveOption(colOrderOpt, sOrders);
}

// --- Batch 6: Smart Selection Commands ---

/**
 * @brief Select all non-placeholder items.
 */
void CDirPaneView::OnSxsSelectAll()
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

/**
 * @brief Select items where this pane's file is newer than the other pane's.
 */
void CDirPaneView::OnSxsSelectNewer()
{
	if (!m_pList || !GetDocument() || !GetDocument()->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int otherPane = (m_nThisPane == 0) ? (ctxt.GetCompareDirs() - 1) : 0;
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

		Poco::Timestamp::TimeDiff diff = di.diffFileInfo[m_nThisPane].mtime - di.diffFileInfo[otherPane].mtime;
		Poco::Timestamp::TimeDiff toleranceUs = static_cast<Poco::Timestamp::TimeDiff>(toleranceSecs) * Poco::Timestamp::resolution();
		if (diff > toleranceUs)
			m_pList->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
	}
}

/**
 * @brief Select orphan items (unique to one side).
 */
void CDirPaneView::OnSxsSelectOrphans()
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

/**
 * @brief Select items that are different.
 */
void CDirPaneView::OnSxsSelectDifferent()
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

/**
 * @brief Invert the current selection.
 */
void CDirPaneView::OnSxsInvertSelection()
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

// --- Phase 2: Next/Previous difference navigation ---

void CDirPaneView::OnSxsNextDiff()
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
			// Found a difference — select it in both panes
			m_pCoordinator->SelectRowInBothPanes(i);
			m_pList->EnsureVisible(i, FALSE);
			return;
		}
	}
}

void CDirPaneView::OnSxsPrevDiff()
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
			m_pCoordinator->SelectRowInBothPanes(i);
			m_pList->EnsureVisible(i, FALSE);
			return;
		}
	}
}

void CDirPaneView::OnSxsDelete()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	std::vector<DIFFITEM*> items;
	m_pCoordinator->GetSelectedItems(m_nThisPane, items);
	if (items.empty())
		return;

	// Confirm deletion
	String msg = strutils::format(_T("Delete %d selected item(s) from this side?"), static_cast<int>(items.size()));
	if (AfxMessageBox(msg.c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int side = m_nThisPane;

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

// --- Phase 2: Sync operation handlers ---

void CDirPaneView::OnSxsUpdateLeft()
{
	if (m_pCoordinator)
	{
		if (AfxMessageBox(_T("Update Left: Copy newer and orphan files from right to left?"), MB_YESNO | MB_ICONQUESTION) == IDYES)
			m_pCoordinator->UpdateLeft();
	}
}

void CDirPaneView::OnSxsUpdateRight()
{
	if (m_pCoordinator)
	{
		if (AfxMessageBox(_T("Update Right: Copy newer and orphan files from left to right?"), MB_YESNO | MB_ICONQUESTION) == IDYES)
			m_pCoordinator->UpdateRight();
	}
}

void CDirPaneView::OnSxsUpdateBoth()
{
	if (m_pCoordinator)
	{
		if (AfxMessageBox(_T("Update Both: Copy newer and orphan files in both directions?"), MB_YESNO | MB_ICONQUESTION) == IDYES)
			m_pCoordinator->UpdateBoth();
	}
}

void CDirPaneView::OnSxsMirrorLeft()
{
	if (m_pCoordinator)
	{
		if (AfxMessageBox(_T("Mirror to Left: Make left side identical to right side?\nThis will copy different files and delete left-only orphans."), MB_YESNO | MB_ICONWARNING) == IDYES)
			m_pCoordinator->MirrorLeft();
	}
}

void CDirPaneView::OnSxsMirrorRight()
{
	if (m_pCoordinator)
	{
		if (AfxMessageBox(_T("Mirror to Right: Make right side identical to left side?\nThis will copy different files and delete right-only orphans."), MB_YESNO | MB_ICONWARNING) == IDYES)
			m_pCoordinator->MirrorRight();
	}
}

void CDirPaneView::OnSxsCompareContents()
{
	CDirDoc *pDoc = GetDocument();
	if (pDoc)
		pDoc->Rescan();
}

// --- CRC Compare handler ---

/**
 * @brief Compute CRC32 for selected items and show results.
 * For items that exist on both sides, compares the CRC values.
 * For items on only one side, shows the CRC of that file.
 */
void CDirPaneView::OnSxsCrcCompare()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	std::vector<DIFFITEM*> items;
	m_pCoordinator->GetSelectedItems(m_nThisPane, items);
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
			if (bMatch)
				nMatch++;
			else
				nDiffer++;

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

	// Summary line
	String summary = strutils::format(_T("\r\n--- Summary: %d match, %d differ, %d single-side ---"),
		nMatch, nDiffer, nSingleSide);
	result += summary;

	if (m_pCoordinator)
		m_pCoordinator->LogOperation(strutils::format(_T("CRC Compare: %d items, %d match, %d differ"),
			static_cast<int>(items.size()), nMatch, nDiffer));

	AfxMessageBox(result.c_str(), MB_ICONINFORMATION);
}

// --- Touch Timestamps handler ---

/**
 * @brief Copy the modification timestamp from files on this pane's side to the other side.
 * Only processes items that exist on both sides.
 */
void CDirPaneView::OnSxsTouchTimestamps()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	std::vector<DIFFITEM*> items;
	m_pCoordinator->GetSelectedItems(m_nThisPane, items);
	if (items.empty())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int srcSide = m_nThisPane;
	int dstSide = (m_nThisPane == 0) ? (ctxt.GetCompareDirs() - 1) : 0;

	// Confirm
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

	String resultMsg = strutils::format(
		_T("Touch Timestamps complete.\nSucceeded: %d\nFailed: %d"), nSuccess, nFailed);
	AfxMessageBox(resultMsg.c_str(), MB_ICONINFORMATION);

	// Rescan to reflect updated timestamps
	pDoc->Rescan();
}

// --- Show Log handler ---

/**
 * @brief Dialog proc for the Log Panel dialog.
 * Shows accumulated log messages in a scrollable read-only edit control.
 */
static INT_PTR CALLBACK LogDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			// The log text is passed via lParam as a LPCTSTR
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
		if (LOWORD(wParam) == 1002) // Clear button
		{
			HWND hEdit = GetDlgItem(hDlg, 1001);
			if (hEdit)
				SetWindowText(hEdit, _T(""));
			EndDialog(hDlg, 1002); // Special return code to signal "clear"
			return TRUE;
		}
		break;
	case WM_SIZE:
		{
			int cx = LOWORD(lParam);
			int cy = HIWORD(lParam);
			HWND hEdit = GetDlgItem(hDlg, 1001);
			if (hEdit)
				MoveWindow(hEdit, 5, 5, cx - 10, cy - 40, TRUE);
			HWND hOk = GetDlgItem(hDlg, IDOK);
			if (hOk)
				MoveWindow(hOk, cx / 2 - 80, cy - 30, 70, 24, TRUE);
			HWND hClear = GetDlgItem(hDlg, 1002);
			if (hClear)
				MoveWindow(hClear, cx / 2 + 10, cy - 30, 70, 24, TRUE);
		}
		return TRUE;
	}
	return FALSE;
}

/**
 * @brief Build an in-memory dialog template for the Log Panel dialog.
 */
static DLGTEMPLATE* BuildLogDlgTemplate(BYTE* buffer, size_t bufSize)
{
	memset(buffer, 0, bufSize);
	const int DLG_W = 350, DLG_H = 250;

	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)buffer;
	pDlg->style = DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
	pDlg->cdit = 3; // Edit control + OK button + Clear button
	pDlg->x = 0; pDlg->y = 0;
	pDlg->cx = DLG_W; pDlg->cy = DLG_H;

	WORD* pw = (WORD*)(pDlg + 1);
	*pw++ = 0; // menu
	*pw++ = 0; // class
	const wchar_t dlgTitle[] = L"Operation Log";
	memcpy(pw, dlgTitle, sizeof(dlgTitle));
	pw += _countof(dlgTitle);
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	// Edit control (multiline, read-only, scrollable)
	DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL |
		ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY;
	pItem->x = 5; pItem->y = 5;
	pItem->cx = DLG_W - 10; pItem->cy = DLG_H - 30;
	pItem->id = 1001;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0081; // Edit class
	*pw++ = 0; // no title
	*pw++ = 0; // no extra data
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	// OK button
	pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
	pItem->x = DLG_W / 2 - 80; pItem->y = DLG_H - 20;
	pItem->cx = 60; pItem->cy = 14;
	pItem->id = IDOK;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0080; // Button class
	const wchar_t ok[] = L"OK";
	memcpy(pw, ok, sizeof(ok));
	pw += _countof(ok);
	*pw++ = 0;
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	// Clear button
	pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
	pItem->x = DLG_W / 2 + 10; pItem->y = DLG_H - 20;
	pItem->cx = 60; pItem->cy = 14;
	pItem->id = 1002;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0080; // Button class
	const wchar_t clear[] = L"Clear";
	memcpy(pw, clear, sizeof(clear));
	pw += _countof(clear);
	*pw++ = 0;

	return pDlg;
}

/**
 * @brief Show the operation log in a modal dialog.
 */
void CDirPaneView::OnSxsShowLog()
{
	if (!m_pCoordinator)
		return;

	const auto& messages = m_pCoordinator->GetLogMessages();

	// Build the log text
	String logText;
	if (messages.empty())
	{
		logText = _T("No operations logged yet.");
	}
	else
	{
		for (const auto& msg : messages)
		{
			logText += msg;
			logText += _T("\r\n");
		}
	}

	BYTE dlgBuf[1024];
	DLGTEMPLATE* pDlgTmpl = BuildLogDlgTemplate(dlgBuf, sizeof(dlgBuf));
	INT_PTR nResult = DialogBoxIndirectParam(AfxGetInstanceHandle(), pDlgTmpl,
		m_hWnd, LogDlgProc, reinterpret_cast<LPARAM>(logText.c_str()));

	// If "Clear" was pressed, clear the log
	if (nResult == 1002)
		m_pCoordinator->ClearLog();
}

// --- Feature: Report Generation ---

/**
 * @brief Get file attributes string (RHSA) for a DIFFITEM on this pane.
 */
String CDirPaneView::GetItemAttributeString(const DIFFITEM& di) const
{
	if (!GetDocument() || !GetDocument()->HasDiffs())
		return _T("");

	const CDiffContext &ctxt = GetDiffContext();
	int side = m_nThisPane;
	if (!di.diffcode.exists(side))
		return _T("");

	String filePath = di.getFilepath(side, ctxt.GetPath(side));
	return CDirSideBySideCoordinator::GetFileAttributeString(filePath);
}

/**
 * @brief Generate an HTML report of the comparison results.
 */
void CDirPaneView::GenerateHTMLReport(const String& filePath)
{
	if (!m_pCoordinator || !GetDocument() || !GetDocument()->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	const auto& rowMapping = m_pCoordinator->GetRowMapping();
	int leftSide = 0;
	int rightSide = ctxt.GetCompareDirs() - 1;

	// Load color settings for HTML output
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
	f << _T(".newer { background: ") << colorToHex(colors.clrDirItemNewer) << _T("; color: ") << colorToHex(colors.clrDirItemNewerText) << _T("; }\n");
	f << _T(".older { background: ") << colorToHex(colors.clrDirItemOlder) << _T("; color: ") << colorToHex(colors.clrDirItemOlderText) << _T("; }\n");
	f << _T(".orphan { background: ") << colorToHex(colors.clrDirItemOrphan) << _T("; color: ") << colorToHex(colors.clrDirItemOrphanText) << _T("; }\n");
	f << _T(".filtered { background: ") << colorToHex(colors.clrDirItemFiltered) << _T("; color: ") << colorToHex(colors.clrDirItemFilteredText) << _T("; }\n");
	f << _T("</style>\n</head>\n<body>\n");
	f << _T("<h1>WinMerge Side-by-Side Folder Comparison Report</h1>\n");
	f << _T("<p><strong>Left:</strong> ") << ctxt.GetPath(leftSide) << _T("</p>\n");
	f << _T("<p><strong>Right:</strong> ") << ctxt.GetPath(rightSide) << _T("</p>\n");
	f << _T("<p><strong>Generated:</strong> ");

	SYSTEMTIME st;
	GetLocalTime(&st);
	f << strutils::format(_T("%04d-%02d-%02d %02d:%02d:%02d"),
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	f << _T("</p>\n");

	f << _T("<table>\n<tr><th>Filename</th><th>Left Status</th><th>Right Status</th>");
	f << _T("<th>Size Left</th><th>Size Right</th><th>Date Left</th><th>Date Right</th>");
	f << _T("<th>Attr Left</th><th>Attr Right</th></tr>\n");

	int toleranceSecs = GetOptionsMgr()->GetInt(OPT_CMP_IGNORE_SMALL_FILETIME_SECS);

	for (const auto& row : rowMapping)
	{
		if (!row.diffpos)
			continue;

		const DIFFITEM &di = ctxt.GetDiffAt(row.diffpos);
		if (di.isEmpty() || di.diffcode.isDirectory())
			continue;

		String cssClass;
		String leftStatus, rightStatus;

		if (di.diffcode.isResultFiltered())
		{
			cssClass = _T("filtered");
			leftStatus = _T("Filtered");
			rightStatus = _T("Filtered");
		}
		else if (!IsItemExistAll(ctxt, di))
		{
			cssClass = _T("orphan");
			if (di.diffcode.exists(leftSide))
			{
				leftStatus = _T("Unique");
				rightStatus = _T("-");
			}
			else
			{
				leftStatus = _T("-");
				rightStatus = _T("Unique");
			}
		}
		else if (di.diffcode.isResultSame())
		{
			cssClass = _T("identical");
			leftStatus = _T("Identical");
			rightStatus = _T("Identical");
		}
		else if (di.diffcode.isResultDiff())
		{
			Poco::Timestamp::TimeDiff diff = di.diffFileInfo[leftSide].mtime - di.diffFileInfo[rightSide].mtime;
			Poco::Timestamp::TimeDiff toleranceUs = static_cast<Poco::Timestamp::TimeDiff>(toleranceSecs) * Poco::Timestamp::resolution();

			if (diff > toleranceUs)
			{
				cssClass = _T("newer");
				leftStatus = _T("Newer");
				rightStatus = _T("Older");
			}
			else if (diff < -toleranceUs)
			{
				cssClass = _T("older");
				leftStatus = _T("Older");
				rightStatus = _T("Newer");
			}
			else
			{
				cssClass = _T("different");
				leftStatus = _T("Different");
				rightStatus = _T("Different");
			}
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
			strutils::format(_T("%lld"), di.diffFileInfo[leftSide].size) : _T("-");
		String sizeRight = di.diffcode.exists(rightSide) ?
			strutils::format(_T("%lld"), di.diffFileInfo[rightSide].size) : _T("-");

		auto formatTime = [](const DiffFileInfo &fi) -> String
		{
			if (fi.mtime == 0)
				return _T("-");
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
		};

		String dateLeft = di.diffcode.exists(leftSide) ? formatTime(di.diffFileInfo[leftSide]) : _T("-");
		String dateRight = di.diffcode.exists(rightSide) ? formatTime(di.diffFileInfo[rightSide]) : _T("-");

		String attrLeft = di.diffcode.exists(leftSide) ?
			CDirSideBySideCoordinator::GetFileAttributeString(di.getFilepath(leftSide, ctxt.GetPath(leftSide))) : _T("-");
		String attrRight = di.diffcode.exists(rightSide) ?
			CDirSideBySideCoordinator::GetFileAttributeString(di.getFilepath(rightSide, ctxt.GetPath(rightSide))) : _T("-");

		f << _T("<tr class=\"") << cssClass << _T("\">");
		f << _T("<td>") << filename << _T("</td>");
		f << _T("<td>") << leftStatus << _T("</td>");
		f << _T("<td>") << rightStatus << _T("</td>");
		f << _T("<td>") << sizeLeft << _T("</td>");
		f << _T("<td>") << sizeRight << _T("</td>");
		f << _T("<td>") << dateLeft << _T("</td>");
		f << _T("<td>") << dateRight << _T("</td>");
		f << _T("<td>") << attrLeft << _T("</td>");
		f << _T("<td>") << attrRight << _T("</td>");
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

/**
 * @brief Generate a CSV report of the comparison results.
 */
void CDirPaneView::GenerateCSVReport(const String& filePath)
{
	if (!m_pCoordinator || !GetDocument() || !GetDocument()->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	const auto& rowMapping = m_pCoordinator->GetRowMapping();
	int leftSide = 0;
	int rightSide = ctxt.GetCompareDirs() - 1;
	int toleranceSecs = GetOptionsMgr()->GetInt(OPT_CMP_IGNORE_SMALL_FILETIME_SECS);

	std::basic_ofstream<tchar_t> f(filePath.c_str());
	if (!f.is_open())
	{
		AfxMessageBox(_T("Failed to create report file."), MB_ICONERROR);
		return;
	}

	f << _T("Filename,Left Status,Right Status,Size Left,Size Right,Date Left,Date Right,Attr Left,Attr Right\n");

	for (const auto& row : rowMapping)
	{
		if (!row.diffpos)
			continue;

		const DIFFITEM &di = ctxt.GetDiffAt(row.diffpos);
		if (di.isEmpty() || di.diffcode.isDirectory())
			continue;

		String leftStatus, rightStatus;

		if (di.diffcode.isResultFiltered())
		{
			leftStatus = _T("Filtered");
			rightStatus = _T("Filtered");
		}
		else if (!IsItemExistAll(ctxt, di))
		{
			if (di.diffcode.exists(leftSide))
			{
				leftStatus = _T("Unique");
				rightStatus = _T("");
			}
			else
			{
				leftStatus = _T("");
				rightStatus = _T("Unique");
			}
		}
		else if (di.diffcode.isResultSame())
		{
			leftStatus = _T("Identical");
			rightStatus = _T("Identical");
		}
		else if (di.diffcode.isResultDiff())
		{
			Poco::Timestamp::TimeDiff diff = di.diffFileInfo[leftSide].mtime - di.diffFileInfo[rightSide].mtime;
			Poco::Timestamp::TimeDiff toleranceUs = static_cast<Poco::Timestamp::TimeDiff>(toleranceSecs) * Poco::Timestamp::resolution();

			if (diff > toleranceUs)
			{
				leftStatus = _T("Newer");
				rightStatus = _T("Older");
			}
			else if (diff < -toleranceUs)
			{
				leftStatus = _T("Older");
				rightStatus = _T("Newer");
			}
			else
			{
				leftStatus = _T("Different");
				rightStatus = _T("Different");
			}
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

		if (filename.find(_T(',')) != String::npos || filename.find(_T('"')) != String::npos)
		{
			String escaped;
			for (auto ch : filename)
			{
				if (ch == _T('"'))
					escaped += _T('"');
				escaped += ch;
			}
			filename = _T("\"") + escaped + _T("\"");
		}

		String sizeLeft = di.diffcode.exists(leftSide) ?
			strutils::format(_T("%lld"), di.diffFileInfo[leftSide].size) : _T("");
		String sizeRight = di.diffcode.exists(rightSide) ?
			strutils::format(_T("%lld"), di.diffFileInfo[rightSide].size) : _T("");

		auto formatTime = [](const DiffFileInfo &fi) -> String
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
		};

		String dateLeft = di.diffcode.exists(leftSide) ? formatTime(di.diffFileInfo[leftSide]) : _T("");
		String dateRight = di.diffcode.exists(rightSide) ? formatTime(di.diffFileInfo[rightSide]) : _T("");

		String attrLeft = di.diffcode.exists(leftSide) ?
			CDirSideBySideCoordinator::GetFileAttributeString(di.getFilepath(leftSide, ctxt.GetPath(leftSide))) : _T("");
		String attrRight = di.diffcode.exists(rightSide) ?
			CDirSideBySideCoordinator::GetFileAttributeString(di.getFilepath(rightSide, ctxt.GetPath(rightSide))) : _T("");

		f << filename << _T(",");
		f << leftStatus << _T(",") << rightStatus << _T(",");
		f << sizeLeft << _T(",") << sizeRight << _T(",");
		f << dateLeft << _T(",") << dateRight << _T(",");
		f << attrLeft << _T(",") << attrRight << _T("\n");
	}

	f.close();
}

/**
 * @brief Generate Report handler -- shows file dialog and creates HTML or CSV report.
 */
void CDirPaneView::OnSxsGenerateReport()
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

// --- Feature: Drag-Drop (drag OUT from pane) ---

/**
 * @brief Handle LVN_BEGINDRAG -- initiate OLE drag with HDROP data.
 * Allows dragging files from the pane to Explorer or the other pane.
 */
void CDirPaneView::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	if (!m_pCoordinator || !GetDocument() || !GetDocument()->HasDiffs())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int side = m_nThisPane;

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
		if (pDoc)
			pDoc->Rescan();
	}
}

// --- Navigation handlers ---

void CDirPaneView::OnSxsNavBack()
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

void CDirPaneView::OnSxsNavForward()
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

void CDirPaneView::OnUpdateSxsNavBack(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_pCoordinator && m_pCoordinator->CanNavigateBack());
}

void CDirPaneView::OnUpdateSxsNavForward(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_pCoordinator && m_pCoordinator->CanNavigateForward());
}

void CDirPaneView::OnSxsUpLevel()
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

void CDirPaneView::OnSxsSetBase()
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
	int side = m_nThisPane;
	if (!di.diffcode.exists(side))
		return;
	String subPath = di.getFilepath(side, ctxt.GetPath(side));
	m_pCoordinator->SetBaseFolder(side, subPath);
}

void CDirPaneView::OnSxsSetBaseOther()
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
	int otherSide = (m_nThisPane == 0) ? (ctxt.GetCompareDirs() - 1) : 0;
	if (!di.diffcode.exists(otherSide))
		return;
	String subPath = di.getFilepath(otherSide, ctxt.GetPath(otherSide));
	m_pCoordinator->SetBaseFolderOtherSide(otherSide, subPath);
}

// --- Find Next / Find Prev ---

bool CDirPaneView::FindFilename(const String& pattern, bool bForward, int startRow)
{
	int nCount = m_pList->GetItemCount();
	if (nCount == 0 || pattern.empty())
		return false;
	for (int i = 1; i <= nCount; ++i)
	{
		int idx = bForward ? (startRow + i) % nCount : (startRow - i + nCount) % nCount;
		DIFFITEM *di = GetItemKey(idx);
		if (!di || !m_pCoordinator->ItemExistsOnPane(idx, m_nThisPane))
			continue;
		const CDiffContext &ctxt = GetDiffContext();
		const DIFFITEM &item = ctxt.GetDiffAt(di);
		const String& name = (m_nThisPane == 0) ? item.diffFileInfo[0].filename : item.diffFileInfo[1].filename;
		if (PathMatchSpec(name.c_str(), pattern.c_str()))
		{
			m_pList->SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
			m_pList->SetItemState(idx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			m_pList->EnsureVisible(idx, FALSE);
			return true;
		}
	}
	return false;
}

void CDirPaneView::OnSxsFindNext()
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

void CDirPaneView::OnSxsFindPrev()
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

// --- Copy to Folder / Move to Folder ---

void CDirPaneView::OnSxsCopyToFolder()
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

	// Collect selected file paths
	std::vector<String> srcPaths;
	int sel = -1;
	while ((sel = m_pList->GetNextItem(sel, LVNI_SELECTED)) != -1)
	{
		DIFFITEM *key = GetItemKey(sel);
		if (!key || !m_pCoordinator->ItemExistsOnPane(sel, m_nThisPane))
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (!di.diffcode.exists(m_nThisPane))
			continue;
		String srcPath = di.getFilepath(m_nThisPane, ctxt.GetPath(m_nThisPane));
		srcPaths.push_back(srcPath);
	}
	if (srcPaths.empty())
		return;

	// Build double-null-terminated source string
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

void CDirPaneView::OnSxsMoveToFolder()
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

	// Collect selected file paths
	std::vector<String> srcPaths;
	int sel = -1;
	while ((sel = m_pList->GetNextItem(sel, LVNI_SELECTED)) != -1)
	{
		DIFFITEM *key = GetItemKey(sel);
		if (!key || !m_pCoordinator->ItemExistsOnPane(sel, m_nThisPane))
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (!di.diffcode.exists(m_nThisPane))
			continue;
		String srcPath = di.getFilepath(m_nThisPane, ctxt.GetPath(m_nThisPane));
		srcPaths.push_back(srcPath);
	}
	if (srcPaths.empty())
		return;

	// Build double-null-terminated source string
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

// --- New Folder ---

/**
 * @brief Dialog proc for the New Folder name input dialog.
 */
static INT_PTR CALLBACK NewFolderDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			SetWindowLongPtr(hDlg, DWLP_USER, lParam);
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

static DLGTEMPLATE* BuildNewFolderDlgTemplate(BYTE* buffer, size_t bufSize)
{
	memset(buffer, 0, bufSize);
	const int DLG_W = 260, DLG_H = 75;

	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)buffer;
	pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
	pDlg->cdit = 4;
	pDlg->x = 0; pDlg->y = 0;
	pDlg->cx = DLG_W; pDlg->cy = DLG_H;

	WORD* pw = (WORD*)(pDlg + 1);
	*pw++ = 0; // menu
	*pw++ = 0; // class
	const wchar_t dlgTitle[] = L"New Folder";
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
	const wchar_t label[] = L"Folder Name:";
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

void CDirPaneView::OnSxsNewFolder()
{
	if (!m_pCoordinator)
		return;

	tchar_t szInput[MAX_PATH] = {};

	BYTE dlgBuf[1024];
	DLGTEMPLATE* pDlgTmpl = BuildNewFolderDlgTemplate(dlgBuf, sizeof(dlgBuf));

	INT_PTR nResult = DialogBoxIndirectParam(AfxGetInstanceHandle(), pDlgTmpl,
		m_hWnd, NewFolderDlgProc, reinterpret_cast<LPARAM>(szInput));

	if (nResult != IDOK)
		return;

	String folderName = szInput;
	if (folderName.empty())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	String basePath = (m_nThisPane == 0) ? ctxt.GetLeftPath() : ctxt.GetRightPath();
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

// --- Delete Permanently ---

void CDirPaneView::OnSxsDeletePermanent()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	std::vector<DIFFITEM*> items;
	m_pCoordinator->GetSelectedItems(m_nThisPane, items);
	if (items.empty())
		return;

	String msg = strutils::format(_T("PERMANENTLY delete %d selected item(s) from this side?\nThis cannot be undone!"),
		static_cast<int>(items.size()));
	if (AfxMessageBox(msg.c_str(), MB_YESNO | MB_ICONWARNING) != IDYES)
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int side = m_nThisPane;

	ShellFileOperations fileOps;
	for (auto *key : items)
	{
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (!di.diffcode.exists(side))
			continue;
		String path = di.getFilepath(side, ctxt.GetPath(side));
		fileOps.AddSource(path);
	}

	fileOps.SetOperation(FO_DELETE, 0, GetSafeHwnd()); // No FOF_ALLOWUNDO
	if (fileOps.Run() && !fileOps.IsCanceled())
	{
		m_pCoordinator->LogOperation(strutils::format(_T("Permanently deleted %d item(s)"),
			static_cast<int>(items.size())));
		pDoc->Rescan();
	}
}

// --- Exchange ---

void CDirPaneView::OnSxsExchange()
{
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	std::vector<DIFFITEM*> items;
	m_pCoordinator->GetSelectedItems(m_nThisPane, items);
	if (items.empty())
		return;

	String msg = strutils::format(_T("Exchange %d selected item(s) between left and right sides?"),
		static_cast<int>(items.size()));
	if (AfxMessageBox(msg.c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;

	m_pCoordinator->ExchangeFiles(items);
	pDoc->Rescan();
}

// --- Change Attributes ---

/**
 * @brief Dialog proc for the Change Attributes dialog.
 * Checkboxes for R, H, S, A attributes.
 */
static INT_PTR CALLBACK ChangeAttrDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
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

static DLGTEMPLATE* BuildChangeAttrDlgTemplate(BYTE* buffer, size_t bufSize)
{
	memset(buffer, 0, bufSize);
	const int DLG_W = 200, DLG_H = 120;

	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)buffer;
	pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
	pDlg->cdit = 6; // 4 checkboxes + OK + Cancel
	pDlg->x = 0; pDlg->y = 0;
	pDlg->cx = DLG_W; pDlg->cy = DLG_H;

	WORD* pw = (WORD*)(pDlg + 1);
	*pw++ = 0; // menu
	*pw++ = 0; // class
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

	// OK button
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

	// Cancel button
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

void CDirPaneView::OnSxsChangeAttributes()
{
	if (!m_pCoordinator || !m_pList)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	// Get current attributes of first selected item
	int nItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
	if (nItem < 0)
		return;

	DIFFITEM *key = GetItemKey(nItem);
	if (!key)
		return;

	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);
	if (!di.diffcode.exists(m_nThisPane))
		return;

	String filePath = di.getFilepath(m_nThisPane, ctxt.GetPath(m_nThisPane));
	DWORD attrs = GetFileAttributes(filePath.c_str());
	if (attrs == INVALID_FILE_ATTRIBUTES)
		return;

	BYTE dlgBuf[2048];
	DLGTEMPLATE* pDlgTmpl = BuildChangeAttrDlgTemplate(dlgBuf, sizeof(dlgBuf));

	INT_PTR nResult = DialogBoxIndirectParam(AfxGetInstanceHandle(), pDlgTmpl,
		m_hWnd, ChangeAttrDlgProc, reinterpret_cast<LPARAM>(&attrs));

	if (nResult != IDOK)
		return;

	// Apply attributes to all selected items
	int nSuccess = 0, nFailed = 0;
	int sel = -1;
	while ((sel = m_pList->GetNextItem(sel, LVNI_SELECTED)) != -1)
	{
		DIFFITEM *selKey = GetItemKey(sel);
		if (!selKey)
			continue;
		const DIFFITEM &selDi = ctxt.GetDiffAt(selKey);
		if (!selDi.diffcode.exists(m_nThisPane))
			continue;
		String selPath = selDi.getFilepath(m_nThisPane, ctxt.GetPath(m_nThisPane));
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

// --- Touch Now / Touch Specific / Touch From Other ---

void CDirPaneView::OnSxsTouchNow()
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
		if (!di.diffcode.exists(m_nThisPane))
			continue;
		String filePath = di.getFilepath(m_nThisPane, ctxt.GetPath(m_nThisPane));
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

/**
 * @brief Dialog proc for the Touch Specific Time dialog.
 */
static INT_PTR CALLBACK TouchSpecificDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			SetWindowLongPtr(hDlg, DWLP_USER, lParam);
			// Set current date/time as default
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
				if (pBuf)
					::GetDlgItemTextW(hDlg, 1001, pBuf, 64);
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

static DLGTEMPLATE* BuildTouchSpecificDlgTemplate(BYTE* buffer, size_t bufSize)
{
	memset(buffer, 0, bufSize);
	const int DLG_W = 260, DLG_H = 75;

	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)buffer;
	pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
	pDlg->cdit = 4;
	pDlg->x = 0; pDlg->y = 0;
	pDlg->cx = DLG_W; pDlg->cy = DLG_H;

	WORD* pw = (WORD*)(pDlg + 1);
	*pw++ = 0; // menu
	*pw++ = 0; // class
	const wchar_t dlgTitle[] = L"Touch to Specific Time";
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
	const wchar_t label[] = L"Date/Time (YYYY-MM-DD HH:MM:SS):";
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

void CDirPaneView::OnSxsTouchSpecific()
{
	if (!m_pCoordinator || !m_pList)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	tchar_t szInput[64] = {};

	BYTE dlgBuf[1024];
	DLGTEMPLATE* pDlgTmpl = BuildTouchSpecificDlgTemplate(dlgBuf, sizeof(dlgBuf));

	INT_PTR nResult = DialogBoxIndirectParam(AfxGetInstanceHandle(), pDlgTmpl,
		m_hWnd, TouchSpecificDlgProc, reinterpret_cast<LPARAM>(szInput));

	if (nResult != IDOK)
		return;

	// Parse YYYY-MM-DD HH:MM:SS
	SYSTEMTIME st = {};
	if (_stscanf_s(szInput, _T("%hd-%hd-%hd %hd:%hd:%hd"),
		&st.wYear, &st.wMonth, &st.wDay,
		&st.wHour, &st.wMinute, &st.wSecond) < 6)
	{
		AfxMessageBox(_T("Invalid date/time format. Use YYYY-MM-DD HH:MM:SS"), MB_ICONERROR);
		return;
	}

	// Convert local SYSTEMTIME to FILETIME
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
		if (!key)
			continue;
		const DIFFITEM &di = ctxt.GetDiffAt(key);
		if (di.diffcode.isDirectory())
			continue;
		if (!di.diffcode.exists(m_nThisPane))
			continue;
		String filePath = di.getFilepath(m_nThisPane, ctxt.GetPath(m_nThisPane));
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

void CDirPaneView::OnSxsTouchFromOther()
{
	// Same as existing OnSxsTouchTimestamps but in reverse direction
	// Delegate to existing handler (which copies timestamps from this side to other side)
	// For "from other", we need to reverse the direction
	if (!m_pCoordinator)
		return;

	CDirDoc *pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	std::vector<DIFFITEM*> items;
	m_pCoordinator->GetSelectedItems(m_nThisPane, items);
	if (items.empty())
		return;

	const CDiffContext &ctxt = GetDiffContext();
	int dstSide = m_nThisPane;
	int srcSide = (m_nThisPane == 0) ? (ctxt.GetCompareDirs() - 1) : 0;

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

	m_pCoordinator->LogOperation(strutils::format(
		_T("Touch From Other: %d succeeded, %d failed"), nSuccess, nFailed));

	AfxMessageBox(strutils::format(
		_T("Touch From Other complete.\nSucceeded: %d\nFailed: %d"), nSuccess, nFailed).c_str(),
		MB_ICONINFORMATION);

	pDoc->Rescan();
}

// --- Advanced Filter ---

/**
 * @brief Dialog proc for the Advanced Filter dialog.
 */
static INT_PTR CALLBACK AdvFilterDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
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

static DLGTEMPLATE* BuildAdvFilterDlgTemplate(BYTE* buffer, size_t bufSize)
{
	memset(buffer, 0, bufSize);
	const int DLG_W = 300, DLG_H = 160;

	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)buffer;
	pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
	pDlg->cdit = 12; // 5 labels + 5 edits + OK + Cancel
	pDlg->x = 0; pDlg->y = 0;
	pDlg->cx = DLG_W; pDlg->cy = DLG_H;

	WORD* pw = (WORD*)(pDlg + 1);
	*pw++ = 0;
	*pw++ = 0;
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
		// Label
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

		// Edit
		pItem = (DLGITEMTEMPLATE*)pw;
		pItem->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
		pItem->x = 120; pItem->y = (short)f.y; pItem->cx = DLG_W - 130; pItem->cy = 14;
		pItem->id = f.editId;
		pw = (WORD*)(pItem + 1);
		*pw++ = 0xFFFF; *pw++ = 0x0081;
		*pw++ = 0;
		*pw++ = 0;
		pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);
	}

	// OK button
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

	// Cancel button
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

void CDirPaneView::OnSxsAdvancedFilter()
{
	if (!m_pCoordinator)
		return;

	CDirSideBySideCoordinator::AdvancedFilter filter = m_pCoordinator->GetAdvancedFilter();

	BYTE dlgBuf[4096];
	DLGTEMPLATE* pDlgTmpl = BuildAdvFilterDlgTemplate(dlgBuf, sizeof(dlgBuf));

	INT_PTR nResult = DialogBoxIndirectParam(AfxGetInstanceHandle(), pDlgTmpl,
		m_hWnd, AdvFilterDlgProc, reinterpret_cast<LPARAM>(&filter));

	if (nResult != IDOK)
		return;

	m_pCoordinator->SetAdvancedFilter(filter);
	m_pCoordinator->LogOperation(_T("Advanced filter updated"));
	m_pCoordinator->Redisplay();
}

// --- Ignore Structure / Row Stripes ---

void CDirPaneView::OnSxsIgnoreStructure()
{
	bool bCurrent = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_IGNORE_FOLDER_STRUCTURE);
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_IGNORE_FOLDER_STRUCTURE, !bCurrent);
	if (m_pCoordinator)
	{
		m_pCoordinator->SetIgnoreFolderStructure(!bCurrent);
		m_pCoordinator->Redisplay();
	}
}

void CDirPaneView::OnUpdateSxsIgnoreStructure(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_DIRVIEW_SXS_IGNORE_FOLDER_STRUCTURE));
}

void CDirPaneView::OnSxsRowStripes()
{
	m_bRowStripes = !m_bRowStripes;
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_ROW_STRIPES, m_bRowStripes);
	if (m_pList)
		m_pList->InvalidateRect(nullptr);

	// Sync the other pane too
	if (m_pCoordinator)
	{
		CDirPaneView *pOtherPane = (m_nThisPane == 0) ?
			m_pCoordinator->GetRightPaneView() : m_pCoordinator->GetLeftPaneView();
		if (pOtherPane)
		{
			pOtherPane->m_bRowStripes = m_bRowStripes;
			pOtherPane->GetListCtrl().InvalidateRect(nullptr);
		}
	}
}

void CDirPaneView::OnUpdateSxsRowStripes(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_bRowStripes);
}

// --- Exclude Pattern ---

void CDirPaneView::OnSxsExcludePattern()
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
	int side = m_nThisPane;
	if (!di.diffcode.exists(side))
		side = (side == 0) ? (ctxt.GetCompareDirs() - 1) : 0;
	if (!di.diffcode.exists(side))
		return;

	String filename = di.diffFileInfo[side].filename;
	// Extract extension
	String::size_type dotPos = filename.rfind(_T('.'));
	String pattern;
	if (dotPos != String::npos)
		pattern = _T("-*.") + filename.substr(dotPos + 1);
	else
		pattern = _T("-") + filename;

	// Append to the name filter
	String currentFilter = m_pCoordinator->GetNameFilter();
	if (!currentFilter.empty())
		currentFilter += _T(" ");
	currentFilter += pattern;
	m_pCoordinator->SetNameFilter(currentFilter);
	m_pCoordinator->Redisplay();

	m_pCoordinator->LogOperation(_T("Added exclude pattern: ") + pattern);
}

// --- Compare Info ---

void CDirPaneView::OnSxsCompareInfo()
{
	if (!m_pCoordinator)
		return;

	String info = m_pCoordinator->FormatCompareInfoString();
	AfxMessageBox(info.c_str(), MB_ICONINFORMATION);
}

// --- Copy Path / Copy Filename ---

void CDirPaneView::OnSxsCopyPath()
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
	if (!di.diffcode.exists(m_nThisPane))
		return;

	String fullPath = di.getFilepath(m_nThisPane, ctxt.GetPath(m_nThisPane));

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

void CDirPaneView::OnSxsCopyFilename()
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
	int side = m_nThisPane;
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

// --- Open With App / Open With... ---

void CDirPaneView::OnSxsOpenWithApp()
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
	if (!di.diffcode.exists(m_nThisPane))
		return;

	String filePath = di.getFilepath(m_nThisPane, ctxt.GetPath(m_nThisPane));
	ShellExecute(GetSafeHwnd(), _T("open"), filePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void CDirPaneView::OnSxsOpenWith()
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
	if (!di.diffcode.exists(m_nThisPane))
		return;

	String filePath = di.getFilepath(m_nThisPane, ctxt.GetPath(m_nThisPane));
	String param = _T("shell32.dll,OpenAs_RunDLL ") + filePath;
	ShellExecute(GetSafeHwnd(), _T("open"), _T("rundll32.exe"), param.c_str(), nullptr, SW_SHOWNORMAL);
}

// --- Explorer Context Menu ---

void CDirPaneView::ShowExplorerContextMenu(const String& filePath, CPoint pt)
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

void CDirPaneView::OnSxsExplorerMenu()
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
	if (!di.diffcode.exists(m_nThisPane))
		return;

	String filePath = di.getFilepath(m_nThisPane, ctxt.GetPath(m_nThisPane));

	CPoint pt;
	GetCursorPos(&pt);
	ShowExplorerContextMenu(filePath, pt);
}

// --- Select Left Only / Select Right Only ---

void CDirPaneView::OnSxsSelectLeftOnly()
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
		if (di.diffcode.isSideFirstOnly())
			m_pList->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
	}
}

void CDirPaneView::OnSxsSelectRightOnly()
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
		if (di.diffcode.isSideSecondOnly())
			m_pList->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
	}
}

// --- Auto-expand All / Auto-expand Diff ---

void CDirPaneView::OnSxsAutoExpandAll()
{
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_AUTO_EXPAND_MODE, 1);
	if (m_pCoordinator)
	{
		m_pCoordinator->ApplyAutoExpand();
		m_pCoordinator->Redisplay();
	}
}

void CDirPaneView::OnSxsAutoExpandDiff()
{
	GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SXS_AUTO_EXPAND_MODE, 2);
	if (m_pCoordinator)
	{
		m_pCoordinator->ApplyAutoExpand();
		m_pCoordinator->Redisplay();
	}
}

void CDirPaneView::OnUpdateSxsAutoExpandAll(CCmdUI* pCmdUI)
{
	pCmdUI->SetRadio(GetOptionsMgr()->GetInt(OPT_DIRVIEW_SXS_AUTO_EXPAND_MODE) == 1);
}

void CDirPaneView::OnUpdateSxsAutoExpandDiff(CCmdUI* pCmdUI)
{
	pCmdUI->SetRadio(GetOptionsMgr()->GetInt(OPT_DIRVIEW_SXS_AUTO_EXPAND_MODE) == 2);
}

// --- Align With ---

void CDirPaneView::OnSxsAlignWith()
{
	if (!m_pCoordinator || !m_pList)
		return;

	// Get selected item on this pane
	int nItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
	if (nItem < 0)
		return;

	DIFFITEM *thisKey = GetItemKey(nItem);
	if (!thisKey)
		return;

	// Get selected item on the other pane
	CDirPaneView *pOtherPane = (m_nThisPane == 0) ?
		m_pCoordinator->GetRightPaneView() : m_pCoordinator->GetLeftPaneView();
	if (!pOtherPane)
		return;

	CListCtrl &otherList = pOtherPane->GetListCtrl();
	int nOtherItem = otherList.GetNextItem(-1, LVNI_SELECTED);
	if (nOtherItem < 0)
	{
		AfxMessageBox(_T("Please select an item on the other pane to align with."), MB_ICONINFORMATION);
		return;
	}

	DIFFITEM *otherKey = pOtherPane->GetItemKey(nOtherItem);
	if (!otherKey)
		return;

	// Determine left/right items based on pane index
	DIFFITEM *leftItem = (m_nThisPane == 0) ? thisKey : otherKey;
	DIFFITEM *rightItem = (m_nThisPane == 0) ? otherKey : thisKey;

	m_pCoordinator->AddAlignmentOverride(leftItem, rightItem);
	m_pCoordinator->Redisplay();

	m_pCoordinator->LogOperation(_T("Added alignment override"));
}

// --- Customize Keys ---

/**
 * @brief Dialog proc for the Customize Keys dialog.
 * A simple list showing command names and current key bindings.
 */
static INT_PTR CALLBACK CustomizeKeysDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			SetWindowLongPtr(hDlg, DWLP_USER, lParam);
			HWND hList = GetDlgItem(hDlg, 1001);
			if (hList)
			{
				// Populate with command info
				auto *pBindings = reinterpret_cast<std::map<UINT, CDirPaneView::KeyBinding>*>(lParam);
				if (pBindings)
				{
					int idx = 0;
					for (auto &kv : *pBindings)
					{
						String desc = strutils::format(_T("Command %u: VK=%u Ctrl=%d Shift=%d Alt=%d"),
							kv.first, kv.second.vkKey,
							kv.second.bCtrl ? 1 : 0,
							kv.second.bShift ? 1 : 0,
							kv.second.bAlt ? 1 : 0);
						SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)desc.c_str());
						idx++;
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

static DLGTEMPLATE* BuildCustomizeKeysDlgTemplate(BYTE* buffer, size_t bufSize)
{
	memset(buffer, 0, bufSize);
	const int DLG_W = 350, DLG_H = 250;

	DLGTEMPLATE* pDlg = (DLGTEMPLATE*)buffer;
	pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
	pDlg->cdit = 2; // List + OK
	pDlg->x = 0; pDlg->y = 0;
	pDlg->cx = DLG_W; pDlg->cy = DLG_H;

	WORD* pw = (WORD*)(pDlg + 1);
	*pw++ = 0;
	*pw++ = 0;
	const wchar_t dlgTitle[] = L"Customize Key Bindings";
	memcpy(pw, dlgTitle, sizeof(dlgTitle));
	pw += _countof(dlgTitle);
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	// List box (id=1001)
	DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)pw;
	pItem->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOINTEGRALHEIGHT;
	pItem->x = 5; pItem->y = 5;
	pItem->cx = DLG_W - 10; pItem->cy = DLG_H - 35;
	pItem->id = 1001;
	pw = (WORD*)(pItem + 1);
	*pw++ = 0xFFFF; *pw++ = 0x0083; // Listbox class
	*pw++ = 0;
	*pw++ = 0;
	pw = (WORD*)(((ULONG_PTR)pw + 3) & ~3);

	// OK button
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

void CDirPaneView::OnSxsCustomizeKeys()
{
	BYTE dlgBuf[1024];
	DLGTEMPLATE* pDlgTmpl = BuildCustomizeKeysDlgTemplate(dlgBuf, sizeof(dlgBuf));

	DialogBoxIndirectParam(AfxGetInstanceHandle(), pDlgTmpl,
		m_hWnd, CustomizeKeysDlgProc, reinterpret_cast<LPARAM>(&m_keyBindings));
}

// --- Load / Save Key Bindings ---

/**
 * @brief Load key bindings from options.
 * Format: "cmdId:vk:ctrl:shift:alt;cmdId:vk:ctrl:shift:alt;..."
 */
void CDirPaneView::LoadKeyBindings()
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

		// Parse "cmdId:vk:ctrl:shift:alt"
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

/**
 * @brief Save key bindings to options.
 */
void CDirPaneView::SaveKeyBindings()
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

/**
 * @brief Navigate to a new folder path on this pane.
 */
void CDirPaneView::NavigateToPath(const String& sPath)
{
	CDirDoc* pDoc = GetDocument();
	if (!pDoc || !pDoc->HasDiffs())
		return;

	const CDiffContext& ctxt = pDoc->GetDiffContext();
	PathContext paths = ctxt.GetNormalizedPaths();
	if (m_nThisPane >= 0 && m_nThisPane < paths.GetSize())
		paths.SetPath(m_nThisPane, sPath);

	fileopenflags_t dwFlags[3] = {};
	GetMainFrame()->DoFileOrFolderOpen(&paths, dwFlags, nullptr, _T(""),
		ctxt.m_bRecursive, nullptr);
}
