#!/usr/bin/env bash

# Resolve directory of this script to reliably locate test_obj.json
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# IP can be passed as 1st argument, via ESP_IP environment variable, or default
ESP_IP="${1:-${ESP_IP:-192.168.0.123}}"
ENDPOINT="http://${ESP_IP}/api/run-test"
PAYLOAD_FILE="${SCRIPT_DIR}/test_obj.json"

# Load payload from file or fallback to default object
if [[ -f "$PAYLOAD_FILE" ]]; then
  json_obj="$(cat "$PAYLOAD_FILE")"
else
  json_obj='{
    "cap_model": "MODEL_A_100F",
    "test_type": "ESR",
    "params": {"pulse_ma": 10000, "pulse_ms": 1425}
  }'
fi

echo "[*] Target endpoint: $ENDPOINT"
echo "[*] Sending payload:"
if command -v jq &>/dev/null; then
  echo "$json_obj" | jq .
else
  echo "$json_obj"
fi
echo

echo "[*] Waiting for response..."
http_response=$(curl -s -S -w "\nHTTP_STATUS:%{http_code}" \
  --connect-timeout 5 \
  --max-time 10 \
  -X POST "$ENDPOINT" \
  -H "Content-Type: application/json" \
  -d "$json_obj" 2>&1)
curl_exit=$?

if [[ $curl_exit -ne 0 ]]; then
  echo "[-] Connection failed (curl error $curl_exit):" >&2
  echo "$http_response" >&2
  exit $curl_exit
fi

# Extract response body and HTTP status code
body=$(echo "$http_response" | sed -e '$d')
status_code=$(echo "$http_response" | tail -n1 | sed -e 's/HTTP_STATUS://')

if [[ "$status_code" -ge 200 && "$status_code" -lt 300 ]]; then
  echo "[+] Success (HTTP $status_code):"
else
  echo "[-] Server returned error (HTTP $status_code):" >&2
fi

if command -v jq &>/dev/null; then
  echo "$body" | jq . 2>/dev/null || echo "$body"
else
  echo "$body"
fi

if [[ "$status_code" -lt 200 || "$status_code" -ge 300 ]]; then
  exit 1
fi
