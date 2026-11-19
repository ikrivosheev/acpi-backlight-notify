#include <gio/gio.h>
#include <glib.h>
#include <libnotify/notification.h>
#include <libnotify/notify.h>
#include <locale.h>

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

static void
context_free(Context* context)
{
    g_clear_object(&context->notification);
    g_clear_object(&context->actual_brightness);
    g_clear_object(&context->max_brightness);
}

static gboolean
context_init(Context* context, const gchar* backlight)
{
    context->max_brightness =
      g_file_new_build_filename(SYS_CLASS_PATH, backlight, "/max_brightness", NULL);
    context->actual_brightness =
      g_file_new_build_filename(SYS_CLASS_PATH, backlight, "/actual_brightness", NULL);

    if (!g_file_query_exists(context->max_brightness, NULL) ||
        !g_file_query_exists(context->actual_brightness, NULL)) {
        gchar* name = g_file_get_basename(context->max_brightness);
        g_warning(
          "Backlight device '%s' not found in %s (no %s file)", backlight, SYS_CLASS_PATH, name);
        g_free(name);
        context_free(context);
        return FALSE;
    }

    context->notification = notify_notification_new(NULL, NULL, NULL);
    context->backlight = -1;
    return TRUE;
}

static const gchar*
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
        notify_notification_set_hint(notification, "value", g_variant_new_int32(brightness));
    } else {
        notify_notification_set_hint(notification, "value", NULL);
    }

    GError* error = NULL;
    if (notify_notification_show(notification, &error) == FALSE) {
        g_warning("Cannot show notification: %s", error->message);
        g_error_free(error);
    }
}

static gboolean
read_int(GFile* file, gint64* value, GError** error)
{
    gchar* content = NULL;

    if (g_file_load_contents(file, NULL, &content, NULL, NULL, error) == FALSE) {
        return FALSE;
    }

    *value = g_ascii_strtoll(content, NULL, 10);
    g_free(content);
    return TRUE;
}

static void
backlight_callback(GFileMonitor* monitor,
                   GFile* file,
                   GFile* other,
                   GFileMonitorEvent evtype,
                   gpointer userdata)
{
    Context* context = (Context*)userdata;

    (void)monitor;
    (void)other;

    switch (evtype) {
        case G_FILE_MONITOR_EVENT_CHANGED: {
            GError* err = NULL;
            gint64 actual, max, nearest_5;

            if (read_int(file, &actual, &err) == FALSE) {
                g_warning("Cannot read actual_brightness: %s", err->message);
                g_error_free(err);
                return;
            }

            if (context->backlight == actual) {
                return;
            }

            g_debug("actual_brightness is changed");

            if (read_int(context->max_brightness, &max, &err) == FALSE) {
                g_warning("Cannot read max_brightness: %s", err->message);
                g_error_free(err);
                return;
            }

            if (max <= 0) {
                g_warning("max_brightness is not positive");
                return;
            }

            gfloat perc = (actual / (gfloat)max) * 100.0f;
            nearest_5 = (gint64)(perc / 5.0f + 0.5f) * 5;

            notify_message(context->notification,
                           "Backlight",
                           NULL,
                           NOTIFY_URGENCY_LOW,
                           notify_icon((gint)nearest_5),
                           config.timeout,
                           (gint)nearest_5);

            context->backlight = actual;
            break;
        }
        default:
            break;
    }
}

static gboolean
options_init(int argc, char* argv[])
{
    GError* error = NULL;
    GOptionContext* option_context;
    gboolean parsed;

    option_context = g_option_context_new(NULL);
    g_option_context_add_main_entries(option_context, option_entries, PROGRAM_NAME);

    parsed = g_option_context_parse(option_context, &argc, &argv, &error);
    g_option_context_free(option_context);

    if (parsed == FALSE) {
        g_warning("Cannot parse command line arguments: %s", error->message);
        g_error_free(error);
        return FALSE;
    }

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
        if (config.timeout > G_MAXINT / 1000) {
            g_warning("-t/--timeout is too large, clamping to %d seconds", G_MAXINT / 1000);
            config.timeout = G_MAXINT / 1000;
        }
        config.timeout *= 1000;
    }

    return TRUE;
}

int
main(int argc, char* argv[])
{
    Context context;
    setlocale(LC_ALL, "");

    if (options_init(argc, argv) == FALSE) {
        g_free(config.backlight);
        return 1;
    }
    g_info("Options have been initialized");

    if (notify_init(PROGRAM_NAME) == FALSE) {
        g_warning("Cannot initialize libnotify");
        g_free(config.backlight);
        return 1;
    }
    g_info("Notify has been initialized");

    if (context_init(&context, config.backlight) == FALSE) {
        notify_uninit();
        g_free(config.backlight);
        return 1;
    }

    GMainLoop* loop = g_main_loop_new(NULL, FALSE);

    GError* err = NULL;
    GFileMonitor* fm = g_file_monitor(context.actual_brightness, G_FILE_MONITOR_NONE, NULL, &err);
    if (fm == NULL) {
        g_warning("Unable to monitor: %s", err->message);
        g_error_free(err);
        g_main_loop_unref(loop);
        context_free(&context);
        notify_uninit();
        g_free(config.backlight);
        return 1;
    }
    g_signal_connect(fm, "changed", G_CALLBACK(backlight_callback), &context);
    g_main_loop_run(loop);

    g_object_unref(fm);
    g_main_loop_unref(loop);
    notify_uninit();
    context_free(&context);
    g_free(config.backlight);

    return 0;
}
