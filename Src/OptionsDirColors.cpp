/** 
 * @file  OptionsDirColors.cpp
 *
 * @brief Implementation for OptionsDirColors class.
 */
#include "pch.h"
#include "OptionsDirColors.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include <windows.h>


namespace Options { namespace DirColors {

void Init(COptionsMgr *pOptionsMgr)
{
	int defaultTextColor = static_cast<int>(GetSysColor(COLOR_WINDOWTEXT));
	pOptionsMgr->InitOption(OPT_DIRCLR_ITEM_EQUAL, (int)GetSysColor(COLOR_WINDOW));
	pOptionsMgr->InitOption(OPT_DIRCLR_ITEM_EQUAL_TEXT, defaultTextColor);
	pOptionsMgr->InitOption(OPT_DIRCLR_ITEM_DIFF, (int)CEColor(240,222,125));
	pOptionsMgr->InitOption(OPT_DIRCLR_ITEM_DIFF_TEXT, defaultTextColor);
	pOptionsMgr->InitOption(OPT_DIRCLR_ITEM_NOT_EXIST_ALL, (int)CEColor(221,221,221));
	pOptionsMgr->InitOption(OPT_DIRCLR_ITEM_NOT_EXIST_ALL_TEXT, defaultTextColor);
	pOptionsMgr->InitOption(OPT_DIRCLR_ITEM_FILTERED, (int)CEColor(250,245,215));
	pOptionsMgr->InitOption(OPT_DIRCLR_ITEM_FILTERED_TEXT, defaultTextColor);
	pOptionsMgr->InitOption(OPT_DIRCLR_MARGIN, (int)GetSysColor(COLOR_WINDOW));
	pOptionsMgr->InitOption(OPT_DIRCLR_USE_COLORS, true);
	// SxS mode colors
	pOptionsMgr->InitOption(OPT_DIRCLR_ITEM_NEWER, (int)CEColor(255,220,220));    // Light red bg
	pOptionsMgr->InitOption(OPT_DIRCLR_ITEM_NEWER_TEXT, (int)CEColor(192,0,0));    // Dark red text
	pOptionsMgr->InitOption(OPT_DIRCLR_ITEM_OLDER, (int)CEColor(240,240,240));    // Light gray bg
	pOptionsMgr->InitOption(OPT_DIRCLR_ITEM_OLDER_TEXT, (int)CEColor(128,128,128));// Gray text
	pOptionsMgr->InitOption(OPT_DIRCLR_ITEM_ORPHAN, (int)CEColor(230,210,240));   // Light purple bg
	pOptionsMgr->InitOption(OPT_DIRCLR_ITEM_ORPHAN_TEXT, (int)CEColor(128,0,128));  // Purple text
	pOptionsMgr->InitOption(OPT_DIRCLR_ITEM_SUPPRESSED, (int)CEColor(210,240,240));// Light teal bg
	pOptionsMgr->InitOption(OPT_DIRCLR_ITEM_SUPPRESSED_TEXT, (int)CEColor(0,128,128));// Teal text
}

void Load(const COptionsMgr *pOptionsMgr, DIRCOLORSETTINGS& colors)
{
	colors.clrDirItemEqual = pOptionsMgr->GetInt(OPT_DIRCLR_ITEM_EQUAL);
	colors.clrDirItemEqualText = pOptionsMgr->GetInt(OPT_DIRCLR_ITEM_EQUAL_TEXT);
	colors.clrDirItemDiff = pOptionsMgr->GetInt(OPT_DIRCLR_ITEM_DIFF);
	colors.clrDirItemDiffText = pOptionsMgr->GetInt(OPT_DIRCLR_ITEM_DIFF_TEXT);
	colors.clrDirItemNotExistAll = pOptionsMgr->GetInt(OPT_DIRCLR_ITEM_NOT_EXIST_ALL);
	colors.clrDirItemNotExistAllText = pOptionsMgr->GetInt(OPT_DIRCLR_ITEM_NOT_EXIST_ALL_TEXT);
	colors.clrDirItemFiltered = pOptionsMgr->GetInt(OPT_DIRCLR_ITEM_FILTERED);
	colors.clrDirItemFilteredText = pOptionsMgr->GetInt(OPT_DIRCLR_ITEM_FILTERED_TEXT);
	colors.clrDirMargin = pOptionsMgr->GetInt(OPT_DIRCLR_MARGIN);
	colors.clrDirItemNewer = pOptionsMgr->GetInt(OPT_DIRCLR_ITEM_NEWER);
	colors.clrDirItemNewerText = pOptionsMgr->GetInt(OPT_DIRCLR_ITEM_NEWER_TEXT);
	colors.clrDirItemOlder = pOptionsMgr->GetInt(OPT_DIRCLR_ITEM_OLDER);
	colors.clrDirItemOlderText = pOptionsMgr->GetInt(OPT_DIRCLR_ITEM_OLDER_TEXT);
	colors.clrDirItemOrphan = pOptionsMgr->GetInt(OPT_DIRCLR_ITEM_ORPHAN);
	colors.clrDirItemOrphanText = pOptionsMgr->GetInt(OPT_DIRCLR_ITEM_ORPHAN_TEXT);
	colors.clrDirItemSuppressed = pOptionsMgr->GetInt(OPT_DIRCLR_ITEM_SUPPRESSED);
	colors.clrDirItemSuppressedText = pOptionsMgr->GetInt(OPT_DIRCLR_ITEM_SUPPRESSED_TEXT);
}

void Save(COptionsMgr *pOptionsMgr, const DIRCOLORSETTINGS& colors)
{
	pOptionsMgr->SaveOption(OPT_DIRCLR_ITEM_EQUAL, (int)colors.clrDirItemEqual);
	pOptionsMgr->SaveOption(OPT_DIRCLR_ITEM_EQUAL_TEXT, (int)colors.clrDirItemEqualText);
	pOptionsMgr->SaveOption(OPT_DIRCLR_ITEM_DIFF, (int)colors.clrDirItemDiff);
	pOptionsMgr->SaveOption(OPT_DIRCLR_ITEM_DIFF_TEXT, (int)colors.clrDirItemDiffText);
	pOptionsMgr->SaveOption(OPT_DIRCLR_ITEM_NOT_EXIST_ALL, (int)colors.clrDirItemNotExistAll);
	pOptionsMgr->SaveOption(OPT_DIRCLR_ITEM_NOT_EXIST_ALL_TEXT, (int)colors.clrDirItemNotExistAllText);
	pOptionsMgr->SaveOption(OPT_DIRCLR_ITEM_FILTERED, (int)colors.clrDirItemFiltered);
	pOptionsMgr->SaveOption(OPT_DIRCLR_ITEM_FILTERED_TEXT, (int)colors.clrDirItemFilteredText);
	pOptionsMgr->SaveOption(OPT_DIRCLR_MARGIN, (int)colors.clrDirMargin);
	pOptionsMgr->SaveOption(OPT_DIRCLR_ITEM_NEWER, (int)colors.clrDirItemNewer);
	pOptionsMgr->SaveOption(OPT_DIRCLR_ITEM_NEWER_TEXT, (int)colors.clrDirItemNewerText);
	pOptionsMgr->SaveOption(OPT_DIRCLR_ITEM_OLDER, (int)colors.clrDirItemOlder);
	pOptionsMgr->SaveOption(OPT_DIRCLR_ITEM_OLDER_TEXT, (int)colors.clrDirItemOlderText);
	pOptionsMgr->SaveOption(OPT_DIRCLR_ITEM_ORPHAN, (int)colors.clrDirItemOrphan);
	pOptionsMgr->SaveOption(OPT_DIRCLR_ITEM_ORPHAN_TEXT, (int)colors.clrDirItemOrphanText);
	pOptionsMgr->SaveOption(OPT_DIRCLR_ITEM_SUPPRESSED, (int)colors.clrDirItemSuppressed);
	pOptionsMgr->SaveOption(OPT_DIRCLR_ITEM_SUPPRESSED_TEXT, (int)colors.clrDirItemSuppressedText);
}

}}
