"""
Regenerate the gRPC Python stubs from gRPC/Protos/BICgRPC.proto.

Usage:
    python scripts/build_proto.py
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

BICMOCKSERVER_ROOT = Path(__file__).resolve().parent.parent
GRPC_ROOT = BICMOCKSERVER_ROOT.parent

PROTO_DIR = GRPC_ROOT / "Protos"
OUT_DIR = BICMOCKSERVER_ROOT / "generated"
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
