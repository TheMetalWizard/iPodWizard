#include "stdafx.h"
#include "iPodWizard.h"
#include "iPodWizardDlg.h"
#include "FWExtractorDialog.h"

IMPLEMENT_DYNAMIC(CFWExtractorDialog, CDarkDialog)

CFWExtractorDialog::CFWExtractorDialog(CWnd* pParent /*=NULL*/)
	: CDarkDialog(CFWExtractorDialog::IDD, pParent)
{
}

CFWExtractorDialog::~CFWExtractorDialog()
{
}

void CFWExtractorDialog::DoDataExchange(CDataExchange* pDX)
{
	CDarkDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CFWExtractorDialog, CDarkDialog)
	ON_BN_CLICKED(IDC_EXTRACTIPOD_BUTTON, OnBnClickedExtractiPodFirmware)
END_MESSAGE_MAP()

void CFWExtractorDialog::OnBnClickedExtractiPodFirmware()
{
	CiPodWizardDlg *pMain = (CiPodWizardDlg *)AfxGetMainWnd();
	CString device, volumeName;
	if (pMain == NULL || !pMain->GetSelectedIPod(device, volumeName))
	{
		MessageBox(TEXT("iPod not connected or unmounted!"));
		return;
	}

	CString defName = volumeName.IsEmpty() ? CString(_T("iPod")) : volumeName;
	defName.Replace(_T('\\'), _T('_')); defName.Replace(_T('/'), _T('_')); defName.Replace(_T(':'), _T('_'));
	defName.Replace(_T('*'), _T('_'));  defName.Replace(_T('?'), _T('_')); defName.Replace(_T('"'), _T('_'));
	defName.Replace(_T('<'), _T('_'));  defName.Replace(_T('>'), _T('_')); defName.Replace(_T('|'), _T('_'));

	CFileDialog dlg(FALSE, TEXT("bin"), defName, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, TEXT("Binary Files (*.bin)|*.bin|All Files (*.*)|*.*||"), this);
	MO_LOAD_FIRMWARE_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_FIRMWARE_PATH(dlg)

	// Reading another iPod changes the disk geometry globals that the loaded firmware depends on; restore them on exit.
	struct GeometryGuard
	{
		int block; long start; long map;
		GeometryGuard() : block(theApp.BLOCK_SIZE), start(theApp.FIRMWARE_START), map(theApp.PARTITION_MAP_ADDRESS) {}
		~GeometryGuard() { theApp.BLOCK_SIZE = block; theApp.FIRMWARE_START = start; theApp.PARTITION_MAP_ADDRESS = map; }
	} guard;

	TCHAR devstring[25];
	wsprintf(devstring, TEXT("%s"), (LPCTSTR)device);
	theApp.InitSectorSize(devstring);

	int dev = _wopen(devstring, O_RDONLY | _O_RAW);
	if (dev == -1)
	{
		MessageBox(TEXT("Unable to access iPod. Make sure iPodWizard is running as administrator and iTunes is closed."));
		return;
	}

	theApp.InitHDDValues(dev);

	DWORD size = 0;
	LPBYTE partitions;
	long pos = theApp.FIRMWARE_START + theApp.PARTITION_MAP_ADDRESS;
	pos %= theApp.BLOCK_SIZE;
	_lseek(dev, theApp.FIRMWARE_START + theApp.PARTITION_MAP_ADDRESS - pos, SEEK_SET);
	partitions = new BYTE[theApp.BLOCK_SIZE * 20];
	memset(partitions, 0, theApp.BLOCK_SIZE * 20);
	_read(dev, partitions, theApp.BLOCK_SIZE * 20);
	IPOD_PARTITION_HEADER *m_pParts;
	int num = 0;
	char ata[4] = {0x21, 0x41, 0x54, 0x41};
	m_pParts = (IPOD_PARTITION_HEADER *)(&partitions[pos]);
	while (num < 16 && (memcmp(&m_pParts[num].type, &ata, 4) == 0 || memcmp(&m_pParts[num].type, &nand, 4) == 0))
	{
		if (m_pParts[num].devOffset + m_pParts[num].len > size)
			size = m_pParts[num].devOffset + m_pParts[num].len;
		num++;
	}
	delete[] partitions;

	if (num == 0)
	{
		_close(dev);
		MessageBox(TEXT("Error finding firmware images on the selected iPod."));
		return;
	}

	LPBYTE buffer;
	DWORD i = 0;
	for (i = 0; i < size; i += theApp.BLOCK_SIZE);
	size = i + theApp.BLOCK_SIZE;
	buffer = new BYTE[size];
	memset(buffer, 0, size);
	_lseek(dev, theApp.FIRMWARE_START, SEEK_SET);
	for (i = 0; i < size; i += theApp.BLOCK_SIZE)
		_read(dev, &buffer[i], theApp.BLOCK_SIZE);
	_close(dev);

	CFile file;
	if (!file.Open(dlg.GetPathName(), CFile::modeCreate | CFile::modeWrite))
	{
		delete[] buffer;
		MessageBox(TEXT("Unable to save file!"));
		return;
	}
	file.Write(buffer, size);
	file.Close();
	delete[] buffer;

	MessageBox(TEXT("Successfully saved iPod firmware!\nWarning: According to Apple's EULA, uploading firmware to 3rd party websites is prohibited! Use this firmware for personal use only!"));
}
