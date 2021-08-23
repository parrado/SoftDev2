#include <gsKit.h>
#include <gsToolkit.h>
#include <stdarg.h>

#include "include/gui.h"




GSGLOBAL *gsGlobal;
GSFONTM *gsFontM ;
GSTEXTURE gsTextures[TEXTURES_COUNT];

void draw()
{
	gsKit_queue_exec(gsGlobal);

	gsKit_sync_flip(gsGlobal);

	gsKit_TexManager_nextFrame(gsGlobal);
}

void drawBackground( internal_texture_t tex)
{
	u64 texCol = GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00);

	gsKit_TexManager_bind(gsGlobal, &gsTextures[tex]);

	gsKit_prim_sprite_texture(gsGlobal, &gsTextures[tex],
						0.0f,  // X1
						0.0f,  // Y2
						0.0f,  // U1
						0.0f,  // V1
						gsTextures[tex].Width, // X2
						gsTextures[tex].Height, // Y2
						gsTextures[tex].Width, // U2
						gsTextures[tex].Height, // V2
						2,
						texCol);
}

void drawTexture( internal_texture_t tex, float fx, float fy)
{
	u64 texCol = GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00);

	gsKit_TexManager_bind(gsGlobal, &gsTextures[tex]);

	gsKit_prim_sprite_texture(gsGlobal, &gsTextures[tex],
						fx,  // X1
						fy,  // Y1
						0.0f,  // U1
						0.0f,  // V1
						fx + gsTextures[tex].Width, // X2
						fy +  gsTextures[tex].Height, // Y2
						gsTextures[tex].Width, // U2
						gsTextures[tex].Height, // V2
						2,
						texCol);
}

void drawFont(float x, float y, float scale, char *text)
{
	u64 WhiteFont = GS_SETREG_RGBAQ(0xFF,0xFF,0xFF,0x80,0x00);	

	gsKit_fontm_print_scaled(gsGlobal, gsFontM, x, y, 0, scale, WhiteFont, text);
}





void *ui_printf(int x, int y, int size, u32 color, const char *format, ...)
{
	char buffer[256];
	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	//struct GSTEXTURE_holder *Textures = draw_text(x, y, size, color, buffer);
	va_end(args);
	//return Textures;
}


void gui_init(){

	u64 Black = GS_SETREG_RGBAQ(0x00,0x00,0x00,0x00,0x00);
	gsGlobal = gsKit_init_global();
	gsFontM = gsKit_init_fontm();



	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
			D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

	// Initialize the DMAC
	dmaKit_chan_init(DMA_CHANNEL_GIF);

	gsGlobal->DoubleBuffering = GS_SETTING_OFF;
	gsGlobal->ZBuffering = GS_SETTING_OFF;

	gsKit_init_screen(gsGlobal);
	gsKit_mode_switch(gsGlobal, GS_PERSISTENT);

	gsKit_fontm_upload(gsGlobal, gsFontM);
	gsFontM->Spacing = 0.75f;
	
	

	gsKit_clear(gsGlobal, Black);

	// Clear static loaded textures, use texture manager instead
	gsKit_vram_clear(gsGlobal);

	gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 128), 0);
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	
	gsGlobal->PSM = GS_PSM_CT24;
	gsGlobal->PSMZ = GS_PSMZ_16S;
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	

	
		// Load all textures
	int i;
	for (i = 0; i < TEXTURES_COUNT; i++)
	{
		gsTextures[i].Delayed = 1;
		texPngLoad(&gsTextures[i], i);
		gsTextures[i].Filter = GS_FILTER_LINEAR;
	}

}
