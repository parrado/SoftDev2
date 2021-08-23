//SoftDev2 Installer by alexparrado 2021
#include <iopheap.h>
#include <kernel.h>
#include <iox_stat.h>
#include <libcdvd.h>
#include <libmc.h>
#include <libpad.h>
#include <errno.h>
#include <hdd-ioctl.h>
#include <io_common.h>
#include <malloc.h>
#include <sifcmd.h>
#include <sifrpc.h>
#include <stdio.h>
#include <string.h>
#include <timer.h>
#include <usbhdfsd-common.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

extern int errno __attribute__((section("data")));

//Embedded install files
extern unsigned char OPL_elf[];
extern unsigned int size_OPL_elf;

extern unsigned char ULE_elf[];
extern unsigned int size_ULE_elf;

extern unsigned char MBR_xin[];
extern unsigned int size_MBR_xin;


/////////////From FHDB 1.9 installer source code////////////////////
int HDDCheckStatus(void)
{
	int status;

	status = fileXioDevctl("hdd0:", HDIOC_STATUS, NULL, 0, NULL, 0);

	if (status == 0)
		fileXioRemove("hdd0:_tmp"); //Remove _tmp, if it exists.

	return status;
}

static int EnableHDDBooting(void)
{
	int OpResult, result;
	unsigned char OSDConfigBuffer[15];

	do
	{
		sceCdOpenConfig(0, 0, 1, &OpResult);
	} while (OpResult & 9);

	do
	{
		result = sceCdReadConfig(OSDConfigBuffer, &OpResult);
	} while (OpResult & 9 || result == 0);

	do
	{
		result = sceCdCloseConfig(&OpResult);
	} while (OpResult & 9 || result == 0);

	if ((OSDConfigBuffer[0] & 3) != 2)
	{ //If ATAD support and HDD booting are not already activated.
		OSDConfigBuffer[0] = (OSDConfigBuffer[0] & ~3) | 2;

		do
		{
			sceCdOpenConfig(0, 1, 1, &OpResult);
		} while (OpResult & 9);

		do
		{
			result = sceCdWriteConfig(OSDConfigBuffer, &OpResult);
		} while (OpResult & 9 || result == 0);

		do
		{
			result = sceCdCloseConfig(&OpResult);
		} while (OpResult & 9 || result == 0);

		result = 0;
	}
	else
		result = 1;

	return result;
}
/////////////////////////////////////////////////////////////////////

//To create file from embedded buffer
int createFileFromBuffer(char *fileName, char *buffer, int size)
{
	FILE *file;
	int result = 0;

	file = fopen(fileName, "w");
	if (file)
	{
		if (fwrite(buffer, 1, size, file) != size)
			result = -1;

		fclose(file);
	}
	else
		result = -1;

	return result;
}

//To inject MBR from embedded buffer (MBR.XIN)
int injectMBR(char *buffer, int size)
{

	int result;
	iox_stat_t statFile;
	unsigned int numSectors;
	unsigned int sector;
	unsigned int remainder;
	unsigned int i;
	unsigned int numBytes;
	hddSetOsdMBR_t MBRInfo;

	//Buffer for I/O devctl operations on HDD
	uint8_t IOBuffer[512 + sizeof(hddAtaTransfer_t)] __attribute__((aligned(64)));

	//IF stats can be retrieved
	if ((result = fileXioGetStat("hdd0:__mbr", &statFile)) >= 0)
	{
		//Sector to write
		sector = statFile.private_5 + 0x2000;

		//Bytes in last sector
		remainder = (size & 0x1FF);		

		//Total sectors to inject
		numSectors = (size / 512) + ((remainder) ? 1 : 0);
		numBytes = 512;

		//Writes sectors
		for (i = 0; i < numSectors; i++)
		{
			//If last sector
			if ((i == (numSectors - 1)) && (remainder != 0))
			{
				numBytes = remainder;
				//Performs read operation for one sector
				((hddAtaTransfer_t *)IOBuffer)->lba = sector + i;
				((hddAtaTransfer_t *)IOBuffer)->size = 1;
				if ((result = fileXioDevctl("hdd0:", APA_DEVCTL_ATA_READ, IOBuffer, sizeof(hddAtaTransfer_t), IOBuffer + sizeof(hddAtaTransfer_t), 512)) < 0)
					break;
			}
			//Copies from buffer
			memcpy(IOBuffer + sizeof(hddAtaTransfer_t), buffer + 512 * i, numBytes);
			//Performs write operation for one sector
			((hddAtaTransfer_t *)IOBuffer)->lba = sector + i;
			((hddAtaTransfer_t *)IOBuffer)->size = 1;

			if ((result = fileXioDevctl("hdd0:", APA_DEVCTL_ATA_WRITE, IOBuffer, 512 + sizeof(hddAtaTransfer_t), NULL, 0)) < 0)
				break;
		}
		//Writes MBR information
		if (result >= 0)
		{
			MBRInfo.start = sector;
			MBRInfo.size = numSectors;
			fileXioDevctl("hdd0:", APA_DEVCTL_SET_OSDMBR, &MBRInfo, sizeof(MBRInfo), NULL, 0);
		}
	}
	return result;
}


//Main installation function
int InstallSoftDev2()
{

	char *party = "hdd0:__sysconf";
	int result;

	if ((result = fileXioMount("pfs0:", party, FIO_MT_RDWR)) == 0)
	{
		result = mkdir("pfs0:/softdev2", 0777);		
		
		
		if ((result==0)||((result==-1) && (errno=EEXIST)))
		{
			result=0;
			if ((result = createFileFromBuffer("pfs0:/softdev2/OPNPS2LD.ELF", OPL_elf, size_OPL_elf)) == 0)
				if ((result = createFileFromBuffer("pfs0:/softdev2/ULE.ELF", ULE_elf, size_ULE_elf)) == 0)
				{
					if ((result = injectMBR(MBR_xin, size_MBR_xin)) >= 0)
						result = EnableHDDBooting();
				}
		}
	}

	return result;
}










