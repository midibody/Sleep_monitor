import tkinter as tk
from io import StringIO
import csv

def plot_from_semicolon_data_tk(lines, scale_secondary=1, title="SleepMonitor Plot"):
    # Parse CSV-like semicolon data
    data_str = "\n".join(lines)
    f = StringIO(data_str)
    reader = csv.DictReader(f, delimiter=';')

    index = []
    accel = []
    position = []
    bpm = []
    bmax = []
    cSnores = []
    csmall = []

    for row in reader:
        index.append(int(row["Index"]))
        mini= min ( (float(row["AccelMax"])/800) ,10.0)
        accel.append( ( mini +35.0) * scale_secondary)
        position.append( (2 * float(row["Position"])) + 32.0)
        bpm.append(float(row["BreathPerMinute"]))
        bmax.append(float(row["BreathMax"]))
        cSnores.append((float(row["cSnores"])) * scale_secondary)
        csmall.append( ( float(row["cSmallMoves"]) +25) * scale_secondary)

    if len(index) < 2:
        print("Not enough data to plot.")
        return

    # Tkinter window
    root = tk.Tk()
    root.title(title)

    # Fullscreen-ish initial geometry (resizable)
    root.geometry("1200x700")
    root.minsize(800, 500)

    # Canvas
    canvas = tk.Canvas(root, bg="white", highlightthickness=0)
    canvas.pack(fill="both", expand=True)

    # Series definitions (left axis and right axis)
    left_series = [
        ("Position", position),
        ("Breath Per Minute", bpm),
        ("Breath Max", bmax),
        ("Accel Max", accel)
    ]
    right_series = [
#        ("AccelMax (scaled)", accel),
        ("cSnores (hash)", cSnores),
        ("cMoves or SnoreAlerts (hash)", csmall),
    ]

    # Fixed colors (simple)
    colors_left = ["#1f77b4", "#2ca02c", "#d62728","#9467bd"]
    colors_right = ["#9467bd", "#8c564b", "#7f7f7f"]

    # Helpers
    def safe_minmax(arrs):
        mn = None
        mx = None
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
        step = (vmax - vmin) / float(n)
        return [vmin + i * step for i in range(n + 1)]

    # Precompute ranges
    x_min = min(index)
    x_max = max(index)
    if x_min == x_max:
        x_max = x_min + 1

    yL_min, yL_max = safe_minmax([s for _, s in left_series])
    #yR_min, yR_max = safe_minmax([s for _, s in right_series])
    
    yL_min =yR_min = 0
    yL_max = yR_max = 45

    # Layout
    PAD_L = 70
    PAD_R = 70
    PAD_T = 40
    PAD_B = 60
    LEG_H = 40

    def redraw():
        canvas.delete("all")
        w = canvas.winfo_width()
        h = canvas.winfo_height()

        plot_left = PAD_L
        plot_right = max(PAD_L + 10, w - PAD_R)
        plot_top = PAD_T
        plot_bottom = max(PAD_T + 10, h - PAD_B - LEG_H)

        # Avoid zero-size
        if plot_right <= plot_left + 10 or plot_bottom <= plot_top + 10:
            return

        # Axis transforms
        def x_to_px(x):
            return plot_left + (x - x_min) * (plot_right - plot_left) / (x_max - x_min)

        def yL_to_py(y):
            return plot_bottom - (y - yL_min) * (plot_bottom - plot_top) / (yL_max - yL_min)

        def yR_to_py(y):
            return plot_bottom - (y - yR_min) * (plot_bottom - plot_top) / (yR_max - yR_min)

        # Grid + axes box
        canvas.create_rectangle(plot_left, plot_top, plot_right, plot_bottom, outline="#000000")

        # X ticks
        xticks = nice_ticks(x_min, x_max, n=8)
        for t in xticks:
            x = x_to_px(t)
            canvas.create_line(x, plot_top, x, plot_bottom, fill="#e6e6e6")
            canvas.create_text(x, plot_bottom + 15, text=str(int(t)), fill="#000000", font=("Segoe UI", 9))

        # Left Y ticks
        yticksL = nice_ticks(yL_min, yL_max, n=6)
        for t in yticksL:
            y = yL_to_py(t)
            canvas.create_line(plot_left, y, plot_right, y, fill="#f0f0f0")
            canvas.create_text(plot_left - 8, y, text=f"{t:.1f}", fill="#000000", font=("Segoe UI", 9), anchor="e")

        # Right Y ticks
        yticksR = nice_ticks(yR_min, yR_max, n=6)
        for t in yticksR:
            y = yR_to_py(t)
            canvas.create_text(plot_right + 8, y, text=f"{t:.2f}", fill="#000000", font=("Segoe UI", 9), anchor="w")

        # Labels
        canvas.create_text((plot_left + plot_right) / 2, 15, text=title, fill="#000000",
                           font=("Segoe UI", 12, "bold"))
        canvas.create_text((plot_left + plot_right) / 2, plot_bottom + 40, text="Index", fill="#000000",
                           font=("Segoe UI", 10))
        canvas.create_text(20, (plot_top + plot_bottom) / 2, text="Respiration / Position", fill="#000000",
                           font=("Segoe UI", 10), angle=90)
        canvas.create_text(w - 20, (plot_top + plot_bottom) / 2, text=f"Movement x{scale_secondary}",
                           fill="#000000", font=("Segoe UI", 10), angle=270)

        # Draw polyline helper
        def draw_series(xs, ys, color, ymap, is_secondary=False):
            pts = []
            for xi, yi in zip(xs, ys):
                pts.append(x_to_px(xi))
                pts.append(ymap(yi))
            if len(pts) >= 4:
                if is_secondary:
                    canvas.create_line(*pts, fill=color, width=2, dash=(10, 8))
                else:
                    canvas.create_line(*pts, fill=color, width=2)

        # Plot series
        for (name, s), col in zip(left_series, colors_left):
            draw_series(index, s, col, yL_to_py, is_secondary=False)
        for (name, s), col in zip(right_series, colors_right):
            draw_series(index, s, col, yR_to_py, is_secondary=True)

        # Legend
        leg_y = plot_bottom + 55
        x = plot_left
        for (name, _), col in zip(left_series, colors_left):
            canvas.create_line(x, leg_y, x + 25, leg_y, fill=col, width=3)
            canvas.create_text(x + 32, leg_y, text=name, anchor="w", font=("Segoe UI", 9))
            x += 32 + 10 + (7 * len(name))
        x += 30
        for (name, _), col in zip(right_series, colors_right):
            canvas.create_line(x, leg_y, x + 25, leg_y, fill=col, width=1, dash=(12, 8))
            canvas.create_text(x + 32, leg_y, text=name, anchor="w", font=("Segoe UI", 9))
            x += 32 + 10 + (7 * len(name))

    # Redraw on resize
    canvas.bind("<Configure>", lambda e: redraw())
    redraw()

    root.mainloop()
