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

#include "EditorFilepathBar.h"
#include "DropHandler.h"
#include <functional>
#include <vector>

/**
 * @brief Beyond-Compare-style path bar for side-by-side folder comparison.
 *
 * Per-pane layout: [ComboBox path + dropdown][Back][Browse][Up]
 * Implements the IHeaderBar interface for CDirDoc integration.
 * Uses IDD_EDITOR_HEADERBAR as the base CDialogBar template
 * (template controls are hidden; all visible controls are created
 * programmatically).
 */
class CDirSideBySideHeaderBar : public CDialogBar, public IHeaderBar
{
public:
	CDirSideBySideHeaderBar();
	~CDirSideBySideHeaderBar();

	BOOL Create(CWnd* pParentWnd);
	virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz);

	enum { IDD = IDD_EDITOR_HEADERBAR };

	/** Resize to match splitter column widths */
	void Resize(int widths[]);
	/** Resize with explicit X offsets for each pane */
	void Resize(int widths[], int offsets[]);

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

	/** Set callback for back navigation button */
	void SetOnBackCallback(const std::function<void(int)> callbackfunc);
	/** Set callback for browse-for-folder button */
	void SetOnBrowseCallback(const std::function<void(int)> callbackfunc);
	/** Set callback for up-level button */
	void SetOnUpLevelCallback(const std::function<void(int)> callbackfunc);

	/** Add a path to the history dropdown for a given pane */
	void AddPathToHistory(int pane, const String& sPath);

protected:
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS);
	afx_msg void OnComboSelChange(UINT id);
	afx_msg void OnBackLeft();
	afx_msg void OnBackRight();
	afx_msg void OnBrowseLeft();
	afx_msg void OnBrowseRight();
	afx_msg void OnUpLevelLeft();
	afx_msg void OnUpLevelRight();
	DECLARE_MESSAGE_MAP()

private:
	CComboBox m_comboPath[2];        /**< Path combo boxes (CBS_DROPDOWN) */
	CButton m_btnBack[2];            /**< Back navigation buttons */
	CButton m_btnBrowse[2];          /**< Browse-for-folder buttons */
	CButton m_btnUpLevel[2];         /**< Up-one-level buttons */
	CFont m_font;                    /**< Font for combo text */
	CFont m_btnFont;                 /**< Font for button symbols */
	CBrush m_brDarkBg;               /**< Dark background brush */
	CBrush m_brDarkEdit;             /**< Dark combo edit background brush */
	int m_nPanes;
	int m_nActivePane;               /**< Currently active pane (-1 if none) */
	std::vector<String> m_pathHistory[2]; /**< Path history per pane */
	std::function<void(int)> m_setFocusCallbackfunc;
	std::function<void(int, const String& sText)> m_captionChangedCallbackfunc;
	std::function<void(int, const String& sFilepath)> m_fileSelectedCallbackfunc;
	std::function<void(int, const String& sFolderpath)> m_folderSelectedCallbackfunc;
	std::function<void(int)> m_backCallbackfunc;
	std::function<void(int)> m_browseCallbackfunc;
	std::function<void(int)> m_upLevelCallbackfunc;
	DropHandler *m_pDropHandlers[2]; /**< Drop handlers for each combo */

	void OnDropFiles(int pane, const std::vector<String>& files);
	static void DrawIconButton(LPDRAWITEMSTRUCT lpDIS, int iconType);
};

inline void CDirSideBySideHeaderBar::SetPaneCount(int nPanes) { m_nPanes = nPanes; }
inline void CDirSideBySideHeaderBar::SetOnSetFocusCallback(const std::function<void(int)> cb) { m_setFocusCallbackfunc = cb; }
inline void CDirSideBySideHeaderBar::SetOnCaptionChangedCallback(const std::function<void(int, const String&)> cb) { m_captionChangedCallbackfunc = cb; }
inline void CDirSideBySideHeaderBar::SetOnFileSelectedCallback(const std::function<void(int, const String&)> cb) { m_fileSelectedCallbackfunc = cb; }
inline void CDirSideBySideHeaderBar::SetOnFolderSelectedCallback(const std::function<void(int, const String&)> cb) { m_folderSelectedCallbackfunc = cb; }
inline void CDirSideBySideHeaderBar::SetOnBackCallback(const std::function<void(int)> cb) { m_backCallbackfunc = cb; }
inline void CDirSideBySideHeaderBar::SetOnBrowseCallback(const std::function<void(int)> cb) { m_browseCallbackfunc = cb; }
inline void CDirSideBySideHeaderBar::SetOnUpLevelCallback(const std::function<void(int)> cb) { m_upLevelCallbackfunc = cb; }
