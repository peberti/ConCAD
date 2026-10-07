/*
 TinyCAD program for schematic capture
 Copyright 1994/1995/2002-2005 Matt Pyne.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// DetailsPropertyPages.h : header file
//

#ifndef __DETAILSPROPERTYPAGES_H__
#define __DETAILSPROPERTYPAGES_H__

#include "Details.h"
#include <vector>

/////////////////////////////////////////////////////////////////////////////
// CDetailsPropertyPage1 dialog

class CDetailsPropertyPage1: public CPropertyPage
{
	DECLARE_DYNCREATE( CDetailsPropertyPage1)

private:
	CMultiSheetDoc* m_pDesign;

	// User-defined variables (tokens) editor, embedded on this page.  A fixed
	// set of Name/Value edit rows is virtualised over the full token list via
	// a scrollbar.  See DetailsPropertyPages.cpp for the row-commit logic.
	struct STokenRow { CString name; CString value; };
	std::vector<STokenRow> m_tokenRows;
	int   m_scrollTop;        // index of the first token shown in the visible rows
	bool  m_tokensDirty;
	CScrollBar m_tokenScroll;
	static const int kVisibleRows = 6;

	void LoadTokens();              // build m_tokenRows from the tokens currently referenced in the design
	void CollectReferencedTokenNames(std::vector<CString>& out);  // ordered, de-duped {name}s actually used
	void RefreshTokenView();        // fill the visible edit rows from m_tokenRows
	void CommitVisibleRows();       // read the visible edit rows back into m_tokenRows
	void UpdateTokenScrollBar();

	// Construction
public:
	CDetailsPropertyPage1(CMultiSheetDoc* pDesign = NULL);
	~CDetailsPropertyPage1();

	// Dialog Data
	//{{AFX_DATA(CDetailsPropertyPage1)
	enum
	{
		IDD = IDD_DETAILS_PAGE1
	};
	CString m_sAuthor;
	CString m_sDate;
	CString m_sOrg;
	CString m_sDescription;
	CString m_sDoc;
	CString m_sRevision;
	CString m_sSheets;
	CString m_sTitle;
	BOOL m_bIsVisible;
	//}}AFX_DATA


	// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CDetailsPropertyPage1)
public:
	virtual BOOL OnApply();
protected:
	virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support
	//}}AFX_VIRTUAL

	// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CDetailsPropertyPage1)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	DECLARE_MESSAGE_MAP()

};

/////////////////////////////////////////////////////////////////////////////
// CDetailsPropertyPage2 dialog

class CDetailsPropertyPage2: public CPropertyPage
{
	DECLARE_DYNCREATE( CDetailsPropertyPage2)

private:
	CMultiSheetDoc* m_pDesign;

	// Construction
public:
	CDetailsPropertyPage2(CMultiSheetDoc* pDesign = NULL);
	~CDetailsPropertyPage2();

	// Dialog Data
	//{{AFX_DATA(CDetailsPropertyPage2)
	enum
	{
		IDD = IDD_DETAILS_PAGE2
	};
	CEdit m_vert_enable;
	CEdit m_horiz_enable;
	BOOL m_bHasRulers;
	CString m_horiz;
	CString m_vert;
	//}}AFX_DATA


	// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CDetailsPropertyPage2)
public:
	virtual BOOL OnApply();
protected:
	virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support
	//}}AFX_VIRTUAL

	// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CDetailsPropertyPage2)
	virtual BOOL OnInitDialog();
	afx_msg void OnShowRulers();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

/////////////////////////////////////////////////////////////////////////////
// CDetailsPropertyPage3 - user-defined variables (tokens)

class CDetailsPropertyPage3: public CPropertyPage
{
	DECLARE_DYNCREATE(CDetailsPropertyPage3)

private:
	CMultiSheetDoc* m_pDesign;
	CDetailsTokenMap m_oTokens;
	bool m_bDirty;

	void RebuildList(int selectIndex = -1);
	bool GetSelectedToken(CString& sName) const;
	//-- Scan the current sheet's title-block fields and SVG template for
	//-- {Name} references and add any that are valid, not built-in and not
	//-- already defined to the token list (with an empty value), so a token
	//-- typed into a field or shipped in a template surfaces here ready to
	//-- be given a value.
	void MergeReferencedTokens();

public:
	CDetailsPropertyPage3(CMultiSheetDoc* pDesign = NULL);
	~CDetailsPropertyPage3();

	enum { IDD = IDD_DETAILS_PAGE3 };

	CListCtrl m_wndList;

	virtual BOOL OnApply();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();

	afx_msg void OnAdd();
	afx_msg void OnEdit();
	afx_msg void OnRemove();
	afx_msg void OnDoubleClickList(NMHDR* pNMHDR, LRESULT* pResult);
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CDetailsPropertyPage4 - SVG title-block picker

#include "SvgTitleBlock.h"

class CDetailsPropertyPage4: public CPropertyPage
{
	DECLARE_DYNCREATE(CDetailsPropertyPage4)

private:
	CMultiSheetDoc* m_pDesign;
	CString         m_sSvg;       // pending SVG content to apply (embedded copy)
	CString         m_sName;      // pending template name; empty for a Browse... file
	bool            m_bDirty;

	CListBox                            m_wndList;
	std::vector<STitleBlockTemplate>    m_templates;

	void UpdateStateLabel();
	void PopulateList();

public:
	CDetailsPropertyPage4(CMultiSheetDoc* pDesign = NULL);
	~CDetailsPropertyPage4();

	enum { IDD = IDD_DETAILS_PAGE4 };

	virtual BOOL OnApply();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();

	afx_msg void OnBrowse();
	afx_msg void OnUseBuiltin();
	afx_msg void OnListSelChange();
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CEditTokenDlg - modal dialog for adding/editing a single name/value pair

class CEditTokenDlg: public CDialog
{
public:
	CString m_sName;
	CString m_sValue;
	CString m_sOriginalName;  // empty when adding; set when editing
	const CDetailsTokenMap* m_pExistingTokens;

	CEditTokenDlg(CWnd* pParent = NULL);
	enum { IDD = IDD_EDIT_TOKEN };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual void OnOK();
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CPickTitleTemplateDlg - title-block picker shown by File -> New

class CPickTitleTemplateDlg: public CDialog
{
public:
	CString m_sName;      // in: template to preselect; out: chosen template (empty = built-in)
	BOOL    m_bDontAsk;

	CPickTitleTemplateDlg(CWnd* pParent = NULL);
	enum { IDD = IDD_PICK_TITLE_TEMPLATE };

protected:
	// List row 0 is the built-in title block; row i+1 is m_templates[i].
	CListBox                            m_wndList;
	std::vector<STitleBlockTemplate>    m_templates;

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnListDblClk();
	DECLARE_MESSAGE_MAP()
};

#endif // __DETAILSPROPERTYPAGES_H__
