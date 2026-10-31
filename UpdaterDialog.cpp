#include "stdafx.h"
#include "iPodWizard.h"
#include "UpdaterDialog.h"

IMPLEMENT_DYNAMIC(CUpdaterDialog, CDarkDialog)

CUpdaterDialog::CUpdaterDialog(CWnd* pParent /*=NULL*/)
	: CDarkDialog(CUpdaterDialog::IDD, pParent),
	  m_listTop(0), m_listLeft(0), m_listWidth(0),
	  m_rowHeight(0), m_rowGap(0), m_bottomGap(0), m_listRowGap(0)
{
}

CUpdaterDialog::~CUpdaterDialog()
{
}

void CUpdaterDialog::DoDataExchange(CDataExchange* pDX)
{
	CDarkDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FIRMWARE_LIST, m_FirmwareList);
}

BEGIN_MESSAGE_MAP(CUpdaterDialog, CDarkDialog)
	ON_BN_CLICKED(IDC_EXTRACT_BUTTON, OnBnClickedExtractFirmware)
	ON_BN_CLICKED(IDC_LOAD_BUTTON, OnBnClickedLoadFirmware)
	ON_BN_CLICKED(IDC_DELETE_BUTTON, OnBnClickedDeleteFirmware)
	ON_BN_CLICKED(IDC_WRITEALL_BUTTON, OnBnClickedWriteAllChanges)
	ON_BN_CLICKED(IDC_USYSINFO_BUTTON, OnBnClickedUpdateSysInfo)
	ON_BN_CLICKED(ID_OPEN, OnBnClickedOpenUpdater)
	ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL CUpdaterDialog::OnInitDialog()
{
	CDarkDialog::OnInitDialog();

	m_FirmwareList.SetExtendedStyle(m_FirmwareList.GetExtendedStyle()|LVS_EX_FULLROWSELECT);
	m_FirmwareList.InsertColumn(0, TEXT("Model"), LVCFMT_LEFT, 120);
	m_FirmwareList.InsertColumn(1, TEXT("Resource Name"), LVCFMT_LEFT, 160);
	m_FirmwareList.InsertColumn(2, TEXT("Length (Bytes)"), LVCFMT_LEFT, 100);

	m_LoadedFirm.RemoveAll();
	m_LoadedFirmStr.RemoveAll();

	// Cache layout metrics for OnSize
	CRect r1, r2, rList;
	GetDlgItem(IDC_EXTRACT_BUTTON)->GetWindowRect(&r1); ScreenToClient(&r1);
	GetDlgItem(ID_OPEN)->GetWindowRect(&r2);            ScreenToClient(&r2);
	m_FirmwareList.GetWindowRect(&rList);               ScreenToClient(&rList);
	m_listTop    = rList.top;
	m_listLeft   = rList.left;
	m_listWidth  = rList.Width();
	m_rowHeight  = r1.Height();
	m_rowGap     = r2.top - r1.bottom;
	m_bottomGap  = m_rowGap;
	m_listRowGap = m_rowGap;

	return TRUE;
}

void CUpdaterDialog::GenerateList(CResourceManager *pRsrcMgr, CString Filename)
{
	m_pRsrcMgr = pRsrcMgr;
	m_Filename.Format(Filename);

	m_FirmwareList.DeleteAllItems();
	int numResources = (int)m_pRsrcMgr->GetNumResources();
	int x=0;
	CString s;

	for (int i = 0; i < numResources; i++)
	{
		CString resName = m_pRsrcMgr->GetResourceName(i);

		// Look up model from INI using key up to second dot (e.g. "IDR_FIRMWARE-10.3")
		CString model;
		int dot1 = resName.Find(_T('.'));
		int dot2 = dot1 >= 0 ? resName.Find(_T('.'), dot1 + 1) : -1;
		if (dot2 > 0)
		{
			TCHAR szModel[512];
			if (GetPrivateProfileString(_T("FirmwareDisplay"), resName.Left(dot2), NULL, szModel, 512, theApp.m_IniPath) > 0)
				model = szModel;
		}
		if (model.IsEmpty())
			model = _T("Unknown");

		m_FirmwareList.InsertItem(x, model);
		m_FirmwareList.SetItemText(x, 1, resName);
		s.Format(TEXT("%d"), m_pRsrcMgr->UpdGetResourceSize(FIRMWARE_RESOURCE_TYPE, resName));
		m_FirmwareList.SetItemText(x, 2, s);
		x++;
	}
}

void CUpdaterDialog::OnOK()
{
	// TODO: Add your specialized code here and/or call the base class

	// CDarkDialog::OnOK();
}

void CUpdaterDialog::OnBnClickedExtractFirmware()
{
	if (m_FirmwareList.GetFirstSelectedItemPosition()==NULL)
	{
		MessageBox(TEXT("You need to select the firmware you want to extract!"));
		return;
	}

	POSITION p=m_FirmwareList.GetFirstSelectedItemPosition();
	int nItem=m_FirmwareList.GetNextSelectedItem(p);
	CString filename;
	filename=m_FirmwareList.GetItemText(nItem, 1);
	if (filename.Right(4).CompareNoCase(_T(".bin")) != 0)
		filename += _T(".bin");
	CFileDialog dlg(FALSE, TEXT("bin"), filename, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, TEXT("Binary Files (*.bin)|*.bin|All Files (*.*)|*.*||"), this);
	MO_LOAD_FIRMWARE_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_FIRMWARE_PATH(dlg)
	LPBYTE buffer=m_pRsrcMgr->UpdLoadResource(FIRMWARE_RESOURCE_TYPE, m_pRsrcMgr->GetResourceName(nItem));
	if (buffer==NULL)
		MessageBox(TEXT("Couldn't load firmware data to memory for extraction!"));
	else
	{
		CFile file;
		if (!file.Open(dlg.GetPathName(), CFile::modeCreate | CFile::modeWrite))
		{
			MessageBox(TEXT("Unable to save file!"));
			return;
		}

		DWORD size;
		CString s;
		s=m_FirmwareList.GetItemText(nItem, 2);
		if (_stscanf(s, TEXT("%d"), &size) == 1)
		{
			file.Write(buffer, size);
			file.Close();
			m_pRsrcMgr->UpdFreeResource();
			MessageBox(TEXT("Extracted firmware successfully!\nWarning: According to Apple's EULA, uploading firmware to 3rd party websites is prohibited! Use this firmware for personal use only!"));
		}
		else
		{
			file.Close();
			MessageBox(TEXT("Error writing file buffer!"));
		}
	}
}

void CUpdaterDialog::OnBnClickedLoadFirmware()
{
	if (m_FirmwareList.GetFirstSelectedItemPosition()==NULL)
	{
		MessageBox(TEXT("You need to select the firmware the new firmware binary will be loaded to!"));
		return;
	}
	
	CString name;
	POSITION p=m_FirmwareList.GetFirstSelectedItemPosition();
	int nItem=m_FirmwareList.GetNextSelectedItem(p);
	name = m_FirmwareList.GetItemText(nItem, 1);
	DWORD size;
	CString s;
	s=m_FirmwareList.GetItemText(nItem, 2);
	if (_stscanf(s, TEXT("%d"), &size) != 1)
	{
		MessageBox(TEXT("Internal memory error!"));
		return;
	}

	for (INT_PTR i=0;i<m_LoadedFirmStr.GetCount();i++)
		if (m_LoadedFirmStr.GetAt(i).Compare(name)==0)
		{
			MessageBox(TEXT("You've already loaded a binary to this firmware!"));
			return;
		}

	//warning
	MessageBox(TEXT("Be aware that the binary file you are about to load must be a valid firmware!\nIf not, you can severely screw your iPod and you can pray for something to fix it.\nEverything should be ok if you extracted the firmware using iPodWizard."));

	CFileDialog dlg(TRUE, NULL, name, OFN_HIDEREADONLY, TEXT("All Files (*.*)|*.*||"), this);
	MO_LOAD_FIRMWARE_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_FIRMWARE_PATH(dlg)
	CFile file;
	if (!file.Open(dlg.GetPathName(), CFile::modeRead))
	{
		MessageBox(TEXT("Unable to load file!"));
		return;
	}

	/*if (size!=file.GetLength())
	{
		file.Close();
		MessageBox(TEXT("The firmware binary size you are about to load has to be exactly like the firmware in the updater!"));
		return;
	}*/
	size=(DWORD)file.GetLength();

	//make firmware checkups
	LPBYTE buf=m_pRsrcMgr->UpdLoadResource(FIRMWARE_RESOURCE_TYPE, m_pRsrcMgr->GetResourceName(nItem));
	if (buf==NULL)
		MessageBox(TEXT("Couldn't load firmware data to memory for checkup!"));

	LPBYTE buffer;
	buffer = new BYTE[size];
	if (file.Read(buffer, size) < size)
	{
		MessageBox(TEXT("Error reading file!"));
		return;
	}

	char ata[4]={0x21,0x41,0x54,0x41};
	IPOD_PARTITION_HEADER *part;
	IPOD_PARTITION_HEADER *part2;
	part=(IPOD_PARTITION_HEADER *)&buffer[theApp.PARTITION_MAP_ADDRESS];
	part2=(IPOD_PARTITION_HEADER *)&buf[theApp.PARTITION_MAP_ADDRESS];
	int num=0;
	DWORD loadAddr=0,loadAddr2=0,offset=0,offset2=0;
	while (memcmp(part[num].type, &ata, 4) == 0 || memcmp(part2[num].type, &nand, 4) == 0)
	{
		if (strcmp(part2[num].id, "soso")==0) // file
		{
			//load address is 0xffff after the firmware is loaded and therefore we need to change it
			offset2=theApp.PARTITION_MAP_ADDRESS + sizeof(IPOD_PARTITION_HEADER) * (num+1) - 4;
			loadAddr2=part2[num].loadAddr;
		}
		if (strcmp(part[num].id, "soso")==0) //real
		{
			//if load address is 0xffff meaning the flash update occured but we need a workign load addr in the firmware so the next time the ipod will flash the firmware and therefore we need to change it
			offset=theApp.PARTITION_MAP_ADDRESS + sizeof(IPOD_PARTITION_HEADER) * (num+1) - 4;
			loadAddr=part[num].loadAddr;
		}
		num++;
	}
	if (offset==0 || offset2==0)
	{
		m_pRsrcMgr->UpdFreeResource();
		file.Close();
		MessageBox(TEXT("Error finding load address offset! It's dangerous to load this kind of firmware and therefore operation is canceled!"));
		return;
	}
	if (loadAddr2==0xFFFFFFFF)
	{
		//that's an iPod extracted firmware
		memcpy(&buffer[offset2],&loadAddr,4);
	}

	file.Close();
	
	m_LoadedFirm.Add(buffer);
	m_LoadedFirmStr.Add(name);
	m_LoadedFirmSize.Add(size);

	MessageBox(TEXT("Successfully loaded firmware binary."));
}

void CUpdaterDialog::OnBnClickedDeleteFirmware()
{
	//TODO:
	//Delete resource binary from PE header because now it only deletes the reference.
	MessageBox(TEXT("This feature is currently not fully supported and therefore disabled."));
	return;
	//

	if (m_FirmwareList.GetFirstSelectedItemPosition()==NULL)
		return;

	CString name;
	POSITION p=m_FirmwareList.GetFirstSelectedItemPosition();
	int nItem=m_FirmwareList.GetNextSelectedItem(p);
	name = m_FirmwareList.GetItemText(nItem, 1);

	INT_PTR i=-1;
	for (i=0;i<m_LoadedFirmStr.GetCount();i++)
		if (m_LoadedFirmStr.GetAt(i).Compare(name)==0)
		{
			if (MessageBox(TEXT("You've already loaded a binary to this firmware you're about to delete.\nIf you choose to delete it, then it won't be loaded like you wanted to in the first place.\nAre you sure you want to delete?"), TEXT("Warning"), MB_YESNO) != IDYES)
				return;
			else
				break;
		}

	if (i!=-1)
	{
		m_LoadedFirm.Add(NULL);
		m_LoadedFirmSize.Add(0);
		m_LoadedFirmStr.Add(name);
	}
	else
	{
		delete[] m_LoadedFirm.GetAt(i);
		m_LoadedFirm.SetAt(i, NULL);
		m_LoadedFirmSize.SetAt(i, 0);
	}
	m_FirmwareList.DeleteItem(nItem);
}

void CUpdaterDialog::OnBnClickedWriteAllChanges()
{
	if (m_LoadedFirm.GetCount()<=0)
	{
		MessageBox(TEXT("You made no changes to the updater and therefore no need to write it."));
		return;
	}
	if (m_pRsrcMgr->UpdChangeResources(m_Filename, FIRMWARE_RESOURCE_TYPE, &m_LoadedFirm, &m_LoadedFirmStr, &m_LoadedFirmSize)==FALSE)
	{
		MessageBox(TEXT("Error writing updater!"));
		return;
	}

	for (INT_PTR i=0;i<m_LoadedFirm.GetCount();i++)
		if (m_LoadedFirm.GetAt(i)!=NULL)
			delete[] m_LoadedFirm.GetAt(i);

	m_LoadedFirm.RemoveAll();
	m_LoadedFirmStr.RemoveAll();

	MessageBox(TEXT("Changes written to the updater file successfully!"));

	if (!m_pRsrcMgr->Open(m_Filename))
	{
		DWORD err = GetLastError();
		CString s;
		s.Format(TEXT("Unable to reopen file! Code=%d"), err);
		MessageBox(s);
	}
}

void CUpdaterDialog::OnBnClickedUpdateSysInfo()
{
	// Forward to the main dialog (grandparent of this child dialog).
	GetParent()->GetParent()->SendMessage(WM_COMMAND,
		MAKEWPARAM(IDC_USYSINFO_BUTTON, BN_CLICKED),
		(LPARAM)GetDlgItem(IDC_USYSINFO_BUTTON)->m_hWnd);
}

void CUpdaterDialog::OnBnClickedOpenUpdater()
{
	GetParent()->GetParent()->SendMessage(WM_COMMAND,
		MAKEWPARAM(ID_OPEN, BN_CLICKED),
		(LPARAM)GetDlgItem(ID_OPEN)->m_hWnd);
}

void CUpdaterDialog::OnSize(UINT nType, int cx, int cy)
{
	CDarkDialog::OnSize(nType, cx, cy);
	if (m_rowHeight == 0 || !IsWindow(m_FirmwareList.m_hWnd)) return;

	int row2Top = cy - m_bottomGap - m_rowHeight;
	int row1Top = row2Top - m_rowGap - m_rowHeight;
	int listH   = max(20, row1Top - m_listRowGap - m_listTop);

	m_FirmwareList.MoveWindow(m_listLeft, m_listTop, m_listWidth, listH);

	static const int row1ids[] = { IDC_EXTRACT_BUTTON, IDC_LOAD_BUTTON, IDC_DELETE_BUTTON };
	static const int row2ids[] = { ID_OPEN, IDC_WRITEALL_BUTTON, IDC_USYSINFO_BUTTON };

	for (int i = 0; i < 3; i++)
	{
		CRect r;
		GetDlgItem(row1ids[i])->GetWindowRect(&r); ScreenToClient(&r);
		GetDlgItem(row1ids[i])->MoveWindow(r.left, row1Top, r.Width(), m_rowHeight);

		GetDlgItem(row2ids[i])->GetWindowRect(&r); ScreenToClient(&r);
		GetDlgItem(row2ids[i])->MoveWindow(r.left, row2Top, r.Width(), m_rowHeight);
	}
}