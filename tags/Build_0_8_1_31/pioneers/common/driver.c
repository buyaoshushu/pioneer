/* Gnocatan - Implementation of the excellent Settlers of Catan board game.
 *   Go buy a copy.
 *
 * Author: Bibek Sahu
 * Copyright (C) 2000 the Free Software Foundation
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

/*
 * Implementation of generic driver-manipulation functions.
 * Creation of relevant globals.
 */

#include "driver.h"
#include "common_glib.h"

UIDriver *driver;

void set_ui_driver( UIDriver *d )
{
	driver = d;

	/* For now, these are universal functions that can't be hooked in
	   at compile time.  We may need to move these out of here if
	   other ports will be using something other than glib.
	 */
	if (driver) {
		driver->input_add_read = evl_glib_input_add_read;
		driver->input_add_write = evl_glib_input_add_write;
		driver->input_remove = evl_glib_input_remove;
	}
}
