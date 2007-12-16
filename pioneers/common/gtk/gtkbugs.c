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

#include "gtkbugs.h"

void set_pixbuf_tree_view_column_autogrow(GtkWidget * parent_widget,
					  GtkTreeViewColumn * column)
{
	if (gtk_major_version == 2
	    && (gtk_minor_version >= 5 && gtk_minor_version <= 6)) {
		gint horizontal_separator, focus_line_width, icon_width;

		gtk_tree_view_column_set_sizing(column,
						GTK_TREE_VIEW_COLUMN_FIXED);
		gtk_widget_style_get(parent_widget,
				     "horizontal_separator",
				     &horizontal_separator,
				     "focus-line-width", &focus_line_width,
				     NULL);
		gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &icon_width,
				     NULL);
		gtk_tree_view_column_set_fixed_width(column,
						     icon_width +
						     2 *
						     horizontal_separator +
						     focus_line_width);
	}
}

/** Since Gtk+ 2.6 you cannot press a button twice, without moving the
 *  mouse.
 */
void action_set_sensitive(GtkAction * action, gboolean sensitive)
{
	GSList *widgets;
	widgets = gtk_action_get_proxies(action);
	while (widgets) {
		widget_set_sensitive(GTK_WIDGET(widgets->data), sensitive);
		widgets = g_slist_next(widgets);
	}
	gtk_action_set_sensitive(action, sensitive);
}

void widget_set_sensitive(GtkWidget * widget, gboolean sensitive)
{
	GtkWidget *button;

	gtk_widget_set_sensitive(widget, sensitive);

	/** @bug Gtk bug 56070. If the mouse is over a toolbar button that
	 *  becomes sensitive, one can't click it without moving the mouse out
	 *  and in again. This bug is registered in Bugzilla as a Gtk bug. The
	 *  workaround tests if the mouse is inside the currently sensitivized
	 *  button, and if yes call button_enter()
	 */
	if (!GTK_IS_BIN(widget))
		return;

	button = gtk_bin_get_child(GTK_BIN(widget));
	if (sensitive && GTK_IS_BUTTON(button)) {
		gint x, y, state;
		gtk_widget_get_pointer(button, &x, &y);
		state = GTK_WIDGET_STATE(button);
		if ((state == GTK_STATE_NORMAL
		     || state == GTK_STATE_PRELIGHT) && x >= 0 && y >= 0
		    && x < button->allocation.width
		    && y < button->allocation.height) {
			gtk_button_enter(GTK_BUTTON(button));
			GTK_BUTTON(button)->in_button = TRUE;
			gtk_widget_set_state(widget, GTK_STATE_PRELIGHT);
		}
	}
}

#if (GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION < 12)
GtkTooltips *tooltips = NULL;
#endif
