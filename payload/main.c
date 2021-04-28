
#include <iopcontrol.h>
#include <iopheap.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <libcdvd.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <kernel.h>
#include <libcdvd.h>
#include <libmc.h>
#include <libpwroff.h>
#include <loadfile.h>
#include <libpad.h>
#include <sbv_patches.h>
#include <sifrpc.h>
#include <stdio.h>
#include <string.h>
#include <io_common.h>
#include <fileXio_rpc.h>
#include <fcntl.h>
#include <sbv_patches.h>

#include <stdio.h>
#include <debug.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

//#include "stdint.h"

#define NTSC 2
#define PAL 3

#define DELAY 100

#define SYSTEM_INIT_THREAD_STACK_SIZE 0x1000

// ELF-loading stuff
#define ELF_MAGIC 0x464c457f
#define ELF_PT_LOAD 1

extern u8 loader_elf[];
extern int size_loader_elf;

extern unsigned char IOMANX_irx[];
extern unsigned int size_IOMANX_irx;

extern unsigned char FILEXIO_irx[];
extern unsigned int size_FILEXIO_irx;

extern unsigned char SIO2MAN_irx[];
extern unsigned int size_SIO2MAN_irx;

extern unsigned char PADMAN_irx[];
extern unsigned int size_PADMAN_irx;

extern unsigned char DEV9_irx[];
extern unsigned int size_DEV9_irx;

extern unsigned char ATAD_irx[];
extern unsigned int size_ATAD_irx;

extern unsigned char HDD_irx[];
extern unsigned int size_HDD_irx;

extern unsigned char PFS_irx[];
extern unsigned int size_PFS_irx;

int VMode = NTSC;
extern void *_gp;

//------------------------------
typedef struct
{
	u8 ident[16]; // struct definition for ELF object header
	u16 type;
	u16 machine;
	u32 version;
	u32 entry;
	u32 phoff;
	u32 shoff;
	u32 flags;
	u16 ehsize;
	u16 phentsize;
	u16 phnum;
	u16 shentsize;
	u16 shnum;
	u16 shstrndx;
} elf_header_t;
//------------------------------
typedef struct
{
	u32 type; // struct definition for ELF program section header
	u32 offset;
	void *vaddr;
	u32 paddr;
	u32 filesz;
	u32 memsz;
	u32 flags;
	u32 align;
} elf_pheader_t;

struct SystemInitParams
{
	int InitCompleteSema;
	unsigned int flags;
};

static void SystemInitThread(struct SystemInitParams *SystemInitParams)
{
	static const char PFS_args[] = "-n\0"
								   "24\0"
								   "-o\0"
								   "8";
	int i;

	if (SifExecModuleBuffer(ATAD_irx, size_ATAD_irx, 0, NULL, NULL) >= 0)
	{
		SifExecModuleBuffer(HDD_irx, size_HDD_irx, 0, NULL, NULL);
		SifExecModuleBuffer(PFS_irx, size_PFS_irx, sizeof(PFS_args), PFS_args, NULL);
	}

	SifExitIopHeap();
	SifLoadFileExit();

	SignalSema(SystemInitParams->InitCompleteSema);
	ExitDeleteThread();
}

int SysCreateThread(void *function, void *stack, unsigned int StackSize, void *arg, int priority)
{
	ee_thread_t ThreadData;
	int ThreadID;

	ThreadData.func = function;
	ThreadData.stack = stack;
	ThreadData.stack_size = StackSize;
	ThreadData.gp_reg = &_gp;
	ThreadData.initial_priority = priority;
	ThreadData.attr = ThreadData.option = 0;

	if ((ThreadID = CreateThread(&ThreadData)) >= 0)
	{
		if (StartThread(ThreadID, arg) < 0)
		{
			DeleteThread(ThreadID);
			ThreadID = -1;
		}
	}

	return ThreadID;
}

u8 romver[16];
char romver_region_char[1];
char ROMVersionNumStr[5];

u32 bios_version = 0;

void ResetIOP()
{

	SifInitRpc(0);
	while (!SifIopReset("", 0))
	{
	};
	while (!SifIopSync())
	{
	};
	SifInitRpc(0);
}

void InitPS2()
{
	int ret;
	ee_sema_t sema;
	static unsigned char SysInitThreadStack[SYSTEM_INIT_THREAD_STACK_SIZE] __attribute__((aligned(16)));
	static struct SystemInitParams InitThreadParams;

	//Do something useful while the IOP resets.
	sema.init_count = 0;
	sema.max_count = 1;
	sema.attr = sema.option = 0;
	InitThreadParams.InitCompleteSema = CreateSema(&sema);
	InitThreadParams.flags = 0;

	ResetIOP();

	SifInitIopHeap();
	SifLoadFileInit();
	fioInit();

	sbv_patch_enable_lmb();
	sbv_patch_disable_prefix_check();

	SifExecModuleBuffer(IOMANX_irx, size_IOMANX_irx, 0, NULL, NULL);
	SifExecModuleBuffer(FILEXIO_irx, size_FILEXIO_irx, 0, NULL, NULL);

	fileXioInit();
	//sceCdInit(SCECdINoD);

	ret = SifExecModuleBuffer(DEV9_irx, size_DEV9_irx, 0, NULL, &stat);

	SifExecModuleBuffer(SIO2MAN_irx, size_SIO2MAN_irx, 0, NULL, NULL);

	SifExecModuleBuffer(PADMAN_irx, size_PADMAN_irx, 0, NULL, NULL);
	PadInitPads();

	SysCreateThread(SystemInitThread, SysInitThreadStack, SYSTEM_INIT_THREAD_STACK_SIZE, &InitThreadParams, 0x2);

	WaitSema(InitThreadParams.InitCompleteSema);
}

void LoadElf(char *filename, char *party)
{

	u8 *boot_elf;
	elf_header_t *eh;
	elf_pheader_t *eph;
	void *pdata;
	int i;
	char *argv[2], bootpath[256];

	if ((!strncmp(party, "hdd0:", 5)) && (!strncmp(filename, "pfs0:", 5)))
	{
		//If a path to a file on PFS is specified, change it to the standard format.
		//hdd0:partition:pfs:path/to/file
		if (strncmp(filename, "pfs0:", 5) == 0)
		{
			sprintf(bootpath, "%s:pfs:%s", party, &filename[5]);
		}
		else
		{
			sprintf(bootpath, "%s:%s", party, filename);
		}

		argv[0] = filename;
		argv[1] = bootpath;
	}
	else
	{
		argv[0] = filename;
		argv[1] = filename;
	}

	/* NB: LOADER.ELF is embedded  */
	boot_elf = (u8 *)loader_elf;
	eh = (elf_header_t *)boot_elf;
	if (_lw((u32)&eh->ident) != ELF_MAGIC)
		asm volatile("break\n");

	eph = (elf_pheader_t *)(boot_elf + eh->phoff);

	/* Scan through the ELF's program headers and copy them into RAM, then
									zero out any non-loaded regions.  */
	for (i = 0; i < eh->phnum; i++)
	{
		if (eph[i].type != ELF_PT_LOAD)
			continue;

		pdata = (void *)(boot_elf + eph[i].offset);
		memcpy(eph[i].vaddr, pdata, eph[i].filesz);

		if (eph[i].memsz > eph[i].filesz)
			memset(eph[i].vaddr + eph[i].filesz, 0,
				   eph[i].memsz - eph[i].filesz);
	}

	/* Let's go.  */
	SifExitRpc();
	FlushCache(0);
	FlushCache(2);

	ExecPS2((void *)eh->entry, NULL, 2, argv);
}

int file_exists(char filepath[])
{
	int fdn;

	fdn = open(filepath, O_RDONLY);
	if (fdn < 0)
		return 0;
	close(fdn);

	return 1;
}

static u32 execps2_code[] = {
	0x24030007, // li v1, 7
	0x0000000c, // syscall
	0x03e00008, // jr ra
	0x00000000	// nop
};
static u32 execps2_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff};

//=========================================================================
//  SkipHdd patch for v3, v4 (those not supporting "SkipHdd" arg)

static u32 pattern10[] = {
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
static u32 pattern10_mask[] = {
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
	args[n++] = "SkipMc"; // Skip mc?:/BREXEC-SYSTEM/osdxxx.elf update on v5 and above

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
void launch_osdsys(void) // Run OSDSYS
{
	u8 *ptr;
	t_ExecData exec;
	int i, j, r;

	SifLoadElf("rom0:OSDSYS", &exec);

	if (exec.epc > 0)
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

	

		SifExitRpc();
		FlushCache(0);
		FlushCache(2);

		// Execute the osd unpacker. If the above patching was successful it will
		// call the patch_and_execute_osdsys() function after unpacking.
		ExecPS2((void *)exec.epc, (void *)exec.gp, 0, NULL);
	}
}

int main(int argc, char *argv[])
{

	int x, r;
	u64 tstart;
	int lastKey = 0;
	int keyStatus;
	int isEarlyJap = 0;
	
	char boot_path[256];

	char *party = "hdd0:PP.SOFTDEV2.APPS";


	InitPS2();
	
	
		
  

	
	int fdnr;
	if ((fdnr = open("rom0:ROMVER", O_RDONLY)) > 0)
	{ // Reading ROMVER
		read(fdnr, romver, sizeof romver);
		close(fdnr);
	}

	// Getting region char
	romver_region_char[0] = (romver[4] == 'E' ? 'E' : (romver[4] == 'J' ? 'I' : (romver[4] == 'H' ? 'A' : (romver[4] == 'U' ? 'A' : romver[4]))));

	strncpy(ROMVersionNumStr, romver, 4);
	ROMVersionNumStr[4] = '\0';
	bios_version = strtoul(ROMVersionNumStr, NULL, 16);

	if ((romver_region_char[0] == 'J') && (bios_version <= 0x120))
		isEarlyJap = 1;

	if (fileXioMount("pfs0:", party, FIO_MT_RDONLY) == 0)
	{

		TimerInit();
		tstart = Timer();

		//Stores last key during DELAY msec
		do
		{

			keyStatus = ReadCombinedPadStatus();
			if (keyStatus)
				lastKey = keyStatus;
				
		  		
		}while (Timer() <= (tstart + DELAY));
		TimerEnd();

		//Deinits pad
		if (!isEarlyJap)
		{
			PadDeinitPads();
		}

		if (lastKey & PAD_CIRCLE)

		{
			if (file_exists("pfs0:ULE.ELF"))
				LoadElf("pfs0:ULE.ELF", party);

			if (file_exists("pfs0:OPNPS2LD.ELF"))
				LoadElf("pfs0:OPNPS2LD.ELF", party);
		}

		if (lastKey & PAD_TRIANGLE)
		{
			fileXioUmount("pfs0:");
			launch_osdsys();
		}

		if (file_exists("pfs0:OPNPS2LD.ELF"))
			LoadElf("pfs0:OPNPS2LD.ELF", party);

		if (file_exists("pfs0:ULE.ELF"))
			LoadElf("pfs0:ULE.ELF", party);
	}

	launch_osdsys();

	return 0;
}
