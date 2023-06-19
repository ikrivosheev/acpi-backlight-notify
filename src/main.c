#include <gio/gio.h>
#include <glib.h>
#include <libnotify/notification.h>
#include <libnotify/notify.h>
#include <locale.h>
#include <math.h>

#define PROGRAM_NAME "backlight-notify"
#define DEFAULT_DEBUG FALSE
#define SYS_CLASS_PATH "/sys/class/backlight/"

typedef struct _Context
{
    NotifyNotification* notification;
    GFile* max_brightness;
    GFile* actual_brightness;
    gint64 backlight;
} Context;

static struct config
{
    gboolean debug;
    gint timeout;
    gchar* backlight;
} config = {
    DEFAULT_DEBUG,
    NOTIFY_EXPIRES_DEFAULT,
    NULL,
};

static GOptionEntry option_entries[] = {
    { "debug", 'd', 0, G_OPTION_ARG_NONE, &config.debug, "Enable/disable debug information", NULL },
    { "timeout",
      't',
      0,
      G_OPTION_ARG_INT,
      &config.timeout,
      "Notification timeout in seconds (-1 - default notification timeout, 0 - notification never "
      "expires)",
      NULL },
    { "backlight",
      'b',
      0,
      G_OPTION_ARG_STRING,
      &config.backlight,
      "/sys/class/backlight/<backlight>",
      NULL },
    { NULL }
};

void
context_init(Context* context, gchar* backlight)
{
    context->notification = notify_notification_new(NULL, NULL, NULL);
    context->max_brightness =
      g_file_new_build_filename(SYS_CLASS_PATH, backlight, "/max_brightness", NULL);
    context->actual_brightness =
      g_file_new_build_filename(SYS_CLASS_PATH, backlight, "/actual_brightness", NULL);
    context->backlight = -1;
}

void
context_free(Context* context)
{
    g_object_unref(context->notification);
    g_object_unref(context->actual_brightness);
    g_object_unref(context->max_brightness);
}

static gchar*
notify_icon(gint brightness)
{
    if (brightness >= 66) {
        return "notification-display-brightness-high";
    } else if (brightness >= 33) {
        return "notification-display-brightness-medium";
    } else {
        return "notification-display-brightness-low";
    }
}

static void
notify_message(NotifyNotification* notification,
               const gchar* summary,
               const gchar* body,
               NotifyUrgency urgency,
               const gchar* icon,
               gint timeout,
               gint brightness)
{
    notify_notification_update(notification, summary, body, icon);
    notify_notification_set_timeout(notification, timeout);
    notify_notification_set_urgency(notification, urgency);
    if (brightness >= 0) {
        GVariant* g_var = g_variant_new_int32(brightness);
        notify_notification_set_hint(notification, "value", g_var);
    } else {
        notify_notification_set_hint(notification, "value", NULL);
    }

    notify_notification_show(notification, NULL);
}

static gboolean
read_int(GFile* file, gint64* value, GError** error)
{
    gchar* content;

    char* filename = g_file_get_path(file);
    g_return_val_if_fail(g_file_get_contents(filename, &content, NULL, error), FALSE);
    *value = g_ascii_strtoll(content, NULL, 10);
    return TRUE;
}

void
backlight_callback(GFileMonitor* monitor,
                   GFile* file,
                   GFile* other,
                   GFileMonitorEvent evtype,
                   gpointer userdata)
{
    Context* context = (Context*)userdata;

    switch (evtype) {
        case G_FILE_MONITOR_EVENT_CHANGED:

            float perc;
            GError* err = NULL;
            gint64 actual, max, nearest_5;

            read_int(file, &actual, &err);
            if (err) {
                g_warning("Cannot read actual_brightness: %s", err->message);
                g_error_free(err);
                return;
            }

            if (context->backlight != actual) {
                g_debug("actual_brightness is changed");

                read_int(context->max_brightness, &max, &err);
                if (err) {
                    g_warning("Cannot read max_brightness: %s", err->message);
                    g_error_free(err);
                    return;
                }

                perc = (actual / (float)max) * 100.0f;
                nearest_5 = (int)(perc / 5.0 + 0.5) * 5.0;
                notify_message(context->notification,
                               "Backlight",
                               NULL,
                               NOTIFY_URGENCY_LOW,
                               notify_icon(nearest_5),
                               config.timeout,
                               nearest_5);

                context->backlight = actual;
            }
            break;
        default:
            break;
    }
}

static gboolean
options_init(int argc, char* argv[])
{
    GError* error = NULL;
    GOptionContext* option_context;

    option_context = g_option_context_new(NULL);
    g_option_context_add_main_entries(option_context, option_entries, PROGRAM_NAME);

    if (g_option_context_parse(option_context, &argc, &argv, &error) == FALSE) {
        g_warning("Cannot parse command line arguments: %s", error->message);
        g_error_free(error);
        return FALSE;
    }

    g_option_context_free(option_context);

    if (config.backlight == NULL) {
        g_warning("-b/--backlight is required argument");
        return FALSE;
    }

    if (config.debug == TRUE)
        g_log_set_handler(NULL, G_LOG_LEVEL_DEBUG, g_log_default_handler, NULL);
    else
        g_log_set_handler(NULL,
                          G_LOG_LEVEL_INFO | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL |
                            G_LOG_LEVEL_ERROR,
                          g_log_default_handler,
                          NULL);

    if (config.timeout > 0) {
        config.timeout *= 1000;
    }

    return TRUE;
}

int
main(int argc, char* argv[])
{
    Context context;
    setlocale(LC_ALL, "");

    g_return_val_if_fail(options_init(argc, argv), 1);
    g_info("Options have been initialized");

    context_init(&context, config.backlight);
    g_return_val_if_fail(notify_init(PROGRAM_NAME), 1);
    g_info("Notify has been initialized");

    GMainLoop* loop = g_main_loop_new(NULL, FALSE);

    GError* err = NULL;
    GFileMonitor* fm = g_file_monitor(context.actual_brightness, G_FILE_MONITOR_NONE, NULL, &err);
    if (err) {
        g_warning("Unable to monitor: %s", err->message);
        g_error_free(err);
        notify_uninit();
        context_free(&context);
        g_object_unref(loop);
        return 1;
    }
    g_signal_connect(G_OBJECT(fm), "changed", G_CALLBACK(backlight_callback), &context);
    g_main_loop_run(loop);

    notify_uninit();
    context_free(&context);
    g_object_unref(loop);

    return 0;
}
