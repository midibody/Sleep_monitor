#include "config.h"

//***************************************
// VARIABLES
uint32_t    programStartMs;
uint32_t    lastTimeAddRecord=0;
uint32_t    lastTimeLedBlink=0;
bool        fCycleStart = false;
uint16_t    counterToRead=-1;
uint16_t    acceleroMax = 0;
float       vPower = 0;
bool        usbPower = false;
uint16_t    cSnoreAlerts = 0;
char        aDebug[1024]; // to store debug text

//***************************************
#ifdef USE_USB 
//#include "Adafruit_SPIFlash.h"
#include <Wire.h> // otherwise, the IMU lib already embarks Wire.h that includes Serial functions
#endif

//#include "myFreeRTos.h"
#include "rtcLib.h"
#include "utilities.h"
#include "imuLib.h"
#include "dataStorage.h"
#include "powerMgt.h"

#ifdef DETECT_SNORE
#include "PdmMic.h"
#endif

//*******************
void longClickEvent()
//*******************
{
if (fPrintDebug) serialPrintln(">LONG CLICK -> SHUTDOWN");
  
storeDataRecords();
rgbLedFlash (RGB_LED_RED , 2 ,100);

motorStart(800,MOTOR_SPEED_NORMAL);  delay(200);  motorStart(800,MOTOR_SPEED_NORMAL); delay(200);  motorStart(800,MOTOR_SPEED_NORMAL);

delay(2000); // put it in case the button is pressed longer than motor duration and switch can be still pressed when calling systemOff (triggering system on again)...

prepareToSleep();

gotoSystemOff_WakeupOnSwitch();
}

//*******************
void shortClickEvent()
//*******************
{
if (fPrintDebug) serialPrintln(">SHORT CLICK-> I'M ALIVE");
rgbLedFlash (RGB_LED_GREEN , 2 ,100);

displayRecordsScreen();
motorStart(300,MOTOR_SPEED_NORMAL); //delay(300);  motorStart(500,MOTOR_SPEED_LOW); delay(300); motorStart(500,MOTOR_SPEED_NORMAL); delay(300); motorStart(500,MOTOR_SPEED_HIGH);
}

//********************
void checkSerialChar() 
//********************
{
char c;
int value = 0; // default we read first page
uint8_t color; 
bool fInitialPrintDebug;

if (readSerialInput() ) {

    switch (aInputText[0]) {

      case '?': // Help on commands
        serialPrintln ("Commands: \n a: read Flash structure\n c: display data in CSV\n d: Display data table\n e: erase flash (1st page)\n f: variable update (param: index+value). 'f?' to get var indexes \n m: Memory dump \n p: Read memory and display in .CSV format [optional: #storage index]\n r: Read memory [optional: #storage index]\n s: Search flash records \n v: View loga\n w: write to flash\n x: reset RAM records\n\n");
      break;

      case 'a': // Read all Flash structure without loading data - used to debug the flash ring buffer state and retrieve specific recordings with their header incremental counter
        fInitialPrintDebug = fPrintDebug;
        fPrintDebug = true; // to force the display in this specific case
        readDataRecords(false);//does NOT overwrite tabRecords with flash data
        fPrintDebug = fInitialPrintDebug;
      break;

      case 'r': // Read all memory and overwrite tabRecords with flash data - optional param beeing the incremental counter value

        if (aInputText[1]) { counterToRead = atoi(&aInputText[1]); serialPrint("counterToRead = ");       serialPrintln(counterToRead); }
        readDataRecords(true);
        counterToRead = -1;
      break;

      case 'p': // Read all memory and overwrite tabRecords with flash data, then displays data in csv format - optional param beeing the incremental counter value

        if (aInputText[1]) { counterToRead = atoi(&aInputText[1]); }
        readDataRecords(true);
        counterToRead = -1;
        displayRecordsForCSV();
      break;

      case 'd': // display data
        displayRecordsScreen();
      break;

      case 'c': // display data for CSV file structure
        displayRecordsForCSV();
      break;

      case 'e': // erase flash first page
        eraseFlashPagesBeforeWrite (FIRST_USER_FLASH_ADDR , 1); // to force delete of 1st Flash page only
      break;

      case 'f': // change flags / variables values
        changeVariable( &aInputText[1] );
      break;

      case 'i':
        addPowerRecord("User cmd");
      break;

      case 'l': //used to test RGB led colors

        switch (aInputText[1]) { case 'r': color = RGB_LED_RED; break; case 'g': color = RGB_LED_GREEN; break; case 'b': color = RGB_LED_BLUE; break; default: color = RGB_LED_WHITE; break; }
        if (aInputText[2] =='1') value = true; else value = false;
        rgbLedSet (color, value);
      break;

      case 'w': // Write - force store in flash of current records
        storeDataRecords();
      break;

      case 's':  // Search flash records
        searchFlashRecords();
      break;

      case 't': // simulate cycle of write with fake data
        testFlash();
      break;

      case 'v':
        displayLog();
      break;

      case 'm': // dump flash memory starting index = start of user flash> expect the Flash page index number after 'm'. Eg: m3

        if (aInputText[1]) value = atoi(&aInputText[1]);
        dump_memory(FIRST_USER_FLASH_ADDR + (value * FLASH_PAGE_SIZE), FLASH_PAGE_SIZE);
      break;

      case 'x': // resetRecords
        
        cRecords = 0;
        if (fPrintDebug) serialPrintln("Records Reset");
      break;
    }
  }
}

extern void wakeUp();

//***************************************
void timerEvent() 
//***************************************
{
  uint32_t now,t0;
  uint16_t v;

  // restarts all required modules
  wakeUp();


  if (PROGRAM_WAKEUP_PERIOD >1000) // We consider that we are in test of power mode; we force cpu to work to measure current
   {
      rgbLedFlash(RGB_LED_WHITE, 2,200);
      
      // to debug power usage we fore the prg to loop x sec
      t0= millis();
      delay(3000);
      //while (millis() - t0 < 3000) delay(1); 
   }

  if (fSwitchedPressed(v))
   {
    if (v==LONG_CLICK) longClickEvent();
    else if (v==SHORT_CLICK) shortClickEvent();
   }

  checkSerialChar();
  checkPosAndMovements();
  checkBreathAccelGyro();
  
  #ifdef DETECT_SNORE
  snoreDetectWithMotionGate();
  #endif

  now = millis();

  if ( fPrintDebug && (TIMER_LED_BLINK && (now - lastTimeLedBlink > TIMER_LED_BLINK )) ) 
   {
    lastTimeLedBlink = now;
    flashLed(1,0);
   }

  if (now - lastTimeAddRecord > TIMER_ADD_RECORD) 
   {
    if (lastBreathFromPreviousPeriod > breathMax) 
      {
       breathMax = lastBreathFromPreviousPeriod;
       lastBreathFromPreviousPeriod =0;
      }

    addDataRecord();

    breathMax         = 0;
    lastTimeAddRecord = now; 
    fCycleStart       = true;
    breathCount       = 0;
    cMoves            = 0;
    cSmallMoves       = 0;
    cSnores           = 0;
   }

  else fCycleStart = false;

  // DISABLE unused features during sleep
  prepareToSleep();
}

//***************************************
void setup()
//***************************************
{
unsigned long t0;

vPower = readVBAT();
if (fPrintDebug)  {   serialPrint ("Power (V_BAT)= "); serialPrintln (vPower,3);  }

sprintf (aDebug , "Build %s %s - %s. Bat=%dmV" , __DATE__, __TIME__, PROGRAM_VERSION_COMMENTS, (uint16_t) (vPower *1000));

#ifdef USE_USB
  strcat(aDebug , " USB/Serial ");
#endif
#ifdef USE_IMU
  strcat(aDebug , " IMU ");
#endif
#ifdef DETECT_SNORE
  strcat(aDebug , " Snore ");
#endif

if (fPrintDebug) strcat (aDebug , " PrintDebug ");
if (fPlotDebug) strcat (aDebug , " PlotDebug ");
if (fMaxTrace) strcat (aDebug , " MaxTrace ");
if (fLogPower) strcat (aDebug , " LogPower ");
if (fPlotMic) strcat (aDebug, " Plot Mic");

addLogRecord(LOGTYPE_SUCCESS , LOG_PROGRAM_STARTED , aDebug);
  
addPowerRecord("Setup-Startup");
addPowerRecord("Setup-Startup-dev chk");

  // 2 vibrations at startup
motorStart(500,MOTOR_SPEED_HIGH);  delay(300);  motorStart(500,MOTOR_SPEED_HIGH);

//-----------
#ifdef USE_USB // We initiate the serial port

  //usbPower = (NRF_POWER->USBREGSTATUS & POWER_USBREGSTATUS_VBUSDETECT_Msk); up if VIN voltage, even if no usb connected
  usbPower = (NRF_USBD->USBADDR != 0);

  if (usbPower) // USB registred
    {
      //addLogRecord(LOGTYPE_INFO , LOG_USB_REGISTRED , NULL);
      rgbLedFlash (RGB_LED_BLUE , 2, 200);
      Serial.begin(115200);
      t0= millis();
      
      while (!Serial && millis() - t0 < 2000) delay(10); // waits max 2 seconds. Why??
      if ((bool) Serial) {    if (fPrintDebug) serialPrintln("USB/Serial port is active"); }
    }

#else // irrelevant to trace as no serial connected
  if (fPrintDebug)  fPrintDebug = false;
  if (fPlotDebug)   fPlotDebug = false;
  if (fMaxTrace)    fMaxTrace = false;
  if (fPlotMic)     fPlotMic = false;
#endif

  if (fPrintDebug) serialPrintln("BOARD STARTED");
  
  //if (fPrintDebug)  {  serialPrint("isSoftDeviceEnabled ->"); serialPrintln(isSoftDeviceEnabled());  }
 
  pinMode (MOTOR_PIN, OUTPUT);
  analogWrite (MOTOR_PIN, 0);

  pinMode (SWITCH_PIN, INPUT_PULLUP);
  pinMode (LED_BUILTIN, OUTPUT);

  rgbLedInit(); 

/* code to automatically stop the device if battery is too low
  if ( v < VBAT_MINI)
    {
      if (fPrintDebug)
        {
          serialPrint ("WARNING: Battery voltage is too low. Switch systemOff. Voltage = "); serialPrintln (v,3);
        }
      digitalWrite(LED_BUILTIN, LOW); motorStart(200,150);  digitalWrite(LED_BUILTIN, HIGH); delay(200); digitalWrite(LED_BUILTIN, LOW); motorStart(200,150);digitalWrite(LED_BUILTIN, HIGH);
      delay (5000); // we let time in case we want to update the software before is goes to systemoff (no way to update the bord when it is systemOff)
      gotoSystemOff_WakeupOnSwitch ();
    }
*/
#ifdef DETECT_SNORE
  micInit();
  snoreInit();
#endif

  initIMU();

  rgbLedFlash(RGB_LED_GREEN, 2,200);
  calibrateGyro();

  initPowerSettings(); // cut all useless stuff
  
  setTickPeriodMs(PROGRAM_WAKEUP_PERIOD); // ms
  rtc2_start();

  programStartMs = 0;
}

//******************************
void changeVariable(char *pText)
//******************************
//Syntaxe command: f+X+(0 or 1). Eg: f21 to switch fLogPower to true.
{
uint16_t s = strlen(pText);
uint16_t varNumber;
uint16_t varValue;
char label[32];

if ( (pText[0] != '?') && (s<2) ) return;

switch (pText[0]) {
    case '?':    serialPrintln ("Variables reference index:\n0:fPrintDebug, \n1:fPlotDebug\n2:fMaxTrace\n3:fLogPower\n4:fPlotMic\n5:fEnableVibrations\n6:Program Wakeup Period(ms)\n"); break;
    case '0':     fPrintDebug = ( pText[1] =='0')? false:true; strcpy(label, "fPrintDebug"); break;
    case '1':     fPlotDebug =  ( pText[1] =='0')? false:true; strcpy(label, "fPlotDebug"); break;
    case '2':     fMaxTrace =  ( pText[1] =='0')? false:true; strcpy(label, "fMaxTrace"); break;
    case '3':     fLogPower =  ( pText[1] =='0')? false:true; strcpy(label, "fLogPower"); break;    
    case '4':     fPlotMic =  ( pText[1] =='0')? false:true; strcpy(label, "fPlotMic"); break;
    case '5':     fEnableVibrations =  ( pText[1] =='0')? false:true; strcpy(label, "fEnableVibrations"); break;
    case '6':     
      { 
        PROGRAM_WAKEUP_PERIOD = atoi(pText+1); 
        setTickPeriodMs(PROGRAM_WAKEUP_PERIOD);
        strcpy(label, "Program Wakeup Period"); 
      }
      break;
  }

if (pText[0] != '?') sprintf(aDebug , "%s = %s\n", label, pText+1); serialPrintln(aDebug);
}

//**********
void loop()
//**********
{
if (checkRtcEvent()) timerEvent();

sleep_until_irq();
}

//****************************
void initPowerSettings()
{

  //microphone_hw_off();
  //uart1_hw_off(); // no change in power...

  NRF_SAADC->ENABLE = 0; // stops ADC converter - could be done once at startup, and activated only when reading vBat or Vin

  NRF_RADIO->POWER = 0;   // Disable 2.4GHz RADIO block (BLE/ESB/anything) ---useless, it seems current is equal with this line in comments

  if ( usbPower == false ) // cut all USB - DANGEROUS!!
    {
      NRF_USBD->USBPULLUP = 0;   // Disconnect D+ pull-up (logical USB detach from host)  // CONNECT = Disabled

      // 2) Optional: put USBD in low-power mode (if it was suspended)
      // NRF_USBD->LOWPOWER = 1;    // Enable USBD low-power mode

      // 3) Disable the USBD peripheral . Note: ensure no EasyDMA transfer is ongoing before disabling
      NRF_USBD->ENABLE = 0;         // USBD disabled, PHY/regulator off
    }
  
}

//****************************
void prepareToSleep()
{
  
 //if (fPrintDebug)   {    sprintf (aDebug, "NRF_SAADC->ENABLE = %d , NRF_RADIO->POWER =%d ", NRF_SAADC->ENABLE , NRF_RADIO->POWER);    serialPrintln(aDebug);   }

  // --- 2) Stop HFCLK (if any lib started it) --- no way to really stop it in Arduino anv, and the PDM Mic also uses it
  //NRF_CLOCK->TASKS_HFCLKSTOP = 1;

  // --- 3) Select low-power sub mode in System ON ---
  NRF_POWER->TASKS_LOWPWR = 1;

disableIMU();

}

//************************
void wakeUp()
{
// tentatives not effective...
enableIMU();
}
