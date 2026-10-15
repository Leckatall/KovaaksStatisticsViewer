from __future__ import annotations

import itertools
import json
import socket
from typing import Any


DEFAULT_TIMEOUT_SECONDS = 60.0


class DriverConnectionError(RuntimeError):
    """The driver could not be reached (not running, or it dropped the connection)."""


class DriverCommandError(RuntimeError):
    """The driver ran the command and reported a failure."""

    def __init__(self, message: str, details: dict[str, Any] | None = None) -> None:
        super().__init__(message)
        self.details = details or {}


_request_ids = itertools.count(1)


def request(port: int, command: str, args: dict[str, Any] | None = None, *, timeout: float | None = None) -> dict[str, Any]:
    """Sends one newline-delimited JSON request to ksv_driver and returns its result object."""
    payload = {"id": next(_request_ids), "cmd": command, "args": args or {}}
    try:
        with socket.create_connection(("127.0.0.1", port), timeout=timeout or DEFAULT_TIMEOUT_SECONDS) as connection:
            connection.sendall(json.dumps(payload).encode("utf-8") + b"\n")
            buffer = bytearray()
            while not buffer.endswith(b"\n"):
                chunk = connection.recv(65536)
                if not chunk:
                    break
                buffer.extend(chunk)
    except (ConnectionError, TimeoutError, OSError) as error:
        raise DriverConnectionError(f"could not talk to ksv_driver on port {port}: {error}") from error
    if not buffer:
        raise DriverConnectionError(f"ksv_driver on port {port} closed the connection without replying")
    response = json.loads(buffer.decode("utf-8"))
    if not response.get("ok"):
        raise DriverCommandError(response.get("error", "unknown error"), response.get("details"))
    return response.get("result", {})
