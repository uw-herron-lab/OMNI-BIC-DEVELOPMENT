"""
Mock BIC gRPC Server
Generates synthetic or replayed 32-channel neural data at 1 kHz for testing.

Implements the OMNI-BIC gRPC API (BICInfoService, BICBridgeService,
BICDeviceService) so downstream clients can develop and test against a
software endpoint with no implant, bridge, or acquisition hardware present.

Modes:
  Synthetic (default):
    - REST epochs:  elevated alpha + beta (idle rhythm)
    - MOVE epochs:  alpha ERD + beta suppression
    - Epochs alternate every ~3 seconds

  Replay (--replay <data-dir>):
    - Loads a prior session filterLog CSV and streams the recorded ECoG data
    - Optionally syncs timestamps to a task log for ground-truth labels
    - Loops back to the start when the data is exhausted
    - See docs/FILE_FORMAT.md for the expected filterLog / task-log columns

Usage:
  python -m src.mock_server                              # synthetic mode
  python -m src.mock_server --replay ./data/session_01   # replay a recording
  python -m src.mock_server --replay                     # browse ./data folders
  python -m src.mock_server --port 50052
"""

from __future__ import annotations

import argparse
import logging
import math
import threading
import time
from concurrent import futures

import grpc
import numpy as np

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'generated'))
import BICgRPC_pb2 as pb
import BICgRPC_pb2_grpc as pb_grpc

logging.basicConfig(level=logging.INFO, format='%(asctime)s %(name)s %(levelname)s %(message)s')
logger = logging.getLogger("MockBIC")

# ── Synthetic signal generator ────────────────────────────────────────────
N_CHANNELS = 32
SAMPLING_RATE = 1000  # Hz
EPOCH_DURATION_S = 3.0  # seconds per rest/move epoch


class SyntheticSignalGenerator:
    """Generates realistic-ish neural data with alternating rest/move states."""

    def __init__(self, n_channels=N_CHANNELS, fs=SAMPLING_RATE):
        self.n_channels = n_channels
        self.fs = fs
        self.sample_counter = 0
        self._rng = np.random.default_rng(42)
        self._epoch_samples = int(EPOCH_DURATION_S * fs)

    @property
    def current_label(self) -> str:
        """Which epoch are we in?"""
        epoch_idx = self.sample_counter // self._epoch_samples
        return "move" if epoch_idx % 2 == 1 else "rest"

    def generate_batch(self, n_samples: int) -> list[pb.NeuralSample]:
        """Generate n_samples of synthetic neural data as protobuf messages."""
        samples = []
        t0 = self.sample_counter

        for i in range(n_samples):
            t = (t0 + i) / self.fs  # time in seconds
            epoch_idx = (t0 + i) // self._epoch_samples
            is_move = (epoch_idx % 2 == 1)

            measurements = []
            for ch in range(self.n_channels):
                # Base: pink-ish noise
                val = self._rng.normal(0, 5.0)

                # Motor channels (0–7) get task-modulated oscillations
                if ch < 8:
                    if is_move:
                        # MOVE: alpha ERD (reduced), beta suppression
                        val += 5.0 * math.sin(2 * math.pi * 20 * t + ch * 0.4)   # weak beta
                        val += 6.0 * math.sin(2 * math.pi * 10 * t + ch * 0.1)   # reduced alpha
                    else:
                        # REST: strong alpha + strong beta (idle rhythm)
                        val += 18.0 * math.sin(2 * math.pi * 10 * t + ch * 0.1)  # strong alpha
                        val += 12.0 * math.sin(2 * math.pi * 12 * t + ch * 0.15) # alpha harmonic
                        val += 20.0 * math.sin(2 * math.pi * 20 * t + ch * 0.4)  # strong beta
                        val += 10.0 * math.sin(2 * math.pi * 28 * t + ch * 0.2)  # beta harmonic

                measurements.append(val)

            sample = pb.NeuralSample(
                numberOfMeasurements=self.n_channels,
                measurements=measurements,
                sampleCounter=(t0 + i) % (2**32),
                timeStamp=int(t * 1e6),  # microseconds
                isInterpolated=False,
                filtSample=0.0,
                filtChannel=0,
                supplyVoltage=3300,
                isConnected=True,
                stimulationNumber=0,
                stimulationActive=False,
                phase=0.0,
                triggerPhase=0.0,
                preFiltSample=0.0,
                hampelFiltSample=0.0,
                isValidTarget=False,
                isInputTrigHigh=False,
            )
            samples.append(sample)

        self.sample_counter += n_samples
        return samples


# ── Replay signal generator ──────────────────────────────────────────────

class ReplaySignalGenerator:
    """
    Replays real ECoG data from a prior session filterLog CSV.
    Loads CH1-CH32 into memory and streams sequentially, looping on exhaustion.
    Ground-truth labels are resolved via timestamp sync with the task log.

    See docs/FILE_FORMAT.md for the expected CSV columns.
    """

    def __init__(self, data_dir: str, task_filter: str = "STD1"):
        import pandas as pd
        from pathlib import Path

        self.n_channels = N_CHANNELS
        self.fs = SAMPLING_RATE
        self.sample_counter = 0

        data_path = Path(data_dir)

        # ── Load filterLog ────────────────────────────────────────────
        filter_files = sorted(data_path.glob("filterLog_*.csv"))
        if not filter_files:
            raise FileNotFoundError(f"No filterLog CSV in {data_path}")

        logger.info("Loading filterLog: %s (this may take ~30s)...", filter_files[0])
        ch_cols = [f"CH{i}" for i in range(1, N_CHANNELS + 1)]
        df = pd.read_csv(
            filter_files[0],
            usecols=["TimeStamp", "InputTrigger"] + ch_cols,
            low_memory=False,
        )
        self._ch_data = df[ch_cols].values.astype(np.float64)  # (n_samples, 32)
        self._n_total = len(self._ch_data)
        logger.info("  Loaded %d samples (%.1f min at %d Hz)",
                     self._n_total, self._n_total / SAMPLING_RATE / 60, SAMPLING_RATE)

        # ── Build per-sample labels via timestamp sync ────────────────
        self._labels = np.zeros(self._n_total, dtype=np.int8)  # 0=rest

        task_files = sorted(data_path.glob("*Task_Log*.csv"))
        if task_files:
            dev_timestamps = df["TimeStamp"].values.astype(np.int64)
            trig_raw = df["InputTrigger"].astype(str).str.strip().str.lower()
            input_trigger = trig_raw.map(
                {"true": 1, "false": 0, "1": 1, "0": 0}
            ).fillna(0).values.astype(np.int64)

            task_log = pd.read_csv(task_files[0])
            self._sync_and_label(dev_timestamps, input_trigger, task_log, task_filter)
        else:
            logger.warning("No task log found -- all samples labeled as rest")

        del df  # free the DataFrame

    def _sync_and_label(self, dev_ts, input_trigger, task_log, task_filter):
        """Sync timestamps and assign ground-truth labels using TriStreamSync."""
        try:
            from src.sync import TriStreamSync
            import pandas as pd

            # Build a minimal filterlog DataFrame for the sync module
            # (it needs PacketNum, TimeStamp, InputTrigger columns)
            sync_df = pd.DataFrame({
                "PacketNum": np.arange(len(dev_ts)),
                "TimeStamp": dev_ts,
                "InputTrigger": input_trigger,
            })

            sync = TriStreamSync(sync_df, task_log)
            self._labels = sync.label_samples(task_filter=task_filter)

        except Exception as e:
            logger.warning("TriStreamSync failed (%s), falling back to simple sync", e)
            self._fallback_sync_and_label(dev_ts, input_trigger, task_log, task_filter)

    def _fallback_sync_and_label(self, dev_ts, input_trigger, task_log, task_filter):
        """Simple fallback sync (histogram offset + linear regression)."""
        from scipy.stats import linregress
        DOTNET_TPS = 10_000_000
        DEBOUNCE = 500_000

        is_high = input_trigger > 0
        edges = np.where(np.diff(is_high.astype(int)) > 0)[0] + 1
        if len(edges) == 0:
            logger.warning("No trigger edges found in replay data")
            return

        edge_ts = dev_ts[edges]
        debounced = [edge_ts[0]]
        for ts in edge_ts[1:]:
            if ts - debounced[-1] > DEBOUNCE:
                debounced.append(ts)
        trigger_edges = np.array(debounced, dtype=np.int64)

        if task_filter == "all":
            starts = task_log[task_log["Marker"] == "Start"]["Timestamp"].values.astype(np.int64)
        else:
            starts = task_log[
                (task_log["Task"] == task_filter) & (task_log["Marker"] == "Start")
            ]["Timestamp"].values.astype(np.int64)

        if len(trigger_edges) == 0 or len(starts) == 0:
            return

        diffs = np.subtract.outer(starts, trigger_edges).ravel()
        bin_width = DOTNET_TPS // 10
        n_bins = max(1, min(int((diffs.max() - diffs.min()) / bin_width), 10000))
        counts, bin_edges = np.histogram(diffs, bins=n_bins)
        offset_est = (bin_edges[np.argmax(counts)] + bin_edges[np.argmax(counts) + 1]) / 2

        matched_dev, matched_dot = [], []
        for te in trigger_edges:
            dists = np.abs(starts - (te + offset_est))
            best = np.argmin(dists)
            if dists[best] < DOTNET_TPS * 2:
                matched_dev.append(te)
                matched_dot.append(starts[best])

        if len(matched_dev) >= 2:
            result = linregress(np.array(matched_dev, dtype=np.float64),
                                np.array(matched_dot, dtype=np.float64))
            slope, intercept = result.slope, result.intercept
            logger.info("  Fallback sync: slope=%.6f, R²=%.6f", slope, result.rvalue**2)
        else:
            slope, intercept = 1.0, offset_est

        dotnet_ts = slope * dev_ts.astype(np.float64) + intercept

        if task_filter == "all":
            task_events = task_log.sort_values("Timestamp")
        else:
            task_events = task_log[task_log["Task"] == task_filter].sort_values("Timestamp")

        for task_name in task_events["Task"].unique():
            te = task_events[task_events["Task"] == task_name]
            t_starts = te[te["Marker"] == "Start"]["Timestamp"].values.astype(np.int64)
            t_stops = te[te["Marker"] == "Stop"]["Timestamp"].values.astype(np.int64)
            for i in range(min(len(t_starts), len(t_stops))):
                mask = (dotnet_ts >= t_starts[i]) & (dotnet_ts <= t_stops[i])
                self._labels[mask] = 1

        move_count = int(self._labels.sum())
        logger.info("  Labels: rest=%d, move=%d (%.1f%% move)",
                     self._n_total - move_count, move_count,
                     100 * move_count / self._n_total)

    @property
    def current_label(self) -> str:
        idx = self.sample_counter % self._n_total
        return "move" if self._labels[idx] == 1 else "rest"

    def generate_batch(self, n_samples: int) -> list:
        """Replay n_samples from the loaded data as protobuf messages."""
        samples = []
        t0 = self.sample_counter

        for i in range(n_samples):
            idx = (t0 + i) % self._n_total
            t = (t0 + i) / self.fs

            measurements = self._ch_data[idx].tolist()

            sample = pb.NeuralSample(
                numberOfMeasurements=self.n_channels,
                measurements=measurements,
                sampleCounter=(t0 + i) % (2**32),
                timeStamp=int(t * 1e6),
                isInterpolated=False,
                filtSample=0.0,
                filtChannel=0,
                supplyVoltage=3300,
                isConnected=True,
                stimulationNumber=0,
                stimulationActive=False,
                phase=0.0,
                triggerPhase=0.0,
                preFiltSample=0.0,
                hampelFiltSample=0.0,
                isValidTarget=False,
                isInputTrigHigh=bool(self._labels[idx]),
            )
            samples.append(sample)

        self.sample_counter += n_samples
        return samples


# ── gRPC Service Implementations ─────────────────────────────────────────

class MockInfoService(pb_grpc.BICInfoServiceServicer):
    def VersionNumber(self, request, context):
        return pb.VersionNumberResponse(version_number="MockBIC v1.0.0")

    def SupportedDevices(self, request, context):
        return pb.SupportedDevicesResponse(supported_devices=["MockImplant-32ch"])

    def InspectRepository(self, request, context):
        return pb.InspectRepositoryResponse(repo_uri=["mock://localhost"])


class MockBridgeService(pb_grpc.BICBridgeServiceServicer):
    def ScanBridges(self, request, context):
        bridge = pb.Bridge(
            name="//mock/bridge/SN0001",
            deviceType="MockBridge",
            deviceId="MOCK-001",
            firmwareVersion="1.0.0",
        )
        return pb.QueryBridgesResponse(bridges=[bridge])

    def ConnectedBridges(self, request, context):
        return pb.QueryBridgesResponse(bridges=[])

    def ListBridges(self, request, context):
        return self.ScanBridges(request, context)

    def ConnectBridge(self, request, context):
        logger.info("Bridge connected: %s", request.name)
        return pb.ConnectBridgeResponse(
            name=request.name,
            connection_status=pb.CONNECTION_SUCCESS,
        )

    def DescribeBridge(self, request, context):
        return pb.DescribeBridgeResponse(name=request.name)

    def DisconnectBridge(self, request, context):
        logger.info("Bridge disconnected: %s", request.name)
        from google.protobuf.empty_pb2 import Empty
        return Empty()


class MockDeviceService(pb_grpc.BICDeviceServiceServicer):
    def __init__(self, generator=None):
        self._generator = generator or SyntheticSignalGenerator()
        self._streaming = False

    def ScanDevices(self, request, context):
        info = pb.bicGetImplantInfoReply(
            firmwareVersion="1.0.0",
            deviceType="MockImplant",
            deviceId="MOCK-IMPLANT-001",
            channelCount=N_CHANNELS,
            measurementChannelCount=N_CHANNELS,
            stimulationChannelCount=0,
            samplingRate=SAMPLING_RATE,
        )
        return pb.ScanDevicesReply(
            name="//mock/device/IMPLANT001",
            discoveredDevice=info,
        )

    def ConnectDevice(self, request, context):
        logger.info("Device connected: %s", request.deviceAddress)
        return pb.bicSuccessReply()

    def bicDispose(self, request, context):
        logger.info("Device disposed: %s", request.deviceAddress)
        self._streaming = False
        return pb.bicSuccessReply()

    def bicNeuralStream(self, request, context):
        """Server-streaming RPC: yield NeuralUpdate messages at ~real-time rate."""
        if not request.enable:
            self._streaming = False
            logger.info("Neural stream DISABLED")
            return

        self._streaming = True
        buffer_size = request.bufferSize if request.bufferSize > 0 else 100
        interval = buffer_size / SAMPLING_RATE  # seconds between messages

        logger.info("Neural stream STARTED (buffer_size=%d, interval=%.3fs)",
                     buffer_size, interval)
        logger.info("  Amplification=%s, RefChannels=%s, GroundRef=%s",
                     request.amplificationFactor, list(request.refChannels),
                     request.useGroundReference)

        epoch_label = ""
        while self._streaming and context.is_active():
            samples = self._generator.generate_batch(buffer_size)
            update = pb.NeuralUpdate(samples=samples)
            yield update

            # Log epoch transitions
            new_label = self._generator.current_label
            if new_label != epoch_label:
                epoch_label = new_label
                logger.info("  ▶ Epoch: %s  (sample %d)",
                            epoch_label.upper(), self._generator.sample_counter)

            time.sleep(interval)

        logger.info("Neural stream STOPPED (total samples: %d)",
                     self._generator.sample_counter)

    def bicGetImplantInfo(self, request, context):
        return pb.bicGetImplantInfoReply(
            channelCount=N_CHANNELS,
            measurementChannelCount=N_CHANNELS,
            samplingRate=SAMPLING_RATE,
        )

    def bicGetTemperature(self, request, context):
        return pb.bicGetTemperatureReply(temperature=37.0, units="C")

    def bicGetHumidity(self, request, context):
        return pb.bicGetHumidityReply(humidity=45.0, units="%")

    def bicConnectionStream(self, request, context):
        """Yield a single 'connected' message then keep alive."""
        yield pb.ConnectionUpdate(connectionType="device", isConnected=True)
        while context.is_active() and request.enable:
            time.sleep(5.0)

    def bicTemperatureStream(self, request, context):
        while context.is_active() and request.enable:
            yield pb.TemperatureUpdate(temperature=37.0, units="C")
            time.sleep(5.0)

    def bicHumidityStream(self, request, context):
        while context.is_active() and request.enable:
            yield pb.HumidityUpdate(humidity=45.0, units="%")
            time.sleep(5.0)

    def bicPowerStream(self, request, context):
        while context.is_active() and request.enable:
            yield pb.PowerUpdate(parameter="voltage", value=3.3, units="V")
            time.sleep(5.0)

    def bicErrorStream(self, request, context):
        # No errors in mock
        while context.is_active() and request.enable:
            time.sleep(10.0)

    # Stim functions (no-ops for mock)
    def bicStartStimulation(self, request, context):
        return pb.bicSuccessReply()

    def bicEnqueueStimulation(self, request, context):
        return pb.bicSuccessReply()

    def bicStopStimulation(self, request, context):
        return pb.bicSuccessReply()

    def enableOpenLoopStimulation(self, request, context):
        return pb.bicSuccessReply()

    def enableDistributedStimulation(self, request, context):
        return pb.bicSuccessReply()

    def bicSetImplantPower(self, request, context):
        return pb.bicSuccessReply()

    def bicGetImpedance(self, request, context):
        return pb.bicGetImpedanceReply(channelImpedance=1000.0, units="Ohm", success="success")

    def bicGetIsStimulating(self, request, context):
        return pb.bicGetIsStimulatingReply(isStimulating=False, isTriggeringStim=False)


# ── Server entry point ────────────────────────────────────────────────────

def serve(port: int = 50051, replay_dir: str | None = None, task: str = "STD1"):
    # Build the signal generator
    if replay_dir:
        generator = ReplaySignalGenerator(replay_dir, task_filter=task)
        mode_desc = f"REPLAY from {replay_dir}"
    else:
        generator = SyntheticSignalGenerator()
        mode_desc = "SYNTHETIC (REST/MOVE epochs, 3s each)"

    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    pb_grpc.add_BICInfoServiceServicer_to_server(MockInfoService(), server)
    pb_grpc.add_BICBridgeServiceServicer_to_server(MockBridgeService(), server)
    pb_grpc.add_BICDeviceServiceServicer_to_server(MockDeviceService(generator), server)

    addr = f"0.0.0.0:{port}"
    server.add_insecure_port(addr)
    server.start()
    logger.info("=" * 55)
    logger.info("  Mock BIC gRPC Server running on %s", addr)
    logger.info("  32 channels @ 1 kHz | %s", mode_desc)
    logger.info("=" * 55)

    try:
        server.wait_for_termination()
    except KeyboardInterrupt:
        logger.info("Shutting down...")
        server.stop(grace=2)


def _find_data_folders():
    """Scan for directories containing filterLog CSVs.

    Looks under the ``data/`` and ``session_data/`` folders next to the
    repository root.  Point ``--replay`` at any folder holding a
    ``filterLog_*.csv`` (see docs/FILE_FORMAT.md).
    """
    from pathlib import Path
    repo_root = Path(__file__).resolve().parent.parent
    candidates = []

    # Check data/ subdirectories
    data_dir = repo_root / "data"
    if data_dir.is_dir():
        for sub in sorted(data_dir.iterdir()):
            if sub.is_dir() and list(sub.glob("filterLog_*.csv")):
                candidates.append(sub)

    # Check session_data/ subdirectories (from prior sessions)
    session_dir = repo_root / "session_data"
    if session_dir.is_dir():
        for sub in sorted(session_dir.iterdir()):
            if sub.is_dir():
                logs = sub / "logs"
                if logs.is_dir() and list(logs.glob("filterLog_*.csv")):
                    candidates.append(logs)

    return candidates


def _interactive_replay_picker() -> tuple[str, str]:
    """Prompt user to select a data folder and task for replay mode."""
    folders = _find_data_folders()

    print()
    print("=" * 55)
    print("  Select data folder for REPLAY mode")
    print("=" * 55)
    print()

    if folders:
        for i, f in enumerate(folders, 1):
            # Count filterLog samples for context
            flogs = list(f.glob("filterLog_*.csv"))
            task_logs = list(f.glob("*Task_Log*.csv"))
            desc = f.name
            if f.parent.name not in ("data", "logs"):
                desc = f"{f.parent.name}/{f.name}"
            tasks_str = ""
            if task_logs:
                import csv as _csv
                try:
                    with open(task_logs[0], encoding="utf-8") as tf:
                        reader = _csv.DictReader(tf)
                        tasks_found = set()
                        for row in reader:
                            if "Task" in row:
                                tasks_found.add(row["Task"])
                        tasks_str = f"  tasks: {', '.join(sorted(tasks_found))}"
                except Exception:
                    pass
            print(f"  [{i}] {desc}")
            print(f"      {f}")
            print(f"      {len(flogs)} filterLog(s){tasks_str}")
            print()
    else:
        print("  No data folders found with filterLog CSVs.")
        print("  (Drop a recording under ./data/<session>/ or enter a path below.)")
        print()

    n_options = len(folders)
    print(f"  [{n_options + 1}] Enter a custom path")
    print(f"  [{n_options + 2}] Use SYNTHETIC data instead (no replay)")
    print()

    while True:
        try:
            choice = input("  Select [1-%d]: " % (n_options + 2)).strip()
            idx = int(choice)
        except (ValueError, EOFError):
            continue

        if 1 <= idx <= n_options:
            selected = folders[idx - 1]
            break
        elif idx == n_options + 1:
            custom = input("  Enter path: ").strip().strip('"')
            from pathlib import Path as _P
            if _P(custom).is_dir():
                selected = _P(custom)
                break
            else:
                print(f"  Directory not found: {custom}")
                continue
        elif idx == n_options + 2:
            return None, "STD1"  # synthetic mode
        else:
            continue

    # Ask for task code
    task = input("  Task code for label sync [STD1]: ").strip() or "STD1"
    return str(selected), task


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Mock BIC gRPC Server")
    parser.add_argument("--port", type=int, default=50051, help="Port (default: 50051)")
    parser.add_argument("--replay", metavar="DIR", nargs="?", const="__browse__", default=None,
                        help="Replay real session data. Omit DIR to browse available folders.")
    parser.add_argument("--task", default="STD1",
                        help="Task code for label sync in replay mode (default: STD1)")
    args = parser.parse_args()

    replay_dir = args.replay
    task = args.task

    # Interactive folder picker if --replay given without a path
    if replay_dir == "__browse__":
        replay_dir, task = _interactive_replay_picker()

    serve(args.port, replay_dir=replay_dir, task=task)
