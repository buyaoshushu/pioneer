/* Gnocatan - Implementation of the excellent Settlers of Catan board game.
 *   Go buy a copy.
 *
 * Copyright (C) 1999 the Free Software Foundation
 * Copyright (C) 2003-2005 Bas Wijnen <shevek@fmf.nl>
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

#include "config.h"
#include <ctype.h>
#include <string.h>
#include "game.h"
#include "cards.h"
#include "map.h"
#include "network.h"
#include "log.h"
#include "cost.h"
#include "client.h"
#include "state.h"
#include "callback.h"
#include "buildrec.h"
#include "quoteinfo.h"

static enum callback_mode previous_mode;
GameParams *game_params;
Map *map;
static gchar *saved_name;
struct recovery_info_t {
	gchar prevstate[40];
	gint turnnum;
	gint playerturn;
	gint numdiscards;
	gboolean rolled_dice;
	gint die1, die2;
	gboolean played_develop;
	gboolean bought_develop;
	GList *build_list;
	gboolean ship_moved;
};

static gboolean global_unhandled(StateMachine * sm, gint event);
static gboolean global_filter(StateMachine * sm, gint event);
static gboolean mode_offline(StateMachine * sm, gint event);
static gboolean mode_players(StateMachine * sm, gint event);
static gboolean mode_player_list(StateMachine * sm, gint event);
static gboolean mode_load_game(StateMachine * sm, gint event);
static gboolean mode_load_gameinfo(StateMachine * sm, gint event);
static gboolean mode_start_response(StateMachine * sm, gint event);
static gboolean mode_setup(StateMachine * sm, gint event);
static gboolean mode_idle(StateMachine * sm, gint event);
static gboolean mode_wait_for_robber(StateMachine * sm, gint event);
static gboolean mode_road_building(StateMachine * sm, gint event);
static gboolean mode_monopoly(StateMachine * sm, gint event);
static gboolean mode_year_of_plenty(StateMachine * sm, gint event);
static gboolean mode_robber(StateMachine * sm, gint event);
static gboolean mode_discard(StateMachine * sm, gint event);
static gboolean mode_turn(StateMachine * sm, gint event);
static gboolean mode_turn_rolled(StateMachine * sm, gint event);
static gboolean mode_domestic_trade(StateMachine * sm, gint event);
static gboolean mode_domestic_quote(StateMachine * sm, gint event);
static gboolean mode_domestic_monitor(StateMachine * sm, gint event);
static gboolean mode_game_over(StateMachine * sm, gint event);
static gboolean mode_choose_gold(StateMachine * sm, gint event);
static gboolean mode_wait_resources(StateMachine * sm, gint event);
static void recover_from_disconnect(StateMachine * sm,
				    struct recovery_info_t *rinfo);

/* Create and/or return the client state machine.
 */
StateMachine *SM()
{
	static StateMachine *state_machine;
	if (state_machine == NULL) {
		state_machine = sm_new(NULL);
		sm_global_set(state_machine, global_filter);
		sm_unhandled_set(state_machine, global_unhandled);
	}
	return state_machine;
}

/* When commands are sent to the server, front ends may want to update
 * the status bar or something to indicate the the game is currently
 * waiting for server respose.
 * Since the GUI may get disabled while waiting, it is good to let the
 * user know why all controls are unresponsive.
 */
static void waiting_for_network(gboolean is_waiting)
{
	if (is_waiting) {
		callbacks.network_status(_("Waiting"));
	} else {
		callbacks.network_status(_("Idle"));
	}
	callbacks.network_wait(is_waiting);
}

/* Dummy callback functions. They do nothing */
static void dummy_init(UNUSED(int argc), UNUSED(char **argv))
{;
}
static void dummy_network_status(UNUSED(gchar * description))
{;
}
static void dummy_instructions(UNUSED(gchar * message))
{;
}
static void dummy_network_wait(UNUSED(gboolean is_waiting))
{;
}
static void dummy_offline(void)
{;
}
static void dummy_discard(void)
{;
}
static void dummy_discard_add(UNUSED(gint player_num),
			      UNUSED(gint discard_num))
{;
}
static void dummy_discard_remove(UNUSED(gint player_num),
				 UNUSED(gint * resources))
{;
}
static void dummy_discard_done(void)
{;
}
static void dummy_gold(void)
{;
}
static void dummy_gold_add(UNUSED(gint player_num), UNUSED(gint gold_num))
{;
}
static void dummy_gold_remove(UNUSED(gint player_num),
			      UNUSED(gint * resources))
{;
}
static void dummy_gold_choose(UNUSED(gint gold_num), UNUSED(gint * bank))
{;
}
static void dummy_gold_done(void)
{;
}
static void dummy_game_over(UNUSED(gint player_num), UNUSED(gint points))
{;
}
static void dummy_init_game(void)
{;
}
static void dummy_start_game(void)
{;
}
static void dummy_setup(UNUSED(unsigned num_settlements),
			UNUSED(unsigned num_roads))
{;
}
static void dummy_quote(UNUSED(gint player_num),
			UNUSED(gint * they_supply),
			UNUSED(gint * they_receive))
{;
}
static void dummy_roadbuilding(UNUSED(gint num_roads))
{;
}
static void dummy_monopoly(void)
{;
}
static void dummy_plenty(UNUSED(gint * bank))
{;
}
static void dummy_turn(void)
{;
}
static void dummy_player_turn(UNUSED(gint player_num))
{;
}
static void dummy_trade(void)
{;
}
static void dummy_trade_player_end(UNUSED(gint player_num))
{;
}
static void dummy_trade_add_quote(UNUSED(gint player_num),
				  UNUSED(gint quote_num),
				  UNUSED(gint * they_supply),
				  UNUSED(gint * they_receive))
{;
}
static void dummy_trade_remove_quote(UNUSED(gint player_num),
				     UNUSED(gint quote_num))
{;
}
static void dummy_trade_domestic(UNUSED(gint partner_num),
				 UNUSED(gint quote_num),
				 UNUSED(gint * we_supply),
				 UNUSED(gint * we_receive))
{;
}
static void dummy_trade_maritime(UNUSED(gint ratio),
				 UNUSED(Resource we_supply),
				 UNUSED(Resource we_receive))
{;
}
static void dummy_quote_player_end(UNUSED(gint player_num))
{;
}
static void dummy_quote_add(UNUSED(gint player_num),
			    UNUSED(gint quote_num),
			    UNUSED(gint * they_supply),
			    UNUSED(gint * they_receive))
{;
}
static void dummy_quote_remove(UNUSED(gint player_num),
			       UNUSED(gint quote_num))
{;
}
static void dummy_quote_start(void)
{;
}
static void dummy_quote_end(void)
{;
}
static void dummy_quote_monitor(void)
{;
}
static void dummy_quote_trade(UNUSED(gint player_num),
			      UNUSED(gint partner_num),
			      UNUSED(gint quote_num),
			      UNUSED(gint * they_supply),
			      UNUSED(gint * they_receive))
{;
}
static void dummy_rolled_dice(UNUSED(gint die1), UNUSED(gint die2),
			      UNUSED(gint player_num))
{;
}
static void dummy_beep(void)
{;
}
static void dummy_draw_edge(UNUSED(Edge * edge))
{;
}
static void dummy_draw_node(UNUSED(Node * node))
{;
}
static void dummy_bought_develop(UNUSED(DevelType type))
{;
}
static void dummy_played_develop(UNUSED(gint player_num),
				 UNUSED(gint card_idx),
				 UNUSED(DevelType type))
{;
}
static void dummy_resource_change(UNUSED(Resource type), UNUSED(gint num))
{;
}
static void dummy_draw_hex(UNUSED(Hex * hex))
{;
}
static void dummy_update_stock(void)
{;
}
static void dummy_robber(void)
{;
}
static void dummy_robber_moved(UNUSED(Hex * old), UNUSED(Hex * new))
{;
}
static void dummy_player_robbed(UNUSED(gint robber_num),
				UNUSED(gint victim_num),
				UNUSED(Resource resource))
{;
}
static void dummy_get_rolled_resources(UNUSED(gint player_num),
				       UNUSED(const gint * resources))
{;
}
static void dummy_new_statistics(UNUSED(gint player_num),
				 UNUSED(StatisticType type),
				 UNUSED(gint num))
{;
}
static void dummy_viewer_name(UNUSED(gint viewer_num),
			      UNUSED(const gchar * name))
{;
}
static void dummy_player_name(UNUSED(gint player_num),
			      UNUSED(const gchar * name))
{;
}
static void dummy_player_quit(UNUSED(gint player_num))
{;
}
static void dummy_viewer_quit(UNUSED(gint player_num))
{;
}
static void dummy_new_bank(UNUSED(const gint * new_bank))
{;
}
static void dummy_error(UNUSED(gchar * message))
{;
}

/*----------------------------------------------------------------------
 * Entry point for the client state machine
 */
void client_init(void)
{
	/* first set everything to 0, so we are sure it segfaults if
	 * someone forgets to update this when adding a new callback */
	memset(&callbacks, 0, sizeof(callbacks));
	/* set all callbacks to their default value: doing nothing */
	callbacks.init = &dummy_init;
	callbacks.network_status = &dummy_network_status;
	callbacks.instructions = &dummy_instructions;
	callbacks.network_wait = &dummy_network_wait;
	callbacks.offline = &dummy_offline;
	callbacks.discard = &dummy_discard;
	callbacks.discard_add = &dummy_discard_add;
	callbacks.discard_remove = &dummy_discard_remove;
	callbacks.discard_done = &dummy_discard_done;
	callbacks.gold = &dummy_gold;
	callbacks.gold_add = &dummy_gold_add;
	callbacks.gold_remove = &dummy_gold_remove;
	callbacks.gold_choose = &dummy_gold_choose;
	callbacks.gold_done = &dummy_gold_done;
	callbacks.game_over = &dummy_game_over;
	callbacks.init_game = &dummy_init_game;
	callbacks.start_game = &dummy_start_game;
	callbacks.setup = &dummy_setup;
	callbacks.quote = &dummy_quote;
	callbacks.roadbuilding = &dummy_roadbuilding;
	callbacks.monopoly = &dummy_monopoly;
	callbacks.plenty = &dummy_plenty;
	callbacks.turn = &dummy_turn;
	callbacks.player_turn = &dummy_player_turn;
	callbacks.trade = &dummy_trade;
	callbacks.trade_player_end = &dummy_trade_player_end;
	callbacks.trade_add_quote = &dummy_trade_add_quote;
	callbacks.trade_remove_quote = &dummy_trade_remove_quote;
	callbacks.trade_domestic = &dummy_trade_domestic;
	callbacks.trade_maritime = &dummy_trade_maritime;
	callbacks.quote_player_end = &dummy_quote_player_end;
	callbacks.quote_add = &dummy_quote_add;
	callbacks.quote_remove = &dummy_quote_remove;
	callbacks.quote_start = &dummy_quote_start;
	callbacks.quote_end = &dummy_quote_end;
	callbacks.quote_monitor = &dummy_quote_monitor;
	callbacks.quote_trade = &dummy_quote_trade;
	callbacks.rolled_dice = &dummy_rolled_dice;
	callbacks.beep = &dummy_beep;
	callbacks.draw_edge = &dummy_draw_edge;
	callbacks.draw_node = &dummy_draw_node;
	callbacks.bought_develop = &dummy_bought_develop;
	callbacks.played_develop = &dummy_played_develop;
	callbacks.resource_change = &dummy_resource_change;
	callbacks.draw_hex = &dummy_draw_hex;
	callbacks.update_stock = &dummy_update_stock;
	callbacks.robber = &dummy_robber;
	callbacks.robber_moved = &dummy_robber_moved;
	callbacks.player_robbed = &dummy_player_robbed;
	callbacks.get_rolled_resources = &dummy_get_rolled_resources;
	callbacks.new_statistics = &dummy_new_statistics;
	callbacks.viewer_name = &dummy_viewer_name;
	callbacks.player_name = &dummy_player_name;
	callbacks.player_quit = &dummy_player_quit;
	callbacks.viewer_quit = &dummy_viewer_quit;
	callbacks.new_bank = &dummy_new_bank;
	callbacks.error = &dummy_error;
	/* mainloop is not set here */
	resource_init();
}

void client_start(int argc, char **argv)
{
	callbacks.init(argc, argv);
	sm_goto(SM(), mode_offline);
}

/*----------------------------------------------------------------------
 * The state machine API supports two global event handling callbacks.
 *
 * All events are sent to the global event handler before they are
 * sent to the current state function.  If the global event handler
 * handles the event and returns TRUE, the event will not be sent to
 * the current state function.  is which allow unhandled events to be
 * processed via a callback.
 *
 * If an event is not handled by either the global event handler, or
 * the current state function, then it will be sent to the unhandled
 * event handler.  Using this, the client code implements some of the
 * error handling globally.
 */

/* Global event handler - this get first crack at events.  If we
 * return TRUE, the event will not be passed to the current state
 * function.
 */
static gboolean global_filter(StateMachine * sm, gint event)
{
	switch (event) {
	case SM_NET_CLOSE:
		log_message(MSG_ERROR,
			    _("We have been kicked out of the game.\n"));
		sm_pop_all(sm);
		sm_goto(sm, mode_offline);
		callbacks.network_status(_("Offline"));
		return TRUE;
	default:
		break;
	}
	return FALSE;
}

/* Global unhandled event handler - this get called for events that
 * fall through the state machine without being handled.
 */
static gboolean global_unhandled(StateMachine * sm, gint event)
{
	char str[512];

	switch (event) {
	case SM_NET_CLOSE:
		g_error("SM_NET_CLOSE not caught by global_filter.\n");
	case SM_RECV:
		/* all errors start with ERR */
		if (sm_recv(sm, "ERR %S", str, sizeof(str))) {
			log_message(MSG_ERROR, "Error (%s): %s\n",
				    sm_current_name(sm), str);
			callbacks.error(str);
			return TRUE;
		}
		/* notices which are not errors should appear in the message
		 * window */
		if (sm_recv(sm, "NOTE %S", str, sizeof(str))) {
			log_message(MSG_ERROR, "Notice: %s\n", str);
			return TRUE;
		}
		/* protocol extensions which may be ignored have this prefix
		 * before the next protocol changing version of the game is
		 * released.  Notify the client about it anyway. */
		if (sm_recv(sm, "extension %S", str, sizeof(str))) {
			log_message(MSG_INFO,
				    "Ignoring extension used by server: %s\n",
				    str);
			return TRUE;
		}
		/* we're receiving strange things */
		if (sm_recv(sm, "%S", str, sizeof(str))) {
			log_message(MSG_ERROR,
				    "Unknown message in %s: %s\n",
				    sm_current_name(sm), str);
			return TRUE;
		}
		/* this is never reached: everything matches "%S" */
		g_error
		    ("This should not be possible, please report this bug.\n");
	default:
		break;
	}
	/* this may happen, for example when a hotkey is used for a function
	 * which cannot be activated */
	return FALSE;
}

/*----------------------------------------------------------------------
 * Hooks for GUI events that can happen at almost any time
 */
void copy_player_name(const gchar * name)
{
	char *tmp = g_strdup(name);
	tmp = g_strstrip(tmp);
	if (*tmp != '\0') {
		if (saved_name != NULL)
			g_free(saved_name);
		saved_name = g_strdup(tmp);
	}
	g_free(tmp);
}

/*----------------------------------------------------------------------
 * Server notifcations about player name changes and chat messages.
 * These can happen in any state (maybe this should be moved to
 * global_filter()?).
 */
static gboolean check_chat_or_name(StateMachine * sm)
{
	gint player_num;
	char str[512];

	if (sm_recv
	    (sm, "player %d chat %S", &player_num, str, sizeof(str))) {
		chat_parser(player_num, str);
		return TRUE;
	}
	if (sm_recv(sm, "player %d is %S", &player_num, str, sizeof(str))) {
		player_change_name(player_num, str);
		return TRUE;
	}
	return FALSE;
}

/*----------------------------------------------------------------------
 * Server notifcations about other players name changes and chat
 * messages.  These can happen in almost any state in which the game
 * is running.
 */
static gboolean check_other_players(StateMachine * sm)
{
	BuildType build_type;
	DevelType devel_type;
	Resource resource_type, supply_type, receive_type;
	gint player_num, victim_num, card_idx, backwards;
	gint turn_num, discard_num, gold_num, num, ratio, die1, die2, x, y,
	    pos;
	gint id;
	gint resource_list[NO_RESOURCE];
	gint sx, sy, spos, dx, dy, dpos;
	gchar str[512];

	if (check_chat_or_name(sm))
		return TRUE;
	if (!sm_recv_prefix(sm, "player %d ", &player_num))
		return FALSE;

	if (sm_recv(sm, "built %B %d %d %d", &build_type, &x, &y, &pos)) {
		player_build_add(player_num, build_type, x, y, pos, TRUE);
		return TRUE;
	}
	if (sm_recv
	    (sm, "move %d %d %d %d %d %d", &sx, &sy, &spos, &dx, &dy,
	     &dpos)) {
		player_build_move(player_num, sx, sy, spos, dx, dy, dpos,
				  FALSE);
		return TRUE;
	}
	if (sm_recv
	    (sm, "move-back %d %d %d %d %d %d", &sx, &sy, &spos, &dx, &dy,
	     &dpos)) {
		player_build_move(player_num, sx, sy, spos, dx, dy, dpos,
				  TRUE);
		return TRUE;
	}
	if (sm_recv(sm, "remove %B %d %d %d", &build_type, &x, &y, &pos)) {
		player_build_remove(player_num, build_type, x, y, pos);
		return TRUE;
	}
	if (sm_recv(sm, "receives %R", resource_list)) {
		player_resource_action(player_num, _("%s receives %s.\n"),
				       resource_list, 1);
		callbacks.get_rolled_resources(player_num, resource_list);
		return TRUE;
	}
	if (sm_recv(sm, "spent %R", resource_list)) {
		player_resource_action(player_num, _("%s spent %s.\n"),
				       resource_list, -1);
		return TRUE;
	}
	if (sm_recv(sm, "refund %R", resource_list)) {
		player_resource_action(player_num,
				       _("%s is refunded %s.\n"),
				       resource_list, 1);
		return TRUE;
	}
	if (sm_recv(sm, "bought-develop")) {
		develop_bought(player_num);
		return TRUE;
	}
	if (sm_recv(sm, "play-develop %d %D", &card_idx, &devel_type)) {
		develop_played(player_num, card_idx, devel_type);
		return TRUE;
	}
	if (sm_recv(sm, "turn %d", &turn_num)) {
		turn_begin(player_num, turn_num);
		return TRUE;
	}
	if (sm_recv(sm, "rolled %d %d", &die1, &die2)) {
		turn_rolled_dice(player_num, die1, die2);
		if (die1 + die2 != 7)
			sm_push(sm, mode_wait_resources);
		return TRUE;
	}
	if (sm_recv(sm, "must-discard %d", &discard_num)) {
		sm_push(sm, mode_discard);
		if (player_num == my_player_num())
			callback_mode = MODE_DISCARD;
		callbacks.discard_add(player_num, discard_num);
		return TRUE;
	}
	if (sm_recv(sm, "discarded %R", resource_list)) {
		player_resource_action(player_num, _("%s discarded %s.\n"),
				       resource_list, -1);
		callbacks.discard_remove(player_num, resource_list);
		return TRUE;
	}
	if (sm_recv(sm, "prepare-gold %d", &gold_num)) {
		sm_goto(sm, mode_choose_gold);
		callbacks.gold_add(player_num, gold_num);
		return TRUE;
	}
	if (sm_recv(sm, "is-robber")) {
		robber_begin_move(player_num);
		return TRUE;
	}
	if (sm_recv(sm, "robber %d %d", &x, &y)) {
		robber_moved(player_num, x, y);
		return TRUE;
	}
	if (sm_recv(sm, "stole from %d", &victim_num)) {
		player_stole_from(player_num, victim_num, NO_RESOURCE);
		return TRUE;
	}
	if (sm_recv(sm, "stole %r from %d", &resource_type, &victim_num)) {
		player_stole_from(player_num, victim_num, resource_type);
		return TRUE;
	}
	if (sm_recv
	    (sm, "monopoly %d %r from %d", &num, &resource_type,
	     &victim_num)) {
		monopoly_player(player_num, victim_num, num,
				resource_type);
		return TRUE;
	}
	if (sm_recv(sm, "largest-army")) {
		player_largest_army(player_num);
		return TRUE;
	}
	if (sm_recv(sm, "longest-road")) {
		player_longest_road(player_num);
		return TRUE;
	}
	if (sm_recv(sm, "get-point %d %d %S", &id, &num, str, sizeof(str))) {
		player_get_point(player_num, id, str, num);
		return TRUE;
	}
	if (sm_recv(sm, "lose-point %d", &id)) {
		player_lose_point(player_num, id);
		return TRUE;
	}
	if (sm_recv(sm, "take-point %d %d", &id, &victim_num)) {
		player_take_point(player_num, id, victim_num);
		return TRUE;
	}
	if (sm_recv(sm, "setup %d", &backwards)) {
		setup_begin(player_num);
		if (backwards)
			sm_push(sm, mode_wait_resources);
		return TRUE;
	}
	if (sm_recv(sm, "setup-double")) {
		setup_begin_double(player_num);
		sm_push(sm, mode_wait_resources);
		return TRUE;
	}
	if (sm_recv(sm, "won with %d", &num)) {
		callbacks.game_over(player_num, num);
		sm_pop_all_and_goto(sm, mode_game_over);
		return TRUE;
	}
	if (sm_recv(sm, "has quit")) {
		player_has_quit(player_num);
		return TRUE;
	}

	if (sm_recv(sm, "maritime-trade %d supply %r receive %r",
		    &ratio, &supply_type, &receive_type)) {
		player_maritime_trade(player_num, ratio, supply_type,
				      receive_type);
		return TRUE;
	}

	sm_cancel_prefix(sm);
	return FALSE;
}

/*----------------------------------------------------------------------
 * State machine state functions.
 *
 * The state machine API works like this:
 *
 * SM_ENTER:
 *   When a state is entered the new state function is called with the
 *   SM_ENTER event.  This allows the client to perform state
 *   initialisation.
 *
 * SM_INIT:
 *   When a state is entered, and every time an event is handled, the
 *   state machine code calls the current state function with an
 *   SM_INIT event.
 *
 * SM_RECV:
 *   Indicates that a message has been received from the server.
 *
 * SM_NET_*:
 *   These are network connection related events.
 *
 * To change current state function, use sm_goto().
 *
 * The state machine API also implements a state stack.  This allows
 * us to reuse parts of the state machine by pushing the current
 * state, and then returning to it when the nested processing is
 * complete.
 *
 * The state machine nesting can be used via sm_push() and sm_pop().
 */

/*----------------------------------------------------------------------
 * Game startup and offline handling
 */

static gboolean mode_offline(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_offline");
	switch (event) {
	case SM_ENTER:
		callback_mode = MODE_INIT;
		callbacks.offline();
		break;
	default:
		break;
	}
	return FALSE;
}

/* Waiting for connect to complete
 */
gboolean mode_connecting(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_connecting");
	switch (event) {
	case SM_NET_CONNECT:
		sm_goto(sm, mode_start);
		return TRUE;
	case SM_NET_CONNECT_FAIL:
		sm_goto(sm, mode_offline);
		return TRUE;
	default:
		break;
	}
	return FALSE;
}

/* Handle initial signon message
 */
gboolean mode_start(StateMachine * sm, gint event)
{
	gint player_num, total_num;
	gchar version[512];

	sm_state_name(sm, "mode_start");

	if (event == SM_ENTER) {
		callbacks.network_status(_("Loading"));
		player_reset();
		callbacks.init_game();
	}

	if (event != SM_RECV)
		return FALSE;
	if (sm_recv(sm, "version report")) {
		sm_send(sm, "version %s\n", PROTOCOL_VERSION);
		return TRUE;
	}
	if (sm_recv(sm, "status report")) {
		if (saved_name != NULL) {
			sm_send(sm, "status reconnect %s\n", saved_name);
			return TRUE;
		} else {
			sm_send(sm, "status newplayer\n");
			return TRUE;
		}
	}
	if (sm_recv(sm, "player %d of %d, welcome to gnocatan server %S",
		    &player_num, &total_num, version, sizeof(version))) {
		player_set_my_num(player_num);
		player_set_total_num(total_num);
		sm_send(sm, "players\n");
		sm_goto(sm, mode_players);

		return TRUE;
	}
	if (sm_recv(sm, "ERR sorry, version conflict")) {
		sm_pop_all(sm);
		sm_goto(sm, mode_offline);
		callbacks.network_status(_("Offline"));
		callbacks.instructions(_("Version mismatch"));
		log_message(MSG_ERROR,
			    "Connect Error: Version mismatch! Please make sure client and server are up to date.\n");
		return TRUE;
	}
	return check_chat_or_name(sm);
}

/* Response to "players" command
 */
static gboolean mode_players(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_players");
	if (event != SM_RECV)
		return FALSE;
	if (sm_recv(sm, "players follow")) {
		sm_goto(sm, mode_player_list);
		return TRUE;
	}
	return check_other_players(sm);
}

/* Handle list of players
 */
static gboolean mode_player_list(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_player_list");
	if (event != SM_RECV)
		return FALSE;
	if (sm_recv(sm, ".")) {
		sm_send(sm, "game\n");
		sm_goto(sm, mode_load_game);
		return TRUE;
	}
	return check_other_players(sm);
}

/* Response to "game" command
 */
static gboolean mode_load_game(StateMachine * sm, gint event)
{
	gchar str[512];

	sm_state_name(sm, "mode_load_game");
	if (event != SM_RECV)
		return FALSE;
	if (sm_recv(sm, "game")) {
		if (game_params != NULL)
			params_free(game_params);
		game_params = params_new();
		return TRUE;
	}
	if (sm_recv(sm, "end")) {
		params_load_finish(game_params);
		map = game_params->map;
		stock_init();
		develop_init();
		sm_send(sm, "gameinfo\n");
		sm_goto(sm, mode_load_gameinfo);
		return TRUE;
	}
	if (check_other_players(sm))
		return TRUE;
	if (sm_recv(sm, "%S", str, sizeof(str))) {
		params_load_line(game_params, str);
		return TRUE;
	}
	return FALSE;
}

/* Response to "gameinfo" command
 */
static gboolean mode_load_gameinfo(StateMachine * sm, gint event)
{
	gchar str[512];
	gint x, y, pos, owner;
	static struct recovery_info_t rinfo = { "", -1, -1, -1, FALSE,
		-1, -1, FALSE, FALSE, NULL, FALSE
	};
	static gboolean disconnected = FALSE;
	static gboolean have_bank = FALSE;
	static gint devcardidx = -1;
	static gint numdevcards = -1;
	gint num_roads, num_bridges, num_ships, num_settlements,
	    num_cities, num_soldiers, road_len;
	gint opnum, opnassets, opncards, opnsoldiers;
	gboolean pchapel, puniv, pgov, plibr, pmarket, plongestroad,
	    plargestarmy;
	DevelType devcard;
	gint devcardturnbought;
	BuildType btype;
	BuildType prevbtype; /** @todo RC 2004-03-28 Protocol needs change for 0.9 This field should not be sent */
	gint resources[NO_RESOURCE];
	gint tmp_bank[NO_RESOURCE];

	sm_state_name(sm, "mode_load_gameinfo");
	if (event == SM_ENTER) {
		gint idx;

		have_bank = FALSE;
		for (idx = 0; idx < NO_RESOURCE; ++idx)
			tmp_bank[idx] = game_params->resource_count;
		set_bank(tmp_bank);
	}
	if (event != SM_RECV)
		return FALSE;
	if (sm_recv(sm, "gameinfo")) {
		return TRUE;
	}
	if (sm_recv(sm, ".")) {
		return TRUE;
	}
	if (sm_recv(sm, "end")) {
		callback_mode = MODE_WAIT_TURN;	/* allow chatting */
		callbacks.start_game();
		if (disconnected) {
			recover_from_disconnect(sm, &rinfo);
		} else {
			sm_send(sm, "start\n");
			sm_goto(sm, mode_start_response);
		}
		return TRUE;
	}
	if (sm_recv(sm, "extension bank %R", tmp_bank)) {
		set_bank(tmp_bank);
		have_bank = TRUE;
		return TRUE;
	}
	if (sm_recv(sm, "turn num %d", &rinfo.turnnum)) {
		return TRUE;
	}
	if (sm_recv(sm, "player turn: %d", &rinfo.playerturn)) {
		return TRUE;
	}
	if (sm_recv(sm, "dice rolled: %d %d", &rinfo.die1, &rinfo.die2)) {
		rinfo.rolled_dice = TRUE;
		return TRUE;
	}
	if (sm_recv(sm, "dice value: %d %d", &rinfo.die1, &rinfo.die2)) {
		return TRUE;
	}
	if (sm_recv(sm, "played develop")) {
		rinfo.played_develop = TRUE;
		return TRUE;
	}
	if (sm_recv(sm, "moved ship")) {
		rinfo.ship_moved = TRUE;
		return TRUE;
	}
	if (sm_recv(sm, "bought develop")) {
		rinfo.bought_develop = TRUE;
		return TRUE;
	}
	if (sm_recv(sm, "player disconnected")) {
		disconnected = TRUE;
		return TRUE;
	}
	if (sm_recv(sm, "state %S", str, sizeof(str))) {
		strcpy(rinfo.prevstate, str);
		return TRUE;
	}
	if (sm_recv(sm, "playerinfo: resources: %R", resources)) {
		resource_init();
		resource_apply_list(my_player_num(), resources, 1);
		/* If the bank was copied from the server, it should not be
		 * compensated for my own resources, because it was already
		 * correct.  So we compensate it back. */
		if (have_bank)
			modify_bank(resources);
		return TRUE;
	}
	if (sm_recv(sm, "playerinfo: numdevcards: %d", &numdevcards)) {
		devcardidx = 0;
		return TRUE;
	}
	if (sm_recv
	    (sm, "playerinfo: devcard: %d %d", &devcard,
	     &devcardturnbought)) {
		if (devcardidx >= numdevcards) {
			return FALSE;
		}

		develop_bought_card_turn(devcard, devcardturnbought);

		devcardidx++;
		if (devcardidx >= numdevcards) {
			devcardidx = numdevcards = -1;
		}
		return TRUE;
	}
	if (sm_recv
	    (sm, "playerinfo: %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
	     &num_roads, &num_bridges, &num_ships, &num_settlements,
	     &num_cities, &num_soldiers, &road_len, &pchapel, &puniv,
	     &pgov, &plibr, &pmarket, &plongestroad, &plargestarmy)) {
		player_modify_statistic(my_player_num(), STAT_SOLDIERS,
					num_soldiers);
		if (pchapel) {
			player_modify_statistic(my_player_num(),
						STAT_CHAPEL, 1);
		}
		if (puniv) {
			player_modify_statistic(my_player_num(),
						STAT_UNIVERSITY_OF_CATAN,
						1);
		}
		if (pgov) {
			player_modify_statistic(my_player_num(),
						STAT_GOVERNORS_HOUSE, 1);
		}
		if (plibr) {
			player_modify_statistic(my_player_num(),
						STAT_LIBRARY, 1);
		}
		if (pmarket) {
			player_modify_statistic(my_player_num(),
						STAT_MARKET, 1);
		}
		if (plongestroad) {
			player_modify_statistic(my_player_num(),
						STAT_LONGEST_ROAD, 1);
		}
		if (plargestarmy) {
			player_modify_statistic(my_player_num(),
						STAT_LARGEST_ARMY, 1);
		}
		return TRUE;
	}
	if (sm_recv
	    (sm, "otherplayerinfo: %d %d %d %d %d %d %d %d %d %d %d",
	     &opnum, &opnassets, &opncards, &opnsoldiers, &pchapel, &puniv,
	     &pgov, &plibr, &pmarket, &plongestroad, &plargestarmy)) {
		player_modify_statistic(opnum, STAT_RESOURCES, opnassets);
		player_modify_statistic(opnum, STAT_DEVELOPMENT, opncards);
		player_modify_statistic(opnum, STAT_SOLDIERS, opnsoldiers);
		if (!have_bank && opnassets != 0)
			log_message(MSG_ERROR,
				    _
				    ("Cannot determine bank, expect out of resource errors\n"));
		if (pchapel) {
			player_modify_statistic(opnum, STAT_CHAPEL, 1);
		}
		if (puniv) {
			player_modify_statistic(opnum,
						STAT_UNIVERSITY_OF_CATAN,
						1);
		}
		if (pgov) {
			player_modify_statistic(opnum,
						STAT_GOVERNORS_HOUSE, 1);
		}
		if (plibr) {
			player_modify_statistic(opnum, STAT_LIBRARY, 1);
		}
		if (pmarket) {
			player_modify_statistic(opnum, STAT_MARKET, 1);
		}
		if (plongestroad) {
			player_modify_statistic(opnum, STAT_LONGEST_ROAD,
						1);
		}
		if (plargestarmy) {
			player_modify_statistic(opnum, STAT_LARGEST_ARMY,
						1);
		}
		return TRUE;
	}
	if (sm_recv
	    (sm, "buildinfo: %B %d %d %d %d", &btype, &prevbtype, &x, &y,
	     &pos)) {
							 /** @todo RC 2004-03-28 Protocol needs change for 0.9: prevbtype should not be sent */
		BuildRec *rec;
		rec = g_malloc0(sizeof(*rec));
		rec->type = btype;
		rec->x = x;
		rec->y = y;
		rec->pos = pos;
		rinfo.build_list = g_list_append(rinfo.build_list, rec);
		return TRUE;
	}
	if (sm_recv(sm, "RO%d,%d", &x, &y)) {
		robber_move_on_map(x, y);
		return TRUE;
	}
	if (sm_recv(sm, "P%d,%d", &x, &y)) {
		pirate_move_on_map(x, y);
		return TRUE;
	}
	if (sm_recv(sm, "S%d,%d,%d,%d", &x, &y, &pos, &owner)) {
		player_build_add(owner, BUILD_SETTLEMENT, x, y, pos,
				 FALSE);
		return TRUE;
	}
	if (sm_recv(sm, "C%d,%d,%d,%d", &x, &y, &pos, &owner)) {
		player_build_add(owner, BUILD_CITY, x, y, pos, FALSE);
		return TRUE;
	}
	if (sm_recv(sm, "R%d,%d,%d,%d", &x, &y, &pos, &owner)) {
		player_build_add(owner, BUILD_ROAD, x, y, pos, FALSE);
		return TRUE;
	}
	if (sm_recv(sm, "SH%d,%d,%d,%d", &x, &y, &pos, &owner)) {
		player_build_add(owner, BUILD_SHIP, x, y, pos, FALSE);
		return TRUE;
	}
	if (sm_recv(sm, "B%d,%d,%d,%d", &x, &y, &pos, &owner)) {
		player_build_add(owner, BUILD_BRIDGE, x, y, pos, FALSE);
		return TRUE;
	}
	return FALSE;
}

/* Response to the "start" command
 */
static gboolean mode_start_response(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_start_response");
	if (event != SM_RECV)
		return FALSE;
	if (sm_recv(sm, "OK")) {
		sm_goto(sm, mode_idle);
		callbacks.network_status(_("Idle"));
		return TRUE;
	}
	return check_other_players(sm);
}

/*----------------------------------------------------------------------
 * Build command processing
 */

/* Handle response to build command
 */
gboolean mode_build_response(StateMachine * sm, gint event)
{
	BuildType build_type;
	gint x, y, pos;

	sm_state_name(sm, "mode_build_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv(sm, "built %B %d %d %d",
			    &build_type, &x, &y, &pos)) {
			build_add(build_type, x, y, pos, TRUE);
			waiting_for_network(FALSE);
			sm_pop(sm);
			return TRUE;
		}
		if (check_other_players(sm))
			return TRUE;
		waiting_for_network(FALSE);
		sm_pop(sm);
		break;
	}
	return FALSE;
}

/* Handle response to move ship command
 */
gboolean mode_move_response(StateMachine * sm, gint event)
{
	gint sx, sy, spos, dx, dy, dpos;

	sm_state_name(sm, "mode_move_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv(sm, "move %d %d %d %d %d %d",
			    &sx, &sy, &spos, &dx, &dy, &dpos)) {
			build_move(sx, sy, spos, dx, dy, dpos, FALSE);
			waiting_for_network(FALSE);
			sm_pop(sm);
			return TRUE;
		}
		if (check_other_players(sm))
			return TRUE;
		waiting_for_network(FALSE);
		sm_pop(sm);
		break;
	}
	return FALSE;
}

/*----------------------------------------------------------------------
 * Setup phase handling
 */

/* Response to a "done"
 */
gboolean mode_done_response(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_done_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv(sm, "OK")) {
			build_clear();
			waiting_for_network(FALSE);
			/* pop back to parent's parent if "done" worked */
			sm_multipop(sm, 2);
			return TRUE;
		}
		if (check_other_players(sm))
			return TRUE;
		waiting_for_network(FALSE);
		/* pop back to parent if "done" didn't work */
		sm_pop(sm);
		break;
	}
	return FALSE;
}

static char *setup_msg(void)
{
	static char msg[1024];
	char *msg_end;
	char *parts[5];
	int num_parts;
	int idx;

	if (is_setup_double())
		strcpy(msg,
		       _("Build two settlements, each with a connecting"));
	else
		strcpy(msg, _("Build a settlement with a connecting"));
	num_parts = 0;
	if (setup_can_build_road())
		parts[num_parts++] = _("road");
	if (setup_can_build_bridge())
		parts[num_parts++] = _("bridge");
	if (setup_can_build_ship())
		parts[num_parts++] = _("ship");

	msg_end = msg + strlen(msg);
	for (idx = 0; idx < num_parts; idx++) {
		if (idx > 0) {
			*msg_end++ = ',';
			if (idx == num_parts - 1) {
				strcpy(msg_end, _(" or"));
				msg_end += 3;
			}
		}
		*msg_end++ = ' ';
		strcpy(msg_end, parts[idx]);
		msg_end += strlen(msg_end);
	}
	*msg_end = '\0';

	return msg;
}

static gboolean mode_setup(StateMachine * sm, gint event)
{
	unsigned total;
	sm_state_name(sm, "mode_setup");
	switch (event) {
	case SM_ENTER:
		callback_mode = MODE_SETUP;
		callbacks.instructions(setup_msg());
		total = is_setup_double()? 2 : 1;
		callbacks.setup(total - build_count_settlements(),
				total - build_count_edges());
		break;
	case SM_RECV:
		/* When a line of text comes in from the network, the
		 * state machine will call us with SM_RECV.
		 */
		if (check_other_players(sm))
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

/*----------------------------------------------------------------------
 * Game is up and running - waiting for our turn
 */

/* Waiting for your turn to come around
 */
static gboolean mode_idle(StateMachine * sm, gint event)
{
	gint num, player_num, x, y, backwards;
	gint they_supply[NO_RESOURCE];
	gint they_receive[NO_RESOURCE];

	sm_state_name(sm, "mode_idle");
	switch (event) {
	case SM_ENTER:
		callback_mode = MODE_WAIT_TURN;
		callbacks.instructions(_("Waiting for your turn"));
		break;
	case SM_RECV:
		if (sm_recv(sm, "setup %d", &backwards)) {
			setup_begin(my_player_num());
			if (backwards)
				sm_push_noenter(sm, mode_wait_resources);
			sm_push(sm, mode_setup);
			return TRUE;
		}
		if (sm_recv(sm, "setup-double")) {
			setup_begin_double(my_player_num());
			sm_push_noenter(sm, mode_wait_resources);
			sm_push(sm, mode_setup);
			return TRUE;
		}
		if (sm_recv(sm, "turn %d", &num)) {
			turn_begin(my_player_num(), num);
			sm_push(sm, mode_turn);
			return TRUE;
		}
		if (sm_recv(sm, "player %d moved-robber %d %d",
			    &player_num, &x, &y)) {
			robber_moved(player_num, x, y);
			return TRUE;
		}
		if (sm_recv(sm, "player %d moved-pirate %d %d",
			    &player_num, &x, &y)) {
			pirate_moved(player_num, x, y);
			return TRUE;
		}
		if (sm_recv
		    (sm,
		     "player %d domestic-trade call supply %R receive %R",
		     &player_num, they_supply, they_receive)) {
			sm_push(sm, mode_domestic_quote);
			callbacks.quote(player_num, they_supply,
					they_receive);
			return TRUE;
		}
		if (check_other_players(sm))
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

/*----------------------------------------------------------------------
 * Nested state machine for robber handling
 */

/* Handle response to move robber
 */
gboolean mode_robber_response(StateMachine * sm, gint event)
{
	gint x, y;

	sm_state_name(sm, "mode_robber_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv(sm, "moved-robber %d %d", &x, &y)) {
			robber_moved(my_player_num(), x, y);
			waiting_for_network(FALSE);
			/* pop back to parent's parent if robber is moved */
			sm_multipop(sm, 2);
			return TRUE;
		}
		if (sm_recv(sm, "moved-pirate %d %d", &x, &y)) {
			pirate_moved(my_player_num(), x, y);
			waiting_for_network(FALSE);
			/* pop back to parent's parent if pirate is moved */
			sm_multipop(sm, 2);
			return TRUE;
		}
		if (check_other_players(sm))
			return TRUE;
		waiting_for_network(FALSE);
		/* pop to parent if move failed */
		sm_pop(sm);
		break;
	}
	return FALSE;
}

/* Get user to place robber
 */
static gboolean mode_robber(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_robber");
	switch (event) {
	case SM_ENTER:
		callback_mode = MODE_ROBBER;
		callbacks.instructions(_("Place the robber"));
		robber_begin_move(my_player_num());
		callbacks.robber();
		break;
	case SM_RECV:
		if (check_other_players(sm))
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

/* We rolled a 7, or played a soldier card - any time now the server
 * is going to tell us to place the robber.  Going into this state as
 * soon as we roll a 7 stops a race condition where the user presses a
 * GUI control in the window between receiving the die roll result and
 * the command to enter robber mode.
 */
static gboolean mode_wait_for_robber(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_wait_for_robber");
	switch (event) {
	case SM_ENTER:
		callbacks.instructions(_("Place the robber"));
		break;
	case SM_RECV:
		if (sm_recv(sm, "you-are-robber")) {
			sm_goto(sm, mode_robber);
			return TRUE;
		}
		if (check_other_players(sm))
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

/*----------------------------------------------------------------------
 * Road building
 */

static gboolean mode_road_building(StateMachine * sm, gint event)
{
	gint build_amount;	/* The amount of available 'roads' */
	sm_state_name(sm, "mode_road_building");
	switch (event) {
	case SM_ENTER:
		callback_mode = MODE_ROAD_BUILD;
		/* Determine the possible amount of road segments */
		build_amount = 0;
		if (road_building_can_build_road())
			build_amount += stock_num_roads();
		if (road_building_can_build_ship())
			build_amount += stock_num_ships();
		if (road_building_can_build_bridge())
			build_amount += stock_num_bridges();
		/* Now determine the amount of segments left to play */
		build_amount = MIN(build_amount, 2 - build_count_edges());
		callbacks.roadbuilding(build_amount);
		switch (build_amount) {
		case 0:
			callbacks.
			    instructions(_
					 ("Finish the road building action."));
			break;
		case 1:
			callbacks.
			    instructions(_("Build one road segment."));
			break;
		case 2:
			callbacks.
			    instructions(_("Build two road segments."));
			break;
		default:
			g_error("Unknown road building amount");
			break;
		};
		break;
	case SM_RECV:
		if (check_other_players(sm))
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

/*----------------------------------------------------------------------
 * Monopoly development card
 */

/* Response to "monopoly"
 */
gboolean mode_monopoly_response(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_monopoly_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv(sm, "OK")) {
			waiting_for_network(FALSE);
			/* pop to parent's parent if it worked */
			sm_multipop(sm, 2);
			return TRUE;
		}
		if (check_other_players(sm))
			return TRUE;
		/* pop to parent if it didn't work */
		sm_pop(sm);
		waiting_for_network(FALSE);
		break;
	}
	return FALSE;
}

static gboolean mode_monopoly(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_monopoly");
	switch (event) {
	case SM_ENTER:
		callback_mode = MODE_MONOPOLY;
		callbacks.monopoly();
		break;
	case SM_RECV:
		if (check_other_players(sm))
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

/*----------------------------------------------------------------------
 * Year of Plenty development card
 */

/* Response to "plenty"
 */
gboolean mode_year_of_plenty_response(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_year_of_plenty_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv(sm, "OK")) {
			waiting_for_network(FALSE);
			/* action is done, go to parent's parent */
			sm_multipop(sm, 2);
			return TRUE;
		}
		if (check_other_players(sm))
			return TRUE;
		waiting_for_network(FALSE);
		/* if it fails, go back to parent */
		sm_pop(sm);
		break;
	}
	return FALSE;
}

static gboolean mode_year_of_plenty(StateMachine * sm, gint event)
{
	gint plenty[NO_RESOURCE];

	sm_state_name(sm, "mode_year_of_plenty");
	switch (event) {
	case SM_RECV:
		if (sm_recv(sm, "plenty %R", plenty)) {
			callback_mode = MODE_PLENTY;
			callbacks.plenty(plenty);
			return TRUE;
		}
		if (check_other_players(sm))
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

/*----------------------------------------------------------------------
 * Nested state machine for handling development card play response
 */

/* Handle response to play develop card
 */
gboolean mode_play_develop_response(StateMachine * sm, gint event)
{
	gint card_idx;
	DevelType card_type;

	sm_state_name(sm, "mode_play_develop_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv
		    (sm, "play-develop %d %D", &card_idx, &card_type)) {
			build_clear();
			waiting_for_network(FALSE);
			develop_played(my_player_num(), card_idx,
				       card_type);
			switch (card_type) {
			case DEVEL_ROAD_BUILDING:
				sm_goto(sm, mode_road_building);
				break;
			case DEVEL_MONOPOLY:
				sm_goto(sm, mode_monopoly);
				break;
			case DEVEL_YEAR_OF_PLENTY:
				sm_goto(sm, mode_year_of_plenty);
				break;
			case DEVEL_SOLDIER:
				sm_goto(sm, mode_wait_for_robber);
				break;
			default:
				sm_pop(sm);
				break;
			}
			return TRUE;
		}
		if (check_other_players(sm))
			return TRUE;
		waiting_for_network(FALSE);
		sm_pop(sm);
		break;
	}
	return FALSE;
}

/*----------------------------------------------------------------------
 * Nested state machine for handling resource card discards.  We enter
 * discard mode whenever any player has to discard resources.
 * 
 * When in discard mode, a section of the GUI changes to list all
 * players who must discard resources.  This is important because if
 * during our turn we roll 7, but have less than 7 resources, we do
 * not have to discard.  The list tells us which players have still
 * not discarded resources.
 */
static gboolean mode_discard(StateMachine * sm, gint event)
{
	gint player_num, discard_num;

	sm_state_name(sm, "mode_discard");
	switch (event) {
	case SM_ENTER:
		if (callback_mode != MODE_DISCARD
		    && callback_mode != MODE_DISCARD_WAIT) {
			previous_mode = callback_mode;
			callback_mode = MODE_DISCARD_WAIT;
		}
		callbacks.discard();
		break;
	case SM_RECV:
		if (sm_recv(sm, "player %d must-discard %d",
			    &player_num, &discard_num)) {
			if (player_num == my_player_num())
				callback_mode = MODE_DISCARD;
			callbacks.discard_add(player_num, discard_num);
			return TRUE;
		}
		if (sm_recv(sm, "discard-done")) {
			callback_mode = previous_mode;
			callbacks.discard_done();
			sm_pop(sm);
			return TRUE;
		}
		if (check_other_players(sm))
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

/*----------------------------------------------------------------------
 * Turn mode processing - before dice have been rolled
 */

/* Handle response to "roll dice"
 */
gboolean mode_roll_response(StateMachine * sm, gint event)
{
	gint die1, die2;

	sm_state_name(sm, "mode_roll_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv(sm, "rolled %d %d", &die1, &die2)) {
			turn_rolled_dice(my_player_num(), die1, die2);
			waiting_for_network(FALSE);
			sm_goto_noenter(sm, mode_turn_rolled);
			if (die1 + die2 == 7) {
				sm_push(sm, mode_wait_for_robber);
			} else
				sm_push(sm, mode_wait_resources);
			return TRUE;
		}
		if (check_other_players(sm))
			return TRUE;
		waiting_for_network(FALSE);
		sm_goto(sm, mode_turn);
		break;
	}
	return FALSE;
}

static gboolean mode_turn(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_turn");
	switch (event) {
	case SM_ENTER:
		callback_mode = MODE_TURN;
		callbacks.instructions(_("It is your turn."));
		callbacks.turn();
		break;
	case SM_RECV:
		if (check_other_players(sm))
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

/*----------------------------------------------------------------------
 * Turn mode processing - after dice have been rolled
 */

/* Handle response to buy development card
 */
gboolean mode_buy_develop_response(StateMachine * sm, gint event)
{
	DevelType card_type;

	sm_state_name(sm, "mode_buy_develop_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv(sm, "bought-develop %D", &card_type)) {
			develop_bought_card(card_type);
			sm_goto(sm, mode_turn_rolled);
			waiting_for_network(FALSE);
			return TRUE;
		}
		if (check_other_players(sm))
			return TRUE;
		sm_goto(sm, mode_turn_rolled);
		waiting_for_network(FALSE);
		break;
	}
	return FALSE;
}

/* Response to "undo"
 */
gboolean mode_undo_response(StateMachine * sm, gint event)
{
	BuildType build_type;
	gint x, y, pos;
	gint sx, sy, spos, dx, dy, dpos;

	sm_state_name(sm, "mode_undo_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv(sm, "remove %B %d %d %d",
			    &build_type, &x, &y, &pos)) {
			build_remove(build_type, x, y, pos);
			waiting_for_network(FALSE);
			sm_pop(sm);
			return TRUE;
		}
		if (sm_recv(sm, "move-back %d %d %d %d %d %d",
			    &sx, &sy, &spos, &dx, &dy, &dpos)) {
			build_move(sx, sy, spos, dx, dy, dpos, TRUE);
			waiting_for_network(FALSE);
			sm_pop(sm);
			return TRUE;
		}
		if (check_other_players(sm))
			return TRUE;
		waiting_for_network(FALSE);
		sm_pop(sm);
		break;
	}
	return FALSE;
}

static gboolean mode_turn_rolled(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_turn_rolled");
	switch (event) {
	case SM_ENTER:
		callback_mode = MODE_TURN;
		callbacks.instructions(_("It is your turn."));
		callbacks.turn();
		break;
	case SM_RECV:
		if (check_other_players(sm))
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

/*----------------------------------------------------------------------
 * Trade processing - all trading done inside a nested state machine
 * to allow trading to be invoked from multiple states.
 */

static gboolean check_trading(StateMachine * sm)
{
	gint player_num, quote_num;
	gint they_supply[NO_RESOURCE];
	gint they_receive[NO_RESOURCE];

	if (!sm_recv_prefix(sm, "player %d ", &player_num))
		return FALSE;

	if (sm_recv(sm, "domestic-quote finish")) {
		callbacks.trade_player_end(player_num);
		return TRUE;
	}
	if (sm_recv(sm, "domestic-quote quote %d supply %R receive %R",
		    &quote_num, they_supply, they_receive)) {
		callbacks.trade_add_quote(player_num, quote_num,
					  they_supply, they_receive);
		return TRUE;
	}
	if (sm_recv(sm, "domestic-quote delete %d", &quote_num)) {
		callbacks.trade_remove_quote(player_num, quote_num);
		return TRUE;
	}

	sm_cancel_prefix(sm);
	return FALSE;
}

/* Handle response to call for domestic trade quotes
 */
gboolean mode_trade_call_response(StateMachine * sm, gint event)
{
	gint we_supply[NO_RESOURCE];
	gint we_receive[NO_RESOURCE];

	sm_state_name(sm, "mode_trade_call_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv(sm, "domestic-trade call supply %R receive %R",
			    we_supply, we_receive)) {
			waiting_for_network(FALSE);
			sm_goto(sm, mode_domestic_trade);
			return TRUE;
		}
		if (check_trading(sm) || check_other_players(sm))
			return TRUE;
		waiting_for_network(FALSE);
		sm_pop(sm);
		break;
	}
	return FALSE;
}

/* Handle response to maritime trade
 */
gboolean mode_trade_maritime_response(StateMachine * sm, gint event)
{
	gint ratio;
	Resource we_supply;
	Resource we_receive;

	Resource no_receive;

	sm_state_name(sm, "mode_trade_maritime_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		/* Handle out-of-resource-cards */
		if (sm_recv(sm, "ERR no-cards %r", &no_receive)) {
			gchar buf_receive[128];

			resource_cards(0, no_receive, buf_receive,
				       sizeof(buf_receive));
			log_message(MSG_TRADE, _("Sorry, %s available.\n"),
				    buf_receive);
			waiting_for_network(FALSE);
			sm_pop(sm);
			return TRUE;
		}
		if (sm_recv(sm, "maritime-trade %d supply %r receive %r",
			    &ratio, &we_supply, &we_receive)) {
			player_maritime_trade(my_player_num(), ratio,
					      we_supply, we_receive);
			waiting_for_network(FALSE);
			sm_pop(sm);
			callbacks.trade_maritime(ratio, we_supply,
						 we_receive);
			return TRUE;
		}
		if (check_trading(sm) || check_other_players(sm))
			return TRUE;
		waiting_for_network(FALSE);
		sm_pop(sm);
		break;
	}
	return FALSE;
}

/* Handle response to call for quotes during domestic trade
 */
gboolean mode_trade_call_again_response(StateMachine * sm, gint event)
{
	gint we_supply[NO_RESOURCE];
	gint we_receive[NO_RESOURCE];

	sm_state_name(sm, "mode_trade_call_again_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv(sm, "domestic-trade call supply %R receive %R",
			    we_supply, we_receive)) {
			waiting_for_network(FALSE);
			sm_pop(sm);
			return TRUE;
		}
		if (check_trading(sm) || check_other_players(sm))
			return TRUE;
		waiting_for_network(FALSE);
		sm_pop(sm);
		break;
	}
	return FALSE;
}

/* Handle response to domestic trade
 */
gboolean mode_trade_domestic_response(StateMachine * sm, gint event)
{
	gint partner_num;
	gint quote_num;
	gint they_supply[NO_RESOURCE];
	gint they_receive[NO_RESOURCE];

	sm_state_name(sm, "mode_trade_domestic_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv
		    (sm,
		     "domestic-trade accept player %d quote %d supply %R receive %R",
		     &partner_num, &quote_num, &they_supply,
		     &they_receive)) {
			player_domestic_trade(my_player_num(), partner_num,
					      they_supply, they_receive);
			waiting_for_network(FALSE);
			callbacks.trade_domestic(partner_num, quote_num,
						 they_receive,
						 they_supply);
			sm_pop(sm);
			return TRUE;
		}
		if (check_trading(sm) || check_other_players(sm))
			return TRUE;
		waiting_for_network(FALSE);
		sm_pop(sm);
		break;
	}
	return FALSE;
}

/* Handle response to domestic trade finish
 */
gboolean mode_domestic_finish_response(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_domestic_finish_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv(sm, "domestic-trade finish")) {
			callback_mode = MODE_TURN;
			waiting_for_network(FALSE);
			/* pop to parent's parent on finish */
			sm_multipop(sm, 2);
			return TRUE;
		}
		if (check_trading(sm) || check_other_players(sm))
			return TRUE;
		waiting_for_network(FALSE);
		/* pop back to parent if finish fails */
		sm_pop(sm);
		break;
	}
	return FALSE;
}

static gboolean mode_domestic_trade(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_domestic_trade");
	switch (event) {
	case SM_ENTER:
		if (callback_mode != MODE_DOMESTIC) {
			callback_mode = MODE_DOMESTIC;
		}
		callbacks.trade();
		break;
	case SM_RECV:
		if (check_trading(sm) || check_other_players(sm))
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

/*----------------------------------------------------------------------
 * Quote processing - all quoting done inside a nested state machine.
 */

static gboolean check_quoting(StateMachine * sm)
{
	gint player_num, partner_num, quote_num;
	gint they_supply[NO_RESOURCE];
	gint they_receive[NO_RESOURCE];

	if (!sm_recv_prefix(sm, "player %d ", &player_num))
		return FALSE;

	if (sm_recv(sm, "domestic-quote finish")) {
		callbacks.quote_player_end(player_num);
		return TRUE;
	}
	if (sm_recv(sm, "domestic-quote quote %d supply %R receive %R",
		    &quote_num, they_supply, they_receive)) {
		callbacks.quote_add(player_num, quote_num, they_supply,
				    they_receive);
		return TRUE;
	}
	if (sm_recv(sm, "domestic-quote delete %d", &quote_num)) {
		callbacks.quote_remove(player_num, quote_num);
		return TRUE;
	}
	if (sm_recv(sm, "domestic-trade call supply %R receive %R",
		    they_supply, they_receive)) {
		callbacks.quote(player_num, they_supply, they_receive);
		return TRUE;
	}
	if (sm_recv
	    (sm,
	     "domestic-trade accept player %d quote %d supply %R receive %R",
	     &partner_num, &quote_num, they_supply, they_receive)) {
		player_domestic_trade(player_num, partner_num, they_supply,
				      they_receive);
		callbacks.quote_trade(player_num, partner_num, quote_num,
				      they_supply, they_receive);
		return TRUE;
	}
	if (sm_recv(sm, "domestic-trade finish")) {
		callback_mode = previous_mode;
		callbacks.quote_end();
		sm_send(sm, "domestic-quote exit\n");
		sm_pop(sm);
		return TRUE;
	}

	sm_cancel_prefix(sm);
	return FALSE;
}

/* Handle response to domestic quote finish
 */
gboolean mode_quote_finish_response(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_quote_finish_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv(sm, "domestic-quote finish")) {
			waiting_for_network(FALSE);
			sm_goto(sm, mode_domestic_monitor);
			return TRUE;
		}
		if (check_quoting(sm) || check_other_players(sm))
			return TRUE;
		waiting_for_network(FALSE);
		sm_goto(sm, mode_domestic_quote);
		callbacks.quote_monitor();
		break;
	}
	return FALSE;
}

/* Handle response to domestic quote submit
 */
gboolean mode_quote_submit_response(StateMachine * sm, gint event)
{
	gint quote_num;
	gint we_supply[NO_RESOURCE];
	gint we_receive[NO_RESOURCE];

	sm_state_name(sm, "mode_quote_submit_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv
		    (sm, "domestic-quote quote %d supply %R receive %R",
		     &quote_num, we_supply, we_receive)) {
			callbacks.quote_add(my_player_num(), quote_num,
					    we_supply, we_receive);
			waiting_for_network(FALSE);
			sm_pop(sm);
			return TRUE;
		}
		if (check_quoting(sm) || check_other_players(sm))
			return TRUE;
		waiting_for_network(FALSE);
		sm_pop(sm);
		break;
	}
	return FALSE;
}

/* Handle response to domestic quote delete
 */
gboolean mode_quote_delete_response(StateMachine * sm, gint event)
{
	gint quote_num;

	sm_state_name(sm, "mode_quote_delete_response");
	switch (event) {
	case SM_ENTER:
		waiting_for_network(TRUE);
		break;
	case SM_RECV:
		if (sm_recv(sm, "domestic-quote delete %d", &quote_num)) {
			callbacks.quote_remove(my_player_num(), quote_num);
			waiting_for_network(FALSE);
			sm_pop(sm);
			return TRUE;
		}
		if (check_quoting(sm) || check_other_players(sm))
			return TRUE;
		waiting_for_network(FALSE);
		sm_pop(sm);
		break;
	}
	return FALSE;
}

/* Another player has called for quotes for domestic trade
 */
static gboolean mode_domestic_quote(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_domestic_quote");
	switch (event) {
	case SM_ENTER:
		if (callback_mode != MODE_QUOTE) {
			previous_mode = callback_mode;
			callback_mode = MODE_QUOTE;
			callbacks.quote_start();
		}
		break;
	case SM_RECV:
		if (check_quoting(sm) || check_other_players(sm))
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

/* We have rejected domestic trade, now just monitor
 */
static gboolean mode_domestic_monitor(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_domestic_monitor");
	switch (event) {
	case SM_RECV:
		if (check_quoting(sm) || check_other_players(sm))
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

/*----------------------------------------------------------------------
 * The game is over
 */

static gboolean mode_game_over(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_game_over");
	switch (event) {
	case SM_ENTER:
		callback_mode = MODE_GAME_OVER;
		callbacks.instructions(_("The game is over."));
		break;
	case SM_RECV:
		if (check_other_players(sm))
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

static gboolean mode_recovery_wait_start_response(StateMachine * sm,
						  gint event)
{
	sm_state_name(sm, "mode_recovery_wait_start_response");
	switch (event) {
	case SM_ENTER:
		sm_send(sm, "start\n");
		break;
	case SM_RECV:
		if (sm_recv(sm, "OK")) {
			sm_pop(sm);
			return TRUE;
		}
		break;
	default:
		break;
	}
	return FALSE;
}

static void recover_from_disconnect(StateMachine * sm,
				    struct recovery_info_t *rinfo)
{
	StateFunc modeturn;
	GList *next;

	if (rinfo->turnnum > 0)
		turn_begin(rinfo->playerturn, rinfo->turnnum);
	if (rinfo->rolled_dice) {
		turn_rolled_dice(rinfo->playerturn, rinfo->die1,
				 rinfo->die2);
	} else if (rinfo->die1 + rinfo->die2 > 1) {
		callbacks.rolled_dice(rinfo->die1, rinfo->die2,
				      rinfo->playerturn);
	}

	if (rinfo->rolled_dice)
		modeturn = mode_turn_rolled;
	else
		modeturn = mode_turn;

	if (rinfo->played_develop || rinfo->bought_develop) {
		develop_reset_have_played_bought(rinfo->played_develop,
						 rinfo->bought_develop);
	}

	if (strcmp(rinfo->prevstate, "PREGAME") == 0) {
		sm_goto(sm, mode_idle);
	} else if (strcmp(rinfo->prevstate, "IDLE") == 0) {
		sm_goto(sm, mode_idle);
	} else if (strcmp(rinfo->prevstate, "SETUP") == 0 ||
		   strcmp(rinfo->prevstate, "SETUPDOUBLE") == 0) {
		if (strcmp(rinfo->prevstate, "SETUP") == 0) {
			setup_begin(my_player_num());
		} else {
			setup_begin_double(my_player_num());
		}
		sm_goto_noenter(sm, mode_idle);
		sm_push(sm, mode_setup);
	} else if (strcmp(rinfo->prevstate, "TURN") == 0) {
		sm_goto_noenter(sm, mode_idle);
		sm_push(sm, modeturn);
	} else if (strcmp(rinfo->prevstate, "YOUAREROBBER") == 0) {
		sm_goto_noenter(sm, mode_idle);
		sm_push_noenter(sm, modeturn);
		sm_push(sm, mode_robber);
	} else if (strcmp(rinfo->prevstate, "DISCARD") == 0) {
		sm_goto_noenter(sm, mode_idle);
		if (my_player_num() == rinfo->playerturn) {
			sm_push_noenter(sm, mode_turn_rolled);
			sm_push_noenter(sm, mode_wait_for_robber);
		}
		sm_push(sm, mode_discard);
	} else if (strcmp(rinfo->prevstate, "MONOPOLY") == 0) {
		sm_goto_noenter(sm, mode_idle);
		sm_push_noenter(sm, modeturn);
		callback_mode = MODE_TURN;
		sm_push(sm, mode_monopoly);
	} else if (strcmp(rinfo->prevstate, "PLENTY") == 0) {
		sm_goto_noenter(sm, mode_idle);
		sm_push_noenter(sm, modeturn);
		callback_mode = MODE_TURN;
		sm_push(sm, mode_year_of_plenty);
	} else if (strcmp(rinfo->prevstate, "GOLD") == 0) {
		sm_goto_noenter(sm, mode_idle);
		sm_push_noenter(sm, modeturn);
		callback_mode = MODE_TURN;
		sm_push(sm, mode_choose_gold);
	} else if (strcmp(rinfo->prevstate, "ROADBUILDING") == 0) {
		sm_goto_noenter(sm, mode_idle);
		sm_push_noenter(sm, modeturn);
		callback_mode = MODE_TURN;
		sm_push(sm, mode_road_building);
	}

	if (rinfo->build_list) {
		for (next = rinfo->build_list; next != NULL;
		     next = g_list_next(next)) {
			BuildRec *build = (BuildRec *) next->data;
			build_add(build->type, build->x, build->y,
				  build->pos, FALSE);
		}
		rinfo->build_list = buildrec_free(rinfo->build_list);
	}

	/* tell the server we have finished reconnecting and wait for
	   its ok response */
	sm_push(sm, mode_recovery_wait_start_response);
}

/*----------------------------------------------------------------------
 * Nested state machine for handling resource card choice.  We enter
 * gold-choose mode whenever any player has to choose resources.
 * 
 * When in gold-choose mode, as in discard mode, a section of the GUI
 * changes to list all players who must choose resources.  This is
 * important because if during our turn we do not receive gold, but others
 * do, the list tells us which players have still not chosen resources.
 * Only the top player in the list can actually choose, the rest is waiting
 * for their turn.
 */
static gboolean mode_choose_gold(StateMachine * sm, gint event)
{
	gint resource_list[NO_RESOURCE], bank[NO_RESOURCE];
	gint player_num, gold_num;

	sm_state_name(sm, "mode_choose_gold");
	switch (event) {
	case SM_ENTER:
		if (callback_mode != MODE_GOLD
		    && callback_mode != MODE_GOLD_WAIT) {
			previous_mode = callback_mode;
			callback_mode = MODE_GOLD_WAIT;
		}
		callbacks.gold();
		break;
	case SM_RECV:
		if (sm_recv(sm, "player %d prepare-gold %d", &player_num,
			    &gold_num)) {
			callbacks.gold_add(player_num, gold_num);
			return TRUE;
		}
		if (sm_recv(sm, "choose-gold %d %R", &gold_num, &bank)) {
			callback_mode = MODE_GOLD;
			callbacks.gold_choose(gold_num, bank);
			return TRUE;
		}
		if (sm_recv(sm, "player %d receive-gold %R",
			    &player_num, resource_list)) {
			player_resource_action(player_num,
					       _("%s takes %s.\n"),
					       resource_list, 1);
			callbacks.gold_remove(player_num, resource_list);
			return TRUE;
		}
		if (sm_recv(sm, "done-resources")) {
			callback_mode = previous_mode;
			callbacks.gold_done();
			sm_pop(sm);
			return TRUE;
		}
		if (check_other_players(sm))
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

static gboolean mode_wait_resources(StateMachine * sm, gint event)
{
	sm_state_name(sm, "mode_wait_resources");
	switch (event) {
	case SM_RECV:
		if (sm_recv(sm, "done-resources")) {
			sm_pop(sm);
			return TRUE;
		}
		if (check_other_players(sm))
			return TRUE;
		break;
	}
	return FALSE;
}

gboolean can_trade_domestic(void)
{
	gint idx;
	/* Check if we can do domestic trading */
	if (!game_params->domestic_trade)
		return FALSE;
	for (idx = 0; idx < NO_RESOURCE; idx++)
		if (resource_asset(idx) > 0)
			return TRUE;
	return FALSE;
}

gboolean can_trade_maritime(void)
{
	MaritimeInfo info;
	gint idx;
	gboolean can_trade;
	/* We are not allowed to trade before we have rolled the dice,
	 * or after we have done built a settlement / city, or after
	 * buying a development card.  */
	if (!have_rolled_dice()
	    || (game_params->strict_trade
		&& (have_built() || have_bought_develop())))
		return FALSE;
	can_trade = FALSE;
	/* Check if we can do a maritime trade */
	map_maritime_info(map, &info, my_player_num());
	for (idx = 0; idx < NO_RESOURCE; idx++)
		if (info.specific_resource[idx]
		    && resource_asset(idx) >= 2) {
			can_trade = TRUE;
			break;
		} else if (info.any_resource && resource_asset(idx) >= 3) {
			can_trade = TRUE;
			break;
		} else if (resource_asset(idx) >= 4) {
			can_trade = TRUE;
			break;
		}
	return can_trade;
}
