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

#include "config.h"
#include <stdio.h>

#include "game.h"
#include "map.h"
#include "client.h"
#include "cost.h"
#include "log.h"
#include "callback.h"

static Player players[MAX_PLAYERS];
static GList *viewers;

static gint turn_player = -1;	/* whose turn is it */
static gint my_player_id = -1;	/* what is my player number */
static gint num_total_players = 4; /* total number of players in the game */

/* this function is called when the game starts, to clean up from the
 * previous game. */
void player_reset ()
{
	gint i;
	/* remove all viewers */
	while (viewers != NULL)
		viewers = g_list_remove (viewers, viewers->data);
	/* free player's memory */
	for (i = 0; i < MAX_PLAYERS; ++i) {
		if (players[i].name != NULL) {
			g_free (players[i].name);
			players[i].name = NULL;
		}
		while (players[i].points != NULL) {
			Points *points = players[i].points->data;
			g_free (points->name);
			g_free (points);
			players[i].points = g_list_remove (players[i].points,
					points);
		}
	}
}

Player *player_get(gint num)
{
	return &players[num];
}

gboolean player_is_viewer (gint num)
{
	return num < 0 || num >= num_total_players;
}

Viewer *viewer_get (num)
{
	GList *list;
	for (list = viewers; list != NULL; list = g_list_next (list) ) {
		Viewer *viewer = list->data;
		if (viewer->num == num) break;
	}
	if (list) return list->data;
	return NULL;
}

gchar *player_name(gint player_num, gboolean word_caps)
{
	static gchar buff[256];
	if (player_num >= num_total_players) {
		/* this is about a viewer */
		Viewer *viewer = viewer_get (player_num);
		if (viewer != NULL)
			return viewer->name;
		else {
			if (word_caps)
				sprintf(buff, _("Viewer %d"), player_num);
			else
				sprintf(buff, _("viewer %d"), player_num);
			return buff;
		}
	} else if (player_num >= 0) {
		Player *player = player_get (player_num);
		return player->name;
	}
	if (word_caps)
		sprintf(buff, _("Player %d"), player_num);
	else
		sprintf(buff, _("player %d"), player_num);
	return buff;
}

gint my_player_num()
{
	return my_player_id;
}

gint num_players ()
{
	return num_total_players;
}

void player_set_my_num(gint player_num)
{
	my_player_id = player_num;
}

void player_reset_statistic(void)
{
	gint idx,player_num;
	Player *player;

	for (player_num = 0; player_num < numElem(players); player_num++) {
		player = player_get (player_num);
		for (idx = 0; idx < numElem(player->statistics); idx++) {
			player->statistics[idx] = 0;
		}
	}
}

void player_modify_statistic(gint player_num, StatisticType type, gint num)
{
	Player *player = player_get (player_num);
	player->statistics[type] += num;
	callbacks.new_statistics (player_num, type, num);
}

void player_change_name(gint player_num, const gchar *name)
{
	Player *player;
	gchar *old_name;

	if (player_num < 0)
		return;
	if (player_num >= num_total_players) {
		/* this is about a viewer */
		Viewer *viewer = viewer_get (player_num);
		if (viewer == NULL) {
			/* there is a new viewer */
			viewer = g_malloc0 (sizeof (*viewer) );
			viewers = g_list_prepend (viewers, viewer);
			viewer->num = player_num;
			viewer->name = NULL;
			old_name = NULL;
		} else {
			old_name = viewer->name;
		}
		if (old_name == NULL)
			log_message( MSG_NAMEANON, _("New viewer: %s.\n"),
					name);
		else
			log_message( MSG_NAMEANON, _("%s is now %s.\n"),
					old_name, name);
		viewer->name = g_strdup (name);
		if (old_name != NULL)
			g_free (old_name);
		return;
	}

	player = player_get (player_num);
	old_name = player->name;
	player->name = g_strdup(name);
	if (old_name == NULL)
		log_message( MSG_NAMEANON, _("Player %d is now %s.\n"), player_num, name);
	else
		log_message( MSG_NAMEANON, _("%s is now %s.\n"), old_name, name);
	if (old_name != NULL)
		g_free(old_name);
	callbacks.player_name (player_num, name);
}

void player_has_quit(gint player_num)
{
	Player *player;
	Viewer *viewer;

	if (player_num < 0)
		return;

	/* usually callbacks are called after the event has been handled.
	 * Here it is called before, so the frontend can access the
	 * information about the quitting player/viewer */

	if (player_num >= num_total_players) {
		/* a viewer has quit */
		callbacks.viewer_quit (player_num);
		viewer = viewer_get (player_num);
		g_free (viewer->name);
		viewers = g_list_remove (viewers, viewer);
		return;
	}
	callbacks.player_quit (player_num);
	player = player_get (player_num);
	log_message( MSG_ERROR, _("%s has quit\n"), player_name(player_num,
				TRUE));
	if (player->name != NULL) {
		g_free(player->name);
		player->name = NULL;
	}
}

void player_largest_army(gint player_num)
{
	gint idx;
	if (player_num < 0)
		log_message( MSG_LARGESTARMY, _("There is no largest army.\n"));
	else
		log_message( MSG_LARGESTARMY, _("%s has the largest army.\n"),
				player_name(player_num, TRUE));
	for (idx = 0; idx < num_total_players; idx++) {
		Player *player = player_get (idx);
		if (player->statistics[STAT_LARGEST_ARMY] != 0
				&& idx != player_num)
			player_modify_statistic(idx, STAT_LARGEST_ARMY, -1);
		if (player->statistics[STAT_LARGEST_ARMY] == 0
				&& idx == player_num)
			player_modify_statistic(idx, STAT_LARGEST_ARMY, 1);
	}
}


void player_longest_road(gint player_num)
{
	gint idx;

	if (player_num < 0)
		log_message( MSG_LONGESTROAD, _("There is no longest road.\n"));
	else
		log_message( MSG_LONGESTROAD, _("%s has the longest road.\n"),
			 player_name(player_num, TRUE));

	for (idx = 0; idx < num_total_players; idx++) {
		Player *player = player_get (idx);

		if (player->statistics[STAT_LONGEST_ROAD] != 0
		    && idx != player_num)
			player_modify_statistic(idx, STAT_LONGEST_ROAD, -1);
		if (player->statistics[STAT_LONGEST_ROAD] == 0
		    && idx == player_num)
			player_modify_statistic(idx, STAT_LONGEST_ROAD, 1);
	}
}

void player_set_current(gint player_num)
{
	turn_player = player_num;
	if (player_num != my_player_num()) {
		char buffer[100];
		snprintf (buffer, sizeof(buffer), _("Waiting for %s."),
				player_name(player_num, FALSE));
		callbacks.instructions(buffer);
		return;
	}
	build_new_turn();
}

void player_set_total_num(gint num)
{
	num_total_players = num;
}

void player_stole_from(gint player_num, gint victim_num, Resource resource)
{
	gchar buf[128];

	player_modify_statistic(player_num, STAT_RESOURCES, 1);
	player_modify_statistic(victim_num, STAT_RESOURCES, -1);

	if (resource == NO_RESOURCE) {
		/* We are not in on the action
		 */
		 /* CHECK THIS: Since anonymous players (NULL) no longer exist,
		  *  player_name doesn't use its static buffer anymore, and
		  * two calls can be safely combined. If not: ai/player.c should also be fixed */
		/* FIXME: in the client, anonymous players can exist.
		 * I prefer changing that instead of this function */
		log_message( MSG_STEAL, _("%s stole a resource from %s.\n"),
			player_name(player_num, TRUE),
			player_name(victim_num, FALSE));
		return;
	}

	resource_cards(1, resource, buf, sizeof(buf));
	if (player_num == my_player_num()) {
		/* We stole a card :-)
		 */
		log_message( MSG_STEAL, _("You stole %s from %s.\n"),
			 buf, player_name(victim_num, FALSE));
		resource_modify(resource, 1);
	} else {
		/* Someone stole our card :-(
		 */
		log_message( MSG_STEAL, _("%s stole %s from you.\n"),
			 player_name(player_num, TRUE), buf);
		resource_modify(resource, -1);
	}
}

void player_domestic_trade(gint player_num, gint partner_num,
			   gint *supply, gint *receive)
{
	gchar supply_desc[512];
	gchar receive_desc[512];
	gint diff;
	gint idx;

	diff = resource_count(receive) - resource_count(supply);
	player_modify_statistic(player_num, STAT_RESOURCES, -diff);
	player_modify_statistic(partner_num, STAT_RESOURCES, diff);
	if (player_num == my_player_num()) {
		for (idx = 0; idx < NO_RESOURCE; idx++) {
			resource_modify(idx, supply[idx]);
			resource_modify(idx, -receive[idx]);
		}
	} else if (partner_num == my_player_num()) {
		for (idx = 0; idx < NO_RESOURCE; idx++) {
			resource_modify(idx, -supply[idx]);
			resource_modify(idx, receive[idx]);
		}
	}

	if (!resource_count(supply)) {
		if (!resource_count(receive)) {
			log_message( MSG_TRADE, _("%s gave %s nothing!?\n"),
				 player_name(player_num, TRUE),
				 player_name(partner_num, FALSE));
		} else {
			resource_format_num(receive_desc,
					sizeof (receive_desc), receive);
			log_message( MSG_TRADE, _("%s gave %s %s for free.\n"),
				 player_name(player_num, TRUE),
				 player_name(partner_num, FALSE),
				 receive_desc);
		}
	} else if (!resource_count(receive)) {
		resource_format_num(supply_desc, sizeof (supply_desc), supply);
		log_message( MSG_TRADE, _("%s gave %s %s for free.\n"),
			 player_name(partner_num, TRUE),
			 player_name(player_num, FALSE),
			 supply_desc);
	} else {
		resource_format_num(supply_desc, sizeof (supply_desc), supply);
		resource_format_num(receive_desc,
					sizeof (receive_desc), receive);
		log_message( MSG_TRADE, _("%s gave %s %s in exchange for %s.\n"),
			 player_name(player_num, TRUE),
			 player_name(partner_num, FALSE),
			 receive_desc, supply_desc);
	}
}

void player_maritime_trade(gint player_num,
			   gint ratio, Resource supply, Resource receive)
{
	gchar buf_give[128];
	gchar buf_receive[128];

	player_modify_statistic(player_num, STAT_RESOURCES, 1 - ratio);
	if (player_num == my_player_num()) {
		resource_modify(supply, -ratio);
		resource_modify(receive, 1);
	}

	resource_cards(ratio, supply, buf_give, sizeof(buf_give));
	resource_cards(1, receive, buf_receive, sizeof(buf_receive));
	log_message( MSG_TRADE, _("%s exchanged %s for %s.\n"),
		 player_name(player_num, TRUE), buf_give, buf_receive);
}

void player_build_add(gint player_num,
		      BuildType type, gint x, gint y, gint pos,
		      gboolean log_changes)
{
	Edge *edge;
	Node *node;

	switch (type) {
	case BUILD_ROAD:
		edge = map_edge(map, x, y, pos);
		edge->owner = player_num;
		edge->type = BUILD_ROAD;
		callbacks.draw_edge (edge);
		if (log_changes)
		{
			log_message( MSG_BUILD, _("%s built a road.\n"),
			             player_name(player_num, TRUE));
		}
		if (player_num == my_player_num())
			stock_use_road();
		break;
	case BUILD_SHIP:
		edge = map_edge(map, x, y, pos);
		edge->owner = player_num;
		edge->type = BUILD_SHIP;
		callbacks.draw_edge (edge);
		if (log_changes)
		{
			log_message( MSG_BUILD, _("%s built a ship.\n"),
			             player_name(player_num, TRUE));
		}
		if (player_num == my_player_num())
			stock_use_ship();
		break;
	case BUILD_SETTLEMENT:
		node = map_node(map, x, y, pos);
		node->type = BUILD_SETTLEMENT;
		node->owner = player_num;
		callbacks.draw_node (node);
		if (log_changes)
		{
			log_message( MSG_BUILD, _("%s built a settlement.\n"),
			             player_name(player_num, TRUE));
		}
		player_modify_statistic(player_num, STAT_SETTLEMENTS, 1);
		if (player_num == my_player_num())
			stock_use_settlement();
		break;
	case BUILD_CITY:
		node = map_node(map, x, y, pos);
		if (node->type == BUILD_SETTLEMENT) {
			player_modify_statistic(player_num,
						STAT_SETTLEMENTS, -1);
			if (player_num == my_player_num())
				stock_replace_settlement();
		}
		node->type = BUILD_CITY;
		node->owner = player_num;
		callbacks.draw_node (node);
		if (log_changes)
		{
			log_message( MSG_BUILD, _("%s built a city.\n"),
			             player_name(player_num, TRUE));
		}
		player_modify_statistic(player_num, STAT_CITIES, 1);
		if (player_num == my_player_num())
			stock_use_city();
		break;
	case BUILD_NONE:
		log_message( MSG_ERROR, _("player_build_add called with BUILD_NONE for user %s\n"),
			 player_name(player_num, TRUE));
		break;
	case BUILD_BRIDGE:
		edge = map_edge(map, x, y, pos);
		edge->owner = player_num;
		edge->type = BUILD_BRIDGE;
		callbacks.draw_edge (edge);
		if (log_changes)
		{
			log_message( MSG_BUILD, _("%s built a bridge.\n"),
			             player_name(player_num, TRUE));
		}
		if (player_num == my_player_num())
			stock_use_bridge();
		break;
	case BUILD_MOVE_SHIP:
		/* This clause here to remove a compiler warning.
		   It should not be possible to reach this code. */
		abort ();
		break;
	}
}

void player_build_remove(gint player_num,
			 BuildType type, gint x, gint y, gint pos)
{
	Edge *edge;
	Node *node;

	switch (type) {
	case BUILD_ROAD:
		edge = map_edge(map, x, y, pos);
		edge->owner = -1;
		callbacks.draw_edge (edge);
		edge->type = BUILD_NONE;
		log_message( MSG_BUILD, _("%s removed a road.\n"),
			 player_name(player_num, TRUE));
		if (player_num == my_player_num())
			stock_replace_road();
		break;
	case BUILD_SHIP:
		edge = map_edge(map, x, y, pos);
		edge->owner = -1;
		callbacks.draw_edge (edge);
		edge->type = BUILD_NONE;
		log_message( MSG_BUILD, _("%s removed a ship.\n"),
			 player_name(player_num, TRUE));
		if (player_num == my_player_num())
			stock_replace_ship();
		break;
	case BUILD_SETTLEMENT:
		node = map_node(map, x, y, pos);
		node->type = BUILD_NONE;
		node->owner = -1;
		callbacks.draw_node (node);
		log_message( MSG_BUILD, _("%s removed a settlement.\n"),
			 player_name(player_num, TRUE));
		player_modify_statistic(player_num, STAT_SETTLEMENTS, -1);
		if (player_num == my_player_num())
			stock_replace_settlement();
		break;
	case BUILD_CITY:
		node = map_node(map, x, y, pos);
		node->type = BUILD_SETTLEMENT;
		node->owner = player_num;
		callbacks.draw_node (node);
		log_message( MSG_BUILD, _("%s removed a city.\n"),
			 player_name(player_num, TRUE));
		player_modify_statistic(player_num, STAT_SETTLEMENTS, 1);
		player_modify_statistic(player_num, STAT_CITIES, -1);
		if (player_num == my_player_num()) {
			stock_use_settlement();
			stock_replace_city();
		}
		break;
	case BUILD_NONE:
		log_message( MSG_ERROR, _("player_build_remove called with BUILD_NONE for user %s\n"),
			 player_name(player_num, TRUE));
		break;
	case BUILD_BRIDGE:
		edge = map_edge(map, x, y, pos);
		edge->owner = -1;
		callbacks.draw_edge (edge);
		edge->type = BUILD_NONE;
		log_message( MSG_BUILD, _("%s removed a bridge.\n"),
			 player_name(player_num, TRUE));
		if (player_num == my_player_num())
			stock_replace_bridge();
		break;
	case BUILD_MOVE_SHIP:
		/* This clause here to remove a compiler warning.
		   It should not be possible to reach this case. */
		abort ();
		break;
	}
}

void player_build_move(gint player_num, gint sx, gint sy, gint spos,
		gint dx, gint dy, gint dpos, gint isundo) {
	Edge *from = map_edge(map, sx, sy, spos),
		*to = map_edge(map, dx, dy, dpos);
	if (isundo) {
		Edge *tmp = from;
		from = to;
		to = tmp;
	}
	from->owner = -1;
	callbacks.draw_edge (from);
	from->type = BUILD_NONE;
	to->owner = player_num;
	to->type = BUILD_SHIP;
	callbacks.draw_edge (to);
	if (isundo) log_message(MSG_BUILD, _("%s has cancelled a ship's movement.\n"),
				player_name (player_num, TRUE));
	else log_message(MSG_BUILD, _("%s moved a ship.\n"),
			player_name (player_num, TRUE));
}

void player_resource_action(gint player_num, gchar *action,
			    gint *resource_list, gint mult)
{
	resource_log_list(player_num, action, resource_list);
	resource_apply_list(player_num, resource_list, mult);
}

/* get a new point */
void player_get_point (gint player_num, gint id, gchar *str, gint num)
{
	Player *player = player_get (player_num);
	/* create the memory and fill it */
	Points *point = g_malloc0 (sizeof (*point) );
	point->id = id;
	point->name = g_strdup (str);
	point->points = num;
	/* put the point in the list */
	player->points = g_list_append (player->points, point);
	/* tell the user */
	log_message (MSG_INFO, _("%s received %s.\n"), player->name, str);
}

/* lose a point: noone gets it */
void player_lose_point (gint player_num, gint id)
{
	Player *player = player_get (player_num);
	Points *point;
	GList *list;
	/* look up the point in the list */
	for (list = player->points; list != NULL; list = g_list_next (list) ) {
		point = list->data;
		if (point->id == id) break;
	}
	/* communication error: the point doesn't exist */
	if (list == NULL) {
		log_message (MSG_ERROR, _("server asks to lose invalid point.\n"));
		return;
	}
	player->points = g_list_remove (player->points, point);
	/* tell the user the point is lost */
	log_message (MSG_INFO, _("%s lost %s.\n"), player->name, point->name);
	/* free the memory */
	g_free (point->name);
	g_free (point);
}

/* take a point from an other player */
void player_take_point (gint player_num, gint id, gint old_owner)
{
	Player *player = player_get (player_num);
	Player *victim = player_get (old_owner);
	Points *point;
	GList *list;
	/* look up the point in the list */
	for (list = victim->points; list != NULL; list = g_list_next (list) ) {
		point = list->data;
		if (point->id == id) break;
	}
	/* communication error: the point doesn't exist */
	if (list == NULL) {
		log_message (MSG_ERROR, _("server asks to move invalid point.\n"));
		return;
	}
	/* move the point in memory */
	victim->points = g_list_remove (victim->points, point);
	player->points = g_list_append (player->points, point);
	/* tell the user about it */
	log_message (MSG_INFO, _("%s lost %s to %s.\n"), victim->name,
			point->name, player->name);
}

gint current_player ()
{
	return turn_player;
}
