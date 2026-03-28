import base64
import os
import socket
import struct
import subprocess
import time
from pathlib import Path


def _send_text_frame(sock: socket.socket, text: str) -> None:
    payload = text.encode("utf-8")
    header = bytearray([0x81])
    n = len(payload)
    if n < 126:
        header.append(0x80 | n)
    elif n < (1 << 16):
        header.append(0x80 | 126)
        header.extend(struct.pack("!H", n))
    else:
        header.append(0x80 | 127)
        header.extend(struct.pack("!Q", n))
    mask = os.urandom(4)
    header.extend(mask)
    masked = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
    sock.sendall(header + masked)


def _recv_text_frame(sock: socket.socket) -> str:
    head = sock.recv(2)
    if len(head) < 2:
        raise AssertionError("websocket frame header too short")
    opcode = head[0] & 0x0F
    length = head[1] & 0x7F
    masked = (head[1] & 0x80) != 0
    if length == 126:
        length = struct.unpack("!H", sock.recv(2))[0]
    elif length == 127:
        length = struct.unpack("!Q", sock.recv(8))[0]

    mask_key = b""
    if masked:
        mask_key = sock.recv(4)

    payload = b""
    while len(payload) < length:
        payload += sock.recv(length - len(payload))

    if masked:
        payload = bytes(b ^ mask_key[i % 4] for i, b in enumerate(payload))

    assert opcode == 1, f"expected text frame, got opcode={opcode}"
    return payload.decode("utf-8", errors="replace")


def test_control_plane_websocket_subscribe_snapshot():
    repo_root = Path(__file__).resolve().parents[1]
    runner_target = repo_root / "build" / "tests" / "test_control_plane_server_runner"
    port = 19190

    subprocess.run(
        [
            "make",
            "build/tests/test_control_plane_server_runner",
            "TEST=test_control_plane_server_runner",
        ],
        cwd=repo_root,
        check=True,
        capture_output=True,
        text=True,
    )

    env = dict(os.environ)
    env["XIAOZHI_TEST_PORT"] = str(port)
    proc = subprocess.Popen(
        [str(runner_target)],
        cwd=repo_root,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    try:
        deadline = time.time() + 5
        while time.time() < deadline:
            line = proc.stdout.readline()
            if "control_plane_runner_ready" in line:
                break
        else:
            raise AssertionError("runner did not become ready")

        key = base64.b64encode(os.urandom(16)).decode("ascii")
        with socket.create_connection(("127.0.0.1", port), timeout=2) as sock:
            request = (
                "GET / HTTP/1.1\r\n"
                f"Host: 127.0.0.1:{port}\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                f"Sec-WebSocket-Key: {key}\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n"
            )
            sock.sendall(request.encode("ascii"))
            response = sock.recv(4096)
            assert b"101" in response, response.decode("latin1", errors="replace")

            _send_text_frame(
                sock,
                '{"type":"command","name":"subscribe_events","payload":{}}',
            )
            message = _recv_text_frame(sock)
            assert '"name":"state_changed"' in message
            assert '"state":"idle"' in message
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=2)
