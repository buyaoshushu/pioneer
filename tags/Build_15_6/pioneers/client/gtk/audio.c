/* Pioneers - Implementation of the excellent Settlers of Catan board game.
 *   Go buy a copy.
 *
 * Copyright (C) 2020 Roland Clobus <rclobus@rclobus.nl>
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
#include "audio.h"
#include "log.h"

#ifdef HAVE_CANBERRA
#include <canberra.h>
#endif

static gboolean silent_mode = FALSE;
static gboolean announce_player = TRUE;

#ifdef HAVE_CANBERRA
static ca_context *canberra_context = NULL;
#endif

gboolean get_announce_player(void)
{
	return announce_player;
}

void set_announce_player(gboolean announce)
{
	announce_player = announce;
}

gboolean get_silent_mode(void)
{
	return silent_mode;
}

void set_silent_mode(gboolean silent)
{
	silent_mode = silent;
}

#ifdef HAVE_CANBERRA
void play_sound(SoundType sound)
{
	int result;

	if (get_silent_mode()) {
		return;
	}
	if (canberra_context == NULL) {
		result = ca_context_create(&canberra_context);
		if (result != 0) {
			log_message(MSG_ERROR,
				    _("Error starting %s: %s\n"),
				    "ca_canberra_create",
				    ca_strerror(result));
			set_silent_mode(TRUE);
		} else {
			(void) ca_context_change_props(canberra_context,
						       CA_PROP_APPLICATION_NAME,
						       _("Pioneers"),
						       CA_PROP_MEDIA_ROLE,
						       "game",
						       CA_PROP_CANBERRA_CACHE_CONTROL,
						       "volatile", NULL);
		}
	}
	if (!get_silent_mode()) {
		switch (sound) {
		case SOUND_BEEP:
			ca_context_play(canberra_context, 0,
					CA_PROP_EVENT_ID, "service-login",
					NULL);
			break;
		case SOUND_TURN:
			ca_context_play(canberra_context, 0,
					CA_PROP_EVENT_ID, "message-sent",
					NULL);
			break;
		case SOUND_ANNOUNCE:
			if (get_announce_player())
				ca_context_play(canberra_context, 0,
						CA_PROP_EVENT_ID,
						"device-added", NULL);
			break;
		}
	}
}
#else
static void do_beep(guint frequency)
{
	gchar *argv[4];
	GError *error = NULL;
	guint i;
	guint n = 0;

	argv[n++] = g_strdup("beep");
	argv[n++] = g_strdup("-f");
	argv[n++] = g_strdup_printf("%u", frequency);
	argv[n++] = NULL;
	g_assert(n <= G_N_ELEMENTS(argv));

	if (!g_spawn_async
	    (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL,
	     &error)) {
		/* Beep is not working, turn on silent mode */
		set_silent_mode(TRUE);
		log_message(MSG_ERROR, _("Error starting %s: %s\n"),
			    argv[0], error->message);
		g_error_free(error);
	}
	for (i = 0; argv[i] != NULL; i++)
		g_free(argv[i]);
}

void play_sound(SoundType sound)
{
	if (get_silent_mode()) {
		return;
	}
	switch (sound) {
	case SOUND_BEEP:
		do_beep(440);
		break;
	case SOUND_TURN:
		do_beep(440);
		break;
	case SOUND_ANNOUNCE:
		if (get_announce_player())
			do_beep(880);
		break;
	}
}
#endif
