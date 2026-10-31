#ifndef AFX_PICTURESDIALOG_H__6D1C0A60_F616_43C6_9850_F3C0F052C0DB__INCLUDED_
#define AFX_PICTURESDIALOG_H__6D1C0A60_F616_43C6_9850_F3C0F052C0DB__INCLUDED_

#pragma once

#include "Firmware.h"
#include "PictureWnd.h"
#include "LayoutDialog.h"
#include "afxwin.h"
#include "ScanDialog.h"

class CLayoutDialog;

// CPicturesDialog dialog


// A push button that goes on clicking while it is held: after a short wait it sends a click to its dialog every 70 ms, until it is let
// go (or it is off, or the pointer has left it). A click that is only a click stays one.
class CRepeatButton : public CButton
{
public:
	CRepeatButton() : m_bRepeated(false) {}
protected:
	bool m_bRepeated;                         // the held button has sent clicks of its own: the release is not one more
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnKillFocus(CWnd *pNewWnd);
	DECLARE_MESSAGE_MAP()
};

class CPicturesDialog : public CDarkDialog
{
	DECLARE_DYNAMIC(CPicturesDialog)

public:
	CPicturesDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CPicturesDialog();

// Dialog Data
	enum { IDD = IDD_PICTURES_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedLoadBitmap();
	CPicture m_Picture;
	CPictureWnd m_PictureWnd;
	CLayoutDialog * m_pLayoutDialog;
	void UpdatePicture();
	void ShowUnreadablePicture();
	BOOL m_bHavePicture;                 // m_Picture is the picture that is on show (the last one read was good)
	CFirmware * m_pFirmware;
	DWORD m_PictureIndex;
	int m_PictureWidth;
	int m_PictureHeight;
	int m_PictureBitdepth;
	int m_PictureID;
	void SetFirmware(CFirmware *pFirmware, CLayoutDialog *pLayoutDialog, CWnd *pThemePreview);
	afx_msg void OnBnClickedSaveBitmap();
	int m_PictureType;
	CComboBox m_PicIndexCombo;
	afx_msg void OnCbnSelchangePicidxCombo();
	afx_msg void OnBnClickedPicPrev();
	afx_msg void OnBnClickedPicNext();
	CRepeatButton m_PicPrevBtn;
	CRepeatButton m_PicNextBtn;
	void StepPicture(int dir);               // the picture before / after the one that is shown
	void UpdateNavButtons();                 // "<" and ">" are off at the first and the last picture
	BOOL SavePicture(CPicture *pPicture, LPCTSTR lpszFilename);
	CWnd *m_pThemePreview;
protected:
	virtual void OnOK();
public:
	afx_msg LRESULT OnDropPictureFiles(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedSaveAll();
	afx_msg void OnBnClickedLoadAll();
	void LoadPictures(LPCTSTR folderPath);
	void SavePictures(LPCTSTR folderPath);
	BOOL LoadPicture(CPicture *pPicture, LPCTSTR lpszFilename);
public:
	afx_msg void OnBnClickedPasteImage();
public:
	afx_msg void OnBnClickedCopyImage();
	afx_msg void OnBnClickedFindBitmapRsrc();
	afx_msg void OnBnClickedThemePreview();
	afx_msg void OnCbnDropdownPicidxCombo();
	afx_msg void OnCbnCloseupPicidxCombo();
	int m_idxComboClosedH;
	afx_msg void OnBnClickedPictureZoomIn();
	afx_msg void OnBnClickedPictureZoomOut();
	void StepPictureZoom(int dir);
	// The page grows with the window: the picture takes the room that is added, the buttons on the right keep to the right edge.
	struct BaseCtl { HWND hwnd; CRect rc; };
	std::vector<BaseCtl> m_BaseCtls;     // where the controls are in the template (what the window grows from)
	CRect m_BaseClient;
	BOOL m_bBaseSet;
	afx_msg void OnSize(UINT nType, int cx, int cy);
};

#endif