/* Pioneers - Implementation of the excellent Settlers of Catan board game.
 *   Go buy a copy.
 *
 * Copyright (C) 1999 Dave Cole
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

/* Pioneers Console Server
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#ifndef HAVE_GLIB_2_6
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#endif
#include <glib.h>

#include "driver.h"
#include "game.h"
#include "cards.h"
#include "map.h"
#include "network.h"
#include "log.h"
#include "buildrec.h"
#include "server.h"

#include "glib-driver.h"

#include "admin.h"

static GMainLoop *event_loop;

#ifndef HAVE_GLIB_2_6
static void usage(void)
{
	fprintf(stderr,
		"Usage: pioneers-server-console [options]\n"
		"  -a port   --  Admin port to listen on\n"
		"  -c num    --  Start num computer players\n"
		"  -g game   --  Game name to use\n"
		"  -h        --  Show this help\n"
		"  -k secs   --  Kill after 'secs' seconds with no players\n"
		"  -m meta   --  Register at meta-server name (implies -r)\n"
		"  -n name   --  Use this hostname when registering\n"
		"  -P num    --  Set Number of players\n"
		"  -p port   --  Port to listen on\n"
		"  -r        --  Register server with meta-server\n"
		"  -R 0|1|2  --  Set seven-rule handling\n"
		"  -s        --  Don't start game immediately, wait for a "
		"command on admin port\n"
		"  -t mins   --  Tournament mode, ai players added after "
		"'mins' minutes\n"
		"  -T 0|1    --  select terrain type, 0=default 1=random\n"
		"  -v points --  Number of points needed to win\n"
		"  -x        --  Quit after a player has won\n");

	exit(1);
}
#endif

#ifdef HAVE_GLIB_2_6
static gint num_players = 0;
static gint num_points = 0;
static gint sevens_rule = -1;
static gint terrain = -1;
static gint timeout = 0;
static gint num_ai_players = 0;
static gchar *server_port = NULL;
static gchar *admin_port = NULL;
static gchar *game_title = NULL;
static gboolean disable_game_start = FALSE;
static gint tournament_time = -1;
static gboolean quit_when_done = FALSE;
static gchar *hostname = NULL;
static gboolean register_server = FALSE;
static gchar *metaname = NULL;

static GOptionEntry commandline_game_entries[] = {
	{"game-title", 'g', 0, G_OPTION_ARG_STRING, &game_title,
	 /* Commandline server-console: game-title */
	 N_("Game title to use"), NULL},
	{"port", 'p', 0, G_OPTION_ARG_STRING, &server_port,
	 /* Commandline server-console: port */
	 N_("Port to listen on"), PIONEERS_DEFAULT_GAME_PORT},
	{"players", 'P', 0, G_OPTION_ARG_INT, &num_players,
	 /* Commandline server-console: players */
	 N_("Override number of players"), NULL},
	{"points", 'v', 0, G_OPTION_ARG_INT, &num_points,
	 /* Commandline server-console: points */
	 N_("Override number of points needed to win"), NULL},
	{"seven-rule", 'R', 0, G_OPTION_ARG_INT, &sevens_rule,
	 /* Commandline server-console: seven-rule */
	 N_("Override seven-rule handling"), "0|1|2"},
	{"terrain", 'T', 0, G_OPTION_ARG_INT, &terrain,
	 /* Commandline server-console: terrain */
	 N_("Override terrain type, 0=default 1=random"), "0|1"},
	{"computer-players", 'c', 0, G_OPTION_ARG_INT, &num_ai_players,
	 /* Commandline server-console: computer-players */
	 N_("Add N computer players"), "N"},
	{NULL, '\0', 0, 0, NULL, NULL, NULL}
};

static GOptionEntry commandline_meta_entries[] = {
	{"register", 'r', 0, G_OPTION_ARG_NONE, &register_server,
	 /* Commandline server-console: register */
	 N_("Register server with meta-server"), NULL},
	{"meta-server", 'm', 0, G_OPTION_ARG_STRING, &metaname,
	 /* Commandline server-console: meta-server */
	 N_("Register at meta-server name (implies -r)"),
	 PIONEERS_DEFAULT_META_SERVER},
	{"hostname", 'n', 0, G_OPTION_ARG_STRING, &hostname,
	 /* Commandline server-console: hostname */
	 N_("Use this hostname when registering"), NULL},
	{NULL, '\0', 0, 0, NULL, NULL, NULL}
};

static GOptionEntry commandline_other_entries[] = {
	{"auto-quit", 'x', 0, G_OPTION_ARG_NONE, &quit_when_done,
	 /* Commandline server-console: auto-quit */
	 N_("Quit after a player has won"), NULL},
	{"empty-timeout", 'k', 0, G_OPTION_ARG_INT, &timeout,
	 /* Commandline server-console: empty-timeout */
	 N_("Quit after N seconds with no players"), "N"},
	{"tournament", 't', 0, G_OPTION_ARG_INT, &tournament_time,
	 /* Commandline server-console: tournament */
	 N_("Tournament mode, computer players added after N minutes"),
	 "N"},
	{"admin-port", 'a', 0, G_OPTION_ARG_STRING, &admin_port,
	 /* Commandline server-console: admin-port */
	 N_("Admin port to listen on"), PIONEERS_DEFAULT_ADMIN_PORT},
	{"admin-wait", 's', 0, G_OPTION_ARG_NONE, &disable_game_start,
	 /* Commandline server-console: admin-wait */
	 N_
	 ("Don't start game immediately, wait for a command on admin port"),
	 NULL},
	{NULL, '\0', 0, 0, NULL, NULL, NULL}
};
#endif				/* HAVE_GLIB_2_6 */

int main(int argc, char *argv[])
{
	int i;
#ifdef HAVE_GLIB_2_6
	GOptionContext *context;
	GOptionGroup *context_group;
	GError *error = NULL;
#else				/* HAVE_GLIB_2_6 */
	int c;
	gint num_players = 0, num_points = 0,
	    sevens_rule = -1, terrain = -1, timeout = 0, num_ai_players =
	    0;
	gchar *server_port = g_strdup(PIONEERS_DEFAULT_GAME_PORT);
	gchar *admin_port = g_strdup(PIONEERS_DEFAULT_ADMIN_PORT);

	gboolean disable_game_start = FALSE;
	gint tournament_time = -1;
	gboolean quit_when_done = FALSE;
	gchar *hostname = NULL;
	gboolean register_server = FALSE;
#endif				/* HAVE_GLIB_2_6 */

	/* set the UI driver to Glib_Driver, since we're using glib */
	set_ui_driver(&Glib_Driver);
	driver->player_added = srv_glib_player_added;
	driver->player_renamed = srv_glib_player_renamed;
	driver->player_removed = srv_player_removed;

	driver->player_change = srv_player_change;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	/* have gettext return strings in UTF-8 */
	bind_textdomain_codeset(PACKAGE, "UTF-8");

	server_init();

#if HAVE_GLIB_2_6
	/* Long description in the commandline for server-console: help */
	context = g_option_context_new(_("- Host a game of Pioneers"));
	g_option_context_add_main_entries(context,
					  commandline_game_entries,
					  PACKAGE);
	context_group = g_option_group_new("meta",
					   /* Commandline server-console: Short description of meta group */
					   _("Meta-server Options"),
					   /* Commandline server-console: Long description of meta group */
					   _
					   ("Options for the meta-server"),
					   NULL, NULL);
	g_option_group_set_translation_domain(context_group, PACKAGE);
	g_option_group_add_entries(context_group,
				   commandline_meta_entries);
	g_option_context_add_group(context, context_group);
	context_group = g_option_group_new("misc",
					   /* Commandline server-console: Short description of misc group */
					   _("Miscellaneous Options"),
					   /* Commandline server-console: Long description of misc group */
					   _("Miscellaneous options"),
					   NULL, NULL);
	g_option_group_set_translation_domain(context_group, PACKAGE);
	g_option_group_add_entries(context_group,
				   commandline_other_entries);
	g_option_context_add_group(context, context_group);
	g_option_context_parse(context, &argc, &argv, &error);
	if (error != NULL) {
		g_print("%s\n", error->message);
		g_error_free(error);
		return 1;
	};

	if (server_port == NULL)
		server_port = g_strdup(PIONEERS_DEFAULT_GAME_PORT);
	if (admin_port == NULL)
		admin_port = g_strdup(PIONEERS_DEFAULT_ADMIN_PORT);
	if (game_title == NULL)
		cfg_set_game("Default");
	else
		cfg_set_game(game_title);
	if (meta_server_name != NULL)
		register_server = TRUE;
	if (sevens_rule != -1)
		sevens_rule = CLAMP(sevens_rule, 0, 2);
#else				/* HAVE_GLIB_2_6 */
	while ((c =
		getopt(argc, argv,
		       "a:c:g:hk:m:n:P:p:rR:st:T:v:x")) != EOF) {
		switch (c) {
		case 'a':
			if (!optarg) {
				break;
			}
			g_free(admin_port);
			admin_port = g_strdup(optarg);
			break;
		case 'c':
			if (!optarg) {
				break;
			}
			num_ai_players = atoi(optarg);
			break;
		case 'g':
			cfg_set_game(optarg);
			break;
		case 'k':
			if (!optarg) {
				break;
			}
			timeout = atoi(optarg);
			break;
		case 'm':
			if (!optarg) {
				break;
			}
			meta_server_name = g_strdup(optarg);
			register_server = TRUE;
			break;
		case 'n':
			if (!optarg) {
				break;
			}
			hostname = g_strdup(optarg);
			break;
		case 'P':
			if (!optarg) {
				break;
			}
			num_players = atoi(optarg);
			break;
		case 'p':
			if (!optarg) {
				break;
			}
			g_free(server_port);
			server_port = g_strdup(optarg);
			break;
		case 'R':
			if (!optarg) {
				break;
			}
			sevens_rule = atoi(optarg);
			if (sevens_rule < 0 || sevens_rule > 2) {
				sevens_rule = 0;
			}
			break;
		case 'r':
			register_server = TRUE;
			break;
		case 's':
			disable_game_start = TRUE;
			break;
		case 't':
			if (!optarg) {
				usage();
			}
			tournament_time = atoi(optarg);
			break;
		case 'T':
			if (!optarg) {
				break;
			}
			terrain = atoi(optarg);
			break;
		case 'v':
			if (!optarg) {
				usage();
			}
			num_points = atoi(optarg);
			break;
		case 'x':
			quit_when_done = TRUE;
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}
#endif				/* HAVE_GLIB_2_6 */

	g_assert(server_port != NULL);
	g_assert(admin_port != NULL);

	if (num_players) {
		cfg_set_num_players(CLAMP(num_players, 2, MAX_PLAYERS));
	}

	if (sevens_rule != -1) {
		cfg_set_sevens_rule(sevens_rule);
	}

	if (num_points) {
		cfg_set_victory_points(MAX(3, num_points));
	}

	if (tournament_time != -1) {
		cfg_set_tournament_time(tournament_time);
	}

	if (quit_when_done) {
		cfg_set_quit(quit_when_done);
	}

	if (terrain != -1) {
		cfg_set_terrain_type(terrain ? 1 : 0);
	}

	if (timeout) {
		cfg_set_timeout(timeout);
	}

	net_init();

	admin_listen(admin_port);

	if (!disable_game_start) {
		if (start_server(hostname, server_port, register_server)) {
			for (i = 0; i < num_ai_players; ++i)
				new_computer_player(NULL, server_port,
						    TRUE);

			event_loop = g_main_loop_new(NULL, FALSE);
			g_main_loop_run(event_loop);
			g_main_loop_unref(event_loop);

		} else {
#ifndef HAVE_GLIB_2_6
			usage();
#endif
		}

	} else {
		/* Ugly... But needed to preserve the original functionality
		   if the disable_game_start flag is set... Even if it doesn't
		   really -do- anything. */
		for (i = 0; i < num_ai_players; ++i)
			new_computer_player(NULL, server_port, TRUE);

		event_loop = g_main_loop_new(NULL, FALSE);
		g_main_loop_run(event_loop);
		g_main_loop_unref(event_loop);
	}

	net_finish();

	g_free(hostname);
	g_free(server_port);
	g_free(admin_port);
#ifdef HAVE_GLIB_2_6
	g_option_context_free(context);
#endif
	server_cleanup_static_data();
	return 0;
}

static gboolean exit_func(G_GNUC_UNUSED gpointer data)
{
	g_main_loop_quit(event_loop);
	return TRUE;
}

void game_is_over(Game * game)
{
	/* quit in ten seconds if configured */
	if (game->params->quit_when_done) {
		g_timeout_add(10 * 1000, &exit_func, NULL);
	}
}
