// firmware.cpp

#include "stdafx.h"
#include "firmware.h"
#include "Picture.h"
#include "Font.h"
#include "OTFFontParser.h"
#include "iPodWizard.h"

DWORD byte_cap_pos = 0;



#define IS_STRING_CHAR(x)	((x >= ' ' && x <= '~') || x == 0x0D || x == 0x0A || x == 0x09)

/////////////////////////////
//
// CFirmware class
//
/////////////////////////////

// construction

CFirmware::CFirmware()
{
	//reset vars
	m_Buffer = NULL;
	m_BufferSize = 0;
	m_NumPartitions = 0;
	m_SLangOffset = 0;
	m_StringOrderFixed = FALSE;
	m_FlashResetPart = -1;
	m_FlashResetValue = 0;
	m_VersionRaised = FALSE;
}

CFirmware::~CFirmware()
{
	Reset();
}

// implementation

void CFirmware::Reset()
{
	//reset vars
	if (m_Buffer != NULL)
	{
		delete m_Buffer;
	}

	m_Buffer = NULL;
	m_FlashResetPart = -1;                 // a reset of the firmware before does not belong to this one
	m_VersionRaised = FALSE;
	m_BufferSize = 0;

	m_NumPartitions = 0;

	//remove all pointers
	m_Pictures.RemoveAll();
	m_PicturesType.RemoveAll();
	m_PicturesID.clear();
	m_Fonts.RemoveAll();
	m_Strings.RemoveAll();
	m_OTFFonts.RemoveAll();

	for (INT_PTR i=0;i<m_iPodLang.GetCount();i++)
	{
		for (INT_PTR j=0;j<m_iPodLang.GetAt(i)->iPodString.GetCount();j++)
			if (m_iPodLang.GetAt(i)->iPodString.GetAt(j)!=NULL)
				delete m_iPodLang.GetAt(i)->iPodString.GetAt(j);
		if (m_iPodLang.GetAt(i)!=NULL)
			delete m_iPodLang.GetAt(i);
	}
	m_iPodLang.RemoveAll();

	for (size_t i=0;i<ViewRecList.size();i++)
	{
		for (size_t j=0;j<ViewRecList.at(i)->ViewElemList.size();j++)
			if (ViewRecList.at(i)->ViewElemList.at(j)!=NULL)
				delete ViewRecList.at(i)->ViewElemList.at(j);
		if (ViewRecList.at(i)!=NULL)
			delete ViewRecList.at(i);
	}
	ViewRecList.clear();
	for (size_t i=0;i<MenuRecList.size();i++)
	{
		for (size_t j=0;j<MenuRecList.at(i)->MenuElemList.size();j++)
			if (MenuRecList.at(i)->MenuElemList.at(j)!=NULL)
				delete MenuRecList.at(i)->MenuElemList.at(j);
		if (MenuRecList.at(i)!=NULL)
			delete MenuRecList.at(i);
	}
	MenuRecList.clear();
	for (size_t i=0;i<TypeRecList.size();i++)
		if (TypeRecList.at(i)!=NULL)
			delete TypeRecList.at(i);
	TypeRecList.clear();
	for (size_t i=0;i<TCMDRecList.size();i++)
	{
		//for (size_t j=0;j<TCMDRecList.at(i)->element.elements.size();j++)
		//	if (TCMDRecList.at(i)->element.elements.at(j)!=NULL)
		//		delete TCMDRecList.at(i)->element.elements.at(j);
		if (TCMDRecList.at(i)!=NULL)
			delete TCMDRecList.at(i);
	}
	TCMDRecList.clear();
	ScriptRecList.clear();

	m_SLangOffset = 0;
	m_StringOrderFixed = FALSE;
	m_ResBlocks.clear();
}

DWORD CFirmware::GetFirmwareVersion()
{
	if (m_pPartitions)
		return m_pPartitions[0].version;
	else
		return -1;
}

// operations

BOOL CFirmware::SetFirmware(LPCTSTR name, LPBYTE buffer, DWORD size)
{
	// reset
	Reset();

	m_Buffer = new BYTE[size+524288];
	if (m_Buffer == NULL)
		return FALSE;

	//copy resource buffer to a buffer we can use
	memcpy(m_Buffer, buffer, size);

	m_BufferSize = size;

	m_Name = name;

	//char ata[4]={0x21,0x41,0x54,0x41};
	//char ata[4]={0x44,0x4E,0x41,0x4E};
	theApp.CalcPartMapAddr(m_Buffer);
	WORD firmware_version=(WORD)m_Buffer[266]; //0x10A
	CPicture::s_bDisplacedFooters = (firmware_version < 3);
	m_pPartitions = (IPOD_PARTITION_HEADER *)&m_Buffer[theApp.PARTITION_MAP_ADDRESS];
	while (memcmp(&m_pPartitions[m_NumPartitions].type, &ata, 4) == 0 || memcmp(&m_pPartitions[m_NumPartitions].type, &nand, 4) == 0)
	{
		m_PartitionInfo[m_NumPartitions].checksumStartAddress = m_pPartitions[m_NumPartitions].devOffset;
		m_PartitionInfo[m_NumPartitions].checksumExtra = 0;
		m_PartitionInfo[m_NumPartitions].checksumTrim = 0;
		m_PartitionInfo[m_NumPartitions].bSealed = FALSE;

		if (strncmp(m_pPartitions[m_NumPartitions].id, "soso", 4)==0 || strncmp(m_pPartitions[m_NumPartitions].id, "crsr", 4)==0)
		{
			//Known images so far:
			//soso: OS Code with initial resources such as pictures and bitmap fonts
			//crsr: Resources image, 5g and above which contain OpenType Bitmap fonts and video stuff
			m_PartitionInfo[m_NumPartitions].bCanModify = TRUE;

			// check checksum
			//if firmware version is 3 (4g and above models) then 512 bytes from devOffset are random and not included in checksum
			//elsewhere, if it's 2 then it's 1-3g
			if (firmware_version==0 && m_pPartitions[m_NumPartitions].devOffset>=2048)
			{
				//first software (1.0.0, 1.1.0.2, 1.1.0.4): the sum starts 2048 bytes before the image and is as long as the image (measured on all
				//three: [devOffset-2048, +len); in 1.1.0.x the 2048 bytes after it are zeros, in 1.0.0 the next image is there)
				m_PartitionInfo[m_NumPartitions].checksumStartAddress -= 2048;
				m_PartitionInfo[m_NumPartitions].checksumExtra = 2048;
				m_PartitionInfo[m_NumPartitions].checksumTrim = 2048;
			}
			else if (firmware_version!=2) //it's either 3 or we don't know this firmware but heck let's try checksum anyway
				m_PartitionInfo[m_NumPartitions].checksumStartAddress += theApp.BLOCK_SIZE;

			//
			if (strncmp(m_pPartitions[m_NumPartitions].id, "soso", 4)==0)
			{
				DecryptAUPD(m_NumPartitions);
			}
			//

			if (m_pPartitions[m_NumPartitions].checksum != ComputeChecksum(m_NumPartitions))
			{
				//checksum went wrong, meaning don't edit this image
				//m_PartitionInfo[m_NumPartitions].checksumStartAddress = m_pPartitions[m_NumPartitions].devOffset;
				m_PartitionInfo[m_NumPartitions].bCanModify = FALSE;
			}
		}
		else if (strncmp(m_pPartitions[m_NumPartitions].id, "dpua", 4)==0)
		{
			m_PartitionInfo[m_NumPartitions].bCanModify = TRUE;
			if (firmware_version==0 && m_pPartitions[m_NumPartitions].devOffset>=2048)
			{
				// same range as the OS image of that software
				m_PartitionInfo[m_NumPartitions].checksumStartAddress -= 2048;
				m_PartitionInfo[m_NumPartitions].checksumExtra = 2048;
				m_PartitionInfo[m_NumPartitions].checksumTrim = 2048;
				DecryptAUPD(m_NumPartitions);
			}
			else if (firmware_version!=2)
			{
				m_PartitionInfo[m_NumPartitions].checksumStartAddress += theApp.BLOCK_SIZE;
				DecryptAUPD(m_NumPartitions);
			}
		}
		else if (strncmp(m_pPartitions[m_NumPartitions].id, "ebih", 4)==0)
		{
			m_PartitionInfo[m_NumPartitions].bCanModify = TRUE;
			// ebih is never RC4-encrypted; checksum starts at devOffset
			m_PartitionInfo[m_NumPartitions].checksumStartAddress = m_pPartitions[m_NumPartitions].devOffset;
			if (m_pPartitions[m_NumPartitions].checksum != ComputeChecksum(m_NumPartitions))
				m_PartitionInfo[m_NumPartitions].bCanModify = FALSE;
		}
		else
		{
			//we don't know what it is so we better not write it
			m_PartitionInfo[m_NumPartitions].bCanModify = FALSE;
		}
		//LPBYTE teemp=m_Buffer+m_pPartitions[m_NumPartitions].devOffset;

		m_NumPartitions++;
	}

	return TRUE;
}

// THE reflash byte: the "flash" field (the image instance index) of the first "dpua" image of the table. The Tweaks button (FlipReflashByte), Preferences > Advanced "Reflash firmware when
// writing to iPod" (ResetImageAdresses), "Never reflash" (PreventReflash) and the question a write asks (ReflashByteSet) all go through this one function, so they read and change the
// SAME byte. (Checked 2026-11-11 against 1.0.0, 1.1.0.2, 1.1.4, 1.1.5 and the 3G firmwares: every one has exactly one "dpua" entry; format 0 files have their only table at 0x4000, format 2
// files are read and changed in the copy at 0x4200.) -1 when the firmware has no such image.
int CFirmware::ReflashPartition()
{
	for (int i = 0; i < m_NumPartitions; i++)
		if (!strncmp(m_pPartitions[i].id, "dpua", 4))
			return i;
	return -1;
}

// The parts of the firmware file whose damage can brick an iPod: the header with the table of images (the bootloader reads it to find everything) and the "dpua" image (the bootloader, disk mode and the
// updater). A write is read back and compared on these only (CiPodWizardDlg::VerifyBootloader); the main firmware ("soso") is not checked: a bad write there is fixed by writing again, while the bootloader
// has to be right before the iPod is restarted. Offsets are positions in the file / on the firmware partition. The first software (format 0) counts its images from 2048 bytes before their data (see the
// checksum range above), the 512 byte block that later images carry in front of their data is inside [blStart, blEnd) too. false when the firmware has no "dpua" image.
BOOL CFirmware::GetBootloaderRanges(DWORD &hdrEnd, DWORD &blStart, DWORD &blEnd)
{
	int d = ReflashPartition();
	if (d < 0 || m_Buffer == NULL)
		return FALSE;
	const BOOL first = (WORD)m_Buffer[266] == 0;
	DWORD lowest = 0xFFFFFFFF;
	for (int i = 0; i < m_NumPartitions; i++)
	{
		DWORD o = m_pPartitions[i].devOffset;
		if (m_pPartitions[i].len == 0)
			continue;
		if (first && o >= 2048)
			o -= 2048;
		if (o < lowest)
			lowest = o;
	}
	if (lowest == 0xFFFFFFFF)
		return FALSE;
	hdrEnd = lowest;
	DWORD dev = m_pPartitions[d].devOffset;
	blStart = (first && dev >= 2048) ? dev - 2048 : dev;
	blEnd = dev + m_pPartitions[d].len + (theApp.BLOCK_SIZE > 0 ? theApp.BLOCK_SIZE : 512);
	if (blEnd > GetFirmwareSize())
		blEnd = GetFirmwareSize();
	return blStart < blEnd;
}

DWORD CFirmware::GetPartitionVersion(int index)
{
	return (m_pPartitions != NULL && index >= 0 && index < m_NumPartitions) ? m_pPartitions[index].version : 0;
}

// The click: the firmware plays it with sound_request(N, L): "pitch number N for L microseconds"; the sequencer writes N | 0x80800000 to the clicker register 0x7000A000 and arms a timer for L
// microseconds, so the pitch is 93750 / (N + 1) Hz [measured on a 4G]. Every firmware of the 4G family that the archive holds (4G, 4G photo, mini 1G / 2G, nano 1G, 5G 13.x and the 5.5G) has the
// same sound_request, which is found by its first instructions (SIG: the message it builds and the queue 0x15 it puts it in). The CLICK is the only CONDITIONAL call of it with a constant pitch:
// "ldr<c> r1,[pc,#X] / mov<c> r0,#N / ... / bl<c> sound_request" (c = ne in the 4G, mini, photo, nano and 5.5G, eq in the 5G 13.x; the other sounds are unconditional). The stock values are N = 85
// (1090 Hz) and L = 3000. The sites are found by these instructions and not by their values, so a firmware whose click was changed before (any N, any L) is found the same way. Only the "soso"
// image is looked at. pitch: buffer offsets of the mov instructions (the low byte is N); len: buffer offsets of the literals (L). FALSE when there is no such call (the 1G / 2G / 3G have another
// sound code), or more than 4. [the pitch formula was measured on a real 4G; this patch, and every other iPod, are NOT tried on hardware]
// The 3G has the same sound_request (queue 0x14), but its click is "tst r0,#1 / movne r1,#L / movne r0,#N / bne sound_request" with both numbers as immediates (stock N = 100, L = 100; found in
// 2.2.0.0 - 2.2.3 and the Stacker prototype). The 3G's hardware is another chip: the sound task (queue 0x14) writes N as a 16-bit value to the registers 0xC0006040 / 0xC0006044 and holds it for L microseconds of the clock 0xCF001110 [read in the code, not heard]; what N is as a frequency is not known. L is a rotated 8-bit immediate (the low 12 bits of the instruction), so the stock L = 100 is a tick; the dialog writes the rotation too. When this is the form, *pImm is TRUE, len holds the
// offsets of the "movne r1,#L" instructions (the low byte is L) and the readouts of the dialog do not claim a frequency.
BOOL CFirmware::FindClickSites(vector<DWORD> &pitch, vector<DWORD> &len, BOOL *pImm)
{
	if (pImm != NULL)
		*pImm = FALSE;
	if (FindClickSitesSR(pitch, len))
		return TRUE;
	pitch.clear();
	len.clear();
	DWORD start, ilen;
	if (m_Buffer == NULL || !GetImageRange("soso", &start, &ilen))
		return FALSE;
	DWORD end = start + ilen;
	if (end > m_BufferSize)
		end = m_BufferSize;
	DWORD target = 0;
	BOOL have = FALSE;
	for (DWORD o = start & ~3UL; o + 16 <= end; o += 4)
	{
		if (*(DWORD *)&m_Buffer[o] != 0xE3100001)                                // tst r0,#1
			continue;
		DWORD w1 = *(DWORD *)&m_Buffer[o + 4], w2 = *(DWORD *)&m_Buffer[o + 8], w3 = *(DWORD *)&m_Buffer[o + 12];
		if ((w1 & 0xFFFFF000) != 0x13A01000 || (w2 & 0xFFFFF000) != 0x13A00000 || (w3 >> 24) != 0x1A && (w3 >> 24) != 0x1B)    // movne r1,#L / movne r0,#N / bne (or blne)
			continue;
		DWORD tgt = o + 12 + 8 + (DWORD)(((long)((w3 & 0xFFFFFF) << 8)) >> 6);
		if (!have)
		{
			target = tgt;
			have = TRUE;
		}
		else if (tgt != target)
			continue;
		pitch.push_back(o + 8);
		len.push_back(o + 4);
	}
	if (pitch.empty() || pitch.size() > 4)
	{
		pitch.clear();
		len.clear();
		return FALSE;
	}
	if (pImm != NULL)
		*pImm = TRUE;
	return TRUE;
}

// The first form: sound_request with a literal length (4G family, 5G, 5.5G).
BOOL CFirmware::FindClickSitesSR(vector<DWORD> &pitch, vector<DWORD> &len)
{
	static const DWORD SIG[14] = { 0xE92D4010, 0xE24DD020, 0xE1A04001, 0xE3A01000, 0xE1A0E00D, 0xE3A02000, 0xE3A03000, 0xE3A0C000, 0, 0xE3A01000, 0xE5CD1010, 0xE28D1014, 0, 0xE3A00F4B };
	pitch.clear();
	len.clear();
	DWORD start, ilen;
	if (m_Buffer == NULL || !GetImageRange("soso", &start, &ilen))
		return FALSE;
	DWORD end = start + ilen;
	if (end > m_BufferSize)
		end = m_BufferSize;
	start &= ~3UL;
	// sound_request: the words of SIG (0 = any word: the two stm instructions)
	DWORD sr = 0;
	for (DWORD o = start; o + 14 * 4 <= end && sr == 0; o += 4)
	{
		if (*(DWORD *)&m_Buffer[o] != SIG[0])
			continue;
		int k = 1;
		while (k < 14 && (SIG[k] == 0 || *(DWORD *)&m_Buffer[o + 4 * k] == SIG[k]))
			k++;
		if (k == 14)
			sr = o;
	}
	if (sr == 0)
		return FALSE;
	for (DWORD o = start + 16; o + 4 <= end; o += 4)
	{
		DWORD w = *(DWORD *)&m_Buffer[o];
		if ((w & 0x0F000000) != 0x0B000000 || (w >> 28) == 0xE || (w >> 28) == 0xF)       // a conditional bl
			continue;
		DWORD tgt = o + 8 + (DWORD)(((long)((w & 0xFFFFFF) << 8)) >> 6);
		if (tgt != sr)
			continue;
		DWORD cond = w >> 28;
		DWORD mov = 0, lit = 0;
		for (int k = 1; k <= 4; k++)
		{
			DWORD y = *(DWORD *)&m_Buffer[o - 4 * k];
			if ((y >> 28) != cond)
				continue;
			if ((y & 0x0FFFFF00) == 0x03A00000 && mov == 0)                           // mov<c> r0,#N (no rotation)
				mov = o - 4 * k;
			else if ((y & 0x0F7FF000) == 0x051F1000 || (y & 0x0F7FF000) == 0x059F1000)   // ldr<c> r1,[pc,#X]
			{
				DWORD d = y & 0xFFF;
				lit = (y & 0x00800000) ? o - 4 * k + 8 + d : o - 4 * k + 8 - d;
			}
		}
		if (mov == 0 || lit == 0 || lit + 4 > m_BufferSize)
			continue;
		DWORD v = *(DWORD *)&m_Buffer[lit];
		if (v == 0 || v > 1000000)
			continue;
		pitch.push_back(mov);
		len.push_back(lit);
	}
	return !pitch.empty() && pitch.size() <= 4;
}

// Invert the screen of the 4G, the minis, the 1G, 2G and 3G. The LCD of these iPods is inverted by bit 1 of its display control register (register 7), and the firmware writes it from the state of the
// BACKLIGHT (Rockbox: HAVE_BACKLIGHT_INVERSION, the panel looks inverted under the backlight): two conditional instructions, "<eq> r0 = A" (backlight off) and "<ne> r0 = B" (on), A and B differing in bit 1:
// 4G / mini 1G / 2G: "orreq r0,r0,#1 / orrne r0,r0,#3 / and r4,r0,#0xff" (A = 1, B = 3), once; 1G / 2G / 3G: "moveq r0,#9 / movne r0,#0xb / and r2,r0,#0xff / mov r0,#7" (A = 9, B = 0xB), once or twice (the
// 3G 2.2.1 and later have the function that sets the state and the one that redraws). Inverted = the two swapped (eq B / ne A): the screen is inverted with the backlight off and on. Writing only one of them
// (as the first version of this did) changes only one backlight state. The byte that tells the state is the immediate of the first instruction (the other one is changed with it). *pStock = A, *pInverted = B.
// [the bit inverting the screen was tried on a 4G (poke at run time); the backlight dependence is from Rockbox; none of the patches is tried on an iPod yet]
BOOL CFirmware::GetLcdInvertSites(vector<DWORD> &sites, BYTE v[4], DWORD *pInitSite, BOOL *pWide, vector<DWORD> *pSkip, vector<DWORD> *pConst)
{
	pConst->clear();
	*pWide = FALSE;
	pSkip->clear();
	// v[0] / v[1] = the first / second immediate when the screen is NOT inverted, v[2] / v[3] when it is.
	*pInitSite = 0;
	sites.clear();
	DWORD start, len;
	if (m_Buffer == NULL || !GetImageRange("soso", &start, &len))
		return FALSE;
	DWORD end = start + len;
	vector<DWORD> a, b, on, guard;
	for (DWORD o = start & ~3UL; o + 20 <= end; o += 4)
	{
		DWORD w1 = *(DWORD *)&m_Buffer[o], w2 = *(DWORD *)&m_Buffer[o + 4], w3 = *(DWORD *)&m_Buffer[o + 8], w4 = *(DWORD *)&m_Buffer[o + 12];
		// the writes of the register 7 when the display is switched on: "orr r4,r0,#1 / mov r0,#7 / bl lcd_cmd / mov r1,r4" (4G family), as "mov r4,#0x27" once changed
		// the backlight flag setter of the newer builds: "cmp r1,#0 / ldrbne r1,[r1,#5] / cmpne r1,#0 / strne r0,[r2,#0xc] / bne refresh" does nothing when the byte 5 of the LCD module's table entry is 0 (it is, on the
		// minis): "movsne r1,#1" in place of the second compare makes it always redraw the register 7, on every change of the backlight
		if (w1 == 0xE3510000 && w2 == 0x15D11005 && (w3 == 0x13510000 || w3 == 0x13B01001) && w4 == 0x1582000C)
			guard.push_back(o + 8);
		if ((w1 == 0xE3804001 || (w1 & 0xFFFFFF00) == 0xE3A04000) && w2 == 0xE3A00007 && (w3 >> 24) == 0xEB && w4 == 0xE1A01004)
		{
			on.push_back(o);
			// the newer builds: "cmp r4,#0 / beq skip / ldrb r0,[r7] / orr r4,r0,#1": the init of the display, called once at boot with r4 = 0, skips the write of the register 7 (the bootloader has lit the
			// display already): a nop in place of the beq makes it write it (the inverted value) at boot too
			if (o >= start + 12 && *(DWORD *)&m_Buffer[o - 12] == 0xE3540000 && (*(DWORD *)&m_Buffer[o - 8] == 0x0A000006 || *(DWORD *)&m_Buffer[o - 8] == 0xE1A00000) && (*(DWORD *)&m_Buffer[o - 4] & 0xFFF00FFF) == 0xE5D00000)
				pSkip->push_back(o - 8);
		}
		if (((w1 == 0x03800001 && w2 == 0x13800003) || ((w1 & 0xFFFFFF00) == 0x03A00000 && w2 == (w1 | 0x10000000))) && w3 == 0xE20040FF)       // orreq #1 / orrne #3 (stock), or moveq / movne #0x27 (inverted)
			a.push_back(o);
		else if ((((w1 == 0x03A00009 || w1 == 0x03A00027) && (w2 == 0x13A0000B || w2 == 0x13A00019 || w2 == 0x13A00027)) || ((w1 == 0x13A0000B || w1 == 0x13A00019 || w1 == 0x13A00027) && (w2 == 0x03A00009 || w2 == 0x03A00027)))
			&& w3 == 0xE20020FF && w4 == 0xE3A00007)       // moveq r0,#9 / movne r0,#0xb in either order (the 3G 2.2.0.x have the ne first), "and r2,r0,#0xff / mov r0,#7"; #0x27 / #0x19 once changed
			b.push_back(o);
		// the other writes of the register 7 as a constant (1G, 2G and the 3G 2.2.1 and later: after the write of the contrast): "mov r2,#9 / mov r1,#0 / mov r0,#7 / pop {...} / b"
		if ((w1 == 0xE3A02009 || w1 == 0xE3A02027) && w2 == 0xE3A01000 && w3 == 0xE3A00007 && (w4 & 0xFFFF0000) == 0xE8BD0000 && (*(DWORD *)&m_Buffer[o + 16] >> 24) == 0xEA)
			pConst->push_back(o);
	}
	if (a.size() == 1 && b.empty())
	{
		sites = a;
		for (size_t i = 0; i < on.size() && i < 3; i++)
			sites.push_back(on[i]);                                          // after the first: the display-on writes ("orr r4,r0,#1" <-> "mov r4,#0x27")
		if (guard.size() == 1)
			*pInitSite = guard[0];                                          // (for the 4G family: the guard of the backlight flag setter)
		*pWide = TRUE;                                                      // the two instructions are replaced as a whole (4 bytes each): "orreq r0,r0,#1 / orrne r0,r0,#3" <-> "moveq r0,#0x27 / movne r0,#0x27"
		return TRUE;
	}
	if (a.empty() && b.size() >= 1 && b.size() <= 2)
	{
		sites = b;
		v[0] = 9; v[1] = 0xB; v[2] = 0x27; v[3] = 0x19;                      // Rockbox: 0x19 / 0x27 are the register 7 values of the panel for normal / inverted
		// the guard of the backlight flag function: "cmp rA,#0 / ldrbne rB,[rA,#N] / cmpne rB,#0 / popeq or moveq pc,lr" (before the two conditional instructions) leaves the function when the byte N of the
		// table entry of the LCD module is 0, and then the register 7 is never written when the backlight changes (the 3G 2.2.0.x did not invert): "movsne rB,#1" in place of the cmpne
		// (only for the builds without the constant writes: the 3G 2.2.0.x / the Stacker prototype; the 2.2.1 and later and the 1G / 2G are left as they were, 2.2.3 inverts on a real 3G)
		for (size_t k = 0; k < b.size() && *pInitSite == 0 && pConst->empty(); k++)
		{
			for (DWORD o = b[k] - 4; o >= b[k] - 64 && o > start; o -= 4)
			{
				DWORD g1 = *(DWORD *)&m_Buffer[o], g2 = *(DWORD *)&m_Buffer[o + 4], g3 = *(DWORD *)&m_Buffer[o + 8];
				DWORD rd = (g1 >> 12) & 15;
				if ((g1 & 0xFFF00000) == 0x15D00000 && (g2 == (0x13500000 | (rd << 16)) || g2 == (0x13B00001 | (rd << 12))) && (g3 >> 28) == 0)
				{
					*pInitSite = o + 4;
					break;
				}
			}
		}
		return TRUE;
	}
	return FALSE;
}

// Preferences > Advanced "Reflash firmware when writing to iPod" (and a firmware whose own byte says reflash): the byte becomes 0 for one write (RestoreImageAdresses takes it back) and, for the
// first software, the table versions are raised so the bootloader runs the updater at all (RaiseUpdaterVersions).
void CFirmware::ResetImageAdresses()
{
	int i = ReflashPartition();
	if (i < 0 || !m_PartitionInfo[i].bCanModify)
		return;
	if (m_FlashResetPart < 0)                       // what it was is kept, for RestoreImageAdresses
	{
		m_FlashResetPart = i;
		m_FlashResetValue = m_pPartitions[i].flash;
	}
	m_pPartitions[i].flash = 0;
	RaiseUpdaterVersions();
}

// The first software of the 1st and 2nd generation (1.0.0, 1.1.0.2, 1.1.0.4, 1.1.1) has 0 as the version of its images in the table. The bootloader
// of an iPod only runs the updater image of a firmware when that version is at least the lowest one its hardware accepts (0x112 and 0x121 for the
// 1st generation hardware, 0x120 and 0x121 for the 2nd; the 3rd generation and later ask for 0x200, which their own firmwares carry), so a reflash
// of such a firmware was ignored. For the time of a write that reflashes, the version is raised to 0x130 (what 1.1.3 to 1.1.5 carry) for the images
// that are below it; RestoreImageAdresses puts the values back. Firmwares of the generations that carry real versions are not touched.
void CFirmware::RaiseUpdaterVersions()
{
	if (m_Buffer == NULL || m_VersionRaised)
		return;
	WORD fmt = (WORD)m_Buffer[266];
	if (fmt != 0 && fmt != 2)
		return;
	static const DWORD kMinVersion = 0x130;
	for (int i = 0; i < m_NumPartitions && i < 10; i++)
	{
		BOOL oneOfOurs = strncmp(m_pPartitions[i].id, "dpua", 4) == 0 || strncmp(m_pPartitions[i].id, "soso", 4) == 0;
		if (oneOfOurs && m_PartitionInfo[i].bCanModify && m_pPartitions[i].version < kMinVersion)
		{
			m_VersionReset[i] = m_pPartitions[i].version;
			m_pPartitions[i].version = kMinVersion;
			m_VersionRaised = TRUE;
		}
		else
			m_VersionReset[i] = 0xFFFFFFFF;
	}
}

// The firmware files of Apple have 0 in the flash address of the "dpua" image, so the iPod flashes it at the next start. When no
// flash is wanted the byte is overwritten with 1 (what a firmware read from an iPod that has flashed has), for one write.
BOOL CFirmware::ReflashByteSet()
{
	int i = ReflashPartition();
	return i < 0 ? TRUE : m_pPartitions[i].flash != 0;
}

BOOL CFirmware::CanFlipReflashByte()
{
	int i = ReflashPartition();
	return i >= 0 && m_PartitionInfo[i].bCanModify;
}

void CFirmware::FlipReflashByte()
{
	int i = ReflashPartition();
	if (i < 0 || !m_PartitionInfo[i].bCanModify)
		return;
	RestoreImageAdresses();                         // a change made for one write is not part of this
	m_pPartitions[i].flash = m_pPartitions[i].flash ? 0 : 1;
}

// The image offsets of the first software (format 0: 1.0.0, 1.1.0.2, 1.1.0.4) are positions on the disk that count from a firmware partition at
// byte 2048 (the file offset of an image + 2048), the layout the Mac iPods had. The bootloader follows them as they are, so on an iPod whose
// firmware partition starts elsewhere (block 63 = byte 32256 after a restore) the table has to move by the difference or the iPod does not
// boot it (shown on 1.0.0: moving the offset of the "soso" image by 30208 made it start). The firmware that is loaded and saved keeps the
// 2048 form; only the copy that is written to an iPod is moved, and a firmware that is read from an iPod is moved back.
// Later software (format 2) carries a second table with offsets that are relative to the partition and needs nothing.
long CFirmware::DiskOffsetDelta(const BYTE *buf, long partitionStart)
{
	if (buf == NULL || buf[0x10A] != 0 || buf[0x10B] != 0 || partitionStart <= 0 || partitionStart == 2048)
		return 0;
	return partitionStart - 2048;
}

BOOL CFirmware::RebaseOffsets(LPBYTE buf, long delta)
{
	if (buf == NULL || delta == 0)
		return FALSE;
	theApp.CalcPartMapAddr(buf);
	IPOD_PARTITION_HEADER *p = (IPOD_PARTITION_HEADER *)&buf[theApp.PARTITION_MAP_ADDRESS];
	int n = 0;
	DWORD lowest = 0xFFFFFFFF;
	while (n < 10 && (memcmp(&p[n].type, &ata, 4) == 0 || memcmp(&p[n].type, &nand, 4) == 0))
	{
		if (p[n].len != 0 && p[n].devOffset < lowest)
			lowest = p[n].devOffset;
		n++;
	}
	if (n == 0 || lowest == 0xFFFFFFFF)
		return FALSE;
	if (delta < 0 && lowest < (DWORD)(-delta) + 2048)       // not in the moved form (a firmware that was written without the move): left as it is
		return FALSE;
	for (int i = 0; i < n; i++)
		if (p[i].len != 0)
			p[i].devOffset += delta;
	return TRUE;
}

// Preferences > Advanced "Never reflash": the same byte as the Tweaks button, 0 -> 1 for one write. (It does not look at bCanModify: keeping an iPod from flashing is always safe.)
void CFirmware::PreventReflash()
{
	int i = ReflashPartition();
	if (i < 0 || m_pPartitions[i].flash != 0)
		return;
	if (m_FlashResetPart < 0)
	{
		m_FlashResetPart = i;
		m_FlashResetValue = 0;
	}
	m_pPartitions[i].flash = 1;
}

// The iPod flashes a partition again when its flash address is 0. A reset that was made for one write must not stay in the
// firmware, or every later write would flash the iPod again.
void CFirmware::RestoreImageAdresses()
{
	if (m_FlashResetPart >= 0 && m_FlashResetPart < m_NumPartitions)
		m_pPartitions[m_FlashResetPart].flash = m_FlashResetValue;
	m_FlashResetPart = -1;
	if (m_VersionRaised)                                      // (the versions raised for the reflash of a first software firmware go back)
	{
		for (int i = 0; i < m_NumPartitions && i < 10; i++)
			if (m_VersionReset[i] != 0xFFFFFFFF)
				m_pPartitions[i].version = m_VersionReset[i];
		m_VersionRaised = FALSE;
	}
}

BOOL CFirmware::DecryptAUPD(int partition)
{
	LPBYTE offset_start;
	offset_start=m_Buffer+m_pPartitions[partition].devOffset;
	SecurityBlock sb(offset_start);
	sb.GetSecurityBlockKey(offset_start);

	if (sb.fileIsProtected==true)
	{
		DWORD size=m_pPartitions[partition].len;
		unsigned char *decrypted_data;

		FileDecrypt fd;
		sb.key=SWAPLONG_L((unsigned long)sb.key);
		decrypted_data=fd.decryptFirmwareFile(&offset_start[theApp.BLOCK_SIZE], sb.key, size);

		memcpy(&offset_start[theApp.BLOCK_SIZE], decrypted_data, size);
		return TRUE;
	}
	return FALSE;
}

void CFirmware::FixChecksum(DWORD imageID)
{
	if (!strncmp(m_pPartitions[imageID].id, "dpua", 4))
		return;

	if (m_PartitionInfo[imageID].bCanModify==TRUE)
		//no need to fix
		return;

	m_pPartitions[imageID].checksum=ComputeChecksum(imageID);
}

//VOLUME HACK

int CFirmware::isCapped(void)
{
	//We search for this byte combination in firmware and after it two bytes decide either cap or not
	BYTE temp[8]={0x8,0x0,0x9F,0xE5,0x38,0x1,0x90,0xE5}; //3/4G + Color + mini
	BYTE temp2[8]={0x8,0x0,0x9F,0xE5,0x94,0x0,0x90,0xE5}; //nano + 5G
	for (byte_cap_pos = 0; byte_cap_pos < (theApp.BLOCK_SIZE * 2000) - 2; byte_cap_pos++)
	{
	    if (memcmp(&m_Buffer[byte_cap_pos], &temp, 8)==0 || memcmp(&m_Buffer[byte_cap_pos], &temp2, 8)==0)
		{
			//if (byte_cap_pos > theApp.BLOCK_SIZE - 3)
            //   break;
			if ((m_Buffer[byte_cap_pos+8] == 0x1 && m_Buffer[byte_cap_pos+9] == 0x0) || (m_Buffer[byte_cap_pos+8] == 0x0 && m_Buffer[byte_cap_pos+9] == 0x1))
			{
				byte_cap_pos+=8;
				goto found;
			}
            else
				continue;
         }
    }
   
	return -1;

found: //check if the ipod is capped or not
	if(m_Buffer[byte_cap_pos] == 0x1 && m_Buffer[byte_cap_pos+1] == 0x0)
		return 1; //capped

    if(m_Buffer[byte_cap_pos] == 0x0 && m_Buffer[byte_cap_pos+1] == 0x1)
		return 0; //uncapped

	return -1;
}

DWORD CFirmware::unCap(void)
{
   m_Buffer[byte_cap_pos]=0x0;
   m_Buffer[byte_cap_pos+1]=0x1;
   
   if(!(m_Buffer[byte_cap_pos] == 0x0 && m_Buffer[byte_cap_pos+1] == 0x1))
   {
      MessageBox(0, TEXT("Failed to uncap your iPod.\nPlease do not write this modded firmware unless you want to screw your iPod."), TEXT("Failed to uncap"), MB_OK);
	  return 0;
   }
   else
   {
      MessageBox(0, TEXT("Successfully uncapped your iPod!"), TEXT("Success"), MB_OK);
	  return byte_cap_pos;
   }
   return -1;
}

DWORD CFirmware::reCap(void)
{
   m_Buffer[byte_cap_pos]=0x1;
   m_Buffer[byte_cap_pos+1]=0x0;
  
   if(m_Buffer[byte_cap_pos] == 0x1 && m_Buffer[byte_cap_pos+1] == 0x0)
   {
      MessageBox(0, TEXT("Successfully recapped your iPod!"), TEXT("Success"), MB_OK);
	  return byte_cap_pos;
   }
   else
   {
      MessageBox(0, TEXT("Failed to cap your iPod.\nPlease do not write this modded firmware unless you want to screw your iPod."), TEXT("Failed to cap"), MB_OK);
	  return 0;
   }
   return -1;
}
//VOLUME HACK

//Scan firmware for all the resources we may find and edit
void CFirmware::ScanFirmware(CWnd *pWnd)
{
	DWORD i;
	int j;
	DWORD startOffset,endOffset;
	CPicture pic;
	CIpodFont font;
	COTFFont ofont;

	// scan firmware for regular fonts

	pWnd->SendMessage(WM_APP, (WPARAM)TEXT("Scanning for fonts..."), 0);

	
	for (j=0;j<m_NumPartitions;j++)
	{
		if (strncmp(m_pPartitions[j].id,"soso",4)!=0 && strncmp(m_pPartitions[j].id,"crsr",4)!=0)
			continue;

		startOffset=m_pPartitions[j].devOffset;
		endOffset=startOffset+m_pPartitions[j].len;
		LPBYTE lpEnd = m_Buffer + endOffset;

		for (i = startOffset; i < endOffset; i+= 4)
		{
			if (font.Read(&m_Buffer[i], lpEnd))
			{
				m_Fonts.Add(&m_Buffer[i]);
				
				/*if (m_Fonts.GetCount()==39)
				{
					CFile temp;
					temp.Open(TEXT("D:\\iTunes Music\\israPod Update\\res\\color_font.bin"), CFile::modeCreate | CFile::modeWrite);
					temp.Write(&m_Buffer[i], font.GetFontBlockLen());
					temp.Close();
				}*/
				
				i += font.GetFontBlockLen() - 4;
				if ((i % 10) == 0)
					pWnd->SendMessage(WM_APP, 0, i);
			}
		}
	}

	pWnd->SendMessage(WM_APP, (WPARAM)TEXT("Scanning for pictures and strings..."), 0);
	
	for (j=0;j<m_NumPartitions;j++)
	{
		if (strncmp(m_pPartitions[j].id,"soso",4)!=0) //this sources will be only in OS image that's why we can skip others
			continue;

		startOffset=m_pPartitions[j].devOffset; //start offset of image in firmware
		endOffset=startOffset+m_pPartitions[j].len; //end offset of image in firmware

		int iFirst=0;
		RESOURCE_HEADER	*pRsHeader;
		char type[4];
		LPBYTE lpEnd = m_Buffer + endOffset;
	
		int y=0;
		for (i = startOffset; i < endOffset; i+= 4)
		{
			pRsHeader=(RESOURCE_HEADER *)&m_Buffer[i];
			memcpy(&type,&m_Buffer[i+sizeof(RESOURCE_HEADER)],4);
			//validate resource header
			if (KNOWN_RESOURCE(type) && pRsHeader->z1==0x03 && pRsHeader->length < 0xffff && pRsHeader->length > 0 && pRsHeader->num_sections < 50 && pRsHeader->num_sections > 0)
			{

				//each resource block is built by resource sections that has the resources in them
				DWORD data_size=0,rs_offset;
				rs_offset=i;
				i+=sizeof(RESOURCE_HEADER);
				RESOURCE_SECTION *pRsSection;
				pRsSection=(RESOURCE_SECTION *)&m_Buffer[i];
				DWORD k,x,lastend=0,total=0;
				
				//calculate the total bytes of section tables
				for (k=0;k<pRsHeader->num_sections;k++)
					total+=pRsSection[k].num_items;

				i+=sizeof(RESOURCE_SECTION)*pRsHeader->num_sections+sizeof(RESOURCE)*total; //data begin
				{ ResBlockInfo rb; rb.offset = rs_offset; rb.dataBegin = i; rb.numSections = pRsHeader->num_sections; m_ResBlocks.push_back(rb); }

				IPOD_LANG *il=new IPOD_LANG;
				BOOL bString=FALSE;
				RESOURCE *pRs;

				//########################################################
				/*
				DWORD pic_section=-1;
				for (k=0;k<pRsHeader->num_sections;k++)
				{
					if (!strncmp(pRsSection[k].type, "paMB", 4))
						pic_section=k;
				}
				if (pic_section!=-1)
				{
					int fntid=1;
					DWORD ppindex=pRsSection[pic_section].num_items-1;
					ppindex=140;
					pRs=(RESOURCE *)&m_Buffer[rs_offset+pRsSection[pic_section].start];

					LPBYTE lpEnd=m_Buffer+m_BufferSize;
					DWORD change=0;
					LPBYTE target;
					
					pRs[ppindex].offset=(DWORD)((LPBYTE)m_Fonts.GetAt(fntid)-m_Buffer)-i;

					
					//DWORD im_size=0;
					
					//create image buffer:
					DWORD im_width=320;
					DWORD im_height=240;
					DWORD im_size=sizeof(IPOD_PICTURE_HEADER2)+im_width*im_height*16/8;
					pRs[ppindex].size=im_size;
					LPBYTE image_buff=new BYTE[im_size];
					IPOD_PICTURE_HEADER2 *im_header=(IPOD_PICTURE_HEADER2 *)image_buff;
					memset(im_header, 0, im_size);
					im_header->texture_format=0x2565;
					im_header->d5=0;
					im_header->bytes_per_row=im_width*16/8;
					im_header->bitDepth=16;
					im_header->height=im_height;
					im_header->width=im_width;
					im_header->blockLen=im_width*im_height*16/8;

					change=im_size;
					target=(LPBYTE)m_Fonts.GetAt(fntid);
					memcpy(target, image_buff, im_size);
					//m_BufferSize+=change;
					//lpEnd=m_Buffer+m_BufferSize;
				}
				*/
				//########################################################
				

				for (k=0;k<pRsHeader->num_sections;k++)
				{
					pRs=(RESOURCE *)&m_Buffer[rs_offset+pRsSection[k].start];
					//TRACE(_T("Section %d, type %S\n"), k, pRsSection[k].type);
					for (x=0;x<pRsSection[k].num_items;x++)
					{
						//if (lastend && pRs[x].offset != lastend)
							//	warning
						//{
						//	i+=lastend-pRs[x].offset;
							//continue;
						//}
						//lastend = DWORD_ALIGN(pRs[x].offset+pRs[x].size);
						//TRACE(_T("ID %d, offset %d, size %d\n"), pRs[x].id, pRs[x].offset, pRs[x].size);
						int *num_elem;
						int q;
						struct resource_type *res_type;
						
						if (!strncmp(pRsSection[k].type, "paMB", 4)) //pictures
						{
							if (pic.Read(&m_Buffer[i+pRs[x].offset], TRUE))
							{
								//add data pointer for picture and picture type (0=changable)
								m_Pictures.Add(&m_Buffer[i+pRs[x].offset]);
								m_PicturesType.Add(0);
								m_PicturesID.push_back(pRs[x].id);
								//if (m_PicturesType.GetCount()==264)
								//{
								//	TCHAR bb[10];
								//	wsprintf(bb, _T("%d"), i+pRs[x].offset);
									//MessageBox(0, bb, _T("a"), 0);
								//}
								pWnd->SendMessage(WM_APP, 0, i);
							}
						}
						else if (!strncmp(pRsSection[k].type, "boot", 4)) //boot pictures - not working because encoded
						{
							pic.SetPictureMode(1);
							if (pic.Read(&m_Buffer[i+pRs[x].offset], TRUE))
							{
								m_Pictures.Add(&m_Buffer[i+pRs[x].offset]);
								m_PicturesType.Add(1);
								m_PicturesID.push_back(pRs[x].offset);
								pWnd->SendMessage(WM_APP, 0, i);
							}
						}
						else if (!strncmp(pRsSection[k].type, " rtS", 4)) //language strings
						{
							IPOD_STRING *is=new IPOD_STRING;
							is->id=pRs[x].id;
							is->offset=i+pRs[x].offset;
							is->size=pRs[x].size;
							is->string_off=&pRs[x].offset;
							is->size_off=&pRs[x].size;
							if (is->offset<m_BufferSize)
								il->iPodString.Add(is);
							bString=TRUE;
							//p=k;
						}
						else if (!strncmp(pRsSection[k].type, "weiV", 4)) //layout
						{
							struct resource_type_view_elem *view;
							LPBYTE lpViewPos=&m_Buffer[i+pRs[x].offset];
							ViewElem *view_elem;
							ViewRecord *view_item;

							num_elem=(int *)lpViewPos;
							lpViewPos+=4;
							//if (*num_elem > MAX_ELEMENTS)
							//	error("Too many elements (%d) in %s", num_elem, file);


							view_item=new ViewRecord;
							view_item->num_elems=*num_elem;
							view_item->id=pRs[x].id;

							for (q = 0; q < *num_elem; q++)
							{
								view=(resource_type_view_elem *)lpViewPos;

								//res_type = get_resource_type(view->type);

								// some have extra fields, different number doesn't seem to be any
								// consistent pattern*/

								//lpViewPos+=sizeof(resource_type_view_elem);
								lpViewPos+=view->size+4;

								// +4 for the length of the size field which isn't included in the size
								//for (q = 0; q < (int)(view->size+4-sizeof(view))/4; q++)
								//{
								//	t1=(unsigned int *)lpViewPos;
								//	lpViewPos+=4;
								//	//add_property(props, item, HEX, t1, NULL);
								//}
								
								view_elem=new ViewElem;
								view_elem->next=NULL;
								view_elem->element=view;
								view_item->ViewElemList.push_back(view_elem);
							}
							ViewRecList.push_back(view_item);
						}
						else if (!strncmp(pRsSection[k].type, "uneM", 4)) //layout
						{
							struct resource_type_menu_elem *menu;
							LPBYTE lpMenuPos=&m_Buffer[i+pRs[x].offset];
							MenuElem *menu_elem;
							MenuRecord *menu_item;

							num_elem=(int *)lpMenuPos;
							lpMenuPos+=4;
							//if (*num_elem > MAX_ELEMENTS)
							//	error("Too many elements (%d) in %s", num_elem, file);


							menu_item=new MenuRecord;
							menu_item->num_elems=*num_elem;
							menu_item->id=pRs[x].id;

							for (q = 0; q < *num_elem; q++)
							{
								menu=(resource_type_menu_elem *)lpMenuPos;

								//res_type = get_resource_type(view->type);

								// some have extra fields, different number doesn't seem to be any
								// consistent pattern*/

								//lpViewPos+=sizeof(resource_type_view_elem);
								lpMenuPos+=menu->size+4;

								// +4 for the length of the size field which isn't included in the size
								//for (q = 0; q < (int)(view->size+4-sizeof(view))/4; q++)
								//{
								//	t1=(unsigned int *)lpViewPos;
								//	lpViewPos+=4;
								//	//add_property(props, item, HEX, t1, NULL);
								//}
								
								menu_elem=new MenuElem;
								//menu_elem->next=NULL;
								menu_elem->element=menu;
								menu_item->MenuElemList.push_back(menu_elem);
							}
							MenuRecList.push_back(menu_item);
						}
						else if (!strncmp(pRsSection[k].type, "epyT", 4)) //layout
						{
							struct resource_type_type_elem *type;
							LPBYTE lpTypePos=&m_Buffer[i+pRs[x].offset];
							TypeRecord *type_item;

							type_item=new TypeRecord;
							type_item->id=pRs[x].id;

							type=(resource_type_type_elem *)lpTypePos;

							lpTypePos+=type->size+4;

							type_item->element=type;
								
							TypeRecList.push_back(type_item);
						}
						else if (!strncmp(pRsSection[k].type, "dmCT", 4)) //layout
						{
							struct resource_type_tcmd tcmd;
							LPBYTE lpTCMDPos=&m_Buffer[i+pRs[x].offset];
							TCMDRecord *tcmd_item;

							// keep the resource as it is, for the script reader (the parse below only copes with one layout)
							if (pRs[x].size>=16 && (ULONGLONG)i+pRs[x].offset+pRs[x].size<=m_BufferSize)
							{
								ScriptRecord script;
								script.id=pRs[x].id;
								script.data=lpTCMDPos;
								script.size=pRs[x].size;
								ScriptRecList.push_back(script);
							}

							tcmd_item=new TCMDRecord;
							tcmd_item->id=pRs[x].id;

							tcmd.size_unk=(unsigned int *)lpTCMDPos;
							lpTCMDPos+=4;
							tcmd.z1=(unsigned short *)lpTCMDPos;
							if (*tcmd.z1==0)
							{
								lpTCMDPos+=2;
								tcmd.header_size=(unsigned short *)lpTCMDPos;
								lpTCMDPos+=2;
								tcmd.elems=(int *)lpTCMDPos;
								lpTCMDPos+=(*tcmd.header_size)*4;
								tcmd.num_offsets=(unsigned short *)lpTCMDPos;
								lpTCMDPos+=2;
								tcmd.t_len=(unsigned short *)lpTCMDPos;
								lpTCMDPos+=2;
								tcmd.tlist=(unsigned short *)lpTCMDPos;
								lpTCMDPos+=(*tcmd.t_len)*2;
								if ((*tcmd.t_len)%2!=0)
									lpTCMDPos+=2;
								tcmd.offsets=(unsigned int *)lpTCMDPos;
								lpTCMDPos+=(*tcmd.num_offsets)*4;
								lpTCMDPos+=4; //add last offset (not used but for aligning)
								LPBYTE save;
								for (unsigned short tidx=0;tidx<(*tcmd.num_offsets);tidx++)
								{
									save=lpTCMDPos+tcmd.offsets[tidx];
									tcmd.elements.push_back((resource_type_tcmd_elem *)save);
								}

								tcmd_item->element=tcmd;
								
								TCMDRecList.push_back(tcmd_item);
							}
							else
							{

							}
						}
						else if (!strncmp(pRsSection[k].type, "mTDL", 4)) //layout
						{
							//not supported
							//looks boring
						}
						//if ((pRs[x].offset+pRs[x].size) > data_size)
						//	data_size=pRs[x].offset+pRs[x].size;
						data_size+=pRs[x].size;
					}
				}

				if (bString)
				{
					IPOD_STRING *is_last;
					is_last=il->iPodString.GetAt(il->iPodString.GetCount()-1);
					il->rsdata_end=is_last->offset+is_last->size;
					il->rsdata_start=i;
					m_iPodLang.Add(il);
				}
				i+=data_size;
				pWnd->SendMessage(WM_APP, 0, i);
				i=DWORD_ALIGN(i);
			}
		}
	}

	//if (m_BufferSize%theApp.BLOCK_SIZE!=0)
	//	m_BufferSize=(m_BufferSize/theApp.BLOCK_SIZE)*theApp.BLOCK_SIZE+theApp.BLOCK_SIZE;
	
	//Link connections between view elements:
	DWORD k;
	bool foundid;
	for (i=0;i<ViewRecList.size();i++)
	{
		for (j=0;j<ViewRecList[i]->num_elems;j++)
		{
			foundid=false;
			for (k=0;k<ViewRecList.size();k++)
				if (ViewRecList[k]->id == ViewRecList[i]->ViewElemList[j]->element->id)
				{
					ViewRecList[i]->ViewElemList[j]->next=ViewRecList[k];
					foundid=true;
				}
			if (foundid==false)
				ViewRecList[i]->ViewElemList[j]->next=NULL;
		}
	}

	WORD firmware_version=(WORD)m_Buffer[266]; //0x10A
	//if (firmware_version==2) //below 4g (not encrypted apple update image
	//{
	
		// scan firmware for boot pictures
		pWnd->SendMessage(WM_APP, (WPARAM)TEXT("Scanning for boot pictures..."), 0);

		pic.SetPictureMode(1);
		for (j=0;j<m_NumPartitions;j++)
		{
			BOOL isDpua = strncmp(m_pPartitions[j].id,"dpua", 4)==0;
			BOOL isEbih = strncmp(m_pPartitions[j].id,"ebih", 4)==0;
			if (!isDpua && !isEbih)
				continue;

			startOffset=m_pPartitions[j].devOffset;
			endOffset=startOffset+m_pPartitions[j].len;
			// dpua has a security block before image data; ebih does not
			LPBYTE lpImageOffset=m_Buffer+startOffset+(isDpua ? (firmware_version==0 && startOffset>=2048 ? -2048 : theApp.BLOCK_SIZE) : 0);
		
			DWORD r=0;
			for (i = startOffset; i < endOffset; i+= 2)
			{
				if (pic.Read(&m_Buffer[i], TRUE, lpImageOffset))
				{
					m_Pictures.Add(&m_Buffer[i]);
					m_PicturesType.Add(1);
					m_PicturesID.push_back(r);
					r++;
					i += 4;
					if ((i % 100) == 0)
						pWnd->SendMessage(WM_APP, 0, i);
				}
			}
		}
	//}

	//scan firmware for OTF fonts

	pWnd->SendMessage(WM_APP, (WPARAM)TEXT("Scanning for OpenType fonts..."), 0);

	for (j=0;j<m_NumPartitions;j++)
	{
		if (strncmp(m_pPartitions[j].id,"crsr",4)!=0)
			continue;

		startOffset=m_pPartitions[j].devOffset;
		endOffset=startOffset+m_pPartitions[j].len;
	
		for (i = startOffset; i < endOffset; i+= 4)
		{
			if (ofont.Read(&m_Buffer[i], FALSE))
			{
				m_OTFFonts.Add(&m_Buffer[i]);
				i += DWORD_ALIGN(ofont.GetFontBlockLen()) - 4;
				if ((i % 10) == 0)
					pWnd->SendMessage(WM_APP, 0, i);
			}
		}
	}

	// scan firmware for strings
	if (theApp.m_AllStringsRender)
		return;

	pWnd->SendMessage(WM_APP, (WPARAM)TEXT("Scanning for all strings..."), 0);

	BYTE c;
	BOOL bInString = FALSE;
	DWORD stringStart;

	for (j=0;j<m_NumPartitions;j++)
	{
		if (!strncmp(m_pPartitions[j].id,"ebih",4)!=0)
			continue;

		startOffset=m_pPartitions[j].devOffset;
		endOffset=startOffset+m_pPartitions[j].len;
	
		for (i = startOffset; i < endOffset; i++)
		{
			c = m_Buffer[i];
			if (bInString)
			{
				if (c == 0)
				{
					if ((i - stringStart) >= 3) // at least 3 chars
					{
						m_Strings.Add(&m_Buffer[stringStart]);
					}
					bInString = FALSE;
				}
				else if (!IS_STRING_CHAR(c))
				{
					bInString = FALSE;
				}
			}
			else
			{
				if (IS_STRING_CHAR(c))
				{
					bInString = TRUE;
					stringStart = i;
				}
			}
			if ((i % 1000) == 0)
				pWnd->SendMessage(WM_APP, 0, i);
		}
	}
}

LPBYTE CFirmware::GetFirmwareBuffer()
{
	return m_Buffer;
}

DWORD CFirmware::GetFirmwareSize()
{
	return m_BufferSize;
}

CString CFirmware::GetName()
{
	return m_Name;
}

DWORD CFirmware::ComputeChecksum(int index)
{
	DWORD i;

	if (m_Buffer == NULL)
		return 0;

	DWORD checksum = 0;
	DWORD blockLen = m_pPartitions[index].len;

	DWORD startAddress = m_PartitionInfo[index].checksumStartAddress;

	blockLen += m_PartitionInfo[index].checksumExtra - m_PartitionInfo[index].checksumTrim;

	for (i = startAddress; i < startAddress + blockLen; i++)
	{
		checksum += m_Buffer[i];
	}
	
	return checksum;
}

LPBYTE CFirmware::GetAUPDBuffer()
{
	int i;
	for (i = 0; i < m_NumPartitions; i++)
	{
		if (!strncmp(m_pPartitions[i].id, "dpua", 4))
			return m_Buffer+m_pPartitions[i].devOffset+(m_Buffer[266]==0 && m_pPartitions[i].devOffset>=2048 ? -2048 : theApp.BLOCK_SIZE);
	}
	return NULL;
}

void CFirmware::SyncChecksum()
{
	int i;

	for (i = 0; i < m_NumPartitions; i++)
	{
		bool bAupd = !strncmp(m_pPartitions[i].id, "dpua", 4);
		// The cipher works both ways: the "dpua" image is decrypted for the program when the firmware is loaded, and put back by this
		// function for the file that is written. A second call has to start from the decrypted form again, or the second file would
		// have the image decrypted and a sum of the encrypted bytes.
		if (bAupd && m_PartitionInfo[i].bSealed)
		{
			DecryptAUPD(i);
			m_PartitionInfo[i].bSealed = FALSE;
		}
		if (m_PartitionInfo[i].bCanModify)
		{
			m_pPartitions[i].checksum = ComputeChecksum(i);
		}
		if (bAupd && DecryptAUPD(i))
			m_PartitionInfo[i].bSealed = TRUE;
	}
}

// The firmware is in the form of the file only for the time of a write (SyncChecksum). Everything else in the program reads the "dpua" image
// decrypted: the boot pictures (the type 2 graphics) are found in it when the firmware is loaded, and the Pictures page, the layout names and the
// themes read them from it. Left encrypted after a save, they no longer read, and the Pictures page went on showing the last picture it could read.
void CFirmware::RestoreWorkingForm()
{
	for (int i = 0; i < m_NumPartitions; i++)
		if (m_PartitionInfo[i].bSealed && !strncmp(m_pPartitions[i].id, "dpua", 4))
		{
			DecryptAUPD(i);
			m_PartitionInfo[i].bSealed = FALSE;
		}
}

int CFirmware::GetNumPartitions()
{
	return m_NumPartitions;
}

// The element of a layout or menu that holds the byte at pos: its size field and the number of bytes that field says follow (the records of
// both are size + 4 bytes in every firmware of the collection), found in the lists that were read from the firmware.
static BOOL FindElementExtent(ViewTable *pViews, MenuTable *pMenus, LPBYTE buf, DWORD pos, DWORD *pStart, DWORD *pLen)
{
	if (buf == NULL)
		return FALSE;
	if (pViews != NULL)
		for (size_t j = 0; j < pViews->size(); j++)
			for (int k = 0; k < pViews->at(j)->num_elems && k < (int)pViews->at(j)->ViewElemList.size(); k++)
			{
				const BYTE *e = (const BYTE *)pViews->at(j)->ViewElemList[k]->element;
				DWORD size = *(const DWORD *)e, start = (DWORD)(e - buf);
				if (size >= 0x20 && size < 0x1000 && pos >= start && pos - start < size + 4)
				{
					*pStart = start;
					*pLen = size + 4;
					return TRUE;
				}
			}
	if (pMenus != NULL)
		for (size_t j = 0; j < pMenus->size(); j++)
			for (int k = 0; k < pMenus->at(j)->num_elems && k < (int)pMenus->at(j)->MenuElemList.size(); k++)
			{
				const BYTE *e = (const BYTE *)pMenus->at(j)->MenuElemList[k]->element;
				DWORD size = *(const DWORD *)e, start = (DWORD)(e - buf);
				if (size >= 0x20 && size < 0x1000 && pos >= start && pos - start < size + 4)
				{
					*pStart = start;
					*pLen = size + 4;
					return TRUE;
				}
			}
	return FALSE;
}

// The resource that holds the byte at pos, for the hex view: its data as the table of its block gives it (the offset and the
// size of the entry), or the tables of the block (header, sections, entries) when pos is in them. FALSE when pos is in none.
BOOL CFirmware::FindResourceExtent(DWORD pos, DWORD *pStart, DWORD *pLen)
{
	if (m_Buffer == NULL)
		return FALSE;
	if (FindElementExtent(&ViewRecList, &MenuRecList, m_Buffer, pos, pStart, pLen))
		return TRUE;
	for (size_t b = 0; b < m_ResBlocks.size(); b++)
	{
		const ResBlockInfo &rb = m_ResBlocks[b];
		if (pos < rb.offset)
			continue;
		if (pos < rb.dataBegin)
		{
			*pStart = rb.offset;
			*pLen = rb.dataBegin - rb.offset;
			return TRUE;
		}
		if (rb.offset + sizeof(RESOURCE_HEADER) + sizeof(RESOURCE_SECTION) * rb.numSections > m_BufferSize)
			continue;
		RESOURCE_SECTION *pSec = (RESOURCE_SECTION *)&m_Buffer[rb.offset + sizeof(RESOURCE_HEADER)];
		for (DWORD k = 0; k < rb.numSections; k++)
		{
			if ((ULONGLONG)rb.offset + pSec[k].start + (ULONGLONG)sizeof(RESOURCE) * pSec[k].num_items > m_BufferSize)
				continue;
			RESOURCE *pRs = (RESOURCE *)&m_Buffer[rb.offset + pSec[k].start];
			for (DWORD x = 0; x < pSec[k].num_items; x++)
			{
				ULONGLONG start = (ULONGLONG)rb.dataBegin + pRs[x].offset;
				if (pRs[x].size != 0 && pos >= start && pos < start + pRs[x].size)
				{
					*pStart = (DWORD)start;
					*pLen = pRs[x].size;
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

BOOL CFirmware::GetImageRange(const char *id, DWORD *pStart, DWORD *pLen)
{
	for (int i = 0; i < m_NumPartitions; i++)
	{
		if (strncmp(m_pPartitions[i].id, id, 4) != 0 || m_pPartitions[i].len == 0)
			continue;
		DWORD start = m_pPartitions[i].devOffset;
		DWORD len = m_pPartitions[i].len;
		if (m_Buffer == NULL || start >= m_BufferSize)
			return FALSE;
		if (len > m_BufferSize - start)
			len = m_BufferSize - start;
		*pStart = start;
		*pLen = len;
		return TRUE;
	}
	return FALSE;
}

// The updater image carries the bootloader that is flashed into the iPod, and the bootloader names its build: the text
// "Bootloader Ui Shell ,Build Apr 19 2003,23:49:56" is in it (1.0.0 has Nov 01 2001, 2.2.0.0 Apr 19 2003, 2.2.3 Feb 16 2005). The image is looked at in
// the form the program works on (the encrypted ones are decrypted when the firmware is loaded), so nothing is found while the firmware is in the
// form of the file for a write.
CString CFirmware::GetBootloaderDate(int index)
{
	CString date;
	if (m_Buffer == NULL || index < 0 || index >= m_NumPartitions || strncmp(m_pPartitions[index].id, "dpua", 4) != 0)
		return date;
	static const char key[] = "Bootloader Ui Shell ,Build ";
	const size_t keyLen = sizeof(key) - 1;
	DWORD start = m_pPartitions[index].devOffset;
	if (start >= 2048)
		start -= 2048;                          // (the first software has the image 2048 bytes in front of its offset)
	DWORD end = m_pPartitions[index].devOffset + m_pPartitions[index].len + 4096;
	if (end > m_BufferSize)
		end = m_BufferSize;
	for (DWORD i = start; i + keyLen + 11 < end; i++)
	{
		if (m_Buffer[i] == 'B' && memcmp(&m_Buffer[i], key, keyLen) == 0)
		{
			char s[12];
			memcpy(s, &m_Buffer[i + keyLen], 11);
			s[11] = 0;
			// "Apr 19 2003": a month name, a day, a year
			if (isalpha((unsigned char)s[0]) && isalpha((unsigned char)s[1]) && isalpha((unsigned char)s[2]) && s[3] == ' ' &&
			    isdigit((unsigned char)s[7]) && isdigit((unsigned char)s[8]) && isdigit((unsigned char)s[9]) && isdigit((unsigned char)s[10]))
			{
				// shown as yy/mm/dd ("Apr 19 2003" -> "03/04/19")
				static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
				int mon = 0;
				for (int m = 0; m < 12 && !mon; m++)
					if (_strnicmp(s, &months[m * 3], 3) == 0)
						mon = m + 1;
				int day = atoi(&s[4]);
				if (mon && day >= 1 && day <= 31)
				{
					date.Format(_T("%c%c/%02d/%02d"), s[9], s[10], mon, day);
					break;
				}
			}
		}
	}
	return date;
}

void CFirmware::GetPartitionChecksums(int index, LPDWORD pChecksum1, LPDWORD pChecksum2)
{
	if (pChecksum1 == NULL || pChecksum2 == NULL || index >= m_NumPartitions)
		return;

	*pChecksum1 = m_pPartitions[index].checksum;
	*pChecksum2 = ComputeChecksum(index);
}

BOOL CFirmware::CanWrite(DWORD offset, DWORD len)
{
	for (int i = 0; i < m_NumPartitions; i++)
	{
		if (offset >= m_PartitionInfo[i].checksumStartAddress &&
			offset + len <= (m_PartitionInfo[i].checksumStartAddress + m_PartitionInfo[i].checksumExtra + m_pPartitions[i].len) &&
			m_PartitionInfo[i].bCanModify)
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL CFirmware::CanWritePicture(DWORD index)
{
	DWORD offset = (DWORD)((LPBYTE)m_Pictures.GetAt(index) - m_Buffer);

	return CanWrite(offset, 1);
}

DWORD CFirmware::GetNumPictures()
{
	return (DWORD)m_Pictures.GetCount();
}

LPBYTE CFirmware::GetPicture(DWORD index)
{
	if (index >= (DWORD)m_Pictures.GetCount())
		return NULL;

	return (LPBYTE)m_Pictures.GetAt(index);
}

DWORD CFirmware::GetPictureType(DWORD index)
{
	if (index >= (DWORD)m_PicturesType.GetCount())
		return FALSE;

	return (DWORD)m_PicturesType.GetAt(index);
}

DWORD CFirmware::GetPictureID(DWORD index)
{
	if (index >= (DWORD)m_PicturesID.size())
		return FALSE;

	return m_PicturesID[index];
}

DWORD CFirmware::GetPictureIndexFromID(DWORD id)
{
	for (int i=0;i<m_PicturesID.size();i++)
	{
		if (id==m_PicturesID[i])
			return (WORD)i;
	}
	return 0xFFFFFFFF;
}

DWORD CFirmware::GetNumFonts()
{
	return (DWORD)m_Fonts.GetCount();
}

LPBYTE CFirmware::GetFont(DWORD index)
{
	if (index >= (DWORD)m_Fonts.GetCount())
		return NULL;

	return (LPBYTE)m_Fonts.GetAt(index);
}

BOOL CFirmware::CanWriteFont(DWORD index)
{
	DWORD offset = (DWORD)((LPBYTE)m_Fonts.GetAt(index) - m_Buffer);

	return CanWrite(offset, 1);
}

DWORD CFirmware::GetNumOTFFonts()
{
	return (DWORD)m_OTFFonts.GetCount();
}

LPBYTE CFirmware::GetOTFFont(DWORD index)
{
	if (index >= (DWORD)m_OTFFonts.GetCount())
		return NULL;

	return (LPBYTE)m_OTFFonts.GetAt(index);
}

BOOL CFirmware::CanWriteOTFFont(DWORD index)
{
	DWORD offset = (DWORD)((LPBYTE)m_OTFFonts.GetAt(index) - m_Buffer);

	return CanWrite(offset, 1);
}

DWORD CFirmware::GetNumLangs()
{
	return (DWORD)m_iPodLang.GetCount();
}

DWORD CFirmware::GetNumLangStrings(DWORD index)
{
	if (index >= (DWORD)m_iPodLang.GetCount())
		return 0;

	IPOD_LANG *il;
	il=m_iPodLang.GetAt(index);
	return (DWORD)il->iPodString.GetCount();
}

DWORD CFirmware::GetNumStrings()
{
	return (DWORD)m_Strings.GetCount();
}

LPCTSTR CFirmware::GetString(DWORD index)
{
	if (index >= (DWORD)m_Strings.GetCount())
		return FALSE;

	return (LPCTSTR)m_Strings.GetAt(index);
}

LPCTSTR CFirmware::GetLangString(DWORD index, DWORD index2)
{
	if (index >= (DWORD)m_iPodLang.GetCount())
		return FALSE;

	IPOD_LANG *il;
	il=m_iPodLang.GetAt(index);

	if (index2 >= (DWORD)il->iPodString.GetCount())
		return FALSE;

	IPOD_STRING *is;
	is=il->iPodString.GetAt(index2);
	return (LPCTSTR)&m_Buffer[is->offset];
}

DWORD CFirmware::GetLangStringSize(DWORD index, DWORD index2)
{
	if (index >= (DWORD)m_iPodLang.GetCount())
		return 0;

	IPOD_LANG *il;
	il=m_iPodLang.GetAt(index);

	if (index2 >= (DWORD)il->iPodString.GetCount())
		return 0;

	IPOD_STRING *is;
	is=il->iPodString.GetAt(index2);
	return is->size;
}

void CFirmware::AddLangString(DWORD index, DWORD index2, LPBYTE str, DWORD size)
{
	if (index >= (DWORD)m_iPodLang.GetCount())
		return;

	IPOD_LANG *il;
	il=m_iPodLang.GetAt(index);

	if (index2 >= (DWORD)il->iPodString.GetCount())
		return;

	IPOD_STRING *is;
	is=il->iPodString.GetAt(index2);

	is->size=size+1;

	LPBYTE lpPos;
	if (index2+1 != (DWORD)il->iPodString.GetCount())
	{
		is->offset=m_SLangOffset;

		lpPos=&m_Buffer[is->offset];
		if (size>0)
			memcpy(lpPos, str, size);
		lpPos[size]=0;

		m_SLangOffset+=is->size;
	}
	else
	{
		is->offset=il->rsdata_end-is->size;
		lpPos=&m_Buffer[is->offset];
		if (size>0)
			memcpy(lpPos, str, size);
		lpPos[size]=0;
	}
	*is->string_off=is->offset-il->rsdata_start;
	*is->size_off=is->size;
}

BOOL CFirmware::ResetLangBlock(DWORD index, DWORD totallen)
{
	if (index >= (DWORD)m_iPodLang.GetCount())
		return FALSE;

	IPOD_LANG *il;
	il=m_iPodLang.GetAt(index);

	IPOD_STRING *is;
	is=il->iPodString.GetAt(0);
	
	DWORD start=is->offset;
	DWORD end=il->rsdata_end;

	if (totallen>end-start+1)
		return FALSE;

	for (DWORD i=start;i<end;i++)
		m_Buffer[i]=0;

	m_SLangOffset=is->offset;
	
	m_StringOrderFixed = TRUE;

	return TRUE;
}

BOOL CFirmware::SetLangString(DWORD index, DWORD index2, LPBYTE str, DWORD size)
{
	if (index >= (DWORD)m_iPodLang.GetCount())
		return FALSE;

	IPOD_LANG *il;
	il=m_iPodLang.GetAt(index);

	DWORD total=(DWORD)il->iPodString.GetCount();
	if (index2 >= total)
		return FALSE;

	DWORD i;

	//calc total space of strings
	DWORD sum=0;
	IPOD_STRING *is;
	is=il->iPodString.GetAt(0);
	DWORD datastart=is->offset;

	for (i=0;i<total;i++)
	{
		is=il->iPodString.GetAt(i);
		sum+=is->size;
	}
	
	is=il->iPodString.GetAt(index2);

	if (size+1 <= is->size)
	{
		LPBYTE slot=&m_Buffer[is->offset];
		if (size>0)
			memcpy(slot, str, size);
		memset(slot+size, 0, is->size-size);
		return TRUE;
	}

	int change=(signed)(size+1-is->size);

	sum+=change;
	DWORD original_len=il->rsdata_end-datastart;
	if (sum>original_len) //check if after the future change there is enough space in language block
		return FALSE;

	//change string
	LPBYTE lpPos,lpBuf;
	if (m_StringOrderFixed)
	{
		if (index2+1!=total)
		{
			IPOD_STRING *ist;
			
			lpBuf=new BYTE[original_len];
			memcpy(lpBuf, &m_Buffer[datastart], original_len);
			for (i=total-2;i>index2;i--)
			{
				ist=il->iPodString.GetAt(i);
				if (ist->size>0)
					memcpy(&lpBuf[ist->offset-datastart+change], &m_Buffer[ist->offset], ist->size);
				ist->offset+=change;
				*ist->string_off=ist->offset-il->rsdata_start;
			}
			memcpy(&m_Buffer[datastart], lpBuf, original_len);
			delete[] lpBuf;

			lpPos=&m_Buffer[is->offset];
			is->size=size+1;
			if (size>0)
				memcpy(lpPos, str, size);
			lpPos[size]=0;

			*is->size_off=is->size;
		}
		else
		{
			is->offset-=change;

			is->size=size+1;

			lpPos=&m_Buffer[is->offset];
			if (size>0)
				memcpy(lpPos, str, size);
			lpPos[size]=0;

			*is->string_off=is->offset-il->rsdata_start;
			*is->size_off=is->size;
		}
	}
	else
	{
		//Trim 00 spaces so will have space for changes.
		lpBuf=new BYTE[il->rsdata_end-datastart];
		memcpy(lpBuf, &m_Buffer[datastart], original_len);
		
		if (ResetLangBlock(index, sum)==FALSE)
			return FALSE;
		
		for (i=0;i<total;i++)
		{
			is=il->iPodString.GetAt(i);
			if (i==index2)
				AddLangString(index, i, str, size);
			else
				AddLangString(index, i, &lpBuf[is->offset-datastart], is->size-1);
		}

		delete[] lpBuf;
		m_StringOrderFixed = TRUE;
	}
	return TRUE;
}

void CFirmware::SetData(DWORD offset, LPBYTE data, DWORD size)
{
	if (offset < m_BufferSize)
	{
		memcpy(&m_Buffer[offset], data, size);
	}
}

__int32 CFirmware::GetLangStringID(DWORD index, DWORD index2)
{
	if (index >= (DWORD)m_iPodLang.GetCount())
		return 0;

	IPOD_LANG *il;
	il=m_iPodLang.GetAt(index);

	if (index2 >= (DWORD)il->iPodString.GetCount())
		return 0;

	IPOD_STRING *is;
	is=il->iPodString.GetAt(index2);
	return is->id;
}

ViewTable *CFirmware::GetViewList()
{
	if (ViewRecList.size()>0)
	{
		return &ViewRecList;
	}
	else
		return NULL;
}

MenuTable *CFirmware::GetMenuList()
{
	if (MenuRecList.size()>0)
	{
		return &MenuRecList;
	}
	else
		return NULL;
}

TypeTable *CFirmware::GetTypeList()
{
	if (TypeRecList.size()>0)
	{
		return &TypeRecList;
	}
	else
		return NULL;
}

TCMDTable *CFirmware::GetTCMDList()
{
	if (TCMDRecList.size()>0)
	{
		return &TCMDRecList;
	}
	else
		return NULL;
}

ScriptTable *CFirmware::GetScriptList()
{
	if (ScriptRecList.size()>0)
	{
		return &ScriptRecList;
	}
	else
		return NULL;
}

bool CFirmware::FindResourceID(int i, int id)
{
	bool found=false;
	int j;
	switch(i)
	{
	case 0: //view
		for (j = 0; j < ViewRecList.size(); j++)
		{
			if (ViewRecList.at(j)->id==id)
			{
				found=true;
				break;
			}
		}
		break;
	case 1: //menu
		for (j = 0; j < MenuRecList.size(); j++)
		{
			if (MenuRecList.at(j)->id==id)
			{
				found=true;
				break;
			}
		}
		break;
	case 2: //type
		for (j = 0; j < TypeRecList.size(); j++)
		{
			if (TypeRecList.at(j)->id==id)
			{
				found=true;
				break;
			}
		}
		break;
	case 3: //tcmd
		for (j = 0; j < TCMDRecList.at(0)->element.elements.size(); j++)
		{
			if (TCMDRecList.at(0)->element.elements.at(j)->id==id)
			{
				found=true;
				break;
			}
		}
		break;
	}
	return found;
}

void SecurityBlock::GetSecurityBlockKey(unsigned char *rawData){
	int constant = 0x54c3a298;
	int key=0;
	data=rawData;
	int aMarker=0;
	int pos=0;
	for (int c=0;c<8;c++){
		pos =offset[c]*4;
		aMarker=readWord(rawData,pos);
		bool result=testMarker(aMarker);
		//System.out.println("Marker ="+Integer.toHexString(aMarker)+" "+result);
		if (result)
		{ // This marker is enable
			fileIsProtected=true;
			// pos of nextblock
			if (c<7)
				pos =(offset[c+1]*4)+4;
			else
				pos =(offset[0]*4)+4;
			key=0;
			int temp1=aMarker;
			for (int count=0;count<2;count++)
			{
				int word=readWord(data,pos);
				temp1=aMarker;
				temp1=temp1^word;
				temp1=temp1^constant;
				key=temp1;
				pos=pos+4;
			}
			int r1=0x6f;
			int r2=0;
			int r12;
			int r14;
			unsigned int r_tmp;
			for (int count=2;count<theApp.BLOCK_SIZE/4;count=count+2)
			{
				r2=readWord(data,count*4);
				r12=readWord(data,(count*4)+4);
				r_tmp=(unsigned int)r12>>16;
				r14=r2 | ((int)r_tmp);
				r2=r2&0xffff;
				r2=r2 | r12;
				r1=r1^r14;
				r1=r1+r2;
			}
			key=key^r1;
			// Invert key, little endian
			r_tmp=((unsigned int)(key&0xff000000))>>24;
			unsigned int r_tmp2=((unsigned int)(key&0xff0000))>>8;
			this->key = ((key&0xff)<<24)|((key&0xff00)<<8)|(r_tmp2)|(r_tmp);
		}
	}
}

int SecurityBlock::readWord(unsigned char *buffer,int pos){
	int p1=buffer[pos];
	int p2=buffer[pos+1];
	int p3=buffer[pos+2];
	int p4=buffer[pos+3];
	if (p1<0)p1=p1+256;
	if (p2<0)p2=p2+256;
	if (p3<0)p3=p3+256;
	if (p4<0)p4=p4+256;
	return p1+(p2<<8)+(p3<<16)+(p4<<24);
}

bool SecurityBlock::testMarker(int marker){
	int mask = (marker&0xff)|((marker&0xff)<<8)|((marker&0xff)<<16)|((marker&0xff)<<24);
	int decrypt = marker ^ mask;
	int temp1=(int)((unsigned int)decrypt>>24);
	int temp2=decrypt<<8;
	if (temp1==0) return false;
	temp2=(int)((unsigned int)temp2>>24);
	decrypt=decrypt<<16;
	decrypt=(int)((unsigned int)decrypt>>24);
	if ((temp1<temp2)&&(temp2<decrypt)){
		temp1=temp1&0xf;
		temp2=temp2&0xf;
		decrypt=decrypt&0xf;
		if ((temp1>temp2)&&(temp2>decrypt)){
			if (decrypt!=0) return true; // This marker is enabled!
		}
	}
	return false;
}

SecurityBlock::SecurityBlock(unsigned char *rawData)
{
	fileIsProtected=false;
}


unsigned char *FileDecrypt::decryptFirmwareFile(unsigned char *fileData, int key, unsigned long data_len){
		unsigned char *data=NULL;
		cipherBox=new int [256];
		cipherKeyArray=new int [256];
		//try{
		unsigned char* theKey = (unsigned char*)malloc(sizeof(key));
		memset(theKey, 0, sizeof(key));
		memcpy(theKey, &key, sizeof(key));
		doRC4keyExpansion(theKey);
		data= doRC4Decrypt(fileData, data_len);
		//}catch(e){
		//e.printStackTrace();
		//}
		return data;
	}

void FileDecrypt::doRC4keyExpansion(unsigned char *thisKey)
	{
		int keyLength =0;
		int dataSwap;
		int b;
		int keyByte;
		keyLength = 4;

		for(int a = 0; a < 256; a++)
		{
			keyByte = thisKey[a % keyLength];
			if (keyByte<0)keyByte=keyByte+256;
			cipherKeyArray[a] = keyByte;
			cipherBox[a]=a;
		}
		b = 0;
		for(int a = 0; a < 256; a++)
		{
			b = (b + cipherBox[a] + cipherKeyArray[a]) & 255;
			dataSwap = cipherBox[a];
			cipherBox[a] = cipherBox[b];
			cipherBox[b] = dataSwap;
		}
	}

unsigned char *FileDecrypt::doRC4Decrypt(unsigned char *rawData, unsigned long data_len){
		int z = 0;
		int t = 0;
		int i = 0;
		int tempInt=0;
		byte cipherByte;
		int cipherBy = 0;
		for(int a = 0; a < data_len; a++)
		{
			i = (i + 1) & 255;
			t = (t + cipherBox[i]) & 255;
			tempInt = cipherBox[i];
			cipherBox[i] = cipherBox[t];
			cipherBox[t]= tempInt;
			z = cipherBox[(cipherBox[i] + cipherBox[t]) & 255];
			cipherByte = rawData[a];
			cipherBy = (int) cipherByte ^z;
			rawData[a]=(byte)cipherBy;
		}
		return rawData;
	}