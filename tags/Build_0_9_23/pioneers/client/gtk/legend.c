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
#include "frontend.h"
#include "theme.h"
#include "cost.h"

/* The order of the terrain_names is EXTREMELY important!  The order
 * must match the enum Terrain.
 */
static const gchar *terrain_names[] = {
	N_("Hill"),
	N_("Field"),
	N_("Mountain"),
	N_("Pasture"),
	N_("Forest"),
	N_("Desert"),
	N_("Sea"),
	N_("Gold")
};

static gint expose_legend_cb(GtkWidget * area,
			     G_GNUC_UNUSED GdkEventExpose * event,
			     Terrain terrain)
{
	MapTheme *theme = theme_get_current();
	static GdkGC *legend_gc;
	GdkPixbuf *p;
	gint height;

	if (area->window == NULL)
		return FALSE;

	if (legend_gc == NULL)
		legend_gc = gdk_gc_new(area->window);

	gdk_gc_set_fill(legend_gc, GDK_TILED);
	gdk_gc_set_tile(legend_gc, guimap_terrain(terrain));

	height = area->allocation.width / theme->scaledata[terrain].aspect;
	p = gdk_pixbuf_scale_simple(theme->scaledata[terrain].
				    native_image, area->allocation.width,
				    height, GDK_INTERP_BILINEAR);

	/* Center the image in the available space */
	gdk_draw_pixbuf(area->window, legend_gc, p,
			0, 0, 0, (area->allocation.height - height) / 2,
			-1, -1, GDK_RGB_DITHER_NONE, 0, 0);
	gdk_pixbuf_unref(p);
	return FALSE;
}

static void add_legend_terrain(GtkWidget * table, gint row, gint col,
			       Terrain terrain, Resource resource)
{
	GtkWidget *area;
	GtkWidget *label;

	area = gtk_drawing_area_new();
	gtk_widget_show(area);
	gtk_table_attach(GTK_TABLE(table), area,
			 col, col + 1, row, row + 1,
			 (GtkAttachOptions) GTK_FILL,
			 (GtkAttachOptions) GTK_FILL, 0, 0);
	gtk_widget_set_size_request(area, 30, 34);
	g_signal_connect(G_OBJECT(area), "expose_event",
			 G_CALLBACK(expose_legend_cb), (gpointer) terrain);

	label = gtk_label_new(_(terrain_names[terrain]));
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label,
			 col + 1, col + 2, row, row + 1,
			 (GtkAttachOptions) GTK_FILL,
			 (GtkAttachOptions) GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	if (resource != NO_RESOURCE) {
		label = gtk_label_new(resource_name(resource, TRUE));
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table), label,
				 col + 2, col + 3, row, row + 1,
				 (GtkAttachOptions) GTK_FILL,
				 (GtkAttachOptions) GTK_FILL, 0, 0);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	}
}

static void add_legend_cost(GtkWidget * table, gint row,
			    const gchar * item, const gint * cost)
{
	GtkWidget *label;
	gchar buffer[1024];

	label = gtk_label_new(item);
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	resource_format_type(buffer, cost);
	label = gtk_label_new(buffer);
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, row, row + 1,
			 GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
}

GtkWidget *legend_create_content()
{
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *vsep;
	GtkWidget *alignment;
	gint num_rows;

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);

	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Terrain Yield</b>"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);

	alignment = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 0, 12, 0);
	gtk_widget_show(alignment);
	gtk_box_pack_start(GTK_BOX(vbox), alignment, FALSE, FALSE, 0);

	table = gtk_table_new(4, 7, FALSE);
	gtk_widget_show(table);
	gtk_container_add(GTK_CONTAINER(alignment), table);
	gtk_table_set_row_spacings(GTK_TABLE(table), 3);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);

	add_legend_terrain(table, 0, 0, HILL_TERRAIN, BRICK_RESOURCE);
	add_legend_terrain(table, 1, 0, FIELD_TERRAIN, GRAIN_RESOURCE);
	add_legend_terrain(table, 2, 0, MOUNTAIN_TERRAIN, ORE_RESOURCE);
	add_legend_terrain(table, 3, 0, PASTURE_TERRAIN, WOOL_RESOURCE);

	vsep = gtk_vseparator_new();
	gtk_widget_show(vsep);
	gtk_table_attach(GTK_TABLE(table), vsep, 3, 4, 0, 4,
			 GTK_FILL, GTK_FILL, 0, 0);

	add_legend_terrain(table, 0, 4, FOREST_TERRAIN, LUMBER_RESOURCE);
	add_legend_terrain(table, 1, 4, GOLD_TERRAIN, GOLD_RESOURCE);
	add_legend_terrain(table, 2, 4, DESERT_TERRAIN, NO_RESOURCE);
	add_legend_terrain(table, 3, 4, SEA_TERRAIN, NO_RESOURCE);

	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Building Costs</b>"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);

	alignment = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 0, 12, 0);
	gtk_widget_show(alignment);
	gtk_box_pack_start(GTK_BOX(vbox), alignment, FALSE, FALSE, 0);

	num_rows = 4;
	if (have_ships())
		num_rows++;
	if (have_bridges())
		num_rows++;
	table = gtk_table_new(num_rows, 2, FALSE);
	gtk_widget_show(table);
	gtk_container_add(GTK_CONTAINER(alignment), table);
	gtk_table_set_row_spacings(GTK_TABLE(table), 3);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);

	num_rows = 0;
	add_legend_cost(table, num_rows++, _("Road"), cost_road());
	if (have_ships())
		add_legend_cost(table, num_rows++, _("Ship"), cost_ship());
	if (have_bridges())
		add_legend_cost(table, num_rows++, _("Bridge"),
				cost_bridge());
	add_legend_cost(table, num_rows++, _("Settlement"),
			cost_settlement());
	add_legend_cost(table, num_rows++, _("City"),
			cost_upgrade_settlement());
	add_legend_cost(table, num_rows++, _("Development Card"),
			cost_development());

	gtk_widget_show(vbox);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);
	return hbox;
}

GtkWidget *legend_create_dlg(void)
{
	static GtkWidget *legend_dlg;
	GtkWidget *dlg_vbox;
	GtkWidget *vbox;

	if (legend_dlg != NULL) {
		gtk_window_present(GTK_WINDOW(legend_dlg));
		return legend_dlg;
	}

	legend_dlg = gtk_dialog_new_with_buttons(_("Legend"),
						 GTK_WINDOW(app_window),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_STOCK_CLOSE,
						 GTK_RESPONSE_CLOSE, NULL);
	g_signal_connect(G_OBJECT(legend_dlg), "destroy",
			 G_CALLBACK(gtk_widget_destroyed), &legend_dlg);

	dlg_vbox = GTK_DIALOG(legend_dlg)->vbox;
	gtk_widget_show(dlg_vbox);

	vbox = legend_create_content();
	gtk_box_pack_start(GTK_BOX(dlg_vbox), vbox, TRUE, TRUE, 0);

	gtk_widget_show(legend_dlg);

	/* destroy dialog when OK is clicked */
	g_signal_connect(legend_dlg, "response",
			 G_CALLBACK(gtk_widget_destroy), NULL);

	return legend_dlg;
}