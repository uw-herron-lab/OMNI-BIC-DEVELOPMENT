# Replay data file format

Replay mode (`python -m src.mock_server --replay <dir>`) streams a previously
recorded session back over the gRPC API as if it were live. Point `--replay` at
a **folder** that contains at least a filterLog CSV; a task log is optional and
only used to derive ground-truth rest/move labels.

> **No participant data ships with this repository.** Bring your own recording.
> Anything under `data/` and `session_data/` is git-ignored so recordings are
> never committed by accident.

```
<dir>/
├── filterLog_<something>.csv     # required — the neural recording
└── <something>Task_Log<...>.csv  # optional — task markers for labels
```

The loader globs `filterLog_*.csv` and `*Task_Log*.csv`, so any prefix/suffix
around those tokens is fine. If several match, the first (sorted) is used.

## filterLog CSV (required)

One row per 1 kHz sample. These columns are read (extra columns are ignored):

| Column        | Type            | Meaning |
|---------------|-----------------|---------|
| `TimeStamp`   | int64           | Device-clock timestamp (ticks). Used for task-log sync. |
| `InputTrigger`| bool / 0-1      | TTL sync line. Accepts `True`/`False` or `1`/`0`. Bridges the device clock to the PC clock for labeling. |
| `CH1` … `CH32`| float           | The 32 neural channels, in microvolts. |

Only `CH1..CH32`, `TimeStamp`, and `InputTrigger` are required for replay. The
channel values are streamed verbatim into each `NeuralSample.measurements`.

## Task log CSV (optional)

Marks when each task interval starts and stops, on the PC (.NET) clock. Used to
label each streamed sample as rest (0) or move (1). Without it, every sample is
labeled rest.

| Column      | Type   | Meaning |
|-------------|--------|---------|
| `Task`      | string | Task code, e.g. `STD1`. Select which with `--task`. |
| `Marker`    | string | `Start` or `Stop` of an interval. |
| `Timestamp` | int64  | .NET DateTime ticks (100 ns each) at the marker. |

Labeling maps each `Start`→`Stop` interval of the selected task onto the
filterLog rows, using the `InputTrigger` edges to align the device clock to the
task-log clock. The alignment is handled by `src/sync.py` (`TriStreamSync`),
with a simpler histogram + linear-fit fallback built into the replay loader if
`TriStreamSync` fails. Choose the task code with `--task STD1` (default), or
`--task all` to treat every task interval as movement.

## Clock domains, in one line

`filterLog.TimeStamp` is the **device** clock; `taskLog.Timestamp` is the **PC**
clock; the `InputTrigger` TTL appears in both and is what lets the two be
aligned so labels land on the right samples.
