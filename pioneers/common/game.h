/* Gnocatan - Implementation of the excellent Settlers of Catan board game.
 *   Go buy a copy.
 *
 * Copyright (C) 1999 the Free Software Foundation
 * Copyright (C) 2003 Bas Wijnen <b.wijnen@phys.rug.nl>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __game_h
#define __game_h

#include "map.h"
#include "driver.h"

#define numElem(a) (sizeof(a)/sizeof(a[0]))

#ifdef __GNUC__
	#define UNUSED(var) var __attribute__ ((unused))
#else
	#define UNUSED(var) var
#endif

typedef enum {
	DEVEL_ROAD_BUILDING,
	DEVEL_MONOPOLY,
	DEVEL_YEAR_OF_PLENTY,
	DEVEL_CHAPEL,
	DEVEL_UNIVERSITY_OF_CATAN,
	DEVEL_GOVERNORS_HOUSE,
	DEVEL_LIBRARY,
	DEVEL_MARKET,
	DEVEL_SOLDIER
} DevelType;
#define NUM_DEVEL_TYPES (DEVEL_SOLDIER + 1)

#define MAX_PLAYERS 8		/* maximum number of players supported */

typedef enum {
	VAR_DEFAULT,		/* plain out-of-the-box game */
	VAR_ISLANDS,		/* Islands of Catan */
	VAR_INTIMATE		/* Intimate Catan */
} GameVariant;

typedef struct {
	gchar *title;		/* title of the game */
	GameVariant variant;	/* which variant is being played */
	gboolean random_terrain; /* shuffle terrain location? */
	gboolean strict_trade;	/* trade only before build/buy? */
	gboolean domestic_trade; /* player trading allowed? */
	gint num_players;	/* number of players in the game */
	gint sevens_rule;	/* what to do when a seven is rolled */
	/* 0 = normal, 1 = no 7s on first 2 turns (official rule variant),
	 * 2 = all 7s rerolled */
	gint victory_points;	/* target number of victory points */
	gint num_build_type[NUM_BUILD_TYPES]; /* number of each build type */
	gint resource_count;	/* number of each resource */
	gint num_develop_type[NUM_DEVEL_TYPES]; /* number of each development */
	gboolean register_server; /* register game with meta-server? */
	gchar *server_port;	/* port to run game on */
	GArray *chits;		/* chit sequence */
	Map *map;		/* the game map */
	gboolean parsing_map;	/* currently parsing map? */ /* Not in game_params[] */
	gint tournament_time;   /* time to start tournament time in minutes */
	gboolean quit_when_done;  /* server quits after someone wins */ /* Not in game_params[] */
	gboolean use_pirate;	/* is there a pirate in this game? */
} GameParams;

typedef struct {
	gint id;	/* identification for client-server communication */
	gchar *name;	/* name of the item */
	gint points;	/* number of points */
} Points;

typedef void (*WriteLineFunc)(gpointer user_data, const gchar *);

GameParams *params_new(void);
GameParams *params_copy(GameParams *params);
void params_free(GameParams *params);
void params_write_lines(GameParams *params, WriteLineFunc func, gpointer user_data);
void params_load_line(GameParams *params, gchar *line);
void params_load_finish(GameParams *params);
#endif
