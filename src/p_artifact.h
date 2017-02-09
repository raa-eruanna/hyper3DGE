//----------------------------------------------------------------------------
//  EDGE2 Heretic Inventory Code
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2017  The EDGE2 Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#ifndef __P_ARTIFACT__
#define __P_ARTIFACT__

#include "../ddf/main.h"

// maximum artifacts player can hold at once
#define MAXARTIFACTS  64

//
// Per-player Artifact Info.
//
typedef struct
{
	artifactdef_c *info;

	// player has this many items.
	int count;
	
}
playerartifact_t;

#endif // __P_ARTIFACT__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
