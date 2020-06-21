/* Pioneers - Implementation of the excellent Settlers of Catan board game.
 *   Go buy a copy.
 *
 * Copyright (C) 1999 Dave Cole
 * Copyright (C) 2003 Bas Wijnen <shevek@fmf.nl>
 * Copyright (C) 2004-2005 Roland Clobus <rclobus@bigfoot.com>
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
#include <math.h>
#include <ctype.h>
#include <assert.h>

#include "aboutbox.h"
#include "frontend.h"
#include "cards.h"
#include "cost.h"
#include "log.h"
#include "common_gtk.h"
#include "histogram.h"
#include "theme.h"
#include "config-gnome.h"
#include "audio.h"
#include "notification.h"

static GtkWidget *preferences_dlg;
GtkWidget *app_window;		/* main application window */

#define MAP_WIDTH 350		/* default map width */
#define MAP_HEIGHT 200		/* default map height */

#define PIONEERS_ICON_FILE	"pioneers.png"

/* Callback functions from the resource file */
G_MODULE_EXPORT void game_quit_cb(GObject * gobject, gpointer user_data);
G_MODULE_EXPORT void game_legend_cb(GObject * gobject, gpointer user_data);
G_MODULE_EXPORT void game_histogram_cb(GObject * gobject,
				       gpointer user_data);
G_MODULE_EXPORT void game_settings_cb(GObject * gobject,
				      gpointer user_data);
G_MODULE_EXPORT void show_admin_interface_cb(GObject * gobject,
					     gpointer user_data);
G_MODULE_EXPORT void zoom_normal_cb(GObject * gobject, gpointer user_data);
G_MODULE_EXPORT void zoom_center_map_cb(GObject * gobject,
					gpointer user_data);
G_MODULE_EXPORT void toggle_full_screen_cb(GObject * gobject,
					   gpointer user_data);
G_MODULE_EXPORT void showhide_toolbar_cb(GObject * gobject,
					 gpointer user_data);
G_MODULE_EXPORT void preferences_cb(GObject * gobject, gpointer user_data);
G_MODULE_EXPORT void help_about_cb(GObject * gobject, gpointer user_data);
#ifdef HAVE_HELP
G_MODULE_EXPORT void help_manual_cb(GObject * gobject, gpointer user_data);
#endif

static GuiMap *gmap;		/* handle to map drawing code */

enum {
	MAP_PAGE,		/* the map */
	TRADE_PAGE,		/* trading interface */
	QUOTE_PAGE,		/* submit quotes page */
	LEGEND_PAGE,		/* legend */
	SPLASH_PAGE		/* splash screen */
};

static GtkWidget *map_notebook;	/* map area panel */
static GtkWidget *trade_page;	/* trade page in map area */
static GtkWidget *quote_page;	/* quote page in map area */
static GtkWidget *legend_page;	/* legend page in map area */
static GtkWidget *splash_page;	/* splash page in map area */

static GtkWidget *develop_notebook;	/* development card area panel */

static GtkWidget *messages_txt;	/* messages text widget */
static GtkWidget *prompt_lbl;	/* big prompt messages */

static GtkWidget *app_bar;
static GtkWidget *net_status;
static GtkWidget *vp_target_status;

static GtkWidget *main_paned;	/* Horizontal for 16:9, Vertical for 4:3 mode */
static GtkWidget *chat_panel = NULL;	/* Panel for chat, placed below or to the right */

static GtkBuilder *builder = NULL;	/* The manager of the GUI */
static GActionGroup *action_group = NULL;	/* Global action group */

static gboolean toolbar_show_accelerators = TRUE;
static gboolean color_messages_enabled = TRUE;
static gboolean legend_page_enabled = TRUE;
static gboolean charity_enabled = FALSE;
static TAutomaticRoll automatic_roll = ROLL_MANUALLY;

static GList *rules_callback_list = NULL;

static void gui_set_toolbar_visible(gboolean visible);
static void gui_toolbar_show_accelerators(gboolean show_accelerators);

static void redirect_gui_event_cb(G_GNUC_UNUSED GSimpleAction * action,
				  G_GNUC_UNUSED GVariant * parameter,
				  gpointer user_data)
{
	route_gui_event(g_variant_get_int32(user_data));
}

static void game_leave_cb(G_GNUC_UNUSED GSimpleAction * action,
			  G_GNUC_UNUSED GVariant * parameter,
			  G_GNUC_UNUSED gpointer user_data)
{
	frontend_quote_end();
	route_gui_event(GUI_DISCONNECT);
}

void game_quit_cb(G_GNUC_UNUSED GObject * gobject,
		  G_GNUC_UNUSED gpointer user_data)
{
	guimap_delete(gmap);
	route_gui_event(GUI_QUIT);
}

void show_admin_interface_cb(G_GNUC_UNUSED GObject * gobject,
			     G_GNUC_UNUSED gpointer user_data)
{
#ifdef ADMIN_GTK
	show_admin_interface();
#endif
}

void toggle_full_screen_cb(GObject * gobject, gpointer user_data)
{
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(gobject))) {
		gtk_window_fullscreen(GTK_WINDOW(user_data));
	} else {
		gtk_window_unfullscreen(GTK_WINDOW(user_data));
	}
}

void zoom_normal_cb(G_GNUC_UNUSED GObject * gobject,
		    G_GNUC_UNUSED gpointer user_data)
{
	guimap_zoom_normal(gmap);
}

void zoom_center_map_cb(G_GNUC_UNUSED GObject * gobject,
			G_GNUC_UNUSED gpointer user_data)
{
	guimap_zoom_center_map(gmap);
}

GtkWidget *gui_get_dialog_button(GtkDialog * dlg, gint button)
{
	GList *list;

	g_return_val_if_fail(dlg != NULL, NULL);
	g_assert(gtk_dialog_get_action_area(dlg) != NULL);

	list =
	    gtk_container_get_children(GTK_CONTAINER
				       (gtk_dialog_get_action_area(dlg)));
	list = g_list_nth(list, button);
	if (list != NULL) {
		g_assert(list->data != NULL);
		return GTK_WIDGET(list->data);
	}
	return NULL;
}

void gui_reset(void)
{
	guimap_reset(gmap);
}

void gui_set_instructions(const gchar * text)
{
	gtk_statusbar_push(GTK_STATUSBAR(app_bar), 0, text);
}

void gui_set_vp_target_value(gint vp)
{
	gchar *vp_text;

	/* Victory points target in statusbar */
	vp_text = g_strdup_printf(_("Points needed to win: %i"), vp);

	gtk_label_set_text(GTK_LABEL(vp_target_status), vp_text);
	g_free(vp_text);
}

void gui_set_net_status(const gchar * text)
{
	gtk_label_set_text(GTK_LABEL(net_status), text);
}

void gui_cursor_none(void)
{
	MapElement dummyElement;
	dummyElement.pointer = NULL;
	guimap_cursor_set(gmap, NO_CURSOR, -1, NULL, NULL, NULL,
			  &dummyElement, FALSE);
}

void gui_cursor_set(CursorType type,
		    CheckFunc check_func, SelectFunc select_func,
		    CancelFunc cancel_func, const MapElement * user_data)
{
	guimap_cursor_set(gmap, type, my_player_num(),
			  check_func, select_func, cancel_func, user_data,
			  FALSE);
}

void gui_draw_hex(const Hex * hex)
{
	if (gmap->surface != NULL)
		guimap_draw_hex(gmap, hex);
}

void gui_draw_edge(const Edge * edge)
{
	if (gmap->surface != NULL)
		guimap_draw_edge(gmap, edge);
}

void gui_draw_node(const Node * node)
{
	if (gmap->surface != NULL)
		guimap_draw_node(gmap, node);
}

void gui_highlight_chits(gint roll)
{
	guimap_highlight_chits(gmap, roll);
}

static gint button_press_map_cb(GtkWidget * area, GdkEventButton * event,
				G_GNUC_UNUSED gpointer user_data)
{
	if (gtk_widget_get_window(area) == NULL || gmap->map == NULL)
		return FALSE;

	if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
		guimap_cursor_select(gmap);
		return TRUE;
	}
	return FALSE;
}

static GtkWidget *build_map_area(void)
{
	GtkWidget *map_area = guimap_build_drawingarea(gmap, MAP_WIDTH,
						       MAP_HEIGHT);
	gtk_widget_add_events(map_area, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(G_OBJECT(map_area), "button_press_event",
			 G_CALLBACK(button_press_map_cb), NULL);

	return map_area;
}

static GtkWidget *build_messages_panel(void)
{
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *scroll_win;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_widget_show(vbox);

	/* Label for messages log */
	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Messages</b>"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	scroll_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scroll_win, -1, 80);
	gtk_widget_show(scroll_win);
	gtk_box_pack_start(GTK_BOX(vbox), scroll_win, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_win),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	messages_txt = gtk_text_view_new();
	gtk_widget_show(messages_txt);
	gtk_container_add(GTK_CONTAINER(scroll_win), messages_txt);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(messages_txt), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(messages_txt),
				    GTK_WRAP_WORD);

	message_window_set_text(messages_txt, chat_panel);

	return vbox;
}

void gui_show_trade_page(gboolean show)
{
	/* Normal keyboard focus when visible */
	chat_set_grab_focus_on_update(!show);
	if (show) {
		gtk_widget_show(trade_page);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(map_notebook),
					      TRADE_PAGE);
	} else {
		gtk_notebook_prev_page(GTK_NOTEBOOK(map_notebook));
		gtk_widget_hide(trade_page);
	}
}

void gui_show_quote_page(gboolean show)
{
	/* Normal keyboard focus when visible */
	chat_set_grab_focus_on_update(!show);
	if (show) {
		gtk_widget_show(quote_page);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(map_notebook),
					      QUOTE_PAGE);
	} else {
		gtk_notebook_prev_page(GTK_NOTEBOOK(map_notebook));
		gtk_widget_hide(quote_page);
	}
}

static void gui_theme_changed(void)
{
	g_assert(legend_page != NULL);
	gtk_widget_queue_draw(legend_page);
	gtk_widget_queue_draw(gmap->area);
}

void gui_show_legend_page(gboolean show)
{
	if (show) {
		gtk_widget_show(legend_page);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(map_notebook),
					      LEGEND_PAGE);
	} else
		gtk_widget_hide(legend_page);
}

void gui_show_splash_page(gboolean show, GtkWidget * chat_widget)
{
	static GtkWidget *widget = NULL;
	if (chat_widget != NULL)
		widget = chat_widget;
	g_assert(widget != NULL);

	chat_set_grab_focus_on_update(TRUE);
	if (show) {
		gtk_widget_show(splash_page);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(map_notebook),
					      SPLASH_PAGE);
		gtk_widget_hide(widget);
	} else {
		gtk_widget_hide(splash_page);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(map_notebook),
					      MAP_PAGE);
		gtk_widget_show(widget);
	}
}

static GtkWidget *splash_build_page(void)
{
	GtkWidget *image;
	gchar *filename;

	filename = g_build_filename(DATADIR, "pixmaps", "pioneers",
				    "splash.png", NULL);
	image = gtk_image_new_from_file(filename);
	gtk_widget_show(image);
	g_free(filename);

	return image;
}

static GtkWidget *build_map_panel(void)
{
	GtkWidget *lbl;
	GtkWidget *hbox;
	GtkWidget *close_button;

	map_notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(map_notebook), GTK_POS_TOP);
	gtk_widget_show(map_notebook);

	/* Tab page name */
	lbl = gtk_label_new(_("Map"));
	gtk_widget_show(lbl);
	gtk_notebook_insert_page(GTK_NOTEBOOK(map_notebook),
				 build_map_area(), lbl, MAP_PAGE);

	hbox = create_label_with_close_button(
						     /* Tab page name */
						     _("Trade"),
						     /* Tooltip */
						     _("Finish trading"),
						     &close_button);
	frontend_gui_register(close_button, GUI_TRADE_FINISH, "clicked");
	trade_page = trade_build_page();
	gtk_notebook_insert_page(GTK_NOTEBOOK(map_notebook),
				 trade_page, hbox, TRADE_PAGE);
	gtk_widget_hide(trade_page);

	hbox = create_label_with_close_button(
						     /* Tab page name */
						     _("Quote"),
						     /* Tooltip */
						     _(""
						       "Reject domestic trade"),
						     &close_button);
	frontend_gui_register(close_button, GUI_QUOTE_REJECT, "clicked");
	quote_page = quote_build_page();
	gtk_notebook_insert_page(GTK_NOTEBOOK(map_notebook),
				 quote_page, hbox, QUOTE_PAGE);
	gtk_widget_hide(quote_page);

	/* Tab page name */
	lbl = gtk_label_new(_("Legend"));
	gtk_widget_show(lbl);
	legend_page = legend_create_content();
	gtk_notebook_insert_page(GTK_NOTEBOOK(map_notebook),
				 legend_page, lbl, LEGEND_PAGE);
	if (!legend_page_enabled)
		gui_show_legend_page(FALSE);
	theme_register_callback(G_CALLBACK(gui_theme_changed));

	/* Tab page name, shown for the splash screen */
	lbl = gtk_label_new(_("Welcome to Pioneers"));
	gtk_widget_show(lbl);
	splash_page = splash_build_page();
	gtk_notebook_insert_page(GTK_NOTEBOOK(map_notebook),
				 splash_page, lbl, SPLASH_PAGE);

	return map_notebook;
}

void gui_discard_show(void)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(develop_notebook), 1);
}

void gui_discard_hide(void)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(develop_notebook), 0);
}

void gui_gold_show(void)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(develop_notebook), 2);
}

void gui_gold_hide(void)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(develop_notebook), 0);
}

void gui_prompt_show(const gchar * message)
{
	gtk_label_set_text(GTK_LABEL(prompt_lbl), message);
	/* Force resize of the notebook, this is needed because
	 * GTK does not redraw when the text in a label changes.
	 */
	gtk_container_check_resize(GTK_CONTAINER(develop_notebook));
	gtk_notebook_set_current_page(GTK_NOTEBOOK(develop_notebook), 3);
}

void gui_prompt_hide(void)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(develop_notebook), 0);
}

static GtkWidget *prompt_build_page(void)
{
	prompt_lbl = gtk_label_new("");
	gtk_widget_show(prompt_lbl);
	return prompt_lbl;
}

static GtkWidget *build_develop_panel(void)
{
	develop_notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(develop_notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(develop_notebook),
				     FALSE);
	gtk_widget_show(develop_notebook);

	gtk_notebook_insert_page(GTK_NOTEBOOK(develop_notebook),
				 develop_build_page(), NULL, 0);
	gtk_notebook_insert_page(GTK_NOTEBOOK(develop_notebook),
				 discard_build_page(), NULL, 1);
	gtk_notebook_insert_page(GTK_NOTEBOOK(develop_notebook),
				 gold_build_page(), NULL, 2);
	gtk_notebook_insert_page(GTK_NOTEBOOK(develop_notebook),
				 prompt_build_page(), NULL, 3);

	return develop_notebook;
}

static gboolean get_16_9_layout(void)
{
	GtkWidget *paned;

	g_return_val_if_fail(main_paned != NULL, FALSE);
	g_return_val_if_fail(chat_panel != NULL, FALSE);

	paned = gtk_paned_get_child1(GTK_PANED(main_paned));
	if (gtk_widget_get_parent(chat_panel) == paned)
		return FALSE;
	return TRUE;
}

static void set_16_9_layout(gboolean layout_16_9)
{
	GtkWidget *paned;
	gboolean can_remove;

	g_return_if_fail(main_paned != NULL);
	g_return_if_fail(chat_panel != NULL);

	paned = gtk_paned_get_child1(GTK_PANED(main_paned));

	/* Increase reference count, otherwise it will be destroyed */
	g_object_ref(chat_panel);

	/* Initially the widget has no parent, and cannot be removed */
	can_remove = gtk_widget_get_parent(chat_panel) != NULL;

	if (layout_16_9) {
		if (can_remove)
			gtk_container_remove(GTK_CONTAINER(paned),
					     chat_panel);
		gtk_container_add(GTK_CONTAINER(main_paned), chat_panel);
	} else {
		if (can_remove)
			gtk_container_remove(GTK_CONTAINER(main_paned),
					     chat_panel);
		gtk_container_add(GTK_CONTAINER(paned), chat_panel);
	}
	g_object_unref(chat_panel);
}

static GtkWidget *build_main_interface(void)
{
	GtkWidget *vbox;
	GtkWidget *hpaned;
	GtkWidget *vpaned;
	GtkWidget *panel;

	hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_show(hpaned);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_widget_show(vbox);
	gtk_paned_pack1(GTK_PANED(hpaned), vbox, FALSE, TRUE);

	gtk_box_pack_start(GTK_BOX(vbox),
			   identity_build_panel(), FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox),
			   resource_build_panel(), FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox),
			   build_develop_panel(), FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox),
			   player_build_summary(), TRUE, TRUE, 0);

	main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_show(main_paned);

	vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
	gtk_widget_show(vpaned);

	gtk_paned_pack1(GTK_PANED(main_paned), vpaned, TRUE, TRUE);

	gtk_paned_pack1(GTK_PANED(vpaned), build_map_panel(), TRUE, TRUE);

	chat_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gui_show_splash_page(TRUE, chat_panel);

	panel = chat_build_panel();
	frontend_gui_register(panel, GUI_DISCONNECT, NULL);
	gtk_box_pack_start(GTK_BOX(chat_panel), panel, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(chat_panel),
			   build_messages_panel(), TRUE, TRUE, 0);

	set_16_9_layout(config_get_int_with_default
			("settings/layout_16_9", TRUE));

	gtk_paned_pack2(GTK_PANED(hpaned), main_paned, TRUE, TRUE);
	return hpaned;
}

static void theme_change_cb(GObject * gobject,
			    G_GNUC_UNUSED gpointer user_data)
{
	gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(gobject));
	MapTheme *theme = g_list_nth_data(theme_get_list(), idx);
	if (theme != theme_get_current()) {
		config_set_string("settings/theme", theme->name);
		theme_set_current(theme);
		if (gmap->surface != NULL) {
			cairo_surface_destroy(gmap->surface);
			gmap->surface = NULL;
		}
		theme_rescale(2 * gmap->x_point);
	}

}

static void show_legend_cb(GtkToggleButton * widget,
			   G_GNUC_UNUSED gpointer user_data)
{
	legend_page_enabled = gtk_toggle_button_get_active(widget);
	gui_show_legend_page(legend_page_enabled);
	config_set_int("settings/legend_page", legend_page_enabled);
}

static void message_color_cb(GtkToggleButton * widget,
			     G_GNUC_UNUSED gpointer user_data)
{
	color_messages_enabled = gtk_toggle_button_get_active(widget);
	config_set_int("settings/color_messages", color_messages_enabled);
	log_set_func_message_color_enable(color_messages_enabled);
}

static void chat_color_cb(GtkToggleButton * widget,
			  G_GNUC_UNUSED gpointer user_data)
{
	color_chat_enabled = gtk_toggle_button_get_active(widget);
	config_set_int("settings/color_chat", color_chat_enabled);
}

static void summary_color_cb(GtkToggleButton * widget,
			     G_GNUC_UNUSED gpointer user_data)
{
	gboolean color_summary = gtk_toggle_button_get_active(widget);
	config_set_int("settings/color_summary", color_summary);
	set_color_summary(color_summary);
}

static void silent_mode_cb(GtkToggleButton * widget,
			   G_GNUC_UNUSED gpointer user_data)
{
	GtkToggleButton *announce_button = user_data;

	gboolean silent_mode = gtk_toggle_button_get_active(widget);
	config_set_int("settings/silent_mode", silent_mode);
	set_silent_mode(silent_mode);
	gtk_toggle_button_set_inconsistent(announce_button, silent_mode);
	gtk_widget_set_sensitive(GTK_WIDGET(announce_button),
				 !silent_mode);
}

static void announce_player_cb(GtkToggleButton * widget,
			       G_GNUC_UNUSED gpointer user_data)
{
	gboolean announce_player = gtk_toggle_button_get_active(widget);
	config_set_int("settings/announce_player", announce_player);
	set_announce_player(announce_player);
}

static void toggle_16_9_cb(GtkToggleButton * widget,
			   G_GNUC_UNUSED gpointer user_data)
{
	gboolean layout_16_9 = gtk_toggle_button_get_active(widget);
	config_set_int("settings/layout_16_9", layout_16_9);
	set_16_9_layout(layout_16_9);
}

static void toggle_notifications_cb(GtkToggleButton * widget,
				    G_GNUC_UNUSED gpointer user_data)
{
	gboolean show_notifications = gtk_toggle_button_get_active(widget);
	config_set_int("settings/show_notifications", show_notifications);
	set_show_notifications(show_notifications);
}

void showhide_toolbar_cb(GObject * gobject,
			 G_GNUC_UNUSED gpointer user_data)
{
	gui_set_toolbar_visible(gtk_check_menu_item_get_active
				(GTK_CHECK_MENU_ITEM(gobject)));
}

static void toolbar_shortcuts_cb(void)
{
	gui_toolbar_show_accelerators(!toolbar_show_accelerators);
}

gboolean get_charity_enabled(void)
{
	return charity_enabled;
}

void set_charity_enabled(gboolean new_charity_enabled)
{
	if (new_charity_enabled != charity_enabled) {
		charity_enabled = new_charity_enabled;
		config_set_int("settings/charity_enabled",
			       charity_enabled);
	}
}

TAutomaticRoll get_automatic_roll(void)
{
	return automatic_roll;
}

void set_automatic_roll(TAutomaticRoll new_automatic_roll)
{
	if (new_automatic_roll != automatic_roll) {
		automatic_roll = new_automatic_roll;
		config_set_int("settings/automatic_roll", automatic_roll);
	}
}

static void automatic_roll_changed_cb(GtkComboBox * widget,
				      G_GNUC_UNUSED gpointer user_data)
{
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter(widget, &iter)) {
		gint value;
		gtk_tree_model_get(gtk_combo_box_get_model(widget), &iter,
				   1, &value, -1);
		set_automatic_roll(value);
	}
}

void preferences_cb(G_GNUC_UNUSED GObject * gobject, gpointer user_data)
{
	GtkWidget *silent_mode_widget;
	GtkWidget *widget;
	GtkWidget *dlg_vbox;
	GtkWidget *theme_label;
	GtkWidget *theme_list;
	GtkWidget *layout;
	GtkListStore *auto_roll_list;
	GtkCellRenderer *cell;
	GtkTreeIter iter;
	GtkTreeIter active_iter;

	guint row;
	gint color_summary;
	GList *theme_elt;
	int i;

	if (preferences_dlg != NULL) {
		gtk_window_present(GTK_WINDOW(preferences_dlg));
		return;
	};

	/* Caption of preferences dialog */
	preferences_dlg = gtk_dialog_new_with_buttons(_(""
							"Pioneers Preferences"),
						      GTK_WINDOW
						      (user_data),
						      GTK_DIALOG_DESTROY_WITH_PARENT,
						      /* Button text */
						      _("_Close"),
						      GTK_RESPONSE_CLOSE,
						      NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(preferences_dlg),
					GTK_RESPONSE_CLOSE);
	g_signal_connect(G_OBJECT(preferences_dlg), "destroy",
			 G_CALLBACK(gtk_widget_destroyed),
			 &preferences_dlg);
	g_signal_connect(G_OBJECT(preferences_dlg), "response",
			 G_CALLBACK(gtk_widget_destroy), NULL);
	gtk_widget_show(preferences_dlg);

	dlg_vbox =
	    gtk_dialog_get_content_area(GTK_DIALOG(preferences_dlg));
	gtk_widget_show(dlg_vbox);

	layout = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(layout), 3);
	gtk_grid_set_column_spacing(GTK_GRID(layout), 5);
	gtk_widget_show(layout);
	gtk_box_pack_start(GTK_BOX(dlg_vbox), layout, FALSE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(layout), 5);

	row = 0;

	theme_list = gtk_combo_box_text_new();
	gtk_widget_set_hexpand(theme_list, TRUE);
	/* Label for changing the theme, in the preferences dialog */
	theme_label = gtk_label_new(_("Theme:"));
	gtk_label_set_xalign(GTK_LABEL(theme_label), 0.0);
	gtk_widget_show(theme_list);
	gtk_widget_show(theme_label);

	for (i = 0, theme_elt = theme_get_list();
	     theme_elt != NULL; ++i, theme_elt = g_list_next(theme_elt)) {
		MapTheme *theme = theme_elt->data;
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT
					       (theme_list), theme->name);
		if (theme == theme_get_current())
			gtk_combo_box_set_active(GTK_COMBO_BOX(theme_list),
						 i);
	}
	g_signal_connect(G_OBJECT(theme_list), "changed",
			 G_CALLBACK(theme_change_cb), NULL);

	gtk_grid_attach(GTK_GRID(layout), theme_label, 0, row, 1, 1);
	gtk_grid_attach(GTK_GRID(layout), theme_list, 1, row, 1, 1);
	gtk_widget_set_tooltip_text(theme_list,
				    /* Tooltip for changing the theme in the preferences dialog */
				    _("Choose one of the themes"));
	row++;

	/* Label for the option to show the legend */
	widget = gtk_check_button_new_with_label(_("Show legend"));
	gtk_widget_show(widget);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
				     legend_page_enabled);
	g_signal_connect(G_OBJECT(widget), "toggled",
			 G_CALLBACK(show_legend_cb), NULL);
	gtk_grid_attach(GTK_GRID(layout), widget, 0, row, 2, 1);
	gtk_widget_set_tooltip_text(widget,
				    /* Tooltip for the option to show the legend */
				    _(""
				      "Show the legend as a page beside the map"));
	row++;

	/* Label for the option to display log messages in color */
	widget = gtk_check_button_new_with_label(_("Messages with color"));
	gtk_widget_show(widget);
	gtk_grid_attach(GTK_GRID(layout), widget, 0, row, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
				     color_messages_enabled);
	g_signal_connect(G_OBJECT(widget), "toggled",
			 G_CALLBACK(message_color_cb), NULL);
	gtk_widget_set_tooltip_text(widget,
				    /* Tooltip for the option to display log messages in color */
				    _("Show new messages with color"));
	row++;

	widget = gtk_check_button_new_with_label(
							/* Label for the option to display chat in color of player */
							_(""
							  "Chat in color of player"));
	gtk_widget_show(widget);
	gtk_grid_attach(GTK_GRID(layout), widget, 0, row, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
				     color_chat_enabled);
	g_signal_connect(G_OBJECT(widget), "toggled",
			 G_CALLBACK(chat_color_cb), NULL);
	gtk_widget_set_tooltip_text(widget,
				    /* Tooltip for the option to display chat in color of player */
				    _(""
				      "Show new chat messages in the color of the player"));
	row++;

	/* Label for the option to display the summary with colors */
	widget = gtk_check_button_new_with_label(_("Summary with color"));
	gtk_widget_show(widget);
	gtk_grid_attach(GTK_GRID(layout), widget, 0, row, 2, 1);
	color_summary =
	    config_get_int_with_default("settings/color_summary", TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), color_summary);	/* @todo RC use correct variable */
	g_signal_connect(G_OBJECT(widget), "toggled",
			 G_CALLBACK(summary_color_cb), NULL);
	gtk_widget_set_tooltip_text(widget,
				    /* Tooltip for the option to display the summary with colors */
				    _("Use colors in the player summary"));
	row++;

	widget =
	    /* Label for the option to display keyboard accelerators in the toolbar */
	    gtk_check_button_new_with_label(_("Toolbar with shortcuts"));
	gtk_widget_show(widget);
	gtk_grid_attach(GTK_GRID(layout), widget, 0, row, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
				     toolbar_show_accelerators);
	g_signal_connect(G_OBJECT(widget), "toggled",
			 G_CALLBACK(toolbar_shortcuts_cb), NULL);
	gtk_widget_set_tooltip_text(widget,
				    /* Tooltip for the option to display keyboard accelerators in the toolbar */
				    _(""
				      "Show keyboard shortcuts in the toolbar"));
	row++;

	silent_mode_widget =
	    /* Label for the option to disable all sounds */
	    gtk_check_button_new_with_label(_("Silent mode"));
	gtk_widget_show(silent_mode_widget);
	gtk_grid_attach(GTK_GRID(layout), silent_mode_widget, 0, row, 2,
			1);
	gtk_widget_set_tooltip_text(silent_mode_widget,
				    /* Tooltip for the option to disable all sounds */
				    _(""
				      "In silent mode no sounds are made"));
	row++;

	widget =
	    /* Label for the option to announce when players/spectators enter */
	    gtk_check_button_new_with_label(_("Announce new players"));
	gtk_widget_show(widget);
	gtk_grid_attach(GTK_GRID(layout), widget, 0, row, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
				     get_announce_player());
	g_signal_connect(G_OBJECT(widget), "toggled",
			 G_CALLBACK(announce_player_cb), NULL);
	gtk_widget_set_tooltip_text(widget,
				    /* Tooltip for the option to use sound when players/spectators enter */
				    _(""
				      "Make a sound when a new player or spectator enters the game"));
	row++;

	/* Silent mode widget is connected an initialized after the announce button */
	g_signal_connect(G_OBJECT(silent_mode_widget), "toggled",
			 G_CALLBACK(silent_mode_cb), widget);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(silent_mode_widget),
				     get_silent_mode());

#ifdef HAVE_NOTIFY
	/* Label for the option to use the notifications. */
	widget = gtk_check_button_new_with_label(_("Show notifications"));
	gtk_widget_show(widget);
	gtk_grid_attach(GTK_GRID(layout), widget, 0, row, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
				     get_show_notifications());
	g_signal_connect(G_OBJECT(widget), "toggled",
			 G_CALLBACK(toggle_notifications_cb), NULL);
	gtk_widget_set_tooltip_text(widget,
				    /* Tooltip for notifications option. */
				    _("Show notifications when it's your "
				      "turn or when new trade is available"));
	row++;
#endif

	/* Label for the option to use the 16:9 layout. */
	widget = gtk_check_button_new_with_label(_("Use 16:9 layout"));
	gtk_widget_show(widget);
	gtk_grid_attach(GTK_GRID(layout), widget, 0, row, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
				     get_16_9_layout());
	g_signal_connect(G_OBJECT(widget), "toggled",
			 G_CALLBACK(toggle_16_9_cb), NULL);
	gtk_widget_set_tooltip_text(widget,
				    /* Tooltip for 16:9 option. */
				    _(""
				      "Use a 16:9 friendly layout for the window"));
	row++;

	auto_roll_list = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
	gtk_list_store_append(auto_roll_list, &iter);
	gtk_list_store_set(auto_roll_list, &iter,
			   0, _("Roll the dice manually each turn"),
			   1, ROLL_MANUALLY, -1);
	if (get_automatic_roll() == ROLL_MANUALLY) {
		active_iter = iter;
	}
	gtk_list_store_append(auto_roll_list, &iter);
	gtk_list_store_set(auto_roll_list, &iter,
			   0,
			   _(""
			     "Roll the dice automatically, except when you have a soldier card"),
			   1, ROLL_AUTOMATICALLY_EXCEPT_WITH_SOLDIER_CARD,
			   -1);
	if (get_automatic_roll() ==
	    ROLL_AUTOMATICALLY_EXCEPT_WITH_SOLDIER_CARD) {
		active_iter = iter;
	}
	gtk_list_store_append(auto_roll_list, &iter);
	gtk_list_store_set(auto_roll_list, &iter,
			   0,
			   _(""
			     "Roll the dice automatically, except when you have a development card"),
			   1, ROLL_AUTOMATICALLY_EXCEPT_WITH_RESOURCE_CARD,
			   -1);
	if (get_automatic_roll() ==
	    ROLL_AUTOMATICALLY_EXCEPT_WITH_RESOURCE_CARD) {
		active_iter = iter;
	}
	gtk_list_store_append(auto_roll_list, &iter);
	gtk_list_store_set(auto_roll_list, &iter,
			   0, _("Roll the dice automatically"),
			   1, ROLL_AUTOMATICALLY, -1);
	if (get_automatic_roll() == ROLL_AUTOMATICALLY) {
		active_iter = iter;
	}
	widget =
	    gtk_combo_box_new_with_model(GTK_TREE_MODEL(auto_roll_list));
	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(widget), &active_iter);

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(widget), cell, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(widget),
				       cell, "text", 0, NULL);

	gtk_widget_show(widget);
	gtk_grid_attach(GTK_GRID(layout), widget, 0, row, 2, 1);
	g_signal_connect(G_OBJECT(widget), "changed",
			 G_CALLBACK(automatic_roll_changed_cb), NULL);
	gtk_widget_set_tooltip_text(widget,
				    /* Tooltip for automatic roll option. */
				    _(""
				      "Automate the rolling of the dice"));
	row++;

}

void help_about_cb(G_GNUC_UNUSED GObject * gobject, gpointer user_data)
{
	aboutbox_display(GTK_WINDOW(user_data),
			 /* Caption of about box */
			 _("About Pioneers"));
}

void game_legend_cb(G_GNUC_UNUSED GObject * gobject, gpointer user_data)
{
	legend_create_dlg(GTK_WINDOW(user_data));
}

void game_histogram_cb(G_GNUC_UNUSED GObject * gobject, gpointer user_data)
{
	histogram_create_dlg(GTK_WINDOW(user_data));
}

void game_settings_cb(G_GNUC_UNUSED GObject * gobject, gpointer user_data)
{
	settings_create_dlg(GTK_WINDOW(user_data));
}

#ifdef HAVE_HELP
void help_manual_cb(G_GNUC_UNUSED GObject * gobject, gpointer user_data)
{
	GError *error = NULL;
	gtk_show_uri_on_window(GTK_WINDOW(user_data), "help:pioneers",
			       GDK_CURRENT_TIME, &error);
	if (error) {
		log_message(MSG_ERROR, "%s: %s\n", _("Show the manual"),
			    error->message);
		g_error_free(error);
	}
}
#endif

static GAction *getAction(GuiEvent id)
{
	const gchar *path = NULL;
#ifdef ADMIN_GTK
	frontend_gui_register_action(gtk_ui_manager_get_action
				     (manager,
				      "ui/MainMenu/GameMenu/GameAdmin"),
				     GUI_CONNECT);
#endif

	switch (id) {
	case GUI_CONNECT:
		path = "action_game_new";
		break;
	case GUI_DISCONNECT:
		path = "action_game_leave";
		break;
	case GUI_CHANGE_NAME:
		path = "action_player_name";
		break;
	case GUI_ROLL:
		path = "action_roll_dice";
		break;
	case GUI_TRADE:
		path = "action_trade";
		break;
	case GUI_UNDO:
		path = "action_undo";
		break;
	case GUI_FINISH:
		path = "action_finish";
		break;
	case GUI_ROAD:
		path = "action_build_road";
		break;
	case GUI_SHIP:
		path = "action_build_ship";
		break;
	case GUI_MOVE_SHIP:
		path = "action_move_ship";
		break;
	case GUI_BRIDGE:
		path = "action_build_bridge";
		break;
	case GUI_SETTLEMENT:
		path = "action_build_settlement";
		break;
	case GUI_CITY:
		path = "action_build_city";
		break;
	case GUI_BUY_DEVELOP:
		path = "action_buy_development";
		break;
	case GUI_CITY_WALL:
		path = "action_build_city_wall";
		break;
	default:
		break;
	};

	if (!path)
		return NULL;

	return g_action_map_lookup_action(G_ACTION_MAP(action_group),
					  path);
}

/** Set the visibility of the toolbar */
static void gui_set_toolbar_visible(gboolean visible)
{
	GtkWidget *toolbar =
	    GTK_WIDGET(gtk_builder_get_object(builder, "toolbar"));

	if (visible)
		gtk_widget_show(GTK_WIDGET(toolbar));
	else
		gtk_widget_hide(GTK_WIDGET(toolbar));
	config_set_int("settings/show_toolbar", visible);
}

/* Note:
 * 1. the accelerators are not translated
 * 2. the accelerators are duplicated here from the .ui file
 * 3. the UI file has Roll Dice\nF1 to ensure the proper initial size
 */
/* *INDENT-OFF* */
static struct {
	const gchar *text;
	const gchar *accelerator;
} toolbar_accelerators[] = {
	{ N_("Roll Dice"), "F1" },
	{ N_("Trade"), "F2" },
	{ N_("Undo"), "F3" },
	{ N_("Finish") , "F4" },
	{ N_("Road"), "F5" },
	{ N_("Ship"), "F6" },
	{ N_("Move Ship"), "F7" },
	{ N_("Bridge"), "F8" },
	{ N_("Settlement"), "F9" },
	{ N_("City"), "F10" },
	{ N_("Develop"), "F11" },
	{ N_("City Wall"), NULL },
};
/* *INDENT-ON* */

/** Show the accelerators in the toolbar */
static void gui_toolbar_show_accelerators(gboolean show_accelerators)
{
	GtkToolbar *tb;
	gint n, i;

	toolbar_show_accelerators = show_accelerators;

	tb = GTK_TOOLBAR(gtk_builder_get_object(builder, "toolbar"));
	g_return_if_fail(tb != NULL);

	n = gtk_toolbar_get_n_items(tb);
	for (i = 0; i < n; i++) {
		GtkToolItem *ti;
		GtkToolButton *tbtn;
		gchar *text;
		guint j;

		ti = gtk_toolbar_get_nth_item(tb, i);
		tbtn = GTK_TOOL_BUTTON(ti);
		g_assert(tbtn != NULL);

		text = g_strdup(gtk_tool_button_get_label(tbtn));
		if (strchr(text, '\n'))
			*strchr(text, '\n') = '\0';
		/* Find the matching entry */
		for (j = 0; j < G_N_ELEMENTS(toolbar_accelerators); j++) {
			if (strcmp(text, _(toolbar_accelerators[j].text))
			    == 0) {
				if (show_accelerators) {
					gchar *label;

					if (toolbar_accelerators
					    [j].accelerator == NULL)
						label =
						    g_strdup_printf
						    ("%s\n", text);
					else {
						label =
						    g_strdup_printf
						    ("%s\n(%s)",
						     text,
						     toolbar_accelerators
						     [j].accelerator);
					}
					gtk_tool_button_set_label
					    (tbtn, label);
					g_free(label);
				} else {
					gtk_tool_button_set_label
					    (tbtn, text);
				}
				break;
			}
		}
		g_free(text);
	}
	config_set_int("settings/toolbar_show_accelerators",
		       toolbar_show_accelerators);
}

/** Show or hide a button in the toolbar */
static void gui_toolbar_show_button(const gchar * id, gboolean visible)
{
	GtkToolItem *item =
	    GTK_TOOL_ITEM(gtk_builder_get_object(builder, id));

	g_return_if_fail(item != NULL);
	gtk_tool_item_set_visible_horizontal(item, visible);
}

void gui_rules_register_callback(GCallback callback)
{
	rules_callback_list = g_list_append(rules_callback_list, callback);
}

void gui_set_game_params(const GameParams * params)
{
	GList *list;
	GtkWidget *label;

	gmap->map = params->map;
	gmap->player_num = my_player_num();
	gtk_widget_queue_resize(gmap->area);

	gui_toolbar_show_button("BuildRoad",
				params->num_build_type[BUILD_ROAD] > 0);
	gui_toolbar_show_button("BuildShip",
				params->num_build_type[BUILD_SHIP] > 0);
	gui_toolbar_show_button("MoveShip",
				params->num_build_type[BUILD_SHIP] > 0);
	gui_toolbar_show_button("BuildBridge",
				params->num_build_type[BUILD_BRIDGE] > 0);
	/* In theory, it is possible to play a game without cities */
	gui_toolbar_show_button("BuildCity",
				params->num_build_type[BUILD_CITY] > 0);
	gui_toolbar_show_button("BuildCityWall",
				params->num_build_type[BUILD_CITY_WALL] >
				0);

	identity_draw();

	gui_set_vp_target_value(params->victory_points);

	list = rules_callback_list;
	while (list) {
		G_CALLBACK(list->data) ();
		list = g_list_next(list);
	}

	label =
	    gtk_notebook_get_tab_label(GTK_NOTEBOOK(map_notebook),
				       legend_page);
	g_object_ref(label);

	gtk_widget_destroy(legend_page);
	legend_page = legend_create_content();
	gtk_notebook_insert_page(GTK_NOTEBOOK(map_notebook),
				 legend_page, label, LEGEND_PAGE);
	if (!legend_page_enabled)
		gui_show_legend_page(FALSE);
	g_object_unref(label);
}

static GtkWidget *build_status_bar(void)
{
	GtkWidget *vsep;

	app_bar = gtk_statusbar_new();
	gtk_widget_show(app_bar);

	vp_target_status = gtk_label_new("");
	gtk_widget_show(vp_target_status);
	gtk_box_pack_start(GTK_BOX(app_bar), vp_target_status, FALSE, TRUE,
			   0);

	vsep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_widget_show(vsep);
	gtk_box_pack_start(GTK_BOX(app_bar), vsep, FALSE, TRUE, 0);

	/* Network status: offline */
	net_status = gtk_label_new(_("Offline"));
	gtk_widget_show(net_status);
	gtk_box_pack_start(GTK_BOX(app_bar), net_status, FALSE, TRUE, 0);

	vsep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_widget_show(vsep);
	gtk_box_pack_start(GTK_BOX(app_bar), vsep, FALSE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(app_bar),
			   player_build_turn_area(), FALSE, TRUE, 0);

	/* Initial text in status bar */
	gui_set_instructions(_("Welcome to Pioneers!"));

	return app_bar;
}

static void register_pixmaps(void)
{
	GtkIconTheme *iconTheme = gtk_icon_theme_get_default();
	gchar *path;

	path = g_build_filename(DATADIR, "pixmaps", "pioneers", NULL);
	gtk_icon_theme_append_search_path(iconTheme, path);
	g_free(path);
}

static void add_guievent_action(GActionGroup * action_group,
				const gchar * name, GuiEvent event)
{
	GSimpleAction *action = g_simple_action_new(name, NULL);
	g_signal_connect(action, "activate",
			 G_CALLBACK(redirect_gui_event_cb),
			 g_variant_new_int32(event));
	g_action_map_add_action(G_ACTION_MAP(action_group),
				G_ACTION(action));
}

GtkWidget *gui_build_interface(void)
{
	GtkWidget *vbox;
	gchar *icon_file;
	GSimpleAction *action;

	player_init();

	gmap = guimap_new();

	register_pixmaps();

	builder =
	    gtk_builder_new_from_resource
	    ("/net/sourceforge/pio/client/gtk/client.ui");
	app_window =
	    GTK_WIDGET(gtk_builder_get_object(builder, "toplevel"));
	/* The name of the application */
	gtk_window_set_title(GTK_WINDOW(app_window), _("Pioneers"));
	gtk_builder_connect_signals(builder, app_window);

	vbox = GTK_WIDGET(gtk_builder_get_object(builder, "vbox"));

	icon_file =
	    g_build_filename(DATADIR, "pixmaps", PIONEERS_ICON_FILE, NULL);
	if (g_file_test(icon_file, G_FILE_TEST_EXISTS)) {
		gtk_window_set_default_icon_from_file(icon_file, NULL);
	} else {
		/* Missing pixmap, main icon file */
		g_warning("Pixmap not found: %s", icon_file);
	}
	g_free(icon_file);

	color_chat_enabled =
	    config_get_int_with_default("settings/color_chat", TRUE);

	color_messages_enabled =
	    config_get_int_with_default("settings/color_messages", TRUE);
	log_set_func_message_color_enable(color_messages_enabled);

	set_color_summary(config_get_int_with_default
			  ("settings/color_summary", TRUE));

	set_silent_mode(config_get_int_with_default
			("settings/silent_mode", FALSE));
	set_announce_player(config_get_int_with_default
			    ("settings/announce_player", TRUE));
	set_show_notifications(config_get_int_with_default
			       ("settings/show_notifications", TRUE));
	set_charity_enabled(config_get_int_with_default
			    ("settings/charity_enabled", FALSE));
	set_automatic_roll(config_get_int_with_default
			   ("settings/automatic_roll",
			    get_automatic_roll()));

	legend_page_enabled =
	    config_get_int_with_default("settings/legend_page", FALSE);

	gtk_box_pack_start(GTK_BOX(vbox), build_main_interface(), TRUE,
			   TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), build_status_bar(), FALSE, FALSE,
			   0);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM
				       (gtk_builder_get_object
					(builder,
					 "toolbar_visible")),
				       config_get_int_with_default
				       ("settings/show_toolbar", TRUE));

	g_signal_connect(G_OBJECT(app_window), "key_press_event",
			 G_CALLBACK(hotkeys_handler), NULL);

	gtk_widget_show(app_window);

	action_group = G_ACTION_GROUP(g_simple_action_group_new());
	add_guievent_action(action_group, "action_game_new", GUI_CONNECT);
	action = g_simple_action_new("action_game_leave", NULL);
	g_signal_connect(action, "activate",
			 G_CALLBACK(game_leave_cb), NULL);
	g_action_map_add_action(G_ACTION_MAP(action_group),
				G_ACTION(action));
	add_guievent_action(action_group, "action_player_name",
			    GUI_CHANGE_NAME);
	add_guievent_action(action_group, "action_roll_dice", GUI_ROLL);
	add_guievent_action(action_group, "action_trade", GUI_TRADE);
	add_guievent_action(action_group, "action_undo", GUI_UNDO);
	add_guievent_action(action_group, "action_finish", GUI_FINISH);
	add_guievent_action(action_group, "action_build_road", GUI_ROAD);
	add_guievent_action(action_group, "action_build_ship", GUI_SHIP);
	add_guievent_action(action_group, "action_move_ship",
			    GUI_MOVE_SHIP);
	add_guievent_action(action_group, "action_build_bridge",
			    GUI_BRIDGE);
	add_guievent_action(action_group,
			    "action_build_settlement", GUI_SETTLEMENT);
	add_guievent_action(action_group, "action_build_city", GUI_CITY);
	add_guievent_action(action_group, "action_buy_development",
			    GUI_BUY_DEVELOP);
	add_guievent_action(action_group, "action_build_city_wall",
			    GUI_CITY_WALL);
	gtk_widget_insert_action_group(app_window, "win", action_group);

	frontend_gui_register_action(getAction(GUI_CONNECT), GUI_CONNECT);
	frontend_gui_register_action(getAction(GUI_DISCONNECT),
				     GUI_DISCONNECT);
#ifdef ADMIN_GTK
	/** @todo RC 2005-05-26 Admin interface: Not tested */
	frontend_gui_register_action(gtk_ui_manager_get_action
				     (manager,
				      "ui/MainMenu/GameMenu/GameAdmin"),
				     GUI_ADMIN);
	gtk_menu_item_set_visible(GTK_MENU_ITEM
				  (gtk_builder_get_object
				   (builder, "admin")), TRUE);
#endif
	frontend_gui_register_action(getAction(GUI_CHANGE_NAME),
				     GUI_CHANGE_NAME);
	frontend_gui_register_action(getAction(GUI_ROLL), GUI_ROLL);
	frontend_gui_register_action(getAction(GUI_TRADE), GUI_TRADE);
	frontend_gui_register_action(getAction(GUI_UNDO), GUI_UNDO);
	frontend_gui_register_action(getAction(GUI_FINISH), GUI_FINISH);
	frontend_gui_register_action(getAction(GUI_ROAD), GUI_ROAD);
	frontend_gui_register_action(getAction(GUI_SHIP), GUI_SHIP);
	frontend_gui_register_action(getAction(GUI_MOVE_SHIP),
				     GUI_MOVE_SHIP);
	frontend_gui_register_action(getAction(GUI_BRIDGE), GUI_BRIDGE);
	frontend_gui_register_action(getAction(GUI_SETTLEMENT),
				     GUI_SETTLEMENT);
	frontend_gui_register_action(getAction(GUI_CITY), GUI_CITY);
	frontend_gui_register_action(getAction(GUI_BUY_DEVELOP),
				     GUI_BUY_DEVELOP);
	frontend_gui_register_action(getAction(GUI_CITY_WALL),
				     GUI_CITY_WALL);
#if 0
	frontend_gui_register_destroy(gtk_ui_manager_get_action
				      (manager, "GameQuit"), GUI_QUIT);
#endif
#ifdef HAVE_HELP
	gtk_widget_set_visible(GTK_WIDGET
			       (gtk_builder_get_object
				(builder, "help_manual")), TRUE);
#endif


	gui_toolbar_show_button("BuildShip", FALSE);
	gui_toolbar_show_button("MoveShip", FALSE);
	gui_toolbar_show_button("BuildBridge", FALSE);
	gui_toolbar_show_button("BuildCityWall", FALSE);

	gui_toolbar_show_accelerators(config_get_int_with_default
				      ("settings/toolbar_show_accelerators",
				       TRUE));

	gtk_widget_show(app_window);
	g_signal_connect(G_OBJECT(app_window), "delete_event",
			 G_CALLBACK(game_quit_cb), NULL);

	return app_window;
}

void gui_set_show_no_setup_nodes(gboolean show)
{
	guimap_set_show_no_setup_nodes(gmap, show);
}

Map *frontend_get_map(void)
{
	g_return_val_if_fail(gmap != NULL, NULL);
	return gmap->map;
}

void frontend_set_map(Map * map)
{
	g_assert(gmap != NULL);
	gmap->map = map;
}
