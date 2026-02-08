## Data Storage (Internal Flash) — Technical/Functional Specification

### Purpose

The data storage subsystem provides **persistent, low-overhead logging** of sleep-monitoring metrics into the nRF52840 **internal flash**. It is designed to:

* Store a night (or several nights) of **fixed-size data records** (8 bytes each) for efficient flash usage.
* Optionally interleave **event/log records** (also 8 bytes) inside the same record stream.
* Preserve contextual metadata (sensor calibration bias, battery voltage, record count) in a compact **header**.
* Support **circular (ring) storage** over a dedicated user flash region, overwriting older data once the region is full.
* Allow the firmware to **recover the last valid dataset** after reset by scanning for a valid header.

This storage is intended for autonomous operation: collect records during the night, persist them, then later export via serial (screen / CSV) or analysis tools.

---

## Memory model and constraints
Internal flash memory has a finite write endurance, typically on the order of 10,000 erase cycles per flash page for the nRF52 family. A page must be erased before it can be rewritten, and repeated erase operations on the same page eventually wear it out.

The use of a circular (ring) storage strategy that I implemented is interesting to achieving longer flash lifetime, specifically during teh testing debugging period while we tend to write massively into Flash.
### Flash region allocation

A fixed flash area near the end of flash is reserved as “user storage”. The implementation assumes:

* Flash writes are **32-bit word** writes (aligned to 4 bytes).
* Flash erases are performed **per page** (4 KB).
* The storage region spans several 4 KB pages (typically ~28 KB in the current setup).

A safety constraint is explicitly enforced: writing is restricted to `[FIRST_USER_FLASH_ADDR … LAST_USER_FLASH_ADDR)`. This prevents accidental overwrites into bootloader/application code.

### Data alignment

All stored record structures are:

* **Exactly 8 bytes**, **4-byte aligned**, to match the flash word-write model and simplify addressing.
* Written in units of 32-bit words to avoid unaligned/partial writes.

---

## What gets stored

### 1) Measurement records (primary dataset)

The main dataset is a sequence of fixed-size “measurement records” containing:

* A “time-like” field (historically seconds since startup; currently used to store **AccelMax** as a compact metric).
* Sleep **position/state** code (0..3 for positions; also reused to distinguish record categories).
* Breathing metrics (breaths per minute, max breath interval or similar peak metric).
* Snoring counter(s) and “small move” / alert counter field (the code currently stores snore-alert count in that slot).

These records are meant to be produced periodically (e.g., once per minute), giving predictable storage size.

### 2) Log/event records (embedded inside the stream)

The storage can also carry “log records” embedded among measurement records. A log record includes:

* Log type (success/info/warning/error, etc.)
* A code (event ID)
* A timestamp in milliseconds since startup
* Optional short text message appended **immediately after the log record** (spanning additional 8-byte slots as needed)

This allows the system to keep a compact timeline of events such as warnings, errors, or power measurements, without needing a separate log file.

### 3) Header metadata (one per stored dataset)

Each stored dataset begins with a header containing:

* A 3-byte magic signature (used to validate headers during scanning)
* An incremental **dataset counter** (monotonic within normal operation, used to identify the most recent dataset)
* The number of measurement records associated with that dataset
* IMU gyroscope bias terms
* Battery/rail voltage snapshot stored as millivolts (scaled)

The header is small and optimized to remain word-aligned.

---

## Storage lifecycle

### A) Collecting in RAM

During runtime, records are accumulated in an in-RAM array (`tabRecords`) until a store operation is triggered.

* Measurement records are appended at a fixed cadence (or when needed).
* Log records can be appended at any time, optionally with text (using additional record slots).

A hard cap (`N_RECORDS_MAX`) prevents buffer overflow; if full, new entries are ignored.

### B) Persisting to flash (commit)

When committing to flash (when user press the button more than 2 seconds), the system performs a **dataset write**:

1. Determine where the next dataset should be written (append-mode inside the circular region).
2. Increment the dataset counter.
3. Write a header at the chosen location.
4. Write the measurement/log record stream immediately after the header.

If the dataset cannot fit contiguously before the end of the flash region, the record stream is **split into two blocks**:

* Block A written at the end of the region
* Block B written at the start of the region (wrap-around)

Importantly, in wrap-around cases the implementation does **not** write a second header for the second block: the header remains the single authoritative start marker for the dataset.

### C) Flash erasure strategy

Flash pages must be erased before writing. The system erases only the pages that will be impacted by the upcoming write, with two key principles:

* Page erase boundaries are computed from write start and length.
* If the write begins in the middle of a page, that page is assumed to have been erased previously (because it was part of earlier write operations), so erasure starts from the **next** page boundary.

This reduces unnecessary page erases (and wear), while keeping writes safe.

### D) Word-write safety and bounds checking

All flash writes are guarded by:

* Null/zero-length checks
* 4-byte alignment checks
* Overflow checks on address arithmetic
* Strict boundary checks to enforce writes only inside the user storage region
* NVMC ready-wait and temporary NVMC mode changes (write-enable / erase-enable / read-only)
* IRQ disable during critical write/erase sequences

This is essential because a failed flash operation can corrupt the dataset or destabilize the system.

---

## Boot-time recovery and reading

### Locating the latest dataset

At startup, the system can recover stored data by scanning the entire user flash region word-by-word and detecting valid headers using the magic signature.

To load datasets, you can use one of two modes:

* **Most recent dataset**: choose the header with the highest counter value.
* **Explicit dataset selection**: if a “counterToRead” is provided, select the header matching that counter.

Once selected, the system extracts:

* record count
* calibration biases
* the start address of the data record stream (immediately after header, with wrap handling)

### Loading records into RAM

When reading a dataset:

* If the record stream is contiguous (does not reach the end of region), it is copied in one block.
* If it wraps, it is copied in two blocks (end of region, then beginning), reassembled into RAM in chronological order.

While doing so, the system also computes the next safe write address for appending the next dataset:

* normally “end of loaded record stream”
* if too close to the region end to fit a header, it wraps to the beginning

A sanity check warns if the computed “next write address” is not empty when it should be (a sign of misalignment or corruption).

---

## Export and inspection

The subsystem provides human- and tool-friendly outputs:

* **Screen format**: tabular listing of measurement records, skipping over embedded log records and their optional text blocks.
* **CSV format**: semicolon-separated output suitable for direct import into spreadsheets or Python scripts.

Logs can also be printed in a readable form:

* timestamp formatted as `HH:MM:SS.mmm`
* type + code + optional appended text

---

## Typical usage patterns

### Night recording

* Collect one measurement record per minute (or similar).
* Store a dataset after a “session” (e.g., morning, or when stopping monitoring).

### Debug / validation

* Synthetic fill + store is available to validate flash integrity and wrap behavior.
* Power consumption logging can be appended as INFO logs if enabled, including delta vs previous current measurement.

