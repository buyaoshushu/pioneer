/* Gnocatan - Implementation of the excellent Settlers of Catan board game.
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
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>

#ifndef HAVE_GETADDRINFO_ET_AL
#include <stdlib.h> /* For atoi */
#endif /* ndef HAVE_GETADDRINFO_ET_AL */

#include "config.h"
#include "driver.h"
#include "game.h"
#include "map.h"
#include "network.h"
#include "log.h"

typedef union {
	struct sockaddr sa;
	struct sockaddr_in in;
#ifdef HAVE_GETADDRINFO_ET_AL
	struct sockaddr_in6 in6;
#endif /* HAVE_GETADDRINFO_ET_AL */
} sockaddr_t;

#ifdef DEBUG
void debug(const gchar * fmt, ...)
{
	va_list ap;
	gchar buff[16 * 1024];
	gint len;
	gint idx;

	va_start(ap, fmt);
	len = g_vsnprintf(buff, sizeof(buff), fmt, ap);
	va_end(ap);

	for (idx = 0; idx < len; idx++) {
		if (isprint(buff[idx]) || idx == len - 1)
			fputc(buff[idx], stderr);
		else
			switch (buff[idx]) {
			case '\n':
				fputc('\\', stderr);
				fputc('n', stderr);
				break;
			case '\r':
				fputc('\\', stderr);
				fputc('r', stderr);
				break;
			case '\t':
				fputc('\\', stderr);
				fputc('t', stderr);
				break;
			default:
				fprintf(stderr, "\\x%02x", buff[idx]);
				break;
			}
	}
}
#endif

static void read_ready(Session * ses);
static void write_ready(Session * ses);

static void listen_read(Session * ses, gboolean monitor)
{
	if (monitor && ses->read_tag == 0)
		ses->read_tag =
		    driver->input_add_read(ses->fd, (InputFunc) read_ready,
					   ses);
	if (!monitor && ses->read_tag != 0) {
		driver->input_remove(ses->read_tag);
		ses->read_tag = 0;
	}

}

static void listen_write(Session * ses, gboolean monitor)
{
	if (monitor && ses->write_tag == 0)
		ses->write_tag =
		    driver->input_add_write(ses->fd,
					    (InputFunc) write_ready, ses);
	if (!monitor && ses->write_tag != 0) {
		driver->input_remove(ses->write_tag);
		ses->write_tag = 0;
	}
}

static void notify(Session * ses, NetEvent event, gchar * line)
{
	if (ses->notify_func != NULL)
		ses->notify_func(event, ses->user_data, line);
}

void net_close(Session * ses)
{
	if (ses->fd >= 0) {
		listen_read(ses, FALSE);
		listen_write(ses, FALSE);
		close(ses->fd);
		ses->fd = -1;

		while (ses->write_queue != NULL) {
			char *data = ses->write_queue->data;

			ses->write_queue
			    = g_list_remove(ses->write_queue, data);
			g_free(data);
		}
	}
}

void net_close_when_flushed(Session * ses)
{
	ses->waiting_for_close = TRUE;
	if (ses->write_queue != NULL)
		return;

	net_close(ses);
	notify(ses, NET_CLOSE, NULL);
}

void net_wait_for_close(Session * ses)
{
	ses->waiting_for_close = TRUE;
}

static void close_and_callback(Session * ses)
{
	net_close(ses);
	notify(ses, NET_CLOSE, NULL);
}

static void write_ready(Session * ses)
{
	if (!ses || ses->fd < 0)
		return;
	if (ses->connect_in_progress) {
		/* We were waiting to connect to server
		 */
		int error;
		socklen_t error_len;

		error_len = sizeof(error);
		if (getsockopt(ses->fd, SOL_SOCKET, SO_ERROR,
			       &error, &error_len) < 0) {
			notify(ses, NET_CONNECT_FAIL, NULL);
			log_message(MSG_ERROR,
				    _
				    ("Error checking connect status: %s\n"),
				    g_strerror(errno));
			net_close(ses);
		} else if (error != 0) {
			notify(ses, NET_CONNECT_FAIL, NULL);
			log_message(MSG_ERROR,
				    _
				    ("Error connecting to host '%s': %s\n"),
				    ses->host, g_strerror(error));
			net_close(ses);
		} else {
			ses->connect_in_progress = FALSE;
			notify(ses, NET_CONNECT, NULL);
			listen_write(ses, FALSE);
			listen_read(ses, TRUE);
		}
		return;
	}

	while (ses->write_queue != NULL) {
		int num;
		char *data = ses->write_queue->data;
		int len = strlen(data);

		num = write(ses->fd, data, len);
#ifdef DEBUG
		debug("write_ready: write(%d, \"%.*s\", %d) = %d\n",
		      ses->fd, len, data, len, num);
#endif
		if (num < 0) {
			if (errno == EAGAIN)
				break;
			if (errno != EPIPE)
				log_message(MSG_ERROR,
					    _
					    ("Error writing socket: %s\n"),
					    g_strerror(errno));
			close_and_callback(ses);
			return;
		} else if (num == len) {
			ses->write_queue
			    = g_list_remove(ses->write_queue, data);
			g_free(data);
		} else {
			memmove(data, data + num, len - num + 1);
			break;
		}
	}

	/* Stop spinning when nothing to do.
	 */
	if (ses->write_queue == NULL) {
		if (ses->waiting_for_close)
			close_and_callback(ses);
		else
			listen_write(ses, FALSE);
	}
}

void net_write(Session * ses, const gchar * data)
{
	if (!ses || ses->fd < 0)
		return;
	if (ses->write_queue != NULL || !net_connected(ses)) {
		/* reassign the pointer, because the glib docs say it may
		 * change and because if we're in the process of connecting the
		 * pointer may currently be null. */
		ses->write_queue =
		    g_list_append(ses->write_queue, g_strdup(data));
	} else {
		int len;
		int num;

		len = strlen(data);
		num = write(ses->fd, data, len);
#ifdef DEBUG
		if (num > 0)
			debug("(%d) --> %s\n", ses->fd, data);
		else if (errno != EAGAIN)
			debug("(%d) --- Error writing to socket.\n",
			      ses->fd);
#endif
		if (num < 0) {
			if (errno != EAGAIN) {
				log_message(MSG_ERROR,
					    _
					    ("Error writing to socket: %s\n"),
					    g_strerror(errno));
				close_and_callback(ses);
				return;
			}
			num = 0;
		}
		if (num != len) {
			ses->write_queue
			    = g_list_append(NULL, g_strdup(data + num));
			listen_write(ses, TRUE);
		}
	}
}

void net_printf(Session * ses, const gchar * fmt, ...)
{
	char buff[4096];
	va_list ap;

	va_start(ap, fmt);
	g_vsnprintf(buff, sizeof(buff), fmt, ap);
	va_end(ap);

	net_write(ses, buff);
}

static int find_line(char *buff, int len)
{
	int idx;

	for (idx = 0; idx < len; idx++)
		if (buff[idx] == '\n')
			return idx;
	return -1;
}

static void read_ready(Session * ses)
{
	int num;
	int offset;

	if (ses->read_len == sizeof(ses->read_buff)) {
		/* We are in trouble now - the application has not
		 * been processing the data we have been
		 * reading. Assume something has gone wrong and
		 * disconnect
		 */
		log_message(MSG_ERROR,
			    _("Read buffer overflow - disconnecting\n"));
		close_and_callback(ses);
		return;
	}

	num = read(ses->fd, ses->read_buff + ses->read_len,
		   sizeof(ses->read_buff) - ses->read_len);
	if (num < 0) {
		if (errno == EAGAIN)
			return;
		log_message(MSG_ERROR, _("Error reading socket: %s\n"),
			    g_strerror(errno));
		close_and_callback(ses);
		return;
	}

	if (num == 0) {
		close_and_callback(ses);
		return;
	}

	ses->read_len += num;

	if (ses->entered)
		return;
	ses->entered = TRUE;

	offset = 0;
	while (ses->fd >= 0 && offset < ses->read_len) {
		char *line = ses->read_buff + offset;
		int len = find_line(line, ses->read_len - offset);

		if (len < 0)
			break;
		line[len] = '\0';
		offset += len + 1;

#ifdef DEBUG
		debug("(%d) <-- %s\n", ses->fd, line);
#endif
		notify(ses, NET_READ, line);
	}

	if (offset < ses->read_len) {
		/* Did not process all data in buffer - discard
		 * processed data and copy remaining data to beginning
		 * of buffer until next time
		 */
		memmove(ses->read_buff, ses->read_buff + offset,
			ses->read_len - offset);
		ses->read_len -= offset;
	} else
		/* Processed all data in buffer, discard it
		 */
		ses->read_len = 0;

	ses->entered = FALSE;
}

Session *net_new(NetNotifyFunc notify_func, void *user_data)
{
	Session *ses;

	ses = g_malloc0(sizeof(*ses));
	ses->notify_func = notify_func;
	ses->user_data = user_data;
	ses->fd = -1;

	return ses;
}

void net_use_fd(Session * ses, int fd)
{
	ses->fd = fd;
	listen_read(ses, TRUE);
}

gboolean net_connected(Session * ses)
{
	return ses->fd >= 0 && !ses->connect_in_progress;
}

gboolean net_connect(Session * ses, const gchar * host, const gchar * port)
{
#ifdef HAVE_GETADDRINFO_ET_AL
	int err;
	struct addrinfo hints, *ai, *aip;
#else
	gint iii;
	struct hostent *he;
	struct sockaddr_in addr;
#endif /* HAVE_GETADDRINFO_ET_AL */

	net_close(ses);
	if (ses->host != NULL)
		g_free(ses->host);
	ses->host = g_strdup(host);
	ses->port = g_strdup(port);

#ifdef HAVE_GETADDRINFO_ET_AL
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if ((err = getaddrinfo(host, port, &hints, &ai))) {
		log_message(MSG_ERROR,
			    _("Cannot resolve %s port %s: %s\n"), host,
			    port, gai_strerror(err));
		return FALSE;
	}
	if (!ai) {
		log_message(MSG_ERROR,
			    _
			    ("Cannot resolve %s port %s: host not found\n"),
			    host, port);
		return FALSE;
	}
#endif /* HAVE_GETADDRINFO_ET_AL */

#ifdef HAVE_GETADDRINFO_ET_AL
	for (aip = ai; aip; aip = aip->ai_next) {
		ses->fd = socket(aip->ai_family, SOCK_STREAM, 0);
#else /* HAVE_GETADDRINFO_ET_AL */
	for (iii=0; iii < 1; ++iii) { /* Loop only once */
		ses->fd = socket(AF_INET, SOCK_STREAM, 0);
#endif /* HAVE_GETADDRINFO_ET_AL */
		if (ses->fd < 0) {
			log_message(MSG_ERROR,
				    _("Error creating socket: %s\n"),
				    g_strerror(errno));
			continue;
		}
		if (fcntl(ses->fd, F_SETFD, 1) < 0) {
			log_message(MSG_ERROR,
				    _
				    ("Error setting socket close-on-exec: %s\n"),
				    g_strerror(errno));
			close(ses->fd);
			ses->fd = -1;
			continue;
		}
		if (fcntl(ses->fd, F_SETFL, O_NDELAY) < 0) {
			log_message(MSG_ERROR,
				    _
				    ("Error setting socket non-blocking: %s\n"),
				    g_strerror(errno));
			close(ses->fd);
			ses->fd = -1;
			continue;
		}

#ifdef HAVE_GETADDRINFO_ET_AL
		if (connect(ses->fd, aip->ai_addr, aip->ai_addrlen) < 0) {
#else /* HAVE_GETADDRINFO_ET_AL */
		he = gethostbyname(host);
		addr.sin_family = AF_INET;
		addr.sin_port = htons(atoi(port));
		addr.sin_addr = *((struct in_addr *)he->h_addr);
		memset(&addr.sin_zero, 0, 8);
		if (connect(ses->fd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) < 0) {
#endif /* HAVE_GETADDRINFO_ET_AL */
			if (errno == EINPROGRESS) {
				ses->connect_in_progress = TRUE;
				listen_write(ses, TRUE);
				break;
			} else {
				log_message(MSG_ERROR,
					    _
					    ("Error connecting to %s: %s\n"),
					    host, g_strerror(errno));
				close(ses->fd);
				ses->fd = -1;
				continue;
			}
		} else
			listen_read(ses, TRUE);
	}

#ifdef HAVE_GETADDRINFO_ET_AL
	freeaddrinfo(ai);
#endif /* HAVE_GETADDRINFO_ET_AL */

	if (ses->fd >= 0)
		return TRUE;
	else
		return FALSE;
}

/* Free and NULL-ity the session *ses */
void net_free(Session ** ses)
{
	net_close(*ses);

	if ((*ses)->host != NULL)
		g_free((*ses)->host);
	g_free(*ses);
	*ses = NULL;
}

gchar *get_my_hostname(void)
{
	char hbuf[256];
	struct hostent *hp;

	if (gethostname(hbuf, sizeof(hbuf))) {
		perror("gethostname");
		return NULL;
	}
	if (!(hp = gethostbyname(hbuf))) {
		herror("gethostbyname");
		return NULL;
	}
	return g_strdup(hp->h_name);
}

gchar *get_meta_server_name(gboolean use_default)
{
	gchar *temp;

	temp = g_strdup(g_getenv("GNOCATAN_META_SERVER"));
	if (!temp) {
		if (use_default)
			temp = g_strdup(GNOCATAN_DEFAULT_META_SERVER);
		else {
			temp = get_my_hostname();
		}
	}
	return temp;
}

const gchar *get_gnocatan_dir(void)
{
	const gchar *gnocatan_dir = g_getenv("GNOCATAN_DIR");
	if (!gnocatan_dir)
		gnocatan_dir = GNOCATAN_DIR_DEFAULT;
	return gnocatan_dir;
}

int net_open_listening_socket(const gchar * port, gchar ** error_message)
{
#ifdef HAVE_GETADDRINFO_ET_AL
	int err;
	struct addrinfo hints, *ai, *aip;
	int yes;
	gint fd = -1;

	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	if ((err = getaddrinfo(NULL, port, &hints, &ai)) || !ai) {
		*error_message =
		    g_strdup_printf(_
				    ("Error creating struct addrinfo: %s"),
				    gai_strerror(err));
		return -1;
	}

	for (aip = ai; aip; aip = aip->ai_next) {
		fd = socket(aip->ai_family, SOCK_STREAM, 0);
		if (fd < 0) {
			continue;
		}
		yes = 1;

		/* setsockopt() before bind(); otherwise it has no effect! -- egnor */
		if (setsockopt
		    (fd, SOL_SOCKET, SO_REUSEADDR, &yes,
		     sizeof(yes)) < 0) {
			close(fd);
			continue;
		}
		if (bind(fd, aip->ai_addr, aip->ai_addrlen) < 0) {
			close(fd);
			continue;
		}

		break;
	}

	if (!aip) {
		*error_message =
		    g_strdup_printf(_
				    ("Error creating listening socket: %s\n"),
				    g_strerror(errno));
		freeaddrinfo(ai);
		return -1;
	}

	freeaddrinfo(ai);

	if (fcntl(fd, F_SETFL, O_NDELAY) < 0) {
		*error_message =
		    g_strdup_printf(_
				    ("Error setting socket non-blocking: %s\n"),
				    g_strerror(errno));
		close(fd);
		return -1;
	}

	if (listen(fd, 5) < 0) {
		*error_message =
		    g_strdup_printf(_
				    ("Error during listen on socket: %s\n"),
				    g_strerror(errno));
		close(fd);
		return -1;
	}
	*error_message = NULL;
	return fd;
#else /* HAVE_GETADDRINFO_ET_AL */
	*error_message = g_strdup(_("Listening not yet supported on this platform."));
	return -1;
#endif /* HAVE_GETADDRINFO_ET_AL */
}

gboolean net_get_peer_name(gint fd, gchar ** hostname, gchar ** servname,
			   gchar ** error_message)
{
#ifdef HAVE_GETADDRINFO_ET_AL
	sockaddr_t peer;
	socklen_t peer_len;
#endif /* HAVE_GETADDRINFO_ET_AL */

	*hostname = g_strdup(_("unknown"));
	*servname = g_strdup(_("unknown"));

#ifdef HAVE_GETADDRINFO_ET_AL
	peer_len = sizeof(peer);
	if (getpeername(fd, &peer.sa, &peer_len) < 0) {
		*error_message =
		    g_strdup_printf(_("Error getting peer name: %s"),
				    strerror(errno));
		return FALSE;
	} else {
		int err;
		char host[NI_MAXHOST];
		char port[NI_MAXSERV];

		if ((err =
		     getnameinfo(&peer.sa, peer_len, host, NI_MAXHOST,
				 port, NI_MAXSERV, 0))) {
			*error_message =
			    g_strdup_printf(_
					    ("Error resolving address: %s"),
					    gai_strerror(err));
			return FALSE;
		} else {
			g_free(*hostname);
			g_free(*servname);
			*hostname = g_strdup(host);
			*servname = g_strdup(port);
			return TRUE;
		}
	}
#else /* HAVE_GETADDRINFO_ET_AL */
	*error_message = g_strdup(_("Net_get_peer_name not yet supported on this platform."));
	return FALSE;
#endif /* HAVE_GETADDRINFO_ET_AL */
}

gint net_accept(gint accept_fd, gchar ** error_message)
{
	gint fd;
	sockaddr_t addr;
	socklen_t addr_len;

	addr_len = sizeof(addr);
	fd = accept(accept_fd, &addr.sa, &addr_len);
	if (fd < 0) {
		*error_message =
		    g_strdup_printf(_("Error accepting connection: %s"),
				    g_strerror(errno));
	}
	return fd;
}
