/*
 * This file is part of the KNOT Project
 *
 * Copyright (c) 2018, CESAR. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdio.h>

#include <ell/ell.h>

#include <knot/knot_types.h>
#include <knot/knot_protocol.h>

#include <json-c/json.h>

#include "parser.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

/*
 * TODO: consider moving this to knot-protocol
 */
static uint64_t knot_value_as_uint64(const knot_value_type *data)
{
	return data->val_u64;
}

/*
 * TODO: consider moving this to knot-protocol
 */
static uint32_t knot_value_as_uint(const knot_value_type *data)
{
	return data->val_u;
}

/*
 * TODO: consider moving this to knot-protocol
 */
static int64_t knot_value_as_int64(const knot_value_type *data)
{
	return data->val_i64;
}

static char *knot_value_as_raw(const knot_value_type *data,
			       uint8_t kval_len, size_t *encoded_len)
{
	char *encoded;
	size_t olen;

	encoded = l_base64_encode(data->raw, kval_len, 0, &olen);
	if (!encoded)
		return NULL;

	*encoded_len = olen;

	return encoded;
}

/*
 * TODO: consider moving this to knot-protocol
 */
static bool knot_value_as_boolean(const knot_value_type *data)
{
	return data->val_b;
}

/*
 * TODO: consider moving this to knot-protocol
 */
static double knot_value_as_double(const knot_value_type *data)
{
	return (double) data->val_f;
}

/*
 * TODO: consider moving this to knot-protocol
 */
static int knot_value_as_int(const knot_value_type *data)
{
	return data->val_i;
}

static char *get_str_value_from_json(json_object *jso, const char *key)
{
	char *str_value;
	json_object *jobjkey;

	if (!json_object_object_get_ex(jso, key, &jobjkey))
		return NULL;

	if (json_object_get_type(jobjkey) != json_type_string)
		return NULL;

	str_value = l_strdup(json_object_get_string(jobjkey));

	return str_value;
}

static void *device_array_item(json_object *array_item,
		create_device_item_cb cb)
{
	json_object *jobjkey;
	struct l_queue *schema;
	const char *id, *name;

	/* Getting 'Id': Mandatory field for registered device */
	id = get_str_value_from_json(array_item,
		KNOT_JSON_FIELD_DEVICE_ID);
	if (!id)
		return NULL;

	/* Getting 'schema': Mandatory field for registered device */
	/* FIXME: Call a parser function instead of json-c function*/
	if (!json_object_object_get_ex(array_item,
			KNOT_JSON_FIELD_SCHEMA, &jobjkey))
		return NULL;

	schema = parser_schema_to_list(json_object_to_json_string(jobjkey));
	if (!schema)
		return NULL;

	/* Getting 'Name' */
	name = get_str_value_from_json(array_item,
		KNOT_JSON_FIELD_DEVICE_NAME);
	if (!name)
		return NULL;
	// l_info("%s",name);
	return cb(id, name, schema);
}

/*
 * Parsing knot_value_type attribute
 */
static int parse_json2data(json_object *jobj, knot_value_type *kvalue)
{
	json_object *jobjkey;
	const char *str;
	uint8_t *u8val;
	size_t olen = 0;

	jobjkey = jobj;
	switch (json_object_get_type(jobjkey)) {
	case json_type_boolean:
		kvalue->val_b = json_object_get_boolean(jobjkey);
		olen = sizeof(kvalue->val_b);
		break;
	case json_type_double:
		/* FIXME: how to handle overflow? */
		kvalue->val_f = (float) json_object_get_double(jobjkey);
		olen = sizeof(kvalue->val_f);
		break;
	case json_type_int:
		kvalue->val_i = json_object_get_int(jobjkey);
		olen = sizeof(kvalue->val_i);
		break;
	case json_type_string:
		str = json_object_get_string(jobjkey);
		u8val = l_base64_decode(str, strlen(str), &olen);
		if (!u8val)
			break;

		if (olen > KNOT_DATA_RAW_SIZE)
			olen = KNOT_DATA_RAW_SIZE; /* truncate */

		memcpy(kvalue->raw, u8val, olen);
		l_free(u8val);
		break;
	/* FIXME: not implemented */
	case json_type_null:
	case json_type_object:
	case json_type_array:
	default:
		break;
	}

	return olen;
}

static json_object *schema_item_create_obj(knot_msg_schema *schema)
{
	json_object *json_schema;

	json_schema = json_object_new_object();

	json_object_object_add(json_schema, KNOT_JSON_FIELD_SENSOR_ID,
				       json_object_new_int(schema->sensor_id));
	json_object_object_add(json_schema, KNOT_JSON_FIELD_VALUE_TYPE,
				json_object_new_int(
					schema->values.value_type));
	json_object_object_add(json_schema, KNOT_JSON_FIELD_UNIT,
				json_object_new_int(
					schema->values.unit));
	json_object_object_add(json_schema, KNOT_JSON_FIELD_TYPE_ID,
				json_object_new_int(
					schema->values.type_id));
	json_object_object_add(json_schema, KNOT_JSON_FIELD_DEVICE_NAME,
				json_object_new_string(
					schema->values.name));

	/*
	 * Returned JSON object is in the following format:
	 *
	 * {
	 *   "sensorId": 1,
	 *   "valueType": 0xFFF1,
	 *   "unit": 0,
	 *   "typeId": 3,
	 *   "name": "Door lock"
	 * }
	 */

	return json_schema;
}

static void schema_item_create_and_append(void *data, void *user_data)
{
	knot_msg_schema *schema = data;
	json_object *schema_list = user_data;
	json_object *item;

	item = schema_item_create_obj(schema);
	json_object_array_add(schema_list, item);
}

char *parser_schema_create_object(const char *device_id,
		struct l_queue *schema_list)
{
	char *json_str;
	json_object *json_msg;
	json_object *json_schema_array;

	json_msg = json_object_new_object();
	json_schema_array = json_object_new_array();

	json_object_object_add(json_msg, KNOT_JSON_FIELD_DEVICE_ID,
			       json_object_new_string(device_id));

	l_queue_foreach(schema_list, schema_item_create_and_append,
			json_schema_array);

	json_object_object_add(json_msg, KNOT_JSON_FIELD_SCHEMA,
			json_schema_array);

	/*
	 * Returned JSON object is in the following format:
	 *
	 * { "id": "fbe64efa6c7f717e",
	 *   "schema" : [{
	 *         "sensorId": 1,
	 *         "valueType": 0xFFF1,
	 *         "unit": 0,
	 *         "typeId": 3,
	 *         "name": "Door lock"
	 *   }]
	 * }
	 */
	json_str = l_strdup(json_object_to_json_string(json_msg));
	json_object_put(json_msg);

	return json_str;
}

struct l_queue *parser_update_to_list(const char *json_str)
{
	json_object *json_obj;
	json_object *json_array;
	json_object *json_data;
	json_object *jobjkey;
	knot_msg_data *msg;
	struct l_queue *list;
	uint64_t i;
	int jtype;
	int olen;
	uint8_t sensor_id;
	bool has_err;

	json_obj = json_tokener_parse(json_str);
	if (!json_obj)
		return NULL;

	list = l_queue_new();

	if (!json_object_object_get_ex(json_obj, KNOT_JSON_FIELD_DATA,
			&json_array)) {
		l_queue_destroy(list, l_free);
		json_object_put(json_obj);
		return NULL;
	}

	has_err = false;
	for (i = 0; i < json_object_array_length(json_array); i++) {

		json_data = json_object_array_get_idx(json_array, i);
		if (!json_data) {
			has_err = true;
			break;
		}

		/* Getting 'sensorId' */
		if (!json_object_object_get_ex(json_data,
				KNOT_JSON_FIELD_SENSOR_ID, &jobjkey)) {
			has_err = true;
			break;
		}

		if (json_object_get_type(jobjkey) != json_type_int) {
			has_err = true;
			break;
		}

		sensor_id = json_object_get_int(jobjkey);

		/* Getting 'value' */
		if (!json_object_object_get_ex(json_data,
				KNOT_JSON_FIELD_VALUE, &jobjkey)) {
			has_err = true;
			break;
		}

		jtype = json_object_get_type(jobjkey);
		if (jtype != json_type_int &&
		jtype != json_type_double && jtype != json_type_boolean &&
			jtype != json_type_string) {
			has_err = true;
			break;
		}

		msg = l_new(knot_msg_data, 1);

		olen = parse_json2data(jobjkey, &msg->payload);
		if (olen <= 0) {
			l_free(msg);
			has_err = true;
			break;
		}

		msg->sensor_id = sensor_id;
		msg->hdr.type = KNOT_MSG_PUSH_DATA_REQ;
		msg->hdr.payload_len = olen + sizeof(msg->sensor_id);

		if (!l_queue_push_tail(list, msg)) {
			l_free(msg);
			has_err = true;
			break;
		}
	}
	json_object_put(json_obj);
	if (has_err) {
		l_queue_destroy(list, l_free);
		return NULL;
	}

	return list;
}

char *parser_data_create_object(const char *device_id, uint8_t sensor_id,
				       uint8_t value_type,
				       const knot_value_type *value,
				       uint8_t kval_len)
{
	char *json_str;
	json_object *json_msg;
	json_object *data;
	json_object *json_array;
	char *encoded;
	size_t encoded_len;
	bool has_err;

	json_msg = json_object_new_object();
	json_array = json_object_new_array();

	json_object_object_add(json_msg, KNOT_JSON_FIELD_DEVICE_ID,
			       json_object_new_string(device_id));
	data = json_object_new_object();
	json_object_object_add(data, KNOT_JSON_FIELD_SENSOR_ID,
			       json_object_new_int(sensor_id));

	has_err = false;
	switch (value_type) {
	case KNOT_VALUE_TYPE_INT:
		json_object_object_add(data, KNOT_JSON_FIELD_VALUE,
				json_object_new_int(knot_value_as_int(value)));
		break;
	case KNOT_VALUE_TYPE_FLOAT:
		json_object_object_add(data, KNOT_JSON_FIELD_VALUE,
			json_object_new_double(knot_value_as_double(value)));
		break;
	case KNOT_VALUE_TYPE_BOOL:
		json_object_object_add(data, KNOT_JSON_FIELD_VALUE,
			json_object_new_boolean(knot_value_as_boolean(value)));
		break;
	case KNOT_VALUE_TYPE_RAW:
		/* Encode as base64 */
		encoded = knot_value_as_raw(value, kval_len, &encoded_len);
		if (!encoded){
			has_err = true;
			json_object_put(json_array);
			json_object_put(data);
			break;
		}
		json_object_object_add(data, KNOT_JSON_FIELD_VALUE,
			json_object_new_string_len(encoded, encoded_len));
		break;
	case KNOT_VALUE_TYPE_INT64:
		json_object_object_add(data, KNOT_JSON_FIELD_VALUE,
			json_object_new_int64(knot_value_as_int64(value)));
		break;
	case KNOT_VALUE_TYPE_UINT:
		json_object_object_add(data, KNOT_JSON_FIELD_VALUE,
			json_object_new_uint64(knot_value_as_uint(value)));
		break;
	case KNOT_VALUE_TYPE_UINT64:
		json_object_object_add(data, KNOT_JSON_FIELD_VALUE,
			json_object_new_uint64(knot_value_as_uint64(value)));
		break;
	default:
		has_err = true;
		break;
	}

	json_object_array_add(json_array, data);
	json_object_object_add(json_msg, KNOT_JSON_FIELD_DATA, json_array);

	json_str = l_strdup(json_object_to_json_string(json_msg));
	json_object_put(json_msg);
	/*
	 * Returned JSON object is in the following format:
	 *
	 * { "id": "fbe64efa6c7f717e",
	 *   "data": [{
	 *     "sensorId": 1,
	 *     "value": false,
	 *   }]
	 * }
	 */

	return (json_str && !has_err) ? json_str : NULL;
}

struct l_queue *parser_schema_to_list(const char *json_str)
{
	json_object *jobjarray, *jobjentry, *jobjkey;
	struct l_queue *list;
	knot_msg_schema *entry;
	int sensor_id, value_type, unit, type_id;
	uint64_t i;
	const char *name;

	jobjarray = json_tokener_parse(json_str);
	if (!jobjarray)
		return NULL;

	if (json_object_get_type(jobjarray) != json_type_array) {
		json_object_put(jobjarray);
		return NULL;
	}

	list = l_queue_new();
	/* Expected JSON object is in the following format:
	 *
	 * [ {"sensorId": x, "valueType": w,
	 *		"unit": z "typeId": y, "name": "foo"}]
	 * }
	 */

	for (i = 0; i < json_object_array_length(jobjarray); i++) {
		jobjentry = json_object_array_get_idx(jobjarray, i);

		/* Getting 'sensorId' */
		if (!json_object_object_get_ex(jobjentry,
						KNOT_JSON_FIELD_SENSOR_ID,
						&jobjkey))
			break;

		if (json_object_get_type(jobjkey) != json_type_int)
			break;

		sensor_id = json_object_get_int(jobjkey);

		/* Getting 'valueType' */
		if (!json_object_object_get_ex(jobjentry,
						KNOT_JSON_FIELD_VALUE_TYPE,
						&jobjkey))
			break;

		if (json_object_get_type(jobjkey) != json_type_int)
			break;

		value_type = json_object_get_int(jobjkey);

		/* Getting 'unit' */
		if (!json_object_object_get_ex(jobjentry, KNOT_JSON_FIELD_UNIT,
						&jobjkey))
			break;

		if (json_object_get_type(jobjkey) != json_type_int)
			break;

		unit = json_object_get_int(jobjkey);

		/* Getting 'typeId' */
		if (!json_object_object_get_ex(jobjentry,
						KNOT_JSON_FIELD_TYPE_ID,
						&jobjkey))
			break;

		if (json_object_get_type(jobjkey) != json_type_int)
			break;

		type_id = json_object_get_int(jobjkey);

		/* Getting 'name' */
		if (!json_object_object_get_ex(jobjentry,
						KNOT_JSON_FIELD_DEVICE_NAME,
						&jobjkey))
			break;

		if (json_object_get_type(jobjkey) != json_type_string)
			break;

		name = json_object_get_string(jobjkey);
		/*
		 * Validation not required: validation has been performed
		 * previously when schema has been submitted to the cloud.
		 */
		entry = l_new(knot_msg_schema, 1);
		entry->sensor_id = sensor_id;
		entry->values.value_type = value_type;
		entry->values.unit = unit;
		entry->values.type_id = type_id;
		strncpy(entry->values.name, name,
						sizeof(entry->values.name) - 1);

		l_queue_push_tail(list, entry);
	}
	json_object_put(jobjarray);

	if (l_queue_isempty(list)) {
		l_queue_destroy(list, NULL);
		list = NULL;
	}

	return list;
}

struct l_queue *parser_queue_from_json_array(const char *json_str,
					     create_device_item_cb item_cb)
{
	json_object *jobj, *jobjentry, *jarray;
	struct l_queue *list;
	void *item;
	int len;
	int i;

	jobj = json_tokener_parse(json_str);
	if (!jobj)
		return NULL;

	jarray = json_object_object_get(jobj, KNOT_JSON_FIELD_DEVICES);
	if (!jarray)
		return NULL;

	if (json_object_get_type(jarray) != json_type_array)
		return NULL;

	len = json_object_array_length(jarray);
	list = l_queue_new();

	for (i = 0; i < len; i++) {
		jobjentry = json_object_array_get_idx(jarray, i);
		item = device_array_item(jobjentry, item_cb);
		if (item)
			l_queue_push_tail(list, item);
	}
	json_object_put(jobj);

	return list;
}

struct l_queue *parser_request_to_list(const char *json_str)
{
	json_object *jso;
	struct l_queue *list;
	json_object *json_array;
	json_object *jobjentry;
	int sensor_id;
	uint64_t i;
	bool has_err;

	jso = json_tokener_parse(json_str);
	if (!jso)
		return NULL;
	list = l_queue_new();

	if (!json_object_object_get_ex(jso, KNOT_JSON_FIELD_SENSOR_IDS,
			&json_array)) {
		l_queue_destroy(list, l_free);
		return NULL;
	}

	has_err = false;
	for (i = 0; i < json_object_array_length(json_array); i++) {

		jobjentry = json_object_array_get_idx(json_array, i);
		if (!jobjentry) {
			has_err = true;
			break;
		}

		if (json_object_get_type(jobjentry) != json_type_int) {
			has_err = true;
			break;
		}

		sensor_id = json_object_get_int(jobjentry);

		if (!l_queue_push_tail(list,
				l_memdup(&sensor_id, sizeof(sensor_id)))) {
			has_err = true;
			break;
		}
	}

	if (has_err) {
		l_queue_destroy(list, l_free);
		return NULL;
	}

	return list;
}

char *parser_sensorid_to_json(const char *key, struct l_queue *list)
{
	char *json_str;
	int *id;
	json_object *ajobj;
	json_object *entry;
	json_object *setdatajobj;

	ajobj = json_object_new_array();

	for (id = l_queue_pop_head(list); id; id = l_queue_pop_head(list)) {
		entry = json_object_new_object();
		json_object_object_add(entry, KNOT_JSON_FIELD_SENSOR_ID,
				       json_object_new_int(*id));
		json_object_array_add(ajobj, json_object_get(entry));
	}

	setdatajobj = json_object_new_object();
	json_object_object_add(setdatajobj, key, ajobj);
	json_str = l_strdup(json_object_to_json_string(setdatajobj));
	json_object_put(setdatajobj);

	return json_str;
}

char *parser_device_json_create(const char *device_id,
			const char *device_name)
{
	char *json_str;
	json_object *device;

	device = json_object_new_object();
	if (!device)
		return NULL;

	json_object_object_add(device, KNOT_JSON_FIELD_DEVICE_NAME,
			json_object_new_string(device_name));
	json_object_object_add(device, KNOT_JSON_FIELD_DEVICE_ID,
			json_object_new_string(device_id));

	/*
	 * Returned JSON object is in the following format:
	 *
	 * { "id": "fbe64efa6c7f717e",
	 *   "name": "KNoT Thing"
	 * }
	 */
	json_str = l_strdup(json_object_to_json_string(device));
	json_object_put(device);

	return json_str;
}

char *parser_auth_json_create(const char *device_id,
			const char *device_token)
{
	char *json_str;
	json_object *auth;

	auth = json_object_new_object();
	if (!auth)
		return NULL;

	json_object_object_add(auth, KNOT_JSON_FIELD_DEVICE_ID,
			       json_object_new_string(device_id));
	json_object_object_add(auth, KNOT_JSON_FIELD_DEVICE_TOKEN,
			       json_object_new_string(device_token));


	/*
	 * Returned JSON object is in the following format:
	 *
	 * { "id": "fbe64efa6c7f717e",
	 *   "token": "0c20c12e2ac058d0513d81dc58e33b2f9ff8c83d"
	 * }
	 */
	json_str = l_strdup(json_object_to_json_string(auth));
	json_object_put(auth);

	return json_str;
}

char *parser_unregister_json_create(const char *device_id)
{
	char *json_str;
	json_object *unreg;

	unreg = json_object_new_object();
	if (!unreg)
		return NULL;

	json_object_object_add(unreg, KNOT_JSON_FIELD_DEVICE_ID,
			       json_object_new_string(device_id));

	/*
	 * Returned JSON object is in the following format:
	 *
	 * { "id": "fbe64efa6c7f717e" }
	 */
	json_str = l_strdup(json_object_to_json_string(unreg));
	json_object_put(unreg);

	return json_str;
}

char *parser_get_key_str_from_json_str(const char *json_str,
				       const char *key)
{
	char *str_key;
	json_object *jso;
	json_object *jobjkey;

	jso = json_tokener_parse(json_str);
	if (!jso) {
		return NULL;
	}

	if (!json_object_object_get_ex(jso, key, &jobjkey)) {
		return NULL;
	}

	if (json_object_get_type(jobjkey) != json_type_string) {
		return NULL;
	}

	str_key = l_strdup(json_object_get_string(jobjkey));
	json_object_put(jso);

	return str_key;
}

bool parser_is_key_str_or_null(const char *json_str, const char *key)
{
	json_object *jso;
	json_object *jobjkey;
	enum json_type type;

	jso = json_tokener_parse(json_str);
	if (!jso)
		return false;

	if (!json_object_object_get_ex(jso, key, &jobjkey))
		return false;

	type = json_object_get_type(jobjkey);
	json_object_put(jso);

	return type == json_type_string || type == json_type_null;
}
