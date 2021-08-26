//SoftDev2 Installer by alexparrado 2021

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
#include <sys/stat.h>
#include <stdint.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include "include/pad.h"
#include "include/iop.h"
#include "include/gui.h"
#include "include/textures.h"

#define FONT_SIZE1 25
#define FONT_SIZE2 25
#define SPACING 20

//External variables for GS
extern GSGLOBAL *gsGlobal;
extern GSTEXTURE gsTextures[TEXTURES_COUNT];

//States of menu Finite State Machine (FSM)
enum
{
	STATE_INIT,
	STATE_HDD,
	STATE_FORMAT,
	STATE_CONFIRM,
	STATE_INSTALLING,
	STATE_EXIT
};

//From FMCB source
static int CheckFormat(void)
{
	int status;

	status = HDDCheckStatus();

	return status;
}

//To draw a selection screen
void drawSelectionScreen(internal_texture_t backgroundTexture, char *title, char *options[], int nOptions)
{

	internal_texture_t i;
	int prevH = 0;

	drawBackground(backgroundTexture);
	

	drawTexture(LOGO, gsGlobal->Width / 2 - gsTextures[LOGO].Width / 2, SPACING);

	prevH += gsTextures[LOGO].Height+SPACING;

	if (title)
	{
		drawFont(SPACING, SPACING + prevH, FONT_SIZE1,  0xFFFFFF, title);
		prevH += 3*SPACING;
	}

	for (i = CROSS; i < (nOptions + CROSS); i++)
	{
		drawTexture(i, SPACING, prevH);
		drawFont(SPACING + 1.5*gsTextures[i].Width, prevH , FONT_SIZE2,  0xFFFFFF, options[i - CROSS]);
		prevH += gsTextures[i].Height;
	}

	drawTexture(CIRCLE, SPACING, prevH);
	drawFont(SPACING + 1.5*gsTextures[CIRCLE].Width, prevH, FONT_SIZE2,  0xFFFFFF, "Exit\n");

	draw();
}

//Main text menu implemented as a Finite State Machine (FSM)
int menu()
{

	int key;

	int hddStatus, formatStatus = 0;

	int ret = 1;

	static int state = STATE_INIT;

	char *options[3];
	int nOptions;

	switch (state)
	{
	//State where installer starts
	case STATE_INIT:

	
		options[0] = "Continue\n";
		nOptions = 1;
		drawSelectionScreen(BACKGROUND, "PLEASE SELECT ONE OPTION", options, nOptions);
		
		
		

		while (1)
		{
			key = ReadCombinedPadStatus();
			if (key & PAD_CROSS)
			{
				state = STATE_HDD;
				break;
			}

			if (key & PAD_CIRCLE)
			{
				state = STATE_EXIT;
				break;
			}
		}

		break;

	//State where HDD is checked
	case STATE_HDD:

		hddStatus = CheckFormat();

		switch (hddStatus)
		{
		case 1: //Not formatted

			options[0] = "Format";
			nOptions = 1;
			drawSelectionScreen(BACKGROUND, "HDD IS NOT FORMATTED\n", options, nOptions);

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
		case 0: //Formatted
			state = STATE_CONFIRM;
			break;

		default: //Unknown errors

			nOptions = 0;
			drawSelectionScreen(BACKGROUND_ERROR, "HDD IS NOT CONNECTED OR IS UNUSABLE\n", options, nOptions);

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
			drawSelectionScreen(BACKGROUND_ERROR, "FORMAT FAILED\n", options, nOptions);
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
			drawSelectionScreen(BACKGROUND_SUCCESS, "FORMAT SUCCEDED\n", options, nOptions);
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

	//State where installation is confirmed
	case STATE_CONFIRM:

		options[0] = "Install\n";
		nOptions = 1;
		drawSelectionScreen(BACKGROUND, "SOFTDEV2 WILL BE INSTALLED ON HDD\n", options, nOptions);

		while (1)
		{
			key = ReadCombinedPadStatus();
			if (key & PAD_CROSS)
			{
				state = STATE_INSTALLING;
				break;
			}
			if (key & PAD_CIRCLE)
			{
				state = STATE_EXIT;
				break;
			}
		}

		break;

	//State where actual installation is carried out
	case STATE_INSTALLING:

		nOptions = 0;
		drawSelectionScreen(BACKGROUND, "INSTALLING SOFTDEV2 ON HDD...\n", options, nOptions);

		if (InstallSoftDev2() < 0)
			drawSelectionScreen(BACKGROUND_ERROR, "INSTALLATION FAILED\n", options, nOptions);
		else
			drawSelectionScreen(BACKGROUND_SUCCESS, "INSTALLATION SUCCEDED\n", options, nOptions);

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

//Main function
int main(int argc, char *argv[])
{

	//Inits PS2
	InitPS2();

	//Inits GUI
	gui_init();
	
	

	//Displays menu
	while (menu())
		;

	//De-inits PS2
	DeInitPS2();

	__asm__ __volatile__(
		"	li $3, 0x04;"
		"	syscall;"
		"	nop;");

	return 0;
}
