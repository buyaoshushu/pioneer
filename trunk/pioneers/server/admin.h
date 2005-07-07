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

#ifndef __admin_h
#define __admin_h

typedef struct _comm_info {
	gint fd;
	guint read_tag;
	guint write_tag;
} comm_info;

/**** backend functions for network administration of the server ****/

/* parse 'line' and run the command requested */
void admin_run_command(Session * admin_session, const gchar * line);

/* network event handler, just like the one in meta.c, state.c, etc. */
void admin_event(NetEvent event, Session * admin_session,
		 const gchar * line);

/* accept a connection made to the admin port */
void admin_connect(comm_info * admin_info);

/* set up the administration port */
void admin_listen(const gchar * port);

#endif				/* __admin_h */
