/* Pioneers - Implementation of the excellent Settlers of Catan board game.
 *   Go buy a copy.
 *
 * Copyright (C) 1999 Dave Cole
 * Copyright (C) 2003 Bas Wijnen <shevek@fmf.nl>
 * Copyright (C) 2006 Roland Clobus <rclobus@bigfoot.com>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __game_h
#define __game_h

#include <stdio.h>
#include "map.h"
#include "driver.h"

typedef enum {
	DEVEL_ROAD_BUILDING,
	DEVEL_MONOPOLY,
	DEVEL_YEAR_OF_PLENTY,
	DEVEL_CHAPEL,
	DEVEL_UNIVERSITY,
	DEVEL_GOVERNORS_HOUSE,
	DEVEL_LIBRARY,
	DEVEL_MARKET,
	DEVEL_SOLDIER
} DevelType;
#define NUM_DEVEL_TYPES (DEVEL_SOLDIER + 1)

#define MAX_PLAYERS 8		/* maximum number of players supported */
#define MAX_CHAT 496		/* maximum chat message size
				 * (512 - strlen("player 0 chat \n") - 1) */
#define MAX_NAME_LENGTH 30	/* maximum length for the name of a player */

typedef enum {
	VAR_DEFAULT,		/* plain out-of-the-box game */
	VAR_ISLANDS,		/* Islands of Catan */
	VAR_INTIMATE		/* Intimate Catan */
} GameVariant;

typedef struct {
	gchar *title;		/* title of the game */
	GameVariant variant;	/* which variant is being played */
	gboolean random_terrain;	/* shuffle terrain location? */
	gboolean strict_trade;	/* trade only before build/buy? */
	gboolean domestic_trade;	/* player trading allowed? */
	gint num_players;	/* number of players in the game */
	gint sevens_rule;	/* what to do when a seven is rolled */
	/* 0 = normal, 1 = no 7s on first 2 turns (official rule variant),
	 * 2 = all 7s rerolled */
	gint victory_points;	/* target number of victory points */
	gboolean check_victory_at_end_of_turn;	/* check victory only at end of turn */
	gint num_build_type[NUM_BUILD_TYPES];	/* number of each build type */
	gint resource_count;	/* number of each resource */
	gint num_develop_type[NUM_DEVEL_TYPES];	/* number of each development */
	Map *map;		/* the game map */
	gboolean parsing_map;	/* currently parsing map? *//* Not in game_params[] */
	gint tournament_time;	/* time to start tournament time in minutes *//* Not in game_params[] */
	gboolean quit_when_done;	/* server quits after someone wins *//* Not in game_params[] */
	gboolean use_pirate;	/* is there a pirate in this game? */
	GArray *island_discovery_bonus;	/* list of VPs for discovering an island */
} GameParams;

typedef struct {
	gint id;		/* identification for client-server communication */
	gchar *name;		/* name of the item */
	gint points;		/* number of points */
} Points;

typedef enum {
	PARAMS_WINNABLE,	/* the game can be won */
	PARAMS_WIN_BUILD_ALL,	/* the game can be won by building all */
	PARAMS_WIN_PERHAPS,	/* the game could be won */
	PARAMS_NO_WIN		/* the game cannot be won */
} WinnableState;

typedef void (*WriteLineFunc) (gpointer user_data, const gchar *);

GameParams *params_new(void);
GameParams *params_copy(const GameParams * params);
GameParams *params_load_file(const gchar * fname);
void params_free(GameParams * params);
void params_write_lines(GameParams * params, gboolean write_secrets,
			WriteLineFunc func, gpointer user_data);
gboolean params_write_file(GameParams * params, const gchar * fname);
gboolean params_load_line(GameParams * params, gchar * line);
gboolean params_load_finish(GameParams * params);
gboolean read_line_from_file(gchar ** line, FILE * f);
/** Check whether, in theory, the game could be won by a player.
 * @param params The game parameters
 * @retval win_message A message describing how/when the game can be won
 * @retval point_specification A message describing how the points are distributed
 * @return Whether the game can be won
 */
WinnableState params_check_winnable_state(const GameParams * params,
					  gchar ** win_message,
					  gchar ** point_specification);

Points *points_new(gint id, const gchar * name, gint points);
void points_free(Points * points);
#endif
