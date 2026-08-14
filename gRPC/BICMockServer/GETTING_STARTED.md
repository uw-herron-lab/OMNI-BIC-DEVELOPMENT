# Getting started

A step-by-step walkthrough for running the OMNI-BIC mock server for the first
time and pointing your own code at it. If you just want the reference summary,
see [README.md](README.md).

**What this is:** a software stand-in for the OMNI-BIC acquisition microservice.
It streams 32-channel, 1 kHz neural data over the *same* gRPC API as the real
implant bridge, so you can build and test clients, decoders, and visualizations.

**What you need:** Windows with Python 3.9+.

---

## 1. Install dependencies & generate Python stubs

Install the required Python dependencies:

```bash
pip install -r requirements.txt
```

Then generate the Python gRPC stubs from the OMNI-BIC protocol definition:

```bash
python scripts/build.py
```

This reads `gRPC/Protos/BICgRPC.proto` and generates required Python bindings in `generated/`

## 2. Start the server (synthetic mode)

```bash
python -m src.mock_server
```

You should see:

```
=======================================================
  Mock BIC gRPC Server running on 0.0.0.0:50051
  32 channels @ 1 kHz | SYNTHETIC (REST/MOVE epochs, 3s each)
=======================================================
```

Leave this terminal running. The server generates fake ECoG that alternates
every 3 seconds between a "rest" rhythm (strong alpha + beta) and a "move"
pattern (alpha ERD + beta suppression) on the motor channels.

## 3. Confirm it works

Open a **second terminal** (activate the same environment), then:

```bash
python scripts/example_client.py
```

Expected:

```
-------------------------------------------------------
Samples received : 2800
Ring snapshot     : (32, 1000) (channels x samples)
Value range       : [-63.41, 59.85]
Ch0 mean/std      : -0.03 / 22.53
-------------------------------------------------------
This means the mock server streamed data over the OMNI-BIC gRPC API.
```

If you see that, the full connect handshake and data stream are working. You're
ready to point your own code at `127.0.0.1:50051`.

---

## 5. Replay a real recording (the "file streamer")

Instead of synthetic data, you can stream a previously recorded session back
over the wire — useful for reproducible decoder testing against real signals.

1. Put a recording folder somewhere, e.g. `./data/session_01/`, containing:
   - `filterLog_*.csv` — the neural recording (required)
   - `*Task_Log*.csv` — task markers (optional; gives rest/move labels)

   The exact column schema is in [docs/FILE_FORMAT.md](docs/FILE_FORMAT.md).
   **No recordings ship with this repo** — bring your own. Anything under
   `data/` is git-ignored so you can't accidentally commit patient data.

2. Start the server in replay mode:

   ```bash
   python -m src.mock_server --replay ./data/session_01 --task STD1
   ```

   Or browse folders under `./data` interactively:

   ```bash
   python -m src.mock_server --replay
   ```

3. Connect with the example client (or your own) exactly as before. The stream
   now carries the recorded channel values, and each sample is tagged with a
   ground-truth rest/move label derived from the task log.

The recording loops back to the start when it runs out, so the stream never
ends.

---

## 6. Use it from your own code

The reference client handles the whole BIC connect sequence and buffers samples
for you:

```python
from src.neural_stream_client import NeuralStreamClient, StreamConfig

client = NeuralStreamClient(StreamConfig(server_address="127.0.0.1:50051"))
client.connect()                        # info → bridge → device → open stream

# Grab the most recent 1 second (32 channels x 1000 samples) whenever you want:
data, timestamps = client.read_latest(1000)
# ... run your feature extraction / decoder on `data` ...

client.disconnect()
```

Because the mock speaks the real API, **the same client code works unchanged
against the real OMNI-BIC microservice** — just change `server_address`.

Prefer to talk gRPC directly (any language)? The service and message
definitions are in `gRPC/Protos/BICgRPC.proto`. The data stream is the
`BICDeviceService.bicNeuralStream` server-streaming RPC; generate stubs for your
language from that `.proto`.

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| `ModuleNotFoundError: No module named 'grpc'` | Dependencies not installed, or wrong environment active. Re-run step 2 and make sure the env is activated in *this* terminal. |
| Client: `failed to connect to all addresses` / `StatusCode.UNAVAILABLE` | The server isn't running, or you used the wrong port. Start it (step 3) and match `--port` / `--address`. |
| `No filterLog CSV in <dir>` (replay) | The folder has no `filterLog_*.csv`. Check the path and filename pattern (see docs/FILE_FORMAT.md). |
| Replay logs `TriStreamSync failed ... falling back to simple sync` | Not fatal — labels are still produced by the fallback aligner. Usually means the task log is sparse or clocks don't line up; the neural stream itself is unaffected. |
| Port 50051 already in use | Another server (or a real microservice) is on that port. Start with `--port 50052` and point your client at it. |
| Two machines: client can't reach server | The server binds `0.0.0.0`, so use the server machine's LAN IP in the client (`--address 192.168.x.x:50051`) and allow the port through the firewall. |

## What's mocked vs. real

- **Fully implemented:** the info/bridge/device connect handshake and the
  neural data stream (`bicNeuralStream`) — the data path you decode against.
- **Stubbed no-ops:** stimulation endpoints and the temperature/humidity/power
  housekeeping streams return canned values. This tool is for **data-path**
  testing, not stimulation behavior.
