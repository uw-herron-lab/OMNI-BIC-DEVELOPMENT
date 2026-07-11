"""
Layer 1 – NeuralStreamClient
gRPC client that connects to the OMNI-BIC microservice, opens a server-side
neural stream, and feeds raw samples into a NumPy ring buffer at ~1 kHz.

Connection sequence (mirrors the C# RealtimeGraphing reference client):
  1. Open an insecure channel to the BIC microservice (default 127.0.0.1:50051).
  2. Query BICInfoService for version / supported devices.
  3. Scan for bridges via BICBridgeService, connect to the first one found.
  4. Scan for implantable devices via BICDeviceService, connect.
  5. Open a server-streaming bicNeuralStream RPC and begin filling the ring buffer.
"""

from __future__ import annotations

import logging
import threading
import time
from dataclasses import dataclass, field
from typing import Callable, Optional

import grpc
import numpy as np

# ── generated stubs (run `python -m grpc_tools.protoc ...` first) ──────────
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'generated'))
import BICgRPC_pb2 as pb
import BICgRPC_pb2_grpc as pb_grpc

logger = logging.getLogger(__name__)


# ── Per-sample metadata dtype (mirrors NeuralSample proto fields) ─────────
SAMPLE_META_DTYPE = np.dtype([
    ("sample_counter",     np.uint32),
    ("stim_active",        np.uint8),
    ("is_interpolated",    np.uint8),
    ("filt_sample",        np.float64),
    ("filt_channel",       np.uint32),
    ("phase",              np.float64),
    ("trigger_phase",      np.float64),
    ("pre_filt_sample",    np.float64),
    ("hampel_filt_sample", np.float64),
    ("is_valid_target",    np.uint8),
    ("is_input_trig_high", np.uint8),
])


# ── Configuration ──────────────────────────────────────────────────────────
@dataclass
class StreamConfig:
    """Parameters that govern the gRPC connection and ring buffer."""
    server_address: str = "127.0.0.1:50051"
    n_channels: int = 32
    sampling_rate_hz: int = 1000
    ring_buffer_seconds: float = 10.0          # how much history to keep
    grpc_buffer_size: int = 100                 # samples per NeuralUpdate message
    max_interpolation_points: int = 10
    amplification_factor: int = 3               # 0–3 → 57.5 / 51.5 / 45.5 / 39.5 dB
    ref_channels: list[int] = field(default_factory=lambda: [17])
    use_ground_reference: bool = True


class NeuralStreamClient:
    """
    Manages the full lifecycle of a BIC neural data stream.

    Usage
    -----
    >>> client = NeuralStreamClient(StreamConfig())
    >>> client.connect()          # blocks until streaming starts
    >>> snapshot = client.read()  # returns (n_channels, ring_len) float64 array
    >>> client.disconnect()
    """

    # ── construction ───────────────────────────────────────────────────────
    def __init__(
        self,
        config: StreamConfig | None = None,
        on_new_data: Optional[Callable[[np.ndarray, np.ndarray], None]] = None,
    ):
        self.cfg = config or StreamConfig()
        self._on_new_data = on_new_data  # optional callback(new_samples, timestamps)

        ring_len = int(self.cfg.ring_buffer_seconds * self.cfg.sampling_rate_hz)
        self._ring = np.full((self.cfg.n_channels, ring_len), np.nan, dtype=np.float64)
        self._ts_ring = np.zeros(ring_len, dtype=np.uint64)
        self._meta_ring = np.zeros(ring_len, dtype=SAMPLE_META_DTYPE)
        self._write_idx = 0
        self._samples_received = 0
        self._lock = threading.Lock()

        self._channel: Optional[grpc.Channel] = None
        self._bridge_stub: Optional[pb_grpc.BICBridgeServiceStub] = None
        self._device_stub: Optional[pb_grpc.BICDeviceServiceStub] = None
        self._info_stub: Optional[pb_grpc.BICInfoServiceStub] = None
        self._device_address: Optional[str] = None
        self._stream_thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()
        self._connected = False
        self._last_sample_counter: int = 0

    # ── public API ─────────────────────────────────────────────────────────
    def connect(self) -> None:
        """Run the full connection sequence and start streaming."""
        logger.info("Opening gRPC channel to %s", self.cfg.server_address)
        self._channel = grpc.insecure_channel(self.cfg.server_address)

        # Instantiate service stubs
        self._info_stub = pb_grpc.BICInfoServiceStub(self._channel)
        self._bridge_stub = pb_grpc.BICBridgeServiceStub(self._channel)
        self._device_stub = pb_grpc.BICDeviceServiceStub(self._channel)

        # 1) Info query
        ver = self._info_stub.VersionNumber(pb.VersionNumberRequest())
        devs = self._info_stub.SupportedDevices(pb.SupportedDevicesRequest())
        logger.info("BIC version: %s | Supported devices: %s",
                     ver.version_number, list(devs.supported_devices))

        # 2) Scan & connect bridge
        scan_resp = self._bridge_stub.ScanBridges(pb.QueryBridgesRequest())
        if not scan_resp.bridges:
            raise RuntimeError("No BIC bridges found during scan.")
        bridge_name = scan_resp.bridges[0].name
        logger.info("Connecting to bridge: %s", bridge_name)
        conn_resp = self._bridge_stub.ConnectBridge(
            pb.ConnectBridgeRequest(name=bridge_name)
        )
        logger.info("Bridge connection status: %s", conn_resp.connection_status)

        # 3) Scan & connect device
        dev_resp = self._device_stub.ScanDevices(
            pb.ScanDevicesRequest(bridgeName=bridge_name)
        )
        if not dev_resp.name:
            raise RuntimeError("No implantable devices found during scan.")
        self._device_address = dev_resp.name
        logger.info("Connecting to device: %s  (SR=%d Hz, CH=%d)",
                     self._device_address,
                     dev_resp.discoveredDevice.samplingRate,
                     dev_resp.discoveredDevice.measurementChannelCount)
        self._device_stub.ConnectDevice(
            pb.ConnectDeviceRequest(
                deviceAddress=self._device_address,
                logFileName="./bci_device_log.txt",
            )
        )

        # 4) Start the neural stream in a background thread
        self._stop_event.clear()
        self._stream_thread = threading.Thread(
            target=self._stream_loop, daemon=True, name="NeuralStreamThread"
        )
        self._stream_thread.start()
        self._connected = True
        logger.info("Neural stream started.")

    def disconnect(self) -> None:
        """Gracefully shut down the stream and gRPC channel."""
        if not self._connected:
            return
        logger.info("Disconnecting…")
        self._stop_event.set()
        if self._stream_thread:
            self._stream_thread.join(timeout=5.0)

        # Tell the server to stop streaming & dispose
        try:
            self._device_stub.bicNeuralStream(
                pb.bicNeuralSetStreamingEnable(
                    deviceAddress=self._device_address, enable=False
                )
            )
            self._device_stub.bicDispose(
                pb.RequestDeviceAddress(deviceAddress=self._device_address)
            )
        except grpc.RpcError as e:
            logger.warning("Error during dispose: %s", e)

        if self._channel:
            self._channel.close()
        self._connected = False
        logger.info("Disconnected.")

    def read(self) -> tuple[np.ndarray, np.ndarray]:
        """
        Return a *copy* of the current ring buffer contents, ordered oldest → newest.

        Returns
        -------
        data : np.ndarray, shape (n_channels, ring_len)
        timestamps : np.ndarray, shape (ring_len,)
        """
        with self._lock:
            idx = self._write_idx % self._ring.shape[1]
            data = np.roll(self._ring, -idx, axis=1).copy()
            ts = np.roll(self._ts_ring, -idx).copy()
        return data, ts

    def read_latest(self, n_samples: int) -> tuple[np.ndarray, np.ndarray]:
        """Return the most recent *n_samples* from the ring buffer."""
        data, ts = self.read()
        return data[:, -n_samples:], ts[-n_samples:]

    def read_latest_with_meta(
        self, n_samples: int
    ) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        """
        Return the most recent *n_samples* with per-sample metadata.

        Returns
        -------
        data : np.ndarray, shape (n_channels, n_samples)
        timestamps : np.ndarray, shape (n_samples,)  uint64
        meta : np.ndarray, shape (n_samples,)  structured array (SAMPLE_META_DTYPE)
        """
        with self._lock:
            idx = self._write_idx % self._ring.shape[1]
            data = np.roll(self._ring, -idx, axis=1).copy()
            ts = np.roll(self._ts_ring, -idx).copy()
            meta = np.roll(self._meta_ring, -idx).copy()
        return data[:, -n_samples:], ts[-n_samples:], meta[-n_samples:]

    @property
    def samples_received(self) -> int:
        return self._samples_received

    @property
    def is_connected(self) -> bool:
        return self._connected

    # ── private ────────────────────────────────────────────────────────────
    def _stream_loop(self) -> None:
        """Background thread: consume the server-streaming RPC."""
        try:
            stream_iter = self._device_stub.bicNeuralStream(
                pb.bicNeuralSetStreamingEnable(
                    deviceAddress=self._device_address,
                    enable=True,
                    bufferSize=self.cfg.grpc_buffer_size,
                    maxInterpolationPoints=self.cfg.max_interpolation_points,
                    amplificationFactor=self.cfg.amplification_factor,
                    refChannels=self.cfg.ref_channels,
                    useGroundReference=self.cfg.use_ground_reference,
                )
            )
            for update in stream_iter:
                if self._stop_event.is_set():
                    break
                self._ingest_update(update)
        except grpc.RpcError as e:
            if not self._stop_event.is_set():
                logger.error("Neural stream RPC error: %s", e)
        logger.info("Stream loop exited.")

    def _ingest_update(self, update) -> None:
        """Parse a NeuralUpdate message and push samples into the ring buffer."""
        samples = update.samples
        if not samples:
            return

        n_new = len(samples)
        ring_len = self._ring.shape[1]

        # Detect gaps (missing packets)
        first_counter = samples[0].sampleCounter
        if self._last_sample_counter > 0:
            expected = self._last_sample_counter + 1
            if first_counter != expected:
                gap = first_counter - expected
                if gap < 0:
                    gap = (2**32 - 1) - expected + first_counter  # uint32 wrap
                n_nan = min(int(gap), 10)  # cap NaN insertions at 10
                gap_meta = np.zeros(1, dtype=SAMPLE_META_DTYPE)
                with self._lock:
                    for i in range(n_nan):
                        col = self._write_idx % ring_len
                        self._ring[:, col] = np.nan
                        self._ts_ring[col] = 0
                        self._meta_ring[col] = gap_meta[0]
                        self._write_idx += 1
                logger.warning("Gap detected: expected %d, got %d (inserted %d NaNs)",
                               expected, first_counter, n_nan)

        # Build blocks from the protobuf repeated fields
        block = np.empty((self.cfg.n_channels, n_new), dtype=np.float64)
        ts_block = np.empty(n_new, dtype=np.uint64)
        meta_block = np.zeros(n_new, dtype=SAMPLE_META_DTYPE)

        for i, s in enumerate(samples):
            for ch in range(self.cfg.n_channels):
                block[ch, i] = s.measurements[ch]
            ts_block[i] = s.timeStamp
            meta_block[i]["sample_counter"]     = s.sampleCounter
            meta_block[i]["stim_active"]        = int(s.stimulationActive)
            meta_block[i]["is_interpolated"]    = int(s.isInterpolated)
            meta_block[i]["filt_sample"]        = s.filtSample
            meta_block[i]["filt_channel"]       = s.filtChannel
            meta_block[i]["phase"]              = s.phase
            meta_block[i]["trigger_phase"]      = s.triggerPhase
            meta_block[i]["pre_filt_sample"]    = s.preFiltSample
            meta_block[i]["hampel_filt_sample"] = s.hampelFiltSample
            meta_block[i]["is_valid_target"]    = int(s.isValidTarget)
            meta_block[i]["is_input_trig_high"] = int(s.isInputTrigHigh)

        # Write into ring buffer
        with self._lock:
            for i in range(n_new):
                col = self._write_idx % ring_len
                self._ring[:, col] = block[:, i]
                self._ts_ring[col] = ts_block[i]
                self._meta_ring[col] = meta_block[i]
                self._write_idx += 1
            self._samples_received += n_new

        self._last_sample_counter = samples[-1].sampleCounter

        # Fire optional callback
        if self._on_new_data is not None:
            self._on_new_data(block, ts_block)
