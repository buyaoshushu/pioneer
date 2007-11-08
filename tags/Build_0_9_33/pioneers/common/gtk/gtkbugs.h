/* Pioneers - Implementation of the excellent Settlers of Catan board game.
 *   Go buy a copy.
 *
 * Copyright (C) 1999 the Free Software Foundation
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

#endif