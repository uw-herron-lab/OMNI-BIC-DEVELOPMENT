# OMNI-BIC Mock Server

A software stand-in for the [OMNI-BIC](https://github.com/uw-herron-lab/OMNI-BIC-DEVELOPMENT)
neural acquisition microservice. It speaks the same gRPC API as the real
implant bridge, so you can develop and test clients, decoders, and
visualizations with **no implant, bridge, or acquisition hardware present**.

Two ways to produce a 32-channel, 1 kHz neural stream:

- **Synthetic** — generated ECoG with alternating rest / move epochs
  (strong alpha + beta at rest, alpha ERD + beta suppression on move).
- **File replay** — stream a previously recorded session back over the wire,
  optionally with ground-truth rest/move labels synced from a task log. This is
  the "neural file data streamer": real signals, replayed deterministically.

Downstream code connects exactly as it would to real hardware.

> **New here?** Start with [GETTING_STARTED.md](GETTING_STARTED.md) — a
> step-by-step walkthrough (install → run → verify → replay → integrate) with
> troubleshooting. The rest of this README is the reference summary.

## What's in here

| Path | Role |
|------|------|
| `src/mock_server.py` | The mock gRPC server (synthetic + file-replay modes) |
| `src/neural_stream_client.py` | Reference client — pulls the stream into a ring buffer |
| `src/sync.py` | Task-log ↔ device-clock alignment used for replay labels |
| `proto/BICgRPC.proto` | The OMNI-BIC gRPC service + message contract |
| `generated/` | Pre-built Python gRPC stubs (committed; run out of the box) |
| `scripts/build_proto.py` | Regenerate the stubs if you edit the `.proto` |
| `scripts/example_client.py` | Runnable demo: connect, stream, print what arrived |
| `GETTING_STARTED.md` | Step-by-step onboarding walkthrough + troubleshooting |
| `docs/FILE_FORMAT.md` | CSV schema for replay recordings |

## Install

Using conda:

```bash
conda env create -f environment.yml
conda activate omni-bic-mock
```

Or plain pip (into any Python ≥ 3.9 environment):

```bash
pip install -r requirements.txt
```

Synthetic mode needs only `grpcio` + `numpy`; file replay adds `pandas` +
`scipy`. `grpcio-tools` is only needed if you regenerate the stubs.

## Run it

**Synthetic** (instant):

```bash
python -m src.mock_server                 # listens on 0.0.0.0:50051
python -m src.mock_server --port 50052
```

**File replay** — point it at a folder with a `filterLog_*.csv`
(see [docs/FILE_FORMAT.md](docs/FILE_FORMAT.md)):

```bash
python -m src.mock_server --replay ./data/session_01
python -m src.mock_server --replay ./data/session_01 --task STD1
python -m src.mock_server --replay        # interactive picker over ./data
```

> No recordings ship with this repo. Drop your own under `data/` (git-ignored).

## Verify with the example client

In a second terminal, with the server running:

```bash
python scripts/example_client.py
python scripts/example_client.py --address 127.0.0.1:50051 --seconds 5
```

Expected output — a summary of the samples received and the value range,
confirming the stream came across the OMNI-BIC gRPC API.

## Using it from your own code

```python
from src.neural_stream_client import NeuralStreamClient, StreamConfig

client = NeuralStreamClient(StreamConfig(server_address="127.0.0.1:50051"))
client.connect()                       # runs the full BIC connect handshake
data, ts = client.read_latest(1000)    # (32, 1000) float64 + timestamps
client.disconnect()
```

The client mirrors the real BIC connect sequence: query info → scan/connect
bridge → scan/connect device → open the `bicNeuralStream` server stream and fill
a NumPy ring buffer. Swap the server address for a real microservice and the
same client works unchanged.

## The gRPC contract

Defined in `proto/BICgRPC.proto` and implemented by the mock across three
services:

- **BICInfoService** — version, supported devices, repository inspection
- **BICBridgeService** — scan / connect / describe / disconnect bridges
- **BICDeviceService** — scan/connect device, `bicNeuralStream` (the data
  stream), plus temperature / humidity / connection / power / error streams and
  stim endpoints (no-ops in the mock)

If you change the `.proto`, regenerate the stubs:

```bash
python scripts/build_proto.py
```

## Scope & provenance

Extracted from the OMNI-BIC real-time BCI applet to stand alone as a shareable
testing tool. Stim endpoints are stubs; the mock is for **data-path** testing
(streaming, decoding, visualization), not stimulation behavior.
