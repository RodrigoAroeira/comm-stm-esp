#!/usr/bin/env bash

json_obj='{
        "cap_model": "MODEL_A_100F",
        "test_type": "ESR",
        "params": {"pulse_ma": 10000, "pulse_ms": 1425 }
}'

echo -n "Sent obj: "
echo "$json_obj" | jq
echo

echo -n "Response: "
response=$(curl -sX POST http://192.168.0.123/api/run-test \
  -H "Content-Type: application/json" \
  -d "$json_obj")

[[ -z "$response" ]] && exit 1
echo $response | jq
