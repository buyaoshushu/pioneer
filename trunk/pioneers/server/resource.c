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

#include <gnome.h>

#include "game.h"
#include "cards.h"
#include "map.h"
#include "buildrec.h"
#include "network.h"
#include "cost.h"
#include "log.h"
#include "server.h"

gboolean resource_available(Player *player, gint *resources, gint *num_in_bank)
{
	Game *game = player->game;
	gint idx;

	if (num_in_bank != NULL)
		*num_in_bank = 0;
	for (idx = 0; idx < NO_RESOURCE; idx++) {
		if (resources[idx] > game->bank_deck[idx]) {
			sm_send(player->sm, "ERR no-cards %d\n", idx);
			return FALSE;
		}
		if (num_in_bank != NULL)
			*num_in_bank += game->bank_deck[idx];
	}

	return TRUE;
}

void resource_start(Game *game)
{
	GList *list;

	for (list = player_first_real(game);
	     list != NULL; list = player_next_real(list)) {
		Player *player = list->data;

		memcpy(player->prev_assets,
		       player->assets, sizeof(player->assets));
	}
}

void resource_end(Game *game, gchar *action, gint mult)
{
	GList *list;

	for (list = player_first_real(game);
	     list != NULL; list = player_next_real(list)) {
		Player *player = list->data;
		gint resource[NO_RESOURCE];
		int idx;
		gboolean send_message = FALSE;

		for (idx = 0; idx < numElem(player->assets); idx++) {
			gint num;

			num = player->assets[idx] - player->prev_assets[idx];
			if (game->bank_deck[idx] - num < 0) {
				num = game->bank_deck[idx];
				player->assets[idx]
					= player->prev_assets[idx] + num;
			}

			resource[idx] = num;
			if (num != 0)
				send_message = TRUE;

			game->bank_deck[idx] -= num;
		}

		if (send_message) {
			for (idx = 0; idx < NO_RESOURCE; idx++)
				resource[idx] *= mult;
			player_broadcast(player, PB_ALL, "%s %R\n", action, resource);
		}
	}
}

void resource_spend(Player *player, gint *cost)
{
	Game *game = player->game;

	resource_start(game);
	cost_buy(cost, player->assets);
	resource_end(game, "spent", -1);
}

void resource_refund(Player *player, gint *cost)
{
	Game *game = player->game;

	resource_start(game);
	cost_refund(cost, player->assets);
	resource_end(game, "refund", 1);
}
