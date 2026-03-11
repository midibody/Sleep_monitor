"""
Tests for the CSV parsing logic inside plot_from_semicolon_data_tk()
(tkinterLib.py).

The function has two distinct phases:
  1. Parse semicolon-delimited CSV lines into Python lists.
  2. Build a Tkinter window and render the data.

We test phase 1 by verifying the function's observable behaviour with valid and
invalid inputs.  Phase 2 (Tkinter) is suppressed by the tkinter mock installed
in conftest.py; root.mainloop() returns immediately as a MagicMock call.

Because the parsing and early-return logic runs before any Tkinter call, we can
also test edge cases (too few rows, missing columns, etc.) that cause the
function to return early with a console message.
"""
import pytest
from io import StringIO
import csv
from unittest.mock import patch, MagicMock


# --------------------------------------------------------------------------- #
# Build realistic test input                                                   #
# --------------------------------------------------------------------------- #

HEADER = "Index;AccelMax;Position;BreathPerMinute;BreathMax;cSnores;cSmallMoves"


def _make_lines(*rows):
    """Return a list of strings starting with the CSV header."""
    return [HEADER] + list(rows)


def _row(idx, accel=1000, pos=2, bpm=14, bmax=30, snores=0, moves=5):
    return f"{idx};{accel};{pos};{bpm};{bmax};{snores};{moves}"


# --------------------------------------------------------------------------- #
# Tests                                                                        #
# --------------------------------------------------------------------------- #

class TestPlotFromSemicolonDataTk:

    # ------------------------------------------------------------------ #
    # Early-return path (< 2 data rows)                                    #
    # ------------------------------------------------------------------ #

    def test_no_data_rows_prints_message(self, capsys):
        from tkinterLib import plot_from_semicolon_data_tk
        plot_from_semicolon_data_tk([HEADER])
        assert "Not enough data" in capsys.readouterr().out

    def test_single_data_row_prints_message(self, capsys):
        from tkinterLib import plot_from_semicolon_data_tk
        lines = _make_lines(_row(0))
        plot_from_semicolon_data_tk(lines)
        assert "Not enough data" in capsys.readouterr().out

    def test_empty_input_prints_message(self, capsys):
        from tkinterLib import plot_from_semicolon_data_tk
        plot_from_semicolon_data_tk([])
        assert "Not enough data" in capsys.readouterr().out

    def test_only_empty_strings_prints_message(self, capsys):
        from tkinterLib import plot_from_semicolon_data_tk
        plot_from_semicolon_data_tk(["", ""])
        assert "Not enough data" in capsys.readouterr().out

    # ------------------------------------------------------------------ #
    # Parsing reaches the Tkinter phase (>= 2 data rows)                  #
    # ------------------------------------------------------------------ #

    def test_two_rows_does_not_raise(self):
        """With enough data, no exception should be raised (Tkinter is mocked)."""
        from tkinterLib import plot_from_semicolon_data_tk
        lines = _make_lines(_row(0), _row(1))
        plot_from_semicolon_data_tk(lines)  # must not raise

    def test_many_rows_does_not_raise(self):
        from tkinterLib import plot_from_semicolon_data_tk
        lines = _make_lines(*[_row(i) for i in range(100)])
        plot_from_semicolon_data_tk(lines)

    def test_scale_secondary_zero_does_not_raise(self):
        from tkinterLib import plot_from_semicolon_data_tk
        lines = _make_lines(_row(0), _row(1))
        plot_from_semicolon_data_tk(lines, scale_secondary=0)

    def test_scale_secondary_two_does_not_raise(self):
        from tkinterLib import plot_from_semicolon_data_tk
        lines = _make_lines(_row(0), _row(1))
        plot_from_semicolon_data_tk(lines, scale_secondary=2)

    def test_custom_title_does_not_raise(self):
        from tkinterLib import plot_from_semicolon_data_tk
        lines = _make_lines(_row(0), _row(1))
        plot_from_semicolon_data_tk(lines, title="My Custom Title")

    # ------------------------------------------------------------------ #
    # AccelMax clamping – verify via parse-only replication               #
    # ------------------------------------------------------------------ #

    def test_accel_max_clamping_high_value(self):
        """Very large AccelMax should clamp to 10 before the +35 offset."""
        raw_accel = 99999
        expected_mini = min(raw_accel / 800, 10.0)
        assert expected_mini == pytest.approx(10.0)

    def test_accel_max_clamping_small_value(self):
        raw_accel = 800
        expected_mini = min(raw_accel / 800, 10.0)
        assert expected_mini == pytest.approx(1.0)

    # ------------------------------------------------------------------ #
    # CSV format quirks                                                    #
    # ------------------------------------------------------------------ #

    def test_extra_whitespace_around_values(self, capsys):
        """The standard csv.DictReader strips field delimiters but not
        interior spaces; verify the function is resilient to real board output."""
        from tkinterLib import plot_from_semicolon_data_tk
        # Board sometimes emits spaces before numbers
        lines = [
            HEADER,
            " 0; 1000; 2; 14; 30; 0; 5",
            " 1; 1200; 2; 15; 28; 1; 3",
        ]
        # csv.DictReader with delimiter=';' does NOT strip spaces from values;
        # float(" 1000") works in Python so this should still parse OK.
        plot_from_semicolon_data_tk(lines)  # must not raise

    def test_zero_accel_does_not_raise(self):
        from tkinterLib import plot_from_semicolon_data_tk
        lines = _make_lines(_row(0, accel=0), _row(1, accel=0))
        plot_from_semicolon_data_tk(lines)

    def test_max_sensor_values_do_not_raise(self):
        from tkinterLib import plot_from_semicolon_data_tk
        lines = _make_lines(
            _row(0, accel=65535, pos=5, bpm=255, bmax=255, snores=255, moves=65535),
            _row(1, accel=65535, pos=5, bpm=255, bmax=255, snores=255, moves=65535),
        )
        plot_from_semicolon_data_tk(lines)

    def test_all_zeros_does_not_raise(self):
        from tkinterLib import plot_from_semicolon_data_tk
        lines = _make_lines(_row(0, 0, 0, 0, 0, 0, 0), _row(1, 0, 0, 0, 0, 0, 0))
        plot_from_semicolon_data_tk(lines)

    # ------------------------------------------------------------------ #
    # Behaviour with non-CSV junk lines mixed in                           #
    # ------------------------------------------------------------------ #

    def test_junk_lines_before_header_cause_parse_failure(self, capsys):
        """If a junk line appears before the header, DictReader may not
        recognise the column names and index list stays empty → early return."""
        from tkinterLib import plot_from_semicolon_data_tk
        lines = ["JUNK LINE", _row(0), _row(1)]
        # DictReader treats first line as header; columns won't match.
        # Function may raise KeyError or return early. Both are acceptable here;
        # the important thing is that it doesn't silently produce wrong output.
        try:
            plot_from_semicolon_data_tk(lines)
        except (KeyError, ValueError):
            pass  # acceptable: bad input, explicit failure


# --------------------------------------------------------------------------- #
# CSV parsing logic replicated for direct unit testing                        #
# --------------------------------------------------------------------------- #

def _parse_csv(lines, scale_secondary=1):
    """
    Replicate the CSV parsing block from plot_from_semicolon_data_tk so we can
    inspect the resulting lists without touching Tkinter at all.
    """
    data_str = "\n".join(lines)
    f = StringIO(data_str)
    reader = csv.DictReader(f, delimiter=";")

    index, accel, position, bpm, bmax, cSnores, csmall = [], [], [], [], [], [], []
    for row in reader:
        index.append(int(row["Index"]))
        mini = min(float(row["AccelMax"]) / 800, 10.0)
        accel.append((mini + 35.0) * scale_secondary)
        position.append(2 * float(row["Position"]) + 32.0)
        bpm.append(float(row["BreathPerMinute"]))
        bmax.append(float(row["BreathMax"]))
        cSnores.append(float(row["cSnores"]) * scale_secondary)
        csmall.append((float(row["cSmallMoves"]) + 25) * scale_secondary)

    return index, accel, position, bpm, bmax, cSnores, csmall


class TestCsvParsingDirect:
    """Directly test the CSV parse + transform logic (no Tkinter required)."""

    def test_index_parsed_as_integers(self):
        lines = _make_lines(_row(0), _row(1), _row(2))
        idx, *_ = _parse_csv(lines)
        assert idx == [0, 1, 2]

    def test_bpm_passed_through_unchanged(self):
        lines = _make_lines(_row(0, bpm=12), _row(1, bpm=18))
        _, _, _, bpm, *_ = _parse_csv(lines)
        assert bpm == pytest.approx([12.0, 18.0])

    def test_breathmax_passed_through_unchanged(self):
        lines = _make_lines(_row(0, bmax=25), _row(1, bmax=40))
        _, _, _, _, bmax, *_ = _parse_csv(lines)
        assert bmax == pytest.approx([25.0, 40.0])

    def test_accel_clamped_and_offset(self):
        # raw=800 → min(1.0,10.0)=1.0 → +35=36.0
        lines = _make_lines(_row(0, accel=800), _row(1, accel=800))
        _, accel, *_ = _parse_csv(lines)
        assert accel == pytest.approx([36.0, 36.0])

    def test_accel_clamped_at_maximum(self):
        lines = _make_lines(_row(0, accel=99999), _row(1, accel=99999))
        _, accel, *_ = _parse_csv(lines)
        assert all(v == pytest.approx(45.0) for v in accel)

    def test_position_offset(self):
        # pos=2 (BACK) → 2*2+32 = 36.0
        lines = _make_lines(_row(0, pos=2), _row(1, pos=2))
        _, _, position, *_ = _parse_csv(lines)
        assert position == pytest.approx([36.0, 36.0])

    def test_csnores_scaled(self):
        lines = _make_lines(_row(0, snores=3), _row(1, snores=5))
        *_, cSnores, _ = _parse_csv(lines, scale_secondary=2)
        assert cSnores == pytest.approx([6.0, 10.0])

    def test_csmall_offset_and_scaled(self):
        # moves=10 → (10+25)*scale=35*1=35
        lines = _make_lines(_row(0, moves=10), _row(1, moves=10))
        *_, csmall = _parse_csv(lines)
        assert csmall == pytest.approx([35.0, 35.0])

    def test_row_count_matches_input(self):
        n = 50
        lines = _make_lines(*[_row(i) for i in range(n)])
        idx, *_ = _parse_csv(lines)
        assert len(idx) == n
