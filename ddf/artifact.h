//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Main)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
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

#ifndef __DDF_ARTIFACT_H__
#define __DDF_ARTIFACT_H__

#include "../epi/utility.h"

#include "types.h"
#include "states.h"


// ------------------------------------------------------------------
// ----------------------ARTIFACT HANDLING---------------------------
// ------------------------------------------------------------------

class artifactdef_c
{
public:
	artifactdef_c();
	~artifactdef_c();
	
public:
	void Default(void);
	void CopyDetail(artifactdef_c &src);

	// Artifact's name, etc...
	epi::strent_c name;

	effectdef_c *effect;	// Effect this artifact gives.
  
	int maxamount;			// Max player can hold
	int artiicon;			// Graphic used to show this artifact

public:

private:
	// disable copy construct and assignment operator
	explicit artifactdef_c(artifactdef_c &rhs) { }
	artifactdef_c& operator= (artifactdef_c &rhs) { return *this; }
};


class artifactdef_container_c : public epi::array_c
{
public:
	artifactdef_container_c();
	~artifactdef_container_c();

private:
	void CleanupObject(void *obj);

public:
	// List Management
	int GetSize() {	return array_entries; } 
	int Insert(artifactdef_c *a) { return InsertObject((void*)&a); }
	
	artifactdef_c* operator[](int idx) 
	{ 
		return *(artifactdef_c**)FetchObject(idx); 
	} 

	// Search Functions
	int FindFirst(const char *name, int startpos = -1);
	artifactdef_c* Lookup(const char* refname);
};


// -------EXTERNALISATIONS-------

extern artifactdef_container_c artifactdefs;	// -ACB- 2004/07/14 Implemented

bool DDF_ReadArtifacts(void *data, int size);

#endif // __DDF_ARTIFACT_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
