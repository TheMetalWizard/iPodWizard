// BrickParams.h - where the numbers of the Brick game (BlockO) are in the operating system image of a firmware, and what they are.
//
// Brick is ARM code in the "osos" image of the monochrome iPods (1G/2G, 3G, mini, 4G). Its numbers are not in a table but in instructions
// (a "mov rX,#n" carries 8 bits rotated by an even amount), with one exception: the levels are a table of 22 rows of three 32 bit words
// {balls, rows of bricks, paddle width} and a row of zeros. Everything is found by the code AROUND the number, not by the number, so a firmware
// whose numbers were changed is found as well as an original one:
//   - the function that puts the balls in play starts  push {r4-r9,lr} ; ldr r6,=levels ; mov r5,#0 ; str r5,[r0,#0x598] ; mov r9,#vx ; mov r8,#vy ;
//     mov r7,#1  (the literal is the address of the level table, the two immediates the start speed of a ball, 16.16 fixed point)
//   - lives of a new game:  mov r0,#n ; add r1,r4,#0x680 ; stmdb r1,{r0,r5}  (lives and level are stored together, at +0x678 and +0x67C)
//   - points for a brick:  ldr r0,[r5,#0x674] ... add r1,r0,#n ; str r1,[r5,#0x674]
//   - lives for a cleared level:  ldr r0,[r5,#0x678] ; ldr r1,[r5,#0x598] ; ... ; add r0,r0,r1 (one per ball in play) or add r0,r0,#n ; str r0,[r5,#0x678]
//   - the ball:  mov lr,#width ; ldr r3,[r0,#0x4c4] ; add r12,r3,#gap ; ... ; add r12,r12,#height  (inside the function that puts the balls in play)
// The research behind this is in research\ghidra\Brick-Game-Logic.txt. Nothing here writes anything.
//
// Released under the GNU General Public License v2, like the rest of iPodWizard.

#pragma once

struct BrickParams
{
	// where the numbers are: offsets inside the image (0xFFFFFFFF: not found). *At of an instruction is the offset of the instruction.
	DWORD tableAt;                    // 22 levels and the closing row of zeros: 23 rows of 3 words
	DWORD vxAt, vyAt;                 // mov rX,#vx and mov rX,#vy (always the two instructions in a row)
	DWORD livesAt;                    // mov r0,#lives
	DWORD scoreAt;                    // add r1,r0,#points
	DWORD bonusAt;                    // add r0,r0,r1 / add r0,r0,#n
	DWORD ballWAt, ballHAt, ballGapAt;   // mov lr,#width / add r12,r12,#height / add r12,r3,#gap

	// what they are now
	DWORD levels[23][3];              // balls, rows of bricks, paddle width
	DWORD vx, vy;                     // speed of a new ball, 16.16 fixed point (stock 0x28000 and 0x21000)
	int lives;                        // -1 when not found
	int points;
	int bonus;                        // lives for a cleared level: -1 = one for every ball still in play, else that many; -2 not found
	int ballW, ballH, ballGap;
};

// the number a "mov rX,#imm12" / "add rX,rY,#imm12" instruction (cond and registers ignored) carries: 8 bits rotated right by an even amount
inline DWORD BrickArmImm(DWORD instruction)
{
	DWORD rot = ((instruction >> 8) & 0xF) * 2, v = instruction & 0xFF;
	return rot ? ((v >> rot) | (v << (32 - rot))) : v;
}

// img: the operating system image (plain ARM code, 4 byte aligned), len its size. True when the Brick game is in it.
inline bool FindBrickParams(const unsigned char *img, DWORD len, BrickParams *p)
{
	const DWORD none = 0xFFFFFFFF;
	p->tableAt = p->vxAt = p->vyAt = p->livesAt = p->scoreAt = p->bonusAt = p->ballWAt = p->ballHAt = p->ballGapAt = none;
	p->vx = p->vy = 0;
	p->lives = p->points = p->ballW = p->ballH = p->ballGap = -1;
	p->bonus = -2;
	if (img == NULL || len < 4096 || (((DWORD_PTR)img) & 3) != 0)
		return false;
	const DWORD *W = (const DWORD *)img;
	DWORD n = len / 4;

	// the function that puts the balls in play: it holds the address of the level table and the start speed of a ball
	DWORD setup = none;
	int count = 0;
	for (DWORD i = 0; i + 8 < n; i++)
		if (W[i] == 0xE92D43F0 && (W[i + 1] & 0xFFFFF000) == 0xE59F6000 && W[i + 2] == 0xE3A05000 && W[i + 3] == 0xE5805598 &&
		    (W[i + 4] & 0xFFFFF000) == 0xE3A09000 && (W[i + 5] & 0xFFFFF000) == 0xE3A08000 && W[i + 6] == 0xE3A07001)
		{
			setup = i;
			count++;
		}
	if (count != 1)
		return false;
	{
		DWORD d = W[setup + 1] & 0xFFF;
		DWORD at = (setup + 1) * 4 + 8 + ((W[setup + 1] >> 23) & 1 ? d : (DWORD)(0 - d));
		if (at + 4 > len)
			return false;
		DWORD table = W[at / 4];
		if (table % 4 != 0 || (ULONGLONG)table + 23 * 12 > len)
			return false;
		p->tableAt = table;
		for (int i = 0; i < 23; i++)
			for (int k = 0; k < 3; k++)
				p->levels[i][k] = W[table / 4 + 3 * i + k];
		if (p->levels[22][0] != 0 || p->levels[0][0] == 0)
			return false;                                       // not a table of levels (a row of zeros ends it)
		p->vxAt = (setup + 4) * 4;
		p->vyAt = (setup + 5) * 4;
		p->vx = BrickArmImm(W[setup + 4]);
		p->vy = BrickArmImm(W[setup + 5]);
	}

	// the ball, in the same function
	{
		int nb = 0;
		DWORD last = setup + 100 < n ? setup + 100 : n;
		for (DWORD i = setup; i + 6 < last; i++)
			if ((W[i] & 0xFFFFF000) == 0xE3A0E000 && W[i + 1] == 0xE59034C4 && (W[i + 2] & 0xFFFFF000) == 0xE283C000 && W[i + 3] == 0xE0843201 &&
			    W[i + 4] == 0xE583C5B4 && (W[i + 5] & 0xFFFFF000) == 0xE28CC000)
			{
				p->ballWAt = i * 4;
				p->ballGapAt = (i + 2) * 4;
				p->ballHAt = (i + 5) * 4;
				p->ballW = (int)BrickArmImm(W[i]);
				p->ballGap = (int)BrickArmImm(W[i + 2]);
				p->ballH = (int)BrickArmImm(W[i + 5]);
				nb++;
			}
		if (nb != 1)
		{
			p->ballWAt = p->ballHAt = p->ballGapAt = none;
			p->ballW = p->ballH = p->ballGap = -1;
		}
	}

	// lives of a new game
	{
		int nl = 0;
		for (DWORD j = 2; j < n; j++)
			if (W[j] == 0xE9010021 && (W[j - 1] & 0xFFF00000) == 0xE2800000 && BrickArmImm(W[j - 1]) == 0x680 && (W[j - 2] & 0xFFFFF000) == 0xE3A00000)
			{
				p->livesAt = (j - 2) * 4;
				p->lives = (int)BrickArmImm(W[j - 2]);
				nl++;
			}
		if (nl != 1)
		{
			// code laid out another way (3G): mov rX,#3 followed within a few instructions by str rX,[rY,#0x678] (only the original number is found)
			p->livesAt = none;
			p->lives = -1;
			int n2 = 0;
			for (DWORD i = 0; i < n; i++)
				if ((W[i] & 0x0FF00FFF) == 0x05800678)
				{
					DWORD rs = (W[i] >> 12) & 0xF;
					for (DWORD j = i; j > 0 && j + 6 > i; j--)
						if ((W[j - 1] & 0x0FFFF000) == (0x03A00000 | (rs << 12)) && BrickArmImm(W[j - 1]) == 3)
						{
							p->livesAt = (j - 1) * 4;
							p->lives = 3;
							n2++;
							break;
						}
				}
			if (n2 != 1)
			{
				p->livesAt = none;
				p->lives = -1;
			}
		}
	}

	// points for a brick, lives for a cleared level
	{
		int ns = 0, nb = 0;
		for (DWORD i = 0; i + 5 < n; i++)
		{
			if (W[i] == 0xE5950674 && (W[i + 2] & 0xFFFFF000) == 0xE2801000 && W[i + 3] == 0xE5851674)
			{
				p->scoreAt = (i + 2) * 4;
				p->points = (int)BrickArmImm(W[i + 2]);
				ns++;
			}
			if (W[i] == 0xE5950678 && W[i + 1] == 0xE5951598 && (W[i + 3] == 0xE0800001 || (W[i + 3] & 0xFFFFF000) == 0xE2800000) && W[i + 4] == 0xE5850678)
			{
				p->bonusAt = (i + 3) * 4;
				p->bonus = W[i + 3] == 0xE0800001 ? -1 : (int)BrickArmImm(W[i + 3]);
				nb++;
			}
		}
		if (ns != 1)
		{
			p->scoreAt = none;
			p->points = -1;
		}
		if (nb != 1)
		{
			p->bonusAt = none;
			p->bonus = -2;
		}
	}
	return true;
}
