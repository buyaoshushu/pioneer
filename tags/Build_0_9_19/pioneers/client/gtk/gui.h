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

#ifndef __gui_h
#define __gui_h

#include "guimap.h"

gint color_messages_enabled;
void set_color_summary(gboolean flag);
gint legend_page_enabled;

GtkWidget *gui_get_dialog_button(GtkDialog * dlg, gint button);

void gui_reset(void);
void gui_set_instructions(const gchar * text);
void gui_set_vp_target_value(gint vp);
void gui_set_net_status(const gchar * text);

void gui_show_trade_page(gboolean show);
void gui_show_quote_page(gboolean show);
void gui_show_legend_page(gboolean show);
void gui_show_splash_page(gboolean show);

void gui_discard_show(void);
void gui_discard_hide(void);
void gui_gold_show(void);
void gui_gold_hide(void);
void gui_prompt_show(const gchar * message);
void gui_prompt_hide(void);

void gui_cursor_none(void);
void gui_cursor_set(CursorType type,
		    CheckFunc check_func, SelectFunc select_func,
		    const MapElement * user_data);
void gui_draw_hex(const Hex * hex);
void gui_draw_edge(const Edge * edge);
void gui_draw_node(const Node * node);

void gui_set_game_params(const GameParams * params);
void gui_setup_mode(gint player_num);
void gui_double_setup_mode(gint player_num);
void gui_highlight_chits(gint roll);

GtkWidget *gui_build_interface(void);
void show_admin_interface(GtkWidget * vbox);

gint hotkeys_handler(GtkWidget * w, GdkEvent * e, gpointer data);

extern GtkWidget *app_window;	/* main application window */

/* gui states */
typedef void (*GuiState) (GuiEvent event);

#ifdef DEBUG
#define set_gui_state(A) \
		{ g_print("New GUI_state: %s\n", #A); \
		set_gui_state_nomacro(A); }
#else
#define set_gui_state(A) \
		set_gui_state_nomacro(A)
#endif
void set_gui_state_nomacro(GuiState state);

GuiState get_gui_state(void);
void route_gui_event(GuiEvent event);

#endif
