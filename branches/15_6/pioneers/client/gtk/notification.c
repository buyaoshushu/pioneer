#include "config.h"
#include "notification.h"
#include "frontend.h"

#ifdef HAVE_NOTIFY
#include <libnotify/notify.h>
#include <libnotify/notification.h>
#endif

static gboolean show_notifications = TRUE;
#ifdef HAVE_NOTIFY
static NotifyNotification *notification = NULL;
#endif

void notification_init(void)
{
#ifdef HAVE_NOTIFY
	notify_init(g_get_application_name());
#endif
}

void notification_cleanup(void)
{
#ifdef HAVE_NOTIFY
	notification_close();
	notify_uninit();
#endif
}

gboolean get_show_notifications(void)
{
	return show_notifications;
}

void set_show_notifications(gboolean notify)
{
	show_notifications = notify;
}

void notification_send(const gchar * text, const gchar * icon)
{
#ifdef HAVE_NOTIFY
	notification_close();
	if (show_notifications) {
		GdkPixbuf *pixbuf;

		pixbuf =
		    gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
					     icon, 24, 0, NULL);
		notification =
		    notify_notification_new(g_get_application_name(), text,
					    NULL);
		notify_notification_set_image_from_pixbuf(notification,
							  pixbuf);
		g_object_unref(pixbuf);
		notify_notification_set_urgency(notification,
						NOTIFY_URGENCY_LOW);
		notify_notification_show(notification, NULL);
	}
#endif
}

void notification_close(void)
{
#ifdef HAVE_NOTIFY
	if (notification != NULL) {
		notify_notification_close(notification, NULL);
		g_object_unref(notification);
		notification = NULL;
	}
#endif
}
