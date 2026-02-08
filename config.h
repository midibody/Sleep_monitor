//************************
//config.h
//-------------------------------------------------------------------------------------------
// PROD RELEASES
//-------------------------------------------------------------------------------------------
//  DATE / TIME         Version   Updates
//-------------------------------------------------------------------------------------------
// Jan 31/2026 14:08    1.0.0     Powered by BAT. Stores Bat voltage into Flash Log. No IMU disable/enable
// Feb 1 /2026 10:00    1.0.1     correct memeory overflow if more than 1000 records
// Feb 6/ 2026 18:49    1.1.0     Add snoring recording and alerts
//-------------------------------------------------------------------------------------------
// POWER USAGE: if IMU is compiled, impossible to go below 0.7 - 0.57mA even when system is off even if serial is not called. 
// MAIN PARAMETERS

// version: Major evol| Minor evol | bug fixes
#define PROGRAM_VERSION_COMMENTS "V1.1.0"  // Label logged in Flash at startup

#define USE_USB // => !!!!! remove for Prod, to reduce power 

#define USE_IMU // remove for tests of IMU impact on power

#define DETECT_SNORE    //=> Enable to test PDM Mic

uint16_t  PROGRAM_WAKEUP_PERIOD =100;// WARNING: sensitive parameter affecting filters, overall program behaviour. dont change except for debug purpose, or impacts potential on many stuff

// DEBUG LEVELS
bool fEnableVibrations= true; // put false to debug or interface with the board when it is over a table to avoid painful false vibration alerts
bool fPrintDebug      = false;
bool fPlotDebug       = false;
bool fMaxTrace        = false;
bool fLogPower        = false;
bool fPlotMic         = false;

//---------------- SNORING - PARAMETERS ---------------

#define SNORE_TIMER_AFTER_SNORE                     1000 // ms. we ignore snore if one has just finished, in case
#define SNORE_TIMER_AFTER_MOVE                      2000 // ms. time while we ignore mic after a move
#define MAX_ELAPSE_TIME_BETWEEN_SNORES_FOR_ALERTS   10000 // ms. if longer than this timing, we reset counter of alerts for snoring
#define COUNTER_SNORING_TO_TRIGGER_ALERT            3   // amount of snores within a period before alert, each being separated by less than MAX_ELAPSE_TIME_BETWEEN_SNORES_FOR_ALERTS

const uint16_t  VOL_THR_HIGH      = 10;    // detection of snoring
const uint16_t  VOL_THR_LOW       = 5;    // back to normal noise
const uint16_t  VOL_THR_VARIANCE  = 3; // if variance measured during a period is smaller then we can consider that there is no noise and we adjust low and high thresholds for snoring detection
 
const uint8_t  MIN_ON_FRAMES      = 2;   // 200 ms above high threshold to trigger
const uint8_t  MIN_OFF_FRAMES     = 2;   // 200 ms below low threshold to end event
//const uint16_t REFRACT_FRAMES     = 15;  // 1.5 s retrigger lockout after snoring end

const float    MOTION_THRESHOLD    = 70.0f;   // for snoring cancellation / ignore when person is moving


//--------------- ON BACK POSITION - PARAMETERS -------

#define ALERTS_SNORING  false //doesnt vibrate when set to false 

#define ALERTS_ON_BACK  true //doesnt vibrate when set to false 

#define TIMER_BACK_POSITION_STARTUP_DELAY (10 * 60*1000UL) // 10 mn - at startup we let the person on the back without triggering vibration alerts,as he/she is not yet supposed to sleep deeply

#define TIMER_BACK_POSITION_GRACE_PERIOD (5 *60*1000UL) // 5 mn - (5*60000UL)  // 60000UL = 1 mn.

#define TIMER_BACK_POSITION_REPETITION (15 * 1000UL) // every 15 seconds - Back position alarm repetition

#define COUNT_ALERTS_ON_BACK_BEFORE_MAX_VIBRATIONS  2 // we start with smooth vibrations , then try to wake up the sleeper with the max we can

#define TIMER_ADD_RECORD (60000UL) // 1 mn - 60000

#define TIMER_LED_BLINK 0 // Zero-> disable blink (1000*10) // every 10 sec

#define N_RECORDS_MAX 1000 // 1 record per minute during 12h = 720 records


// movement & breath parameters
#define BREATH_MIN_PERIOD_MS    2000     //after detecting a breath we ignore next ones during this period to avoid duplicates counting 
#define GYRO_TH_BIG_MOVE        250.0f   // was 150 ...to set fMoving = true;
#define ROTATION_COOLDOWN_MS    200    // duration in ms to ignore respiration after big rotation movement

//-------------------------------------------------------------------------------------------
// HW CONFIG
//-------------------------------------------------------------------------------------------

#define SWITCH_PIN      0 // !! Wake up on pin level does NOT work with pin 9...
#define MOTOR_PIN       1

#define VIN_ENABLE_PIN  2
#define VIN_VOLTAGE_PIN 3

#define VIN2_ENABLE_PIN  4
#define VIN2_VOLTAGE_PIN 5

#define MOTOR_SPEED_LOW     150
#define MOTOR_SPEED_NORMAL  200
#define MOTOR_SPEED_HIGH    255

// battery voltage test
#define VBAT_MINI (2.5f) // untrusted (and unused currenty) value - put higher value when real batteries


//***************************
// Codes for logging on flash

enum {LOG_TEXT, LOG_PROGRAM_STARTED, LOG_SWITCHOFF_BY_USER , LOG_POWER_CONSUMPTION, LOG_USB_REGISTRED };
const char* aLogText[]={"LOG_TEXT", "LOG_PROGRAM_STARTED" , "LOG_SWITCHOFF_BY_USER", "LOG_POWER_CONSUMPTION", "LOG_USB_REGISTRED"};

