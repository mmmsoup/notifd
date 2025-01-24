#include <errno.h>
#include <gio/gio.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "org_freedesktop_notifications.h"

#define PATH_DELIM		':'

#define TIMEOUT			5000000 // microseconds

#define DEBUG_ENABLED	1

#define BUS_NAME		"org.freedesktop.Notifications"
#define OBJECT_PATH		"/org/freedesktop/Notifications"

#define NAME			"notifd"
#define VENDOR			"mmmsoup"
#define VERSION			"1.1.0"
#define SPEC_VERSION	"1.2"

#define LOG(colour, format, ...) fprintf(stderr, "\e[1;%im[%s:%i]\e[0m " format "\n", colour, __FILE__, __LINE__, ##__VA_ARGS__);

#define NOTE(format, ...) LOG(33, format, ##__VA_ARGS__);

#define ERR(format, ...) LOG(31, format, ##__VA_ARGS__);
#define ERR_PTR(str) ERR("%s", str);
#define ERR_ERRNO(format, ...) ERR(format ": %s", ##__VA_ARGS__, strerror(errno));

#define DEBUG(format, ...) if (DEBUG_ENABLED) LOG(1, format, ##__VA_ARGS__);

#define ENV_ID			"NOTIF_ID"
#define ENV_APP_NAME	"NOTIF_APP_NAME"
#define ENV_APP_ICON	"NOTIF_APP_ICON"
#define ENV_SUMMARY		"NOTIF_SUMMARY"
#define ENV_BODY		"NOTIF_BODY"

#define DEFAULT_NOTIFY_SCRIPT "~/.config/notifd.sh"
char *notify_cmd = DEFAULT_NOTIFY_SCRIPT " &";

static pthread_mutex_t show_notif_mutex = PTHREAD_MUTEX_INITIALIZER;

unsigned int num_digits(unsigned int num) {
	unsigned int digits = 0;
	while (num > 0) {
		num /= 10;
		digits++;
	}
	return digits;
}

static void method_callback(GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name, const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer data) {
	if (strcmp(method_name, "GetServerInformation") == 0) {
		DEBUG("'GetServerInformation' method called");
		g_dbus_method_invocation_return_value(invocation, g_variant_new("(ssss)", NAME, VENDOR, VERSION, SPEC_VERSION));
	} else if (strcmp(method_name, "GetCapabilities") == 0) {
		DEBUG("'GetCapabilities' method called");

		// https://specifications.freedesktop.org/notification-spec/latest/protocol.html
		const gchar *capabilities[] = {
			//"action-icons",
			//"actions",
			"body",
			"body-hyperlinks",
			//"body-images",
			"body-markup",
			//"icon-multi",
			"icon-static",
			//"persistence",
			//"sound",
		};

		GVariantBuilder builder;

		g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
		for (int i = 0; i < sizeof(capabilities)/sizeof(gchar*); i++) {
			g_variant_builder_add(&builder, "s", capabilities[i]);
		}
		GVariant *array = g_variant_builder_end(&builder);

		g_variant_builder_init(&builder, G_VARIANT_TYPE("(as)"));
		g_variant_builder_add(&builder, "@as", array);
		GVariant *ret = g_variant_builder_end(&builder);
		
		g_dbus_method_invocation_return_value(invocation, ret);
	} else if (strcmp(method_name, "Notify") == 0) {
		DEBUG("'Notify' method called");

		pthread_mutex_lock(&show_notif_mutex);

		GVariant *parameters = g_dbus_method_invocation_get_parameters(invocation);

		gchar *app_name;
		guint replaces_id;
		gchar *app_icon;
		gchar *summary;
		gchar *body;
		GVariant *actions;
		GVariant *hints;
		gint expire_timeout;

		g_variant_get(parameters, "(susssasa{sv}i)", &app_name, &replaces_id, &app_icon, &summary, &body, &actions, &hints, &expire_timeout);

	  	guint id = (guint)random();
		char *str_id = malloc(sizeof(char)*(num_digits(id)+1));
		sprintf(str_id, "%i", id);

		setenv(ENV_ID, str_id, 1);
		setenv(ENV_APP_NAME, app_name, 1);
		setenv(ENV_APP_ICON, app_icon, 1);
		setenv(ENV_SUMMARY, summary, 1);
		setenv(ENV_BODY, body, 1);

		system(notify_cmd);

		free(str_id);

		g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", id));

		pthread_mutex_unlock(&show_notif_mutex);
	} else {
		NOTE("no handling for '%s' method", method_name);
	}

	return;
}

static const GDBusInterfaceVTable interface_vtable = {
	method_callback, // method call
	NULL, // get property
	NULL // set property
};

static void on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer data) {
	GDBusNodeInfo *introspection = g_dbus_node_info_new_for_xml(org_freedesktop_notifications_xml, NULL);

	guint object_id = g_dbus_connection_register_object(connection, OBJECT_PATH, introspection->interfaces[0], &interface_vtable, NULL, NULL, NULL);
	
	if (object_id == 0) {
		ERR_ERRNO("g_dbus_connection_register_object()");
		exit(EXIT_FAILURE);
	}

	return;
}

static void on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer data) {
	return;
}

static void on_name_lost(GDBusConnection *connection, const gchar *name, gpointer data) {
	NOTE("name '%s' claimed by another message bus connection", name);
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
	GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);

	if (connection == NULL) {
		ERR_ERRNO("g_bus_get_sync()");
		exit(EXIT_FAILURE);
	}

	GMainLoop *loop = g_main_loop_new(NULL, 0);

	guint bus_id = g_bus_own_name(G_BUS_TYPE_SESSION, BUS_NAME, G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT | G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE, on_bus_acquired, on_name_acquired, on_name_lost, NULL, NULL);

	if (bus_id == 0) {
		ERR_ERRNO("g_bus_own_name()");
		exit(EXIT_FAILURE);
	}

	g_main_loop_run(loop);

	return EXIT_SUCCESS;
}
