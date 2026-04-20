# clauduino-led

Arduino-driven LED strip that lights up when Claude Code finishes a task. Claude Code hooks publish over MQTT; an Arduino subscribes and drives the strip.

## Architecture

```
Claude Code (Stop/SubagentStop hook)
        │ publishes
        ▼
  MQTT broker (Mosquitto, this repo)
        │ subscribed by
        ▼
  Arduino + LED strip
```

- **Broker:** Eclipse Mosquitto 2.0 in Docker (this repo).
- **Transport:** MQTT over TCP on `1883`, WebSockets on `9001` (for browser debug clients).
- **Auth:** anonymous (LAN-only test setup).
- **Topic convention (proposed):** `clauduino/led/<event>` — e.g. `clauduino/led/status` for task-finished events.

## Layout

- `docker-compose.yml` — starts the Mosquitto broker
- `mosquitto/config/mosquitto.conf` — broker config
- `mosquitto/data/`, `mosquitto/log/` — runtime (gitignored)

Arduino sketch and Claude Code hook script will live alongside these as the project grows.

## Running the broker

`make help` lists all targets. Common ones:

```bash
make up          # start broker
make ps          # check health
make logs        # tail logs
make sub         # interactive subscribe to clauduino/#
make smoke       # end-to-end pub/sub round-trip test
make down        # stop broker
make clean       # stop + wipe runtime data/log
```

Raw docker-compose works too (`docker-compose up -d`, etc.) if you prefer.

## Smoke-testing pub/sub

Subscribe (inside the broker container):
```bash
docker exec -it clauduino-mqtt mosquitto_sub -h localhost -t 'clauduino/#' -v
```

Publish from another terminal:
```bash
docker exec clauduino-mqtt mosquitto_pub -h localhost -t 'clauduino/led/status' -m 'task_complete'
```

From a separate host/container (confirms the published host port works):
```bash
docker run --rm eclipse-mosquitto:2.0 \
  mosquitto_pub -h host.docker.internal -p 1883 \
  -t 'clauduino/led/status' -m 'task_complete'
```

## Conventions

- **Commits:** always create commits via the `/commit` skill rather than crafting commit messages by hand. It keeps message style consistent across the project.

## Status

- [x] Mosquitto broker in docker-compose, verified pub/sub round-trip
- [ ] Claude Code hook script that publishes on task completion
- [ ] Arduino sketch that subscribes and drives the LED strip
