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

#include <glib.h>

#include "game.h"
#include "cards.h"
#include "map.h"
#include "buildrec.h"
#include "network.h"
#include "cost.h"
#include "log.h"
#include "server.h"

void check_longest_road(Game *game, gboolean can_cut)
{
	Map *map = game->params->map;
	gint road_len[MAX_PLAYERS]; /* work out the longest road */
	GList *list;
	Player *new_longest;
	gint num_have_longest;
	gboolean was_cut;	/* was the longest road cut? */

	map_longest_road(map, road_len, game->params->num_players);

	new_longest = NULL;
	was_cut = FALSE;
	num_have_longest = 0;
	for (list = player_first_real(game);
	     list != NULL; list = player_next_real(list)) {
		Player *player = list->data;

#ifdef DEBUG_LONGEST
		log_message( MSG_INFO, "%s", player_name(player));
		if (game->longest_road == player)
			log_message( MSG_INFO, "(current)");
		log_message( MSG_INFO, "=%d", road_len[player->num]);
		if (player->road_len != road_len[player->num])
			log_message( MSG_INFO, "(was %d)", player->road_len);
		log_message( MSG_INFO, " ");
#endif

		/* only see if the ongest road was cut if can_cut is true.
		 * If it is false, no building was built, and the road may
		 * have become shorter because a ship moved away. */
		if (can_cut
		    && player->road_len > road_len[player->num]
		    && game->longest_road == player)
			/* My longest road has been cut, I Must
			 * re-earn longest road
			 */
			was_cut = TRUE;
		player->road_len = road_len[player->num];

		/* Only 5 or more road segments can earn longest road
		 */
		if (road_len[player->num] < 5)
			continue;

		if (new_longest == NULL
			|| road_len[player->num] > road_len[new_longest->num]) {
			new_longest = player;
			num_have_longest = 1;
		} else if (road_len[player->num] == road_len[new_longest->num])
			num_have_longest++;
	}

	if (new_longest == NULL) {
		if (game->longest_road != NULL) {
			/* Ouch! Lost longest road
			 */
#ifdef DEBUG_LONGEST
			log_message( MSG_INFO, "lost longest road\n");
#endif
			player_broadcast(player_none(game), PB_ALL, "longest-road\n");
			game->longest_road = NULL;
			return;
		}
#ifdef DEBUG_LONGEST
		log_message( MSG_INFO, "no longest road\n");
#endif
		return;
	}

	/* Handle multiple longest road owners - when there is more
	 * than one player with the longest road, we never award longest
	 * road, we can only take it away.
	 */
	if (num_have_longest > 1) {
		if (game->longest_road == NULL) {
			/* No one had longest road, no one gets it
			 */
#ifdef DEBUG_LONGEST
			log_message( MSG_INFO, "multiple longest road; no one gets it\n");
#endif
			return;
		}
		/* The current longest road owner only loses the
		 * longest road if he no longer has the longest road,
		 * or his road was cut.
		 */
		if (game->longest_road->road_len < new_longest->road_len
		    || was_cut) {
			player_broadcast(player_none(game), PB_ALL, "longest-road\n");
			game->longest_road = NULL;
#ifdef DEBUG_LONGEST
			log_message( MSG_INFO, "multiple longest road; no one gets it\n");
#endif
			return;
		}
#ifdef DEBUG_LONGEST
		log_message( MSG_INFO, "multiple longest road; no change in owner\n");
#endif
		return;
	}

	/* Now change the longest road owner if necessary
	 */
	if (game->longest_road == NULL) {
		game->longest_road = new_longest;
		player_broadcast(game->longest_road, PB_ALL, "longest-road\n");
#ifdef DEBUG_LONGEST
		log_message( MSG_INFO, "%s has longest road\n", player_name(new_longest));
#endif
		return;
	}
	/* Did longest road owner change?
	 */
	if (new_longest != game->longest_road
	    && road_len[new_longest->num] > road_len[game->longest_road->num]) {
		game->longest_road = new_longest;
		player_broadcast(game->longest_road, PB_ALL, "longest-road\n");
#ifdef DEBUG_LONGEST
		log_message( MSG_INFO, "%s has longest road\n", player_name(new_longest));
#endif
	}
#ifdef DEBUG_LONGEST
	log_message( MSG_INFO, "no change\n");
#endif
}

/* build something on a node */
void node_add(Player *player,
	      BuildType type, int x, int y, int pos, gboolean paid_for)
{
	Game *game = player->game;
	Map *map = game->params->map;
	Node *node = map_node(map, x, y, pos);
	BuildRec *rec;

	/* administrate the built number of structures */
	if (type == BUILD_SETTLEMENT)
		player->num_settlements++;
	else {
		if (node->type == BUILD_SETTLEMENT)
			player->num_settlements--;
		player->num_cities++;
	}

	/* fill the backup struct */
	rec = g_malloc0(sizeof(*rec));
	rec->type = type;
	rec->prev_status = node->type;
	rec->x = x;
	rec->y = y;
	rec->pos = pos;
	rec->longest_road = game->longest_road ? game->longest_road->num : -1;

	/* compute the cost */
	if (paid_for) {
		if (type == BUILD_CITY)
			if (node->type == BUILD_SETTLEMENT)
				rec->cost = cost_upgrade_settlement();
			else
				rec->cost = cost_city();
		else
			rec->cost = cost_settlement();

		resource_spend(player, rec->cost);
	} else
		rec->cost = NULL;

	/* put the struct in the undo list */
	player->build_list = g_list_append(player->build_list, rec);

	/* update the node information */
	node->owner = player->num;
	node->type = type;

	/* tell everybody about it */
	player_broadcast(player, PB_RESPOND,
			   "built %B %d %d %d\n", type, x, y, pos);

	/* see if the longest road was cut */
	check_longest_road(game, TRUE);
}

/* build something on an edge */
void edge_add(Player *player, BuildType type, int x, int y, int pos, gboolean paid_for)
{
	Game *game = player->game;
	Map *map = game->params->map;
	Edge *edge = map_edge(map, x, y, pos);
	BuildRec *rec;

	/* fill the undo struct */
	rec = g_malloc0(sizeof(*rec));
	rec->type = type;
	rec->x = x;
	rec->y = y;
	rec->pos = pos;
	rec->longest_road = game->longest_road ? game->longest_road->num : -1;

	/* take the money if needed */
	if (paid_for) {
		switch (type) {
		case BUILD_ROAD: rec->cost = cost_road(); break;
		case BUILD_SHIP: rec->cost = cost_ship(); break;
		case BUILD_BRIDGE: rec->cost = cost_bridge(); break;
		case BUILD_MOVE_SHIP:
		case BUILD_SETTLEMENT:
		case BUILD_CITY:
		case BUILD_NONE:
			log_message( MSG_ERROR, "In buildutils.c::edge_add() - Invalid build type.\n" );
			break;
		}
		resource_spend(player, rec->cost);
	} else
		rec->cost = NULL;

	/* put the struct in the undo list */
	player->build_list = g_list_append(player->build_list, rec);

	/* update the pieces */
	switch(type)
	{
		case BUILD_ROAD: player->num_roads++; break;
		case BUILD_BRIDGE: player->num_bridges++; break;
		case BUILD_SHIP: player->num_ships++; break;
		case BUILD_MOVE_SHIP:
		case BUILD_SETTLEMENT:
		case BUILD_CITY:
		case BUILD_NONE:
			log_message( MSG_ERROR, "In buildutils.c::edge_add() - Invalid build type.\n" );
			break;
	}

	/* update the board */
	edge->owner = player->num;
	edge->type = type;
	player_broadcast(player, PB_RESPOND,
			 "built %B %d %d %d\n", type, x, y, pos);

	/* perhaps the longest road changed owner */
	check_longest_road(game, FALSE);
}

/* undo a build action */
gboolean perform_undo(Player *player)
{
	Game *game = player->game;
	Map *map = game->params->map;
	GList *list;
	BuildRec *rec;
	Hex *hex;
	int longest_road;

	/* If the player hasn't built anything, the undo fails */
	if (player->build_list == NULL) return FALSE;

	/* Fill some convenience variables */
	list = g_list_last (player->build_list);
	rec = list->data;
	hex = map_hex(map, rec->x, rec->y);

	/* Remove the entry from the list (doesn't remove the data itself) */
	player->build_list = g_list_remove_link(player->build_list, list);
	g_list_free_1(list);

	/* Do structure-specific things */
	switch (rec->type) {
	case BUILD_NONE:
		g_error("BUILD_NONE in perform_undo()");
		break;
	case BUILD_ROAD:
		player->num_roads--;

		player_broadcast(player, PB_RESPOND, "remove %B %d %d %d\n",
				 BUILD_ROAD, rec->x, rec->y, rec->pos);
		hex->edges[rec->pos]->owner = -1;
		hex->edges[rec->pos]->type = BUILD_NONE;
		break;
	case BUILD_BRIDGE:
		player->num_bridges--;

		player_broadcast(player, PB_RESPOND, "remove %B %d %d %d\n",
				 BUILD_BRIDGE, rec->x, rec->y, rec->pos);
		hex->edges[rec->pos]->owner = -1;
		hex->edges[rec->pos]->type = BUILD_NONE;
		break;
	case BUILD_SHIP:
		player->num_ships--;

		player_broadcast(player, PB_RESPOND, "remove %B %d %d %d\n",
				 BUILD_SHIP, rec->x, rec->y, rec->pos);
		hex->edges[rec->pos]->owner = -1;
		hex->edges[rec->pos]->type = BUILD_NONE;
		break;
	case BUILD_CITY:
		player->num_cities--;
		player->num_settlements++;

		player_broadcast(player, PB_RESPOND, "remove %B %d %d %d\n",
				 BUILD_CITY, rec->x, rec->y, rec->pos);
		hex->nodes[rec->pos]->type = BUILD_SETTLEMENT;
		if (rec->prev_status == BUILD_SETTLEMENT)
			break;
		/* Fall through and remove the settlement too
		 */
	case BUILD_SETTLEMENT:
		player->num_settlements--;

		player_broadcast(player, PB_RESPOND, "remove %B %d %d %d\n",
				 BUILD_SETTLEMENT, rec->x, rec->y, rec->pos);
		hex->nodes[rec->pos]->type = BUILD_NONE;
		hex->nodes[rec->pos]->owner = -1;
		break;
	case BUILD_MOVE_SHIP:
		hex->edges[rec->pos]->owner = -1;
		hex->edges[rec->pos]->type = BUILD_NONE;
		hex = map_hex(map, rec->prev_x, rec->prev_y);
		hex->edges[rec->prev_pos]->owner = player->num;
		hex->edges[rec->prev_pos]->type = BUILD_SHIP;
		map->has_moved_ship = FALSE;
		player_broadcast(player, PB_RESPOND,
				"move-back %d %d %d %d %d %d\n",
				rec->prev_x, rec->prev_y, rec->prev_pos,
				rec->x, rec->y, rec->pos);
		break;
	}
	
	/* Give back the money, if any */
	if (rec->cost != NULL)
		resource_refund(player, rec->cost);

	/* If the longest road changed, change it back */
	longest_road = game->longest_road ? game->longest_road->num : -1;
	if (longest_road != rec->longest_road) {
		if (rec->longest_road >= 0)
			player_broadcast(player_by_num (game, rec->longest_road),
					PB_ALL, "longest-road\n");
		else
			player_broadcast(player_none (game), PB_ALL,
					"longest-road\n");
	}
	game->longest_road = player_by_num (game, rec->longest_road);

	/* free the memory */
	g_free(rec);

	return TRUE;
}
