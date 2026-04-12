#!/usr/bin/env python3
from __future__ import annotations

import argparse
import asyncio
import json
import logging
import signal
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

try:
    from websockets.asyncio.server import ServerConnection, serve
    from websockets.exceptions import ConnectionClosed
except ImportError as exc:
    raise SystemExit(
        "Missing Python dependency 'websockets'. "
        "Install it with: python -m pip install -r requirements.txt"
    ) from exc


PROTOCOL_VERSION = 1
VALID_MESSAGE_TYPES = {
    "hello",
    "welcome",
    "room_list",
    "create_room",
    "join_room",
    "room_joined",
    "peer_joined",
    "peer_left",
    "offer",
    "answer",
    "ice_candidate",
    "error",
}


@dataclass
class ClientState:
    peer_id: str = ""
    room_id: str = ""


@dataclass
class RoomState:
    password: str = ""
    max_participants: int = 2
    owner: ServerConnection | None = None
    owner_peer_id: str = ""
    members: set[ServerConnection] = field(default_factory=set)


@dataclass
class ServerConfig:
    host: str = "0.0.0.0"
    port: int = 8765
    ice_servers: list[str] = field(default_factory=list)
    log_level: str = "INFO"
    ping_interval_seconds: int | None = None
    ping_timeout_seconds: int | None = None


class SignalingServer:
    def __init__(self, config: ServerConfig) -> None:
        self._config = config
        self._clients: dict[ServerConnection, ClientState] = {}
        self._rooms: dict[str, RoomState] = {}
        self._lock = asyncio.Lock()
        self._stop_event = asyncio.Event()

    async def run(self) -> None:
        async with serve(
            self._handle_connection,
            self._config.host,
            self._config.port,
            max_size=1024 * 1024,
            ping_interval=self._config.ping_interval_seconds,
            ping_timeout=self._config.ping_timeout_seconds,
        ):
            logging.info(
                "Signaling server listening on ws://%s:%s with %d ICE server(s), ping_interval=%s, ping_timeout=%s",
                self._config.host,
                self._config.port,
                len(self._config.ice_servers),
                self._config.ping_interval_seconds,
                self._config.ping_timeout_seconds,
            )
            await self._stop_event.wait()

    def stop(self) -> None:
        self._stop_event.set()

    async def _handle_connection(self, websocket: ServerConnection) -> None:
        async with self._lock:
            self._clients[websocket] = ClientState()

        try:
            async for payload in websocket:
                await self._handle_message(websocket, payload)
        except ConnectionClosed:
            pass
        finally:
            await self._remove_client(websocket)

    async def _handle_message(self, websocket: ServerConnection, payload: str) -> None:
        message = self._parse_message(payload)
        if message is None:
            await self._send_error(websocket, "Invalid signaling payload")
            return

        message_type = message["type"]
        if message_type == "hello":
            await self._handle_hello(websocket, message)
            return
        if message_type == "room_list":
            await self._handle_room_list(websocket)
            return
        if message_type == "create_room":
            await self._handle_create_room(websocket, message)
            return
        if message_type == "join_room":
            await self._handle_join_room(websocket, message)
            return
        if message_type in {"offer", "answer", "ice_candidate"}:
            await self._handle_direct_signal(websocket, message)
            return

        await self._send_error(websocket, "Unsupported signaling message")

    async def _handle_hello(self, websocket: ServerConnection, message: dict[str, Any]) -> None:
        peer_id = _as_non_empty_string(message.get("peerId"))
        if not peer_id:
            await self._send_error(websocket, "Missing peer id")
            return

        async with self._lock:
            client = self._clients.setdefault(websocket, ClientState())
            client.peer_id = peer_id

        logging.info("peer hello peerId=%s requestedRoom=%s", peer_id, _as_string(message.get("roomId")))

        await self._send_message(
            websocket,
            {
                "type": "welcome",
                "roomId": _as_string(message.get("roomId")),
                "peerId": peer_id,
                "iceServers": self._config.ice_servers,
            },
        )

    async def _handle_room_list(self, websocket: ServerConnection) -> None:
        async with self._lock:
            rooms = [
                {
                    "roomId": room_id,
                    "passwordProtected": bool(room.password),
                }
                for room_id, room in self._rooms.items()
                if room.members
            ]

        await self._send_message(
            websocket,
            {
                "type": "room_list",
                "rooms": rooms,
            },
        )

    async def _handle_create_room(self, websocket: ServerConnection, message: dict[str, Any]) -> None:
        room_id = _as_non_empty_string(message.get("roomId"))
        peer_id = _as_non_empty_string(message.get("peerId"))
        password = _as_string(message.get("password"))
        max_participants = max(2, _as_int(message.get("maxParticipants"), 2))
        if not room_id or not peer_id:
            await self._send_error(websocket, "Missing room id or peer id")
            return

        async with self._lock:
            existing_room = self._rooms.get(room_id)
            if existing_room is not None and existing_room.members:
                room_taken = True
            else:
                room_taken = False
                client = self._clients.setdefault(websocket, ClientState())
                old_room_id = client.room_id
                client.peer_id = peer_id
                client.room_id = room_id

                if old_room_id and old_room_id != room_id:
                    old_room = self._rooms.get(old_room_id)
                    if old_room is not None:
                        old_room.members.discard(websocket)
                        if not old_room.members:
                            self._rooms.pop(old_room_id, None)

                room = self._rooms.setdefault(room_id, RoomState())
                room.password = password
                room.max_participants = max_participants
                room.owner = websocket
                room.owner_peer_id = peer_id
                room.members.add(websocket)
                logging.info(
                    "room created roomId=%s ownerPeerId=%s maxParticipants=%d members=%d",
                    room_id,
                    peer_id,
                    room.max_participants,
                    len(room.members),
                )

        if room_taken:
            logging.warning("create_room rejected roomId=%s peerId=%s reason=room_exists", room_id, peer_id)
            await self._send_error(websocket, "Room already exists")
            return

        await self._send_message(
            websocket,
            {
                "type": "room_joined",
                "roomId": room_id,
                "peerId": peer_id,
                "iceServers": self._config.ice_servers,
            },
        )

    async def _handle_join_room(self, websocket: ServerConnection, message: dict[str, Any]) -> None:
        room_id = _as_non_empty_string(message.get("roomId"))
        peer_id = _as_non_empty_string(message.get("peerId"))
        password = _as_string(message.get("password"))
        if not room_id or not peer_id:
            await self._send_error(websocket, "Missing room id or peer id")
            return

        async with self._lock:
            room = self._rooms.get(room_id)
            if room is None or not room.members:
                room_missing = True
                invalid_password = False
                room_full = False
                recipients: list[ServerConnection] = []
            else:
                room_missing = False
                if room.owner is None or room.owner not in room.members:
                    room_missing = True
                    invalid_password = False
                    room_full = False
                    recipients = []
                else:
                    invalid_password = room.password != password
                    room_full = len(room.members) >= room.max_participants
                    if invalid_password or room_full:
                        recipients = []
                    else:
                        recipients = [member for member in room.members if member is not websocket]

                        client = self._clients.setdefault(websocket, ClientState())
                        old_room_id = client.room_id
                        client.peer_id = peer_id
                        client.room_id = room_id

                        if old_room_id and old_room_id != room_id:
                            old_room = self._rooms.get(old_room_id)
                            if old_room is not None:
                                old_room.members.discard(websocket)
                                if old_room.owner is websocket:
                                    self._rooms.pop(old_room_id, None)
                                elif not old_room.members:
                                    self._rooms.pop(old_room_id, None)

                        room.members.add(websocket)

        if room_missing:
            logging.warning("join_room rejected roomId=%s peerId=%s reason=room_missing", room_id, peer_id)
            await self._send_error(websocket, "Room does not exist")
            return
        if invalid_password:
            logging.warning("join_room rejected roomId=%s peerId=%s reason=invalid_password", room_id, peer_id)
            await self._send_error(websocket, "Invalid room password")
            return
        if room_full:
            logging.warning("join_room rejected roomId=%s peerId=%s reason=room_full", room_id, peer_id)
            await self._send_error(websocket, "Room is full")
            return

        logging.info(
            "room joined roomId=%s peerId=%s notifyRecipients=%d recipients=%s",
            room_id,
            peer_id,
            len(recipients),
            self._describe_peer_ids(recipients),
        )

        await self._send_message(
            websocket,
            {
                "type": "room_joined",
                "roomId": room_id,
                "peerId": peer_id,
                "iceServers": self._config.ice_servers,
            },
        )

        await self._broadcast_to_specific_peers(
            recipients,
            {
                "type": "peer_joined",
                "roomId": room_id,
                "peerId": peer_id,
            },
        )

    async def _handle_direct_signal(self, websocket: ServerConnection, message: dict[str, Any]) -> None:
        target_peer_id = _as_non_empty_string(message.get("targetPeerId"))
        if not target_peer_id:
            await self._send_error(websocket, "Missing target peer id")
            return

        async with self._lock:
            client = self._clients.setdefault(websocket, ClientState())
            if not client.room_id:
                room_id = ""
                sender_peer_id = ""
                target_ws = None
            else:
                room_id = client.room_id
                sender_peer_id = client.peer_id
                target_ws = self._find_peer_in_room_locked(room_id, target_peer_id)

        if not room_id:
            await self._send_error(websocket, "Join a room before sending signaling data")
            return
        if target_ws is None:
            logging.warning(
                "direct_signal dropped type=%s roomId=%s senderPeerId=%s targetPeerId=%s reason=target_not_connected",
                _as_string(message.get("type")),
                room_id,
                sender_peer_id,
                target_peer_id,
            )
            await self._send_error(websocket, "Target peer is not connected")
            return

        logging.info(
            "direct_signal type=%s roomId=%s senderPeerId=%s targetPeerId=%s",
            _as_string(message.get("type")),
            room_id,
            sender_peer_id,
            target_peer_id,
        )

        outbound = {
            "type": message["type"],
            "roomId": room_id,
            "peerId": sender_peer_id,
            "targetPeerId": target_peer_id,
            "sdp": _as_string(message.get("sdp")),
            "candidate": _as_string(message.get("candidate")),
            "mid": _as_string(message.get("mid")),
            "mlineIndex": _as_int(message.get("mlineIndex"), -1),
        }
        await self._send_message(target_ws, outbound)

    async def _remove_client(self, websocket: ServerConnection) -> None:
        peer_left_message: dict[str, Any] | None = None
        room_id = ""
        owner_disconnected = False
        owner_peer_id = ""
        orphan_recipients: list[ServerConnection] = []

        async with self._lock:
            client = self._clients.pop(websocket, None)
            if client is None:
                return

            if client.room_id:
                room_id = client.room_id
                room = self._rooms.get(client.room_id)
                if room is not None:
                    room.members.discard(websocket)
                    if room.owner is websocket:
                        owner_disconnected = True
                        owner_peer_id = room.owner_peer_id or client.peer_id
                        orphan_recipients = list(room.members)
                        self._rooms.pop(client.room_id, None)
                    elif not room.members:
                        self._rooms.pop(client.room_id, None)

                if client.peer_id:
                    logging.info(
                        "peer disconnected roomId=%s peerId=%s remainingMembers=%d",
                        client.room_id,
                        client.peer_id,
                        len(room.members) if room is not None else 0,
                    )
                    peer_left_message = {
                        "type": "peer_left",
                        "roomId": client.room_id,
                        "peerId": client.peer_id,
                    }

        if owner_disconnected and room_id:
            logging.warning(
                "room owner disconnected roomId=%s ownerPeerId=%s; closing %d remaining member(s)",
                room_id,
                owner_peer_id,
                len(orphan_recipients),
            )
            await self._close_specific_peers(
                orphan_recipients,
                code=1012,
                reason="Room owner disconnected",
            )

        if peer_left_message is not None and room_id:
            await self._broadcast_to_room(room_id, peer_left_message, except_ws=websocket)

    async def _send_error(self, websocket: ServerConnection, error: str) -> None:
        await self._send_message(websocket, {"type": "error", "error": error})

    async def _send_message(self, websocket: ServerConnection, message: dict[str, Any]) -> None:
        payload = _normalize_message(message, ice_servers=self._config.ice_servers)
        try:
            await websocket.send(json.dumps(payload, separators=(",", ":")))
        except Exception as exc:
            logging.warning(
                "send failed type=%s peerId=%s roomId=%s error=%s",
                payload.get("type", ""),
                payload.get("peerId", ""),
                payload.get("roomId", ""),
                exc,
            )
            raise

    async def _broadcast_to_room(
        self,
        room_id: str,
        message: dict[str, Any],
        except_ws: ServerConnection | None = None,
    ) -> None:
        async with self._lock:
            room = self._rooms.get(room_id)
            recipients = list(room.members) if room is not None else []

        if except_ws is not None:
            recipients = [recipient for recipient in recipients if recipient is not except_ws]

        await self._broadcast_to_specific_peers(recipients, message)

    async def _broadcast_to_specific_peers(
        self,
        recipients: list[ServerConnection],
        message: dict[str, Any],
    ) -> None:
        if not recipients:
            return

        payload = json.dumps(
            _normalize_message(message, ice_servers=self._config.ice_servers),
            separators=(",", ":"),
        )
        async with self._lock:
            recipient_peer_ids = [
                self._clients.get(recipient).peer_id
                for recipient in recipients
                if self._clients.get(recipient) is not None
            ]
        logging.info(
            "broadcast type=%s roomId=%s peerId=%s recipients=%s",
            message.get("type", ""),
            _as_string(message.get("roomId")),
            _as_string(message.get("peerId")),
            recipient_peer_ids,
        )
        results = await asyncio.gather(
            *(recipient.send(payload) for recipient in recipients),
            return_exceptions=True,
        )
        for result in results:
            if isinstance(result, Exception):
                logging.warning(
                    "broadcast send failed type=%s roomId=%s peerId=%s error=%s",
                    message.get("type", ""),
                    _as_string(message.get("roomId")),
                    _as_string(message.get("peerId")),
                    result,
                )

    async def _close_specific_peers(
        self,
        recipients: list[ServerConnection],
        *,
        code: int,
        reason: str,
    ) -> None:
        if not recipients:
            return

        results = await asyncio.gather(
            *(recipient.close(code=code, reason=reason) for recipient in recipients),
            return_exceptions=True,
        )
        for result in results:
            if isinstance(result, Exception):
                logging.warning("peer close failed code=%s reason=%s error=%s", code, reason, result)

    def _find_peer_in_room_locked(
        self,
        room_id: str,
        peer_id: str,
    ) -> ServerConnection | None:
        room = self._rooms.get(room_id)
        if room is None:
            return None
        for websocket in room.members:
            client = self._clients.get(websocket)
            if client is not None and client.peer_id == peer_id:
                return websocket
        return None

    def _describe_peer_ids(self, recipients: list[ServerConnection]) -> list[str]:
        return [
            self._clients.get(recipient).peer_id
            for recipient in recipients
            if self._clients.get(recipient) is not None
        ]

    @staticmethod
    def _parse_message(payload: str) -> dict[str, Any] | None:
        try:
            message = json.loads(payload)
        except json.JSONDecodeError:
            return None
        if not isinstance(message, dict):
            return None
        message_type = message.get("type")
        if not isinstance(message_type, str) or message_type not in VALID_MESSAGE_TYPES:
            return None
        return message


def _as_string(value: Any) -> str:
    return value if isinstance(value, str) else ""


def _as_non_empty_string(value: Any) -> str:
    if isinstance(value, str):
        value = value.strip()
        if value:
            return value
    return ""


def _as_int(value: Any, default: int) -> int:
    return value if isinstance(value, int) else default


def _as_optional_int(value: Any) -> int | None:
    return value if isinstance(value, int) and value > 0 else None


def _normalize_message(message: dict[str, Any], ice_servers: list[str]) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "version": PROTOCOL_VERSION,
        "type": message["type"],
    }

    room_id = _as_string(message.get("roomId"))
    peer_id = _as_string(message.get("peerId"))
    target_peer_id = _as_string(message.get("targetPeerId"))
    sdp = _as_string(message.get("sdp"))
    candidate = _as_string(message.get("candidate"))
    mid = _as_string(message.get("mid"))
    password = _as_string(message.get("password"))
    error = _as_string(message.get("error"))
    rooms = message.get("rooms")
    mline_index = _as_int(message.get("mlineIndex"), -1)
    include_ice = bool(message.get("iceServers")) or message["type"] in {"welcome", "room_joined"}

    if room_id:
        payload["roomId"] = room_id
    if peer_id:
        payload["peerId"] = peer_id
    if target_peer_id:
        payload["targetPeerId"] = target_peer_id
    if sdp:
        payload["sdp"] = sdp
    if candidate:
        payload["candidate"] = candidate
    if mid:
        payload["mid"] = mid
    if password:
        payload["password"] = password
    if mline_index >= 0:
        payload["mlineIndex"] = mline_index
    if error:
        payload["error"] = error
    if include_ice and ice_servers:
        payload["iceServers"] = list(ice_servers)
    if isinstance(rooms, list):
        normalized_rooms: list[dict[str, Any]] = []
        for room in rooms:
            if not isinstance(room, dict):
                continue
            listed_room_id = _as_non_empty_string(room.get("roomId"))
            if not listed_room_id:
                continue
            normalized_rooms.append(
                {
                    "roomId": listed_room_id,
                    "passwordProtected": bool(room.get("passwordProtected")),
                }
            )
        if normalized_rooms:
            payload["rooms"] = normalized_rooms

    return payload


def _load_config_file(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        loaded = json.load(handle)
    if not isinstance(loaded, dict):
        raise ValueError("Config file root must be a JSON object.")
    return loaded


def _build_config(args: argparse.Namespace) -> ServerConfig:
    loaded: dict[str, Any] = {}
    if args.config is not None:
        loaded = _load_config_file(Path(args.config))

    host = str(args.host if args.host is not None else loaded.get("host", "0.0.0.0"))
    port = int(args.port if args.port is not None else loaded.get("port", 8765))
    log_level = str(args.log_level if args.log_level is not None else loaded.get("logLevel", "INFO"))
    ping_interval_seconds = _as_optional_int(
        args.ping_interval if args.ping_interval is not None else loaded.get("pingIntervalSeconds")
    )
    ping_timeout_seconds = _as_optional_int(
        args.ping_timeout if args.ping_timeout is not None else loaded.get("pingTimeoutSeconds")
    )

    ice_servers: list[str] = []
    if isinstance(loaded.get("iceServers"), list):
        ice_servers.extend(str(entry).strip() for entry in loaded["iceServers"] if str(entry).strip())

    for values in (args.ice_server, args.stun, args.turn_url):
        ice_servers.extend(value.strip() for value in values if value.strip())

    deduped_ice_servers: list[str] = []
    seen: set[str] = set()
    for entry in ice_servers:
        if entry not in seen:
            deduped_ice_servers.append(entry)
            seen.add(entry)

    return ServerConfig(
        host=host,
        port=port,
        ice_servers=deduped_ice_servers,
        log_level=log_level.upper(),
        ping_interval_seconds=ping_interval_seconds,
        ping_timeout_seconds=ping_timeout_seconds,
    )


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Standalone GeraNES WebRTC signaling server.",
    )
    parser.add_argument("--config", help="Path to a JSON config file.")
    parser.add_argument("--host", help="Host/interface to bind. Defaults to 0.0.0.0.")
    parser.add_argument("--port", type=int, help="TCP port to bind. Defaults to 8765.")
    parser.add_argument(
        "--ice-server",
        action="append",
        default=[],
        help="ICE server URL to advertise to clients. Repeat as needed.",
    )
    parser.add_argument(
        "--stun",
        action="append",
        default=[],
        help="Convenience alias for --ice-server with a STUN URL.",
    )
    parser.add_argument(
        "--turn-url",
        action="append",
        default=[],
        help="Convenience alias for --ice-server with a TURN URL.",
    )
    parser.add_argument(
        "--log-level",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        help="Logging verbosity.",
    )
    parser.add_argument(
        "--ping-interval",
        type=int,
        help="WebSocket ping interval in seconds. Omit or set <= 0 in config to disable server heartbeats.",
    )
    parser.add_argument(
        "--ping-timeout",
        type=int,
        help="WebSocket ping timeout in seconds. Omit or set <= 0 in config to disable server heartbeats.",
    )
    return parser.parse_args()


async def _async_main() -> int:
    args = _parse_args()
    config = _build_config(args)

    logging.basicConfig(
        level=getattr(logging, config.log_level, logging.INFO),
        format="%(asctime)s %(levelname)s %(message)s",
    )

    server = SignalingServer(config)
    loop = asyncio.get_running_loop()

    for sig_name in ("SIGINT", "SIGTERM"):
        sig = getattr(signal, sig_name, None)
        if sig is None:
            continue
        try:
            loop.add_signal_handler(sig, server.stop)
        except NotImplementedError:
            signal.signal(sig, lambda *_: server.stop())

    await server.run()
    return 0


def main() -> int:
    try:
        return asyncio.run(_async_main())
    except KeyboardInterrupt:
        return 0
    except Exception as exc:
        logging.error("Signaling server failed: %s", exc)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
