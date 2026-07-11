"""
Regenerate the gRPC Python stubs from proto/BICgRPC.proto.

The generated stubs (generated/BICgRPC_pb2.py, generated/BICgRPC_pb2_grpc.py)
are committed to the repo so the server runs out of the box.  Run this only if
you edit the .proto file or upgrade grpcio-tools.

Usage:
    python scripts/build_proto.py
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
PROTO_DIR = REPO_ROOT / "proto"
OUT_DIR = REPO_ROOT / "generated"
PROTO_FILE = PROTO_DIR / "BICgRPC.proto"


def main() -> int:
    if not PROTO_FILE.exists():
        print(f"ERROR: {PROTO_FILE} not found", file=sys.stderr)
        return 1

    OUT_DIR.mkdir(exist_ok=True)

    cmd = [
        sys.executable, "-m", "grpc_tools.protoc",
        f"-I{PROTO_DIR}",
        f"--python_out={OUT_DIR}",
        f"--grpc_python_out={OUT_DIR}",
        str(PROTO_FILE),
    ]
    print("Running:", " ".join(cmd))
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print("protoc failed. Is grpcio-tools installed? (pip install grpcio-tools)",
              file=sys.stderr)
        return result.returncode

    print(f"OK — stubs written to {OUT_DIR}/")
    print("  BICgRPC_pb2.py")
    print("  BICgRPC_pb2_grpc.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
