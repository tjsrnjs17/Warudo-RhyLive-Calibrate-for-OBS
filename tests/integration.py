import base64
import hashlib
import http.server
import json
import os
from pathlib import Path
import socket
import socketserver
import struct
import subprocess
import sys
import tempfile
import threading
import time


TOKEN = "0123456789ABCDEF0123456789ABCDEF"
ASSET_ID = "11111111-2222-3333-4444-555555555555"
TYPE_ID = "d2a5d113-91b9-4840-94d1-06326e7dd93e"


def frame(payload, opcode=1, final=True):
    header = bytes([(128 if final else 0) | opcode])
    if len(payload) < 126:
        header += bytes([len(payload)])
    elif len(payload) <= 65535:
        header += b"\x7e" + struct.pack("!H", len(payload))
    else:
        header += b"\x7f" + struct.pack("!Q", len(payload))
    return header + payload


def receive_exact(connection, size):
    result = b""
    while len(result) < size:
        chunk = connection.recv(size - len(result))
        if not chunk:
            raise ConnectionError("Unexpected end of connection")
        result += chunk
    return result


def client_frame(connection):
    first, second = receive_exact(connection, 2)
    if not second & 128:
        raise ValueError("Client frame was not masked")
    length = second & 127
    if length == 126:
        length = struct.unpack("!H", receive_exact(connection, 2))[0]
    elif length == 127:
        length = struct.unpack("!Q", receive_exact(connection, 8))[0]
    mask = receive_exact(connection, 4)
    payload = receive_exact(connection, length)
    return first & 15, bytes(value ^ mask[index % 4] for index, value in enumerate(payload))


def run_case(executable, scenario, locale, expected_success):
    state = {"http": 0, "triggers": 0, "errors": []}

    class HttpHandler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            state["http"] += 1
            if self.path != "/api/openedScene" or self.headers.get("X-Token") != TOKEN:
                state["errors"].append("Unexpected HTTP path or token")
            self.send_response(401 if scenario == "auth" else 200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            receiver = {"id": ASSET_ID, "typeId": TYPE_ID, "active": True, "name": "RhyLive Receiver"}
            assets = [] if scenario == "missing" else [receiver]
            if scenario == "multiple":
                assets.append({**receiver, "id": "66666666-7777-8888-9999-000000000000"})
            if scenario == "inactive":
                assets.append({**receiver, "id": "66666666-7777-8888-9999-000000000000", "active": False})
            if scenario == "object":
                assets = {ASSET_ID: {key: value for key, value in receiver.items() if key != "id"}}
            payload = b"invalid JSON" if scenario == "malformed" else json.dumps({"assets": assets}).encode()
            self.wfile.write(payload)

    class WebSocketHandler(socketserver.BaseRequestHandler):
        def handle(self):
            try:
                self.request.settimeout(8)
                request = b""
                while not request.endswith(b"\r\n\r\n"):
                    request += receive_exact(self.request, 1)
                headers = {}
                for line in request.decode().split("\r\n")[1:]:
                    if ":" in line:
                        key, value = line.split(":", 1)
                        headers[key.lower()] = value.strip()
                if headers.get("sec-websocket-protocol") != TOKEN:
                    raise ValueError("Missing WebSocket authentication")
                accept = base64.b64encode(hashlib.sha1(
                    (headers["sec-websocket-key"] + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest())
                if scenario == "handshake":
                    accept = b"invalid"
                self.request.sendall(b"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                                     b"Connection: Upgrade\r\nSec-WebSocket-Accept: " + accept +
                                     b"\r\nSec-WebSocket-Protocol: " + TOKEN.encode() + b"\r\n\r\n")
                if scenario == "handshake":
                    return
                opcode, payload = client_frame(self.request)
                command = json.loads(payload)
                if opcode != 1 or command != {"action": "invokeEntityTriggerPort",
                                               "data": {"id": ASSET_ID, "port": "Calibrate"}}:
                    raise ValueError("Incorrect calibration command")
                state["triggers"] += 1
                if scenario == "timeout":
                    time.sleep(6.2)
                    return
                response = {"action": "invokeEntityTriggerPort", "data": scenario != "unconfirmed"}
                if scenario == "trigger-error":
                    response["error"] = "Synthetic test error"
                message = json.dumps(response).encode()
                if scenario == "fragmented":
                    self.request.sendall(frame(b"test", opcode=9))
                    pong_opcode, pong_data = client_frame(self.request)
                    if (pong_opcode, pong_data) != (10, b"test"):
                        raise ValueError("Missing masked pong")
                    update = json.dumps({"action": "frameUpdate", "data": "x" * 66000}).encode()
                    self.request.sendall(frame(update))
                    self.request.sendall(frame(message[:20], final=False))
                    time.sleep(0.02)
                    self.request.sendall(frame(message[20:], opcode=0))
                else:
                    self.request.sendall(frame(message))
                try:
                    self.request.recv(1)
                except (ConnectionError, TimeoutError):
                    pass
            except Exception as error:
                state["errors"].append(str(error))

    class TcpServer(socketserver.ThreadingTCPServer):
        allow_reuse_address = True
        daemon_threads = True

    with tempfile.TemporaryDirectory() as folder:
        token_file = Path(folder) / "Player.log"
        token_file.write_text("One Time API Auth Token: " + TOKEN + "\n", encoding="utf-8")
        http_server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), HttpHandler)
        ws = TcpServer(("127.0.0.1", 0), WebSocketHandler)
        for server in (http_server, ws):
            threading.Thread(target=server.serve_forever, daemon=True).start()
        try:
            port = http_server.server_port
            if scenario == "offline":
                with socket.socket() as temporary:
                    temporary.bind(("127.0.0.1", 0))
                    port = temporary.getsockname()[1]
            result = subprocess.run([
                str(executable), scenario, locale, str(port), str(ws.server_address[1]),
                str(token_file), str(expected_success).lower()
            ], capture_output=True, text=True, encoding="utf-8", timeout=15,
                env={**os.environ, "QT_QPA_PLATFORM": "minimal"})
            if result.returncode:
                raise RuntimeError(f"{scenario}: exit {result.returncode}\n{result.stdout}\n{result.stderr}")
            if state["errors"]:
                raise RuntimeError(f"{scenario}: {state['errors']}")
            expected_triggers = 1 if scenario in {
                "success", "object", "inactive", "fragmented", "trigger-error", "unconfirmed", "timeout"
            } else 0
            if state["triggers"] != expected_triggers or state["http"] != (0 if scenario == "offline" else 1):
                raise RuntimeError(f"{scenario}: duplicate request or wrong receiver selection: {state}")
            print(result.stdout.strip(), flush=True)
        finally:
            http_server.shutdown()
            ws.shutdown()
            http_server.server_close()
            ws.server_close()


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    executable = Path(sys.argv[1]).resolve()
    cases = [
        ("success", "ko-KR", True),
        ("object", "ja-JP", True),
        ("inactive", "zh-TW", True),
        ("fragmented", "en-US", True),
        ("auth", "fr-FR", False),
        ("missing", "zh-CN", False),
        ("multiple", "de-DE", False),
        ("malformed", "es-ES", False),
        ("handshake", "pt-BR", False),
        ("trigger-error", "ko-KR", False),
        ("unconfirmed", "ja-JP", False),
        ("timeout", "en-US", False),
        ("offline", "ko-KR", False),
    ]
    for case in cases:
        run_case(executable, *case)
    print(f"Passed {len(cases)} isolated API integration scenarios")
