// PicturesDialog.cpp : implementation file
//

#include "stdafx.h"
#include "iPodWizard.h"
#include "PicturesDialog.h"

// CPicturesDialog dialog

IMPLEMENT_DYNAMIC(CPicturesDialog, CDarkDialog)
CPicturesDialog::CPicturesDialog(CWnd* pParent /*=NULL*/)
	: CDarkDialog(CPicturesDialog::IDD, pParent)
	, m_PictureWidth(0)
	, m_PictureHeight(0)
	, m_PictureBitdepth(0)
	, m_PictureIndex(0)
	, m_PictureType(0)
	, m_PictureID(0)
	, m_pFirmware(NULL)
	, m_bBaseSet(FALSE)
	, m_bHavePicture(FALSE)
{
}

CPicturesDialog::~CPicturesDialog()
{
}

void CPicturesDialog::DoDataExchange(CDataExchange* pDX)
{
	CDarkDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_WIDTH_EDIT, m_PictureWidth);
	DDX_Text(pDX, IDC_HEIGHT_EDIT, m_PictureHeight);
	DDX_Text(pDX, IDC_BITDEPTH_EDIT, m_PictureBitdepth);
	DDX_Text(pDX, IDC_TYPE_EDIT, m_PictureType);
	DDX_Text(pDX, IDC_ID_EDIT, m_PictureID);
	DDX_Control(pDX, IDC_PICTURE, m_PictureWnd);
	DDX_Control(pDX, IDC_PICIDX_COMBO, m_PicIndexCombo);
	DDX_Control(pDX, IDC_PIC_PREV, m_PicPrevBtn);
	DDX_Control(pDX, IDC_PIC_NEXT, m_PicNextBtn);
}


BEGIN_MESSAGE_MAP(CPicturesDialog, CDarkDialog)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_PICTURE_ZOOMIN, OnBnClickedPictureZoomIn)
	ON_BN_CLICKED(IDC_PICTURE_ZOOMOUT, OnBnClickedPictureZoomOut)
	ON_BN_CLICKED(ID_LOAD_BITMAP, OnBnClickedLoadBitmap)
	ON_BN_CLICKED(ID_SAVE_BITMAP, OnBnClickedSaveBitmap)
	ON_CBN_SELCHANGE(IDC_PICIDX_COMBO, OnCbnSelchangePicidxCombo)
	ON_BN_CLICKED(IDC_PIC_PREV, OnBnClickedPicPrev)
	ON_BN_CLICKED(IDC_PIC_NEXT, OnBnClickedPicNext)
	ON_BN_CLICKED(ID_SAVE_ALL, OnBnClickedSaveAll)
	ON_BN_CLICKED(ID_LOAD_ALL, OnBnClickedLoadAll)
	ON_BN_CLICKED(ID_PASTE_IMAGE, &CPicturesDialog::OnBnClickedPasteImage)
	ON_BN_CLICKED(ID_COPY_IMAGE, &CPicturesDialog::OnBnClickedCopyImage)
	ON_MESSAGE(WM_APP, OnDropPictureFiles)
	ON_BN_CLICKED(ID_FIND_BITMAP_RSRC, &CPicturesDialog::OnBnClickedFindBitmapRsrc)
	ON_BN_CLICKED(IDC_THEMEPREVIEW_BUTTON, OnBnClickedThemePreview)
	ON_CBN_DROPDOWN(IDC_PICIDX_COMBO, OnCbnDropdownPicidxCombo)
	ON_CBN_CLOSEUP(IDC_PICIDX_COMBO, OnCbnCloseupPicidxCombo)
END_MESSAGE_MAP()


// CPicturesDialog message handlers

BOOL CPicturesDialog::OnInitDialog()
{
	CDarkDialog::OnInitDialog();

	m_PictureWnd.OnCreate(NULL);

	int selH = (int)m_PicIndexCombo.SendMessage(CB_GETITEMHEIGHT, (WPARAM)-1, 0);
	m_idxComboClosedH = selH + 8;
	CRect rc; m_PicIndexCombo.GetWindowRect(&rc); ScreenToClient(&rc);
	m_PicIndexCombo.MoveWindow(rc.left, rc.top, rc.Width(), m_idxComboClosedH);

	GetClientRect(&m_BaseClient);
	m_BaseCtls.clear();
	for (CWnd *w = GetWindow(GW_CHILD); w; w = w->GetWindow(GW_HWNDNEXT))
	{
		BaseCtl b;
		b.hwnd = w->m_hWnd;
		w->GetWindowRect(&b.rc);
		ScreenToClient(&b.rc);
		m_BaseCtls.push_back(b);
	}
	m_bBaseSet = TRUE;

	return TRUE;
}

// the picture takes all the room that is added, the buttons on the right keep to the right edge
void CPicturesDialog::OnSize(UINT nType, int cx, int cy)
{
	CDarkDialog::OnSize(nType, cx, cy);
	if (!m_bBaseSet || nType == SIZE_MINIMIZED || cx <= 0 || cy <= 0)
		return;
	CRect marks(374, 0, 0, 0);              // left of the buttons on the right
	MapDialogRect(&marks);
	int xBar = marks.left;
	HWND hPic = ::GetDlgItem(m_hWnd, IDC_PICTURE);
	CRect pic(0, 0, 0, 0);
	for (size_t i = 0; i < m_BaseCtls.size(); i++)
		if (m_BaseCtls[i].hwnd == hPic)
			pic = m_BaseCtls[i].rc;
	int dx = cx - m_BaseClient.Width(), dy = cy - m_BaseClient.Height();
	dx = max(dx, 100 - pic.Width());        // the picture never gets smaller than what it needs to work
	dy = max(dy, 60 - pic.Height());
	for (size_t i = 0; i < m_BaseCtls.size(); i++)
	{
		if (!::IsWindow(m_BaseCtls[i].hwnd))
			continue;
		CRect r = m_BaseCtls[i].rc;
		if (m_BaseCtls[i].hwnd == hPic)
		{
			r.right += dx;
			r.bottom += dy;
		}
		else if (r.left >= xBar && r.top >= pic.top)
			r.OffsetRect(dx, 0);
		else
			continue;
		::SetWindowPos(m_BaseCtls[i].hwnd, NULL, r.left, r.top, r.Width(), r.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
	}
	Invalidate();
}

// zoom of the picture, the same steps as the zoom of the font preview (up to 20 times)
void CPicturesDialog::StepPictureZoom(int dir)
{
	m_PictureWnd.StepZoom(dir);
	CString t;
	t.Format(_T("x%d"), m_PictureWnd.GetZoom());
	SetDlgItemText(IDC_PICTURE_ZOOMTXT, t);
}

void CPicturesDialog::OnBnClickedPictureZoomIn()
{
	StepPictureZoom(+1);
}

void CPicturesDialog::OnBnClickedPictureZoomOut()
{
	StepPictureZoom(-1);
}

void CPicturesDialog::OnCbnDropdownPicidxCombo()
{
	CWnd* pMain = AfxGetMainWnd();
	if (!pMain) return;
	CRect rcMain, rcCombo;
	pMain->GetClientRect(&rcMain);
	m_PicIndexCombo.GetWindowRect(&rcCombo);
	pMain->ScreenToClient(&rcCombo);
	int dropH = rcMain.bottom - rcCombo.top - 8;
	if (dropH < 50) dropH = 50;
	CRect rc; m_PicIndexCombo.GetWindowRect(&rc); ScreenToClient(&rc);
	m_PicIndexCombo.MoveWindow(rc.left, rc.top, rc.Width(), dropH);
}

void CPicturesDialog::OnCbnCloseupPicidxCombo()
{
	CRect rc; m_PicIndexCombo.GetWindowRect(&rc); ScreenToClient(&rc);
	m_PicIndexCombo.MoveWindow(rc.left, rc.top, rc.Width(), m_idxComboClosedH);
}

void CPicturesDialog::OnBnClickedThemePreview()
{
	//CThemePreviewDialog dlg;
	
	//dlg.SetFirmware(m_pFirmware, m_pEditorDialog);
	//dlg.DoModal();
	m_pThemePreview->SendMessage(BM_CLICK, 0, 0);
}

LRESULT CPicturesDialog::OnDropPictureFiles(WPARAM wParam, LPARAM lParam)
{
	if (wParam == NULL)
		return 0;
	if (m_pFirmware==NULL)
		return 0;
	int mg=(int)wParam;
	
	if (mg==10)
	{
		LoadPictures((TCHAR *)lParam);
	}
	else if (mg==11)
	{
		if (m_bHavePicture)                      // (a picture that could not be read is not on show: m_Picture is the one before)
			LoadPicture(&m_Picture, (TCHAR *)lParam);
	}
	return 0;
}

void CPicturesDialog::OnBnClickedLoadBitmap()
{
	CFileDialog dlg(TRUE, 0, 0, OFN_HIDEREADONLY, TEXT("Bitmap Files (*.bmp, *.gif, *.jpg)|*.bmp;*.gif;*.jpg||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	if (!LoadPicture(&m_Picture, dlg.GetPathName()))
	{
		MessageBox(TEXT("Unable to load image!"));
		return;
	}

	UpdatePicture();
}

void CPicturesDialog::UpdatePicture()
{
	BOOL bEnable;
	LPBYTE lpBuffer = m_pFirmware->GetPicture(m_PictureIndex);
	if (lpBuffer == NULL)
		return;

	DWORD off_set=lpBuffer-m_pFirmware->GetFirmwareBuffer();

	// The picture is read into one of its own first. A read that fails leaves a picture half read, and the window draws m_Picture (the one
	// on show): that must not be touched unless the new one is good.
	CPicture pic;
	if (m_pFirmware->GetPictureType(m_PictureIndex) != 0)
		pic.SetPictureMode(1);
	else
		pic.SetPictureMode(0);

	// check if partition can be written
	bEnable = m_pFirmware->CanWritePicture(m_PictureIndex);

	if (!pic.Read(lpBuffer, FALSE, m_pFirmware->GetAUPDBuffer()))
	{
		ShowUnreadablePicture();
		return;
	}
	m_Picture = pic;
	m_bHavePicture = TRUE;

	CSize picSize = m_Picture.GetPictureSize();
	m_PictureID = m_pFirmware->GetPictureID(m_PictureIndex);
	m_PictureWidth = picSize.cx;
	m_PictureHeight = picSize.cy;
	m_PictureBitdepth = m_Picture.GetPictureBitDepth();

	if (m_Picture.GetPictureMode()==1)
		m_PictureType = 2;
	else
		m_PictureType = 1;

	UpdateData(FALSE);

	m_PictureWnd.SetPicture(&m_Picture);

	m_PicIndexCombo.SetCurSel(m_PictureIndex);

	GetDlgItem(ID_LOAD_BITMAP)->EnableWindow(bEnable);
	GetDlgItem(ID_SAVE_BITMAP)->EnableWindow(TRUE);
	GetDlgItem(ID_SAVE_ALL)->EnableWindow(TRUE);
	GetDlgItem(ID_LOAD_ALL)->EnableWindow(TRUE);
	GetDlgItem(ID_COPY_IMAGE)->EnableWindow(TRUE);
	GetDlgItem(ID_PASTE_IMAGE)->EnableWindow(TRUE);
	GetDlgItem(ID_FIND_BITMAP_RSRC)->EnableWindow(TRUE);
	GetDlgItem(IDC_THEMEPREVIEW_BUTTON)->EnableWindow(TRUE);
	UpdateNavButtons();
}

// A picture of the firmware that cannot be read (damaged, or in an image that is not in the form the program reads): nothing is shown for it
// and what needs a picture is switched off, instead of the picture that was on show before standing for it.
void CPicturesDialog::ShowUnreadablePicture()
{
	m_bHavePicture = FALSE;
	m_PictureWnd.SetPicture(NULL);
	m_PictureID = m_pFirmware->GetPictureID(m_PictureIndex);
	m_PictureWidth = 0;
	m_PictureHeight = 0;
	m_PictureBitdepth = 0;
	m_PictureType = m_pFirmware->GetPictureType(m_PictureIndex) != 0 ? 2 : 1;
	UpdateData(FALSE);
	m_PicIndexCombo.SetCurSel(m_PictureIndex);

	GetDlgItem(ID_LOAD_BITMAP)->EnableWindow(FALSE);
	GetDlgItem(ID_SAVE_BITMAP)->EnableWindow(FALSE);
	GetDlgItem(ID_COPY_IMAGE)->EnableWindow(FALSE);
	GetDlgItem(ID_PASTE_IMAGE)->EnableWindow(FALSE);
	UpdateNavButtons();
}

void CPicturesDialog::SetFirmware(CFirmware *pFirmware, CLayoutDialog *pLayoutDialog, CWnd *pThemePreview)
{
	m_pFirmware = pFirmware;

	m_pLayoutDialog = pLayoutDialog;

	m_pThemePreview = pThemePreview;

	m_PictureIndex = 0;

	m_PicIndexCombo.ResetContent();

	CString s;
	for (DWORD i = 0; i < m_pFirmware->GetNumPictures(); i++)
	{
		s.Format(TEXT("%d"), i);
		m_PicIndexCombo.InsertString(i, s);
	}
	m_PicIndexCombo.SetCurSel(0);

	UpdatePicture();
	UpdateNavButtons();
}

void CPicturesDialog::OnBnClickedSaveBitmap()
{
	CString filename;
	filename.Format(TEXT("pic%05d.bmp"), m_pFirmware->GetPictureID(m_PictureIndex));

	CFileDialog dlg(FALSE, TEXT("bmp"), filename, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, TEXT("BMP Files (*.bmp)|*.bmp||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	if (!SavePicture(&m_Picture, dlg.GetPathName()))
	{
		MessageBox(TEXT("Unable to save image!"));
	}
}

void CPicturesDialog::OnCbnSelchangePicidxCombo()
{
	m_PictureIndex = m_PicIndexCombo.GetCurSel();

	UpdatePicture();
	UpdateNavButtons();
}

void CPicturesDialog::StepPicture(int dir)
{
	if (m_pFirmware == NULL)
		return;
	int idx = m_PicIndexCombo.GetCurSel() + dir;
	if (idx < 0 || idx >= m_PicIndexCombo.GetCount())
		return;
	m_PicIndexCombo.SetCurSel(idx);
	m_PictureIndex = idx;
	UpdatePicture();
	UpdateNavButtons();
}

void CPicturesDialog::OnBnClickedPicPrev()
{
	StepPicture(-1);
}

void CPicturesDialog::OnBnClickedPicNext()
{
	StepPicture(1);
}

// ---- the buttons "<" and ">": a held button goes on

BEGIN_MESSAGE_MAP(CRepeatButton, CButton)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_TIMER()
	ON_WM_KILLFOCUS()
END_MESSAGE_MAP()

static const UINT_PTR kRepeatTimer = 1;

void CRepeatButton::OnLButtonDown(UINT nFlags, CPoint point)
{
	m_bRepeated = false;
	CButton::OnLButtonDown(nFlags, point);
	SetTimer(kRepeatTimer, 400, NULL);          // the wait before the steps go on
}

void CRepeatButton::OnLButtonUp(UINT nFlags, CPoint point)
{
	KillTimer(kRepeatTimer);
	if (m_bRepeated)
	{
		m_bRepeated = false;
		ReleaseCapture();
		SetState(FALSE);                        // the clicks were sent while it was held: the release is not a click
		Invalidate();
		return;
	}
	CButton::OnLButtonUp(nFlags, point);
}

void CRepeatButton::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent != kRepeatTimer)
	{
		CButton::OnTimer(nIDEvent);
		return;
	}
	if (!IsWindowEnabled() || !(GetState() & BST_PUSHED))
	{
		KillTimer(kRepeatTimer);                // let go, the pointer left the button, or the end of the list: it is off
		return;
	}
	m_bRepeated = true;
	SetTimer(kRepeatTimer, 70, NULL);           // the quick steps
	CWnd *pParent = GetParent();
	if (pParent != NULL)
		pParent->SendMessage(WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), BN_CLICKED), (LPARAM)m_hWnd);
}

void CRepeatButton::OnKillFocus(CWnd *pNewWnd)
{
	KillTimer(kRepeatTimer);
	CButton::OnKillFocus(pNewWnd);
}

void CPicturesDialog::UpdateNavButtons()
{
	int n = m_PicIndexCombo.GetCount();
	int cur = m_PicIndexCombo.GetCurSel();
	BOOL bHave = m_pFirmware != NULL && n > 0 && cur >= 0;
	GetDlgItem(IDC_PIC_PREV)->EnableWindow(bHave && cur > 0);
	GetDlgItem(IDC_PIC_NEXT)->EnableWindow(bHave && cur < n - 1);
}

void CPicturesDialog::OnOK()
{
	// TODO: Add your specialized code here and/or call the base class

	// CDarkDialog::OnOK();
}

BOOL CPicturesDialog::SavePicture(CPicture *pPicture, LPCTSTR lpszFilename)
{
	if (pPicture == NULL || lpszFilename == NULL)
		return FALSE;

	CImage img;
	CSize size = pPicture->GetPictureSize();

	if (!img.Create(size.cx, size.cy, 24))
	{
		return FALSE;
	}

	int x, y;

	for (x = 0; x < size.cx; x++)
	{
		for (y = 0; y < size.cy; y++)
		{
			img.SetPixel(x, y, pPicture->GetPixel(x, y));
		}
	}

	if (FAILED(img.Save(lpszFilename)))
	{
		return FALSE;
	}

	return TRUE;
}

void CPicturesDialog::OnBnClickedSaveAll()
{
	CWaitCursor wait;
	LPITEMIDLIST il;
	TCHAR folderPath[MAX_PATH];

	BROWSEINFO br;
    br.hwndOwner = AfxGetMainWnd()->GetSafeHwnd();
    br.pidlRoot = NULL;
    br.pszDisplayName = folderPath;
    br.lpszTitle = TEXT("Select folder for files");
    br.ulFlags = BIF_RETURNONLYFSDIRS;
    br.lpfn = NULL;
    br.lParam = 0;
    br.iImage = 0;

	il = SHBrowseForFolder(&br);
	if (il == NULL)
		return;

	SHGetPathFromIDList(il, folderPath);

	SavePictures(folderPath);
}

void CPicturesDialog::SavePictures(LPCTSTR folderPath)
{
	CString filename;

	if (theApp.m_OldPicIndex==TRUE)
	{
		for (DWORD i = 0; i < m_pFirmware->GetNumPictures(); i++)
		{
			filename.Format(TEXT("%s\\pic%03d.bmp"), folderPath, i);
			CPicture pic;
			if (m_pFirmware->GetPictureType(m_PictureIndex) != 0)
				pic.SetPictureMode(1);
			else
				pic.SetPictureMode(0);
			if (pic.Read(m_pFirmware->GetPicture(i), FALSE, m_pFirmware->GetAUPDBuffer()))
			{
				if (!SavePicture(&pic, filename))
				{
					MessageBox(TEXT("Unable to save pictures!"));
					return;
				}
			}
		}
	}
	else
	{
		for (DWORD i = 0; i < m_pFirmware->GetNumPictures(); i++)
		{
			filename.Format(TEXT("%s\\pic%05d.bmp"), folderPath, m_pFirmware->GetPictureID(i));
			CPicture pic;
			if (m_pFirmware->GetPictureType(m_PictureIndex) != 0)
				pic.SetPictureMode(1);
			else
				pic.SetPictureMode(0);
			if (pic.Read(m_pFirmware->GetPicture(i), FALSE, m_pFirmware->GetAUPDBuffer()))
			{
				if (!SavePicture(&pic, filename))
				{
					MessageBox(TEXT("Unable to save pictures!"));
					return;
				}
			}
		}
	}
}

void CPicturesDialog::OnBnClickedLoadAll()
{
	CWaitCursor wait;
	LPITEMIDLIST il;
	TCHAR folderPath[MAX_PATH];

	BROWSEINFO br;
    br.hwndOwner = AfxGetMainWnd()->GetSafeHwnd();
    br.pidlRoot = NULL;
    br.pszDisplayName = folderPath;
    br.lpszTitle = TEXT("Select folder for files");
    br.ulFlags = BIF_RETURNONLYFSDIRS;
    br.lpfn = NULL;
    br.lParam = 0;
    br.iImage = 0;

	il = SHBrowseForFolder(&br);
	if (il == NULL)
		return;

	SHGetPathFromIDList(il, folderPath);

	CScanDialog dlg;
	dlg.Create(dlg.IDD);
	dlg.SetWindowText(_T("Loading pictures into firmware..."));
	dlg.ShowWindow(SW_SHOW);

	LoadPictures(folderPath);

	dlg.DestroyWindow();
}

void CPicturesDialog::LoadPictures(LPCTSTR folderPath)
{
	DWORD index;
	CString filename = folderPath;
	
	if (theApp.m_OldPicIndex==FALSE)
	{
		filename += "\\pic?????.bmp";

		WIN32_FIND_DATA findData;
		HANDLE hFind = FindFirstFile(filename, &findData);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				filename = folderPath;
				filename += "\\";
				filename += findData.cFileName;
				index = m_pFirmware->GetPictureIndexFromID(_wtoi(findData.cFileName + 3));

				if (index!=0xFFFFFFFF)
				{
					CPicture pic;
					LPBYTE lpBuffer = m_pFirmware->GetPicture(index);
					if (lpBuffer != NULL)
					{
						if (m_pFirmware->GetPictureType(index) != 0)
							pic.SetPictureMode(1);
						else
							pic.SetPictureMode(0);

						if (pic.Read(lpBuffer, FALSE, m_pFirmware->GetAUPDBuffer()) && m_pFirmware->CanWritePicture(index)==TRUE)
							LoadPicture(&pic, filename);
					}
				}
			} while (FindNextFile(hFind, &findData) != 0);

			FindClose(hFind);

			UpdatePicture();
		}
	}
	else
	{
		filename += "\\pic???.bmp";

		WIN32_FIND_DATA findData;
		HANDLE hFind = FindFirstFile(filename, &findData);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				filename = folderPath;
				filename += "\\";
				filename += findData.cFileName;
				index = _wtoi(findData.cFileName + 3);

				CPicture pic;
				LPBYTE lpBuffer = m_pFirmware->GetPicture(index);
				if (lpBuffer != NULL)
				{
					if (m_pFirmware->GetPictureType(index) != 0)
						pic.SetPictureMode(1);
					else
						pic.SetPictureMode(0);

					if (pic.Read(lpBuffer, FALSE, m_pFirmware->GetAUPDBuffer()) && m_pFirmware->CanWritePicture(index)==TRUE)
						LoadPicture(&pic, filename);
				}
			} while (FindNextFile(hFind, &findData) != 0);

			FindClose(hFind);

			UpdatePicture();
		}
	}
}

BOOL CPicturesDialog::LoadPicture(CPicture *pPicture, LPCTSTR lpszFilename)
{
	CImage img;
	if (FAILED(img.Load(lpszFilename)))
	{
		return FALSE;
	}

	CSize size = pPicture->GetPictureSize();

	LONG x, y;
	COLORREF color;

	for (x = 0; x < size.cx; x++)
	{
		for (y = 0; y < size.cy; y++)
		{
			if (x < img.GetWidth() && y < img.GetHeight())
				color = img.GetPixel(x, y);
			else
				color = RGB(255, 255, 255);
			pPicture->SetPixel(x, y, color);
		}
	}

	if (pPicture->GetPictureMode()==1 || pPicture->GetPictureMode()==3 || pPicture->GetPictureMode()==4)
		theApp.m_BootPicChanged=TRUE;

	return TRUE;
}

void CPicturesDialog::OnBnClickedPasteImage()
{
	if (!OpenClipboard())
		return;
	HBITMAP hBm;
	CImage img;
	hBm = (HBITMAP)GetClipboardData(CF_BITMAP);
	if (hBm==NULL)
		return;
	img.Attach(hBm);
	CSize size = m_Picture.GetPictureSize();

	LONG x, y;
	COLORREF color;

	for (x = 0; x < size.cx; x++)
	{
		for (y = 0; y < size.cy; y++)
		{
			if (x < img.GetWidth() && y < img.GetHeight())
				color = img.GetPixel(x, y);
			else
				color = RGB(255, 255, 255);
			m_Picture.SetPixel(x, y, color);
		}
	}
	CloseClipboard();
	UpdatePicture();
}

void CPicturesDialog::OnBnClickedCopyImage()
{
	
	/*HBITMAP hBm;
	CImage img;
	

	if (!img.Create(size.cx, size.cy, 24))
		return;

	int x, y;
	COLORREF c;

	for (x = 0; x < size.cx; x++)
	{
		for (y = 0; y < size.cy; y++)
		{
			c = m_Picture.GetPixel(x, y);
			img.SetPixel(x, y, c);
		}
	}*/

	//CDC *dc;
	//CBitmap bm;
	//img.ReleaseDC();
	//dc=m_PictureWnd.GetDC();
	//dc.Attach(hdc);
	//bm.CreateCompatibleBitmap(m_PictureWnd.GetDC(), size.cx, size.cy);
	CSize size = m_Picture.GetPictureSize();
	HDC hdc=::GetDC(m_PictureWnd.m_hWnd);
	CDC dc;
	dc.Attach(hdc);
	CDC memDC;
    memDC.CreateCompatibleDC(&dc);
	CBitmap bm;
	bm.CreateCompatibleBitmap(&dc, size.cx, size.cy);
	CBitmap * oldbm = memDC.SelectObject(&bm);
    memDC.BitBlt(0, 0, size.cx, size.cy, &dc, 0, 0, SRCCOPY);

	this->OpenClipboard();
	EmptyClipboard();
	SetClipboardData(CF_BITMAP, bm.m_hObject);
	CloseClipboard();
	memDC.SelectObject(oldbm);
     bm.Detach();  // make sure bitmap not deleted with CBitmap object
}

void CPicturesDialog::OnBnClickedFindBitmapRsrc()
{
	CString sID;
	sID.Format(_T("%d"), m_pFirmware->GetPictureID(m_PictureIndex));
	m_pLayoutDialog->GetDlgItem(IDC_FIND_EDIT)->SetWindowText(sID);
	m_pLayoutDialog->OnBnClickedFindButton();
	::SendMessage(::GetParent(::GetParent(m_hWnd)), WM_APP, (WPARAM)21, (LPARAM)0);
}
