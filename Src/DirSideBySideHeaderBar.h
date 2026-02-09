/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirSideBySideHeaderBar.h
 *
 * @brief Declaration of CDirSideBySideHeaderBar class
 */
#pragma once

#include "FilepathEdit.h"
#include "EditorFilepathBar.h"
#include "DropHandler.h"
#include <functional>

/**
 * @brief Header bar for side-by-side folder comparison.
 *
 * Implements the IHeaderBar interface with two CFilepathEdit controls
 * (left and right) that sync their widths with the splitter columns.
 * Reuses the same dialog template as CEditorFilePathBar (IDD_EDITOR_HEADERBAR)
 * since both need two path edit controls.
 */
class CDirSideBySideHeaderBar : public CDialogBar, public IHeaderBar
{
public:
	CDirSideBySideHeaderBar();
	~CDirSideBySideHeaderBar();

	BOOL Create(CWnd* pParentWnd);
	virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz);
	virtual void DoPaint(CDC* pDC);

	enum { IDD = IDD_EDITOR_HEADERBAR };

	/** Resize to match splitter column widths */
	void Resize(int widths[]);

	// IHeaderBar implementation
	String GetCaption(int pane) const override;
	void SetCaption(int pane, const String& sCaption) override;
	String GetPath(int pane) const override;
	void SetPath(int pane, const String& sPath) override;
	int GetActive() const override;
	void SetActive(int pane, bool bActive) override;
	void SetPaneCount(int nPanes) override;
	void Resize() override;
	void SetOnSetFocusCallback(const std::function<void(int)> callbackfunc) override;
	void SetOnCaptionChangedCallback(const std::function<void(int, const String& sText)> callbackfunc) override;
	void SetOnFileSelectedCallback(const std::function<void(int, const String& sFilepath)> callbackfunc) override;
	void SetOnFolderSelectedCallback(const std::function<void(int, const String& sFolderpath)> callbackfunc) override;
	void EditActivePanePath() override;

protected:
	afx_msg BOOL OnToolTipNotify(UINT id, NMHDR * pTTTStruct, LRESULT * pResult);
	afx_msg void OnSetFocusEdit(UINT id);
	afx_msg void OnKillFocusEdit(UINT id);
	afx_msg void OnChangeEdit(UINT id);
	afx_msg void OnSelectEdit(UINT id);
	DECLARE_MESSAGE_MAP()

private:
	CFilepathEdit m_Edit[2]; /**< Left and right path edit controls */
	std::unordered_set<HWND> m_Tips;
	CFont m_font;
	int m_nPanes;
	std::function<void(int)> m_setFocusCallbackfunc;
	std::function<void(int, const String& sText)> m_captionChangedCallbackfunc;
	std::function<void(int, const String& sFilepath)> m_fileSelectedCallbackfunc;
	std::function<void(int, const String& sFolderpath)> m_folderSelectedCallbackfunc;
	DropHandler *m_pDropHandlers[2]; /**< Drop handlers for each pane edit */

	void SetToolTipsFirstTime(HWND hTip);
	void OnDropFiles(int pane, const std::vector<String>& files);
};

inline void CDirSideBySideHeaderBar::SetPaneCount(int nPanes)
{
	m_nPanes = nPanes;
}

inline void CDirSideBySideHeaderBar::SetOnSetFocusCallback(const std::function<void(int)> callbackfunc)
{
	m_setFocusCallbackfunc = callbackfunc;
}

inline void CDirSideBySideHeaderBar::SetOnCaptionChangedCallback(const std::function<void(int, const String& sText)> callbackfunc)
{
	m_captionChangedCallbackfunc = callbackfunc;
}

inline void CDirSideBySideHeaderBar::SetOnFileSelectedCallback(const std::function<void(int, const String& sFilepath)> callbackfunc)
{
	m_fileSelectedCallbackfunc = callbackfunc;
	for (int pane = 0; pane < m_nPanes; ++pane)
		m_Edit[pane].EnableFileSelection(true);
}

inline void CDirSideBySideHeaderBar::SetOnFolderSelectedCallback(const std::function<void(int, const String& sFolderpath)> callbackfunc)
{
	m_folderSelectedCallbackfunc = callbackfunc;
	for (int pane = 0; pane < m_nPanes; ++pane)
		m_Edit[pane].EnableFolderSelection(true);
}
