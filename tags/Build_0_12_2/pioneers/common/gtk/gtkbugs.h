/* Pioneers - Implementation of the excellent Settlers of Catan board game.
 *   Go buy a copy.
 *
 * Copyright (C) 1999 Dave Cole
 * Copyright (C) 2005 Roland Clobus <rclobus@bigfoot.com>
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

#ifndef __gtkbugs_h
#define __gtkbugs_h

#include <gtk/gtk.h>

/** @bug 2005-07-25 In Gtk+ 2.6 the pixbuf column is 
 *  one pixel too small, the focus_line_width is not used correctly.
 *  See also http://bugzilla.gnome.org/show_bug.cgi?id=147867
 */
void set_pixbuf_tree_view_column_autogrow(GtkWidget * parent_widget,
					  GtkTreeViewColumn * column);

/** @bug 2006-01-15 In Gtk+ 2.4 it is not possible to disable actions.
 *  This will be close to its functionality, but will show -/- in the 
 *  menus for the shortcut keys.
 */
void action_set_sensitive(GtkAction * action, gboolean sensitive);

/** @bug Gtk bug 56070. If the mouse is over a button that becomes
 *  sensitive, one can't click it without moving the mouse out and in again.
 */
void widget_set_sensitive(GtkWidget * widget, gboolean sensitive);

/* Not a Gtk+ bug, but a work around is presented here.
 * In Gtk+ 2.12 GtkToolTips has been replaced by GtkToolTip.
 */
#if (GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION < 12)
#include <gtk/gtktooltips.h>
extern GtkTooltips *tooltips;
#define gtk_widget_set_tooltip_text(widget, text) \
	if (tooltips == NULL) \
		tooltips = gtk_tooltips_new(); \
	gtk_tooltips_set_tip(tooltips, widget, text, NULL)
#endif

#endif
