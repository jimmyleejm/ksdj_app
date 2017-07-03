#ifndef	_FBVIDEO_H
#define	_FBVIDEO_H
/* fbvideo.h
 *
 * Copyright (c) 2009 Nuvoton technology corporation
 * All rights reserved.
 *  
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

extern __u32 g_u32VpostWidth, g_u32VpostHeight;
int InitDisplay(void);
void FiniDisplay(void);
int RenderDisplay(void* data, int toggle, int Srcwidth, int Srcheight, int Tarwidth, int Tarheight);

#endif	//_FBVIDEO_H


