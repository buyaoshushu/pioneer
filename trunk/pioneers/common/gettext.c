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
#ifdef ENABLE_NLS
#include <glib.h>
#include <libintl.h>
#ifdef HAVE_SETLOCALE
#include <locale.h>
#endif
#endif

#include "gettext.h"

void gettext_init(void)
{
#ifdef ENABLE_NLS
#ifdef HAVE_SETLOCALE
	setlocale(LC_ALL, "");
#endif
#ifdef G_OS_WIN32
	gchar *root_glib_encoding =
	    g_win32_get_package_installation_directory_of_module(NULL);
	gchar *full_path_glib_encoding =
	    g_build_filename(root_glib_encoding, LOCALEDIR, NULL);
	gchar *full_path_local_encoding =
	    g_win32_locale_filename_from_utf8(full_path_glib_encoding);
	bindtextdomain(PACKAGE, full_path_local_encoding);
	g_free(full_path_local_encoding);
	g_free(full_path_glib_encoding);
	g_free(root_glib_encoding);
#else
	bindtextdomain(PACKAGE, LOCALEDIR);
#endif
	textdomain(PACKAGE);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
#endif				/* ENABLE_NLS */
}
