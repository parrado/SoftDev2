#include <iopcontrol.h>
#include <iopheap.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <unistd.h>
#include <iopcontrol.h>
#include <iopheap.h>
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
#include <fcntl.h>
#include <stdlib.h>
#include <sys/times.h>
#include <hdd-ioctl.h>
#include "pad.h"

#define NEWLIB_PORT_AWARE
#include <fileio.h>

#define NTSC 2
#define PAL 3

#define DELAY 100L

// ELF-loading stuff
#define ELF_MAGIC 0x464c457f
#define ELF_PT_LOAD 1

//Extern references to embedded IRX/loaders.
extern u8 elf_loader_elf[];
extern int elf_size_loader_elf;

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
void LoadElf(char *filename, char *party);


u8 romver[16];
char romver_region_char[1];
char ROMVersionNumStr[5];

u32 bios_version = 0;

static void BootError(int argc, char **argv){
	static char *argv_BootBrowser[2]={
		"BootBrowser",
		NULL
	};

	fileXioDevctl("hdd:", HDIOC_DEV9OFF, NULL, 0, NULL, 0);

	if(argc<2){
		ExecOSD(1, argv_BootBrowser);
	}
	else{
		ExecOSD(argc, argv);
	}
}

/* Integrity checks for HDD
 * Under Sony design for MBR programs, the MBR was in charge of checking if S.M.A.R.T and HDD format are ok
 * If one of these checks fail, the MBR program task is simply looking for a FSCK program on __system and running it
 * WARNING: FreeHdBoot FSCK (Special build of SP193 HDDChecker) usage is heavily encouraged
 */
static int HDDCheckSMARTStatus(void)
{
    return (fileXioDevctl("hdd0:", APA_DEVCTL_SMART_STAT, NULL, 0, NULL, 0) != 0);
}

static int HDDCheckPartErrorStatus(void)
{
	if(fileXioDevctl("hdd0:", HDIOC_GETERRORPARTNAME, NULL, 0, ErrorPartName, sizeof(ErrorPartName))!=0){
		if(strcmp(ErrorPartName, "__system")==0) /* Do not continue if it is `__system` that has the error, since fsck is stored there. */
			BootError(argc, argv);
		return 1;
	}
}

/// @return true if HDD is not connected, formatted and ready to use 
static int HDDCheckHDIOCIssues(void)
{
	return (fileXioDevctl("hdd0:", HDIOC_STATUS, NULL, 0, NULL, 0)!=0);
}

static int HDDCheckSectorError(void)
{
	return (fileXioDevctl("hdd0:", HDIOC_GETSECTORERROR, NULL, 0, NULL, 0)!=0);
}


static void RunFSCK(char *hddosd_party)
{
	 if (fileXioMount("pfs0:", hddosd_party, FIO_MT_RDONLY) == 0)
	 {
		if (file_exists("pfs0:/fsck/fsck.elf"))
			LoadElf( "pfs0:/fsck/fsck.elf", hddosd_party);
		if (file_exists("pfs0:/fsck100/fsck.elf"))
			LoadElf( "pfs0:/fsck100/fsck.elf", hddosd_party);
         } 
         BootError(); // Go to OSDSYS memcard browser if FSCK can't be found or if __system can't be mounted
}
//--------

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

	static const char PFS_args[] = "-n\0"
								   "24\0"
								   "-o\0"
								   "8";
	ResetIOP();

	SifInitIopHeap();
	SifLoadFileInit();
	fioInit();

	sbv_patch_enable_lmb();
	sbv_patch_disable_prefix_check();

	SifExecModuleBuffer(IOMANX_irx, size_IOMANX_irx, 0, NULL, NULL);
	SifExecModuleBuffer(FILEXIO_irx, size_FILEXIO_irx, 0, NULL, NULL);

	fileXioInit();

	SifExecModuleBuffer(DEV9_irx, size_DEV9_irx, 0, NULL, NULL);
	SifExecModuleBuffer(SIO2MAN_irx, size_SIO2MAN_irx, 0, NULL, NULL);
	SifExecModuleBuffer(PADMAN_irx, size_PADMAN_irx, 0, NULL, NULL);
	PadInitPads();

	if (SifExecModuleBuffer(ATAD_irx, size_ATAD_irx, 0, NULL, NULL) >= 0)
	{
		SifExecModuleBuffer(HDD_irx, size_HDD_irx, 0, NULL, NULL);
		SifExecModuleBuffer(PFS_irx, size_PFS_irx, sizeof(PFS_args), PFS_args, NULL);
	}

	SifExitIopHeap();
	SifLoadFileExit();
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
	boot_elf = (u8 *)elf_loader_elf;
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

	int lastKey = 0;
	int keyStatus;
	int isEarlyJap = 0;
	struct tms tstart, tstop;

	char *party = "hdd0:__sysconf";
	char *hddosd_party = "hdd0:__system";

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

// Perform the checks in order

	if (HDDCheckHDIOCIssues()) // HDD has connection isues or it's not formatted?
		BootError(argc, argv);

	if (HDDCheckSMARTStatus()) // S.M.A.R.T first. who cares of filesystem is HDD is nearly dead?
		RunFSCK(hddosd_party);

	if (HDDCheckSectorError()) // sector damage second...
		RunFSCK(hddosd_party);

	if (HDDCheckPartErrorStatus()) // Check for partition error. will boot FSCK unless the damaged partition is the one wich holds FSCK
		RunFSCK(hddosd_party);


	if (fileXioMount("pfs0:", party, FIO_MT_RDONLY) == 0)
	{

		times(&tstart);
		//Stores last key during DELAY msec
		do
		{

			keyStatus = ReadCombinedPadStatus();
			if (keyStatus)
				lastKey = keyStatus;
			times(&tstop);
		} while (tstop.tms_utime <= (tstart.tms_utime + DELAY));

		//Deinits pad
		if (!isEarlyJap)
		{
			padPortClose(0, 0);
			padPortClose(1, 0);
			padEnd();
		}

		if (lastKey & PAD_TRIANGLE)
		{
			//Check if HDD-OSD is installed
			fileXioUmount("pfs0:");
			if (fileXioMount("pfs0:", hddosd_party, FIO_MT_RDONLY) == 0)
			{

				//Tests for FHDB
				if (file_exists("pfs0:/osd/osdmain.elf"))
					LoadElf("pfs0:/osd/osdmain.elf", hddosd_party);

				//Tests for two locations of HDD-OSD
				if (file_exists("pfs0:/osd/hosdsys.elf"))
					LoadElf("pfs0:/osd/hosdsys.elf", hddosd_party);

				if (file_exists("pfs0:/osd100/hosdsys.elf"))
					LoadElf("pfs0:/osd100/hosdsys.elf", hddosd_party);

				fileXioUmount("pfs0:");
			}

			//If no HDD-OSD was found, then launch ROM OSD
			LoadElf("rom0:OSDSYS", "hdd0:");
		}

		if (lastKey & PAD_CIRCLE)
		{

			if (file_exists("pfs0:/softdev2/ULE.ELF"))
				LoadElf("pfs0:/softdev2/ULE.ELF", party);

			if (file_exists("pfs0:/softdev2/OPNPS2LD.ELF"))
				LoadElf("pfs0:/softdev2/OPNPS2LD.ELF", party);
		}

		if (file_exists("pfs0:/softdev2/OPNPS2LD.ELF"))
			LoadElf("pfs0:/softdev2/OPNPS2LD.ELF", party);

		if (file_exists("pfs0:/softdev2/ULE.ELF"))
			LoadElf("pfs0:/softdev2/ULE.ELF", party);

		fileXioUmount("pfs0:");
	}

	//Check if HDD-OSD is installed
	if (fileXioMount("pfs0:", hddosd_party, FIO_MT_RDONLY) == 0)
	{
	
		//Tests for FHDB
		if (file_exists("pfs0:/osd/osdmain.elf"))
			LoadElf("pfs0:/osd/osdmain.elf", hddosd_party);
		//Tests for two locations of HDD-OSD
		if (file_exists("pfs0:/osd/hosdsys.elf"))
			LoadElf("pfs0:/osd/hosdsys.elf", hddosd_party);

		if (file_exists("pfs0:/osd100/hosdsys.elf"))
			LoadElf("pfs0:/osd100/hosdsys.elf", hddosd_party);

		fileXioUmount("pfs0:");
	}

	//If no HDD-OSD was found, then launch ROM OSD with error argumment, to prevent endles loop 
	BootError(argc, argv);
	return 0;
}
