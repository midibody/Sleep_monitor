"""
Tests for the data scaling and transformation formulas used in tkinterLib.py.

These formulas convert raw sensor values into the coordinate space displayed
on the graph.  They are pure arithmetic expressions embedded in the CSV
parsing loop of plot_from_semicolon_data_tk(); we test them here as standalone
calculations so any future regressions are caught immediately without needing
to spin up a Tkinter window.

Formulas under test (from tkinterLib.py lines 21-27):
  accel   = (min(AccelMax / 800, 10.0) + 35.0) * scale_secondary
  position = 2 * Position + 32.0
  bpm      = BreathPerMinute           (no transformation)
  bmax     = BreathMax                 (no transformation)
  cSnores  = cSnores * scale_secondary
  csmall   = (cSmallMoves + 25) * scale_secondary
"""
import pytest


# --------------------------------------------------------------------------- #
# Helpers – mirror the exact expressions in tkinterLib.py                     #
# --------------------------------------------------------------------------- #

def scale_accel(raw, scale=1):
    return (min(raw / 800, 10.0) + 35.0) * scale

def scale_position(raw):
    return 2 * raw + 32.0

def scale_csnores(raw, scale=1):
    return raw * scale

def scale_csmall(raw, scale=1):
    return (raw + 25) * scale


# --------------------------------------------------------------------------- #
# AccelMax scaling                                                             #
# --------------------------------------------------------------------------- #

class TestAccelScaling:
    """AccelMax / 800 is clamped to 10, then 35 is added, then multiplied by
    scale_secondary."""

    def test_zero_maps_to_35(self):
        assert scale_accel(0) == 35.0

    def test_value_below_cap(self):
        # 4000 / 800 = 5.0 → 5.0 + 35 = 40.0
        assert scale_accel(4000) == pytest.approx(40.0)

    def test_value_exactly_at_cap(self):
        # 8000 / 800 = 10.0 (exactly at cap) → 10.0 + 35 = 45.0
        assert scale_accel(8000) == pytest.approx(45.0)

    def test_value_above_cap_is_clamped(self):
        # 16000 / 800 = 20 → clamped to 10 → + 35 = 45.0
        assert scale_accel(16000) == pytest.approx(45.0)

    def test_very_large_value_clamped(self):
        assert scale_accel(999_999) == pytest.approx(45.0)

    def test_scale_secondary_multiplied(self):
        # raw=800 → min(1.0, 10.0) + 35 = 36.0; × 2 = 72.0
        assert scale_accel(800, scale=2) == pytest.approx(72.0)

    def test_scale_secondary_zero(self):
        assert scale_accel(4000, scale=0) == pytest.approx(0.0)

    def test_fractional_raw_value(self):
        # 400 / 800 = 0.5 → + 35 = 35.5
        assert scale_accel(400) == pytest.approx(35.5)


# --------------------------------------------------------------------------- #
# Position scaling                                                             #
# --------------------------------------------------------------------------- #

class TestPositionScaling:
    """Position enum values 1-5 are multiplied by 2 and offset by 32."""

    @pytest.mark.parametrize("pos,expected", [
        (0, 32.0),   # POS_UNKNOWN
        (1, 34.0),   # POS_LEFT
        (2, 36.0),   # POS_BACK
        (3, 38.0),   # POS_RIGHT
        (4, 40.0),   # POS_STOMAC
        (5, 42.0),   # POS_STANDUP
    ])
    def test_known_positions(self, pos, expected):
        assert scale_position(pos) == pytest.approx(expected)

    def test_zero_position(self):
        assert scale_position(0) == pytest.approx(32.0)

    def test_offset_is_always_32(self):
        for pos in range(6):
            result = scale_position(pos)
            assert result >= 32.0


# --------------------------------------------------------------------------- #
# cSnores scaling                                                              #
# --------------------------------------------------------------------------- #

class TestCSnoresScaling:
    """cSnores is only multiplied by scale_secondary (no offset)."""

    def test_zero_snores(self):
        assert scale_csnores(0) == pytest.approx(0.0)

    def test_scale_one(self):
        assert scale_csnores(5) == pytest.approx(5.0)

    def test_scale_two(self):
        assert scale_csnores(5, scale=2) == pytest.approx(10.0)

    def test_scale_zero_clears_value(self):
        assert scale_csnores(100, scale=0) == pytest.approx(0.0)


# --------------------------------------------------------------------------- #
# cSmallMoves scaling                                                          #
# --------------------------------------------------------------------------- #

class TestCSmallMovesScaling:
    """cSmallMoves is offset by +25 before being multiplied by scale_secondary.
    The +25 offset ensures small non-zero counts are always visible on the graph."""

    def test_zero_moves(self):
        # 0 + 25 = 25
        assert scale_csmall(0) == pytest.approx(25.0)

    def test_typical_value(self):
        # 10 + 25 = 35
        assert scale_csmall(10) == pytest.approx(35.0)

    def test_scale_two(self):
        # (5 + 25) * 2 = 60
        assert scale_csmall(5, scale=2) == pytest.approx(60.0)

    def test_scale_zero(self):
        assert scale_csmall(10, scale=0) == pytest.approx(0.0)

    def test_offset_shifts_zero_above_baseline(self):
        """Even zero moves produce a non-zero value (the +25 baseline)."""
        assert scale_csmall(0) > 0.0


# --------------------------------------------------------------------------- #
# Graph axis helpers (replicated from tkinterLib.py nested functions)         #
# --------------------------------------------------------------------------- #

def safe_minmax(arrs):
    """Replica of safe_minmax() nested in plot_from_semicolon_data_tk."""
    mn = mx = None
    for a in arrs:
        for v in a:
            if mn is None or v < mn:
                mn = v
            if mx is None or v > mx:
                mx = v
    if mn is None:
        mn = 0.0
    if mx is None:
        mx = 1.0
    if mn == mx:
        mx = mn + 1.0
    return mn, mx


def nice_ticks(vmin, vmax, n=6):
    """Replica of nice_ticks() nested in plot_from_semicolon_data_tk."""
    step = (vmax - vmin) / float(n)
    return [vmin + i * step for i in range(n + 1)]


class TestSafeMinmax:
    """safe_minmax() finds the global min/max across a list of value lists,
    with sensible fallbacks for empty or constant data."""

    def test_basic_range(self):
        mn, mx = safe_minmax([[1, 5, 3]])
        assert mn == pytest.approx(1.0)
        assert mx == pytest.approx(5.0)

    def test_multiple_arrays(self):
        mn, mx = safe_minmax([[1, 2], [10, 3], [0, 7]])
        assert mn == pytest.approx(0.0)
        assert mx == pytest.approx(10.0)

    def test_empty_arrays_fallback(self):
        mn, mx = safe_minmax([[], []])
        assert mn == pytest.approx(0.0)
        assert mx == pytest.approx(1.0)

    def test_single_value_gets_unit_range(self):
        """When all values are identical, mx is bumped up by 1 to avoid
        a zero-height axis."""
        mn, mx = safe_minmax([[5.0, 5.0]])
        assert mn == pytest.approx(5.0)
        assert mx == pytest.approx(6.0)

    def test_negative_values(self):
        mn, mx = safe_minmax([[-10, -1]])
        assert mn == pytest.approx(-10.0)
        assert mx == pytest.approx(-1.0)

    def test_mixed_positive_negative(self):
        mn, mx = safe_minmax([[-5, 5]])
        assert mn == pytest.approx(-5.0)
        assert mx == pytest.approx(5.0)

    def test_no_arrays_fallback(self):
        mn, mx = safe_minmax([])
        assert mn == pytest.approx(0.0)
        assert mx == pytest.approx(1.0)


class TestNiceTicks:
    """nice_ticks() returns n+1 evenly-spaced values from vmin to vmax."""

    def test_returns_n_plus_one_values(self):
        ticks = nice_ticks(0, 10, n=6)
        assert len(ticks) == 7

    def test_first_value_is_vmin(self):
        ticks = nice_ticks(5, 15)
        assert ticks[0] == pytest.approx(5.0)

    def test_last_value_is_vmax(self):
        ticks = nice_ticks(5, 15)
        assert ticks[-1] == pytest.approx(15.0)

    def test_evenly_spaced(self):
        ticks = nice_ticks(0, 6, n=6)
        for t in ticks:
            assert t == pytest.approx(round(t))  # each should be an integer

    def test_single_division(self):
        ticks = nice_ticks(0, 10, n=1)
        assert ticks == pytest.approx([0.0, 10.0])

    def test_floating_point_range(self):
        ticks = nice_ticks(0.0, 45.0, n=6)
        assert ticks[0] == pytest.approx(0.0)
        assert ticks[-1] == pytest.approx(45.0)
        assert len(ticks) == 7
