#ifndef __gnocatan_server_h
#define __gnocatan_server_h

#include <netdb.h>
#include "gnocatan-path.h"

#define TERRAIN_DEFAULT	0
#define TERRAIN_RANDOM	1

typedef struct _comm_info {
	gint fd;
	guint read_tag;
	guint write_tag;
} comm_info;

/**** global variables ****/
extern gboolean register_server;
extern gchar server_port[NI_MAXSERV];
extern gchar server_admin_port[NI_MAXSERV];
extern gint server_port_int;

extern GameParams *params;

/**** game list control functions ****/
void game_list_add_item( GameParams *item );
GameParams *game_list_find_item( gchar *title );
void game_list_foreach( GHFunc func, gpointer user_data );
GameParams *load_game_desc(gchar *fname);
void load_game_types( gchar *path );

/**** callbacks to set parameters ****/
void cfg_set_num_players( gint num_players );
void cfg_set_sevens_rule( gint sevens_rule );
void cfg_set_victory_points( gint victory_points );
void cfg_set_game( gchar *game );
void cfg_set_terrain_type( gint terrain_type );
void cfg_set_tournament_time( gint tournament_time );
void cfg_set_exit( gboolean exitdone);
void cfg_set_timeout( gint to );

/* callbacks related to server starting / stopping */
gboolean start_server( gchar *port, gboolean register_server );

/* initialize the server */
void server_init( gchar *default_gnocatan_dir );

/**** backend functions for network administration of the server ****/

/* parse 'line' and run the command requested */
void admin_run_command( Session *admin_session, gchar *line );

/* network event handler, just like the one in meta.c, state.c, etc. */
void admin_event( NetEvent event, Session *admin_session, gchar *line );

/* accept a connection made to the admin port */
void admin_connect( comm_info *admin_info );

/* set up the administration port */
void admin_listen( gchar *port );

#endif /* __gnocatan_server_h */
