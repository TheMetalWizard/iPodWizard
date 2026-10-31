#include "stdafx.h"
#include "iPodWizard.h"
#include "ThemesDialog.h"
#include ".\themesdialog.h"

#define NO_SCF_STR	_T("No String List file loaded")

IMPLEMENT_DYNAMIC(CThemesDialog, CDarkDialog)

CThemesDialog::CThemesDialog(CWnd* pParent /*=NULL*/)
	: CDarkDialog(CThemesDialog::IDD, pParent)
	, m_pFirmware(NULL)
	, m_pEditorDialog(NULL)
{
}

CThemesDialog::~CThemesDialog()
{
}

void CThemesDialog::DoDataExchange(CDataExchange* pDX)
{
	CDarkDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_GRAPHICS_LIST, m_GraphicsList);
	DDX_Control(pDX, IDC_RESOURCES_LIST, m_ResourcesList);
	DDX_Control(pDX, IDC_FONT_LIST, m_FontsList);
	DDX_Control(pDX, IDC_FONTIDX_CMB, m_FontIndexCombo);
	DDX_Control(pDX, IDC_OTFFONTIDX_CMB, m_OTFFontIndexCombo);
	DDX_Control(pDX, IDC_STRIDX_CMB, m_StringIndexCombo);
}

BEGIN_MESSAGE_MAP(CThemesDialog, CDarkDialog)
	ON_BN_CLICKED(IDC_LOADTHEME_BUTTON, OnBnClickedLoadTheme)
	ON_BN_CLICKED(IDC_MAKETHEME_BUTTON, OnBnClickedMakeTheme)
	ON_BN_CLICKED(IDC_LOADGRAPHICS_BUTTON, OnBnClickedLoadGraphics)
	ON_BN_CLICKED(IDC_DELETEGRAPHIC_BUTTON, OnBnClickedDeleteGraphic)
	ON_BN_CLICKED(IDC_LOADFONT_BUTTON, OnBnClickedLoadFont)
	ON_BN_CLICKED(IDC_DELETEFONT_BUTTON, OnBnClickedDeleteFont)
	ON_BN_CLICKED(IDC_LOADSCF_BUTTON, OnBnClickedLoadSCF)
	ON_BN_CLICKED(IDC_CLEARSCF_BUTTON, OnBnClickedClearSCF)
	ON_BN_CLICKED(IDC_OTF_CHK, OnBnClickedOtfChk)
	ON_BN_CLICKED(IDC_TLOADRESOURCES_BUTTON, &CThemesDialog::OnBnClickedTloadresourcesButton)
	ON_BN_CLICKED(IDC_TDELETERESOURCE_BUTTON, &CThemesDialog::OnBnClickedTdeleteresourceButton)
	ON_BN_CLICKED(IDC_THEMEPREVIEWER_BUTTON, &CThemesDialog::OnBnClickedThemepreviewerButton)
	ON_BN_CLICKED(IDC_EXPLAIN_BUTTON, &CThemesDialog::OnBnClickedExplainButton)
	ON_BN_CLICKED(IDC_MAKEFULLTHEME_BUTTON, &CThemesDialog::OnBnClickedMakefullthemeButton)
	ON_BN_CLICKED(IDC_CHK_RESOURCES, &CThemesDialog::OnBnClickedChkResources)
	ON_BN_CLICKED(IDC_CHECKTHEME_BUTTON, &CThemesDialog::OnBnClickedCheckTheme)
	ON_CONTROL_RANGE(BN_CLICKED, IDC_CHK_RES_LAYOUT, IDC_CHK_RES_TCMD, &CThemesDialog::OnBnClickedResSub)
END_MESSAGE_MAP()

BOOL CThemesDialog::OnInitDialog()
{
	CDarkDialog::OnInitDialog();
	
	// initialize lists
	m_GraphicsList.SetExtendedStyle(m_GraphicsList.GetExtendedStyle()|LVS_EX_FULLROWSELECT);
	m_GraphicsList.InsertColumn(0, TEXT("#"), LVCFMT_LEFT, 30);
	m_GraphicsList.InsertColumn(1, TEXT("Path"), LVCFMT_LEFT, 320);

	m_FontsList.SetExtendedStyle(m_FontsList.GetExtendedStyle()|LVS_EX_FULLROWSELECT);
	m_FontsList.InsertColumn(0, TEXT("#"), LVCFMT_LEFT, 20);
	m_FontsList.InsertColumn(1, TEXT("Type"), LVCFMT_LEFT, 50);
	m_FontsList.InsertColumn(2, TEXT("Bitmap Path"), LVCFMT_LEFT, 250);
	m_FontsList.InsertColumn(3, TEXT("Metrics Path"), LVCFMT_LEFT, 250);

	m_ResourcesList.SetExtendedStyle(m_ResourcesList.GetExtendedStyle()|LVS_EX_FULLROWSELECT);
	m_ResourcesList.InsertColumn(0, TEXT("#"), LVCFMT_LEFT, 30);
	m_ResourcesList.InsertColumn(1, TEXT("Type"), LVCFMT_LEFT, 30);
	m_ResourcesList.InsertColumn(2, TEXT("Path"), LVCFMT_LEFT, 320);

	return TRUE;
}

void CThemesDialog::OnOK()
{
	// TODO: Add your specialized code here and/or call the base class

	// CDarkDialog::OnOK();
}

void CThemesDialog::SetFirmware(CFirmware *pFirmware, CEditorDialog *pEditorDialog)
{
	m_pFirmware = pFirmware;

	m_pEditorDialog = pEditorDialog;

	m_pEditorDialog->SetPreviewButton(GetDlgItem(IDC_THEMEPREVIEWER_BUTTON));

	m_FontIndexCombo.ResetContent();
	m_StringIndexCombo.ResetContent();

	CString s;
	DWORD i;
	for (i = 0; i < m_pFirmware->GetNumFonts(); i++)
	{
		s.Format(TEXT("%d"), i);
		m_FontIndexCombo.InsertString(i, s);
	}
	m_FontIndexCombo.SetCurSel(0);

	for (i = 0; i < m_pFirmware->GetNumLangs(); i++)
	{
		s.Format(TEXT("%d"), i);
		m_StringIndexCombo.InsertString(i, s);
	}
	m_StringIndexCombo.SetCurSel(0);

	CheckDlgButton(IDC_OTF_CHK, 0);
	m_OTFFontIndexCombo.ResetContent();
	m_OTFFontIndexCombo.EnableWindow(FALSE);
	if (m_pFirmware->GetNumOTFFonts()>0)
	{
		m_OTFFontIndexCombo.ShowWindow(SW_SHOW);
		GetDlgItem(IDC_OTF_CHK)->ShowWindow(SW_SHOW);
		for (i = 0; i < m_pFirmware->GetNumOTFFonts(); i++)
		{
			s.Format(TEXT("%d"), i);
			m_OTFFontIndexCombo.InsertString(i, s);
		}
		m_OTFFontIndexCombo.SetCurSel(0);
	}
	else
	{
		m_OTFFontIndexCombo.ShowWindow(SW_HIDE);
		GetDlgItem(IDC_OTF_CHK)->ShowWindow(SW_HIDE);
	}

	m_GraphicsList.DeleteAllItems();
	m_FontsList.DeleteAllItems();
	m_SCFPath.Empty();
	GetDlgItem(IDC_SCFPATH_STATIC)->SetWindowText(NO_SCF_STR);

	GetDlgItem(IDC_LOADTHEME_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_MAKETHEME_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_MAKEFULLTHEME_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_LOADGRAPHICS_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_LOADFONT_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_LOADSCF_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_TLOADRESOURCES_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_THEMEPREVIEWER_BUTTON)->EnableWindow(TRUE);
}

void CThemesDialog::OnBnClickedMakeTheme()
{
	//The firmware of the desired iPod must be loaded.
	if (m_pFirmware==NULL)
	{
		MessageBox(TEXT("Can't make theme!\nYou must load the firmware you intend to put the theme on before you can make it."));
		return;
	}

	BOOL bGraphics=FALSE,bFonts=FALSE,bSCF=FALSE;
	//Check if the user loaded something to be on the theme
	if (m_GraphicsList.GetItemCount()>0)
		bGraphics=TRUE;
	if (m_FontsList.GetItemCount()>0)
		bFonts=TRUE;
	if (m_SCFPath.IsEmpty()==FALSE)
		bSCF=TRUE;
	if (!bGraphics && !bFonts && !bSCF)
	{
		MessageBox(TEXT("You need at least one type of resource to make a theme!"));
		return;
	}
	//Why bother to make a theme only for SCF file?
	if (bSCF && !bGraphics && !bFonts)
	{
		MessageBox(TEXT("You can't make a theme just for string changes, if you want string changes then just click 'Save Changes' in Strings tab in firmware editor and save the SCF file you want."));
		return;
	}

	CFileDialog dlg(FALSE, TEXT("ipw"), TEXT("MyTheme.ipw"), OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, TEXT("iPodWizard Theme Files (*.ipw)|*.ipw||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	CFile file;
	CZipArchive zip;
	zip.Open(dlg.GetPathName(), CZipArchive::zipCreate);

	CString s,s2,sMsg;
	CFile scffile;
	DWORD w,value;
	int x;

	//write current firmware version for later identification purposes:
	CString name;
	name=m_pFirmware->GetName();
	
	CZipFileHeader fhinfo;
	fhinfo.SetFileName(_T("firmware-id"));
	zip.OpenNewFile(fhinfo);
	DWORD version=m_pFirmware->GetFirmwareVersion();
	zip.WriteNewFile(&version, 4);
	zip.CloseNewFile();
	//zip.Close();

	CIpodFont font;
	COTFFont ofont;

	//Prepare pictures
	for (x=0;x<m_GraphicsList.GetItemCount();x++)
	{
		s=m_GraphicsList.GetItemText(x, 1);
		zip.AddNewFile(s);
		zip.CloseNewFile();
	}

	//Prepare fonts

	LPBYTE lpEnd = m_pFirmware->GetFirmwareBuffer() + m_pFirmware->GetFirmwareSize();
	BOOL bOTF,bSeperated;
	for (x=0;x<m_FontsList.GetItemCount();x++)
	{
		s=m_FontsList.GetItemText(x, 1);
		bOTF=FALSE;
		if (!s.Compare(TEXT("OpenType")))
			bOTF=TRUE;

		if (!s.Compare(TEXT("Mac Bitmap")))
			bSeperated=TRUE;

		s=m_FontsList.GetItemText(x, 0);
		if (_stscanf(s, TEXT("%d"), &value) != 1)
			value=0;
		
		if (!bOTF)
		{
			if (!font.Read(m_pFirmware->GetFont(value), lpEnd))
			{
				sMsg.Format(TEXT("Error reading font, make sure firmware is loaded!"));
				goto readerror;
			}
		}
		else if (!ofont.Read(m_pFirmware->GetOTFFont(value)))
		{
			sMsg.Format(TEXT("Error reading font, make sure firmware is loaded!"));
			goto readerror;
		}

		s=m_FontsList.GetItemText(x, 2);
		s2=m_FontsList.GetItemText(x, 3);
		if (!bOTF)
		{
			if (LoadFont(&font, s, s2)==FALSE)
			{
				sMsg.Format(TEXT("Error loading font from specific file in list! Either the bitmap or the metrics file is damaged."));
				goto readerror;
			}
			w=font.GetFontBlockLen();
			{ CString _fn; _fn.Format(_T("%s-%s-%d.fnt"), font.GetFontName(), font.GetFontStyle(), font.GetFontSize()); fhinfo.SetFileName(_fn); }
			zip.OpenNewFile(fhinfo);
			zip.WriteNewFile(m_pFirmware->GetFont(value), w);
			zip.CloseNewFile();
		}
		else
		{
			if (LoadOTFFont(&ofont, value, s)==FALSE)
			{
				sMsg.Format(TEXT("Error loading font from specific file in list! The file you tried to load is damaged."));
				goto readerror;
			}
			w=ofont.GetFontBlockLen();
			{ CString _fn; _fn.Format(_T("%s-%s-%d"), ofont.GetFontName(), ofont.GetFontStyle(), ofont.GetFontSize()); fhinfo.SetFileName(_fn); }
			zip.OpenNewFile(fhinfo);
			zip.WriteNewFile(m_pFirmware->GetOTFFont(value), w);
			zip.CloseNewFile();
		}
	}

	//Prepare String Changes File
	LPBYTE buffer;
	if (!bSCF)
		goto nextstep;
	if (!scffile.Open(m_SCFPath, CFile::modeRead))
		goto nextstep;

	w=(DWORD)m_StringIndexCombo.GetCurSel();
	{ CString _fn; _fn.Format(_T("StringList-%d.txt"), w); fhinfo.SetFileName(_fn); }
	zip.OpenNewFile(fhinfo);
	w=(DWORD)scffile.GetLength();
	buffer = new BYTE[w];
	if (scffile.Read(buffer, w) < w)
		goto readerror;
	zip.WriteNewFile(buffer, w);
	zip.CloseNewFile();
	delete buffer;

	//Prepare resources
	for (x=0;x<m_ResourcesList.GetItemCount();x++)
	{
		s=m_ResourcesList.GetItemText(x, 2);
		zip.AddNewFile(s);
		zip.CloseNewFile();
	}

nextstep:
	zip.Close();
	MessageBox(TEXT("Successfully wrote theme file!"));
	return;

readerror:
	zip.Close();
	DeleteFile(dlg.GetPathName());
	sMsg.AppendFormat(TEXT("\nAborting theme maker."));
	MessageBox(sMsg);
}

BOOL CThemesDialog::LoadOTFFont(COTFFont *pFont, DWORD index, LPCTSTR lpszFilename)
{
	CFile file;
	if (!file.Open(lpszFilename, CFile::modeRead))
		return FALSE;
	
	if (file.Read(m_pFirmware->GetOTFFont(index), pFont->GetFontBlockLen()) < pFont->GetFontBlockLen())
		return FALSE;
	
	return TRUE;
}

BOOL CThemesDialog::LoadFont(CIpodFont *pFont, LPCTSTR lpszBmpFilename, LPCTSTR lpszIfmFilename)
{
	CString mytemp;
	mytemp.Format(_T("%s"), lpszBmpFilename);
	if (!mytemp.Right(3).Compare(_T("fnt")))
	{
		CFile ffile;
		if (!ffile.Open(lpszBmpFilename, CFile::modeRead))
		{
			//MessageBox(TEXT("Unable to load file!"));
			return FALSE;
		}
		DWORD flength=(DWORD)ffile.GetLength();
		ffile.Close();
		m_pEditorDialog->m_FontsDialog.LoadFullFont(mytemp, flength);

		return TRUE;
	}
	else
	{

		//Bitmap
		CImage img;
		if (FAILED(img.Load(lpszBmpFilename)))
		{
			//MessageBox(TEXT("Unable to load image!"));
			return FALSE;
		}

		CSize size = pFont->GetFontBitmapSize();

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
				pFont->SetFontPixel(x, y, color);
			}
		}

		//Metrics
		CFile file;
		if (!file.Open(lpszIfmFilename, CFile::modeRead))
		{
			//MessageBox(TEXT("Unable to load file!"));
			return FALSE;
		}

		//new way :)
		DWORD fsize=pFont->GetMetricDataLen();
		DWORD length=(DWORD)file.GetLength();
		if (length!=fsize)
			goto NotValid;

		LPBYTE lpBuf;
		lpBuf=new BYTE[fsize];
		if (file.Read(lpBuf, fsize) == fsize)
		{
			pFont->SetMetricData(lpBuf);
		}
		else
			goto NotValid;

		return TRUE;
	}

	/*
	WORD offset1, offset2;
	SHORT width, ident;
	WORD c;

	WORD index1, index2, start, len;

	//Make file size validity check
	if ((file.GetLength() - 8*pFont->GetNumUnicodeGroups())%14 != 0)
		goto NotValid;

	while (TRUE)
	{
		if (file.Read(&index1, 2) < 2)
			break;
		if (file.Read(&start, 2) < 2)
			break;
		if (file.Read(&len, 2) < 2)
			break;
		if (file.Read(&offset1, 2) < 2)
			break;
		pFont->SetUnicodeGroup(index1, start, len, offset1);

		if (file.GetPosition() == (8*pFont->GetNumUnicodeGroups()))
			break;
	}

	while (TRUE)
	{
		if (file.Read(&index1, 2) < 2)
			break;
		if (file.Read(&index2, 2) < 2)
			break;

		if (file.Read(&c, 2) < 2)
			break;

		if (file.Read(&offset1, 2) < 2)
			break;
		if (file.Read(&offset2, 2) < 2)
			break;
		if (file.Read(&width, 2) < 2)
			break;
		if (file.Read(&ident, 2) < 2)
			break;
		pFont->SetCharMapping(index1, index2);
		pFont->SetCharMetrics(index2, offset1, offset2, width, ident);
	}
	file.Close();

	return TRUE;
	*/

NotValid:
	return FALSE;
}

BOOL CThemesDialog::LoadPicture(CPicture *pPicture, LPCTSTR lpszFilename)
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

	return TRUE;
}

void CThemesDialog::OnBnClickedLoadTheme()
{
	//The firmware of the desired iPod must be loaded.
	if (m_pFirmware==NULL)
	{
		MessageBox(TEXT("Can't load theme!\nYou must load the firmware you intend to put the theme on before you can load the theme to it."));
		return;
	}

	CFileDialog dlg(TRUE, 0, 0, OFN_HIDEREADONLY, TEXT("iPodWizard Theme Files (*.ipw)|*.ipw||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	LoadTheme(dlg.GetPathName());
}

void CThemesDialog::LoadTheme(CString filename)
{
	CZipArchive zip;
	zip.Open(filename, CZipArchive::zipOpenReadOnly);
	CString extract_dir;
	extract_dir=filename.Left(filename.ReverseFind('\\')+1);
	extract_dir.AppendFormat(_T("ThemeTempDir"));
	CZipFileHeader finfo;
	CStringArray files;
	DWORD nBufSize = 65535;
	CZipAutoBuffer buffer(nBufSize);

	//Check firmware identification to make sure it matches the currently loaded firmware.
	int file_id=zip.FindFile(_T("firmware-id"));
	zip.GetFileInfo(finfo, file_id);
	if (!zip.OpenFile(file_id))
	{
		MessageBox(_T("Couldn't find firmware id file, Theme file corrupted!"));
		zip.Close();
		goto finishup;
	}

	if (!zip.GetFileInfo(finfo, file_id))
	{
		//error
		MessageBox(_T("Couldn't find firmware id file, Theme file corrupted!"));
		zip.Close();
		goto finishup;
	}
	DWORD iRead;
	DWORD ptr=0;
	unsigned char *buf=new unsigned char[finfo.m_uUncomprSize];
	do
	{
		iRead = zip.ReadFile(buffer, buffer.GetSize());
		if (iRead)
		{
			memcpy(&buf[ptr], buffer, iRead);
			ptr+=iRead;
		}
	}
	while (iRead == buffer.GetSize());

	if (m_pFirmware->GetFirmwareVersion()!=*((unsigned long *)buf))
	{
		MessageBox(_T("The theme you are trying to load is intended for another firmware, aborting loading."));
		zip.Close();
		goto finishup;
	}
	zip.CloseFile();
	//Read firmware version and check if it's good //
	
	//Extract all files to temporary directory for loading
	for (WORD i = 0; i < zip.GetCount(); i++)
	{
		/*if (!zip.OpenFile(i))
		{
			MessageBox(_T("Theme file corrupted!"));
			zip.Close();
			return;
		}*/

		if (!zip.GetFileInfo(finfo, i))
		{
			//error
			MessageBox(_T("Theme file corrupted!"));
			zip.Close();
			goto finishup;
		}

		CString ifile=finfo.GetFileName(); //MB2Unicode((char *)finfo.GetFileName().GetBuffer());
		if (!ifile.Right(3).Compare(_T("fnt")) || !ifile.Right(3).Compare(_T("jpg")) || !ifile.Right(3).Compare(_T("bmp")) || !ifile.Right(3).Compare(_T("png")) || !ifile.Right(3).Compare(_T("txt")) || !ifile.Right(3).Compare(_T("rsrc")))
		{
			files.Add(finfo.GetFileName());
			zip.ExtractFile(i, extract_dir);
		}
	}

	//Load pictures
	m_pEditorDialog->m_PicsDialog.LoadPictures(extract_dir);

	//Load fonts
	m_pEditorDialog->m_FontsDialog.LoadFullFontsDir(extract_dir);
	m_pEditorDialog->m_OTFDialog.LoadFullFontsDir(extract_dir);

	//Load resources
	m_pEditorDialog->m_LayoutDialog.LoadAllResources(extract_dir, 0, TRUE);
	m_pEditorDialog->m_LayoutDialog.LoadAllResources(extract_dir, 1, TRUE);
	m_pEditorDialog->m_LayoutDialog.LoadAllResources(extract_dir, 2, TRUE);

	//Load strings
	file_id=zip.FindFile(_T("*.txt"));
	if (zip.OpenFile(file_id))
	{
		if (!zip.GetFileInfo(finfo, file_id))
		{
			//error
			MessageBox(_T("Couldn't find firmware id file, Theme file corrupted!"));
			zip.Close();
			goto finishup;
		}

		int fpos=finfo.GetFileName().Find('-');
		DWORD ilang=-1;
		if (fpos!=-1)
		{
			CString mylang=finfo.GetFileName().Right(finfo.GetFileName().GetLength()-fpos);
			if (mylang.IsEmpty()==FALSE)
				ilang=(DWORD)_ttoi(mylang);
		}

		if (ilang!=-1)
		{
			CFile file;
			CString strfile;
			strfile=extract_dir;
			strfile.Append(_T("\\"));
			strfile.Append(finfo.GetFileName());
			if (!file.Open(strfile, CFile::modeRead))
			{
				MessageBox(TEXT("Unable to load strings file!"));
				goto finishup;
			}
			
			DWORD filelen=(DWORD)file.GetLength();
			if (m_pEditorDialog->m_StringDialog.LoadLanguageBlock(&file, filelen,ilang)==FALSE)
			{
				file.Close();
				MessageBox(TEXT("The file you chose is not a correct String List File!"), TEXT("Error"));
				goto finishup;
			}
			else
			{
				file.Close();
			}
		}
	}


/*
	//Make validation checks to see if this theme is compatible with the current loaded firmware
	int bFirmCompare=0;
	if (name.Compare(currname)==0)
		bFirmCompare=0;
	else
	{
		currname = currname.Left(currname.Find(TEXT(".")));
		name = name.Left(name.Find(TEXT(".")));
		if (name.Compare(currname)==0)
			bFirmCompare=1;
		else
			bFirmCompare=2;
	}

	if (bFirmCompare==2)
	{
		file.Close();
		MessageBox(TEXT("Damage protection: not allowed to load theme!\nThe firmware you are trying to load this theme to is from a different iPod model than the iPod model the theme creator used."));
		return;
	}
	if (bSCF)
	{
		if (bFirmCompare!=0)
		{
			file.Close();
			MessageBox(TEXT("Damage protection: not allowed to load theme!\nThe theme you are about to load contains string changes but the firmware you want to load this theme to isn't the original firmware the theme creator used and it can screw you're iPod badly if you load it."));
			return;
		}
	}
	if ((bGraphics || bFonts) && bFirmCompare==1)
		if (MessageBox(TEXT("The theme you are about to load is from a different firmware revision than the current one in your updater (it's the same iPod model though). Do you still want to load this theme?"), TEXT("Warning"), MB_YESNO) != IDYES)
		{
			file.Close();
			return;
		}
*/
	zip.Close();

	RecursiveDelete(extract_dir);
	RemoveDirectory(extract_dir);

	//reset stuff:
	m_GraphicsList.DeleteAllItems();
	m_FontsList.DeleteAllItems();
	m_SCFPath.Empty();
	//
	MessageBox(TEXT("Successfully loaded theme file!"));
	return;

finishup:
	RecursiveDelete(extract_dir);
	RemoveDirectory(extract_dir);
}

void CThemesDialog::OnBnClickedLoadGraphics()
{
	//The firmware of the desired iPod must be loaded.
	if (m_pFirmware==NULL)
	{
		MessageBox(TEXT("Can't load graphics!\nYou must load the firmware you intend to put the theme on before you can load graphics."));
		return;
	}

	LPITEMIDLIST il;
	TCHAR folderPath[MAX_PATH];

	BROWSEINFO br;
    br.hwndOwner = AfxGetMainWnd()->GetSafeHwnd();
    br.pidlRoot = NULL;
    br.pszDisplayName = folderPath;
    br.lpszTitle = TEXT("Select folder for graphics");
    br.ulFlags = BIF_RETURNONLYFSDIRS;
    br.lpfn = NULL;
    br.lParam = 0;
    br.iImage = 0;

	il = SHBrowseForFolder(&br);
	if (il == NULL)
		return;

	SHGetPathFromIDList(il, folderPath);

	DWORD index;
	CString filename = folderPath;
	
	if (theApp.m_OldPicIndex==TRUE)
	{
		filename += "\\pic???.bmp";

		CString s,s3;
		int c=0;
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
				
				if (index!=0xFFFFFFFF)
				{
					s3.Format(TEXT("%5d"), index);
					int j=-1;
					for (int x=0;x<m_GraphicsList.GetItemCount();x++)
					{
						s=m_GraphicsList.GetItemText(x, 0);
						if (s.Compare(s3)==0)
						{
							j=x;
							break;
						}
					}
					if (j==-1)
					{
						j=m_GraphicsList.GetItemCount();
						m_GraphicsList.InsertItem(j, s3);
					}
					m_GraphicsList.SetItemText(j, 1, filename);
					c++;
				}

			} while (FindNextFile(hFind, &findData) != 0);

			FindClose(hFind);

			s.Format(TEXT("Found %d valid picture(s)."), c);
			MessageBox(s);
		}
	}
	else
	{
		filename += "\\pic?????.bmp";

		CString s,s3;
		int c=0;
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
					s3.Format(TEXT("%5d"), index);
					int j=-1;
					for (int x=0;x<m_GraphicsList.GetItemCount();x++)
					{
						s=m_GraphicsList.GetItemText(x, 0);
						if (s.Compare(s3)==0)
						{
							j=x;
							break;
						}
					}
					if (j==-1)
					{
						j=m_GraphicsList.GetItemCount();
						m_GraphicsList.InsertItem(j, s3);
					}
					m_GraphicsList.SetItemText(j, 1, filename);
					c++;
				}
			} while (FindNextFile(hFind, &findData) != 0);

			FindClose(hFind);

			s.Format(TEXT("Found %d valid picture(s)."), c);
			MessageBox(s);
		}
	}
}

void CThemesDialog::OnBnClickedDeleteGraphic()
{
	POSITION pos = m_GraphicsList.GetFirstSelectedItemPosition();
	CWordArray items;
	while (pos)
	{
		items.Add((WORD)m_GraphicsList.GetNextSelectedItem(pos));
	}
	int y=0;
	for (int i=0;i<items.GetCount();i++)
	{
		int x=(int)items.GetAt(i);
		m_GraphicsList.DeleteItem(x-y);
		y++;
	}
}

void CThemesDialog::OnBnClickedLoadFont()
{
	if (m_pFirmware==NULL)
	{
		MessageBox(TEXT("Can't load font! Font index not selected.\nYou must load the firmware you intend to put the theme on before you can load a font with a corresponding index to the list."));
		return;
	}

	BOOL bSeperated=FALSE;
	BOOL bOTF=FALSE;
	if (IsDlgButtonChecked(IDC_OTF_CHK) == 1)
		bOTF=TRUE;

	CString filetype,title;
	if (bOTF)
	{
		filetype.Format(TEXT("OpenType Font Files (*.otf)|*.otf||"));
		title.Format(TEXT("Open OpenType font file"));
	}
	else
	{
		if (MessageBox(_T("Do you want to load font as one .fnt file or bitmap and metrics separated?\nYes for one .fnt file, no for the other."), _T("Question"), MB_YESNO)!=IDYES)
		{
			bSeperated=TRUE;
			filetype.Format(TEXT("Bitmap Files (*.bmp, *.gif, *.jpg)|*.bmp;*.gif;*.jpg||"));
			title.Format(TEXT("Open bitmap font file"));
		}
		else
		{
			filetype.Format(TEXT("RAW iPod Font Files (*.fnt)|*.fnt||"));
			title.Format(TEXT("Open iPod font file"));
		}
	}
	//Get font file
	CFileDialog dlg_b(TRUE, 0, 0, OFN_HIDEREADONLY, filetype, this);
	MO_LOAD_RESOURCES_PATH(dlg_b)

	dlg_b.m_pOFN->lpstrTitle = title;

	if (dlg_b.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg_b)
	CString s,s2,s3;
	int i=-1,x;
	if (bOTF)
	{
		s2.Format(TEXT("%d"), m_OTFFontIndexCombo.GetCurSel());
		for (x=0;x<m_FontsList.GetItemCount();x++)
		{
			s3=m_FontsList.GetItemText(x, 3);
			if (s3.Compare(TEXT("OpenType"))==0)
			{
				s=m_FontsList.GetItemText(x, 0);
				if (s.Compare(s2)==0)
				{
					i=x;
					break;
				}
			}
		}
		if (i==-1)
		{
			i=m_FontsList.GetItemCount();
			m_FontsList.InsertItem(i, s2);
		}

		s.Format(TEXT("OpenType"));
		m_FontsList.SetItemText(i, 1, s);

		s.Format(dlg_b.GetPathName());
		m_FontsList.SetItemText(i, 2, s);

		s.Format(TEXT(""));
		m_FontsList.SetItemText(i, 3, s);
	}
	else if (bSeperated)
	{
		//Get metrics file
		CFileDialog dlg_m(TRUE, TEXT("ifm"), 0, OFN_HIDEREADONLY, TEXT("Fonts Metrics (*.ifm)|*.ifm||"), this);
		MO_LOAD_RESOURCES_PATH(dlg_m)

		dlg_m.m_pOFN->lpstrTitle = TEXT("Open metrics file");

		if (dlg_m.DoModal() != IDOK)
			return;
		MO_SAVE_RESOURCES_PATH(dlg_m)
		s2.Format(TEXT("%d"), m_FontIndexCombo.GetCurSel());
		for (x=0;x<m_FontsList.GetItemCount();x++)
		{
			s3=m_FontsList.GetItemText(x, 3);
			if (s3.Compare(TEXT("Mac Bitmap"))==0)
			{
				s=m_FontsList.GetItemText(x, 0);
				if (s.Compare(s2)==0)
				{
					i=x;
					break;
				}
			}
		}
		if (i==-1)
		{
			i=m_FontsList.GetItemCount();
			m_FontsList.InsertItem(i, s2);
		}
		s.Format(TEXT("Mac Bitmap"));
		m_FontsList.SetItemText(i, 1, s);

		s.Format(dlg_b.GetPathName());
		m_FontsList.SetItemText(i, 2, s);

		s.Format(dlg_m.GetPathName());
		m_FontsList.SetItemText(i, 3, s);
	}
	else
	{
		s2.Format(TEXT("%d"), m_FontIndexCombo.GetCurSel());
		for (x=0;x<m_FontsList.GetItemCount();x++)
		{
			s3=m_FontsList.GetItemText(x, 3);
			if (s3.Compare(TEXT("iPod RAW Font"))==0)
			{
				s=m_FontsList.GetItemText(x, 0);
				if (s.Compare(s2)==0)
				{
					i=x;
					break;
				}
			}
		}
		if (i==-1)
		{
			i=m_FontsList.GetItemCount();
			m_FontsList.InsertItem(i, s2);
		}
		s.Format(TEXT("iPod RAW Font"));
		m_FontsList.SetItemText(i, 1, s);

		s.Format(dlg_b.GetPathName());
		m_FontsList.SetItemText(i, 2, s);

		s.Format(TEXT(""));
		m_FontsList.SetItemText(i, 3, s);
	}
}

void CThemesDialog::OnBnClickedDeleteFont()
{
	POSITION pos = m_FontsList.GetFirstSelectedItemPosition();
	CWordArray items;
	while (pos)
	{
		items.Add((WORD)m_FontsList.GetNextSelectedItem(pos));
	}
	int y=0;
	for (int i=0;i<items.GetCount();i++)
	{
		int x=(int)items.GetAt(i);
		m_FontsList.DeleteItem(x-y);
		y++;
	}
}

void CThemesDialog::OnBnClickedLoadSCF()
{
	CFileDialog dlg(TRUE, TEXT("txt"), 0, OFN_HIDEREADONLY, TEXT("String List File (*.txt)|*.txt||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	m_SCFPath.Format(dlg.GetPathName());
	GetDlgItem(IDC_SCFPATH_STATIC)->SetWindowText(m_SCFPath);
}

void CThemesDialog::OnBnClickedClearSCF()
{
	m_SCFPath.Empty();
	GetDlgItem(IDC_SCFPATH_STATIC)->SetWindowText(NO_SCF_STR);
}
void CThemesDialog::OnBnClickedOtfChk()
{
	if (IsDlgButtonChecked(IDC_OTF_CHK) == 1)
	{
		m_OTFFontIndexCombo.EnableWindow(TRUE);
		m_FontIndexCombo.EnableWindow(FALSE);
	}
	else
	{
		m_OTFFontIndexCombo.EnableWindow(FALSE);
		m_FontIndexCombo.EnableWindow(TRUE);
	}
}
void CThemesDialog::OnBnClickedTloadresourcesButton()
{
	//The firmware of the desired iPod must be loaded.
	if (m_pFirmware==NULL)
	{
		MessageBox(TEXT("Can't load graphics!\nYou must load the firmware you intend to put the theme on before you can load graphics."));
		return;
	}

	LPITEMIDLIST il;
	TCHAR folderPath[MAX_PATH];

	BROWSEINFO br;
    br.hwndOwner = AfxGetMainWnd()->GetSafeHwnd();
    br.pidlRoot = NULL;
    br.pszDisplayName = folderPath;
    br.lpszTitle = TEXT("Select folder for resources");
    br.ulFlags = BIF_RETURNONLYFSDIRS;
    br.lpfn = NULL;
    br.lParam = 0;
    br.iImage = 0;

	il = SHBrowseForFolder(&br);
	if (il == NULL)
		return;

	SHGetPathFromIDList(il, folderPath);

	CString filename = folderPath;

	bool found;
	int c=0;
	CString s,s3,type;

	for (int r=0;r<4;r++)
	{
		filename = folderPath;
		switch(r)
		{
		case 0:
			filename += "\\view?????.rsrc";
			type = _T("Scheme");
			break;
		case 1:
			filename += "\\menu?????.rsrc";
			type = _T("Menu");
			break;
		case 2:
			filename += "\\type?????.rsrc";
			type = _T("Font");
			break;
		case 3:
			filename += "\\tcmd?????.rsrc";
			type = _T("Command");
			break;
		}

		WIN32_FIND_DATA findData;
		HANDLE hFind = FindFirstFile(filename, &findData);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				filename = folderPath;
				filename += "\\";
				filename += findData.cFileName;

				found=m_pFirmware->FindResourceID(r, _wtoi(findData.cFileName + 4));
				if (found)
				{
					s3.Format(TEXT("%5d"), _wtoi(findData.cFileName + 4));
					int j=-1;
					for (int x=0;x<m_ResourcesList.GetItemCount();x++)
					{
						s=m_ResourcesList.GetItemText(x, 0);
						if (s.Compare(s3)==0)
						{
							j=x;
							break;
						}
					}
					if (j==-1)
					{
						j=m_ResourcesList.GetItemCount();
						m_ResourcesList.InsertItem(j, s3);
					}
					m_ResourcesList.SetItemText(j, 1, type);
					m_ResourcesList.SetItemText(j, 2, filename);
					c++;
				}
			} while (FindNextFile(hFind, &findData) != 0);

			FindClose(hFind);
			
		}
	}
	UpdateData(FALSE);
	s.Format(TEXT("Found %d valid resource(s)."), c);
	MessageBox(s);
}

void CThemesDialog::OnBnClickedTdeleteresourceButton()
{
	POSITION pos = m_ResourcesList.GetFirstSelectedItemPosition();
	CWordArray items;
	while (pos)
	{
		items.Add((WORD)m_ResourcesList.GetNextSelectedItem(pos));
	}
	int y=0;
	for (int i=0;i<items.GetCount();i++)
	{
		int x=(int)items.GetAt(i);
		m_ResourcesList.DeleteItem(x-y);
		y++;
	}
}

void CThemesDialog::OnBnClickedThemepreviewerButton()
{
	CThemePreviewDialog dlg;
	
	dlg.SetFirmware(m_pFirmware, m_pEditorDialog);
	dlg.DoModal();
}

// "Making a theme": the labels of the four asset kinds are in bold, which a message box cannot do
static HFONT g_ThemeHelpBold = NULL;

static INT_PTR CALLBACK ThemeHelpDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			LOGFONT lf;
			GetObject((HFONT)SendMessage(hDlg, WM_GETFONT, 0, 0), sizeof(lf), &lf);
			lf.lfWeight = FW_BOLD;
			g_ThemeHelpBold = CreateFontIndirect(&lf);
			for (int id = IDC_THEMEHELP_B1; id <= IDC_THEMEHELP_B4; id++)
				SendDlgItemMessage(hDlg, id, WM_SETFONT, (WPARAM)g_ThemeHelpBold, TRUE);
		}
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, IDOK);
			return TRUE;
		}
		break;
	case WM_DESTROY:
		if (g_ThemeHelpBold)
			DeleteObject(g_ThemeHelpBold);
		g_ThemeHelpBold = NULL;
		break;
	}
	return FALSE;
}

void CThemesDialog::OnBnClickedExplainButton()
{
	DialogBoxParam(AfxGetResourceHandle(), MAKEINTRESOURCE(IDD_THEME_HELP), m_hWnd, ThemeHelpDlgProc, 0);
}

void CThemesDialog::OnBnClickedMakefullthemeButton()
{
	CWaitCursor wait;
	//The firmware of the desired iPod must be loaded.
	if (m_pFirmware==NULL)
	{
		MessageBox(TEXT("Can't make theme!\nYou must load the firmware you intend to put the theme on before you can make it."));
		return;
	}

	CFileDialog dlg(FALSE, TEXT("ipw"), TEXT("MyTheme.ipw"), OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, TEXT("iPodWizard Theme Files (*.ipw)|*.ipw||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	CFile file;
	CZipArchive zip;
	zip.Open(dlg.GetPathName(), CZipArchive::zipCreate);

	CString s,s2,sMsg;
	CFile scffile;
	DWORD w;
	int x;

	//write current firmware version for later identification purposes:
	
	CZipFileHeader fhinfo;
	fhinfo.SetFileName(_T("firmware-id"));
	zip.OpenNewFile(fhinfo);
	DWORD version=m_pFirmware->GetFirmwareVersion();
	zip.WriteNewFile(&version, 4);
	zip.CloseNewFile();
	//zip.Close();

	CIpodFont font;
	COTFFont ofont;
	CPicture pic;

	CString filename;
	CString folderPath;
	folderPath.SetString(dlg.GetPathName().Left(dlg.GetPathName().ReverseFind('\\')+1));
	folderPath.AppendFormat(_T("ThemeTempDir"));

	CreateDirectory(folderPath, NULL);

	if (IsDlgButtonChecked(IDC_CHK_GRAPHICS))
	{
		//Prepare pictures
		for (x=0;x<m_pFirmware->GetNumPictures();x++)
		{
			if (pic.Read(m_pFirmware->GetPicture(x)))
			{
				filename.Format(_T("%s\\%05d.bmp"), folderPath, m_pFirmware->GetPictureID(x));
				m_pEditorDialog->m_PicsDialog.SavePicture(&pic, filename);
				zip.AddNewFile(filename);
			}
		}
	}

	if (IsDlgButtonChecked(IDC_CHK_FONTS))
	{
		//Prepare fonts
		LPBYTE lpEnd = m_pFirmware->GetFirmwareBuffer() + m_pFirmware->GetFirmwareSize();
		for (x=0;x<m_pFirmware->GetNumFonts();x++)
		{
			if (!font.Read(m_pFirmware->GetFont(x), lpEnd))
			{
				sMsg.Format(TEXT("Error reading font, make sure firmware is loaded!"));
				goto readerror;
			}

			w=font.GetFontBlockLen();
			{ CString _fn; _fn.Format(_T("%s-%s-%d.fnt"), font.GetFontName(), font.GetFontStyle(), font.GetFontSize()); fhinfo.SetFileName(_fn); }
			zip.OpenNewFile(fhinfo);
			zip.WriteNewFile(m_pFirmware->GetFont(x), w);
			zip.CloseNewFile();
		}

		for (x=0;x<m_pFirmware->GetNumOTFFonts();x++)
		{
			if (!ofont.Read(m_pFirmware->GetOTFFont(x)))
			{
				sMsg.Format(TEXT("Error reading font, make sure firmware is loaded!"));
				goto readerror;
			}

			w=ofont.GetFontBlockLen();
			{ CString _fn; _fn.Format(_T("%s-%s-%d"), ofont.GetFontName(), ofont.GetFontStyle(), ofont.GetFontSize()); fhinfo.SetFileName(_fn); }
			zip.OpenNewFile(fhinfo);
			zip.WriteNewFile(m_pFirmware->GetOTFFont(x), w);
			zip.CloseNewFile();
		}
	}

	if (IsDlgButtonChecked(IDC_CHK_STRINGS))
	{
		//Prepare String Changes File
		filename.Format(_T("%s\\Lang-%d.txt"), folderPath, m_pFirmware->GetNumLangs()-1);
		if (!scffile.Open(filename, CFile::modeCreate | CFile::modeWrite))
			goto nextstep;
		m_pEditorDialog->m_StringDialog.SaveAllStrings(&scffile);

		LPBYTE buffer;
		
		if (!scffile.Open(filename, CFile::modeRead))
			goto nextstep;

		w=(DWORD)m_StringIndexCombo.GetCurSel();
		{ CString _fn; _fn.Format(_T("StringList-%d.txt"), w); fhinfo.SetFileName(_fn); }
		zip.OpenNewFile(fhinfo);
		w=(DWORD)scffile.GetLength();
		buffer = new BYTE[w];
		if (scffile.Read(buffer, w) < w)
			goto readerror;
		zip.WriteNewFile(buffer, w);
		zip.CloseNewFile();
		delete buffer;
	}

	if (IsDlgButtonChecked(IDC_CHK_RESOURCES))
	{
		//Prepare resources
		DWORD j;
		int k;
		LPBYTE lpBuf;
		UINT nCount;
		
		if (IsDlgButtonChecked(IDC_CHK_RES_LAYOUT))
		{
			for (j = 0; j < m_pEditorDialog->m_LayoutDialog.ViewList->size(); j++)
			{
				nCount=4;
				for (k = 0; k < m_pEditorDialog->m_LayoutDialog.ViewList->at(j)->num_elems; k++)
				{
					nCount+=m_pEditorDialog->m_LayoutDialog.ViewList->at(j)->ViewElemList[k]->element->size+4;
				}
				lpBuf=(LPBYTE)m_pEditorDialog->m_LayoutDialog.ViewList->at(j)->ViewElemList[0]->element-4;
				filename.Format(TEXT("%s\\view%d.rsrc"), folderPath, m_pEditorDialog->m_LayoutDialog.ViewList->at(j)->id);
				m_pEditorDialog->m_LayoutDialog.SaveRAWResource(filename, lpBuf, nCount);
				zip.AddNewFile(filename);
			}
		}
		if (IsDlgButtonChecked(IDC_CHK_RES_MENU))
		{
			for (j = 0; j < m_pEditorDialog->m_LayoutDialog.MenuList->size(); j++)
			{
				nCount=4;
				for (k = 0; k < m_pEditorDialog->m_LayoutDialog.MenuList->at(j)->num_elems; k++)
				{
					nCount+=m_pEditorDialog->m_LayoutDialog.MenuList->at(j)->MenuElemList[k]->element->size+4;
				}
				lpBuf=(LPBYTE)m_pEditorDialog->m_LayoutDialog.MenuList->at(j)->MenuElemList[0]->element-4;
				filename.Format(TEXT("%s\\menu%d.rsrc"), folderPath, m_pEditorDialog->m_LayoutDialog.MenuList->at(j)->id);
				m_pEditorDialog->m_LayoutDialog.SaveRAWResource(filename, lpBuf, nCount);
				zip.AddNewFile(filename);
			}
		}
		if (IsDlgButtonChecked(IDC_CHK_RES_TYPE))
		{
			for (j = 0; j < m_pEditorDialog->m_LayoutDialog.TypeList->size(); j++)
			{
				nCount=sizeof(*m_pEditorDialog->m_LayoutDialog.TypeList->at(j)->element);
				lpBuf=(LPBYTE)m_pEditorDialog->m_LayoutDialog.TypeList->at(j)->element;
				filename.Format(TEXT("%s\\type%d.rsrc"), folderPath, m_pEditorDialog->m_LayoutDialog.TypeList->at(j)->id);
				m_pEditorDialog->m_LayoutDialog.SaveRAWResource(filename, lpBuf, nCount);
				zip.AddNewFile(filename);
			}
		}
		if (IsDlgButtonChecked(IDC_CHK_RES_TCMD))
		{
			for (j = 0; j < m_pEditorDialog->m_LayoutDialog.TCMDList->at(0)->element.elements.size(); j++)
			{
				nCount=sizeof(*m_pEditorDialog->m_LayoutDialog.TCMDList->at(0)->element.elements.at(j));
				lpBuf=(LPBYTE)m_pEditorDialog->m_LayoutDialog.TCMDList->at(0)->element.elements.at(j);
				filename.Format(TEXT("%s\\tcmd%d.rsrc"), folderPath, m_pEditorDialog->m_LayoutDialog.TCMDList->at(0)->element.elements.at(j)->id);
				m_pEditorDialog->m_LayoutDialog.SaveRAWResource(filename, lpBuf, nCount);
				zip.AddNewFile(filename);
			}
		}
	}

nextstep:
	RecursiveDelete(folderPath);
	RemoveDirectory(folderPath);
	zip.Close();
	MessageBox(TEXT("Successfully wrote theme file!"));
	return;

readerror:
	zip.Close();
	DeleteFile(dlg.GetPathName());
	RecursiveDelete(folderPath);
	RemoveDirectory(folderPath);
	sMsg.AppendFormat(TEXT("\nAborting theme maker."));
	MessageBox(sMsg);
}

static const UINT kResSubIds[4] = { IDC_CHK_RES_LAYOUT, IDC_CHK_RES_TYPE, IDC_CHK_RES_MENU, IDC_CHK_RES_TCMD };

// Resources is checked exactly when at least one of its four elements is selected.
void CThemesDialog::OnBnClickedResSub(UINT nID)
{
	CheckDlgButton(nID, IsDlgButtonChecked(nID) ? BST_UNCHECKED : BST_CHECKED);
	BOOL any = FALSE;
	for (int i = 0; i < 4; i++)
		if (IsDlgButtonChecked(kResSubIds[i]))
			any = TRUE;
	CheckDlgButton(IDC_CHK_RESOURCES, any ? BST_CHECKED : BST_UNCHECKED);
}

// Checking or unchecking Resources selects or clears all four elements.
void CThemesDialog::OnBnClickedChkResources()
{
	UINT state = IsDlgButtonChecked(IDC_CHK_RESOURCES) ? BST_CHECKED : BST_UNCHECKED;
	for (int i = 0; i < 4; i++)
		CheckDlgButton(kResSubIds[i], state);
}

// Describes a firmware version ID using the [ThemeFirmwareID] section of iPW.ini (e.g. B002=5g iPod: 13.6.1, 13.6.1.1).
static CString DescribeFirmwareId(DWORD id)
{
	CString key;
	key.Format(_T("%X"), id);
	TCHAR buf[512];
	if (GetPrivateProfileString(_T("ThemeFirmwareID"), key, NULL, buf, 512, theApp.m_IniPath) > 0)
		return CString(buf);
	return CString();
}

static void AppendCount(CString &s, int n, LPCTSTR label)
{
	if (n <= 0)
		return;
	CString t;
	t.Format(_T("\r\n  %d %s"), n, label);
	s += t;
}

// Inspects a theme file without needing a firmware to be loaded.
void CThemesDialog::OnBnClickedCheckTheme()
{
	CFileDialog dlg(TRUE, 0, 0, OFN_HIDEREADONLY, TEXT("iPodWizard Theme Files (*.ipw)|*.ipw||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)

	DWORD themeId = 0;
	bool haveId = false;
	int nPics = 0, nBmpFonts = 0, nOtfFonts = 0, nViews = 0, nMenus = 0, nTypes = 0, nTcmd = 0, nStrings = 0, nOther = 0;

	CZipArchive zip;
	try
	{
		zip.Open(dlg.GetPathName(), CZipArchive::zipOpenReadOnly);

		int idIdx = zip.FindFile(_T("firmware-id"));
		if (idIdx >= 0 && zip.OpenFile(idIdx))
		{
			DWORD v = 0;
			if (zip.ReadFile(&v, sizeof(v)) == sizeof(v))
			{
				themeId = v;
				haveId = true;
			}
			zip.CloseFile();
		}

		CZipFileHeader finfo;
		for (WORD i = 0; i < zip.GetCount(); i++)
		{
			if (!zip.GetFileInfo(finfo, i))
				continue;
			CString full = finfo.GetFileName();
			full.Replace(_T('/'), _T('\\'));
			CString name = full.Mid(full.ReverseFind(_T('\\')) + 1);
			name.MakeLower();
			if (name == _T("firmware-id"))
				continue;

			int dot = name.ReverseFind(_T('.'));
			if (dot < 0)
			{
				nOtfFonts++;
				continue;
			}
			CString ext = name.Mid(dot + 1);
			if (ext == _T("bmp") || ext == _T("jpg") || ext == _T("png"))
				nPics++;
			else if (ext == _T("fnt"))
				nBmpFonts++;
			else if (ext == _T("txt"))
				nStrings++;
			else if (ext == _T("rsrc"))
			{
				if (name.Left(4) == _T("view"))
					nViews++;
				else if (name.Left(4) == _T("menu"))
					nMenus++;
				else if (name.Left(4) == _T("type"))
					nTypes++;
				else if (name.Left(4) == _T("tcmd"))
					nTcmd++;
				else
					nOther++;
			}
			else
				nOther++;
		}
		zip.Close();
	}
	catch (CException *e)
	{
		e->Delete();
		MessageBox(TEXT("This file could not be read as a theme (it isn't a valid iPodWizard theme file)."), TEXT("Theme compatibility"), MB_OK | MB_ICONWARNING);
		return;
	}

	CString fileName = dlg.GetPathName();
	fileName = fileName.Mid(fileName.ReverseFind(_T('\\')) + 1);
	CString report;
	report.Format(_T("Theme: %s\r\n\r\n"), (LPCTSTR)fileName);

	if (!haveId)
	{
		report += _T("No firmware ID was found in this file, so it doesn't look like a valid iPodWizard theme.");
		MessageBox(report, TEXT("Theme compatibility"), MB_OK | MB_ICONWARNING);
		return;
	}

	CString line;
	line.Format(_T("Firmware ID: 0x%X\r\n"), themeId);
	report += line;
	CString desc = DescribeFirmwareId(themeId);
	if (desc.IsEmpty())
		report += _T("Made for: unknown (this ID isn't listed under [ThemeFirmwareID] in iPW.ini)\r\n");
	else
		report += _T("Made for: ") + desc + _T("\r\n");

	CString contents;
	AppendCount(contents, nPics, _T("picture(s)"));
	AppendCount(contents, nBmpFonts, _T("bitmap font(s)"));
	AppendCount(contents, nOtfFonts, _T("OpenType font(s)"));
	AppendCount(contents, nViews, _T("view resource(s)"));
	AppendCount(contents, nMenus, _T("menu resource(s)"));
	AppendCount(contents, nTypes, _T("font type resource(s)"));
	AppendCount(contents, nTcmd, _T("command table resource(s)"));
	AppendCount(contents, nStrings, _T("string list file(s)"));
	AppendCount(contents, nOther, _T("other file(s)"));
	report += _T("\r\nContents:");
	report += contents.IsEmpty() ? CString(_T("\r\n  (nothing recognized)")) : contents;
	report += _T("\r\n\r\n");

	if (m_pFirmware == NULL)
	{
		report += _T("No firmware is loaded. Load a firmware to test it against this theme.");
	}
	else
	{
		DWORD loadedId = m_pFirmware->GetFirmwareVersion();
		CString loadedDesc = DescribeFirmwareId(loadedId);
		line.Format(_T("Loaded firmware: ID 0x%X"), loadedId);
		report += line;
		if (!loadedDesc.IsEmpty())
			report += _T(" - ") + loadedDesc;
		report += _T("\r\n");
		report += (loadedId == themeId) ? _T("Result: compatible with the loaded firmware.")
										 : _T("Result: NOT compatible with the loaded firmware (different firmware ID).");
	}

	MessageBox(report, TEXT("Theme compatibility"), MB_OK | MB_ICONINFORMATION);
}
