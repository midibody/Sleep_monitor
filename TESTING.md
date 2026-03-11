# Sleep Monitor – Test Coverage Analysis & Improvement Proposals

## Current State

The codebase previously had **zero automated tests**.  This document records
what was added in this pass and proposes the next areas to improve.

---

## What Was Added

A `pytest`-based test suite covering the Python `UserInterface/` layer:

```
tests/
├── conftest.py              – tkinter mock + sys.path bootstrap
├── test_serial_discovery.py – port listing & auto-detection helpers
├── test_read_table.py       – serial data reading / silence timeout
├── test_data_scaling.py     – pure data-transform formulas & axis helpers
└── test_data_parsing.py     – end-to-end CSV parse via plot function
```

Run with:

```bash
pip install -r requirements-dev.txt
pytest                        # all 91 tests
pytest --cov=UserInterface    # with coverage report
```

### Modules touched

| Module | Functions under test | Tests added |
|--------|---------------------|-------------|
| `sleepMonitor.py` | `serialPortGetDeviceName`, `find_arduino_port`, `list_arduino_port`, `read_table` | 22 |
| `tkinterLib.py` | `plot_from_semicolon_data_tk` (parsing + early-return), `safe_minmax` replica, `nice_ticks` replica, all 6 scaling formulas | 69 |

---

## Gaps and Proposed Improvements

The sections below are ordered from highest impact / easiest to implement
(top) to most complex (bottom).

---

### 1  Extract and test `safe_minmax` / `nice_ticks` directly

**Why it matters:** Both functions are nested inside
`plot_from_semicolon_data_tk`.  The current tests replicate them at module
level to exercise them in isolation.  If the originals diverge, the replicas
won't catch the regression.

**Proposal:** Move `safe_minmax` and `nice_ticks` to module-level functions in
`tkinterLib.py` and import them directly in the tests.  This is a two-line
refactor with significant testability gain.

---

### 2  Test `open_serial_with_handling` error paths

**Why it matters:** The function catches `serial.SerialException` and calls
`sys.exit(0)`.  If the exception message format changes, or if a new error
class needs to be caught (e.g. `PermissionError`), this will silently break.

**Proposal:** Add tests that inject a `serial.SerialException` via mock and
assert that `sys.exit` is called (use `pytest.raises(SystemExit)`).  Also
test the happy path where `serial.Serial` opens successfully.

```python
def test_serial_exception_causes_exit(monkeypatch):
    monkeypatch.setattr(
        "serial.Serial",
        lambda **kw: (_ for _ in ()).throw(serial.SerialException("busy"))
    )
    with pytest.raises(SystemExit):
        open_serial_with_handling("/dev/ttyUSB0", 115200, 0.8)
```

---

### 3  Test the CSV file-write logic (commands `c` / `p`)

**Why it matters:** The file-writing block (lines 203–217 of `sleepMonitor.py`)
catches `OSError` silently.  A path-traversal edge case or a full disk will
produce no error in the current code.

**Proposal:** Extract the file-write logic into its own function
`save_data_to_csv(lines, filename)` and test:
- Successful write and file contents.
- `OSError` on open → prints a descriptive message, does not crash.
- `OSError` on per-line write → handled gracefully.

---

### 4  Test `computeVariance` (C++ logic, Python replica)

**Why it matters:** `computeVariance` in `PdmMic.h` implements a sliding-window
mean-absolute-deviation that adjusts the dynamic snoring thresholds.  Bugs
here cause false snore alerts or missed detections — the core product feature.

**Proposal:** Port the 30-line function to Python and test it exhaustively:

| Scenario | Expected behaviour |
|----------|--------------------|
| First call (count=1) | average == v, variation == 0, thresholds set |
| Window not yet full (count < 10) | uses actual count, not numReadings |
| Window full, constant signal | variation == 0 → thresholds adjusted upward |
| Window full, noisy signal | variation ≥ VOL_THR_VARIANCE → thresholds NOT adjusted |
| Roll-over (> 10 calls) | oldest reading is evicted correctly |

---

### 5  Test snoring event detection state machine (C++ logic, Python replica)

**Why it matters:** `snoreDetectWithMotionGate` is a multi-state hysteresis
machine.  Edge cases like:
- Volume oscillating around the threshold
- Motion arriving exactly when `aboveCount == MIN_ON_FRAMES - 1`
- Consecutive snore events within `SNORE_TIMER_AFTER_SNORE`

…are very hard to reproduce on hardware but trivial to cover in a unit test.

**Proposal:** Port the state machine to a Python class and parameterise the
test with the frame-by-frame volume and motion sequences:

```python
@pytest.mark.parametrize("volumes,motions,expected_snore_count", [
    ([15, 15, 15],  [False]*3, 1),   # clean snore
    ([15, 15],      [False]*2, 0),   # too few frames
    ([15, 15, 15],  [True, False, False], 0),  # motion resets counter
    ...
])
def test_snore_state_machine(volumes, motions, expected_snore_count): ...
```

---

### 6  Test `checkSnoringAlert` aggregation and cooldown

**Why it matters:** The vibration alert fires when 3 snore events occur within
`MAX_ELAPSE_TIME_BETWEEN_SNORES_FOR_ALERTS` ms.  Off-by-one errors in the
counter or timer reset lead to either missed alerts or excessive vibration.

**Proposal:** Mock `motorStart` and drive `checkSnoringAlert` with controlled
timestamps:

| Scenario | Expected outcome |
|----------|-----------------|
| 3 snores within window | alert fires, counter resets |
| 3 snores but 3rd is outside window | counter resets, no alert |
| 2 snores within window, then timeout | no alert |
| Rapid 6 snores → 2 alerts | motorStart called twice |

---

### 7  Test flash storage record encoding / decoding (C++ / Python replica)

**Why it matters:** `recordData_t` is packed into 8 bytes with specific bit
layouts.  If a field exceeds its bit width (e.g. `breathPerMinut > 255`), data
silently wraps.  These constraints are never validated in code.

**Proposal:** Write a Python struct-pack replica of the record format and test:
- Round-trip encode → decode for boundary values (0, max, overflow).
- Header magic-byte validation.
- Wrap-around dataset detection (monotonic counter overflow at 65 535).
- Multi-dataset scanning (`findLatestDataset` equivalent).

---

### 8  Integration test: `read_table` → CSV parse → graph

**Why it matters:** The full pipeline (board → serial → list of strings → graph)
is never tested end-to-end.  Subtle issues like a missing header line or an
extra blank line at the start can silently produce an empty graph.

**Proposal:** A parameterised integration test that feeds realistic board output
strings (captured from real sessions and stored as fixtures) through
`read_table` (mocked serial) → `plot_from_semicolon_data_tk` (mocked Tkinter)
and asserts the parsed data matches expectations.

---

### 9  Property-based testing for scaling formulas

**Why it matters:** The scaling formulas are simple but their output feeds
directly into canvas pixel coordinates.  An output outside [0, canvas_height]
crashes the rendering silently.

**Proposal:** Use [Hypothesis](https://hypothesis.readthedocs.io/) to
fuzz-test the scaling functions with arbitrary float inputs:

```python
from hypothesis import given, strategies as st

@given(st.floats(min_value=0, max_value=2**16))
def test_accel_scaling_always_in_range(raw):
    result = scale_accel(raw)
    assert 35.0 <= result <= 45.0
```

---

### 10  CI pipeline

**Why it matters:** Without CI, tests only run if a developer remembers to run
them locally.

**Proposal:** Add a `.github/workflows/test.yml` (or equivalent) that runs
`pytest` on every push and pull request.  The test suite is fast (< 1 s) and
requires no hardware.

```yaml
name: Tests
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: "3.11" }
      - run: pip install -r requirements-dev.txt
      - run: pytest --tb=short
```

---

## Priority Summary

| Priority | Area | Effort | Impact |
|----------|------|--------|--------|
| High | Extract & test `safe_minmax` / `nice_ticks` | Low | Medium |
| High | `open_serial_with_handling` error paths | Low | High |
| High | CSV file-write logic | Low | High |
| High | `computeVariance` (Python replica) | Medium | High |
| High | Snore state machine (Python replica) | Medium | High |
| Medium | `checkSnoringAlert` aggregation | Medium | High |
| Medium | Flash record encode/decode | Medium | Medium |
| Medium | End-to-end integration test | Medium | High |
| Low | Property-based (Hypothesis) scaling tests | Low | Medium |
| Low | CI pipeline | Low | High |
