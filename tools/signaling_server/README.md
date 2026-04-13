# Signaling Server

Standalone Python WebRTC signaling server for GeraNES.

It implements the same JSON message protocol as the embedded signaling server in [`src/GeraNESNetplay/WebRtcSignalingServer.cpp`](c:/Users/geral/Desktop/pacman/GeraNES/src/GeraNESNetplay/WebRtcSignalingServer.cpp):

- `hello`
- `welcome`
- `room_list`
- `create_room`
- `join_room`
- `leave_room`
- `room_joined`
- `peer_joined`
- `peer_left`
- `offer`
- `answer`
- `ice_candidate`
- `error`

The server forwards SDP and ICE messages by `targetPeerId` inside a room, and broadcasts `peer_joined` / `peer_left` to the other peers in that room.

For robust cleanup, the protocol also supports `leave_room` for intentional disconnects. Socket close detection still cleans up crashed or disconnected peers even when heartbeats are disabled.

Room behavior:

- Hosts should send `create_room`
- Clients should send `join_room`
- `create_room` may include an optional `password`
- Empty password means a public room
- `join_room` must include the password field as well
- `create_room` returns signaling `error` with `Room already exists` if the room name is already taken
- `join_room` returns signaling `error` with `Invalid room password` if the password does not match
- `room_list` returns current active rooms with `passwordProtected`

## Install

```powershell
python -m pip install -r tools/signaling_server/requirements.txt
```

## Run

From the repo root:

```powershell
python tools/signaling_server/server.py --host 0.0.0.0 --port 8765
```

With ICE servers:

```powershell
python tools/signaling_server/server.py `
  --port 8765 `
  --stun "stun:stun.l.google.com:19302" `
  --turn-url "turn:username:password@turn.example.com:3478?transport=udp"
```

Or from a JSON config file:

```powershell
python tools/signaling_server/server.py --config tools/signaling_server/config.example.json
```

## ICE Server Behavior

Configured `iceServers` are attached to:

- `welcome`
- `room_joined`

That matches the current client behavior in [`src/GeraNESNetplay/NetTransport.cpp`](c:/Users/geral/Desktop/pacman/GeraNES/src/GeraNESNetplay/NetTransport.cpp), which reads signaled ICE servers during bootstrap from either of those messages.

## Config Format

Example:

```json
{
  "host": "0.0.0.0",
  "port": 8765,
  "logLevel": "INFO",
  "pingIntervalSeconds": null,
  "pingTimeoutSeconds": null,
  "iceServers": [
    "stun:stun.l.google.com:19302",
    "turn:username:password@turn.example.com:3478?transport=udp"
  ]
}
```

Notes:

- `iceServers` is a list of strings because that is what the current GeraNES signaling protocol carries.
- If you use TURN credentials, encode them directly in the TURN URL string.
- `pingIntervalSeconds` / `pingTimeoutSeconds` default to disabled (`null`).
- Set them to a positive integer to enable WebSocket heartbeats if you specifically want aggressive dead-socket detection.
- A heartbeat timeout will close the owner socket and therefore destroy the room, so use it carefully on unstable mobile/browser networks.

## Protocol Notes

`create_room` example:

```json
{
  "type": "create_room",
  "roomId": "my-room",
  "peerId": "host-123",
  "password": "secret"
}
```

Public room:

```json
{
  "type": "create_room",
  "roomId": "public-room",
  "peerId": "host-123",
  "password": ""
}
```

`join_room` example:

```json
{
  "type": "join_room",
  "roomId": "my-room",
  "peerId": "client-456",
  "password": "secret"
}
```

`room_list` response example:

```json
{
  "type": "room_list",
  "rooms": [
    {"roomId": "public-room", "passwordProtected": false},
    {"roomId": "private-room", "passwordProtected": true}
  ]
}
```
