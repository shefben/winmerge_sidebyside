#pragma once

#include "cecolor.h"

/** 
 * @brief Dir color settings.
 */
struct DIRCOLORSETTINGS
{
	CEColor	clrDirItemEqual;			/**< Item equal background color */
	CEColor	clrDirItemEqualText;		/**< Item equal text color */
	CEColor	clrDirItemDiff;				/**< Item diff background color */
	CEColor	clrDirItemDiffText;			/**< Item diff text color */
	CEColor	clrDirItemNotExistAll;		/**< Item not-exist-all background color */
	CEColor	clrDirItemNotExistAllText;	/**< Item not-exist-all text color */
	CEColor	clrDirItemFiltered;			/**< Item filtered background color */
	CEColor	clrDirItemFilteredText;		/**< Item filtered text color */
	CEColor	clrDirMargin;				/**< Background color */
	CEColor	clrDirItemNewer;			/**< Item newer background color (SxS) */
	CEColor	clrDirItemNewerText;		/**< Item newer text color (SxS) */
	CEColor	clrDirItemOlder;			/**< Item older background color (SxS) */
	CEColor	clrDirItemOlderText;		/**< Item older text color (SxS) */
	CEColor	clrDirItemOrphan;			/**< Item orphan background color (SxS) */
	CEColor	clrDirItemOrphanText;		/**< Item orphan text color (SxS) */
	CEColor	clrDirItemSuppressed;		/**< Item suppressed-filter background color (SxS) */
	CEColor	clrDirItemSuppressedText;	/**< Item suppressed-filter text color (SxS) */
};

class COptionsMgr;

namespace Options { namespace DirColors {

void Init(COptionsMgr *pOptionsMgr);
void Load(const COptionsMgr *pOptionsMgr, DIRCOLORSETTINGS& colors);
void Save(COptionsMgr *pOptionsMgr, const DIRCOLORSETTINGS& colors);

}}
