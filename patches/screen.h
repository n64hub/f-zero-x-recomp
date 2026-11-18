#ifndef __SCREEN_H__
#define __SCREEN_H__

#include "patches.h"

// A framed screen at the picture's full height (screen.c).

// How much a framed screen of that game mode is scaled now, 0 with Aspect Ratio Original, and the line its picture's
// middle is.
f32 Screen_GetFill(s32 mode, f32* centreY);
// The commands from start to end, a framed screen's, scaled by fill about centreY.
void Screen_Fill(Gfx* start, Gfx* end, f32 fill, f32 centreY);

#endif
