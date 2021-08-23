#ifndef __TEXTURES_H
#define __TEXTURES_H


typedef enum  {
	BACKGROUND = 0,
	BACKGROUND_SUCCESS,
	BACKGROUND_ERROR,
	LOGO,
	CROSS,
	SQUARE,
	TRIANGLE,
	CIRCLE,
	TEXTURES_COUNT
} internal_texture_t;

int texPngLoad(GSTEXTURE *texture, int texID);

#endif
