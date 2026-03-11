"""
Shared fixtures and bootstrap for Sleep Monitor tests.

tkinter requires a display, which is unavailable in headless CI environments.
We mock the entire tkinter module before any test file imports sleepMonitor or
tkinterLib, so the GUI code never tries to open a window.
"""
import sys
from unittest.mock import MagicMock

# ------------------------------------------------------------------ #
# Mock tkinter early so headless environments can run the test suite. #
# ------------------------------------------------------------------ #
_tk_mock = MagicMock()

# The canvas's winfo_width/winfo_height are called in redraw(); they must
# return real integers otherwise arithmetic operators raise TypeError.
_canvas_mock = MagicMock()
_canvas_mock.winfo_width.return_value = 1200
_canvas_mock.winfo_height.return_value = 700
_tk_mock.Canvas.return_value = _canvas_mock

sys.modules.setdefault("tkinter", _tk_mock)

# Make sure the UserInterface directory is on the path so that
# "import sleepMonitor" and "import tkinterLib" work from any CWD.
import os
_ui_path = os.path.join(os.path.dirname(__file__), "..", "UserInterface")
if _ui_path not in sys.path:
    sys.path.insert(0, os.path.abspath(_ui_path))
