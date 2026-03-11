"""
Tests for read_table() in sleepMonitor.py.

read_table() polls a global serial object (ser) until END_SILENCE seconds
elapse without new data.  We inject a mock serial object via setattr (since
`ser` only exists as a module global when the script is run as __main__) and
mock time.time() to drive the silence-detection logic deterministically.
"""
import pytest
from unittest.mock import MagicMock, patch
import sleepMonitor


def _make_ser(*responses):
    """
    Build a mock serial object whose readline() returns each item in
    *responses* in order.  b'' simulates a read timeout (no data).
    """
    mock_ser = MagicMock()
    mock_ser.readline.side_effect = list(responses)
    return mock_ser


def _run_read_table(ser_mock, time_values):
    """
    Inject *ser_mock* as the module-level ``ser`` and drive time.time()
    through *time_values*, then call read_table().
    """
    time_iter = iter(time_values)
    setattr(sleepMonitor, "ser", ser_mock)
    try:
        with patch("sleepMonitor.time") as mock_time:
            mock_time.time.side_effect = lambda: next(time_iter)
            return sleepMonitor.read_table()
    finally:
        # Clean up so other tests don't see a stale ser attribute
        if hasattr(sleepMonitor, "ser"):
            delattr(sleepMonitor, "ser")


class TestReadTable:
    """Verify that read_table() collects lines correctly and terminates when
    the inter-byte silence exceeds END_SILENCE seconds."""

    # ------------------------------------------------------------------ #
    # Happy-path: normal data reception                                   #
    # ------------------------------------------------------------------ #

    def test_collects_single_line(self):
        """One data line followed by a timeout should return that line."""
        ser = _make_ser(b"hello\r\n", b"")
        times = [0.0, 0.1, 0.1 + sleepMonitor.END_SILENCE + 0.01]
        result = _run_read_table(ser, times)
        assert result == ["hello"]

    def test_collects_multiple_lines(self):
        ser = _make_ser(b"line1\r\n", b"line2\r\n", b"line3\r\n", b"")
        times = [0.0, 0.1, 0.2, 0.3, 0.3 + sleepMonitor.END_SILENCE + 0.01]
        result = _run_read_table(ser, times)
        assert result == ["line1", "line2", "line3"]

    def test_strips_carriage_return_and_newline(self):
        ser = _make_ser(b"data\r\n", b"")
        times = [0.0, 0.1, 0.1 + sleepMonitor.END_SILENCE + 0.01]
        result = _run_read_table(ser, times)
        assert result == ["data"]

    def test_preserves_leading_whitespace(self):
        """rstrip() should not remove leading spaces."""
        ser = _make_ser(b"  indented\r\n", b"")
        times = [0.0, 0.1, 0.1 + sleepMonitor.END_SILENCE + 0.01]
        result = _run_read_table(ser, times)
        assert result == ["  indented"]

    # ------------------------------------------------------------------ #
    # Edge cases: no data / silence timing                                 #
    # ------------------------------------------------------------------ #

    def test_empty_response_returns_empty_list(self):
        """If the board sends nothing at all, return an empty list."""
        ser = _make_ser(b"")
        times = [0.0, sleepMonitor.END_SILENCE + 0.01]
        result = _run_read_table(ser, times)
        assert result == []

    def test_silence_timeout_respected(self):
        """Loop must not break until END_SILENCE seconds have elapsed since
        the last received byte."""
        ser = _make_ser(b"row\r\n", b"", b"")
        # Second empty read: silence NOT yet expired; third: silence expired
        times = [
            0.0,                                        # initial last_rx_time
            0.1,                                        # after "row" received → update last_rx_time
            0.1 + sleepMonitor.END_SILENCE * 0.5,      # empty, but not yet expired
            0.1 + sleepMonitor.END_SILENCE + 0.01,     # expired → break
        ]
        result = _run_read_table(ser, times)
        assert result == ["row"]

    def test_handles_non_ascii_bytes_gracefully(self):
        """decode(errors='replace') should not raise for invalid UTF-8."""
        ser = _make_ser(b"bad\xff\xfe bytes\r\n", b"")
        times = [0.0, 0.1, 0.1 + sleepMonitor.END_SILENCE + 0.01]
        result = _run_read_table(ser, times)
        assert len(result) == 1
        assert "bad" in result[0]   # replacement char replaces the bad bytes

    def test_csv_header_and_rows_returned_verbatim(self):
        """A realistic board response (CSV data) should be passed through as-is."""
        header = b"Index;AccelMax;Position;BreathPerMinute;BreathMax;cSnores;cSmallMoves\r\n"
        row1   = b"0;1200;2;14;30;0;3\r\n"
        row2   = b"1;1350;2;15;28;1;2\r\n"
        ser = _make_ser(header, row1, row2, b"")
        times = [0.0, 0.1, 0.2, 0.3, 0.3 + sleepMonitor.END_SILENCE + 0.01]
        result = _run_read_table(ser, times)
        assert result[0].startswith("Index")
        assert result[1] == "0;1200;2;14;30;0;3"
        assert result[2] == "1;1350;2;15;28;1;2"
