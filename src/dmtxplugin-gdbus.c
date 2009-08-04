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


#define EIR_TAG_MAX_LEN 1024

static int tag_count = 0;
static char tag_buff[EIR_TAG_MAX_LEN];
static int offset = 0;
static char *ptr;

struct eir_tag {
        uint8_t len;
        uint8_t type;
        uint8_t *data;
};

struct eir_tag *tag;

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
	gboolean found_bdaddr; /* To avoid mismatch with bdaddr */
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
                                /*printf(" bddaddr : %s \n", ctx_data->bdaddr ); */
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
				sscanf(attribute_values[i], "%x", &ctx_data->len);
				ctx_data->found = TRUE;
                                /* printf("Got len: %x \n", ctx_data->len ); */
			}
		}
	}

}

static void element_oobtags_start(GMarkupParseContext *context,
		const gchar *element_name, const gchar **attribute_names,
		const gchar **attribute_values, gpointer user_data, GError **err)
{
	int i, l, t, cod;
	struct context_oobtags_data *ctx_data = user_data;
        tag = (struct eir_tag *) tag_buff + offset;

	if (!strcmp(element_name, "eirtag")) {
	        //printf("===== New eir tag found=== \n");
	        tag_count++;
	        //printf("***offset***: %d \n", offset);
		return;
	}

        ctx_data->found_len = FALSE;

        /* parse tag by tag */
        if (!strcmp(element_name, "len")) {
                //printf("tag %d len start\n", tag_count);
                return;
        }

         if(!strcmp(element_name, "unit8") && !(ctx_data->found_len)) {
		for (i = 0; attribute_names[i]; i++) {
		        //printf(" attb val %s \n", attribute_values[i]);
			if (strcmp(attribute_names[i], "value") == 0) {
				sscanf(attribute_values[i], "%x", &l);
                                tag->len = l; /*FIXME : can't find the value later */
                                //printf(" tag len %x \n", tag->len);
                                ptr = tag_buff;
                                strncat(ptr, attribute_values[i]+2, 2 );
                                ctx_data->found_len = TRUE;
                                offset += sizeof(l);
			}
		}
		return;
         }

        ctx_data->found_datatype = FALSE;

        if (!strcmp(element_name, "eirdatatype")) {
                //printf("tag %d datatype start\n", tag_count);
                return;
        }

        if (!strcmp(element_name, "unit8") && !(ctx_data->found_datatype)) {
		for (i = 0; attribute_names[i]; i++) {
			if (strcmp(attribute_names[i], "value") == 0) {
				sscanf(attribute_values[i], "%x", &t);
                                tag->type =  t;
                                //printf(" tag type %x \n", tag->type);
                                ctx_data->found_datatype = TRUE;
                                ptr = ptr + 2;
                                strncat(ptr, attribute_values[i]+2, 2 );
			}
		}
		return;
	}

        ctx_data->found_data = FALSE;

        if (!strcmp(element_name, "eirdata")) {
                //printf("tag %d data start\n", tag_count);
                ctx_data->found_bdaddr = TRUE;
                return;
        }

        if (!strcmp(element_name, "text") && !(ctx_data->found_data)
                && ctx_data->found_bdaddr) {
		for (i = 0; attribute_names[i]; i++) {
			if (strcmp(attribute_names[i], "value") == 0) {
				tag->data = (uint8_t *) attribute_values[i];
                                //printf(" tag data %s \n", tag->data);
                                ctx_data->found_data = TRUE;
                                ptr = ptr + 2;
                                strncat(ptr, (char *) tag->data, 32 );
			}
		}
		return;
	}
	if ( !strcmp(element_name, "unit32") && !(ctx_data->found_data)
                && (ctx_data->found_bdaddr)) {
		for (i = 0; attribute_names[i]; i++) {
			if (strcmp(attribute_names[i], "value") == 0) {
			        sscanf(attribute_values[i], "%x", &cod);
			        char buff[10];
			        sprintf(buff, "%x", cod);
				tag->data = (uint8_t *) buff;
                                //printf(" cod tag data %s \n", tag->data);
                                ctx_data->found_data = TRUE;
                                ptr = ptr + 2;
                                strncat(ptr, (char *) tag->data+2, 6 );
			}
		}
		return;
        }
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
	int size, ret;
        GError *error;
	size = strlen(data);
	printf("XML parser: start parsing with data size %d\n", size);

	ctx_data.found = FALSE;
	ctx_data.bdaddr = NULL;
	ctx = g_markup_parse_context_new(&bdaddr_parser, 0, &ctx_data, NULL);

        ret = g_markup_parse_context_parse(ctx, data, size, &error);

        if (ret == FALSE) {
                printf("parser returned %d error : %s \n", ret, error->message );
		g_free(ctx_data.bdaddr);
        }

	g_markup_parse_context_free(ctx);

	return ctx_data.bdaddr;
}

int dmtxplugin_xml_parse_len(char *data)
{
	GMarkupParseContext *ctx;
	struct context_len_data ctx_data;
	int size, ret;
	GError *error;

	size = strlen(data);
	//printf("XML parser: start parsing with data size %d\n", size);

	ctx_data.found = FALSE;
	ctx_data.len = 0;
	ctx = g_markup_parse_context_new(&len_parser, 0, &ctx_data, NULL);

        ret = g_markup_parse_context_parse(ctx, data, size, &error);
	if ( ret == FALSE)
	        printf("parser returned %d error : %s \n", ret, error->message );

	g_markup_parse_context_free(ctx);

	return ctx_data.len;
}

static char *dmtxplugin_xml_parse_oobtags(char *data)
{
	GMarkupParseContext *ctx;
	struct context_oobtags_data ctx_data;
	int size, ret;
	GError *error;

	size = strlen(data);
	//printf("XML parser: start parsing with data size %d\n", size);

	ctx_data.oobtags = NULL;
        ctx_data.found_len = FALSE;
	ctx_data.found_datatype= FALSE;
	ctx_data.found_data = FALSE;
	ctx_data.found_bdaddr = FALSE;
        offset = 0;
        tag_buff[EIR_TAG_MAX_LEN - 1] = '\0';

	ctx = g_markup_parse_context_new(&oobtags_parser, 0, &ctx_data, NULL);

        ret = g_markup_parse_context_parse(ctx, data, size, &error);
	if (ret == FALSE)
                printf("parser returned %d error : %s \n", ret, error->message );


        g_markup_parse_context_free(ctx);

	return tag_buff;
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

static char *gdbus_create_paired_device(const char *adapter, char *bdaddr,
 char *oobtags, int len)
{
        char *device_path = NULL;
        /* TODO */
        return device_path;
}

void dmtxplugin_gdbus_create_paired_oob_device(char *data)
{
        /* test xml file and either pass as raw xml or oob data
         first test as oob data */
        char *bdaddr;
        char *device_path;
        char *oobtags;
	int len, optional_len;
        device_path = NULL;

        bdaddr = dmtxplugin_xml_parse_bdaddr(data);
        printf("Decoded bdadd: %s \n", bdaddr);
        /* parse length field and set len as get the length of optional oobtags  */
        len = dmtxplugin_xml_parse_len(data);
        printf("Decoded len: %d \n", len);
        optional_len = len - 8; /* Lengh field 2 Bytes + BDADDR 6 Bytes*/

        oobtags = dmtxplugin_xml_parse_oobtags(data);
        printf("Decoded oobtags: %s \n", oobtags);


        device_path = gdbus_create_paired_device(NULL, bdaddr, oobtags, optional_len);

        if (device_path)
                 printf("Paired Device created on path: %s \n ", device_path);
        else {
                printf("No response from plugin \n");
                printf("Paired Device creation failed\n");
        }

}
