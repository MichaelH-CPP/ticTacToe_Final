#!/bin/bash

MQTT_BROKER=$1
REQUEST_TOPIC="gcpRequest"
RESPONSE_TOPIC="espRequest"

mosquitto_sub -h "$MQTT_BROKER" -t "$REQUEST_TOPIC" | while read -r message; do    
    # Process the move
    echo $message
    updated_board=$(./playerO.sh "$message")
    echo $updated_board
    mosquitto_pub -h "$MQTT_BROKER" -t "$RESPONSE_TOPIC" -m "$updated_board"
done