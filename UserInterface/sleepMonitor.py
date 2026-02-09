# -*- coding: utf-8 -*-

#to generate .exe standalone file: pyinstaller --onefile --name SleepMonitor sleepMonitor.py

# nedd to install serial: python -m pip install pyserial
# need to install pyinstaller before: python -m pip install --upgrade pyinstaller

import serial
import serial.tools.list_ports
import time
#import matplotlib.pyplot as plt
import csv
from io import StringIO
import sys
from tkinterLib import plot_from_semicolon_data_tk

"""---------------------------
 to build: 
    pip install pyserial
"""

VERSION ="1.0.0"    
BAUDRATE = 115200          # adapter si besoin
READ_TIMEOUT = 0.8         # timeout court pour polling
END_SILENCE = 0.2          # fin de reception apres 0.5 s sans data

cPorts = 0

#----------------------
"""
def plot_from_semicolon_data(lines, scale_secondary=1):
    # lines = list[str]
    import matplotlib.pyplot as plt
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
        accel.append( (float(row["AccelMax"])/5000) * scale_secondary)
        position.append(10*float(row["Position"]))
        bpm.append(float(row["BreathPerMinute"]))
        bmax.append(float(row["BreathMax"]))
        cSnores.append(float(row["cSnores"]) * scale_secondary)
        csmall.append( (float(row["cSmallMoves"])/10) * scale_secondary)

    #fig, ax1 = plt.subplots()
    fig, ax1 = plt.subplots(figsize=(18, 10))

    ax1.plot(index, position, label="Position")
    ax1.plot(index, bpm, label="BreathPerMinute")
    ax1.plot(index, bmax, label="BreathMax")
    ax1.set_xlabel("Index")
    ax1.set_ylabel("Respiration / Position")
    ax1.legend(loc="upper left")
    ax1.grid(True)

    ax2 = ax1.twinx()
    ax2.plot(index, accel, "--", label="AccelMax (scaled)")
    ax2.plot(index, cSnores, "--", label="cSnores (scaled)")
    ax2.plot(index, csmall, "--", label="cSmallMoves (scaled)")
    ax2.set_ylabel(f"Movement x{scale_secondary}")
    ax2.legend(loc="upper right")

    plt.tight_layout()
    #plt.get_current_fig_manager().full_screen_toggle()
    plt.show()
"""

#------------------------
def list_arduino_port():
    global cPorts
    ports = serial.tools.list_ports.comports()
 
    for p in ports:
        print(f"{cPorts}: {p.description}")
        cPorts = cPorts+1

#----------------------
def serialPortGetDeviceName( iPort):
    global cPorts
    ports = serial.tools.list_ports.comports()
    i = 0
    for p in ports:
        if (iPort == i):
            return p.device
        i += 1
    
    return None

#----------------------
def find_arduino_port():
    ports = serial.tools.list_ports.comports()
 
    for p in ports:
        
        if ("Arduino" in p.description or
            "USB" in p.description or
            "(COM" in p.description or
            p.vid is not None):
            return p.device
    return None

#-----------------------
def open_serial_with_handling(port, baudrate, timeout):
    try:
        ser = serial.Serial(port=port, baudrate=baudrate, timeout=timeout)
        return ser

    except serial.SerialException as e:
        msg = str(e).lower()

        # Autres erreurs d'ouverture (port inexistant, etc.)
        print(f"* * * Cannot access to the device {port}. Check if another program instance is running * * * \n  Details: {e}")
        sys.exit(0)

#-----------------------
def read_table():
    # purge buffers
    #ser.reset_input_buffer()
    #ser.reset_output_buffer()

    received_lines = []
    last_rx_time = time.time()

    while True:
        data = ser.readline()
        if data:
            line = data.decode(errors="replace").rstrip()
            received_lines.append(line)
            last_rx_time = time.time()
        else:
            if time.time() - last_rx_time >= END_SILENCE:
                break

    return received_lines

#---------------------------
if __name__ == "__main__":

#    with serial.Serial(port, BAUDRATE, timeout=READ_TIMEOUT) as ser:
    print (f"Sleep Monitor version {VERSION}\n\nUSB Ports available:")
    #while (cPorts ==0):
    list_arduino_port()

    print("")
    
    #port = find_arduino_port()
    #if port is None:
    if (cPorts == 0):
        raise RuntimeError("No board detected")

    #ser = serial.Serial( port=port, baudrate=BAUDRATE, timeout=READ_TIMEOUT)
    if (cPorts > 1):
        print ("\n=> Type the USB port index (0,1,...) you want to use: ")
        iPort = int(input("> "))
        port = serialPortGetDeviceName(iPort)
    else:
        port = serialPortGetDeviceName(0)
    
    ser = open_serial_with_handling(port=port, baudrate=BAUDRATE, timeout=READ_TIMEOUT)
    if ser is None:
        sys.exit(1)

    print (f"Connected to board on USB/Serial ({port})\n");

    print ("To load data from monitoring device and display values type 'p + ENTER'. To Quit type'z + ENTER'. For more commands type '? + ENTER'")
    while (1):

        cmd = input("> ").strip().lower()          # read until ENTER, removes end line
        if not cmd:
            continue

        ser.reset_input_buffer()
        ser.write (cmd.encode("utf-8")) #ser.write(b"?\n")

        lines = read_table()
        if not lines: #strange bug, we retry
            ser.reset_input_buffer()
            ser.write (cmd.encode("utf-8")) 
            lines = read_table() #retry...

        for l in lines:
            print(l)

        if cmd and cmd[0] == '?':
            print (" z: QUIT PROGRAM")

        if cmd and (cmd[0] == 'c' or cmd[0] == 'p'): #save data to dataImport.csv file and display graph

            with open("../dataImport.csv", "w", encoding="utf-8") as f:
                for line in lines:
                    f.write(line.rstrip("\n") + "\n")

            #plot_from_semicolon_data(lines, 1)
            plot_from_semicolon_data_tk(lines, 1, "Sleep Monitoring")
        if cmd and cmd[0] == 'z':
            sys.exit(0)


