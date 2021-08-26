#include <gsKit.h>
#include <gsToolkit.h>
#include <stdarg.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "include/gui.h"

GSGLOBAL *gsGlobal;
GSTEXTURE gsTextures[TEXTURES_COUNT];
FT_Library library;
FT_Face face;

extern uint8_t font_ttf;
extern int size_font_ttf;



void drawBitmap(int x, int y, FT_Bitmap *bitmap, GSTEXTURE *Texture, u32 color)
{
	memset(Texture, 0, sizeof(GSTEXTURE));
	
	Texture->Delayed = 1;
	Texture->PSM = GS_PSM_CT32;
	Texture->Filter = GS_FILTER_NEAREST;
	Texture->VramClut = 0;
	Texture->Clut = NULL;
	
	Texture->Width = bitmap->width;
	Texture->Height = bitmap->rows;
	u32 TextureSize = gsKit_texture_size_ee(Texture->Width, Texture->Height, Texture->PSM);
	Texture->Mem = memalign(128, TextureSize);
	
	u32 *buff = (u32 *) Texture->Mem;
	
	memset(buff, 0, TextureSize);
	
	if (bitmap->pixel_mode == FT_PIXEL_MODE_GRAY)
	{
		for (FT_Int i = 0; i < bitmap->rows; i++)	
		{
			for (FT_Int j = 0; j < bitmap->width; j++)
			{
				u8 cl = bitmap->buffer[i * bitmap->width + j];
				buff[i * bitmap->width + j] = cl << 24 | color;
			}
		}
	}
	
	
	
	gsKit_texture_finish(gsGlobal, Texture);
	
	gsKit_TexManager_bind(gsGlobal, Texture);
	
	gsKit_prim_sprite_texture(gsGlobal, Texture, x, y, 0.0f, 0.0f, x + Texture->Width, y + Texture->Height, Texture->Width, Texture->Height, 2, GS_SETREG_RGBAQ(0xFF,0xFF,0xFF,0xFF,0x00));
}

void getTextSize(int size, const char *text, int *x, int *y)
{
	FT_GlyphSlot slot = face->glyph;
	u32 text_len = strlen(text);
	
	FT_Set_Pixel_Sizes(face, 0, size);
	
	*x = 0;
	*y = 0;
	
	for (int n = 0; n < text_len; n++)
	{
		int error = FT_Load_Char(face, text[n], FT_LOAD_RENDER);
		if (error)
			continue;

		*x += slot->advance.x >> 6;	
		*y += slot->advance.y >> 6;
	}
}

void drawText(int x, int y, int size, u32 color, const char *text)
{
	FT_GlyphSlot slot = face->glyph;
	u32 text_len = strlen(text);
	
	FT_Set_Pixel_Sizes(face, 0, size);
	
	GSTEXTURE *Texture;
	
	if (x < 0 || y < 0)
	{	
		int size_x, size_y;
		getTextSize(size, text, &size_x, &size_y);
		
		if (x < 0)
			x += gsGlobal->Width - size_x;		
		if (y < 0)
			y += gsGlobal->Height - size_y;
	}

	y += size;


	for (int n = 0; n < text_len; n++)
	{
		int error = FT_Load_Char(face, text[n], FT_LOAD_RENDER);
		if (error)
			continue;

		
		memset(Texture, 0, sizeof(GSTEXTURE));
		drawBitmap(x + slot->bitmap_left, y - slot->bitmap_top, &slot->bitmap, &Texture, color);

		
		x += slot->advance.x >> 6;
		y += slot->advance.y >> 6;
	}
	
	
}

void drawFont(int x, int y, int size, u32 color, const char *format, ...)
{
	char buffer[256];
	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	drawText(x, y, size, color, buffer);
	va_end(args);
	
}

void draw()
{
	gsKit_queue_exec(gsGlobal);

	gsKit_sync_flip(gsGlobal);

	gsKit_TexManager_nextFrame(gsGlobal);
}

void drawBackground(internal_texture_t tex)
{
	u64 texCol = GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x00);

	gsKit_TexManager_bind(gsGlobal, &gsTextures[tex]);

	gsKit_prim_sprite_texture(gsGlobal, &gsTextures[tex],
							  0.0f,					  // X1
							  0.0f,					  // Y2
							  0.0f,					  // U1
							  0.0f,					  // V1
							  gsTextures[tex].Width,  // X2
							  gsTextures[tex].Height, // Y2
							  gsTextures[tex].Width,  // U2
							  gsTextures[tex].Height, // V2
							  2,
							  texCol);
}

void drawTexture(internal_texture_t tex, float fx, float fy)
{
	u64 texCol = GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x00);

	gsKit_TexManager_bind(gsGlobal, &gsTextures[tex]);

	gsKit_prim_sprite_texture(gsGlobal, &gsTextures[tex],
							  fx,						   // X1
							  fy,						   // Y1
							  0.0f,						   // U1
							  0.0f,						   // V1
							  fx + gsTextures[tex].Width,  // X2
							  fy + gsTextures[tex].Height, // Y2
							  gsTextures[tex].Width,	   // U2
							  gsTextures[tex].Height,	   // V2
							  2,
							  texCol);
}

void gui_init()
{

	u64 Black = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x00, 0x00);
	gsGlobal = gsKit_init_global();
	
	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
				D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

	// Initialize the DMAC
	dmaKit_chan_init(DMA_CHANNEL_GIF);

	gsGlobal->DoubleBuffering = GS_SETTING_OFF;
	gsGlobal->ZBuffering = GS_SETTING_OFF;

	gsKit_init_screen(gsGlobal);
	gsKit_mode_switch(gsGlobal, GS_PERSISTENT);

	gsKit_clear(gsGlobal, Black);

	// Clear static loaded textures, use texture manager instead
	gsKit_vram_clear(gsGlobal);

	gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 128), 0);

	gsGlobal->PSM = GS_PSM_CT32;
	gsGlobal->PSMZ = GS_PSMZ_16S;
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;

	// Load all textures
	int i;
	for (i = 0; i < TEXTURES_COUNT; i++)
	{
		gsTextures[i].Delayed = 1;
		texPngLoad(&gsTextures[i], i);
		gsTextures[i].Filter = GS_FILTER_NEAREST;
	}
	
	FT_Init_FreeType(&library);
	FT_New_Memory_Face(library, &font_ttf, size_font_ttf, 0, &face);	
	
	
}
