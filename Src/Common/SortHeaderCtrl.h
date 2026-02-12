/**
 *  @file SortHeaderCtrl.h
 *
 *  @brief Declaration of CSortHeaderCtrl
 */ 
#pragma once

/////////////////////////////////////////////////////////////////////////////
// CSortHeaderCtrl

class CSortHeaderCtrl : public CHeaderCtrl
{
	DECLARE_DYNAMIC(CSortHeaderCtrl)

// Construction
public:
	CSortHeaderCtrl();

// Attributes
protected:
	int 	m_nSortCol;
	bool	m_bSortAsc;
	int		m_nExtraTopMargin; /**< Extra top margin for embedded controls above header */

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSortHeaderCtrl)
	//}}AFX_VIRTUAL

	virtual int 	SetSortImage(int nCol, bool bAsc);
	void SetExtraTopMargin(int nMargin);
	int GetExtraTopMargin() const { return m_nExtraTopMargin; }

// Implementation
public:
	virtual ~CSortHeaderCtrl();

// Generated message map functions
protected:
	//{{AFX_MSG(CSortHeaderCtrl)
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();
	//}}AFX_MSG
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;

	DECLARE_MESSAGE_MAP()

	bool m_bMouseTracking;
	int m_nTrackingPane;
};
