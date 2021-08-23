#ifndef __GUI_H
#define __GUI_H

#include "textures.h"

void drawBackground(internal_texture_t tex);
void drawTexture(internal_texture_t tex, float fx, float fy);
void drawFont(float x, float y, float scale, char *text);

#endif
