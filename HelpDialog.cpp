// HelpDialog.cpp : implementation file
//

#include "stdafx.h"
#include "iPodWizard.h"
#include "HelpDialog.h"
#include ".\helpdialog.h"
#include <windows.h>
#include <tchar.h>
#include <string.h>


// CHelpDialog dialog

IMPLEMENT_DYNAMIC(CHelpDialog, CDarkDialog)
CHelpDialog::CHelpDialog(CWnd* pParent /*=NULL*/)
	: CDarkDialog(CHelpDialog::IDD, pParent)
{
}

CHelpDialog::~CHelpDialog()
{
}

void CHelpDialog::DoDataExchange(CDataExchange* pDX)
{
	CDarkDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CHelpDialog, CDarkDialog)
	ON_MESSAGE(WM_CLOSE, OnClose)
END_MESSAGE_MAP()


// CHelpDialog message handlers

BOOL CHelpDialog::OnInitDialog()
{
	CDarkDialog::OnInitDialog();

	// the three headings in bold
	LOGFONT lf;
	GetFont()->GetLogFont(&lf);
	lf.lfWeight = FW_BOLD;
	m_BoldFont.CreateFontIndirect(&lf);
	GetDlgItem(IDC_HELP_H1)->SetFont(&m_BoldFont);
	GetDlgItem(IDC_HELP_H2)->SetFont(&m_BoldFont);
	GetDlgItem(IDC_HELP_H3)->SetFont(&m_BoldFont);

	return TRUE;
}

LRESULT CHelpDialog::OnClose(WPARAM wParam, LPARAM lParam)
{
	this->EndDialog(IDOK);
	return 0;
}
