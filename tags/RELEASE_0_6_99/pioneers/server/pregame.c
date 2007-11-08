/*
 * Gnocatan: a fun game.
 * (C) 1999 the Free Software Foundation
 *
 * Author: Dave Cole.
 *
 * Implementation of the excellent Settlers of Catan board game.  Go
 * buy a copy.
 */
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>

#include <glib.h>

#include "game.h"
#include "cards.h"
#include "map.h"
#include "buildrec.h"
#include "network.h"
#include "cost.h"
#include "server.h"

static gboolean mode_setup(Player *player, gint event);

static void build_add(Player *player, BuildType type, gint x, gint y, gint pos)
{
	StateMachine *sm = player->sm;
	Game *game = player->game;
	Map *map = game->params->map;
	gint num;
	gint num_allowed;

	num_allowed = game->double_setup ? 2 : 1;

	/* Add settlement/road
	 */
	num = buildrec_count_type(player->build_list, type);
	if (num == num_allowed) {
		sm_send(sm, "ERR too-many\n");
		return;
	}

	if (type == BUILD_ROAD) {
		/* Building a road, make sure it is next to a
		 * settlement/road
		 */
		if (!buildrec_can_setup_road(player->build_list, map,
					     map_edge(map, x, y, pos),
					     game->double_setup)) {
			sm_send(sm, "ERR bad-pos\n");
			return;
		}
		edge_add(player, BUILD_ROAD, x, y, pos, FALSE);
		return;
	}

	if (type == BUILD_BRIDGE) {
		/* Building a bridge, make sure it is next to a
		 * settlement/road
		 */
		if (!buildrec_can_setup_bridge(player->build_list, map,
					       map_edge(map, x, y, pos),
					       game->double_setup)) {
			sm_send(sm, "ERR bad-pos\n");
			return;
		}
		edge_add(player, BUILD_BRIDGE, x, y, pos, FALSE);
		return;
	}

	if (type == BUILD_SHIP) {
		/* Building a ship, make sure it is next to a
		 * settlement/ship
		 */
		if (!buildrec_can_setup_ship(player->build_list, map,
					     map_edge(map, x, y, pos),
					     game->double_setup)) {
			sm_send(sm, "ERR bad-pos\n");
			return;
		}
		edge_add(player, BUILD_SHIP, x, y, pos, FALSE);
		return;
	}

	if (type != BUILD_SETTLEMENT) {
		sm_send(sm, "ERR expected-road-or-settlement\n");
		return;
	}
	/* Build the settlement
	 */
	if (!buildrec_can_setup_settlement(player->build_list, map,
					   map_node(map, x, y, pos),
					   game->double_setup)) {
		sm_send(sm, "ERR bad-pos\n");
		return;
	}
	node_add(player, BUILD_SETTLEMENT, x, y, pos, FALSE);
}

static void build_remove(Player *player, BuildType type, gint x, gint y, gint pos)
{
	/* Remove the settlement/road we just built
	 */
	if (!perform_undo(player, type, x, y, pos))
		sm_send(player->sm, "ERR bad-pos\n");
}

static void start_setup_player(Player *player)
{
	Game *game = player->game;

	player->build_list = buildrec_free(player->build_list);

	if (game->double_setup)
		player_broadcast(player, PB_RESPOND, "setup-double\n");
	else
		player_broadcast(player, PB_RESPOND, "setup\n");

	sm_goto(player->sm, (StateFunc)mode_setup);
}

static void allocate_resources(Player *player, BuildRec *rec)
{
	Game *game = player->game;
	Map *map = game->params->map;
	Node *node;
	gint idx;

	node = map_node(map, rec->x, rec->y, rec->pos);

	resource_start(game);
	for (idx = 0; idx < numElem(node->hexes); idx++) {
		Hex *hex = node->hexes[idx];
		if (hex->roll > 0)
			player->assets[hex->terrain]++;
	}
	resource_end(game, "receives", 1);
}

/* Player tried to finish setup mode
 */
static void try_setup_done(Player *player)
{
	StateMachine *sm = player->sm;
	Game *game = player->game;
	Map *map = game->params->map;
	gint num_allowed;

	num_allowed = game->double_setup ? 2 : 1;

	/* Make sure we have built the right number of
	 * settlements/roads
	 */
	if (buildrec_count_edges(player->build_list) != num_allowed
	    || buildrec_count_type(player->build_list, BUILD_SETTLEMENT) != num_allowed) {
		sm_send(sm, "ERR expected-build-or-remove\n");
		return;
	}
	/* We have the right number, now make sure that all roads are
	 * connected to buildings and vice-versa
	 */
	if (!buildrec_is_valid(player->build_list, map, player->num)) {
		sm_send(sm, "ERR unconnected\n");
		return;
	}
	/* Player has finished setup phase - give resources for second
	 * settlement
	 */
	sm_send(sm, "OK\n");
	sm_goto(sm, (StateFunc)mode_idle);

	if (game->double_setup)
		allocate_resources(player, buildrec_get(player->build_list, BUILD_SETTLEMENT, 1));
	else if (game->reverse_setup)
		allocate_resources(player, buildrec_get(player->build_list, BUILD_SETTLEMENT, 0));

	if (game->reverse_setup) {
		/* Going back for second setup phase
		 */
		game->double_setup = FALSE;
		game->setup_player = g_list_previous(game->setup_player);
		if (game->setup_player != NULL) {
			start_setup_player(game->setup_player->data);
			if (!game->setup_player_name)
				g_free(game->setup_player_name);
			game->setup_player_name = g_strdup(((Player *)game->setup_player->data)->name);
		}
		else {
			/* Start the game!!!
			 */
			turn_next_player(game);
		}
	} else {
		/* First setup phase
		 */
		game->setup_player = g_list_next(game->setup_player);
		if (!game->setup_player_name)
			g_free(game->setup_player_name);
		game->setup_player_name = g_strdup(((Player *)game->setup_player->data)->name);
		/* Last player gets double setup
		 */
		game->double_setup = (g_list_next(game->setup_player) == NULL);
		/* Prepare to go backwards next time
		 */
		game->reverse_setup = game->double_setup;
		start_setup_player(game->setup_player->data);
	}
}

/* Player must place exactly one settlement and one road which connect
 * to each other.  If last player, then perform a double setup.
 */
gboolean mode_setup(Player *player, gint event)
{
	StateMachine *sm = player->sm;
	BuildType type;
	gint x, y, pos;

        sm_state_name(sm, "mode_setup");
	if (event != SM_RECV)
		return FALSE;

	if (sm_recv(sm, "done")) {
		try_setup_done(player);
		return TRUE;
	}
	if (sm_recv(sm, "build %B %d %d %d", &type, &x, &y, &pos)) {
		build_add(player, type, x, y, pos);
		return TRUE;
	}
	if (sm_recv(sm, "remove %B %d %d %d", &type, &x, &y, &pos)) {
		build_remove(player, type, x, y, pos);
		return TRUE;
	}

	return FALSE;
}

static void try_start_game(Game *game)
{
	GList *list;
	int num;
	int numturn;

	num = 0;
	numturn = 0;
	for (list = player_first_real(game);
	     list != NULL; list = player_next_real(list)) {
		Player *player = list->data;

		if (sm_current(player->sm) == (StateFunc)mode_idle)
			num++;

		if (sm_current(player->sm) == (StateFunc)mode_turn ||
		    sm_current(player->sm)
                     == (StateFunc)mode_discard_resources ||
		    sm_current(player->sm)
                     == (StateFunc)mode_wait_others_place_robber ||
		    sm_current(player->sm)
                     == (StateFunc)mode_discard_resources_place_robber ||
		    sm_current(player->sm)
                     == (StateFunc)mode_place_robber ||
		    sm_current(player->sm)
                     == (StateFunc)mode_road_building ||
		    sm_current(player->sm)
                     == (StateFunc)mode_monopoly ||
		    sm_current(player->sm)
                     == (StateFunc)mode_plenty_resources)
		{
			/* looks like this player got disconnected and
			   now it's his turn. */
			num++;
			numturn++;
		}
	}
	if (num != game->params->num_players)
		return;

	if (numturn > 0)
	{
		/* someone got disconnected. Now everyone's back. Let's
		   continue the game... */

		return;
	}

	/* All players have connected, and are ready to begin
	 */
	meta_start_game();
	game->is_game_full = TRUE;
	game->setup_player = game->player_list;
	if (!game->setup_player_name)
		g_free(game->setup_player_name);
	game->setup_player_name = g_strdup(((Player *)game->setup_player->data)->name);
	game->double_setup = game->reverse_setup = FALSE;

	start_setup_player(game->setup_player->data);
}

/* Send the player list to the client
 */
static void send_player_list(Player *player)
{
	StateMachine *sm = player->sm;
	Game *game = player->game;
	GList *list;

	sm_send(sm, "players follow\n");
	for (list = player_first_real(game);
	     list != NULL; list = player_next_real(list)) {
		Player *scan = list->data;

		if (list->data == player)
			continue;

		if (scan->name == NULL)
			sm_send(sm, "player %d is anonymous\n", scan->num);
		else
			sm_send(sm, "player %d is %s\n", scan->num, scan->name);
	}
	sm_send(sm, ".\n");
}

/* Send the game parameters to the player
 */
static void send_game_line(Player *player, gchar *str)
{
	sm_send(player->sm, "%s\n", str);
}

gboolean send_gameinfo(Map *map, Hex *hex, StateMachine *sm)
{
	gint i;
	for (i = 0; i < 2; i++)
	{
		if (hex->nodes[i]->owner >= 0)
		{
			switch (hex->nodes[i]->type)
			{
			case BUILD_SETTLEMENT:
				sm_send(sm, "S%d,%d,%d,%d\n", hex->x,
					hex->y, i,
					hex->nodes[i]->owner);
				break;
			case BUILD_CITY:
				sm_send(sm, "C%d,%d,%d,%d\n", hex->x,
					hex->y, i,
					hex->nodes[i]->owner);
				break;
			default:
				;
			}
		}
	}

	for (i = 0; i < 3; i++)
	{
		if (hex->edges[i]->owner >= 0)
		{
			switch (hex->edges[i]->type)
			{
			case BUILD_ROAD:
				sm_send(sm, "R%d,%d,%d,%d\n", hex->x,
					hex->y, i,
					hex->edges[i]->owner);
				break;
			case BUILD_SHIP:
				sm_send(sm, "SH%d,%d,%d,%d\n", hex->x,
					hex->y, i,
					hex->nodes[i]->owner);
				break;
			case BUILD_BRIDGE:
				sm_send(sm, "B%d,%d,%d,%d\n", hex->x,
					hex->y, i,
					hex->nodes[i]->owner);
				break;
			default:
				;
			}
		}
	}

	if (hex->robber)
	{
		sm_send(sm, "RO%d,%d\n", hex->x, hex->y);
	}

	return FALSE;
}

/* Player setup phase
 */
gboolean mode_pre_game(Player *player, gint event)
{
	StateMachine *sm = player->sm;
	Game *game = player->game;
	Map *map = game->params->map;
	StateFunc state;
	gchar prevstate[40];
	gint i;
	GList *next;
	gint longestroadpnum = -1;
	gint largestarmypnum = -1;
	if (game->longest_road) {
		longestroadpnum = game->longest_road->num;
	}
	if (game->largest_army) {
		largestarmypnum = game->largest_army->num;
	}

        sm_state_name(sm, "mode_pre_game");
	switch (event) {
	case SM_ENTER:
		sm_send(sm, "player %d of %d, welcome to gnocatan server %s\n",
			player->num, game->params->num_players, VERSION);
		break;

	case SM_RECV:
		if (sm_recv(sm, "players")) {
			send_player_list(player);
			return TRUE;
		}
		if (sm_recv(sm, "game")) {
			params_write_lines(game->params, (WriteLineFunc)send_game_line, player);
			return TRUE;
		}
		if (sm_recv(sm, "gameinfo")) {
			sm_send(sm, "gameinfo\n");
			map_traverse(map, (HexFunc)send_gameinfo, sm);
			sm_send(sm, ".\n");

			/* now, send state info */
			sm_send(sm, "turn num %d\n", game->curr_turn);
			if (game->curr_player)
			{
				Player *playerturn
				 = player_by_name(game, game->curr_player);
				sm_send(sm, "player turn: %d\n",
				        playerturn->num);
			}
			if (game->rolled_dice)
			{
				sm_send(sm, "dice rolled: %d %d\n",
				        game->die1, game->die2);
			}
			else if (game->die1 + game->die2 > 1)
			{
				sm_send(sm, "dice value: %d %d\n",
				        game->die1, game->die2);
			}
			if (game->played_develop)
			{
				sm_send(sm, "played develop\n");
			}
			if (game->bought_develop)
			{
				sm_send(sm, "bought develop\n");
			}
			if (player->disconnected)
			{
				sm_send(sm, "player disconnected\n");
			}
			state = player->disconnected
			      ? sm_previous(player->sm)
			      : sm_current(player->sm);
			if (state == (StateFunc)mode_idle)
				strcpy(prevstate, "IDLE");
			else if (state == (StateFunc)mode_turn) {
				strcpy(prevstate, "TURN");
				if (game->rolled_dice && game->die1 + game->die2 == 7) {
					/* search for the player who is the robber */
					for (next = game->player_list;
					     next != NULL; next = g_list_next(next)) {
						Player *p = (Player *)next->data;
						state = p->disconnected
						      ? sm_previous(p->sm) : sm_current(p->sm);
						if (state == (StateFunc)mode_place_robber) {
							sprintf(prevstate, "ISROBBER %d", p->num);
							break;
						}
					}
				}
			}
			else if (state == (StateFunc)mode_discard_resources ||
			         state == (StateFunc)mode_discard_resources_place_robber)
			{
				sprintf(prevstate, "DISCARD %d",
					player->discard_num);
			}
			else if (state == (StateFunc)mode_wait_others_place_robber)
				strcpy(prevstate, "WAIT_OTHERS_PLACE_ROBBER");
			else if (state == (StateFunc)mode_place_robber)
				strcpy(prevstate, "YOUAREROBBER");
			else if (state == (StateFunc)mode_road_building)
				strcpy(prevstate, "ROADBUILDING");
			else if (state == (StateFunc)mode_monopoly)
				strcpy(prevstate, "MONOPOLY");
			else if (state == (StateFunc)mode_plenty_resources)
				sprintf(prevstate, "PLENTY");
			else if (state == (StateFunc)mode_setup) {
				/* reconstruct the setup_player list */
				/* search for the player whose setup it
				   is */
				for (next = game->player_list;
				     next != NULL; next = g_list_next(next)) {
					if (strcmp(((Player *)next->data)->name, game->setup_player_name) == 0) {
						game->setup_player = next;
						break;
					}
				}

				if (game->double_setup) {
					sprintf(prevstate, "SETUPDOUBLE");
				}
				else {
					sprintf(prevstate, "SETUP");
				}
			}
			else
				strcpy(prevstate, "PREGAME");
			if (state == (StateFunc)mode_plenty_resources) {
				sm_send(sm, "state PLENTY %R\n", game->bank_deck);
			}
			else
			{
				sm_send(sm, "state %s\n", prevstate);
			}
			/* send player info about what he has:
                            resources, dev cards, roads, # roads,
                            # bridges, # ships, # settles, # cities,
                            # soldiers, road len, dev points,
			    who has longest road/army */
			sm_send(sm, "playerinfo: resources: %R\n", player->assets);
			sm_send(sm, "playerinfo: numdevcards: %d\n", player->devel->num_cards);
			for (i = 0; i < player->devel->num_cards; i++)
			{
				sm_send(sm, "playerinfo: devcard: %d %d\n", (gint)player->devel->cards[i].type, player->devel->cards[i].turn_bought);
			}
			sm_send(sm, "playerinfo: %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
				player->num_roads, player->num_bridges,
				player->num_ships, player->num_settlements,
				player->num_cities, player->num_soldiers,
				player->road_len, player->chapel_played,
				player->univ_played, player->gov_played,
				player->libr_played, player->market_played,
			        (player->num == longestroadpnum),
			        (player->num == largestarmypnum));
			/* send info about other players */
			for (next = game->player_list;
			     next != NULL; next = g_list_next(next))
			{
				Player *p = (Player *)next->data;
				if (p->num != player->num)
				{
					gint numassets = 0;
					for (i = 0; i < NO_RESOURCE; i++) {
						numassets += p->assets[i];
					}
					sm_send(sm, "otherplayerinfo: %d %d %d %d %d %d %d %d %d %d %d\n",
					        p->num, numassets, p->devel->num_cards,
					        p->num_soldiers, p->chapel_played,
						p->univ_played, p->gov_played,
						p->libr_played, p->market_played,
					        (p->num == longestroadpnum),
					        (p->num == largestarmypnum));
				}
			}

			/* send discard info for all players */
			for (next = game->player_list;
			     next != NULL; next = g_list_next(next))
			{
				Player *p = (Player *)next->data;
				if (p->discard_num > 0) {
					sm_send(sm, "player %d must-discard %d\n",
					        p->num, p->discard_num);
				}
			}

			/* send build info for the current player
			   - what builds the player was in the process
			     of building when he disconnected */
			for (next = player->build_list;
			     next != NULL; next = g_list_next(next))
			{
				gint cost[NO_RESOURCE] = { 0, 0, 0, 0, 0 };
				BuildRec *build = (BuildRec *)next->data;
				if (build->cost) {
					memcpy(cost, build->cost, sizeof(gint)*NO_RESOURCE);
				}
				sm_send(sm, "buildinfo: %B %d %d %d %d %R\n",
					build->type, build->prev_status, build->x, build->y, build->pos, cost);
			}

			sm_send(sm, "end\n");
			return TRUE;
		}
		if (sm_recv(sm, "start")) {
			sm_send(sm, "OK\n");
			if (player->disconnected)
			{
				player->disconnected = FALSE;
				sm_pop(sm);
			}
			else
			{
				sm_goto(sm, (StateFunc)mode_idle);
			}
			try_start_game(game);
			return TRUE;
		}
		break;
	default:
		break;
	}
	return FALSE;
}