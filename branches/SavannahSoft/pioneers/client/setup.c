/*
 * Gnocatan: a fun game.
 * (C) 1999 the Free Software Foundation
 *
 * Author: Dave Cole.
 *
 * Implementation of the excellent Settlers of Catan board game.  Go
 * buy a copy.
 */
#include <ctype.h>

#include <gnome.h>

#include "game.h"
#include "cards.h"
#include "map.h"
#include "gui.h"
#include "network.h"
#include "player.h"
#include "log.h"
#include "client.h"

static gboolean double_setup;

gboolean is_setup_double()
{
	return double_setup;
}

gboolean setup_can_undo()
{
	return build_can_undo();
}

gboolean setup_can_build_road()
{
	if (double_setup)
		return (build_count(BUILD_ROAD) +
			build_count(BUILD_SHIP)) < 2;
	else
		return (build_count(BUILD_ROAD) +
			build_count(BUILD_SHIP)) < 1;
}

gboolean setup_can_build_ship()
{
	if (double_setup)
		return (build_count(BUILD_SHIP) + build_count(BUILD_ROAD)) < 2;
	else
		return (build_count(BUILD_SHIP) + build_count(BUILD_ROAD)) < 1;
}

gboolean setup_can_build_settlement()
{
	if (double_setup)
		return build_count(BUILD_SETTLEMENT) < 2;
	else
		return build_count(BUILD_SETTLEMENT) < 1;
}

gboolean setup_can_finish()
{
	if (double_setup)
		return (build_count(BUILD_ROAD) + build_count(BUILD_SHIP)) == 2
			&& build_count(BUILD_SETTLEMENT) == 2
			&& build_is_valid();
	else
		return (build_count(BUILD_ROAD) + build_count(BUILD_SHIP)) == 1
			&& build_count(BUILD_SETTLEMENT) == 1
			&& build_is_valid();
}

/* Place some restrictions on road placement during setup phase
 */
gboolean setup_check_road(Edge *edge, gint owner)
{
	return build_can_setup_road(edge, double_setup);
}

/* Place some restrictions on ship placement during setup phase
 */
gboolean setup_check_ship(Edge *edge, gint owner)
{
	return build_can_setup_ship(edge, double_setup);
}

/* Place some restrictions on settlement placement during setup phase
 */
gboolean setup_check_settlement(Node *node, gint owner)
{
	return build_can_setup_settlement(node, double_setup);
}

void setup_begin(gint player_num)
{
	log_info(_("Setup for %s.\n"), player_name(player_num, FALSE));
	player_set_current(player_num);
	if (player_num != my_player_num())
		return;

	double_setup = FALSE;
	build_clear();
}

void setup_begin_double(gint player_num)
{
	log_info(_("Double setup for %s.\n"), player_name(player_num, FALSE));
	player_set_current(player_num);
	if (player_num != my_player_num())
		return;

	double_setup = TRUE;
	build_clear();
}
