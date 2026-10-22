#!/usr/bin/env python3
import argparse
import json
import os
import socket
import sys
from pathlib import Path

DEFAULT_MAC = os.environ.get("ESP_BT_MAC", "C4:DE:E2:5B:9E:F6")
DEFAULT_PORT = 1  # Standard SPP channel
BUFFER_SIZE = 4096

DEFAULT_PAYLOAD = {
    "cap_model": "MODEL_A_100F",
    "test_type": "ESR",
    "params": {"pulse_ma": 10000, "pulse_ms": 1425},
}


def load_payload(payload_path: Path) -> dict:
    try:
        with open(payload_path, "r", encoding="utf-8") as f:
            return json.load(f)
    except (json.JSONDecodeError, OSError, FileNotFoundError) as e:
        print(f"[!] Warning: Failed to read {payload_path}: {e}")
    return DEFAULT_PAYLOAD


def receive_response(sock: socket.socket, timeout: float) -> str:
    """Read stream until a newline-terminated JSON line or end of data."""
    sock.settimeout(timeout)
    chunks = []
    while True:
        try:
            chunk = sock.recv(BUFFER_SIZE)
            if not chunk:
                break
            chunks.append(chunk)
            if b"\n" in chunk:
                break
        except TimeoutError:
            break
    return b"".join(chunks).decode("utf-8", errors="replace").strip()


def send_test_packet(mac: str, port: int, payload: dict, timeout: float) -> str:
    """Connect to the device, send the payload, and return the raw response."""
    with socket.socket(
        socket.AF_BLUETOOTH, socket.SOCK_STREAM, socket.BTPROTO_RFCOMM
    ) as sock:
        sock.settimeout(timeout)
        print(f"[*] Connecting to {mac}...")
        sock.connect((mac, port))
        print("[+] Connected successfully!")

        print("\n[*] Sending test payload:")
        print(json.dumps(payload, indent=2))
        sock.sendall((json.dumps(payload) + "\n").encode("utf-8"))

        print("\n[*] Waiting for response...")
        return receive_response(sock, timeout=timeout)


def print_and_validate(raw_response: str) -> bool:
    """Print response and return True if test execution succeeded."""
    if not raw_response:
        print(
            "[-] No response received (connection closed or timed out).",
            file=sys.stderr,
        )
        return False

    print("[+] Response received:")
    try:
        parsed = json.loads(raw_response)
        print(json.dumps(parsed, indent=2))
        status = parsed.get("status")
        if status != "FORWARD_SUCCESS":
            print(
                f"[-] Server returned failure status: {status}",
                file=sys.stderr,
            )
            return False
        return True
    except json.JSONDecodeError:
        print(raw_response)
        return True


def main():
    parser = argparse.ArgumentParser(
        description="Test ESP32 supercapacitor test bench over Bluetooth SPP (RFCOMM)"
    )
    parser.add_argument(
        "mac",
        nargs="?",
        default=DEFAULT_MAC,
        help=f"ESP32 Bluetooth MAC address (default: {DEFAULT_MAC})",
    )
    parser.add_argument(
        "-p",
        "--port",
        type=int,
        default=DEFAULT_PORT,
        help=f"RFCOMM channel / port (default: {DEFAULT_PORT})",
    )
    parser.add_argument(
        "-t",
        "--timeout",
        type=float,
        default=10.0,
        help="Socket connection and read timeout in seconds (default: 10.0)",
    )
    parser.add_argument(
        "-f",
        "--file",
        type=Path,
        default=Path(__file__).parent / "test_obj.json",
        help="Path to JSON test payload file (default: tests/test_obj.json)",
    )

    args = parser.parse_args()
    mac_address = args.mac.strip()
    payload = load_payload(args.file)

    print(f"[*] Target device: {mac_address} on channel {args.port}")
    print(
        f"[*] Payload source: {args.file if args.file.is_file() else 'inline default'}"
    )

    try:
        raw_response = send_test_packet(mac_address, args.port, payload, args.timeout)
    except TimeoutError:
        print(
            f"[-] Connection or read timed out after {args.timeout}s.", file=sys.stderr
        )
        sys.exit(1)
    except OSError as e:
        print(f"[-] Bluetooth error: {e}", file=sys.stderr)
        print(
            "    Tip: Ensure the ESP32 has Bluetooth enabled, is within range, and is paired.",
            file=sys.stderr,
        )
        sys.exit(1)

    if not print_and_validate(raw_response):
        sys.exit(1)


if __name__ == "__main__":
    main()
