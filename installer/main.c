// SoftDev2 Installer by alexparrado 2021

#include <iopheap.h>
#include <kernel.h>
#include <libcdvd.h>
#include <libmc.h>
#include <libhdd.h>
#include <libpad.h>
#include <loadfile.h>
#include <malloc.h>
#include <sbv_patches.h>
#include <sifcmd.h>
#include <sifrpc.h>
#include <stdio.h>
#include <string.h>
#include <io_common.h>
#include <sys/stat.h>
#include <stdint.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include "include/pad.h"
#include "include/iop.h"
#include "include/gui.h"
#include "include/textures.h"

#define FONT_SIZE 0.8F
#define SPACING 20

// External variables for GS
extern GSGLOBAL *gsGlobal;
extern GSTEXTURE gsTextures[TEXTURES_COUNT];

int file_exists(char filepath[])
{
	int fdn;

	fdn = open(filepath, O_RDONLY);
	if (fdn < 0)
		return 0;
	close(fdn);

	return 1;
}

// States of menu Finite State Machine (FSM)
enum
{
	STATE_INIT,
	STATE_HDD,
	STATE_ENABLE_HDD_BOOT,
	STATE_FORMAT,
	STATE_CONFIRM,
	STATE_INSTALLING_FHDB_MBR,
	STATE_INSTALLING_SOFTDEV2,
	STATE_EXIT
};

// From FMCB source
static int CheckFormat(void)
{
	int status;

	status = HDDCheckStatus();

	return status;
}

// To draw a selection screen
void drawSelectionScreen(internal_texture_t backgroundTexture, char *title, char *options[], int nOptions)
{

	internal_texture_t i;
	int prevH = 0;
	u64 WhiteFont = GS_SETREG_RGBAQ(0xFF, 0xFF, 0xFF, 0x80, 0x00);

	drawBackground(backgroundTexture);

	drawTexture(LOGO, gsGlobal->Width / 2 - gsTextures[LOGO].Width / 2, SPACING);

	prevH += gsTextures[LOGO].Height + SPACING;

	if (title)
	{
		drawFont(SPACING, SPACING + prevH, FONT_SIZE, WhiteFont, title);
		prevH += 3 * SPACING;
	}

	for (i = CROSS; i < (nOptions + CROSS); i++)
	{
		drawTexture(i, SPACING, prevH);
		drawFont(SPACING + 1.5 * gsTextures[i].Width, (gsTextures[i].Height - (int)(26.0f * FONT_SIZE)) / 2 + prevH, FONT_SIZE, WhiteFont, options[i - CROSS]);
		prevH += gsTextures[i].Height;
	}

	drawTexture(CIRCLE, SPACING, prevH);
	drawFont(SPACING + 1.5 * gsTextures[CIRCLE].Width, (gsTextures[i].Height - (int)(26.0f * FONT_SIZE)) / 2 + prevH, FONT_SIZE, WhiteFont, "Exit");

	draw();
}

// Main text menu implemented as a Finite State Machine (FSM)
int menu()
{

	int key;

	int hddStatus;
	int isOSDUpdate = 0;

	int ret = 1;

	static int state = STATE_INIT;
	static int formatStatus = 0;
	char *hddosd_party = "hdd0:__system";

	char *options[3];
	int nOptions;

	switch (state)
	{
	// State where installer starts
	case STATE_INIT:

		options[0] = "Continue";
		options[1] = "Enable HDD booting";
		nOptions = 2;
		drawSelectionScreen(BACKGROUND, "PLEASE SELECT ONE OPTION", options, nOptions);

		while (1)
		{
			key = ReadCombinedPadStatus();
			if (key & PAD_CROSS)
			{
				state = STATE_HDD;
				break;
			}
			if (key & PAD_SQUARE)
			{
				state = STATE_ENABLE_HDD_BOOT;
				break;
			}

			if (key & PAD_CIRCLE)
			{
				state = STATE_EXIT;
				break;
			}
		}

		break;
	case STATE_ENABLE_HDD_BOOT:

		EnableHDDBooting();

		nOptions = 0;
		drawSelectionScreen(BACKGROUND, "HDD BOOT ENABLED", options, nOptions);

		while (1)
		{
			key = ReadCombinedPadStatus();

			if (key & PAD_CIRCLE)
			{
				state = STATE_EXIT;
				break;
			}
		}

		break;

	// State where HDD is checked
	case STATE_HDD:

		hddStatus = CheckFormat();

		switch (hddStatus)
		{
		case 1: // Not formatted

			options[0] = "Format";
			nOptions = 1;
			drawSelectionScreen(BACKGROUND, "HDD IS NOT FORMATTED", options, nOptions);

			while (1)
			{
				key = ReadCombinedPadStatus();
				if (key & PAD_CROSS)
				{

					formatStatus = hddFormat();
					state = STATE_FORMAT;
					break;
				}

				if (key & PAD_CIRCLE)
				{
					state = STATE_EXIT;
					break;
				}
			}

			break;
		case 0: // Formatted
			state = STATE_CONFIRM;
			break;

		default: // Unknown errors

			nOptions = 0;
			drawSelectionScreen(BACKGROUND_ERROR, "HDD IS NOT CONNECTED OR IS UNUSABLE", options, nOptions);

			while (1)
			{
				key = ReadCombinedPadStatus();

				if (key & PAD_CIRCLE)
				{
					state = STATE_EXIT;
					break;
				}
			}
			break;
		}

		break;

	case STATE_FORMAT:

		if (formatStatus)
		{

			nOptions = 0;
			drawSelectionScreen(BACKGROUND_ERROR, "FORMAT FAILED", options, nOptions);
			while (1)
			{
				key = ReadCombinedPadStatus();

				if (key & PAD_CIRCLE)
				{
					state = STATE_EXIT;
					break;
				}
			}
		}
		else
		{

			nOptions = 1;
			options[0] = "Continue";
			drawSelectionScreen(BACKGROUND_SUCCESS, "FORMAT SUCCEDED", options, nOptions);
			while (1)
			{
				key = ReadCombinedPadStatus();

				if (key & PAD_CROSS)
				{
					state = STATE_CONFIRM;
					break;
				}

				if (key & PAD_CIRCLE)
				{
					state = STATE_EXIT;
					break;
				}
			}
		}

		break;

	// State where installation is confirmed
	case STATE_CONFIRM:

		if (fileXioMount("pfs0:", hddosd_party, FIO_MT_RDONLY) == 0)
		{

			// Tests for FHDB
			if (file_exists("pfs0:/osd/osdmain.elf"))
				isOSDUpdate = 1;

			// Tests for two locations of HDD-OSD
			if (file_exists("pfs0:/osd/hosdsys.elf"))
				isOSDUpdate = 1;

			if (file_exists("pfs0:/osd100/hosdsys.elf"))
				isOSDUpdate = 1;

			fileXioUmount("pfs0:");
		}

		options[0] = "Install SoftDev2 on HDD";
		nOptions = 1;

		if (isOSDUpdate)
		{
			options[1] = "Restore FreeHDBoot MBR";
			nOptions = 2;
		}
		drawSelectionScreen(BACKGROUND, "PLEASE SELECT ONE OPTION", options, nOptions);

		while (1)
		{
			key = ReadCombinedPadStatus();
			if (key & PAD_CROSS)
			{
				state = STATE_INSTALLING_SOFTDEV2;
				break;
			}

			if (isOSDUpdate)
			{
				if (key & PAD_SQUARE)
				{
					state = STATE_INSTALLING_FHDB_MBR;
					break;
				}
			}
			if (key & PAD_CIRCLE)
			{
				state = STATE_EXIT;
				break;
			}
		}

		break;
		// State where actual installation is carried out
	case STATE_INSTALLING_FHDB_MBR:

		nOptions = 0;
		drawSelectionScreen(BACKGROUND, "RESTORING FHDB MBR...", options, nOptions);

		if (InstallFHDBMBR() < 0)
			drawSelectionScreen(BACKGROUND_ERROR, "RESTORATION FAILED", options, nOptions);
		else
			drawSelectionScreen(BACKGROUND_SUCCESS, "RESTORATION SUCCEDED", options, nOptions);

		while (1)
		{
			key = ReadCombinedPadStatus();
			if (key & PAD_CIRCLE)
			{
				state = STATE_EXIT;
				break;
			}
		}

		state = STATE_EXIT;

		break;

	// State where SoftDev2 installation is carried out
	case STATE_INSTALLING_SOFTDEV2:

		nOptions = 0;
		drawSelectionScreen(BACKGROUND, "INSTALLING SOFTDEV2 ON HDD...", options, nOptions);

		if (InstallSoftDev2() < 0)
			drawSelectionScreen(BACKGROUND_ERROR, "INSTALLATION FAILED", options, nOptions);
		else
			drawSelectionScreen(BACKGROUND_SUCCESS, "INSTALLATION SUCCEDED", options, nOptions);

		while (1)
		{
			key = ReadCombinedPadStatus();
			if (key & PAD_CIRCLE)
			{
				state = STATE_EXIT;
				break;
			}
		}

		state = STATE_EXIT;

		break;
	case STATE_EXIT:
		ret = 0;

		break;

	default:
		break;
	}

	return ret;
}

// Main function
int main(int argc, char *argv[])
{

	// Inits PS2
	InitPS2();

	// Inits GUI
	gui_init(FONT_SIZE);

	// Displays menu
	while (menu())
		;

	// De-inits PS2
	DeInitPS2();

	__asm__ __volatile__(
		"	li $3, 0x04;"
		"	syscall;"
		"	nop;");

	return 0;
}
