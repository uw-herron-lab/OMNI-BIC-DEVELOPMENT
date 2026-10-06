"""
Minimal example: connect to the mock server, stream a few seconds of neural
data, and print what arrived.  Demonstrates how a downstream consumer uses
the reference NeuralStreamClient.

Prerequisite — start the server in another terminal:
    python -m src.mock_server                 # synthetic
    python -m src.mock_server --replay ./data/session_01   # file replay

Then run:
    python scripts/example_client.py
    python scripts/example_client.py --address 127.0.0.1:50051 --seconds 5
"""

from __future__ import annotations

import argparse
import logging
import sys
import time
from pathlib import Path

# Make ``src`` importable when run from the repo root or the scripts/ dir
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import numpy as np

from src.neural_stream_client import NeuralStreamClient, StreamConfig

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")


def main() -> int:
    parser = argparse.ArgumentParser(description="Example mock-server client")
    parser.add_argument("--address", default="127.0.0.1:50051",
                        help="Server address (default: 127.0.0.1:50051)")
    parser.add_argument("--seconds", type=float, default=5.0,
                        help="How long to stream before disconnecting (default: 5)")
    args = parser.parse_args()

    client = NeuralStreamClient(StreamConfig(server_address=args.address))

    print(f"Connecting to {args.address} ...")
    client.connect()

    try:
        time.sleep(args.seconds)
    finally:
        client.disconnect()

    # Inspect the ring buffer
    data, ts = client.read_latest(1000)  # last ~1 s
    print("-" * 55)
    print(f"Samples received : {client.samples_received}")
    print(f"Ring snapshot     : {data.shape} (channels x samples)")
    if np.isfinite(data).any():
        finite = data[np.isfinite(data)]
        print(f"Value range       : [{finite.min():.2f}, {finite.max():.2f}]")
        print(f"Ch0 mean/std      : {np.nanmean(data[0]):.2f} / {np.nanstd(data[0]):.2f}")
    print("-" * 55)
    print("OK — the mock server streamed data over the OMNI-BIC gRPC API.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
