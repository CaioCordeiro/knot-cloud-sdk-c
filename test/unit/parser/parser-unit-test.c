#include <ell/ell.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
// #include <json-c/json.h>

#include <knot/knot_protocol.h>

#include "../src/parser.h"

static int test_schema_create_object()
{
	struct l_queue *schema_queue;

	knot_msg_header header;
	knot_msg_schema msg_schema;
	knot_schema schema;

	json_object *jobj_schema;
	const char *json_str;

	int err;

	strcpy(schema.name ,"Schema_test");
	schema.type_id = 65521;
	schema.unit = 0;
	schema.value_type = 3;

	header.type = 3;
	header.payload_len = sizeof(int8_t);

	msg_schema.hdr = header;
	msg_schema.sensor_id = 1;
	msg_schema.values = schema;

	schema_queue = l_queue_new();

	l_queue_push_tail(schema_queue, &msg_schema);

	jobj_schema = parser_schema_create_object("DEVICE_1",schema_queue);

	json_str = json_object_to_json_string(jobj_schema);

	err = strcmp("{ \"id\": \"DEVICE_1\", \"schema\": [ { \"sensorId\": 1,"
	" \"valueType\": 3, \"unit\": 0, \"typeId\": 65521, "
	"\"name\": \"Schema_test\" } ] }",json_str);

	if(err != 0 )
	{
		return -EINVAL;
	}

	return 0;
}

static int test_data_create_object()
{
	knot_value_type value;
	value.val_i = 1223;
	json_object *jobj;
	const char *json_str;
	int err;

	jobj = parser_data_create_object("DEVICE_1",12,1,&value,sizeof(value));

	json_str = json_object_to_json_string(jobj);
	l_info("%s",json_str);

	err = strcmp("{ \"id\": \"DEVICE_1\", \"data\": [ { \"sensorId\": 12,"



		     " \"value\": 1223 } ] }",json_str);

	if(err != 0 )
	{

		return -EINVAL;
	}

	return 0;

}

static int test_sensorid_to_json()
{

}

// static int test_update_to_list()
// {
// 	struct l_queue *queue;
// 	struct knot_cloud_msg *msg;
// 	struct json_object *jso;



// 	queue = parser_update_to_list();

// 	return 0;
// }

void start_test()
{
	int err;

	err = test_schema_create_object();
	if (err<0)
		l_error("SCHEMA_CREATE_OBJECT: ERR");
	else
		l_info("SCHEMA CREATE OBJECT: OK");

	err = test_data_create_object();
	if (err<0)
		l_error("DATA_CREATE_OBJECT: ERR");
	else
		l_info("DATA_CREATE_OBJECT: OK");
}

int main(int argc, char const *argv[])
{
	l_log_set_stderr();
	start_test();
	// test_schema_create_object();

	return 0;
}
