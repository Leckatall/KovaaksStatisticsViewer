from __future__ import annotations

import json
import socket
import threading

import pytest

from ui_automation.client import DriverCommandError, DriverConnectionError, request


def serve_once(response: dict | None) -> tuple[int, list[dict]]:
    """A one-shot fake ksv_driver: records the request line, replies with `response`."""
    server = socket.socket()
    server.bind(("127.0.0.1", 0))
    server.listen(1)
    received: list[dict] = []

    def run() -> None:
        connection, _ = server.accept()
        with connection:
            buffer = b""
            while not buffer.endswith(b"\n"):
                buffer += connection.recv(4096)
            received.append(json.loads(buffer))
            if response is not None:
                reply = dict(response, id=received[0]["id"])
                connection.sendall(json.dumps(reply).encode() + b"\n")
        server.close()

    threading.Thread(target=run, daemon=True).start()
    return server.getsockname()[1], received


def test_request_sends_one_json_line_and_returns_the_result() -> None:
    port, received = serve_once({"ok": True, "result": {"pong": True}})

    assert request(port, "ping", {"x": 1}) == {"pong": True}
    assert received[0]["cmd"] == "ping"
    assert received[0]["args"] == {"x": 1}


def test_command_failures_carry_details() -> None:
    port, _ = serve_once({"ok": False, "error": "ambiguous", "details": {"candidates": [{"type": "Button"}]}})

    with pytest.raises(DriverCommandError, match="ambiguous") as raised:
        request(port, "click", {"selector": "type:Button"})
    assert raised.value.details["candidates"] == [{"type": "Button"}]


def test_a_silent_disconnect_is_a_connection_error() -> None:
    port, _ = serve_once(None)

    with pytest.raises(DriverConnectionError, match="without replying"):
        request(port, "ping")


def test_nothing_listening_is_a_connection_error() -> None:
    probe = socket.socket()
    probe.bind(("127.0.0.1", 0))
    port = probe.getsockname()[1]
    probe.close()

    with pytest.raises(DriverConnectionError):
        request(port, "ping", timeout=2)
