/***************************************************************************
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.              *
 ***************************************************************************/

#include <dmtxplugin-gdbus.h>

struct context_bdaddr_data {
	gboolean found;
	char *bdaddr;
};

struct context_len_data {
	gboolean found;
	int len;
};

struct context_oobtags_data {
	gboolean found_len;
	gboolean found_datatype;
	gboolean found_data;
	char *oobtags_len;
	char *oobtags_datatype;
	char *oobtags_data;
	char *oobtags;
};

static DBusConnection *conn = NULL;

static void element_bdaddr_start(GMarkupParseContext *context,
		const gchar *element_name, const gchar **attribute_names,
		const gchar **attribute_values, gpointer user_data, GError **err)
{
	struct context_bdaddr_data *ctx_data = user_data;

	if (!strcmp(element_name, "bdaddr")) {
		return;
	}

	if (!strcmp(element_name, "text") && !(ctx_data->found)) {
		int i;
		for (i = 0; attribute_names[i]; i++) {
			if (strcmp(attribute_names[i], "value") == 0) {
				ctx_data->bdaddr = g_strdup(attribute_values[i]);
				ctx_data->found = TRUE;
                                /* printf(" bddaddr : %s \n", ctx_data->bdaddr ); */
			}
		}
	}

}

static void element_len_start(GMarkupParseContext *context,
		const gchar *element_name, const gchar **attribute_names,
		const gchar **attribute_values, gpointer user_data, GError **err)
{
	struct context_len_data *ctx_data = user_data;

	if (!strcmp(element_name, "length")) {
		return;
	}

	if (!strcmp(element_name, "unit16") && !(ctx_data->found)) {
		int i;
		for (i = 0; attribute_names[i]; i++) {
			if (strcmp(attribute_names[i], "value") == 0) {
				ctx_data->len = atoi(attribute_values[i]);
				ctx_data->found = TRUE;
                                printf(" len: %d \n", ctx_data->len );
			}
		}
	}

}

static void element_oobtags_start(GMarkupParseContext *context,
		const gchar *element_name, const gchar **attribute_names,
		const gchar **attribute_values, gpointer user_data, GError **err)
{
	struct context_bdaddr_data *ctx_data = user_data;

	if (!strcmp(element_name, "eirtag")) {
		return;
	}

        if (!strcmp(element_name, "length")) {
		return;
	}

        struct eir_tag tag;
        char len[3]; /* FIXME: how many bytes to hold a hex value? */

        /* parse tag's length*/
	if (!strcmp(element_name, "unit8") && !(ctx_data->found_len)) {
		int i;
		for (i = 0; attribute_names[i]; i++) {
			if (strcmp(attribute_names[i], "value") == 0) {
			        /*TODO:  we should discard 0x from the beginning */
			        itoa(attribute_values[i], len, 16);
				ctx_data->oobtags_len = g_strconcat(ctx_data->oobtags_len, len);
				ctx_data->found_len = TRUE; /* FIXME: how to use in this case? */
                                /* printf(" cod : %s \n", ctx_data->oobtags ); */
			}
		}
	}

        /* TODO: parse eirdatatypes */

        /* TODO: parse eirdata */

}

static GMarkupParser bdaddr_parser = {
	element_bdaddr_start, NULL, NULL, NULL, NULL
};

static GMarkupParser len_parser = {
	element_len_start, NULL, NULL, NULL, NULL
};

static GMarkupParser oobtags_parser = {
	element_oobtags_start, NULL, NULL, NULL, NULL
};

static char *dmtxplugin_xml_parse_bdaddr(const char *data)
{
	GMarkupParseContext *ctx;
	struct context_bdaddr_data ctx_data;
	int size;

	size = strlen(data);
	printf("XML parser: start parsing with data size %d\n", size);

	ctx_data.found = FALSE;
	ctx_data.bdaddr = NULL;
	ctx = g_markup_parse_context_new(&bdaddr_parser, 0, &ctx_data, NULL);

	if (g_markup_parse_context_parse(ctx, data, size, NULL) == FALSE) {
		g_markup_parse_context_free(ctx);
		g_free(ctx_data.bdaddr);
		return NULL;
	}

	g_markup_parse_context_free(ctx);

	return ctx_data.bdaddr;
}

int dmtxplugin_xml_parse_len(char *data)
{
	GMarkupParseContext *ctx;
	struct context_len_data ctx_data;
	int size;

	size = strlen(data);
	printf("XML parser: start parsing with data size %d\n", size);

	ctx_data.found = FALSE;
	ctx_data.bdaddr = NULL;
	ctx = g_markup_parse_context_new(&len_parser, 0, &ctx_data, NULL);

	if (g_markup_parse_context_parse(ctx, data, size, NULL) == FALSE) {
		g_markup_parse_context_free(ctx);
		return NULL;
	}

	g_markup_parse_context_free(ctx);

	return ctx_data.len;
}

void dmtxplugin_xml_parse_oobtags(char *data)
{
	GMarkupParseContext *ctx;
	struct context_oobtags_data ctx_data;
	int size;

	size = strlen(data);
	printf("XML parser: start parsing with data size %d\n", size);

	ctx_data.found = FALSE;
	ctx_data.oobtags = NULL;
	ctx = g_markup_parse_context_new(&oobtags_parser, 0, &ctx_data, NULL);

	if (g_markup_parse_context_parse(ctx, data, size, NULL) == FALSE) {
		g_markup_parse_context_free(ctx);
		g_free(ctx_data.oobtags);
		return NULL;
	}

	g_markup_parse_context_free(ctx);

        /*TODO: join tags len1+type1+data1+len2+... into oobtags */
	return ctx_data.oobtags;
}

static char *gdbus_device_create(const char *adapter, char *bdaddr)
{
	DBusMessage *message, *reply, *adapter_reply;
	DBusMessageIter iter;

	char *object_path = NULL;;
	adapter_reply = NULL;

	conn = g_dbus_setup_bus(DBUS_BUS_SYSTEM, NULL, NULL);
	/* printf("Dbus conn: %x\n", conn); */
	if (conn == NULL)
                return NULL;

	if (adapter == NULL) {
		message = dbus_message_new_method_call("org.bluez", "/",
						       "org.bluez.Manager",
						       "DefaultAdapter");

		adapter_reply = dbus_connection_send_with_reply_and_block(conn,
                                                                message, -1, NULL );
                if (adapter_reply == NULL) {
                        printf("Bluetoothd or adapter unavailable\n");
                        return NULL;
                } else {
                        dbus_message_unref(adapter_reply);
                }

		if (dbus_message_get_args(adapter_reply, NULL, DBUS_TYPE_OBJECT_PATH,
                                        &adapter, DBUS_TYPE_INVALID) == FALSE )
			return NULL;
	}

        printf("Bluetoothd adapter path: %s\n", adapter);

        message = dbus_message_new_method_call("org.bluez", adapter,
                                               "org.bluez.Dmtx",
                                               "CreateOOBDevice");
        dbus_message_iter_init_append(message, &iter);
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &bdaddr);

        reply = dbus_connection_send_with_reply_and_block(conn,
                                                          message, -1, NULL );
        if (!reply)
                return NULL;

        if (dbus_message_get_args(reply, NULL, DBUS_TYPE_OBJECT_PATH, &object_path,
                                                        DBUS_TYPE_INVALID) == FALSE)
                return NULL;

	dbus_message_unref(reply);

	dbus_connection_unref(conn);

	return object_path;
}

void dmtxplugin_gdbus_create_device(char *data)
{
        char *bdaddr;
        char *device_path;
        device_path = NULL;

        bdaddr = dmtxplugin_xml_parse_bdaddr(data);
        printf("Decoded bdadd: %s \n", bdaddr);

        device_path = gdbus_device_create(NULL, bdaddr);

        if (device_path)
                 printf("Device created on path: %s \n ", device_path);
        else
                printf("No response from plugin \nDevice creation failed \n");
}

static void gdbus_create_paired_device(const char *adapter, char *bdaddr,
 uint8_t *oobtags, int len)
{

}

void dmtxplugin_gdbus_create_paired_oob_device(char *data)
{
        /* test xml file and either pass as raw xml or oob data
         first test as oob data */
        char *bdaddr;
        char *device_path;
        uint8_t *oobtags;
	int len, optional_len;
        device_path = NULL;

        bdaddr = dmtxplugin_xml_parse_bdaddr(data);
        printf("Decoded bdadd: %s \n", bdaddr);

        oobtags = dmtxplugin_xml_parse_oobtags(data);
        /* parse length field and set len as get the length of optional oobtags  */
        len = dmtxplugin_xml_parse_len(data);
        optional_len = len - 8; /* Lengh field 2 Bytes + BDADDR 6 Bytes*/

        device_path = gdbus_create_paired_device(NULL, bdaddr, oobtags, optional_len);

        if (device_path)
                 printf("Paired Device created on path: %s \n ", device_path);
        else {
                printf("No response from plugin \n");
                printf("Paired Device creation failed\n");
        }

}

