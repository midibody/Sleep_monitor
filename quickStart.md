Quick start help
---
If you wish to play with the code, no need to solder all the components; you can just plug a Seeed nRf52840 Sense board to your arduino terminal via USB Serial.
To simulate the vibrator motor you can just plug a led to D1 with a resistor (1 to 50k depending on your led sensitivity).
To play with power management and shut down the board or wakeup the software, you can plug a push button to D0, the other side going to the ground.

To build the sketch, you need first to install:
* the 'Seeed nRF52 boards' from Seeed studio.
* the library for the IMU: 'Seeed Arduino LSM6DS3'.

Then, in menu Tools > Board, you need to select: Seeed NFR 52 board > Seeed XIAO NRF 52840 Sense

once your sketch is built and uploaded into the board, you can check if the code is running several ways:

* connecting shortly D0 to the ground (or pressing the connected push button) you should see the RGB led of the board blicking twice green.
 (if you connect D0 to ground more than 2 sec, the board goes system down, and to wake it up, you need to connect D0 shortly to ground.
 
There are several flags that can be used for debugging from the Arduino IDE terminal. Once connected to the terminal, to get help on the commands, type ?+ENTER
You should see this, if not there is a problem:

Commands: 
 a: read Flash structure
 c: display data in CSV
 d: Display data table
 e: erase flash (1st page)
 f: variable update (param: index+value). 'f?' to get var indexes 
 m: Memory dump 
 p: Read memory and display in .CSV format [optional: #storage index]
 r: Read memory [optional: #storage index]
 s: Search flash records 
 v: View loga
 w: write to flash
 x: reset RAM records
 
There is sub help to dynamically change some debugging variables ('f' command):
To get it, type f?+enter.
Another way is of course to change value in fie config.h and rebuild the sketch.

Variables reference index:
0:fPrintDebug, 
1:fPlotDebug
2:fMaxTrace
3:fLogPower
4:fPlotMic
5:fEnableVibrations
6:Program Wakeup Period(ms)

to change them from te terminal : type 'f' followed by the index, then value. Exemple: 'f11' to set fPlotDebug to true. 

Exemple - Monitor the breath detection with the Arduino plotter:
set the flag 'fPlotDebug' to 1 (command f11+ENTER)
You should see values in the terminaland such curves when launching the plotter:

* * GRAPH * * 

Exemple - Read the records stored each minut:
type command: 'd', to display this type of information:

====== Data Records ======
Rec | XLmax | Pos | Bpm | Bmax | cSnores | cSmallMoves
  13| 42652 |  3  |  17 |  10  |    10  |     2
  14|  3148 |  3  |  13 |   6  |     4  |     1
  15|  1150 |  3  |  13 |   7  |     4  |     1
  16|   665 |  3  |  11 |   7  |     2  |     0
  17|   702 |  3  |  12 |   6  |     2  |     0
  18|   575 |  3  |  14 |   5  |     7  |     0
  
Enjoy !
