/* Gnocatan - Implementation of the excellent Settlers of Catan board game.
 *   Go buy a copy.
 *
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

#ifndef _client_h
#define _client_h

#include "state.h"
#include "game.h"
#include "map.h"
#include "callback.h"

/* variables */
extern Map *map;
extern GameParams *game_params;

/********* client.c ***********/
/* client initialization */
void client_init (void); /* before frontend initialization */
void client_start (int argc, char **argv); /* after frontend initialization */

/* access the state machine (a client has only one state machine) */
StateMachine *SM (void);

/* handle a player name change */
void copy_player_name (const gchar *name);

/* state machine modes */
gboolean mode_connecting(StateMachine *sm, gint event);
gboolean mode_start(StateMachine *sm, gint event);
gboolean mode_build_response(StateMachine *sm, gint event);
gboolean mode_move_response(StateMachine *sm, gint event);
gboolean mode_done_response(StateMachine *sm, gint event);
gboolean mode_robber_response(StateMachine *sm, gint event);
gboolean mode_monopoly_response(StateMachine *sm, gint event);
gboolean mode_year_of_plenty_response(StateMachine *sm, gint event);
gboolean mode_play_develop_response(StateMachine *sm, gint event);
gboolean mode_roll_response(StateMachine *sm, gint event);
gboolean mode_buy_develop_response(StateMachine *sm, gint event);
gboolean mode_undo_response(StateMachine *sm, gint event);
gboolean mode_trade_call_response(StateMachine *sm, gint event);
gboolean mode_trade_maritime_response(StateMachine *sm, gint event);
gboolean mode_trade_call_again_response(StateMachine *sm, gint event);
gboolean mode_trade_domestic_response(StateMachine *sm, gint event);
gboolean mode_domestic_finish_response(StateMachine *sm, gint event);
gboolean mode_quote_finish_response(StateMachine *sm, gint event);
gboolean mode_quote_submit_response(StateMachine *sm, gint event);
gboolean mode_quote_delete_response(StateMachine *sm, gint event);

/******** chat.c ************/
/* parse an incoming chat message */
void chat_parser (gint player_num, char *chat_str);

/******* player.c **********/
void player_reset (void);
void player_set_my_num(gint player_num);
void player_modify_statistic(gint player_num, StatisticType type, gint num);
void player_change_name(gint player_num, const gchar *name);
void player_has_quit(gint player_num);
void player_largest_army(gint player_num);
void player_longest_road(gint player_num);
void player_set_current(gint player_num);
void player_set_total_num(gint num);
void player_stole_from(gint player_num, gint victim_num, Resource resource);
void player_domestic_trade(gint player_num, gint partner_num, gint *supply,
		gint *receive);
void player_maritime_trade(gint player_num, gint ratio, Resource supply,
		Resource receive);
void player_build_add(gint player_num, BuildType type, gint x, gint y,
		gint pos, gboolean log_changes);
void player_build_remove(gint player_num, BuildType type, gint x, gint y,
		gint pos);
void player_build_move(gint player_num, gint sx, gint sy, gint spos,
		gint dx, gint dy, gint dpos, gint isundo);
void player_resource_action(gint player_num, gchar *action,
		gint *resource_list, gint mult);
void player_get_point (gint player_num, gint id, gchar *str, gint num);
void player_lose_point (gint player_num, gint id);
void player_take_point (gint player_num, gint id, gint old_owner);

/********* build.c **********/
void build_clear(void);
void build_new_turn (void);
void build_remove (BuildType build_type, gint x, gint y, gint pos);
void build_move (gint sx, gint sy, gint spos, gint dx, gint dy, gint dpos,
		gint isundo);
void build_add(BuildType type, gint x, gint y, gint pos, gint *cost,
		gboolean newbuild);
gboolean build_can_undo(void);
gboolean build_is_valid(void);
gboolean have_built(void);
gboolean build_can_setup_road(Edge *edge, gboolean double_setup);
gboolean build_can_setup_ship(Edge *edge, gboolean double_setup);
gboolean build_can_setup_bridge(Edge *edge, gboolean double_setup);
gboolean build_can_setup_settlement(Node *node, gboolean double_setup);

/********** develop.c **********/
void develop_init(void);
void develop_bought_card_turn(DevelType type, gint turnbought);
void develop_bought_card(DevelType type);
void develop_reset_have_played_bought(gboolean played_develop,
		gboolean bought_develop);
void develop_bought(gint player_num);
void develop_played(gint player_num, gint card_idx, DevelType type);
void monopoly_player(gint player_num, gint victim_num, gint num, Resource type);
void road_building_begin (void);
void develop_begin_turn (void);
gboolean have_bought_develop(void);

/********** stock.c **********/
void stock_init(void);
void stock_use_road (void);
void stock_replace_road (void);
void stock_use_ship (void);
void stock_replace_ship (void);
void stock_use_bridge (void);
void stock_replace_bridge (void);
void stock_use_settlement (void);
void stock_replace_settlement (void);
void stock_use_city (void);
void stock_replace_city (void);
void stock_use_develop(void);

/********** resource.c **********/
void resource_init (void);
void resource_apply_list (gint player_num, gint *resources, gint multiplier);
void resource_cards (gint num, Resource which, gchar *buffer, gint buf_size);
gint resource_count (gint *resources);
void resource_format_num (gchar *desc, guint desc_size, gint *resources);
void resource_log_list (gint player_num, gchar *action, gint *resources);
void resource_modify (Resource type, gint num);

/********** robber.c **********/
void robber_move_on_map(gint x, gint y);
void pirate_move_on_map(gint x, gint y);
void robber_moved(gint player_num, gint x, gint y);
void pirate_moved (gint player_num, gint x, gint y);
void robber_begin_move(gint player_num);

/********* setup.c *********/
void setup_begin(gint player_num);
void setup_begin_double(gint player_num);

/********* turn.c *********/
void turn_rolled_dice(gint player_num, gint die1, gint die2);
void turn_begin(gint player_num, gint turn_num);

#endif
