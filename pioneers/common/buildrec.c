/*
 * Gnocatan: a fun game.
 * (C) 1999 the Free Software Foundation
 *
 * Author: Dave Cole.
 *
 * Implementation of the excellent Settlers of Catan board game.  Go
 * buy a copy.
 */
#include <math.h>
#include <ctype.h>

#include <glib.h>

#include "game.h"
#include "map.h"
#include "buildrec.h"

GList *buildrec_free(GList *list)
{
	while (list != NULL) {
		BuildRec *rec = list->data;
		list = g_list_remove(list, rec);
		g_free(rec);
	}

	return NULL;
}

gint buildrec_count_type(GList *list, BuildType type)
{
	gint num = 0;

	while (list != NULL) {
		BuildRec *rec = list->data;
		list = g_list_next(list);
		if (rec->type == type)
			num++;
	}

	return num;
}

gint buildrec_count_edges(GList *list)
{
	gint num = 0;

	while (list != NULL) {
		BuildRec *rec = list->data;
		list = g_list_next(list);
		if (rec->type == BUILD_ROAD
		    || rec->type == BUILD_SHIP
		    || rec->type == BUILD_BRIDGE)
			num++;
	}

	return num;
}

BuildRec *buildrec_get(GList *list, BuildType type, gint idx)
{

	while (list != NULL) {
		BuildRec *rec = list->data;
		list = g_list_next(list);
		if (rec->type == type && idx-- == 0)
			return rec;
	}

	return NULL;
}

BuildRec *buildrec_get_edge(GList *list, gint idx)
{

	while (list != NULL) {
		BuildRec *rec = list->data;
		list = g_list_next(list);
		if ((rec->type == BUILD_ROAD
		     || rec->type == BUILD_SHIP
		     || rec->type == BUILD_BRIDGE) && idx-- == 0)
			return rec;
	}

	return NULL;
}

gboolean buildrec_is_valid(GList *list, Map *map, gint owner)
{
	while (list != NULL) {
		BuildRec *rec = list->data;
		list = g_list_next(list);

		switch (rec->type) {
		case BUILD_NONE:
			g_warning("BUILD_NONE in buildrec_is_valid");
			continue;
		case BUILD_ROAD:
			/* Roads have to be adjacent to buildings / road
			 */
			if (!map_road_connect_ok(map, owner,
						 rec->x, rec->y, rec->pos))
				return FALSE;
			continue;
		case BUILD_BRIDGE:
			/* Bridges have to be adjacent to buildings /
			 * road, and they have to be over water.
			 */
			if (!map_bridge_connect_ok(map, owner,
						   rec->x, rec->y, rec->pos))
				return FALSE;
			continue;
		case BUILD_SHIP:
			/* Bridges have to be adjacent to buildings /
			 * ships, and they have to be over water /
			 * coast.
			 */
			if (!map_ship_connect_ok(map, owner,
						 rec->x, rec->y, rec->pos))
				return FALSE;
			continue;
		case BUILD_SETTLEMENT:
		case BUILD_CITY:
			/* Buildings must be adjacent to a road
			 */
			if (!map_building_connect_ok(map, owner, rec->type,
						     rec->x, rec->y, rec->pos))
				return FALSE;
			continue;
		}
	}

	return TRUE;
}

static gboolean edge_has_place_for_settlement(Edge *edge)
{
	gint idx;

	for (idx = 0; idx < numElem(edge->nodes); idx++) {
		Node *node = edge->nodes[idx];
		if (node->type == BUILD_NONE
		    && is_node_on_land(node)
		    && is_node_spacing_ok(node))
			return TRUE;
	}
	return FALSE;
}

/* Check if we can place this edge with 0 existing settlements during setup
 */
static gboolean can_setup_edge_0(GList *list, Map *map, Edge *edge)
{
	BuildRec *rec = buildrec_get_edge(list, 0);
	Edge *other_edge;
	int idx;

	if (rec == NULL)
		/* This is the only edge - it can only placed if one
		 * of its nodes is a legal location for a new
		 * settlement.
		 */
		return edge_has_place_for_settlement(edge);

	/* There is already one edge placed.  We can only place this
	 * edge if it creates a second legal place for settlements.
	 * If I place a settlement on one of the edges, make sure
	 * there is still a place where the second settlement can be
	 * placed.
	 */
	other_edge = map_edge(map, rec->x, rec->y, rec->pos);
	for (idx = 0; idx < numElem(edge->nodes); idx++) {
		Node *node = edge->nodes[idx];

		if (node->type == BUILD_NONE && is_node_spacing_ok(node)) {
			gboolean ok;

			node->type = BUILD_SETTLEMENT;
			ok = edge_has_place_for_settlement(other_edge);
			node->type = BUILD_NONE;

			if (ok)
				return TRUE;
		}
	}

	return FALSE;
}

/* Check if we can place this edge with 1 existing settlement during setup
 */
static gboolean can_setup_edge_1(GList *list, Map *map, Edge *edge)
{
	BuildRec *rec = buildrec_get(list, BUILD_SETTLEMENT, 0);
	Node *node = map_node(map, rec->x, rec->y, rec->pos);
	Edge *other_edge;

	rec = buildrec_get_edge(list, 0);
	if (rec == NULL)
		/* No other edges placed yet, we can either place this
		 * edge next to the existing settlement, or somewhere
		 * which has a legal place for an additional
		 * settlement.
		 */
		return is_edge_adjacent_to_node(edge, node)
			|| edge_has_place_for_settlement(edge);

	/* This is the second edge, we must ensure that one of the
	 * edges is adjacent to the settlement, and the other has a
	 * place for the second settlement.
	 */
	other_edge = map_edge(map, rec->x, rec->y, rec->pos);
	return (is_edge_adjacent_to_node(edge, node)
		&& edge_has_place_for_settlement(other_edge))
		|| (is_edge_adjacent_to_node(other_edge, node)
		    && edge_has_place_for_settlement(edge));
}

/* Check if we can place this edge with 2 existing settlements during setup
 */
static gboolean can_setup_edge_2(GList *list, Map *map, Edge *edge)
{
	BuildRec *rec = buildrec_get(list, BUILD_SETTLEMENT, 0);
	Node *node = map_node(map, rec->x, rec->y, rec->pos);
	Node *other_node;
	Edge *other_edge;

	rec = buildrec_get(list, BUILD_SETTLEMENT, 1);
	other_node = map_node(map, rec->x, rec->y, rec->pos);

	rec = buildrec_get_edge(list, 0);
	if (rec == NULL)
		/* No other edges placed yet, we must place this edge
		 * next to either settlement.
		 */
		return is_edge_adjacent_to_node(edge, node)
			|| is_edge_adjacent_to_node(edge, other_node);

	/* Two settlements and one edge placed, we must make sure that
	 * we place this edge next to a settlement and both
	 * settlements then have an adjacent edge.  If we have
	 * bridges, it is possible to have both settlements adjacent
	 * to a single bridge.
	 */
	other_edge = map_edge(map, rec->x, rec->y, rec->pos);
	if (is_edge_adjacent_to_node(other_edge, node)
	    && is_edge_adjacent_to_node(other_edge, other_node))
		/* other_edge is a bridge connecting both settlements
		 * -> edge can connect to either settlement.
		 */
		return is_edge_adjacent_to_node(edge, node)
			|| is_edge_adjacent_to_node(edge, other_node);
	if (is_edge_adjacent_to_node(edge, node)
	    && is_edge_adjacent_to_node(edge, other_node)
	    && !is_edge_on_land(edge))
		/* This edge is a bridge connecting both settlements
		 */
		return TRUE;
	/* No bridges -> edge must be adjacent to the settlement which
	 * other_edge is not adjacent to.
	 */
	if (is_edge_adjacent_to_node(other_edge, other_node))
		return is_edge_adjacent_to_node(edge, node);
	else
		return is_edge_adjacent_to_node(edge, other_node);
}

gboolean buildrec_can_setup_edge(GList *list, Map *map,
				 Edge *edge, gboolean is_double)
{
	if (!is_double) {
		BuildRec *rec = buildrec_get(list, BUILD_SETTLEMENT, 0);
		if (rec != NULL) {
			/* We have placed a settlement, the edge must
			 * be placed adjacent to that settlement.
			 */
			Node *node = map_node(map, rec->x, rec->y, rec->pos);
			return is_edge_adjacent_to_node(edge, node);
		}
		/* We have not placed a settlement yet, the edge can
		 * only placed if one of its nodes is a legal location
		 * for a new settlement.
		 */
		return edge_has_place_for_settlement(edge);
	}

	/* Double setup is more difficult - there are a lot more
	 * situations to be handled.
	 */
	switch (buildrec_count_type(list, BUILD_SETTLEMENT)) {
	case 0: return can_setup_edge_0(list, map, edge);
	case 1: return can_setup_edge_1(list, map, edge);
	case 2: return can_setup_edge_2(list, map, edge);
	}
	g_warning("more than 2 settlements in setup!!!");
	return FALSE;
}

gboolean buildrec_can_setup_road(GList *list, Map *map,
				 Edge *edge, gboolean is_double)
{
	if (!can_road_be_setup(edge, -1))
		return FALSE;

	return buildrec_can_setup_edge(list, map, edge, is_double);
}

gboolean buildrec_can_setup_ship(GList *list, Map *map,
				 Edge *edge, gboolean is_double)
{
	if (!can_ship_be_setup(edge, -1))
		return FALSE;

	return buildrec_can_setup_edge(list, map, edge, is_double);
}

gboolean buildrec_can_setup_bridge(GList *list, Map *map,
				   Edge *edge, gboolean is_double)
{
	if (!can_bridge_be_setup(edge, -1))
		return FALSE;

	return buildrec_can_setup_edge(list, map, edge, is_double);
}

/* Check if we can place this settlement with 0 existing edges during setup
 */
static gboolean can_setup_settlement_0(GList *list, Map *map, Node *node)
{
	return TRUE;
}

/* Check if we can place this settlement with 1 existing edge during setup
 */
static gboolean can_setup_settlement_1(GList *list, Map *map, Node *node)
{
	BuildRec *rec = buildrec_get_edge(list, 0);
	Edge *edge = map_edge(map, rec->x, rec->y, rec->pos);
	Node *other_node;

	/* Make sure that we place one settlement next to the existing edge.
	 */
	rec = buildrec_get(list, BUILD_SETTLEMENT, 0);
	if (rec == NULL)
		/* No other settlements placed yet.
		 */
		return TRUE;

	/* There is one edge and one settlement placed.  One of the
	 * settlements must be placed next to the edge.
	 */
	other_node = map_node(map, rec->x, rec->y, rec->pos);
	return is_edge_adjacent_to_node(edge, node)
		|| is_edge_adjacent_to_node(edge, other_node);
}

/* Check if we can place this settlement with 2 existing edges during setup
 */
static gboolean can_setup_settlement_2(GList *list, Map *map, Node *node)
{
	BuildRec *rec = buildrec_get_edge(list, 0);
	Edge *edge = map_edge(map, rec->x, rec->y, rec->pos);
	Edge *other_edge;
	Node *other_node;

	rec = buildrec_get_edge(list, 1);
	other_edge = map_edge(map, rec->x, rec->y, rec->pos);

	/* Two edges placed, we must make sure that we place this
	 * settlement adjacent to an edge.
	 */
	if (!is_edge_adjacent_to_node(edge, node)
	    && !is_edge_adjacent_to_node(other_edge, node))
		return FALSE;

	rec = buildrec_get(list, BUILD_SETTLEMENT, 0);
	if (rec == NULL) {
		/* No settlements placed yet, place the settlement and
		 * make sure that there is still a valid place for the
		 * second settlement.
		 */
		gboolean is_ok = FALSE;

		node->type = BUILD_SETTLEMENT;
		node->owner = edge->owner;
		if (is_edge_adjacent_to_node(edge, node)) {
			if (is_edge_adjacent_to_node(other_edge, node))
				/* Node is adjacent to both edges -
				 * make sure there is still a valid
				 * location on either edge.
				 */
				is_ok = edge_has_place_for_settlement(edge)
					|| edge_has_place_for_settlement(other_edge);
			else
				/* Node is adjacent to edge, make sure
				 * other edge has location for
				 * settlement.
				 */
				is_ok = edge_has_place_for_settlement(other_edge);
		} else
			/* Node is adjacent to other edge - make sure
			 * edge has location for settlement.
			 */
			is_ok = edge_has_place_for_settlement(edge);
		node->type = BUILD_NONE;
		node->owner = -1;
		return is_ok;
	}

	/* Two edges and one settlement placed, ensure that each edge
	 * is adjacent to at least one settlement.
	 */
	other_node = map_node(map, rec->x, rec->y, rec->pos);
	if (is_edge_adjacent_to_node(edge, other_node)) {
		if (is_edge_adjacent_to_node(other_edge, other_node))
			return TRUE;
		else
			return is_edge_adjacent_to_node(edge, other_node);
	} else
		return is_edge_adjacent_to_node(edge, node);
}

gboolean buildrec_can_setup_settlement(GList *list, Map *map,
				       Node *node, gboolean is_double)
{
	if (!can_settlement_be_setup(node, -1))
		return FALSE;

	if (!is_double) {
		BuildRec *rec = buildrec_get_edge(list, 0);
		if (rec != NULL) {
			/* We have placed an edge, the settlement must
			 * be placed adjacent to that edge.
			 */
			Edge *edge = map_edge(map, rec->x, rec->y, rec->pos);
			return is_edge_adjacent_to_node(edge, node);
		}
		/* We have not placed an edge yet, the settlement is OK.
		 */
		return TRUE;
	}

	/* Double setup is more difficult - there are a lot more
	 * situations to be handled.
	 */
	switch (buildrec_count_edges(list)) {
	case 0: return can_setup_settlement_0(list, map, node);
	case 1: return can_setup_settlement_1(list, map, node);
	case 2: return can_setup_settlement_2(list, map, node);
	}
	g_warning("more than 2 settlements in setup!!!");
	return FALSE;
}
