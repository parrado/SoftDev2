
#include <iopcontrol.h>
#include <iopheap.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <unistd.h>
//#include <input.h>
#include <time.h>
#include <string.h>
#include <libcdvd.h>
#include <iopcontrol.h>
//#include <iopcontrol_special.h>
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

#include "stdint.h"




#define NTSC 2
#define PAL 3

#define DELAY 0

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
extern void*      _gp;

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

	argv[0] = filename;
	argv[1] = filename;

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


int main(int argc, char *argv[])
{

	
	int x, r;
	u64 tstart;
	int lastKey = 0;
	int keyStatus;
	int isEarlyJap = 0;
	char *argv[2];

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

	

	


	int result = fileXioMount("pfs0:", "hdd0:PP.SOFTDEV2.APPS", FIO_MT_RDWR);


	
	

	
	TimerInit();
	tstart = Timer();

	//Stores last key during DELAY msec
	while (Timer() <= (tstart + DELAY))
	{
	
			keyStatus  = ReadCombinedPadStatus();
			if(keyStatus)
				lastKey=keyStatus;
	}
	TimerEnd();

	

	//Deinits pad
	if (!isEarlyJap)
	{
	PadDeinitPads();
	}

	if (lastKey & PAD_CIRCLE)
	{
		if (file_exists("pfs0:ULE.ELF"))
			LoadElf("pfs0:ULE.ELF", "hdd0:PP.SOFTDEV2.APPS");

		

		if (file_exists("pfs0:OPNPS2LD.ELF"))
			LoadElf("pfs0:OPNPS2LD.ELF", "hdd0:PP.SOFTDEV2.APPS");

	}
	else
	{
		if (file_exists("pfs0:OPNPS2LD.ELF"))
			LoadElf("pfs0:OPNPS2LD.ELF", "hdd0:PP.SOFTDEV2.APPS");

		

		if (file_exists("pfs0:ULE.ELF"))
			LoadElf("pfs0:ULE.ELF", "hdd0:PP.SOFTDEV2.APPS");

	}


	argv[0]="BootBrowser";
	argv[1]="SkipHdd";


	LoadExecPS2("rom0:OSDSYS", 2, argv);
	return 0;

	
}
