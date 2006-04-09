/*
 * Gnocatan: a fun game.
 * (C) 1999 the Free Software Foundation
 *
 * Author: Dave Cole.
 *
 * Implementation of the excellent Settlers of Catan board game.  Go
 * buy a copy.
 */

#include <stdio.h>

#include "log.h"
#include "driver.h"

/* Set the default logging function to 'func'. */
void log_set_func( LogFunc func )
{
	driver->log_write = func;
}

/* Set the default logging function to the system default (LOG_FUNC_DEFAULT,
 *   found in log.h).
 */
void log_set_func_default( void )
{
	driver->log_write = LOG_FUNC_DEFAULT;
}

/* Take a string of text and write it to the console. */
void add_text_console(gchar *text, gchar *type_str)
{
	if( type_str )
		fprintf( stderr, "%s%s", type_str, text );
	else
		fprintf( stderr, "%s", text );
}

/* Write a message string to the console, adding a prefix depending on 
 *   its type.
 */
void log_message_string_console( gint msg_type, gchar *text )
{
	gchar *prefix;
	
	switch( msg_type ) {
		case MSG_ERROR:	prefix = _("*ERROR* ");
						break;
		
		case MSG_INFO:	prefix = _("- ");
						break;
		
		case MSG_CHAT:	prefix = _("CHAT: ");  
			/* why the hell are you logging chat to the console? */
						break;
		
		default:		prefix = _("** UNKNOWN MESSAGE TYPE ** ");
	}
	
	add_text_console( text, prefix );
}

/* Log a message, sending it through logfunc after turning the params into a
 *   single string.
 */
void log_message_using_func(LogFunc logfunc, gint msg_type, gchar *fmt, ...)
{
	gchar text[1024];
	va_list ap;

	va_start(ap, fmt);
	g_vsnprintf(text, sizeof(text), fmt, ap);
	va_end(ap);
	
	logfunc( msg_type, text );
}

/* Log a message, sending it through driver->log_write (or if that's NULL,
 *   then through LOG_FUNC_DEFAULT) after turning the params into a single
 *   string.
 */
void log_message(gint msg_type, gchar *fmt, ...)
{
	gchar text[1024];
	va_list ap;

	va_start(ap, fmt);
	g_vsnprintf(text, sizeof(text), fmt, ap);
	va_end(ap);
	
	if( driver->log_write )
		driver->log_write( msg_type, text );
	else
		LOG_FUNC_DEFAULT( msg_type, text );
}