//---------------------------------------------------------------------------
//  EDGE2 Main Init + Program Loop Code
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2009  The EDGE2 Team.
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
//
// DESCRIPTION:
//      EDGE2 main program (E_Main),
//      game loop (E_Loop) and startup functions.
//
// -MH- 1998/07/02 "shootupdown" --> "true3dgameplay"
// -MH- 1998/08/19 added up/down movement variables
//

#include "system/i_defs.h"
#include "e_main.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#ifndef LINUX
#include <libcpuid.h>
#else
#include <libcpuid/libcpuid.h>
#endif
#ifdef HAVE_PHYSFS
#include <physfs.h>
#endif

#include "../epi/exe_path.h"
#include "../epi/file.h"
#include "../epi/filesystem.h"
#include "../epi/path.h"
#include "../epi/utility.h"

#include "am_map.h"
#include "con_gui.h"
#include "con_main.h"
#include "con_var.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "games/wolf3d/wlf_local.h"
//#include "games/rott/rott_local.h"
#include "dstrings.h"
#include "e_input.h"
#include "f_finale.h"
#include "f_interm.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "l_glbsp.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_cheatcodes.h"
#include "m_misc.h"
#include "m_menu.h"
#include "n_network.h"
#include "p_setup.h"
#include "p_spec.h"
#include "r_local.h"
#include "rad_trig.h"
#include "r_gldefs.h"
#include "r_wipe.h"
#include "s_sound.h"
#include "s_music.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_modes.h"
#include "r_image.h"
#include "w_model.h"
#include "w_sprite.h"
#include "w_texture.h"
#include "games/wolf3d/wlf_rawdef.h"
#include "w_wad.h"
#include "version.h"
#include "vm_coal.h"
#include "z_zone.h"



#define E_TITLE  "EDGE2 v" EDGEVERSTR

// Application active?
int app_state = APP_STATE_ACTIVE;

bool singletics = false;  // debug flag to cancel adaptiveness

bool splitscreen_mode = false;

bool wolf3d_mode = false; //Wolfenstein 3D game detection . . . kind of a 'hack'

bool rott_mode = false; //hack!

bool heretic_mode = false; //hack!

bool show_splash = true;

bool no_render_buffers = false;

// -ES- 2000/02/13 Takes screenshot every screenshot_rate tics.
// Must be used in conjunction with singletics.
static int screenshot_rate;

// For screenies...
bool m_screenshot_required = false;
bool need_save_screenshot = false;

FILE *logfile = NULL;
FILE *debugfile = NULL;

cvar_c i_skipsplash;

gameflags_t default_gameflags =
{
	false,  // nomonsters
	false,  // fastparm

	false,  // respawn
	false,  // res_respawn
	false,  // item respawn

	false,  // true 3d gameplay
	MENU_GRAV_NORMAL, // gravity
	false,  // more blood

	true,   // jump
	true,   // crouch
	true,   // mlook
	AA_ON,  // autoaim

	true,   // cheats
	true,   // have_extra
	false,  // limit_zoom
	true,  // shadows
	false,  // halos

	false,    // edge_compat
	true,     // kicking
	true,     // weapon_switch
	true,     // pass_missile
	false,    // team_damage
};

// -KM- 1998/12/16 These flags are the users prefs and are copied to
//   gameflags when a new level is started.
// -AJA- 2000/02/02: Removed initialisation (done in code using
//       `default_gameflags').

gameflags_t global_flags;

int newnmrespawn = 0;

bool swapstereo = false;
bool mus_pause_stop = false;
bool png_scrshots = false;
bool autoquickload = false;

std::string cfgfile;
std::string ewadfile;
std::string epakfile; //<---- EDGE PAK FILE
std::string iwad_base;
std::string wolf_base; //<--- Wolfenstein file?

std::string cache_dir;
std::string ddf_dir;
std::string game_dir;
std::string home_dir;
std::string save_dir;
std::string shot_dir;

extern cvar_c m_language;
extern cvar_c g_aggression;

cvar_c debug_testlerp;
cvar_c ddf_strict;
cvar_c ddf_lax;
cvar_c ddf_quiet;

cvar_c r_gpuswitch;

static void E_TitleDrawer(void);


class startup_progress_c
{
private:
	int perc;
	int g_step, g_size, g_total;  // global progress
	int l_step, l_total;   // local progress

public:
	startup_progress_c() :
		perc(-1),  // force update on initial setting
		g_step(0), g_size(0), g_total(0),
		l_step(0), l_total(0) { }

	~startup_progress_c() { }

	void recomputePercent()
	{
		int pp = (100 * g_step + (100 * g_size * l_step / l_total)) / g_total;

		SYS_ASSERT(0 <= pp && pp <= 100);

		if (pp != perc)
		{
			perc = pp; drawIt();
		}
	}

	void drawIt(int glbsp_perc = -1)  // a bit kludgy...
	{
		RGL_DrawProgress(perc, glbsp_perc);
	}

	void setGlobal(int step, int size, int total)
	{
		g_step = step;
		g_size = size;
		g_total = total;
	}

	void setLocal(int step, int total)
	{
		l_step = step;
		l_total = total;
	}
};

static startup_progress_c s_progress;

void E_ProgressMessage(const char *message)
{
	// FIXME: show message near progress bar
	//I_Printf("%s", message);
}

void E_LocalProgress(int step, int total)
{
	s_progress.setLocal(step, total);
	s_progress.recomputePercent();
}

void E_GlobalProgress(int step, int size, int total)
{
	s_progress.setGlobal(step, size, total);

	E_LocalProgress(0, 1);  // recomputes the percentage
}

void E_NodeMessage(const char *message)
{
	// FIXME: show message
	//I_Printf("LOADING, PLEASE WAIT.%s", message);
}

void E_NodeProgress(int perc)
{
	s_progress.drawIt(perc);
}

//
// -ACB- 1999/09/20 Created. Sets Global Stuff.
//
static void SetGlobalVars(void)
{
	int p;
	const char *s;

	// Screen Resolution Check...
	s = M_GetParm("-width");
	if (s)
		SCREENWIDTH = atoi(s);


	s = M_GetParm("-height");
	if (s)
		SCREENHEIGHT = atoi(s);

	p = M_CheckParm("-res");
	if (p && p + 2 < M_GetArgCount())
	{
		SCREENWIDTH = atoi(M_GetArgument(p + 1));
		SCREENHEIGHT = atoi(M_GetArgument(p + 2));
	}

	// Bits per pixel check....
	s = M_GetParm("-bpp");
	if (s)
	{
		SCREENBITS = atoi(s);

		if (SCREENBITS <= 4) // backwards compat
			SCREENBITS *= 8;
	}

	// restrict depth to allowable values
	if (SCREENBITS < 15) SCREENBITS = 15;
	else if (SCREENBITS > 32) SCREENBITS = 32;

	M_CheckBooleanParm("windowed", &FULLSCREEN, true);
	M_CheckBooleanParm("fullscreen", &FULLSCREEN, false);

	// sprite kludge (TrueBSP)
	p = M_CheckParm("-spritekludge");
	if (p)
	{
		if (p + 1 < M_GetArgCount())
			sprite_kludge = atoi(M_GetArgument(p + 1));

		if (!sprite_kludge)
			sprite_kludge = 1;
	}

	s = M_GetParm("-screenshot");
	if (s)
	{
		screenshot_rate = atoi(s);
		singletics = true;
	}

	// -AJA- 1999/10/18: Reworked these with M_CheckBooleanParm
	M_CheckBooleanParm("rotatemap", &rotatemap, false);
	M_CheckBooleanParm("sound", &nosound, true);
	M_CheckBooleanParm("music", &nomusic, true);
	M_CheckBooleanParm("cdmusic", &nocdmusic, true);
	M_CheckBooleanParm("itemrespawn", &global_flags.itemrespawn, false);
	M_CheckBooleanParm("mlook", &global_flags.mlook, false);
	M_CheckBooleanParm("monsters", &global_flags.nomonsters, true);
	M_CheckBooleanParm("fast", &global_flags.fastparm, false);
	M_CheckBooleanParm("extras", &global_flags.have_extra, false);
	M_CheckBooleanParm("shadows", &global_flags.shadows, true);
	M_CheckBooleanParm("halos", &global_flags.halos, false);
	M_CheckBooleanParm("kick", &global_flags.kicking, false);
	M_CheckBooleanParm("singletics", &singletics, false);
	M_CheckBooleanParm("true3d", &global_flags.true3dgameplay, false);
	M_CheckBooleanParm("blood", &global_flags.more_blood, false);
	M_CheckBooleanParm("cheats", &global_flags.cheats, false);
	M_CheckBooleanParm("jumping", &global_flags.jump, false);
	M_CheckBooleanParm("crouching", &global_flags.crouch, false);
	M_CheckBooleanParm("weaponswitch", &global_flags.weapon_switch, false);
	M_CheckBooleanParm("autoload", &autoquickload, false);

	M_CheckBooleanParm("norenderbuffers", &no_render_buffers, false);

	if (M_CheckParm("-wolf3d_mode"))
		wolf3d_mode = true;

	if (M_CheckParm("-rott_mode"))
		rott_mode = true;

	if (M_CheckParm("-infight"))
		g_aggression = 1;

	if (M_CheckParm("-dlights"))
		use_dlights = 1;
	else if (M_CheckParm("-nodlights"))
		use_dlights = 0;

	if (M_CheckParm("-ecompat"))
		global_flags.edge_compat = true;

	if (!global_flags.respawn)
	{
		if (M_CheckParm("-newnmrespawn"))
		{
			global_flags.res_respawn = true;
			global_flags.respawn = true;
		}
		else if (M_CheckParm("-respawn"))
		{
			global_flags.respawn = true;
		}
	}

	// check for strict and no-warning options
	M_CheckBooleanCVar("strict", &ddf_strict, false);
	M_CheckBooleanCVar("lax", &ddf_lax, false);
	M_CheckBooleanCVar("warn", &ddf_quiet, true);

	strict_errors = ddf_strict.d ? true : false;
	lax_errors = ddf_lax.d ? true : false;
	no_warnings = ddf_quiet.d ? true : false;
}

//
// SetLanguage
//
void SetLanguage(void)
{
	const char *want_lang = M_GetParm("-lang");
	if (!want_lang)
		want_lang = M_GetParm("-language");

	if (want_lang)
		m_language = want_lang;

	if (language.Select(m_language.str))
		return;

	I_Warning("Invalid language: '%s'\n", m_language.str);

	if (!language.IsValid())
		if (!language.Select(0))
			I_Error("Unable to select any language!");

	m_language = language.GetName();
}

//
// SpecialWadVerify
//
static void SpecialWadVerify(void)
{
	int lump = W_CheckNumForName("EDGEVER");
	if (lump < 0)
		I_Error("EDGEVER lump not found. Get EDGE2.WAD at http://edge2.sourceforge.net/");

	const void *data = W_CacheLumpNum(lump);

	// parse version number
	const char *s = (const char*)data;
	int wad_ver = atoi(s) * 100;

	while (isdigit(*s)) s++;
	s++;
	wad_ver += atoi(s);

	W_DoneWithLump(data);

	I_Printf("EDGE2.WAD version %1.2f found.\n", wad_ver / 100.0);

	if (wad_ver < EDGE_WAD_VERSION)
	{
		I_Warning("EDGE2.WAD is an older version (expected %1.2f)\n",
			EDGE_WAD_VERSION / 100.0);
	}
	else if (wad_ver > EDGE_WAD_VERSION)
	{
		I_Warning("EDGE2.WAD is a newer version (expected %1.2f)\n",
			EDGE_WAD_VERSION / 100.0);
	}
}

//
// SpecialPAKVerify
//
static void SpecialPAKVerify(void)
{
	int lump = W_CheckNumForName("EDGEVER");
	if (lump < 0)
		I_Error("EDGEVER lump not found. Get EDGE.PAK at http://edge2.sourceforge.net/");

	const void *data = W_CacheLumpNum(lump);

	// parse version number
	const char *s = (const char*)data;
	int pak_ver = atoi(s) * 100;

	while (isdigit(*s)) s++;
	s++;
	pak_ver += atoi(s);

	W_DoneWithLump(data);

	I_Printf("EDGE.PAK version %1.2f found.\n", pak_ver / 100.0);

	if (pak_ver < EDGE_PAK_VERSION)
	{
		I_Warning("EDGE.PAK is an older version (expected %1.2f)\n",
			EDGE_PAK_VERSION / 100.0);
	}
	else if (pak_ver > EDGE_PAK_VERSION)
	{
		I_Warning("EDGE.PAK is a newer version (expected %1.2f)\n",
			EDGE_PAK_VERSION / 100.0);
	}
}


//
// ShowNotice
//
static void ShowNotice(void)
{
	CON_MessageColor(RGB_MAKE(64, 192, 255));

	I_Printf("%s", language["Notice"]);
}


static void DoSystemStartup(void)
{
	// startup the system now
	W_InitImages();

	I_Debugf("- System startup begun.\n");

	// [SP] Set up Optimus to use desired GPU by setting environment variable before init.
	if (r_gpuswitch.d == 1)
	{
		I_Debugf("* Setting Optimus High-Performance GPU.\n");
		putenv("SHIM_MCCOMPAT=0x800000001");
	}
	else if (r_gpuswitch.d == 2)
	{
		I_Debugf("* Setting Optimus Power-Saving GPU.\n");
		putenv("SHIM_MCCOMPAT=0x800000000");
	}
	else
	{
		I_Debugf("* Optimus GPU setting not set.\n");
	}

	I_SystemStartup();

	// -ES- 1998/09/11 Use R_ChangeResolution to enter gfx mode

	R_DumpResList();

	// -KM- 1998/09/27 Change res now, so music doesn't start before
	// screen.  Reset clock too.
	I_Debugf("- Changing Resolution...\n");

	R_InitialResolution();

	RGL_Init();
	R_SoftInitResolution();

	I_Debugf("- System startup done.\n");
}

//TODO: Restored Pause graphic, dupe function as Pause2 with no graphic for the console responder!
static void M_DisplayPause(void)
{
	///CON_Printf("Paused ;)\n");
	 	static const image_c *pause_image = NULL;

		if (!pause_image)
		{
			if (heretic_mode)
			{
				pause_image = W_ImageLookup("PAUSED");
			}
			else
				pause_image = W_ImageLookup("M_PAUSE");
		}

	// make sure image is centered horizontally

	float w = IM_WIDTH(pause_image);
	float h = IM_HEIGHT(pause_image);

	if (heretic_mode)
	{
		float x = 160 - 3 / 2;
		float y = 3;
		HUD_StretchImage(x, y, w, h, pause_image);
		return;
	}
	else
	{
		float x = 160 - w / 2;
		float y = 10;
		HUD_StretchImage(x, y, w, h, pause_image);
		return;
	}

	//HUD_StretchImage(x, y, w, h, pause_image);
}


wipetype_e wipe_method = WIPE_Melt;
int wipe_reverse = 0;

static bool need_wipe = false;

void E_ForceWipe(void)
{
	if (gamestate == GS_NOTHING)
		return;

	if (wipe_method == WIPE_None)
		return;

	need_wipe = true;

	// capture screen now (before new level is loaded etc..)
	E_Display();
}

//
// E_Display
//
// Draw current display, possibly wiping it from the previous
//
// -ACB- 1998/07/27 Removed doublebufferflag check (unneeded).

static bool wipe_gl_active = false;

void E_Display(void)
{
	if (nodrawers)
		return;  // for comparative timing / profiling

#if 0
	if (debug_testlerp.d > 0)
	{
		//tapamn check fps
		static int last = 0;
		int now = I_GetMillies();
		CON_Printf("T: %f\n", 1.0f / ((now - last) / 1000.0f));
		last = now;
	}
#endif // 0

	//CA 9.27.17:
	//Interpolator is now set in P_Tick (which should improve rendering hitches)
	//N_SetInterpolater();
#if 0
	//tapamn check interpolater value
	I_Printf("I: %f\n", N_GetInterpolater());
#endif

	// Start the frame - should we need to.
	I_StartFrame();

	HUD_FrameSetup(0);

	// -AJA- 1999/08/02: Make sure palette/gamma is OK. This also should
	//       fix (finally !) the "gamma too late on walls" bug.
	V_ColourNewFrame();

	switch (gamestate)
	{
	case GS_LEVEL:
		HU_Erase();

		R_PaletteStuff();

		if (splitscreen_mode)
		{
			VM_RunHud(1);
			VM_RunHud(2);
		}
		else
			VM_RunHud(0);

		if (need_save_screenshot)
		{
			M_MakeSaveScreenShot();
			need_save_screenshot = false;
		}

		HU_Drawer();
		RAD_Drawer();
		break;

	case GS_INTERMISSION:
		WI_Drawer();
		break;

	case GS_FINALE:
		F_Drawer();
		break;

	case GS_TITLESCREEN:
		E_TitleDrawer();
		break;

	case GS_NOTHING:
		break;
	}

	if (wipe_gl_active)
	{
		//maybe this will pause the game?
		// -AJA- Wipe code for GL.  Sorry for all this ugliness, but it just
		//       didn't fit into the existing wipe framework.
		//
		if (RGL_DoWipe())
		{

			RGL_StopWipe();
			wipe_gl_active = false;
			paused = false;
		}
	}

	// save the current screen if about to wipe
	if (need_wipe)
	{
		need_wipe = false;
		wipe_gl_active = true;

		RGL_InitWipe(wipe_reverse, wipe_method);
	}

	if (paused)
		M_DisplayPause();

	// menus go directly to the screen
#if 0
	if (heretic_mode)
	{
		H_Drawer();
	}
	else if (!heretic_mode)
	{
	}
#endif // 0
		M_Drawer();  // menu is drawn even on top of everything (except console)


	N_NetUpdate(false);  // send out any new accumulation

	if (m_screenshot_required)
	{
		m_screenshot_required = false;
		M_ScreenShot(true);
	}
	else if (screenshot_rate && (gamestate >= GS_LEVEL))
	{
		SYS_ASSERT(singletics);

		if (leveltime % screenshot_rate == 0)
			M_ScreenShot(false);
	}

	// draw console _after_ doing screenshots
	CON_Drawer();

	M_DisplayDisk();

	I_FinishFrame();  // page flip or blit buffer
}


//
//  DEMO LOOP
//
static int title_game;
static int title_pic;
static int title_countdown;

static const image_c *title_image = NULL;


static void E_TitleDrawer(void)
{
	if (title_image)
		HUD_StretchImage(0, 0, 320, 200, title_image);
	else
		HUD_SolidBox(0, 0, 320, 200, RGB_MAKE(64, 64, 64));
}


//
// This cycles through the demo sequences.
// -KM- 1998/12/16 Fixed for DDF.
//
void E_AdvanceTitle(void)
{
	title_pic++;

	// prevent an infinite loop
	for (int loop = 0; loop < 100; loop++)
	{
		gamedef_c *g = gamedefs[title_game];
		SYS_ASSERT(g);

		if (title_pic >= g->titlepics.GetSize())
		{
			title_game = (title_game + 1) % gamedefs.GetSize();
			title_pic = 0;
			continue;
		}

		// ignore non-existing episodes.  Doesn't include title-only ones
		// like [EDGE].
		if (title_pic == 0 && g->firstmap && g->firstmap[0] &&
			W_CheckNumForName(g->firstmap) == -1)
		{
			title_game = (title_game + 1) % gamedefs.GetSize();
			title_pic = 0;
			continue;
		}

		// ignore non-existing images
		title_image = W_ImageLookup(g->titlepics[title_pic], INS_Graphic, ILF_Null);

		if (!title_image)
		{
			title_pic++;
			continue;
		}

		// found one !!

		if (title_pic == 0 && g->titlemusic > 0)
			S_ChangeMusic(g->titlemusic, false);

		title_countdown = g->titletics;
		return;
	}

	// not found

	title_image = NULL;
	title_countdown = TICRATE;
}


void E_StartTitle(void)
{
	gameaction = ga_nothing;
	gamestate = GS_TITLESCREEN;

	paused = false;

	// force pic overflow -> first available titlepic
	title_game = gamedefs.GetSize() - 1;
	title_pic = 29999;
	title_countdown = 1;

	E_AdvanceTitle();
}


void E_TitleTicker(void)
{
	if (title_countdown > 0)
	{
		title_countdown--;

		if (title_countdown == 0)
			E_AdvanceTitle();
	}
}


//
// Detects which directories to search for DDFs, WADs and other files in.
//
// -ES- 2000/01/01 Written.
//
void InitDirectories(void)
{
	std::string path;

	const char *s = M_GetParm("-home");
	if (s)
		home_dir = s;

	// Get the Home Directory from environment if set
	if (home_dir.empty())
	{
		s = getenv("HOME");
		if (s)
		{
			home_dir = epi::PATH_Join(s, EDGEHOMESUBDIR);

			if (!epi::FS_IsDir(home_dir.c_str()))
			{
				epi::FS_MakeDir(home_dir.c_str());

				// Check whether the directory was created
				if (!epi::FS_IsDir(home_dir.c_str()))
					home_dir.clear();
			}
		}
	}

	if (home_dir.empty())
		home_dir = "."; // Default to current directory

						// Get the Game Directory from parameter.
	s = epi::GetResourcePath();
	game_dir = s;
	free((void*)s);

	s = M_GetParm("-game");
	if (s)
		game_dir = s;

	// add parameter file "gamedir/parms" if it exists.
	std::string parms = epi::PATH_Join(game_dir.c_str(), "parms");

	if (epi::FS_Access(parms.c_str(), epi::file_c::ACCESS_READ))
	{
		// Insert it right after the game parameter
		M_ApplyResponseFile(parms.c_str(), M_CheckParm("-game") + 2);
	}

	s = M_GetParm("-ddf");
	if (s)
	{
		ddf_dir = std::string(s);
	}

	else if (heretic_mode)
	{
		ddf_dir = epi::PATH_Join(game_dir.c_str(), "her_ddf");
	}
	else if (rott_mode)
	{
		ddf_dir = epi::PATH_Join(game_dir.c_str(), "rott_ddf");
	}
	else
	{
		ddf_dir = epi::PATH_Join(game_dir.c_str(), "doom_ddf");
	}


	DDF_SetWhere(ddf_dir);

	// config file
	s = M_GetParm("-config");
	if (s)
	{
		cfgfile = M_ComposeFileName(home_dir.c_str(), s);
	}
	else
	{
		cfgfile = epi::PATH_Join(home_dir.c_str(), EDGECONFIGFILE);
	}

	// EDGE2.wad file
	s = M_GetParm("-ewad");
	if (s)
	{
		ewadfile = M_ComposeFileName(home_dir.c_str(), s);
	}
	else
	{
		ewadfile = epi::PATH_Join(home_dir.c_str(), "edge2.wad");
	}

	// EDGE2.pak file
	s = M_GetParm("-epak");
	if (s)
	{
	epakfile = M_ComposeFileName(home_dir.c_str(), s);
	}
	else
	{
	epakfile = epi::PATH_Join(home_dir.c_str(), "edge.pak");
	}

	// cache directory
	cache_dir = epi::PATH_Join(home_dir.c_str(), CACHEDIR);

	if (!epi::FS_IsDir(cache_dir.c_str()))
		epi::FS_MakeDir(cache_dir.c_str());

	// savegame directory
	save_dir = epi::PATH_Join(home_dir.c_str(), SAVEGAMEDIR);

	if (!epi::FS_IsDir(save_dir.c_str()))
		epi::FS_MakeDir(save_dir.c_str());

	SV_ClearSlot("current");

	// screenshot directory
	shot_dir = epi::PATH_Join(home_dir.c_str(), SCRNSHOTDIR);

	if (!epi::FS_IsDir(shot_dir.c_str()))
		epi::FS_MakeDir(shot_dir.c_str());
}


//
// Adds an IWAD and EDGE2.WAD. -ES-  2000/01/01 Rewritten.
//
// Adding HERETIC.WAD to string 2.24.2013
// Kept freedoom.wad for backward compatibility
// 2016/02/07: Added Darkwar.wad for ROTT
const char *wadname[] = { "doom2", "doom","hyper", "plutonia", "tnt", "hacx", "heretic", "freedoom", "freedm", "chex", "freedoom1", "freedoom2", "darkwar", "slave", NULL };

static void IdentifyVersion(void)
{
	I_Debugf("- Identify IWADS\n");

	// Check -wolf3d param (which is the ONLY way to start Wolfenstein for now), if this is a Wolf3D map, drastically alter startupcode, and set a global bool to 'wolf3d_mode'.

	if (wolf3d_mode)
	{
		//IdentifyWolfenstein();
		I_Printf("Detected Wolfenstein mode, breaking into IdentifyWolfenstein()! \n");
		return;
	}

	if (rott_mode)
	{
		I_Printf("Rise of the Triad: Darkwar detected\n");
	}


	// Check -iwad parameter, find out if it is the IWADs directory
	std::string iwad_par;
	std::string iwad_file;
	std::string iwad_dir;

	// Check -pak parameter, find out if it is PAKs. . .
	std::string pak_par;
	std::string pak_file;
	std::string pak_dir;


	const char *s = M_GetParm("-iwad");

	iwad_par = std::string(s ? s : "");

#if 0
	///This handles the startup for Heretic, which forces 3DGE to load her_ddf and the heretic fix PWAD, scheduled for removal.
//if (s ? s : "heretic")
	if (stricmp(wadname[iwad_file], "heretic") == 0)
	{
		heretic_mode = true;
#if 0

		I_Printf("Heretic IWAD: Joining PWAD fix!!!\n");
		I_Debugf("Added filename: %s\n", REQHERETICPWAD "." EDGEWADEXT);
		epi::PATH_Join(game_dir.c_str(), REQHERETICPWAD "." EDGEWADEXT);


#endif // 0
		I_Printf("DDF: Loading Heretic HDF\n");
		ddf_dir = epi::PATH_Join(game_dir.c_str(), "her_ddf");
		DDF_SetWhere(ddf_dir);
	}
	else
	{
		heretic_mode = false;
		ddf_dir = epi::PATH_Join(game_dir.c_str(), "doom_ddf");
		DDF_SetWhere(ddf_dir);
	}
#endif // 0


	if (!iwad_par.empty())
	{
		if (epi::FS_IsDir(iwad_par.c_str()))
		{
			iwad_dir = iwad_par;
			iwad_par.clear(); // Discard id
		}
	}

	// If we haven't yet set the IWAD directory, then we check
	// the DOOMWADDIR environment variable
	if (iwad_dir.empty())
	{
		s = getenv("DOOMWADDIR");
		//else
		//s = getenv("DOOMWADPATH");

		if (s && epi::FS_IsDir(s))
			iwad_dir = std::string(s);
	}

	//Check the DOOMWADPATH environment variable.
	if (iwad_dir.empty())
	{
		s = getenv("DOOMWADPATH");

		if (s && epi::FS_IsDir(s))
			iwad_dir = std::string(s);
	}

	// Should the IWAD directory not be set by now, then we
	// use our standby option of the current directory.
	if (iwad_dir.empty())
		iwad_dir = ".";

	// Should the IWAD Parameter not be empty then it means
	// that one was given which is not a directory. Therefore
	// we assume it to be a name
	if (!iwad_par.empty())
	{
		std::string fn = iwad_par;

		// Is it missing the extension?
		std::string ext = epi::PATH_GetExtension(iwad_par.c_str());
		if (ext.empty())
		{
			fn += ("." EDGEWADEXT);
		}

		// If no directory given use the IWAD directory
		std::string dir = epi::PATH_GetDir(fn.c_str());
		if (dir.empty())
			iwad_file = epi::PATH_Join(iwad_dir.c_str(), fn.c_str());
		else
			iwad_file = fn;

		if (!epi::FS_Access(iwad_file.c_str(), epi::file_c::ACCESS_READ))
		{
			I_Error("IdentifyVersion: Unable to add specified '%s'",
				fn.c_str());
		}
		else
		{
			// next lines check for Heretic mode, and set it to true
			if (fn.size() >= 11 && fn.compare(fn.size() - 11, 11, "heretic.wad") == 0)
			{
				I_Printf("DDF: Loading Heretic HDF\n");
				ddf_dir = epi::PATH_Join(game_dir.c_str(), "her_ddf");
				DDF_SetWhere(ddf_dir);
				heretic_mode = true;
				printf("Heretic mode TRUE\n");
			}
			else if (fn.size() >= 11 && fn.compare(fn.size() - 11, 11, "darkwar.wad") == 0)
			{
				I_Printf("DDF: Rise of the Triad: DARKWAR\n");
				ddf_dir = epi::PATH_Join(game_dir.c_str(), "rott_ddf");
				DDF_SetWhere(ddf_dir);
				rott_mode = true;
				printf("ROTT mode TRUE\n");
			}

		}
	}
	else
	{
		const char *location;

		int max = 1;

		if (stricmp(iwad_dir.c_str(), game_dir.c_str()) != 0)
		{
			// IWAD directory & game directory differ
			// therefore do a second loop which will
			// mean we check both.
			max++;
		}


		bool done = false;

		for (int i = 0; i < max && !done; i++)
		{
			location = (i == 0 ? iwad_dir.c_str() : game_dir.c_str());

			//
			// go through the available wad names constructing an access
			// name for each, adding the file if they exist.
			//
			// -ACB- 2000/06/08 Quit after we found a file - don't load
			//                  more than one IWAD
			//

			for (int w_idx = 0; wadname[w_idx]; w_idx++)
			{
					std::string fn(epi::PATH_Join(location, wadname[w_idx]));

					fn += ("." EDGEWADEXT);


				if (epi::FS_Access(fn.c_str(), epi::file_c::ACCESS_READ))
				{
					// next two lines check for Heretic mode, and set it to true
					if (stricmp(wadname[w_idx], "heretic") == 0)
					{
						I_Printf("DDF: Loading Heretic HDF\n");
						ddf_dir = epi::PATH_Join(game_dir.c_str(), "her_ddf");
						DDF_SetWhere(ddf_dir);
						heretic_mode = true;
					}
					else if (stricmp(wadname[w_idx], "darkwar") == 0)
					{
						I_Printf("GAME: Rise of the Triad: DARKWAR\n");
						ddf_dir = epi::PATH_Join(game_dir.c_str(), "rott_ddf");
						DDF_SetWhere(ddf_dir);
						rott_mode = true;
					}
					iwad_file = fn;
					done = true;
					//I_Printf("iwad_file returning true!\n");
					break;
				}
			}

		}
	}

	if (iwad_file.empty())
		I_Warning("IdentifyVersion: No IWADS found!\n");

		W_AddRawFilename(iwad_file.c_str(), FLKIND_IWad);

		iwad_base = epi::PATH_GetBasename(iwad_file.c_str());

		I_Debugf("IWAD BASE = [%s]\n", iwad_base.c_str());


	// Emulate this behaviour?

	// Look for the required wad in the IWADs dir and then the gamedir

	std::string reqwad(epi::PATH_Join(iwad_dir.c_str(), REQUIREDWAD "." EDGEPAKEXT));


	///this one will join pak files with IWAD.
	///std::string reqpak(epi::PATH_Join(iwad_dir.c_str(), REQUIREDWAD "." EDGEPAKEXT));

	if (!epi::FS_Access(reqwad.c_str(), epi::file_c::ACCESS_READ))
	{
		reqwad = epi::PATH_Join(game_dir.c_str(), REQUIREDWAD "." EDGEPAKEXT);

		if (!epi::FS_Access(reqwad.c_str(), epi::file_c::ACCESS_READ))
		{
			I_Error("IdentifyVersion: Could not find required %s.%s!\n",
				REQUIREDWAD, EDGEPAKEXT);
		}
	}

	W_AddRawFilename(reqwad.c_str(), FLKIND_PAK);
}

//WLF_EXTENSION ADDS ALL WL6 FILES ALL AT ONCE FOR WOLFENSTEIN, JUST FOR TESTING, MAYBE MAKE THIS MORE ROBUST IN THE FUTURE...?
const char *wlf_extension[] = { "audiohed", "audiot", "gamemaps","maphead", "vgadict", "vgagraph", "vgahead", "vswap", NULL }; //test to load this bitch up. . .
static void IdentifyWolfenstein(void)
{
	if (!wolf3d_mode)
	{
		//IdentifyVersion();
		//I_Printf("Wolf: Breaking back into IdentifyVersion (IWADS)!\n");
		return;
	}

	std::string wolf_par; //parameter
	std::string wolf_file; //filename
	std::string wolf_dir; //directory

	const char *w = M_GetParm("-startwolf"); ///Yep, the big one!

	wolf_par = std::string(w ? w : ""); //Brute force MAPHEAD, just for now.

											// Should the Wolfenstein directory not be set by now, then we
											// use our standby option of the current directory.
	if (wolf_dir.empty())
		wolf_dir = ".";

	// Should the Wolf Parameter not be empty then it means
	// that one was given which is not a directory. Therefore
	// we assume it to be a name or some shit!
	else if (!wolf_par.empty())
	{
		std::string fn = wolf_par;

		//testing just opening MAPHEAD for now. Apparently, Wolf3D ignores
		//extensions (stripping them) and using what would be the wlf_extensions
		// Is it missing the extension?

		std::string ext = epi::PATH_GetExtension(wolf_par.c_str());
		if (ext.empty())
		{
			fn += ("."); //Don't use an extension, just the raw filename!
			I_Printf("- missing extension, so ext.empty() passed the test!");
		}

		// If no directory given...
		std::string dir = epi::PATH_GetDir(fn.c_str());

		if (dir.empty())
			wolf_file = epi::PATH_Join(wolf_dir.c_str(), fn.c_str());
		else
			wolf_file = fn;

		if (!epi::FS_Access(wolf_file.c_str(), epi::file_c::ACCESS_READ))
		{
			I_Error("IdentifyWolfenstein: Unable to add specified Wolfenstein file: '%s'", fn.c_str());
		}
	}

	else
	{

		const char *location2;

		int max = 1;

		if (stricmp(wolf_dir.c_str(), game_dir.c_str()) != 0)
		{
			// WOLF directory & game directory differ
			// therefore do a second loop which will
			// mean we check both.
			max++;
		}

		bool done = false;

		for (int i = 0; i < max && !done; i++)
		{
			location2 = (i == 0 ? wolf_dir.c_str() : game_dir.c_str());

			//
			// go through the available WL6 names constructing an access
			// name for each, adding the file if they exist.
			//

			//
			// FIND MAPHEAD
			//
			for (int w_idx = 0; wlf_extension[w_idx]; w_idx++)
			{
				std::string fn(epi::PATH_Join(location2, wlf_extension[w_idx]));

				fn += ("." WOLFDATEXT); //Wolfenstein Datas, maybe instead of +=, use an iterator, fn++?

				if (epi::FS_Access(fn.c_str(), epi::file_c::ACCESS_READ))
				{
					if (stricmp(wlf_extension[w_idx], "MAPHEAD") == 0)
					{
						wolf3d_mode = true;
						I_Printf("DDF: Loading Wolfenstein, joining path\n");
						ddf_dir = epi::PATH_Join(game_dir.c_str(), "wolf_ddf");
						DDF_SetWhere(ddf_dir);
					}

					wolf_file = fn;
					done = true;
					I_Printf("MAPHEAD.WL6 found!/n");
					break;
				}
			}
		}
	}

	//Brute this out -- make sure the fucking thing is even trying to be opened..
	if (wolf_file.empty())
		I_Printf("Wolfenstein -- brute force all WL6 files!\n");

	if (wolf3d_mode)
	I_Printf("BruteForce_DDF: Loading Wolfenstein DDF\n");
	ddf_dir = epi::PATH_Join(game_dir.c_str(), "wolf_ddf");
	DDF_SetWhere(ddf_dir);


	I_Printf("Wolfenstein: Joining Wolf3D Data!!!\n");
	I_Debugf("Added filename: %s\n", WOLFMAPHEAD "." WOLFDATEXT);
	epi::PATH_Join(game_dir.c_str(), WOLFMAPHEAD "." WOLFDATEXT);


	I_Debugf("Added filename: %s\n", WOLFGAMEMAPS "." WOLFDATEXT);
	epi::PATH_Join(game_dir.c_str(), WOLFGAMEMAPS "." WOLFDATEXT);


	I_Debugf("Added filename: %s\n", WOLFVGAHEAD "." WOLFDATEXT);
	epi::PATH_Join(game_dir.c_str(), WOLFVGAHEAD "." WOLFDATEXT);

	I_Debugf("Added filename: %s\n", WOLFVGADICT "." WOLFDATEXT);
	epi::PATH_Join(game_dir.c_str(), WOLFVGADICT "." WOLFDATEXT);

	I_Debugf("Added filename: %s\n", WOLFVGAGRAPH "." WOLFDATEXT);
	epi::PATH_Join(game_dir.c_str(), WOLFVGAGRAPH "." WOLFDATEXT);

	I_Debugf("Added filename: %s\n", WOLFVSWAP "." WOLFDATEXT);
	epi::PATH_Join(game_dir.c_str(), WOLFVSWAP "." WOLFDATEXT);

	I_Debugf("Added filename: %s\n", WOLFAUDIOHED "." WOLFDATEXT);
	epi::PATH_Join(game_dir.c_str(), WOLFAUDIOHED "." WOLFDATEXT);

	I_Debugf("Added filename: %s\n", WOLFAUDIOT "." WOLFDATEXT);
	epi::PATH_Join(game_dir.c_str(), WOLFAUDIOT "." WOLFDATEXT);

	CreatePlaypal();

	wolf_base = epi::PATH_GetBasename(wolf_file.c_str());

	//I_Debugf("WOLF BASE = [%s]\n", wolf_base.c_str());

	//W_AddRawFilename(wolf_file.c_str(), FLKIND_WL6); //<--- This needs defining! Done.c

	// Just AddRawFilename. Strip the god damn extension (instead of FLKIND_WL6, use FLKIND_Lump. Ooops.... First the file, then the filetype for loading via w_wad (AddFile()).
	//I_Printf("WF_InitMaps: Breaking into WF_InitMaps to find MAPHEAD info!\n");
	//WF_InitMaps(); //In Wlf_Maps.cc, this thing directly calls MapsReadHeaders(), which will open the file and read its rlew_tag, etc.

	//Finally sets the wolf_base (maphead_base)
	//wolf_base = epi::PATH_GetBasename(wolf_file.c_str()); //<---

	//I_Debugf("WOLF BASE = [%s]\n", wolf_file.c_str()); //Eventually we should collect all of these. . .

														   // Emulate this behaviour?

	//All this function below does is add EDGE2.WAD to whatever the fuck we are adding as well.

	std::string reqwad(epi::PATH_Join(game_dir.c_str(), REQUIREDWAD "." EDGEPAKEXT));

	///this one will join pak files with IWAD.
	///std::string reqpak(epi::PATH_Join(iwad_dir.c_str(), REQUIREDWAD "." EDGEPAKEXT));

	if (!epi::FS_Access(reqwad.c_str(), epi::file_c::ACCESS_READ))
	{
		reqwad = epi::PATH_Join(game_dir.c_str(), REQUIREDWAD "." EDGEPAKEXT);

		if (!epi::FS_Access(reqwad.c_str(), epi::file_c::ACCESS_READ))
		{
			I_Error("IdentifyVersion: Could not find required %s.%s!\n",
				REQUIREDWAD, EDGEPAKEXT);
		}
	}

	//W_AddRawFilename()

	W_AddRawFilename(reqwad.c_str(), FLKIND_PAK);

	I_Printf("Wolfenstein Data is loaded and joined with 3DGE -- let's keep going!\n");

	// After this, should we skip all the bullshit and load the stuff directly?
}


static void CheckTurbo(void)
{
	int turbo_scale = 100;

	int p = M_CheckParm("-turbo");

	if (p)
	{
		if (p + 1 < M_GetArgCount())
			turbo_scale = atoi(M_GetArgument(p + 1));
		else
			turbo_scale = 200;

		if (turbo_scale < 10)  turbo_scale = 10;
		if (turbo_scale > 400) turbo_scale = 400;

		CON_MessageLDF("TurboScale", turbo_scale);
	}

	E_SetTurboScale(turbo_scale);
}


static void ShowDateAndVersion(void)
{
	time_t cur_time;
	char timebuf[100];

	time(&cur_time);
	strftime(timebuf, 99, "%I:%M %p on %d/%b/%Y", localtime(&cur_time));

	I_Debugf("[Log file created at %s]\n\n", timebuf);
	I_Debugf("[Debug file created at %s]\n\n", timebuf);

	// 23-6-98 KM Changed to hex to allow versions such as 0.65a etc
	I_Printf("3DGE v" EDGEVERSTR " compiled on " __DATE__ " at " __TIME__ "\n");
	I_Printf("hyper3DGE homepage is at http://edge2.sourceforge.net/\n");
	I_Printf("hyper3DGE wiki is at http://3dfxdev.net/edgewiki/\n");
	I_Printf("hyper3DGE is based on id Tech by id Software http://www.idsoftware.com/\n");
	I_Printf("hyper3DGE problems should be reported at http://tdgmods.net/smf\n");

#ifdef WIN32
	I_Printf("Executable path: '%s'\n", win32_exe_path);
#endif

	M_DebugDumpArgs();
}

// ~CA~ 5.6.2016: Implemented CPU detection.
// linked via libcpuID ->> <libcpuid.h>
static void E_ShowCPU(void)
{
	if (!cpuid_present())
	{
		I_Printf("3DGE cannot detect your Processor Type!\n");
		//return -1;
	}

	struct cpu_raw_data_t raw;  // contains only raw data
	struct cpu_id_t data;      // contains recognized CPU features data

	if (cpuid_get_raw_data(&raw) < 0)
	{   // obtain the raw CPUID data
		I_Printf("Sorry, cannot get the CPUID raw data.\n");
		I_Printf("Error: %s\n", cpuid_error());                          // cpuid_error() gives the last error description
																		 //return -2;
	}

	if (cpu_identify(&raw, &data) < 0)
	{   // identify the CPU, using the given raw data.
		I_Printf("Sorrry, CPU identification failed.\n");
		I_Printf("Error: %s\n", cpuid_error());
		//return -3;
	}
	I_Printf("==============================================================================\n");

	I_Printf("E_ShowCPU: detected %s CPU\n", data.vendor_str); // print out the vendor string (e.g. `GenuineIntel')
															   /* 	I_Printf("I_ShowCPU:`%s'\n", data.cpu_codename); // print out the CPU code name (e.g. `Pentium 4 (Northwood)') */
	I_Printf("E_ShowCPU: `%s'\n", data.brand_str);             // print out the CPU brand string
	I_Printf("E_ShowCPU: clocked at %d MHz (according to your OS)\n",
		cpu_clock_by_os());  // print out the CPU clock, according to the OS

							 // Debugfile will output more information
	I_Debugf("E_ShowCPU: The processor has %dK L1 cache and %dK L2 cache\n",
		data.l1_data_cache, data.l2_cache);                            // print out cache size information
	I_Debugf("E_ShowCPU: The processor has %d cores and %d logical processors\n",
		data.num_cores, data.num_logical_cpus);                        // print out CPU cores information

	I_Debugf("E_ShowCPU: Supported multimedia instruction sets:\n");
	I_Debugf("  MMX         : %s\n", data.flags[CPU_FEATURE_MMX] ? "present" : "absent");
	I_Debugf("  MMX-extended: %s\n", data.flags[CPU_FEATURE_MMXEXT] ? "present" : "absent");
	I_Debugf("  SSE         : %s\n", data.flags[CPU_FEATURE_SSE] ? "present" : "absent");
	I_Debugf("  SSE2        : %s\n", data.flags[CPU_FEATURE_SSE2] ? "present" : "absent");
	I_Debugf("  3DNow!      : %s\n", data.flags[CPU_FEATURE_3DNOW] ? "present" : "absent");

	I_Printf("==============================================================================\n");
}

static void SetupLogAndDebugFiles(void)
{
	// -AJA- 2003/11/08 The log file gets all CON_Printfs, I_Printfs,
	//                  I_Warnings and I_Errors.

	std::string log_fn(epi::PATH_Join(home_dir.c_str(), EDGELOGFILE));
	std::string debug_fn(epi::PATH_Join(home_dir.c_str(), "debug.txt"));

	logfile = NULL;
	debugfile = NULL;

	if (!M_CheckParm("-nolog"))
	{

		logfile = fopen(log_fn.c_str(), "w");

		if (!logfile)
			I_Error("[E_Startup] Unable to create log file\n");
	}

	//
	// -ACB- 1998/09/06 Only used for debugging.
	//                  Moved here to setup debug file for DDF Parsing...
	//
	// -ES- 1999/08/01 Debugfiles can now be used without -DDEVELOPERS, and
	//                 then logs all the CON_Printfs, I_Printfs and I_Errors.
	//
	// -ACB- 1999/10/02 Don't print to console, since we don't have a console yet.

	/// int p = M_CheckParm("-debug");
	if (true)
	{
		debugfile = fopen(debug_fn.c_str(), "w");

		if (!debugfile)
			I_Error("[E_Startup] Unable to create debugfile");
	}
}

static void AddSingleCmdLineFile(const char *name)
{
	std::string ext = epi::PATH_GetExtension(name);
	int kind = FLKIND_Lump;

	// no need to check for GWA (shouldn't be added manually)
	// cw - check for GWA... need to add manually for pak/pk3/pk7

	if (stricmp(ext.c_str(), "edm") == 0)
		I_Error("Demos are no longer supported\n");
	else if (stricmp(ext.c_str(), "gwa") == 0)
		kind = FLKIND_GWad;
	else if (stricmp(ext.c_str(), "wad") == 0)
		kind = FLKIND_PWad;
	else if (stricmp(ext.c_str(), "wl6") == 0)
		kind = FLKIND_WL6;
#ifdef HAVE_PHYSFS
	else if (stricmp(ext.c_str(), "pak") == 0) /// ~CA~ 5.7.2016 - new PAK class file
		kind = FLKIND_PAK;
	else if (stricmp(ext.c_str(), "pk3") == 0) /// ~CW~ 1.7.2017 - new PK3 class file
		kind = FLKIND_PK3;
	else if (stricmp(ext.c_str(), "pk7") == 0) /// ~CW~ 1.8.2017 - new PK7 class file
		kind = FLKIND_PK7;
#else
	else if (stricmp(ext.c_str(), "pak") == 0)
		I_Error("PAK files not supported\n");
	else if (stricmp(ext.c_str(), "pk3") == 0)
		I_Error("PK3 files not supported\n");
	else if (stricmp(ext.c_str(), "pk7") == 0)
		I_Error("PK7 files not supported\n");
#endif
	else if (stricmp(ext.c_str(), "hwa") == 0)
		kind = FLKIND_HWad;
	else if (stricmp(ext.c_str(), "rts") == 0)
		kind = FLKIND_RTS;
	else if (stricmp(ext.c_str(), "ddf") == 0 || stricmp(ext.c_str(), "ldf") == 0)
		kind = FLKIND_DDF;
	else if (stricmp(ext.c_str(), "deh") == 0 || stricmp(ext.c_str(), "bex") == 0)
		kind = FLKIND_Deh;

	std::string fn = M_ComposeFileName(game_dir.c_str(), name);

#if 0
	if (heretic_mode)
	{
		std::string fn = M_ComposeFileName(game_dir.c_str(), name);
		W_AddRawFilename(fn.c_str(), kind);
	}
#endif // 0

	W_AddRawFilename(fn.c_str(), kind);
}

static void AddCommandLineFiles(void)
{
	// first handle "loose" files (arguments before the first option)

	int p;
	const char *ps;

	for (p = 1; p < M_GetArgCount() && '-' != (ps = M_GetArgument(p))[0]; p++)
	{
		AddSingleCmdLineFile(ps);
	}

	// next handle the -file option (we allow multiple uses)

	p = M_CheckNextParm("-file", 0);

	while (p)
	{
		// the parms after p are wadfile/lump names,
		// go until end of parms or another '-' preceded parm

		for (p++; p < M_GetArgCount() && '-' != (ps = M_GetArgument(p))[0]; p++)
		{
			AddSingleCmdLineFile(ps);
		}

		p = M_CheckNextParm("-file", p - 1);
	}

	// scripts....

	p = M_CheckNextParm("-script", 0);

	while (p)
	{
		// the parms after p are script filenames,
		// go until end of parms or another '-' preceded parm

		for (p++; p < M_GetArgCount() && '-' != (ps = M_GetArgument(p))[0]; p++)
		{
			std::string ext = epi::PATH_GetExtension(ps);

			// sanity check...
			if (stricmp(ext.c_str(), "wad") == 0 ||
				stricmp(ext.c_str(), "wl6") == 0 ||
				stricmp(ext.c_str(), "pak") == 0 ||
				stricmp(ext.c_str(), "pk7") == 0 ||
				stricmp(ext.c_str(), "pk3") == 0 ||
				stricmp(ext.c_str(), "gwa") == 0 ||
				stricmp(ext.c_str(), "hwa") == 0 ||
				stricmp(ext.c_str(), "ddf") == 0 ||
				stricmp(ext.c_str(), "deh") == 0 ||
				stricmp(ext.c_str(), "bex") == 0)
			{
				I_Error("Illegal filename for -script: %s\n", ps);
			}

			std::string fn = M_ComposeFileName(game_dir.c_str(), ps);

			W_AddRawFilename(fn.c_str(), FLKIND_RTS);
		}

		p = M_CheckNextParm("-script", p - 1);
	}


	// finally handle the -deh option(s)

	p = M_CheckNextParm("-deh", 0);

	while (p)
	{
		// the parms after p are Dehacked/BEX filenames,
		// go until end of parms or another '-' preceded parm

		for (p++; p < M_GetArgCount() && '-' != (ps = M_GetArgument(p))[0]; p++)
		{
			std::string ext(epi::PATH_GetExtension(ps));

			// sanity check...
			if (stricmp(ext.c_str(), "wad") == 0 ||
				stricmp(ext.c_str(), "wl6") == 0 ||
				stricmp(ext.c_str(), "gwa") == 0 ||
				stricmp(ext.c_str(), "hwa") == 0 ||
				stricmp(ext.c_str(), "ddf") == 0 ||
				stricmp(ext.c_str(), "rts") == 0)
			{
				I_Error("Illegal filename for -deh: %s\n", ps);
			}

			std::string fn = M_ComposeFileName(game_dir.c_str(), ps);

			W_AddRawFilename(fn.c_str(), FLKIND_Deh);
		}

		p = M_CheckNextParm("-deh", p - 1);
	}
}

static void InitDDF(void)
{
	I_Debugf("- Initialising DDF\n");

	DDF_Init(EDGEVER);
}


void E_EngineShutdown(void)
{
	N_QuitNetGame();

	S_StopMusic();

	// Pause to allow sounds to finish
	for (int loop = 0; loop < 30; loop++)
	{
		S_SoundTicker();
		I_Sleep(50);
	}

	S_Shutdown();
}

typedef struct
{
	int prog_time;  // rough indication of progress time
	void(*function)(void);
}
startuporder_t;

startuporder_t startcode[] =
{
	{ 1, InitDDF },
	{ 1, IdentifyVersion },
	{ 1, IdentifyWolfenstein },
	{ 1, AddCommandLineFiles },
	{ 1, CheckTurbo },
	{ 1, RAD_Init },
	{ 4, W_InitMultipleFiles },
	{ 1, V_InitPalette },
	{ 2, HU_Init },
	{ 3, W_InitFlats },
	{ 10, W_InitTextures },
	{ 1, CON_Start },
	{ 1, SpecialPAKVerify }, //<---- Change to SpecialPAKVerify for testing. . .
	{ 1, M_InitMiscConVars },
	{ 20, W_ReadDDF },
	{ 1, DDF_CleanUp },
	{ 1, SetLanguage },
	{ 1, ShowNotice },
	{ 1, SV_MainInit },
	{ 10, W_ImageCreateUser },
	{ 20, W_InitSprites },
	{ 3, W_ProcessTX_HI },
	{ 1, W_InitModels },
	{ 1, M_Init },
	{ 3, R_Init },
	{ 1, P_Init },
	//{ 1, WF_InitMaps },
	{ 1, P_MapInit },
	{ 1, P_InitSwitchList },
	{ 1, W_InitPicAnims },
	{ 1, S_Init },
	{ 1, N_InitNetwork },
	{ 1, M_CheatInit },
	{ 1, VM_InitCoal },
	{ 8, VM_LoadScripts },
	{ 0, NULL }
};

extern void WF_InitMaps(void); //!!!

								// Local Prototypes
extern void E_SplashScreen(void);
static void E_Startup(void);
static void E_Shutdown(void);


static void E_Startup(void)
{
	int p;
	const char *ps;

#ifdef DREAMCAST_DEBUG
	printf("Changing dir\n");
	fflush(stdout);
	chdir("/pc/3dgedc/");
#endif
#ifdef DREAMCAST_RELEASE
#error Get CD working
#endif
	// Version check ?
	if (M_CheckParm("-version"))
	{
		// -AJA- using I_Error here, since I_Printf crashes this early on
		I_Error("\nEDGE version is " EDGEVERSTR "\n");
	}

	// -AJA- 2000/02/02: initialise global gameflags to defaults
	global_flags = default_gameflags;

	InitDirectories();

	SetupLogAndDebugFiles();

	CON_InitConsole();
	CON_ResetAllVars();

	ShowDateAndVersion();

	E_ShowCPU();

	M_LoadDefaults();

	CON_HandleProgramArgs();
	SetGlobalVars();

#ifdef HAVE_PHYSFS
	PHYSFS_init(M_GetArgument(0));
#endif

	DoSystemStartup();

	bool nosplash = false;

	//Splash Screen Check
	M_CheckBooleanParm("nosplash", &nosplash, false);
	if (!nosplash && i_skipsplash.d == 0)
	{
		E_SplashScreen();
	}

#if 0
	pt = M_GetParm("-wolf3d");
	if (pt)
	{
		WF_InitMaps();
	}
#endif // 0

	I_PutTitle(E_TITLE); // Needs to be done once the system is up and running

						 // RGL_FontStartup();

	E_GlobalProgress(0, 0, 1);

	int total = 0;
	int cur = 0;

	for (p = 0; startcode[p].function != NULL; p++)
		total += startcode[p].prog_time;

	// Cycle through all the startup functions
	for (p = 0; startcode[p].function != NULL; p++)
	{
		E_GlobalProgress(cur, startcode[p].prog_time, total);

		startcode[p].function();

		cur += startcode[p].prog_time;
	}

	E_GlobalProgress(100, 0, 100);
}


static void E_Shutdown(void)
{
#ifdef HAVE_PHYSFS
	PHYSFS_deinit();
#endif
}


static void E_InitialState(void)
{
	I_Debugf("- Setting up Initial State...\n");

	const char *ps;

	// do demos and loadgames first, as they contain all of the
	// necessary state already (in the demo file / savegame).

	if (M_CheckParm("-playdemo") || M_CheckParm("-timedemo") ||
		M_CheckParm("-record"))
	{
		I_Error("Demos are no longer supported\n");
	}

	ps = M_GetParm("-loadgame");
	if (ps)
	{
		G_DeferredLoadGame(atoi(ps));
		return;
	}

	bool warp = false;

	// get skill / episode / map from parms
	std::string warp_map;
	skill_t     warp_skill = sk_medium;
	int         warp_deathmatch = 0;

	int bots = 0;

	ps = M_GetParm("-bots");
	if (ps)
		bots = atoi(ps);

	ps = M_GetParm("-warp");
	if (ps)
	{
		warp = true;
		warp_map = std::string(ps);
	}

	// -KM- 1999/01/29 Use correct skill: 1 is easiest, not 0
	ps = M_GetParm("-skill");
	if (ps)
	{
		warp = true;
		warp_skill = (skill_t)(atoi(ps) - 1);
	}

	// deathmatch check...
	int pp = M_CheckParm("-deathmatch");
	if (pp)
	{
		warp_deathmatch = 1;

		if (pp + 1 < M_GetArgCount())
			warp_deathmatch = MAX(1, atoi(M_GetArgument(pp + 1)));
	}
	else if (M_CheckParm("-altdeath") > 0)
	{
		warp_deathmatch = 2;
	}

	if (M_CheckParm("-splitscreen") > 0)
		splitscreen_mode = true;

	if (M_GetParm("-record"))
		warp = true;

	// start the appropriate game based on parms
	if (!warp)
	{
		I_Debugf("- Startup: showing title screen.\n");
		E_StartTitle();
		return;
	}

	newgame_params_c params;

	params.skill = warp_skill;
	params.deathmatch = warp_deathmatch;

	if (warp_map.length() > 0)
		params.map = G_LookupMap(warp_map.c_str());
	else
		params.map = G_LookupMap("1");

	if (!params.map)
		I_Error("-warp: no such level '%s'\n", warp_map.c_str());

	SYS_ASSERT(G_MapExists(params.map));
	SYS_ASSERT(params.map->episode);

	params.random_seed = I_PureRandom();

	if (splitscreen_mode)
		params.Splitscreen();
	else
		params.SinglePlayer(bots);

	G_DeferredNewGame(params);
}


//
// ---- MAIN ----
//
// -ACB- 1998/08/10 Removed all reference to a gamemap, episode and mission
//                  Used LanguageLookup() for lang specifics.
//
// -ACB- 1998/09/06 Removed all the unused code that no longer has
//                  relevance.
//
// -ACB- 1999/09/04 Removed statcopy parm check - UNUSED
//
// -ACB- 2004/05/31 Moved into a namespace, the c++ revolution begins....
//
void E_Main(int argc, const char **argv)
{
	// Start the EPI Interface
	epi::Init();

	// Start memory allocation system at the very start (SCHEDULED FOR REMOVAL)
	Z_Init();

	// Implemented here - since we need to bring the memory manager up first
	// -ACB- 2004/05/31
	M_InitArguments(argc, argv);

	try
	{
		E_Startup();

		E_InitialState();

		CON_MessageColor(RGB_MAKE(255, 255, 0));
		I_Printf("EDGE2 v" GIT_DESCRIPTION " system ready.\n");

		I_Debugf("- Entering game loop...\n");

		while (!(app_state & APP_STATE_PENDING_QUIT))
		{
			// We always do this once here, although the engine may
			// makes in own calls to keep on top of the event processing
			I_ControlGetEvents();

			if (app_state & APP_STATE_ACTIVE)
				E_Tick();
		}
	}
	catch (...)
	{
		I_Error("EDGE caught: Unexpected internal failure occurred!\n");
	}

	E_Shutdown();    // Shutdown whatever at this point

					 // Kill the epi interface
	epi::Shutdown();
}


//
// Called when this application has lost focus (i.e. an ALT+TAB event)
//
void E_Idle(void)
{
	E_ReleaseAllKeys();
}


//
// This Function is called for a single loop in the system.
//
// -ACB- 1999/09/24 Written
// -ACB- 2004/05/31 Namespace'd
//
void E_Tick(void)
{
	// -ES- 1998/09/11 It's a good idea to frequently check the heap
#ifdef DEVELOPERS
	//Z_CheckHeap();
#endif

	G_BigStuff();

#if 0
	static int ticker = 70;
	ticker--;
	if (ticker == 0) {
		extern cvar_c r_lerp;

		ticker = 70;
		r_lerp.d = !r_lerp.d;
		CON_Message("Interp: %i\n", r_lerp.d);
	}
#endif
	// Update display, next frame, with current state.
	// Render frames until it's time to run a gametic
	// Measure frame length in order to avoid
	float interpstart = 0, interpdiff = 0;
	static int nextframe = 0;
	do {
		interpstart += interpdiff;

		extern cvar_c r_maxfps;

		if (r_maxfps.d > 0) 
		{
			while (I_GetMillies() < nextframe) 
			{
				//just in case someone plays for over 24 days and nextframe/getmillies overflow
				if ((nextframe - I_GetMillies()) > 1000)
					break;
			}
			nextframe = I_GetMillies() + 1000.0f / r_maxfps.f;
		}

		N_SetInterpolater();
		E_Display();

		extern float N_CalculateCurrentSubTickPosition(void);

		interpdiff = N_CalculateCurrentSubTickPosition() - interpstart;
		//interpdiff = N_GetInterpolater() - interpstart;

		//if (start of frame time + time to render frame + predicted next frame render time) > 1 whole gametic, stop rendering
		//predicted next frame render time is the render time of the previous frame
		//TODO maybe predicted time should be event processing time?
	} while ((interpstart + 2 * interpdiff) <  0.98);

	bool fresh_game_tic;

	// this also runs the responder chain via E_ProcessEvents
	int counts = N_TryRunTics(&fresh_game_tic);

	SYS_ASSERT(counts > 0);

	for (; counts > 0; counts--)  // run the tics
	{
		CON_Ticker();
		M_Ticker();

		if (fresh_game_tic)
			G_Ticker();

		S_SoundTicker();
		S_MusicTicker(); // -ACB- 1999/11/13 Improved music update routines

		N_NetUpdate(false);  // check for new console commands
	}
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
