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

#define TEST_ID "5b620bcd419afed7"

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

static bool on_received_msg(const struct knot_cloud_msg *msg,
			   void *user_data)
{
	return true;
}

static int test_register_device(void)
{
	char id[KNOT_PROTOCOL_UUID_LEN + 1];
	char name[KNOT_PROTOCOL_DEVICE_NAME_LEN];

	strcpy(name, "TESTDEVICE");
	strcpy(id, TEST_ID);

	return knot_cloud_register_device(id, name);
}

static int test_auth_device(void)
{
	char token[KNOT_PROTOCOL_TOKEN_LEN + 1];
	char id[KNOT_PROTOCOL_UUID_LEN + 1];

	strcpy(token, "41aa0229342ecf8eb686cde53f739c1c3da9c1c5");
	strcpy(id, TEST_ID);

	return knot_cloud_auth_device(id, token);
}

static int test_update_config(void)
{
	struct l_queue *config_queue;
	knot_msg_config *entry;
	knot_schema schema;
	int err;

	config_queue = l_queue_new();

	entry = l_new(knot_msg_config, 1);

	entry->sensor_id = 1;
	entry->schema.type_id = 0xFFF1;
	entry->schema.unit = 0;
	entry->schema.value_type = 1;
	strncpy(entry->schema.name,"Door lock",sizeof(entry->schema.name) - 1);
	entry->event.lower_limit.val_i = 1000;
	entry->event.upper_limit.val_i = 3000;
	entry->event.time_sec = 10;
	entry->event.event_flags = 7;

	l_queue_push_tail(config_queue, entry);

	err = knot_cloud_update_config(TEST_ID, config_queue);

	l_queue_destroy(config_queue, NULL);
	l_free(entry);

	return err;
}

static int test_publish_data(void)
{
	knot_value_type value;
	int rc;
	value.val_i = 1223;

	rc = knot_cloud_publish_data("TEST_SENSOR", 12,
				     1, &value,
				     sizeof(value));

	return rc;
}

static int test_unregister_device(void)
{
	char id[KNOT_PROTOCOL_UUID_LEN + 1];

	strcpy(id, TEST_ID);

	return knot_cloud_unregister_device(id);
}

static void start_test_when_connected(void *user_data)
{
	int err;

	//READ START
	knot_cloud_read_start(TEST_ID, on_received_msg, NULL);

	//TEST REGISTER
	err = test_register_device();
	if (err < 0)
		l_error("SEND: REGISTER DEVICE");

	//TEST AUTH DEVICE
	err = test_auth_device();
	if (err < 0)
		l_error("SEND: AUTH DEVICE");

	// //TEST UPDATE CONFIG
	err = test_update_config();
	if (err < 0)
		l_error("SEND: UPDATE CONFIG");

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

	knot_cloud_set_log_priority(L_LOG_INFO);

	rc = knot_cloud_start("amqp://guest:guest@localhost:5672", "USER_TOKEN",
			      start_test_when_connected,
			      on_cloud_disconnected,
			      NULL);

	l_main_run_with_signal(signal_handler, NULL);

	knot_cloud_stop();

	l_main_exit();

	return 0;
}
