#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <ell/ell.h>
#include <json-c/json.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <signal.h>

#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <amqp_framing.h>

#include <knot/knot_protocol.h>
#include <knot/knot_types.h>
#include <knot/knot_cloud.h>

#include <assert.h>

static void on_cloud_disconnected(void *user_data){}

static void signal_handler(uint32_t signo, void *user_data)
{
	switch (signo)
	{
	case SIGINT:
	case SIGTERM:

		l_main_quit();
		break;
	}
}

static bool on_recived_msg(const struct knot_cloud_msg *msg,
			   void *user_data)
{
	return true;
}

static int test_register_device()
{
	char id[KNOT_PROTOCOL_UUID_LEN + 1];
	char name[KNOT_PROTOCOL_DEVICE_NAME_LEN];

	strcpy(name, "TESTDEVICE");
	strcpy(id, "5b620bcd419afed7");

	return knot_cloud_register_device(id, name);
}

static int test_auth_device()
{
	char token[KNOT_PROTOCOL_TOKEN_LEN + 1];
	char id[KNOT_PROTOCOL_UUID_LEN + 1];

	strcpy(token, "41aa0229342ecf8eb686cde53f739c1c3da9c1c5");
	strcpy(id, "5b620bcd419afed7");



	return knot_cloud_auth_device(id, token);
}

static int test_update_schema()
{
	struct l_queue *schema_queue;

	schema_queue = l_queue_new();

	l_queue_push_tail(schema_queue, "SCHEMA DATA");

	return knot_cloud_update_schema("DEVICE_ID", schema_queue);
}

static int test_publish_data()
{
	knot_value_type value;
	int rc;
	value.val_i = 1223;

	rc = knot_cloud_publish_data("TEST_SENSOR", 12,
				     1, &value,
				     sizeof(value));

	return rc;
}

static int test_unregister_device()
{
	char id[KNOT_PROTOCOL_UUID_LEN + 1];

	strcpy(id, "5b620bcd419afed7");

	return knot_cloud_unregister_device(id);
}

static void start_test_when_connected(void *user_data)
{
	int err;

	//READ START
	knot_cloud_read_start("5b620bcd419afed7", on_recived_msg, NULL);

	//TEST REGISTER
	err = test_register_device();
	if (err < 0)
		l_error("SEND: REGISTER DEVICE");

	//TEST AUTH DEVICE
	err = test_auth_device();
	if (err < 0)
		l_error("SEND: AUTH DEVICE");

	// //TEST UPDATE SCHEMA
	err = test_update_schema();
	if (err < 0)
		l_error("SEND: UPDATE SCHEMA");

	//TEST PUBLISH
	err = test_publish_data();
	if (err < 0)
		l_error("SEND: PUBLISH DATA");

	//TEST UNREGISTER DEVICE
	err = test_unregister_device();
	if (err < 0)
		l_error("SEND: UNREGISTER DEVICE");
}

int main(int argc, char const *argv[])
{
	int rc;

	l_log_set_stderr();

	if (!l_main_init())
		return -1;

	knot_cloud_set_log_priority("info");

	rc = knot_cloud_start("amqp://guest:guest@localhost:5672", "USER_TOKEN",
			      start_test_when_connected,
			      on_cloud_disconnected,
			      NULL);

	l_main_run_with_signal(signal_handler, NULL);

	knot_cloud_stop();

	l_main_exit();

	return 0;
}
