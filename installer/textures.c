#include <gsKit.h>
#include <malloc.h>
#include <png.h>
#include "include/textures.h"

extern void *background_png;
extern void *background_success_png;
extern void *background_error_png;
extern void *logo_png;
extern void *cross_png;
extern void *circle_png;
extern void *square_png;
extern void *triangle_png;

typedef struct
{
	int id;
	void **texture;
} texture_t;

static texture_t internalTexture[TEXTURES_COUNT] = {
	{BACKGROUND, &background_png},
	{BACKGROUND_SUCCESS, &background_success_png},
	{BACKGROUND_ERROR, &background_error_png},
	{LOGO, &logo_png},
	{CROSS, &cross_png},
	{SQUARE, &square_png},
	{TRIANGLE, &triangle_png},
	{CIRCLE, &circle_png}
};

static int texSizeValidate(int width, int height, short psm)
{
	if (width > 1024 || height > 1024)
		return -1;

	if (gsKit_texture_size(width, height, psm) > 720*512*4)
		return -1;

	return 0;
}

typedef struct
{
	u8 red;
	u8 green;
	u8 blue;
	u8 alpha;
} png_clut_t;

typedef struct
{
	png_colorp palette;
	int numPalette;
	int numTrans;
	png_bytep trans;
	png_clut_t *clut;
} png_texture_t;

static png_texture_t pngTexture;

static int texPngEnd(png_structp pngPtr, png_infop infoPtr, int status)
{
	if (infoPtr != NULL)
		png_destroy_read_struct(&pngPtr, &infoPtr, (png_infopp)NULL);

	return status;
}

static void texPngReadMemFunction(png_structp pngPtr, png_bytep data, png_size_t length)
{
	void **PngBufferPtr = png_get_io_ptr(pngPtr);

	memcpy(data, *PngBufferPtr, length);
	*PngBufferPtr = (u8 *)(*PngBufferPtr) + length;
}

static void texPngReadPixels4(GSTEXTURE *texture, png_bytep *rowPointers)
{
	unsigned char *pixel = (unsigned char *)texture->Mem;
	png_clut_t *clut = (png_clut_t *)texture->Clut;

	int i, j, k = 0;

	for (i = pngTexture.numPalette; i < 16; i++) {
		memset(&clut[i], 0, sizeof(clut[i]));
	}

	for (i = 0; i < pngTexture.numPalette; i++) {
		clut[i].red = pngTexture.palette[i].red;
		clut[i].green = pngTexture.palette[i].green;
		clut[i].blue = pngTexture.palette[i].blue;
		clut[i].alpha = 0x80;
	}

	for (i = 0; i < pngTexture.numTrans; i++)
		clut[i].alpha = pngTexture.trans[i] >> 1;

	for (i = 0; i < texture->Height; i++) {
		for (j = 0; j < texture->Width / 2; j++) {
			memcpy(&pixel[k], &rowPointers[i][1 * j], 1);
			pixel[k] = (pixel[k] << 4) | (pixel[k] >> 4);
			k++;
		}
	}
}

static void texPngReadPixels8(GSTEXTURE *texture, png_bytep *rowPointers)
{
	unsigned char *pixel = (unsigned char *)texture->Mem;
	png_clut_t *clut = (png_clut_t *)texture->Clut;

	int i, j, k = 0;

	for (i = pngTexture.numPalette; i < 256; i++) {
		memset(&clut[i], 0, sizeof(clut[i]));
	}

	for (i = 0; i < pngTexture.numPalette; i++) {
		clut[i].red = pngTexture.palette[i].red;
		clut[i].green = pngTexture.palette[i].green;
		clut[i].blue = pngTexture.palette[i].blue;
		clut[i].alpha = 0x80;
	}

	for (i = 0; i < pngTexture.numTrans; i++)
		clut[i].alpha = pngTexture.trans[i] >> 1;

	// rotate clut
	for (i = 0; i < pngTexture.numPalette; i++) {
		if ((i&0x18) == 8) {
			png_clut_t tmp = clut[i];
			clut[i] = clut[i+8];
			clut[i+8] = tmp;
		}
	}

	for (i = 0; i < texture->Height; i++) {
		for (j = 0; j < texture->Width; j++) {
			memcpy(&pixel[k++], &rowPointers[i][1 * j], 1);
		}
	}
}

static void texPngReadPixels24(GSTEXTURE *texture, png_bytep *rowPointers)
{
	struct pixel3
	{
		unsigned char r, g, b;
	};
	struct pixel3 *Pixels = (struct pixel3 *)texture->Mem;

	int i, j, k = 0;
	for (i = 0; i < texture->Height; i++) {
		for (j = 0; j < texture->Width; j++) {
			memcpy(&Pixels[k++], &rowPointers[i][4 * j], 3);
		}
	}
}

static void texPngReadPixels32(GSTEXTURE *texture, png_bytep *rowPointers)
{
	struct pixel
	{
		unsigned char r, g, b, a;
	};
	struct pixel *Pixels = (struct pixel *)texture->Mem;

	int i, j, k = 0;
	for (i = 0; i < texture->Height; i++) {
		for (j = 0; j < texture->Width; j++) {
			memcpy(&Pixels[k], &rowPointers[i][4 * j], 3);
			Pixels[k++].a = rowPointers[i][4 * j + 3] >> 1;
		}
	}
}

static void texPngReadData(GSTEXTURE *texture, png_structp pngPtr, png_infop infoPtr,
						   void (*texPngReadPixels)(GSTEXTURE *texture, png_bytep *rowPointers))
{
	int row, rowBytes = png_get_rowbytes(pngPtr, infoPtr);
	size_t size = gsKit_texture_size_ee(texture->Width, texture->Height, texture->PSM);
	texture->Mem = memalign(128, size);

	// failed allocation
	if (!texture->Mem) {
		printf("TEXTURES PngReadData: Failed to allocate %d bytes\n", size);
		return;
	}

	png_bytep *rowPointers = calloc(texture->Height, sizeof(png_bytep));
	for (row = 0; row < texture->Height; row++) {
		rowPointers[row] = malloc(rowBytes);
	}
	png_read_image(pngPtr, rowPointers);

	texPngReadPixels(texture, rowPointers);

	for (row = 0; row < texture->Height; row++)
		free(rowPointers[row]);

	free(rowPointers);

	png_read_end(pngPtr, NULL);
}

int texPngLoad(GSTEXTURE *texture, int texID)
{
	png_structp pngPtr = NULL;
	png_infop infoPtr = NULL;
	png_voidp readData = NULL;
	png_rw_ptr readFunction = NULL;
	void **PngFileBufferPtr;

	texture->ClutPSM = 0;
	texture->Filter = GS_FILTER_LINEAR;
	texture->Mem = NULL;
	texture->Vram = 0;
	texture->VramClut = 0;
	texture->Clut = NULL;

	if (!internalTexture[texID].texture)
		return -1;

	PngFileBufferPtr = internalTexture[texID].texture;
	readData = &PngFileBufferPtr;
	readFunction = &texPngReadMemFunction;

	pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, NULL, NULL);
	if (!pngPtr)
		return texPngEnd(pngPtr, infoPtr, -2);

	infoPtr = png_create_info_struct(pngPtr);
	if (!infoPtr)
		return texPngEnd(pngPtr, infoPtr, -3);

	if (setjmp(png_jmpbuf(pngPtr)))
		return texPngEnd(pngPtr, infoPtr, -4);

	png_set_read_fn(pngPtr, readData, readFunction);

	unsigned int sigRead = 0;
	png_set_sig_bytes(pngPtr, sigRead);
	png_read_info(pngPtr, infoPtr);

	png_uint_32 pngWidth, pngHeight;
	int bitDepth, colorType, interlaceType;
	png_get_IHDR(pngPtr, infoPtr, &pngWidth, &pngHeight, &bitDepth, &colorType, &interlaceType, NULL, NULL);
	texture->Width = pngWidth;
	texture->Height = pngHeight;

	if (colorType == PNG_COLOR_TYPE_GRAY)
		png_set_expand(pngPtr);

	if (bitDepth == 16)
		png_set_strip_16(pngPtr);

	png_set_filler(pngPtr, 0x80, PNG_FILLER_AFTER);
	png_read_update_info(pngPtr, infoPtr);

	void (*texPngReadPixels)(GSTEXTURE * texture, png_bytep * rowPointers);
	switch (png_get_color_type(pngPtr, infoPtr)) {
		case PNG_COLOR_TYPE_RGB_ALPHA:
			texture->PSM = GS_PSM_CT32;
			texPngReadPixels = &texPngReadPixels32;
			break;
		case PNG_COLOR_TYPE_RGB:
			texture->PSM = GS_PSM_CT24;
			texPngReadPixels = &texPngReadPixels24;
			break;
		case PNG_COLOR_TYPE_PALETTE:
			pngTexture.palette = NULL;
			pngTexture.numPalette = 0;
			pngTexture.trans = NULL;
			pngTexture.numTrans = 0;

			png_get_PLTE(pngPtr, infoPtr, &pngTexture.palette, &pngTexture.numPalette);
			png_get_tRNS(pngPtr, infoPtr, &pngTexture.trans, &pngTexture.numTrans, NULL);
			texture->ClutPSM = GS_PSM_CT32;

			if (bitDepth == 4) {
				texture->PSM = GS_PSM_T4;
				texture->Clut = memalign(128, gsKit_texture_size_ee(8, 2, GS_PSM_CT32));
				memset(texture->Clut, 0, gsKit_texture_size_ee(8, 2, GS_PSM_CT32));

				texPngReadPixels = &texPngReadPixels4;
			}
			else {
				texture->PSM = GS_PSM_T8;
				texture->Clut = memalign(128, gsKit_texture_size_ee(16, 16, GS_PSM_CT32));
				memset(texture->Clut, 0, gsKit_texture_size_ee(16, 16, GS_PSM_CT32));

				texPngReadPixels = &texPngReadPixels8;
			}
			break;
		default:
			return texPngEnd(pngPtr, infoPtr, -6);
	}

	if (texSizeValidate(texture->Width, texture->Height, texture->PSM) < 0) {
		if (texture->Clut) {
			free(texture->Clut);
			texture->Clut = NULL;
		}

		return texPngEnd(pngPtr, infoPtr, -7);
	}

	texPngReadData(texture, pngPtr, infoPtr, texPngReadPixels);

	return texPngEnd(pngPtr, infoPtr, 0);
}
