"""
Tri-Stream Synchronization Module

Aligns three data streams recorded during BCI sessions:
  1. FilterLog (Cortec ECoG) — device tick timestamps, 1 kHz
  2. Task Log — .NET DateTime tick timestamps (PC clock)
  3. EMG data — .NET DateTime tick timestamps, 2 kHz

The FilterLog uses the implanted device's clock.  Task Log and EMG use the
PC's .NET clock.  The ``InputTrigger`` column in the FilterLog is a TTL pulse
that bridges both clock domains.

Synchronization uses per-segment cross-correlation of the trigger signal
for sub-sample precision, with a global linear fit as the coarse estimate.

Usage
-----
    from src.sync import TriStreamSync

    sync = TriStreamSync(filterlog_df, task_log_df)
    labels = sync.label_samples(task_filter="all")
    sync.export_aligned("aligned_trials.parquet")

    # With EMG
    sync = TriStreamSync(filterlog_df, task_log_df, emg_df=emg_df)
    trials = sync.get_trial_segments()
"""

from __future__ import annotations

import logging
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

import numpy as np
import pandas as pd
from scipy import signal as scipy_signal
from scipy.stats import linregress

logger = logging.getLogger(__name__)

# ── Constants ─────────────────────────────────────────────────────────────
FS_CORTEC = 1000                       # ECoG sampling rate (Hz)
FS_EMG = 2000                          # EMG sampling rate (Hz)
N_CHANNELS = 32
DOTNET_TICKS_PER_SEC = 10_000_000      # 1 tick = 100 ns
TRIGGER_DEBOUNCE_TICKS = 500_000       # ~500ms in device ticks


# ═══════════════════════════════════════════════════════════════════════════
#  1. Trigger Signal Extraction
# ═══════════════════════════════════════════════════════════════════════════

def extract_cortec_trigger(df: pd.DataFrame) -> tuple[np.ndarray, np.ndarray]:
    """
    Extract and interpolate the binary trigger signal from a FilterLog DataFrame.

    Handles dropped packets by interpolating between known PacketNum values.

    Returns
    -------
    samples : (n_interp,) int array — interpolated sample indices (0-based)
    trigger : (n_interp,) float array — binary trigger signal (0.0 or 1.0)
    """
    packets = df["PacketNum"].values
    raw_trigger = df["InputTrigger"]

    # Normalize True/False/1/0 text to int — always go through string path
    # since the column may be string dtype even if pandas doesn't report object
    trigger_int = (
        raw_trigger.astype(str).str.strip().str.lower()
        .map({"true": 1, "false": 0, "1": 1, "0": 0, "1.0": 1, "0.0": 0})
        .fillna(0).values.astype(np.float64)
    )

    # Interpolate for dropped packets (gaps in PacketNum)
    pkt_0 = packets[0]
    interp_samples = np.arange(0, packets[-1] - pkt_0 + 1)
    interp_trigger = np.interp(interp_samples, packets - pkt_0, trigger_int)

    # Threshold back to binary
    interp_trigger = (interp_trigger > 0.5).astype(np.float64)

    n_dropped = len(interp_samples) - len(packets)
    if n_dropped > 0:
        logger.info("  Cortec trigger: %d samples (%d interpolated for dropped packets)",
                     len(interp_samples), n_dropped)
    else:
        logger.info("  Cortec trigger: %d samples (no drops)", len(interp_samples))

    return interp_samples, interp_trigger


def extract_emg_trigger(df: pd.DataFrame, sync_channel: int = 14) -> np.ndarray:
    """
    Reconstruct binary trigger signal from an EMG differential channel.

    The EMG system records the trigger as a differential signal on one channel.
    We detect rising/falling edge pairs and reconstruct a binary pulse train.

    Parameters
    ----------
    df : EMG DataFrame with columns like EMG1, EMG2, ...
    sync_channel : 1-indexed EMG channel carrying the trigger signal

    Returns
    -------
    trigger : (n_samples,) float array — binary trigger signal at EMG sample rate
    """
    col = f"EMG{sync_channel}"
    if col not in df.columns:
        # Try 0-indexed
        col = df.columns[sync_channel] if sync_channel < len(df.columns) else None
        if col is None:
            raise ValueError(f"Cannot find sync channel {sync_channel} in EMG data")

    emg_sig = df[col].to_numpy(dtype=np.float64)
    n = len(emg_sig)

    # Threshold from middle third of signal (avoids edge artifacts)
    seg_start = n // 3
    seg_end = 2 * n // 3
    seg = emg_sig[seg_start:seg_end]
    thresh = 0.6 * np.nanmax(np.abs(seg))

    if thresh < 1e-10:
        logger.warning("  EMG trigger: sync channel %s appears to be flat (thresh=%.2e)", col, thresh)
        return np.zeros(n, dtype=np.float64)

    # Find positive peaks (falling edges of diff signal) and negative peaks (rising edges)
    pos_peaks = scipy_signal.argrelextrema(emg_sig, np.greater)[0]
    neg_peaks = scipy_signal.argrelextrema(emg_sig, np.less)[0]

    fall_edges = pos_peaks[emg_sig[pos_peaks] > thresh]
    rise_edges = neg_peaks[emg_sig[neg_peaks] < -thresh]

    # Match rising→falling edge pairs
    trigger = np.zeros(n, dtype=np.float64)
    max_pairs = min(len(rise_edges), len(fall_edges))

    for i in range(max_pairs):
        rise = rise_edges[i]
        # Find the closest falling edge after this rising edge
        dists = fall_edges - rise
        candidates = np.where((dists > 2) & (dists < 100))[0]  # 1-50ms at 2kHz
        if len(candidates) >= 1:
            fall = fall_edges[candidates[0]]
            trigger[rise:fall + 1] = 1.0

    n_pulses = int(np.sum(np.diff(trigger) > 0))
    logger.info("  EMG trigger: %d samples, %d pulses detected", n, n_pulses)
    return trigger


# ═══════════════════════════════════════════════════════════════════════════
#  2. Cross-Correlation Alignment
# ═══════════════════════════════════════════════════════════════════════════

def cross_correlate_offset(
    sig_ref: np.ndarray,
    sig_target: np.ndarray,
    fs_ref: float = 1000,
    fs_target: float = 1000,
) -> tuple[int, float]:
    """
    Find the sample offset that best aligns sig_target to sig_ref
    using normalized cross-correlation.

    If sampling rates differ, sig_target is resampled to fs_ref.

    Returns
    -------
    offset : int — samples to shift sig_target to align with sig_ref
                   (positive = target is delayed relative to ref)
    quality : float — peak normalized cross-correlation value (0-1)
    """
    # Resample target to reference rate if needed
    if fs_target != fs_ref:
        n_resamp = int(len(sig_target) * fs_ref / fs_target)
        sig_target = scipy_signal.resample(sig_target, n_resamp)

    # Normalized cross-correlation
    corr = scipy_signal.correlate(sig_ref, sig_target, mode="full")
    norm = np.sqrt(np.sum(sig_ref ** 2) * np.sum(sig_target ** 2))
    if norm > 0:
        corr_norm = corr / norm
    else:
        return 0, 0.0

    peak_idx = np.argmax(corr_norm)
    offset = peak_idx - (len(sig_target) - 1)
    quality = float(corr_norm[peak_idx])

    return int(offset), quality


def find_trigger_edges(trigger: np.ndarray, debounce: int = 500) -> np.ndarray:
    """
    Find rising-edge sample indices in a binary trigger signal, debounced.

    Parameters
    ----------
    trigger : binary signal (0/1)
    debounce : minimum samples between edges

    Returns
    -------
    edges : array of sample indices at each rising edge
    """
    is_high = trigger > 0.5
    edges = np.where(np.diff(is_high.astype(int)) > 0)[0] + 1

    if len(edges) == 0:
        return np.array([], dtype=np.int64)

    # Debounce
    debounced = [edges[0]]
    for e in edges[1:]:
        if e - debounced[-1] > debounce:
            debounced.append(e)

    return np.array(debounced, dtype=np.int64)


# ═══════════════════════════════════════════════════════════════════════════
#  3. LinearTransform — maps between clock domains
# ═══════════════════════════════════════════════════════════════════════════

@dataclass
class LinearTransform:
    """Affine mapping: target = slope * source + intercept."""
    slope: float = 1.0
    intercept: float = 0.0
    r_squared: float = 0.0
    n_matched: int = 0

    def to_target(self, source: np.ndarray | float) -> np.ndarray | float:
        return self.slope * source + self.intercept

    def to_source(self, target: np.ndarray | float) -> np.ndarray | float:
        if abs(self.slope) < 1e-15:
            raise ValueError("Cannot invert: slope is zero")
        return (target - self.intercept) / self.slope


# ═══════════════════════════════════════════════════════════════════════════
#  4. Cortec ↔ .NET Alignment
# ═══════════════════════════════════════════════════════════════════════════

def align_cortec_to_dotnet(
    filterlog_df: pd.DataFrame,
    task_log_df: pd.DataFrame,
    task_filter: str = "all",
    window_sec: float = 5.0,
) -> tuple[LinearTransform, list[dict]]:
    """
    Align FilterLog device ticks to .NET DateTime ticks using the trigger signal.

    1. Global coarse alignment via histogram of trigger-edge↔Start-marker offsets
    2. Per-segment cross-correlation refinement for each matched trial
    3. Linear fit across all refined per-segment offsets

    Parameters
    ----------
    filterlog_df : FilterLog DataFrame
    task_log_df : Task Log DataFrame
    task_filter : "all" or a specific task code
    window_sec : seconds of context around each trial for cross-correlation

    Returns
    -------
    transform : LinearTransform (device_tick → dotnet_tick)
    segments : list of dicts with per-segment alignment details
    """
    # --- Extract timestamps and trigger ---
    device_ts = filterlog_df["TimeStamp"].values.astype(np.int64)
    _, cortec_trigger = extract_cortec_trigger(filterlog_df)

    # Find trigger edges in device-tick domain
    cortec_edges_samples = find_trigger_edges(cortec_trigger, debounce=500)
    # Convert sample indices to device ticks
    pkt0 = filterlog_df["PacketNum"].values[0]
    # Map: interp sample index → approximate device tick
    # device_ts are at the original (non-interpolated) packet positions
    # Use linear interp from PacketNum→TimeStamp
    pkt_nums = filterlog_df["PacketNum"].values
    cortec_edges_ticks = np.interp(
        cortec_edges_samples + pkt0, pkt_nums, device_ts
    ).astype(np.int64)

    logger.info("  Cortec: %d trigger edges (debounced)", len(cortec_edges_ticks))

    # --- Get Start markers ---
    if task_filter == "all":
        starts_df = task_log_df[task_log_df["Marker"] == "Start"].sort_values("Timestamp")
    else:
        starts_df = task_log_df[
            (task_log_df["Task"] == task_filter) & (task_log_df["Marker"] == "Start")
        ].sort_values("Timestamp")

    start_ticks = starts_df["Timestamp"].values.astype(np.int64)
    logger.info("  TaskLog: %d Start markers (task=%s)", len(start_ticks), task_filter)

    if len(cortec_edges_ticks) == 0 or len(start_ticks) == 0:
        raise ValueError("No trigger edges or Start markers — cannot sync")

    # --- Step 1: Coarse alignment via histogram ---
    diffs = np.subtract.outer(start_ticks, cortec_edges_ticks).ravel()
    bin_width = DOTNET_TICKS_PER_SEC // 10  # 0.1s bins
    n_bins = max(1, min(10000, int((diffs.max() - diffs.min()) / bin_width)))
    counts, bin_edges = np.histogram(diffs, bins=n_bins)
    peak_bin = np.argmax(counts)
    coarse_offset = (bin_edges[peak_bin] + bin_edges[peak_bin + 1]) / 2
    logger.info("  Coarse offset: %.6e (peak count=%d)", coarse_offset, counts[peak_bin])

    # --- Step 2: Match edges to start markers ---
    tolerance = DOTNET_TICKS_PER_SEC * 2  # 2 sec
    matched = []

    for edge_tick in cortec_edges_ticks:
        expected_dotnet = edge_tick + coarse_offset
        dists = np.abs(start_ticks - expected_dotnet)
        best_idx = np.argmin(dists)
        if dists[best_idx] < tolerance:
            matched.append({
                "device_tick": edge_tick,
                "dotnet_tick": start_ticks[best_idx],
                "task": starts_df.iloc[best_idx].get("Task", ""),
                "trial": starts_df.iloc[best_idx].get("Trial", 0),
            })

    logger.info("  Matched %d trigger-start pairs", len(matched))

    if len(matched) < 2:
        logger.warning("  Too few matches — using coarse offset only")
        transform = LinearTransform(slope=1.0, intercept=coarse_offset, n_matched=len(matched))
        return transform, matched

    # --- Step 3: Per-segment cross-correlation refinement ---
    window_samples = int(window_sec * FS_CORTEC)
    segments = []

    for m in matched:
        # Find the cortec sample index nearest this device tick
        cortec_sample = int(np.searchsorted(device_ts, m["device_tick"]))

        # Extract a window of cortec trigger around this point
        seg_start = max(0, cortec_sample - window_samples)
        seg_end = min(len(cortec_trigger), cortec_sample + window_samples)

        if seg_end - seg_start < window_samples // 2:
            continue

        cortec_seg = cortec_trigger[seg_start:seg_end]

        # Create the expected trigger pattern: a single pulse at the match point
        # Use the actual cortec trigger segment and cross-correlate against
        # a reference pulse centered at the coarse-estimated position
        # The offset refines the coarse match
        expected_pos = cortec_sample - seg_start
        ref_pulse = np.zeros_like(cortec_seg)
        pulse_half = min(50, expected_pos, len(ref_pulse) - expected_pos - 1)
        if pulse_half > 0:
            ref_pulse[expected_pos - pulse_half:expected_pos + pulse_half] = 1.0

        if np.sum(cortec_seg) > 0 and np.sum(ref_pulse) > 0:
            offset, quality = cross_correlate_offset(cortec_seg, ref_pulse)
        else:
            offset, quality = 0, 0.0

        # Refined device tick for this edge
        refined_device_tick = m["device_tick"] + offset
        segments.append({
            **m,
            "refined_device_tick": refined_device_tick,
            "xcorr_offset": offset,
            "xcorr_quality": quality,
            "cortec_sample": cortec_sample,
        })

    if len(segments) < 2:
        logger.warning("  Too few refined segments — using coarse fit")
        transform = LinearTransform(slope=1.0, intercept=coarse_offset, n_matched=len(matched))
        return transform, segments

    # --- Step 4: Global linear fit from refined pairs ---
    dev_arr = np.array([s["refined_device_tick"] for s in segments], dtype=np.float64)
    dot_arr = np.array([s["dotnet_tick"] for s in segments], dtype=np.float64)

    result = linregress(dev_arr, dot_arr)
    transform = LinearTransform(
        slope=result.slope,
        intercept=result.intercept,
        r_squared=result.rvalue ** 2,
        n_matched=len(segments),
    )
    logger.info("  Linear fit: slope=%.8f, R²=%.6f, n=%d",
                transform.slope, transform.r_squared, transform.n_matched)

    return transform, segments


# ═══════════════════════════════════════════════════════════════════════════
#  5. EMG ↔ Cortec Alignment
# ═══════════════════════════════════════════════════════════════════════════

def align_emg_to_cortec(
    emg_df: pd.DataFrame,
    filterlog_df: pd.DataFrame,
    sync_channel: int = 14,
) -> tuple[LinearTransform, float]:
    """
    Align EMG samples to Cortec samples via trigger cross-correlation.

    EMG is at 2 kHz, Cortec at 1 kHz.  The EMG trigger signal is reconstructed
    from a differential channel, downsampled to 1 kHz, and cross-correlated
    against the Cortec InputTrigger.

    Returns
    -------
    transform : LinearTransform (EMG sample → Cortec sample)
    quality : cross-correlation peak quality
    """
    _, cortec_trigger = extract_cortec_trigger(filterlog_df)
    emg_trigger = extract_emg_trigger(emg_df, sync_channel)

    # Downsample EMG trigger to Cortec rate (2kHz → 1kHz)
    emg_ds = emg_trigger[::2]

    offset, quality = cross_correlate_offset(cortec_trigger, emg_ds)

    # Transform: emg_sample_idx → cortec_sample_idx
    # EMG sample i at 2kHz = cortec sample (i/2 + offset) at 1kHz
    transform = LinearTransform(
        slope=0.5,  # EMG samples to Cortec samples (2:1 rate ratio)
        intercept=float(offset),
        r_squared=quality,
        n_matched=1,
    )

    logger.info("  EMG→Cortec: offset=%d samples, quality=%.3f", offset, quality)
    return transform, quality


# ═══════════════════════════════════════════════════════════════════════════
#  6. TrialSegment and TriStreamSync
# ═══════════════════════════════════════════════════════════════════════════

@dataclass
class TrialSegment:
    """Aligned index ranges for one trial across all streams."""
    task: str
    trial: int
    dotnet_start: int
    dotnet_end: int
    cortec_start: int
    cortec_end: int
    emg_start: int | None = None
    emg_end: int | None = None
    sync_quality: float = 0.0


class TriStreamSync:
    """
    Unified interface for tri-stream synchronization.

    Parameters
    ----------
    filterlog_df : FilterLog DataFrame
    task_log_df : Task Log DataFrame
    emg_df : optional EMG DataFrame
    emg_sync_channel : EMG channel carrying the trigger (1-indexed, default 14)
    """

    def __init__(
        self,
        filterlog_df: pd.DataFrame,
        task_log_df: pd.DataFrame,
        emg_df: pd.DataFrame | None = None,
        emg_sync_channel: int = 14,
    ):
        self._filterlog = filterlog_df
        self._task_log = task_log_df
        self._emg = emg_df
        self._device_ts = filterlog_df["TimeStamp"].values.astype(np.int64)

        # Cortec ↔ .NET alignment
        logger.info("Aligning Cortec → .NET DateTime ticks...")
        self.cortec_transform, self._segments = align_cortec_to_dotnet(
            filterlog_df, task_log_df, task_filter="all"
        )
        logger.info("  Result: slope=%.8f, intercept=%.6e, R²=%.6f",
                     self.cortec_transform.slope,
                     self.cortec_transform.intercept,
                     self.cortec_transform.r_squared)

        # EMG ↔ Cortec alignment (optional)
        self.emg_transform: LinearTransform | None = None
        self.emg_sync_quality = 0.0
        if emg_df is not None:
            logger.info("Aligning EMG → Cortec...")
            self.emg_transform, self.emg_sync_quality = align_emg_to_cortec(
                emg_df, filterlog_df, emg_sync_channel
            )

    def device_to_dotnet(self, device_ticks: np.ndarray) -> np.ndarray:
        """Convert device ticks to .NET DateTime ticks."""
        return self.cortec_transform.to_target(device_ticks.astype(np.float64))

    def dotnet_to_device(self, dotnet_ticks: np.ndarray) -> np.ndarray:
        """Convert .NET DateTime ticks to device ticks."""
        return self.cortec_transform.to_source(dotnet_ticks.astype(np.float64))

    def get_cortec_indices(self, dotnet_start: int, dotnet_end: int) -> tuple[int, int]:
        """Return FilterLog row indices for a .NET tick time range."""
        dev_start = self.cortec_transform.to_source(float(dotnet_start))
        dev_end = self.cortec_transform.to_source(float(dotnet_end))
        idx_start = int(np.searchsorted(self._device_ts, dev_start))
        idx_end = int(np.searchsorted(self._device_ts, dev_end))
        return idx_start, idx_end

    def get_emg_indices(self, dotnet_start: int, dotnet_end: int) -> tuple[int, int] | None:
        """Return EMG row indices for a .NET tick time range."""
        if self._emg is None or self.emg_transform is None:
            return None
        cortec_start, cortec_end = self.get_cortec_indices(dotnet_start, dotnet_end)
        emg_start = int(self.emg_transform.to_source(float(cortec_start)))
        emg_end = int(self.emg_transform.to_source(float(cortec_end)))
        emg_start = max(0, min(emg_start, len(self._emg) - 1))
        emg_end = max(0, min(emg_end, len(self._emg)))
        return emg_start, emg_end

    def label_samples(self, task_filter: str = "all") -> np.ndarray:
        """
        Label each FilterLog sample as 0 (rest) or 1 (movement).

        Uses the refined Cortec↔.NET alignment to map Task Log Start/Stop
        intervals onto FilterLog rows.
        """
        n = len(self._filterlog)
        labels = np.zeros(n, dtype=np.int32)

        if task_filter == "all":
            events = self._task_log
        else:
            events = self._task_log[self._task_log["Task"] == task_filter]

        tasks = events["Task"].unique()
        total_move = 0

        for task_name in tasks:
            te = events[events["Task"] == task_name].sort_values("Timestamp")
            starts = te[te["Marker"] == "Start"]["Timestamp"].values.astype(np.int64)
            stops = te[te["Marker"] == "Stop"]["Timestamp"].values.astype(np.int64)
            n_intervals = min(len(starts), len(stops))

            task_move = 0
            for i in range(n_intervals):
                idx_start, idx_end = self.get_cortec_indices(starts[i], stops[i])
                idx_start = max(0, min(idx_start, n))
                idx_end = max(0, min(idx_end, n))
                labels[idx_start:idx_end] = 1
                task_move += idx_end - idx_start

            total_move += task_move
            logger.info("  %s: %d intervals, %d movement samples", task_name, n_intervals, task_move)

        logger.info("  Total: rest=%d, move=%d (%.1f%%)",
                     n - total_move, total_move,
                     100 * total_move / n if n > 0 else 0)
        return labels

    def get_trial_segments(self, task_filter: str = "all") -> list[TrialSegment]:
        """
        Return aligned index ranges for every trial across all streams.
        """
        if task_filter == "all":
            events = self._task_log
        else:
            events = self._task_log[self._task_log["Task"] == task_filter]

        trials = []
        tasks = events["Task"].unique()

        for task_name in tasks:
            te = events[events["Task"] == task_name].sort_values("Timestamp")
            starts = te[te["Marker"] == "Start"]
            stops = te[te["Marker"] == "Stop"]

            start_rows = starts.reset_index(drop=True)
            stop_rows = stops.reset_index(drop=True)
            n_intervals = min(len(start_rows), len(stop_rows))

            for i in range(n_intervals):
                dotnet_start = int(start_rows.iloc[i]["Timestamp"])
                dotnet_end = int(stop_rows.iloc[i]["Timestamp"])
                trial_num = int(start_rows.iloc[i].get("Trial", i + 1))

                cortec_start, cortec_end = self.get_cortec_indices(dotnet_start, dotnet_end)
                emg_indices = self.get_emg_indices(dotnet_start, dotnet_end)

                # Find sync quality from segments if available
                quality = 0.0
                for seg in self._segments:
                    if seg.get("task") == task_name and seg.get("trial") == trial_num:
                        quality = seg.get("xcorr_quality", 0.0)
                        break

                seg = TrialSegment(
                    task=task_name,
                    trial=trial_num,
                    dotnet_start=dotnet_start,
                    dotnet_end=dotnet_end,
                    cortec_start=cortec_start,
                    cortec_end=cortec_end,
                    emg_start=emg_indices[0] if emg_indices else None,
                    emg_end=emg_indices[1] if emg_indices else None,
                    sync_quality=quality,
                )
                trials.append(seg)

        logger.info("  %d trial segments across %d tasks", len(trials), len(tasks))
        return trials

    def export_aligned(self, output_path: str | Path) -> pd.DataFrame:
        """
        Export aligned trial segments to a Parquet file.

        Returns the DataFrame for immediate use.
        """
        trials = self.get_trial_segments()

        rows = []
        for t in trials:
            rows.append({
                "task": t.task,
                "trial": t.trial,
                "dotnet_start": t.dotnet_start,
                "dotnet_end": t.dotnet_end,
                "cortec_start_idx": t.cortec_start,
                "cortec_end_idx": t.cortec_end,
                "emg_start_idx": t.emg_start if t.emg_start is not None else -1,
                "emg_end_idx": t.emg_end if t.emg_end is not None else -1,
                "sync_quality": t.sync_quality,
                "duration_ms": (t.dotnet_end - t.dotnet_start) / (DOTNET_TICKS_PER_SEC / 1000),
                "cortec_samples": t.cortec_end - t.cortec_start,
            })

        df = pd.DataFrame(rows)

        output_path = Path(output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)

        try:
            df.to_parquet(output_path, index=False)
            logger.info("Exported %d aligned trials → %s", len(df), output_path)
        except ImportError:
            # pyarrow not installed — fall back to CSV
            csv_path = output_path.with_suffix(".csv")
            df.to_csv(csv_path, index=False)
            logger.info("Exported %d aligned trials → %s (CSV fallback)", len(df), csv_path)

        return df


# ═══════════════════════════════════════════════════════════════════════════
#  7. Convenience loaders
# ═══════════════════════════════════════════════════════════════════════════

def load_filterlog(csv_path: str | Path) -> pd.DataFrame:
    """Load a FilterLog CSV into a DataFrame."""
    import time as _time
    logger.info("Loading filterLog: %s", csv_path)
    t0 = _time.time()
    ch_cols = [f"CH{i}" for i in range(1, N_CHANNELS + 1)]
    usecols = ["PacketNum", "TimeStamp", "InputTrigger", "boolInterpolated"] + ch_cols
    df = pd.read_csv(csv_path, usecols=usecols, low_memory=False)
    logger.info("  Loaded %d rows in %.1fs", len(df), _time.time() - t0)
    return df


def load_task_log(csv_path: str | Path) -> pd.DataFrame:
    """Load a Task Log CSV."""
    logger.info("Loading task log: %s", csv_path)
    df = pd.read_csv(csv_path)
    logger.info("  %d events, tasks: %s", len(df), sorted(df["Task"].dropna().unique().tolist()))
    return df


def load_emg(csv_path: str | Path) -> pd.DataFrame:
    """Load a raw EMG CSV."""
    logger.info("Loading EMG: %s", csv_path)
    df = pd.read_csv(csv_path, on_bad_lines="skip", low_memory=False)
    logger.info("  %d rows, %d columns", len(df), len(df.columns))
    return df
