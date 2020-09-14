#include <ell/ell.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <knot/knot_protocol.h>

#include "../src/parser.h"

static int test_schema_create_object()
{
	struct l_queue *schema_queue;
	json_object *jobj_schema;
	char *json_str;
	schema_queue = l_queue_new();

	l_queue_push_tail(schema_queue, "SCHEMA DATA");

	jobj_schema = parser_schema_create_object("TEST_ID",schema_queue);

	json_str = json_object_to_json_string(jobj_schema);

	l_info("%s",json_str);

	return 0;
}

int main(int argc, char const *argv[])
{

	test_schema_create_object();

	l_log_set_stderr();

	return 0;
}
