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
#include "ai.h"
#include <stdio.h>
#include <stdlib.h>
/*
 * This is a rudimentary AI for gnocatan. 
 *
 * What it does _NOT_ do:
 *
 * -Play with gold.  It aborts with an error.
 * -Play monopoly development card
 * -Make roads explicitly to get the longest road card
 * -Trade with other players
 * -Do anything seafarers
 *
 */

typedef struct resource_values_s {
	float value[NO_RESOURCE];
	MaritimeInfo info;
} resource_values_t;

static Map *map;
static int quote_num;
/* used to avoid multiple chat messages when more than one other player
 * must discard resources */
static gboolean discard_starting;

/*
 * Forward declarations
 */
static Edge *best_road_to_road_spot(Node * n, float *score,
				    resource_values_t * resval);

static Edge *best_road_spot(resource_values_t * resval);

/*
 * Functions to keep track of what nodes we've visited
 */

typedef struct node_seen_set_s {

	Node *seen[MAP_SIZE * MAP_SIZE];
	int size;

} node_seen_set_t;

static void nodeset_reset(node_seen_set_t * set)
{
	set->size = 0;
}

static void nodeset_set(node_seen_set_t * set, Node * n)
{
	int i;

	for (i = 0; i < set->size; i++)
		if (set->seen[i] == n)
			return;

	set->seen[set->size] = n;
	set->size++;
}

static int nodeset_isset(node_seen_set_t * set, Node * n)
{
	int i;

	for (i = 0; i < set->size; i++)
		if (set->seen[i] == n)
			return 1;

	return 0;
}

typedef void iterate_node_func_t(Node * n, void *rock);

/*
 * Iterate over all the nodes on the map calling func() with 'rock'
 *
 */
static void for_each_node(iterate_node_func_t * func, void *rock)
{
	int i, j, k;

	for (i = 0; i < map->x_size; i++) {
		for (j = 0; j < map->y_size; j++) {
			for (k = 0; k < 6; k++) {
				Node *n = map_node(map, i, j, k);

				if (n)
					func(n, rock);
			}
		}
	}

}

/*
 * How many short of this resource?
 */
static int num_short(gint have, int need)
{
	if (have >= need)
		return 0;

	return need - have;
}

/*
 * Do I have the resources to buy this?
 *
 */
#define DEVEL_CARD 222

static int has_resources(gint assets[NO_RESOURCE], BuildType bt,
			 gint need[NO_RESOURCE])
{
	int i;
	for (i = 0; i < NO_RESOURCE; i++)
		need[i] = 0;

	switch (bt) {
	case BUILD_CITY:
		if ((assets[ORE_RESOURCE] >= 3) &&
		    (assets[GRAIN_RESOURCE] >= 2))
			return 1;

		need[ORE_RESOURCE] = num_short(assets[ORE_RESOURCE], 3);
		need[GRAIN_RESOURCE] =
		    num_short(assets[GRAIN_RESOURCE], 2);

		break;
	case BUILD_SETTLEMENT:
		if ((assets[BRICK_RESOURCE] >= 1) &&
		    (assets[GRAIN_RESOURCE] >= 1) &&
		    (assets[WOOL_RESOURCE] >= 1) &&
		    (assets[LUMBER_RESOURCE] >= 1))
			return 1;

		need[BRICK_RESOURCE] =
		    num_short(assets[BRICK_RESOURCE], 1);
		need[GRAIN_RESOURCE] =
		    num_short(assets[GRAIN_RESOURCE], 1);
		need[WOOL_RESOURCE] = num_short(assets[WOOL_RESOURCE], 1);
		need[LUMBER_RESOURCE] =
		    num_short(assets[LUMBER_RESOURCE], 1);

		break;
	case BUILD_ROAD:
		if ((assets[BRICK_RESOURCE] >= 1) &&
		    (assets[LUMBER_RESOURCE] >= 1))
			return 1;

		need[BRICK_RESOURCE] =
		    num_short(assets[BRICK_RESOURCE], 1);
		need[LUMBER_RESOURCE] =
		    num_short(assets[LUMBER_RESOURCE], 1);

		break;

	case DEVEL_CARD:
		if ((assets[ORE_RESOURCE] >= 1) &&
		    (assets[GRAIN_RESOURCE] >= 1) &&
		    (assets[WOOL_RESOURCE] >= 1))
			return 1;

		need[ORE_RESOURCE] = num_short(assets[ORE_RESOURCE], 1);
		need[GRAIN_RESOURCE] =
		    num_short(assets[GRAIN_RESOURCE], 1);
		need[WOOL_RESOURCE] = num_short(assets[WOOL_RESOURCE], 1);

		break;

	default:
		/* xxx bridge, ship */
		return 0;
	}

	return 0;
}

/*
 * Probability of a dice roll
 */

static float dice_prob(int roll)
{
	switch (roll) {
	case 2:
	case 12:
		return 3;
	case 3:
	case 11:
		return 6;
	case 4:
	case 10:
		return 8;
	case 5:
	case 9:
		return 11;
	case 6:
	case 8:
		return 14;
	default:
		return 0;
	}
}

/*
 * By default how valuable is this resource?
 */

static float default_score_resource(Resource resource)
{
	float score;

	switch (resource) {
	case HILL_TERRAIN:	/* brick */
		score = 1.1;
		break;
	case FIELD_TERRAIN:	/* grain */
		score = 1.0;
		break;
	case MOUNTAIN_TERRAIN:	/* rock */
		score = 1.05;
		break;
	case PASTURE_TERRAIN:	/* sheep */
		score = 1.0;
		break;
	case FOREST_TERRAIN:	/* wood */
		score = 1.1;
		break;
	case DESERT_TERRAIN:
	case SEA_TERRAIN:
	default:
		score = 0;
		break;
	}

	return score;
}

/* For each node I own see how much i produce with it. keep a
 * tally with 'produce'
 */

static void reevaluate_iterator(Node * n, void *rock)
{
	float *produce = (float *) rock;

	/* if i own this node */
	if ((n) && (n->owner == my_player_num())) {
		int l;
		for (l = 0; l < 3; l++) {
			Hex *h = n->hexes[l];
			float mult = 1.0;

			if (n->type == BUILD_CITY)
				mult = 2.0;

			if (h && h->terrain < NO_RESOURCE) {
				produce[h->terrain] +=
				    mult *
				    default_score_resource(h->terrain) *
				    dice_prob(h->roll);
			}

		}
	}

}

/*
 * Reevaluate the value of all the resources to us
 */

static void reevaluate_resources(resource_values_t * outval)
{
	float produce[NO_RESOURCE];
	int i;

	for (i = 0; i < NO_RESOURCE; i++) {
		produce[i] = 0;
	}

	for_each_node(&reevaluate_iterator, (void *) produce);

	/* Now invert all the positive numbers and give any zeros a weight of 2
	 *
	 */
	for (i = 0; i < NO_RESOURCE; i++) {
		if (produce[i] == 0) {
			outval->value[i] = default_score_resource(i);
		} else {
			outval->value[i] = 1.0 / produce[i];
		}

	}

	/*
	 * Save the maritime info too so we know if we can do port trades
	 */
	map_maritime_info(map, &outval->info, my_player_num());
}


/*
 *
 */
static float resource_value(Resource resource, resource_values_t * resval)
{
	if (resource >= NO_RESOURCE)
		return 0.0;

	return resval->value[resource];
}


/*
 * How valuable is this hex to me?
 */
static float score_hex(Hex * hex, resource_values_t * resval)
{
	float score;

	if (hex == NULL)
		return 0;

	/* multiple resource value by dice probability */
	score =
	    resource_value(hex->terrain, resval) * dice_prob(hex->roll);

	/* if we don't have a 3 for 1 port yet and this is one it's valuable! */
	if (!resval->info.any_resource) {
		if (hex->resource == ANY_RESOURCE)
			score += 0.5;
	}

	return score;
}

/*
 * How valuable is this hex to others
 */
static float default_score_hex(Hex * hex)
{
	int score;

	if (hex == NULL)
		return 0;

	/* multiple resource value by dice probability */
	score =
	    default_score_resource(hex->terrain) * dice_prob(hex->roll);

	return score;
}

/* 
 * Give a numerical score to how valuable putting a settlement/city on this spot is
 *
 */
static float score_node(Node * node, int city, resource_values_t * resval)
{
	int i;
	float score = 0;


	/* if not a node score -1 */
	if (node == NULL)
		return -1;

	/* if already occupied, in water, or too close to others  give a score of -1 */
	if (is_node_on_land(node) == FALSE)
		return -1;
	if (is_node_spacing_ok(node) == FALSE)
		return -1;
	if (city == 0) {
		if (node->owner != -1)
			return -1;
	}

	for (i = 0; i < 3; i++) {
		score += score_hex(node->hexes[i], resval);
	}

	return score;
}

/*
 * Road connects here
 */
static int road_connects(Node * n)
{
	int i;

	if (n == NULL)
		return 0;

	for (i = 0; i < 3; i++) {
		Edge *e = n->edges[i];

		if ((e) && (e->owner == my_player_num()))
			return 1;
	}

	return 0;
}


/*
 * Find the best spot for a house
 *
 * connected - does it have to be connected to a road playernum owns?
 */
static Node *best_settlement_spot(int connected,
				  resource_values_t * resval)
{
	int i, j, k;
	Node *best = NULL;
	float bestscore = -1.0;

	for (i = 0; i < map->x_size; i++) {
		for (j = 0; j < map->y_size; j++) {
			for (k = 0; k < 6; k++) {
				Node *n = map_node(map, i, j, k);
				float score = score_node(n, 0, resval);

				if ((connected) && (!road_connects(n)))
					continue;

				if (score > bestscore) {
					best = n;
					bestscore = score;
				}
			}

		}
	}

	return best;
}


/*
 * What is the best settlement to upgrade to a city?
 *
 */
static Node *best_city_spot(resource_values_t * resval)
{
	int i, j, k;
	Node *best = NULL;
	float bestscore = -1.0;

	for (i = 0; i < map->x_size; i++) {
		for (j = 0; j < map->y_size; j++) {
			for (k = 0; k < 6; k++) {
				Node *n = map_node(map, i, j, k);

				if ((n != NULL)
				    && (n->owner == my_player_num())
				    && (n->type == BUILD_SETTLEMENT)) {
					float score =
					    score_node(n, 1, resval);

					if (score > bestscore) {
						best = n;
						bestscore = score;
					}
				}
			}

		}
	}

	return best;
}

/*
 * Return the opposite end of this node, edge
 *
 */
static Node *other_node(Edge * e, Node * n)
{
	if (e->nodes[0] == n)
		return e->nodes[1];
	else
		return e->nodes[0];
}

/*
 *
 *
 */
static Edge *traverse_out(Node * n, node_seen_set_t * set, float *score,
			  resource_values_t * resval)
{
	float bscore = 0.0;
	Edge *best = NULL;
	int i;

	/* mark this node as seen */
	nodeset_set(set, n);

	for (i = 0; i < 3; i++) {
		Edge *e = n->edges[i];
		Edge *cur_e = NULL;
		Node *othernode;
		float cur_score;

		if (!e)
			continue;

		othernode = other_node(e, n);

		/* if our road traverse it */
		if (e->owner == my_player_num()) {

			if (!nodeset_isset(set, othernode))
				cur_e =
				    traverse_out(othernode, set,
						 &cur_score, resval);

		} else if ((e->owner == -1) && (is_edge_on_land(e))) {

			/* no owner, how good is the other node ? */
			cur_e = e;

			cur_score = score_node(othernode, 0, resval);

			/* umm.. can we build here? */
			/*if (!can_settlement_be_built(othernode, my_player_num ()))
			   cur_e = NULL;       */
		}

		/* is this the best edge we've seen? */
		if ((cur_e != NULL) && (cur_score > bscore)) {
			best = cur_e;
			bscore = cur_score;

		}
	}

	*score = bscore;
	return best;
}

/*
 * Best road to a road
 *
 */
static Edge *best_road_to_road_spot(Node * n, float *score,
				    resource_values_t * resval)
{
	int bscore = -1.0;
	Edge *best = NULL;
	int i, j;

	for (i = 0; i < 3; i++) {
		Edge *e = n->edges[i];
		if (e) {
			Node *othernode = other_node(e, n);

			if (is_edge_on_land(e) && e->owner == -1) {

				for (j = 0; j < 3; j++) {
					Edge *e2 = othernode->edges[j];


					if ((e2) && (e2->owner == -1)) {
						float score =
						    score_node(other_node
							       (e2,
								othernode),
							       0, resval);

						if (score > bscore) {
							bscore = score;
							best = e;
						}
					}
				}
			}

		}
	}

	*score = bscore;
	return best;
}

/*
 * Best road to road on whole map
 *
 */
static Edge *best_road_to_road(resource_values_t * resval)
{
	int i, j, k;
	Edge *best = NULL;
	float bestscore = -1.0;

	for (i = 0; i < map->x_size; i++) {
		for (j = 0; j < map->y_size; j++) {
			for (k = 0; k < 6; k++) {
				Node *n = map_node(map, i, j, k);
				Edge *e;
				float score;

				if ((n) && (n->owner == my_player_num())) {
					e = best_road_to_road_spot(n,
								   &score,
								   resval);
					if (score > bestscore) {
						best = e;
						bestscore = score;
					}
				}
			}
		}
	}

	return best;
}

/*
 * Best road spot
 *
 */
static Edge *best_road_spot(resource_values_t * resval)
{
	int i, j, k;
	Edge *best = NULL;
	float bestscore = -1.0;
	node_seen_set_t nodeseen;

	/*
	 * For every node that we're the owner of traverse out to find the best
	 * node we're one road away from and build that road
	 *
	 *
	 * xxx loops
	 */

	for (i = 0; i < map->x_size; i++) {
		for (j = 0; j < map->y_size; j++) {
			for (k = 0; k < 6; k++) {
				Node *n = map_node(map, i, j, k);

				if ((n != NULL)
				    && (n->owner == my_player_num())) {
					float score = -1.0;
					Edge *e;

					nodeset_reset(&nodeseen);

					e = traverse_out(n, &nodeseen,
							 &score, resval);

					if (score > bestscore) {
						best = e;
						bestscore = score;
					}
				}
			}

		}
	}

	return best;
}


/*
 * Any road at all that's valid for us to build
 */

static void rand_road_iterator(Node * n, void *rock)
{
	int i;
	Edge **out = (Edge **) rock;

	if (n == NULL)
		return;

	for (i = 0; i < 3; i++) {
		Edge *e = n->edges[i];

		if ((e) && (can_road_be_built(e, my_player_num())))
			*out = e;
	}
}

/*
 * Find any road we can legally build
 *
 */
static Edge *find_random_road(void)
{
	Edge *e = NULL;

	for_each_node(&rand_road_iterator, &e);

	return e;
}


static void places_can_build_iterator(Node * n, void *rock)
{
	int *count = (int *) rock;

	if (can_settlement_be_built(n, my_player_num()))
		(*count)++;
}

static int places_can_build_settlement(void)
{
	int count = 0;

	for_each_node(&places_can_build_iterator, (void *) &count);

	return count;
}

/*
 * How many resource cards does player have?
 *
 */
static int num_assets(gint assets[NO_RESOURCE])
{
	int i;
	int count = 0;

	for (i = 0; i < NO_RESOURCE; i++) {
		count += assets[i];
	}

	return count;
}

static int player_get_num_resource(int player)
{
	return player_get(player)->statistics[STAT_RESOURCES];
}

/*
 * Does this resource list contain one element? If so return it
 * otherwise return NO_RESOURCE
 */
static int which_one(gint assets[NO_RESOURCE])
{
	int i;
	int res = NO_RESOURCE;
	int tot = 0;

	for (i = 0; i < NO_RESOURCE; i++) {

		if (assets[i] > 0) {
			tot += assets[i];
			res = i;
		}
	}

	if (tot == 1)
		return res;

	return NO_RESOURCE;
}

/*
 * Does this resource list contain just one kind of element? If so return it
 * otherwise return NO_RESOURCE
 */
static int which_resource(gint assets[NO_RESOURCE])
{
	int i;
	int res = NO_RESOURCE;
	int n_nonzero = 0;

	for (i = 0; i < NO_RESOURCE; i++) {

		if (assets[i] > 0) {
			n_nonzero++;
			res = i;
		}
	}

	if (n_nonzero == 1)
		return res;

	return NO_RESOURCE;
}

/*
 * What resource do we want the most?
 *
 */

/* Don't want what isn't available anymore... */

static int resource_desire(gint assets[NO_RESOURCE],
			   resource_values_t * resval, int trade,
			   int tradenum)
{
	int i;
	int res = NO_RESOURCE;
	float value = 0.0;
	gint need[NO_RESOURCE];

	/* make it as if we don't have what we're trading away */
	if (trade != NO_RESOURCE) {
		assets[trade] -= tradenum;
	}

	/* do i need just 1 more for something? */
	if (!has_resources(assets, BUILD_CITY, need)) {
		if (which_one(need) != NO_RESOURCE)
			goto oneneed;
	}
	if (!has_resources(assets, BUILD_SETTLEMENT, need)) {
		if (which_one(need) != NO_RESOURCE)
			goto oneneed;
	}
	if (!has_resources(assets, BUILD_ROAD, need)) {
		if (which_one(need) != NO_RESOURCE)
			goto oneneed;
	}
	if (!has_resources(assets, DEVEL_CARD, need)) {
		if (which_one(need) != NO_RESOURCE)
			goto oneneed;
	}

	if (trade != NO_RESOURCE) {
		assets[trade] += tradenum;
	}

	/* desire the one we don't produce the most */
	for (i = 0; i < NO_RESOURCE; i++) {

		if ((resval->value[i] > value) && (assets[i] < 2)) {
			res = i;
			value = resval->value[i];
		}
	}

	/* don't do stupid trades :) */
	if (res == trade)
		return NO_RESOURCE;

	return res;

      oneneed:
	if (trade != NO_RESOURCE) {
		assets[trade] += tradenum;
	}
	res = which_one(need);

	/* don't do stupid trades :) */
	if (res == trade)
		return NO_RESOURCE;

	return res;
}



static void findit_iterator(Node * n, void *rock)
{
	Node **node = (Node **) rock;
	int i;

	if (!n)
		return;
	if (n->owner != my_player_num())
		return;

	/* if i own this node */
	for (i = 0; i < 3; i++) {
		if (n->edges[i]->owner == my_player_num())
			return;
	}

	*node = n;
}


/* Find the settlement with no roads yet
 *
 */

static Node *void_settlement(void)
{
	Node *ret = NULL;

	for_each_node(&findit_iterator, (void *) &ret);

	return ret;
}

/*
 * Game setup
 * Build one house and one road
 */
static void greedy_setup_house(void)
{
	Node *node;
	resource_values_t resval;

	reevaluate_resources(&resval);

	node = best_settlement_spot(0, &resval);

	/*node_add(player, BUILD_SETTLEMENT, node->x, node->y, node->pos, FALSE); */
	cb_build_settlement(node);
}


/*
 * Game setup
 * Build one house and one road
 */
static void greedy_setup_road(void)
{
	Node *node;
	Edge *e = NULL;
	int i;
	resource_values_t resval;
	float tmp;

	reevaluate_resources(&resval);

	node = void_settlement();

	e = best_road_to_road_spot(node, &tmp, &resval);

	/* if didn't find one just pick one randomly */
	if (e == NULL) {
		for (i = 0; i < numElem(node->edges); i++) {
			if (is_edge_on_land(node->edges[i])) {
				e = node->edges[i];
				break;
			}
		}
	}

	cb_build_road(e);
}

/*
 * What to do? what to do?
 *
 */

static void greedy_turn(void)
{
	resource_values_t resval;
	int i;
	gint need[NO_RESOURCE], assets[NO_RESOURCE];

	if (!have_rolled_dice()) {
		cb_roll();
		return;
	}

	/* Don't wait before the dice roll, that will take too long */
	ai_wait();
	for (i = 0; i < NO_RESOURCE; ++i)
		assets[i] = resource_asset(i);

	reevaluate_resources(&resval);

	/* if can then buy city */
	if (has_resources(assets, BUILD_CITY, need)) {

		Node *n = best_city_spot(&resval);

		/* don't if no cities left */
		if (stock_num_cities() <= 0)
			n = NULL;

		if (n != NULL) {
			cb_build_city(n);
			return;
		}
	}

	/* if can then buy settlement */
	if (has_resources(assets, BUILD_SETTLEMENT, need)) {

		Node *n = best_settlement_spot(1, &resval);

		/* don't if no settlements left */
		if (stock_num_settlements() <= 0)
			n = NULL;

		if (n != NULL) {
			cb_build_settlement(n);
			return;
		}

	}

	if (has_resources(assets, BUILD_ROAD, need)) {

		Edge *e;

		e = best_road_spot(&resval);

		if (e == NULL) {
			e = best_road_to_road(&resval);
		}

		/* don't sprawl :) */
		if (places_can_build_settlement() >= 2) {
			e = NULL;
		}

		/* don't if no roads left */
		if (stock_num_roads() <= 0)
			e = NULL;

		if (e != NULL) {
			cb_build_road(e);
			return;
		}
	}

	/* if we can buy a developement card and there are some left */
	if (has_resources(assets, DEVEL_CARD, need)) {
		if (can_buy_develop()) {
			cb_buy_develop();
			return;
		}
	}

	/* if have a lot of cards see if we can trade anything */
	if (num_assets(assets) >= 3) {

		if (can_trade_maritime()) {
			gint amount;
			MaritimeInfo info;

			map_maritime_info(map, &info, my_player_num());

			/* trade if possible. Try to trade every resource.
			 * First try trading 2:1, then 3:1, then 4:1
			 */
			for (amount = 2; amount <= 4; ++amount) {
				Resource try;
				for (try = 0; try < NO_RESOURCE; ++try) {
					Resource res;
					/* first check if the trade is possible */
					switch (amount) {
					case 2:
						/* do we have a 2 to 1 harbour for this resource? */
						if (!info.
						    specific_resource[try])
							continue;
						break;
					case 3:
						/* do we have a 3 to 1 harbour? */
						if (!info.any_resource)
							continue;
						break;
					case 4:
						/* always possible */
						break;
					}

					if (assets[try] >= amount) {	/* I have enough to trade away */
						res =
						    resource_desire(assets,
								    &resval,
								    try,
								    amount);
						if (get_bank()[res] == 0)
							res = NO_RESOURCE;

						if (res != NO_RESOURCE) {
							cb_maritime(amount,
								    try,
								    res);
							return;
						}
					}
				}
			}
		}
	}

	/* play developement cards */
	if (can_play_any_develop()) {
		const DevelDeck *deck = get_devel_deck();

		for (i = 0; i < deck->num_cards; i++) {
			DevelType cardtype = deck_card_type(deck, i);

			/* can't play card we just bought */
			if (can_play_develop(i)) {

				if (is_victory_card(cardtype)) {
					cb_play_develop(i);
					return;
				}

				switch (cardtype) {
				case DEVEL_SOLDIER:
				case DEVEL_YEAR_OF_PLENTY:
					cb_play_develop(i);
					return;

				case DEVEL_ROAD_BUILDING:
					/* don't if don't have two roads left */
					if (stock_num_roads() < 2)
						break;
					cb_play_develop(i);
					return;
				default:
					break;
				}

			}
		}
	}
	cb_end_turn();
}

#define randchat(array,nochat_percent)				\
	do {							\
		int p = (numElem(array)*1000/nochat_percent);	\
		int n = (rand() % p) / 10;			\
		if (n < numElem(array) )			\
			ai_chat (array[n]);			\
	} while(0)

static const char *chat_turn_start[] = {
	N_("Ok, let's go!"),
	N_("I'll beat you all now! ;)"),
	N_("Now for another try..."),
};

static const char *chat_receive_one[] = {
	N_("At least I get something..."),
	N_("One is better than none..."),
};

static const char *chat_receive_many[] = {
	N_("Wow!"),
	N_("Ey, I'm becoming rich ;)"),
	N_("This is really a good year!"),
};
static const char *chat_other_receive_many[] = {
	N_("You really don't deserve that much!"),
	N_("You don't know what to do with that many resources ;)"),
	N_("Ey, wait for my robber and lose all this again!"),
};
static const char *chat_self_moved_robber[] = {
	N_("Hehe!"),
	N_("Go, robber, go!"),
};
static const char *chat_moved_robber_to_me[] = {
	N_("You bastard!"),
	N_("Can't you move that robber somewhere else?!"),
	N_("Why always me??"),
};
static const char *chat_discard_self[] = {
	N_("Oh no!"),
	N_("Grrr!"),
	N_("Who the hell rolled that 7??"),
	N_("Why always me?!?"),
};
static const char *chat_discard_other[] = {
	N_("Say good bye to your cards... :)"),
	N_("*evilgrin*"),
	N_("/me says farewell to your cards ;)"),
	N_("That's the price for being rich... :)"),
};
static const char *chat_stole_from_me[] = {
	N_("Ey! Where's that card gone?"),
	N_("Thieves! Thieves!!"),
	N_("Wait for my revenge..."),
};
static const char *chat_monopoly_other[] = {
	N_("Oh no :("),
	N_("Must this happen NOW??"),
	N_("Args"),
};
static const char *chat_largestarmy_self[] = {
	N_("Hehe, my soldiers rule!"),
};
static const char *chat_largestarmy_other[] = {
	N_("First robbing us, then grabbing the points..."),
};
static const char *chat_longestroad_self[] = {
	N_("See that road!"),
};
static const char *chat_longestroad_other[] = {
	N_("Pf, you won't win with roads alone..."),
};

static float score_node_hurt_opponents(Node * node)
{
	/* no building there */
	if (node->owner == -1)
		return 0;

	/* do I have a house there? */
	if (my_player_num() == node->owner) {
		if (node->type == BUILD_SETTLEMENT) {
			return -2.0;
		} else {
			return -3.5;
		}
	}

	/* opponent has house there */
	if (node->type == BUILD_SETTLEMENT) {
		return 1.5;
	} else {
		return 2.5;
	}
}

/*
 * How much does putting the robber here hurt my opponents?
 */
static float score_hex_hurt_opponents(Hex * hex)
{
	int i;
	float score = 0;

	if (hex == NULL)
		return -1000;

	/* secord arg in can_robber_or_pirate_be_moved is not used.
	   don't move the pirate. */
	if (!can_robber_or_pirate_be_moved(hex, 0)
	    || hex->terrain == SEA_TERRAIN)
		return -1000;

	for (i = 0; i < 6; i++) {
		score += score_node_hurt_opponents(hex->nodes[i]);
	}

	/* multiply by resource/roll value */
	score *= default_score_hex(hex);

	return score;
}

/*
 * Find the best (worst for opponents) place to put the robber
 *
 */
static void greedy_place_robber(void)
{
	int i, j;
	float bestscore = -1000;
	Hex *besthex = NULL;
	int victim = -1;
	int victim_resources = -1;

	ai_wait();
	for (i = 0; i < map->x_size; i++) {
		for (j = 0; j < map->y_size; j++) {
			Hex *hex = map_hex(map, i, j);
			float score = score_hex_hurt_opponents(hex);

			if (score > bestscore) {
				bestscore = score;
				besthex = hex;
			}

		}
	}

	/* which opponent to steal from */
	for (i = 0; i < 6; i++) {
		int numres = 0;

		/* if has owner (and isn't me) */
		if ((besthex->nodes[i]->owner != -1) &&
		    (besthex->nodes[i]->owner != my_player_num())) {

			numres =
			    player_get_num_resource(besthex->nodes[i]->
						    owner);
		}

		if (numres > victim_resources) {
			victim = besthex->nodes[i]->owner;
			victim_resources = numres;
		}
	}
	cb_place_robber(besthex, victim);
	randchat(chat_self_moved_robber, 15);
}

/*
 * A devel card game us two free roads. let's build them
 *
 */

static void greedy_free_road(void)
{
	Edge *e;
	resource_values_t resval;

	reevaluate_resources(&resval);

	e = best_road_spot(&resval);

	if (e == NULL) {
		e = best_road_to_road(&resval);
	}

	if (e == NULL) {
		e = find_random_road();
	}

	if (e != NULL) {

		cb_build_road(e);
		return;

	} else {
		log_message(MSG_ERROR,
			    "unable to find spot to build free road\n");
		cb_disconnect();
	}
}

/*
 * We played a year of plenty card. pick the two resources we most need
 */

static void greedy_year_of_plenty(gint bank[NO_RESOURCE])
{
	gint want[NO_RESOURCE];
	gint assets[NO_RESOURCE];
	int i;
	int r1, r2;
	resource_values_t resval;

	ai_wait();
	for (i = 0; i < NO_RESOURCE; i++) {
		want[i] = 0;
		assets[i] = resource_asset(i);
	}

	/* what two resources do we desire most */
	reevaluate_resources(&resval);

	r1 = resource_desire(assets, &resval, NO_RESOURCE, 0);

	/* If we don't desire anything anymore, ask for a road.
	 * This happens if we have at least 2 of each resource
	 */
	if (r1 == NO_RESOURCE)
		r1 = BRICK_RESOURCE;

	assets[r1]++;

	reevaluate_resources(&resval);

	r2 = resource_desire(assets, &resval, NO_RESOURCE, 0);

	if (r2 == NO_RESOURCE)
		r2 = LUMBER_RESOURCE;

	assets[r1]--;

	/* If we want something that is not in the bank, request something else */
	/* WARNING: This code can cause a lockup if the bank is empty, but
	 * then the year of plenty must not have been playable */
	while (bank[r1] < 1)
		r1 = (r1 + 1) % NO_RESOURCE;
	while (bank[r2] < (r1 == r2 ? 2 : 1))
		r2 = (r2 + 1) % NO_RESOURCE;

	want[r1]++;
	want[r2]++;

	cb_choose_plenty(want);
}


/*
 * Of these resources which is least valuable to us
 *
 * Get rid of the one we have the most of
 * if there's a tie let resource_values_t settle it
 */

static int least_valuable(gint assets[NO_RESOURCE],
			  resource_values_t * resval)
{
	int ret = -1;
	int res;
	int most = 0;
	float mostval = -1;

	for (res = 0; res != NO_RESOURCE; res++) {
		if (assets[res] > most) {
			if (resval->value[res] > mostval) {
				ret = res;
				most = assets[res];
				mostval = resval->value[res];
			}
		}
	}

	return ret;
}

/*
 * Which resource do we desire the least?
 */

static int resource_desire_least(gint my_assets[NO_RESOURCE],
				 resource_values_t * resval)
{
	int res;
	gint assets[NO_RESOURCE];
	gint need[NO_RESOURCE];
	int leastval;

	/* make copy of what we got */
	for (res = 0; res != NO_RESOURCE; res++) {
		assets[res] = my_assets[res];
	}

	/* eliminate things we need to build stuff */
	if (has_resources(assets, BUILD_CITY, need)) {
		assets[ORE_RESOURCE] -= 3;
		assets[GRAIN_RESOURCE] -= 2;
	}

	if (has_resources(assets, BUILD_SETTLEMENT, need)) {
		assets[GRAIN_RESOURCE] -= 1;
		assets[BRICK_RESOURCE] -= 1;
		assets[WOOL_RESOURCE] -= 1;
		assets[LUMBER_RESOURCE] -= 1;
	}
	if (has_resources(assets, BUILD_ROAD, need)) {
		assets[BRICK_RESOURCE] -= 1;
		assets[LUMBER_RESOURCE] -= 1;
	}
	if (has_resources(assets, DEVEL_CARD, need)) {
		assets[GRAIN_RESOURCE] -= 1;
		assets[WOOL_RESOURCE] -= 1;
		assets[ORE_RESOURCE] -= 1;
	}

	/* of what's left what do do we care for least */
	leastval = least_valuable(assets, resval);
	if (leastval != -1)
		return leastval;

	/* otherwise least valuable of what we have in total */
	leastval = least_valuable(my_assets, resval);
	if (leastval != -1)
		return leastval;

	/* last resort just pick something */
	for (res = 0; res != NO_RESOURCE; res++) {
		if (my_assets[res] > 0)
			return res;
	}

	/* Should never get here */
	return 0;
}


/*
 * A seven was rolled. we need to discard some resources :(
 *
 */
static void greedy_discard(int num)
{
	int res;
	gint todiscard[NO_RESOURCE];
	int i;
	resource_values_t resval;
	gint assets[NO_RESOURCE];

	/* zero out */
	for (res = 0; res != NO_RESOURCE; res++) {
		todiscard[res] = 0;
		assets[res] = resource_asset(res);
	}

	for (i = 0; i < num; i++) {

		reevaluate_resources(&resval);

		res = resource_desire_least(assets, &resval);

		todiscard[res]++;
		assets[res]--;
	}

	cb_discard(todiscard);
}

/*
 * Domestic Trade
 *
 */
static int quote_next_num(void)
{
	return quote_num++;
}

static void greedy_quote_start(void)
{
	quote_num = 0;
}

static int trade_desired(gint assets[NO_RESOURCE], gint give, gint take)
{
	int i, n;
	int res = NO_RESOURCE;
	resource_values_t resval;
	float value = 0.0;
	gint need[NO_RESOURCE];

	/* don't give away cards we have only once */
	if (assets[give] <= 1) {
		return 0;
	}

	/* make it as if we don't have what we're trading away */
	assets[give] -= 1;

	for (n = 1; n <= 3; ++n) {
		/* do i need something more for something? */
		if (!has_resources(assets, BUILD_CITY, need)) {
			if ((res = which_resource(need)) == take
			    && need[res] == n)
				break;
		}
		if (!has_resources(assets, BUILD_SETTLEMENT, need)) {
			if ((res = which_resource(need)) == take
			    && need[res] == n)
				break;
		}
		if (!has_resources(assets, BUILD_ROAD, need)) {
			if ((res = which_resource(need)) == take
			    && need[res] == n)
				break;
		}
		if (!has_resources(assets, DEVEL_CARD, need)) {
			if ((res = which_resource(need)) == take
			    && need[res] == n)
				break;
		}
	}
	assets[give] += 1;
	if (n <= 3)
		return n;

	/* desire the one we don't produce the most */
	reevaluate_resources(&resval);
	for (i = 0; i < NO_RESOURCE; i++) {
		if ((resval.value[i] > value) && (assets[i] < 2)) {
			res = i;
			value = resval.value[i];
		}
	}

	if (res == take && assets[give] > 2) {
		return 1;
	}

	return 0;
}

static void greedy_consider_quote(UNUSED(gint partner),
				  gint we_receive[NO_RESOURCE],
				  gint we_supply[NO_RESOURCE])
{
	gint give, take, ntake;
	gint give_res[NO_RESOURCE], take_res[NO_RESOURCE],
	    my_assets[NO_RESOURCE];
	int i;

	for (i = 0; i < NO_RESOURCE; ++i)
		my_assets[i] = resource_asset(i);

	for (give = 0; give < NO_RESOURCE; give++) {
		if (!we_supply[give])
			continue;
		if (!my_assets[give]) {
			continue;
		}
		for (take = 0; take < NO_RESOURCE; take++) {
			if (!we_receive[take])
				continue;
			if ((ntake =
			     trade_desired(my_assets, give, take)) > 0)
				goto doquote;
		}
	}

	log_message(MSG_INFO, _("Rejecting trade.\n"));
	cb_end_quote();
	return;

      doquote:
	for (i = 0; i < NO_RESOURCE; ++i) {
		give_res[i] = give == i ? 1 : 0;
		take_res[i] = take == i ? ntake : 0;
	}
	cb_quote(quote_next_num(), give_res, take_res);
	log_message(MSG_INFO, "Quoting.\n");
}

/* function for checking if the map contains gold, so the ai can
 * quit if it does */
static gboolean greedy_check_gold(UNUSED(Map * map), Hex * hex,
				  UNUSED(void *unused))
{
	return hex->terrain == GOLD_TERRAIN;
}

static void greedy_setup(unsigned num_settlements, unsigned num_roads)
{
	ai_wait();
	if (num_settlements > 0)
		greedy_setup_house();
	else if (num_roads > 0)
		greedy_setup_road();
	else
		cb_end_turn();
}

static void greedy_roadbuilding(gint num_roads)
{
	ai_wait();
	if (num_roads > 0)
		greedy_free_road();
	else
		cb_end_turn();
}

static void greedy_discard_start(void)
{
	discard_starting = TRUE;
}

static void greedy_discard_add(gint player_num, gint discard_num)
{
	if (player_num == my_player_num()) {
		randchat(chat_discard_self, 10);
		ai_wait();
		greedy_discard(discard_num);
	} else {
		if (discard_starting) {
			discard_starting = FALSE;
			randchat(chat_discard_other, 10);
		}
	}
}

static void greedy_error(gchar * message)
{
	gchar buffer[1000];
	snprintf(buffer, sizeof(buffer),
		 _("Received error from server: %s.  Quitting\n"),
		 message);
	cb_chat(buffer);
	cb_disconnect();
}

static void greedy_game_over(gint player_num, UNUSED(gint points))
{
	if (player_num == my_player_num())
		ai_chat(N_("Yippie!"));
	else
		ai_chat(N_("My congratulations"));
	cb_disconnect();
}

/* functions for chatting follow */
static void greedy_player_turn(gint player)
{
	if (player == my_player_num())
		randchat(chat_turn_start, 70);
}

static void greedy_robber_moved(UNUSED(Hex * old), Hex * new)
{
	int idx;
	gboolean iam_affected = FALSE;
	for (idx = 0; idx < numElem(new->nodes); idx++) {
		if (new->nodes[idx]->owner == my_player_num())
			iam_affected = TRUE;
	}
	if (iam_affected)
		randchat(chat_moved_robber_to_me, 20);
}

static void greedy_player_robbed(UNUSED(gint robber_num), gint victim_num,
				 UNUSED(Resource resource))
{
	if (victim_num == my_player_num())
		randchat(chat_stole_from_me, 15);
}

static void greedy_get_rolled_resources(gint player_num,
					const gint * resources)
{
	gint total = 0, i;
	for (i = 0; i < NO_RESOURCE; ++i)
		total += resources[i];
	if (player_num == my_player_num()) {
		if (total == 1)
			randchat(chat_receive_one, 60);
		else if (total >= 3)
			randchat(chat_receive_many, 20);
	} else if (total >= 3)
		randchat(chat_other_receive_many, 30);
}

static void greedy_played_develop(gint player_num, UNUSED(gint card_idx),
				  DevelType type)
{
	if (player_num != my_player_num() && type == DEVEL_MONOPOLY)
		randchat(chat_monopoly_other, 20);
}

static void greedy_new_statistics(gint player_num, StatisticType type,
				  gint num)
{
	if (num != 1)
		return;
	if (type == STAT_LONGEST_ROAD) {
		if (player_num == my_player_num())
			randchat(chat_longestroad_self, 10);
		else
			randchat(chat_longestroad_other, 10);
	} else if (type == STAT_LARGEST_ARMY) {
		if (player_num == my_player_num())
			randchat(chat_largestarmy_self, 10);
		else
			randchat(chat_largestarmy_other, 10);
	}
}

void greedy_init(UNUSED(int argc), UNUSED(char **argv))
{
	map = get_map();

	/* ai cannot handle gold: quit if the board contains it */
	if (map_traverse(map, &greedy_check_gold, NULL) == TRUE) {
		cb_chat(N_
			("Sorry, I do not know how to play with gold.\n"));
		exit(1);
	}

	callbacks.setup = &greedy_setup;
	callbacks.turn = &greedy_turn;
	callbacks.robber = &greedy_place_robber;
	callbacks.roadbuilding = &greedy_roadbuilding;
	callbacks.plenty = &greedy_year_of_plenty;
	callbacks.discard_add = &greedy_discard_add;
	callbacks.quote_start = &greedy_quote_start;
	callbacks.quote = &greedy_consider_quote;
	callbacks.game_over = &greedy_game_over;
	callbacks.error = &greedy_error;

	/* chatting */
	callbacks.player_turn = &greedy_player_turn;
	callbacks.robber_moved = &greedy_robber_moved;
	callbacks.discard = &greedy_discard_start;
	callbacks.player_robbed = &greedy_player_robbed;
	callbacks.get_rolled_resources = &greedy_get_rolled_resources;
	callbacks.played_develop = &greedy_played_develop;
	callbacks.new_statistics = &greedy_new_statistics;
}