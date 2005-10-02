/* Pioneers - Implementation of the excellent Settlers of Catan board game.
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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

static const char *server = PIONEERS_DEFAULT_GAME_HOST;
static const char *port = PIONEERS_DEFAULT_GAME_PORT;
static char *ai;
static int waittime = 1000;
static int local_argc;
static char **local_argv;
static gboolean silent = FALSE;

static gchar *random_name(void)
{
	gchar *filename;
	FILE *stream;
	char line[512];
	gchar *name = NULL;
	int num = 1;

	filename =
	    g_build_filename(get_pioneers_dir(), "computer_names", NULL);
	stream = fopen(filename, "r");
	if (!stream) {
		g_warning("Unable to open %s", filename);
		/* Default name for the AI when the computer_names file
		 * is not found or empty.
		 */
		name = g_strdup(_("Computer Player"));
	} else {
		while (fgets(line, sizeof(line) - 1, stream)) {
			if (g_random_int_range(0, num) == 0) {
				if (name)
					g_free(name);
				name = g_strdup(line);
			}
			num++;
		}
		fclose(stream);
		if (num == 1) {
			g_warning("Empty file: %s", filename);
			/* Default name for the AI when the computer_names file
			 * is not found or empty.
			 */
			name = g_strdup(_("Computer Player"));
		}
	}
	g_free(filename);
	return name;
}

static void usage(int retval)
{
	printf("Usage: pioneersai [args]\n"
	       "\n"
	       "s - server\n"
	       "p - port\n"
	       "n - computer name (leave absent for random name)\n"
	       "a - AI player (possible values: greedy)\n"
	       "t - time to wait between turns (in milliseconds; default 1000)\n"
	       "c - stop computer players from talking\n");
	exit(retval);
}

UIDriver Glib_Driver;

/* this needs some tweaking.  It would be nice if anything not handled by
 * the AI program can be handled by the AI implementation that is playing.
 * -c is typically an option which should not be handled globally */
static void ai_init(int argc, char **argv)
{
	int c;
	char *name = NULL;

	local_argc = argc;
	local_argv = argv;

	while ((c = getopt(argc, argv, "s:p:n:a:t:ch")) != EOF) {
		switch (c) {
		case 'c':
			silent = TRUE;
			break;
		case 's':
			server = optarg;
			break;
		case 'p':
			port = optarg;
			break;
		case 'n':
			name = g_strdup(optarg);
			break;
		case 'a':
			ai = optarg;
			break;
		case 't':
			waittime = atoi(optarg);
			break;
		case 'h':
			usage(0);
			/* does not return */
		default:
			usage(1);
			/* does not return */
		}
	}

	printf("ai port is %s\n", port);

	g_random_set_seed(time(NULL) + getpid());

	if (!name) {
		name = random_name();
	}
	cb_name_change(name);
	g_free(name);

	set_ui_driver(&Glib_Driver);
	log_set_func_default();
}

static void ai_quit(void)
{
	exit(0);
}

static void ai_offline(void)
{
	callbacks.offline = &ai_quit;
	cb_connect(server, port);
}

static void ai_start_game(void)
{
	if (player_is_viewer(my_player_num())) {
		cb_chat(N_("The game is already full. I'm leaving."));
		exit(1);
	}

	/** @todo choose which ai implementation to use */
	greedy_init(local_argc, local_argv);
}

void ai_wait(void)
{
	g_usleep(waittime * 1000);
}

void ai_chat(const char *message)
{
	if (!silent)
		cb_chat(message);
}

void frontend_set_callbacks(void)
{
	callbacks.init = &ai_init;
	callbacks.offline = &ai_offline;
	callbacks.start_game = &ai_start_game;
}
