"""
Tests for serial port discovery helpers in sleepMonitor.py.

Covered functions:
  - serialPortGetDeviceName(iPort)
  - find_arduino_port()
  - list_arduino_port()        (side-effect: prints & increments cPorts)
"""
import pytest
from unittest.mock import patch, MagicMock
import sleepMonitor


# --------------------------------------------------------------------------- #
# Helpers                                                                      #
# --------------------------------------------------------------------------- #

def _make_port(description, device, vid=None):
    """Build a minimal mock that looks like a serial.tools.list_ports port."""
    p = MagicMock()
    p.description = description
    p.device = device
    p.vid = vid
    return p


# --------------------------------------------------------------------------- #
# serialPortGetDeviceName                                                      #
# --------------------------------------------------------------------------- #

class TestSerialPortGetDeviceName:
    """serialPortGetDeviceName(iPort) returns the .device string at position
    iPort in the enumerated port list, or None when the index is out of range."""

    def test_returns_device_at_index_0(self):
        ports = [_make_port("Arduino Uno", "/dev/ttyUSB0")]
        with patch("serial.tools.list_ports.comports", return_value=ports):
            assert sleepMonitor.serialPortGetDeviceName(0) == "/dev/ttyUSB0"

    def test_returns_device_at_index_1(self):
        ports = [
            _make_port("Port A", "/dev/ttyUSB0"),
            _make_port("Port B", "/dev/ttyUSB1"),
        ]
        with patch("serial.tools.list_ports.comports", return_value=ports):
            assert sleepMonitor.serialPortGetDeviceName(1) == "/dev/ttyUSB1"

    def test_returns_none_for_out_of_range_index(self):
        ports = [_make_port("Arduino", "/dev/ttyUSB0")]
        with patch("serial.tools.list_ports.comports", return_value=ports):
            assert sleepMonitor.serialPortGetDeviceName(5) is None

    def test_returns_none_when_no_ports(self):
        with patch("serial.tools.list_ports.comports", return_value=[]):
            assert sleepMonitor.serialPortGetDeviceName(0) is None

    def test_returns_last_port_correctly(self):
        ports = [
            _make_port("A", "/dev/ttyUSB0"),
            _make_port("B", "/dev/ttyUSB1"),
            _make_port("C", "/dev/ttyUSB2"),
        ]
        with patch("serial.tools.list_ports.comports", return_value=ports):
            assert sleepMonitor.serialPortGetDeviceName(2) == "/dev/ttyUSB2"

    def test_negative_index_returns_none(self):
        """Negative indices should not match any port (no Python negative indexing)."""
        ports = [_make_port("Arduino", "/dev/ttyUSB0")]
        with patch("serial.tools.list_ports.comports", return_value=ports):
            # The function iterates with i starting at 0, so -1 never equals i
            assert sleepMonitor.serialPortGetDeviceName(-1) is None


# --------------------------------------------------------------------------- #
# find_arduino_port                                                            #
# --------------------------------------------------------------------------- #

class TestFindArduinoPort:
    """find_arduino_port() auto-detects the first port that looks like an
    Arduino based on its description string or VID."""

    def test_finds_by_arduino_in_description(self):
        ports = [_make_port("Arduino Uno", "/dev/ttyUSB0")]
        with patch("serial.tools.list_ports.comports", return_value=ports):
            assert sleepMonitor.find_arduino_port() == "/dev/ttyUSB0"

    def test_finds_by_usb_in_description(self):
        ports = [_make_port("USB Serial Port", "COM3")]
        with patch("serial.tools.list_ports.comports", return_value=ports):
            assert sleepMonitor.find_arduino_port() == "COM3"

    def test_finds_by_com_in_parentheses(self):
        ports = [_make_port("Some Device (COM5)", "COM5")]
        with patch("serial.tools.list_ports.comports", return_value=ports):
            assert sleepMonitor.find_arduino_port() == "COM5"

    def test_finds_by_non_none_vid(self):
        ports = [_make_port("Unknown Device", "/dev/ttyACM0", vid=0x2341)]
        with patch("serial.tools.list_ports.comports", return_value=ports):
            assert sleepMonitor.find_arduino_port() == "/dev/ttyACM0"

    def test_returns_none_when_no_matching_ports(self):
        """A Bluetooth device with no VID should not be auto-selected."""
        ports = [_make_port("Bluetooth Device", "/dev/rfcomm0", vid=None)]
        with patch("serial.tools.list_ports.comports", return_value=ports):
            assert sleepMonitor.find_arduino_port() is None

    def test_returns_none_when_port_list_is_empty(self):
        with patch("serial.tools.list_ports.comports", return_value=[]):
            assert sleepMonitor.find_arduino_port() is None

    def test_returns_first_matching_port(self):
        """When multiple ports match, the first one in the list is returned."""
        ports = [
            _make_port("Arduino Mega", "/dev/ttyUSB0"),
            _make_port("Arduino Uno", "/dev/ttyUSB1"),
        ]
        with patch("serial.tools.list_ports.comports", return_value=ports):
            assert sleepMonitor.find_arduino_port() == "/dev/ttyUSB0"

    def test_skips_non_matching_before_matching(self):
        """Non-matching ports before a matching one are skipped."""
        ports = [
            _make_port("Bluetooth", "/dev/rfcomm0", vid=None),
            _make_port("Arduino Nano", "/dev/ttyUSB0"),
        ]
        with patch("serial.tools.list_ports.comports", return_value=ports):
            assert sleepMonitor.find_arduino_port() == "/dev/ttyUSB0"

    def test_matches_lowercase_usb_description(self):
        """'USB' check is case-sensitive per the source code."""
        ports = [_make_port("usb serial device", "/dev/ttyUSB0", vid=None)]
        with patch("serial.tools.list_ports.comports", return_value=ports):
            # 'USB' not in 'usb serial device' — should NOT match
            assert sleepMonitor.find_arduino_port() is None

    def test_matches_mixed_case_arduino_description(self):
        """'Arduino' check is case-sensitive per the source code."""
        ports = [_make_port("arduino uno", "/dev/ttyUSB0", vid=None)]
        with patch("serial.tools.list_ports.comports", return_value=ports):
            # 'Arduino' not in 'arduino uno' — should NOT match
            assert sleepMonitor.find_arduino_port() is None


# --------------------------------------------------------------------------- #
# list_arduino_port                                                            #
# --------------------------------------------------------------------------- #

class TestListArduinoPort:
    """list_arduino_port() prints each port's description and increments the
    global cPorts counter."""

    def test_increments_cports_for_each_port(self, capsys):
        ports = [
            _make_port("Arduino Uno", "/dev/ttyUSB0"),
            _make_port("USB Serial Port", "COM3"),
        ]
        sleepMonitor.cPorts = 0  # reset global state
        with patch("serial.tools.list_ports.comports", return_value=ports):
            sleepMonitor.list_arduino_port()
        assert sleepMonitor.cPorts == 2

    def test_prints_port_descriptions(self, capsys):
        ports = [_make_port("Arduino Mega", "/dev/ttyUSB0")]
        sleepMonitor.cPorts = 0
        with patch("serial.tools.list_ports.comports", return_value=ports):
            sleepMonitor.list_arduino_port()
        output = capsys.readouterr().out
        assert "Arduino Mega" in output

    def test_no_ports_leaves_cports_unchanged(self):
        sleepMonitor.cPorts = 0
        with patch("serial.tools.list_ports.comports", return_value=[]):
            sleepMonitor.list_arduino_port()
        assert sleepMonitor.cPorts == 0

    def test_cumulative_calls_accumulate_cports(self):
        """cPorts is a module-level global and accumulates across calls."""
        ports = [_make_port("Arduino", "/dev/ttyUSB0")]
        sleepMonitor.cPorts = 0
        with patch("serial.tools.list_ports.comports", return_value=ports):
            sleepMonitor.list_arduino_port()
            sleepMonitor.list_arduino_port()
        assert sleepMonitor.cPorts == 2
