//--------------------------------------------------------------
//File name:    loader.c
//--------------------------------------------------------------
//dlanor: This subprogram has been modified to minimize the code
//dlanor: size of the resident loader portion. Some of the parts
//dlanor: that were moved into the main program include loading
//dlanor: of all IRXs and mounting pfs0: for ELFs on hdd.
//dlanor: Another change was to skip threading in favor of ExecPS2
/*==================================================================
==											==
==	Copyright(c)2004  Adam Metcalf(gamblore_@hotmail.com)		==
==	Copyright(c)2004  Thomas Hawcroft(t0mb0la@yahoo.com)		==
==	This file is subject to terms and conditions shown in the	==
==	file LICENSE which should be kept in the top folder of	==
==	this distribution.							==
==											==
==	Portions of this code taken from PS2Link:				==
==				pkoLoadElf						==
==				wipeUserMemory					==
==				(C) 2003 Tord Lindstrom (pukko@home.se)	==
==				(C) 2003 adresd (adresd_ps2dev@yahoo.com)	==
==	Portions of this code taken from Independence MC exploit	==
==				tLoadElf						==
==				LoadAndRunHDDElf					==
==				(C) 2003 Marcus Brown <mrbrown@0xd6.org>	==
==											==
==================================================================*/
#include <kernel.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <errno.h>
#include <string.h>
#include <iopcontrol.h>


   void _ps2sdk_libc_init() {}
   void _ps2sdk_libc_deinit() {}
//Code for OSDSYS patching was taken from FMCB 1.8 sources

 u32 execps2_code[] = {
	0x24030007, // li v1, 7
	0x0000000c, // syscall
	0x03e00008, // jr ra
	0x00000000	// nop
};
 u32 execps2_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff};

//=========================================================================
//  SkipHdd patch for v3, v4 (those not supporting "SkipHdd" arg)

 u32 pattern10[] = {
	// Code near MC Update & HDD load
	0x0c000000, // jal 	 CheckMcUpdate
	0x0220282d, // daddu a1, s1, zero
	0x3c04002a, // lui	 a0, 0x002a         #SkipHdd jump must be here
	0x0000282d, // daddu a1, zero, zero 	#arg1: 0
	0x24840000, // addiu a0, a0, 0xXXXX  	#arg0: "rom0:ATAD"
	0x0c000000, // jal 	 LoadModule
	0x0000302d, // daduu a2, zero, zero		#arg2: 0
	0x04400000	// bltz  v0, Exit_HddLoad
};
 u32 pattern10_mask[] = {
	0xfc000000,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffff0000,
	0xfc000000,
	0xffffffff,
	0xffff0000};

//--------------------------------------------------------------
 u8 *find_bytes_with_mask(u8 *buf, u32 bufsize, u8 *bytes, u8 *mask, u32 len)
{
	u32 i, j;

	for (i = 0; i < bufsize - len; i++)
	{
		for (j = 0; j < len; j++)
		{
			if ((buf[i + j] & mask[j]) != bytes[j])
				break;
		}
		if (j == len)
			return &buf[i];
	}
	return NULL;
}
//--------------------------------------------------------------
 u8 *find_string(const u8 *string, u8 *buf, u32 bufsize)
{
	u32 i;
	const u8 *s, *p;

	for (i = 0; i < bufsize; i++)
	{
		s = string;
		for (p = buf + i; *s && *s == *p; s++, p++)
			;
		if (!*s)
			return (buf + i);
	}
	return NULL;
}
 void patch_skip_hdd(u8 *osd)
{
	u8 *ptr;
	u32 addr;

	// Search code near MC Update & HDD load
	ptr = find_bytes_with_mask(osd, 0x00100000, (u8 *)pattern10, (u8 *)pattern10_mask, sizeof(pattern10));
	if (!ptr)
		return;
	addr = (u32)ptr;

	// Place "beq zero, zero, Exit_HddLoad" just after CheckMcUpdate() call
	_sw(0x10000000 + ((signed short)(_lw(addr + 28) & 0xffff) + 5), addr + 8);
}

 void patch_and_execute_osdsys(void *epc, void *gp)
{

	int n = 0;
	char *args[5], *ptr;

	args[n++] = "rom0:";
	args[n++] = "BootBrowser"; // triggers BootBrowser to reach internal mc browser
	args[n++] = "SkipMc";	   // Skip mc?:/BREXEC-SYSTEM/osdxxx.elf update on v5 and above

	if (find_string("SkipHdd", (u8 *)epc, 0x100000)) // triggers SkipHdd arg
		args[n++] = "SkipHdd";						 // Skip Hddload on v5 and above
	else
		patch_skip_hdd((u8 *)epc); // SkipHdd patch for v3 & v4

	// To avoid loop in OSDSYS (Handle those models not supporting SkipMc arg) :
	while ((ptr = find_string("EXEC-SYSTEM", (u8 *)epc, 0x100000)))
		strncpy(ptr, "EXEC-OSDSYS", 11);

	SifExitRpc();
	FlushCache(0);
	FlushCache(2);

	ExecPS2(epc, gp, n, args);
}

//--------------------------------------------------------------
//End of data declarations
//--------------------------------------------------------------
//Start of function code:
//--------------------------------------------------------------
// Clear user memory
// PS2Link (C) 2003 Tord Lindstrom (pukko@home.se)
//         (C) 2003 adresd (adresd_ps2dev@yahoo.com)
//--------------------------------------------------------------
void wipeUserMem(void)
{
	int i;
	for (i = 0x100000; i < GetMemorySize(); i += 64)
	{
		asm volatile(
			"\tsq $0, 0(%0) \n"
			"\tsq $0, 16(%0) \n"
			"\tsq $0, 32(%0) \n"
			"\tsq $0, 48(%0) \n" ::"r"(i));
	}
}

//--------------------------------------------------------------
//End of func:  void wipeUserMem(void)
//--------------------------------------------------------------
// *** MAIN ***
//--------------------------------------------------------------
int main(int argc, char *argv[])
{
	static t_ExecData elfdata;
	char *target, *path;
	char *args[1];
	int ret;
	u8 *ptr;

	// Initialize
	SifInitRpc(0);
	wipeUserMem();

	if (argc != 2)
	{ // arg1=path to ELF, arg2=partition to mount
		SifExitRpc();
		return -EINVAL;
	}

	target = argv[0];
	path = argv[1];

	//Writeback data cache before loading ELF.
	FlushCache(0);
	ret = SifLoadElf(target, &elfdata);
	if (ret == 0)
	{

		args[0] = path;

		//If OSDSYS load requested then patch before launching (taken from FMCB 1.8 source code)
		if (strncmp(target, "rom0:OSDSYS", 11) == 0)
		{

			// Find the ExecPS2 function in the unpacker starting from 0x100000.
			ptr = find_bytes_with_mask((u8 *)0x100000, 0x1000, (u8 *)execps2_code, (u8 *)execps2_mask, sizeof(execps2_code));

			// If found replace it with a call to our patch_and_execute_osdsys() function.
			if (ptr)
			{
				u32 instr = 0x0c000000;
				instr |= ((u32)patch_and_execute_osdsys >> 2);
				*(u32 *)ptr = instr;
				*(u32 *)&ptr[4] = 0;
			}
		}

		if (strncmp(path, "hdd", 3) == 0 && (path[3] >= '0' && path[3] <= ':'))
		{ /* Final IOP reset, to fill the IOP with the default modules.
               It appears that it was once a thing for the booting software to leave the IOP with the required IOP modules.
               This can be seen in OSDSYS v1.0x (no IOP reboot) and the mechanism to boot DVD player updates (OSDSYS will get LoadExecPS2 to load SIO2 modules).
               However, it changed with the introduction of the HDD unit, as the software booted may be built with a different SDK revision.

               Reboot the IOP, to leave it in a clean & consistent state.
               But do not do that for boot targets on other devices, for backward-compatibility with older (homebrew) software. */
			while (!SifIopReset("", 0))
			{
			};
			while (!SifIopSync())
			{
			};
		}

		SifExitRpc();

		FlushCache(0);
		FlushCache(2);

		ExecPS2((void *)elfdata.epc, (void *)elfdata.gp, 1, args);
		return 0;
	}
	else
	{
		SifExitRpc();
		return -ENOENT;
	}
}
//--------------------------------------------------------------
//End of func:  int main(int argc, char *argv[])
//--------------------------------------------------------------
//End of file:  loader.c
//--------------------------------------------------------------
