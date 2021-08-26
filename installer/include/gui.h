#ifndef __GUI_H
#define __GUI_H

#include "textures.h"

void drawBackground(internal_texture_t tex);
void drawTexture(internal_texture_t tex, float fx, float fy);
void drawFont(int x, int y, int size, u32 color, const char *format, ...);

#endif
