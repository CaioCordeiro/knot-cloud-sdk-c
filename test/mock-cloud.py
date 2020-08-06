#!/usr/bin/python3
#
#  This file is part of the KNOT Project
#
#  Copyright (c) 2019, CESAR. All rights reserved.
#
#   This library is free software; you can redistribute it and/or
#   modify it under the terms of the GNU Lesser General Public
#   License as published by the Free Software Foundation; either
#   version 2.1 of the License, or (at your option) any later version.
#
#   This library is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   Lesser General Public License for more details.
#
#   You should have received a copy of the GNU Lesser General Public
#   License along with this library; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

import pika
import logging
import json
import argparse
import secrets

cloud_exchange = 'connIn'
fog_exchange = 'connOut'

QUEUE_CLOUD_NAME = 'connIn-messages'

MESSAGE_EXPIRATION_TIME_MS = '10000'

EVENT_REGISTER = 'device.register'
KEY_REGISTERED = 'device.registered'

EVENT_UNREGISTER = 'device.unregister'
KEY_UNREGISTERED = 'device.unregistered'

EVENT_AUTH = 'device.cmd.auth'
KEY_AUTH = 'device.auth'

EVENT_LIST = 'device.cmd.list'
KEY_LIST_DEVICES = 'device.list'

EVENT_SCHEMA = 'schema.update'
KEY_SCHEMA = 'schema.updated'

EVENT_DATA = 'data.publish'

KEY_UPDATE = 'data.update'
KEY_REQUEST = 'data.request'

mapper = {
    EVENT_REGISTER: KEY_REGISTERED,
    EVENT_UNREGISTER: KEY_UNREGISTERED,
    EVENT_AUTH: KEY_AUTH,
    EVENT_LIST: KEY_LIST_DEVICES,
    EVENT_SCHEMA: KEY_SCHEMA,
}

logging.basicConfig(
    format='%(asctime)s %(message)s',
    level=logging.INFO,
    datefmt='%Y-%m-%d %H:%M:%S')

def __on_msg_received(args, channel, method, properties, body):
    logging.info("%r:%r" % (method.routing_key, body))
    message = json.loads(body)
    message['error'] = 'error mocked' if args.with_side_effect else None

    if method.routing_key == EVENT_DATA:
        return None
    elif method.routing_key == EVENT_REGISTER:
        message['token'] = secrets.token_hex(20)
        del message['name']
    elif method.routing_key == EVENT_AUTH:
        del message['token']
    elif method.routing_key == EVENT_LIST:
        message['devices'] = [
        {
            'id': secrets.token_hex(8),
            'name': 'test',
            'schema': [{
                "sensor_id": 0,
                "value_type": 3,
                "unit": 0,
                "type_id": 65521,
                "name": "LED"
            }]
        }, {
            'id': secrets.token_hex(8),
            'name': 'test2',
            'schema': [{
                "sensor_id": 0,
                "value_type": 3,
                "unit": 0,
                "type_id": 65521,
                "name": "LED"
            }]
        }]
    elif method.routing_key == EVENT_SCHEMA:
        del message['schema']

    channel.basic_publish(
        exchange=fog_exchange,
        routing_key=mapper[method.routing_key],
        body=json.dumps(message)
    )
    logging.info(" [x] Sent %r" % (message))

def __amqp_start():
    connection = pika.BlockingConnection(
        pika.ConnectionParameters(host='localhost'))
    channel = connection.channel()

    channel.exchange_declare(exchange=fog_exchange, durable=True,
    exchange_type='topic')
    channel.exchange_declare(exchange=cloud_exchange, durable=True,
    exchange_type='topic')

    return channel

# Parser sub-commands
def msg_consume(args):
    channel = __amqp_start()
    result = channel.queue_declare(QUEUE_CLOUD_NAME, exclusive=False, durable=True)
    queue_name = result.method.queue

    channel.queue_bind(
            exchange=cloud_exchange, queue=queue_name, routing_key='device.*')
    channel.queue_bind(
        exchange=cloud_exchange, queue=queue_name, routing_key='device.cmd.list')
    channel.queue_bind(
            exchange=cloud_exchange, queue=queue_name, routing_key='schema.*')
    channel.queue_bind(
            exchange=cloud_exchange, queue=queue_name, routing_key='data.*')

    def __wrapper_msg_received(ch, mth, props, body):
        __on_msg_received(args, ch, mth, props, body)

    channel.basic_consume(queue=queue_name,
    on_message_callback=__wrapper_msg_received,
    auto_ack=True)

    logging.info('Listening to messages')
    channel.start_consuming()

def no_command(args):
    parser.print_help()
    exit(1)

parser = argparse.ArgumentParser(description='Mock KNoT Fog Connector')
parser.set_defaults(func=no_command)
subparsers = parser.add_subparsers(help='sub-command help', dest='subcommand')

parser_listen = subparsers.add_parser('listen', help='Listen to messages \
    from client KNoT daemon', formatter_class=argparse.RawTextHelpFormatter)
parser_listen.add_argument('-s', '--with-side-effect', action='store_true',
                           help='Send messages with error')
# Consuming Mensages
parser_listen.set_defaults(func=msg_consume)

options = parser.parse_args()
options.func(options)
