/* Pioneers - Implementation of the excellent Settlers of Catan board game.
 *   Go buy a copy.
 *
 * Copyright (C) 1999 Dave Cole
 * Copyright (C) 2003 Bas Wijnen <shevek@fmf.nl>
 * Copyright (C) 2004 Roland Clobus <rclobus@bigfoot.com>
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

#include "config.h"
#include "frontend.h"
#ifdef HAVE_LIBGNOME
#include <libgnome/libgnome.h>
#endif
#include "common_gtk.h"
#include "config-gnome.h"
#include "theme.h"
#include "histogram.h"

static gboolean have_dlg = FALSE;
static gboolean connectable = FALSE;

static const gchar *server = NULL;
static const gchar *port = NULL;
static const gchar *name = NULL;
static gboolean quit_when_offline = FALSE;
#ifdef ENABLE_NLS
static const gchar *override_language = NULL;
#endif

#ifdef HAVE_LIBGNOME
const struct poptOption options[] = {
	/* Commandline option of client: hostname of the server */
	{"server", 's', POPT_ARG_STRING, &server, 0, N_("Server Host"),
	 PIONEERS_DEFAULT_GAME_HOST},
	/* Commandline option of client: port of the server */
	{"port", 'p', POPT_ARG_STRING, &port, 0, N_("Server Port"),
	 PIONEERS_DEFAULT_GAME_PORT},
	/* Commandline option of client: name of the player */
	{"name", 'n', POPT_ARG_STRING, &name, 0, N_("Player name"), NULL},
#ifdef ENABLE_NLS
	/* Commandline option of client: override the language */
	{"language", '\0', POPT_ARG_STRING, &override_language, 0,
	 N_("Override the language of the system"), "en " ALL_LINGUAS},
#endif
	{NULL, '\0', 0, 0, 0, NULL, NULL}
};
#endif

static void frontend_offline_start_connect_cb(void)
{
	connect_create_dlg();
	have_dlg = TRUE;
	frontend_gui_update();
}

/* gui function to handle gui events in offline mode */
static void frontend_offline_gui(GuiEvent event)
{
	switch (event) {
	case GUI_UPDATE:
		frontend_gui_check(GUI_CONNECT_TRY, TRUE);
		frontend_gui_check(GUI_CONNECT, !have_dlg && connectable);
		frontend_gui_check(GUI_DISCONNECT, !have_dlg
				   && !connectable);
		break;
	case GUI_CONNECT_TRY:
		gui_show_splash_page(FALSE);
		gui_set_net_status(_("Connecting"));

		connectable = FALSE;
		have_dlg = FALSE;
		cb_name_change(connect_get_name());
		cb_connect(connect_get_server(), connect_get_port());
		frontend_gui_update();
		break;
	case GUI_CONNECT:
		frontend_offline_start_connect_cb();
		break;
	case GUI_CONNECT_CANCEL:
		have_dlg = FALSE;
		frontend_gui_update();
		break;
	default:
		break;
	}
}

/* this function is called when offline mode is entered. */
void frontend_offline()
{
	gui_cursor_none();	/* Clear possible cursor */

	connectable = TRUE;
	if (have_dlg)
		return;

	/* set the callback for gui events */
	set_gui_state(frontend_offline_gui);

	if (quit_when_offline) {
		route_gui_event(GUI_QUIT);
	}

	/* Commandline overrides the dialog */
	if (port && server) {
		connectable = FALSE;
		gui_show_splash_page(FALSE);
		cb_connect(server, port);
		quit_when_offline = TRUE;
		port = NULL;
		server = NULL;
		frontend_gui_update();
	} else {
		frontend_offline_start_connect_cb();
	}
}

/* this function is called to let the frontend initialize itself. */
void frontend_init(int argc, char **argv)
{
	GtkWidget *app;
	gboolean default_returned;
#if ENABLE_NLS
	lang_desc *ld;
#endif

	frontend_gui_register_init();

	set_ui_driver(&GTK_Driver);

	config_init("pioneers");

#ifdef HAVE_LIBGNOME
	gnome_program_init(PACKAGE, VERSION,
			   LIBGNOME_MODULE,
			   argc, argv,
			   GNOME_PARAM_POPT_TABLE, options,
			   GNOME_PARAM_APP_DATADIR, DATADIR, NULL);
#endif

	/* Initialize the widget set */
	gtk_init(&argc, &argv);

#if ENABLE_NLS
	/* Gtk+ handles the locale, we must bind the translations */
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	bind_textdomain_codeset(PACKAGE, "UTF-8");

	/* Override the language if it is set in the command line */
	if (override_language && (ld = find_lang_desc(override_language)))
		change_nls(ld);
#endif

	/* Create the application window
	 */
	themes_init();
	settings_init();
	histogram_init();
	app = gui_build_interface();

	callbacks.mainloop = &gtk_main;

	/* in theory, all windows are created now... 
	 *   set logging to message window */
	log_set_func_message_window();

	if (!name) {
		name =
		    config_get_string("connect/name", &default_returned);
		if (default_returned) {
			name = g_strdup(g_get_user_name());
		}
	}
	cb_name_change(name);

	if (server && port) {
		/* Both are set, do nothing here */
	} else if (server || port) {
		g_warning(_
			  ("Only server or port set, ignoring command line"));
		server = NULL;
		port = NULL;
	}
}