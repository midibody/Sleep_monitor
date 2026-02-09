Quick start help
---
If you wish **to play with the code**, no need to solder all the components; you can just plug a Seeed nRf52840 Sense board to your arduino terminal via USB Serial.
To simulate the vibrator motor you can just plug a led to D1 with a resistor (1 to 50k depending on your led sensitivity).
To play with power management and shut down the board or wakeup the software, you can plug a push button to D0, the other side going to the ground.

**To build the sketch**, you need first to install:
* the 'Seeed nRF52 boards' from Seeed studio.
* the library for the IMU: 'Seeed Arduino LSM6DS3'.

Then, in menu Tools > Board, you need to select: Seeed NFR 52 board > Seeed XIAO NRF 52840 Sense

Checking the program is running:
---
**once your sketch is built and uploaded into the board**, you can check if the code is running several ways:
connecting shortly D0 to the ground (or pressing the connected push button) you should see the RGB led of the board blicking twice green.
 (if you connect D0 to ground more than 2 sec, the board goes system down, and to wake it up, you need to connect D0 shortly to ground.

Once the code is running you can send commands to the board from the Arduino terminal.

Using the commands to monitor and control he program:
---
To get help on the commands, type: ?+ENTER
You should see this, if not, there is a problem:

**Commands: **
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
 
Sub help to dynamically change some debugging variables ('f' command):
---
To get it, type f?+enter.
Another way is of course to change variable values directly in file 'config.h' and rebuild the sketch.

**Variables reference index:**
0:fPrintDebug, 
1:fPlotDebug
2:fMaxTrace
3:fLogPower
4:fPlotMic
5:fEnableVibrations
6:Program Wakeup Period(ms)

to change them direclty from the terminal : type 'f' followed by the index, then value. Exemple: 'f11' to set fPlotDebug to true. 

Monitor the breath detection with the Arduino plotter:
---
set the flag 'fPlotDebug' to 1 (command f11+ENTER)
You should see values in the terminal window, and such curves when launching the plotter:

<img width="678" height="384" alt="image" src="https://github.com/user-attachments/assets/6490f41a-3bd0-4e73-a677-b65ca7fdc17a" />


Display the records stored each minut:**
---
type command: 'd', to display this type of information:

====== Data Records ======

_Rec | XLmax | Pos | Bpm | Bmax | cSnores | cSmallMoves

13| 42652 |  3  |  17 |  10  |    10  |     2

14|  3148 |  3  |  13 |   6  |     4  |     1

15|  1150 |  3  |  13 |   7  |     4  |     1

16|   665 |  3  |  11 |   7  |     2  |     0_

17|   702 |  3  |  12 |   6  |     2  |     0

18|   575 |  3  |  14 |   5  |     7  |     0

Store the current session on the flash
---
When the program is running, each minut it adds a record into the tabRecords table in RAM. 
There are 2 ways to store in Flash the recording of the ram table:
- Use the command 'w'+ENTER from the terminal
- or long press on the push button connected to DO (or directly D) to ground more than 2 sec).

Display all sessions recored on Flash
---
using the **command 'a'**, you can display the 'kind of' file system on the flash:

_FileID| #Records | Voltage | storage@

131 |   23     | 4368    | 0x000ED030

132 |  427     | 3376    | 0x000ED0F4

133 |   10     | 3217    | 0x000EDE58

134 |   10     | 2692    | 0x000EDEB4

135 |   13     | 3967    | 0x000EDF10_

Read a session stored in Flash
---
- Using the command 'r' you read the latest recorded session.
- Using the command 'r'+FileID you load in memory the session you wish (eg: 'r131')

You can then display the content using command 'd'.

**_More to be added..._**

**Enjoy !
**
