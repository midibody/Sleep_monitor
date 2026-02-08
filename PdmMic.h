#ifdef DETECT_SNORE
#include <PDM.h>

//--------------= TECHNICAL PARAMETERS ----------------
#define MIC_BUFFER_SIZE 512
#define MIC_BUFFER_SIZE_WORDS (MIC_BUFFER_SIZE / 2)
#define MIC_GAIN  50
#define SAMPLE_RATE 16000.0f
#define RMS_DEFAULT_SAMPLES  256

static int16_t micBuffer[(MIC_BUFFER_SIZE/2)+1]; // we store int of 2 bytes
static volatile bool micDataReady = false;
static uint16_t cMicData;

uint32_t millisSnoreIgnoreMicAfterMove= 0; 
uint32_t millisSnoreIgnoreMicAfterSnore= 0;
uint32_t lastSnoringMillis = 0;
uint16_t cSnoringToTriggerAlert = 0;
uint16_t volDynThrHigh, volDynThrLow = 0;
bool     fDataProcessed = true;

//*****************************************
const int numReadings  = 10;

typedef struct {
  int aReadings[numReadings];
  long total;
  int readIndex;
  int count;              // 0..numReadings
  unsigned long previousVal;
} T_Variance;

T_Variance tVariance;

//*********************************************************
void computeVariance (int v)
{
  int16_t average;
  int16_t c,i;
  int16_t variation;
  int16_t diff;


     tVariance.total -= tVariance.aReadings[tVariance.readIndex];
    tVariance.aReadings[tVariance.readIndex] = v;
    tVariance.total += v;

    tVariance.readIndex++;
    if (tVariance.readIndex >= numReadings) tVariance.readIndex = 0;

    if (tVariance.count < numReadings) tVariance.count++;

    c = tVariance.count;
    average = (uint16_t)(tVariance.total / c);

    for (variation = 0, i=0 ; i < c ; i++)
    {
      if (tVariance.aReadings[i] > average) diff = tVariance.aReadings[i] - average;
      else diff = average - tVariance.aReadings[i];
      variation += diff;
    }
    variation /= c;
    
    if (fPrintDebug)
        {
          sprintf(aDebug, "volume=%d average=%d. variation = %d. volDynThrLow=%d volDynThrLow=%d" , v, average, variation , volDynThrLow, volDynThrHigh);
          serialPrintln(aDebug);
        }
    
    if (variation < VOL_THR_VARIANCE) // seems quite, we can adjust thresholds
      {
        //if (average < VOL_THR_HIGH) volDynThrHigh = VOL_THR_HIGH - average;
        //if (average < VOL_THR_LOW) volDynThrLow = VOL_THR_LOW - average;
        volDynThrHigh = VOL_THR_HIGH + average;
        volDynThrLow = VOL_THR_LOW + average;
      }
}

//*******************************
void varianceInit(T_Variance *p)
//******************************
{
  memset(p, 0, sizeof(*p));
}



//********************
void onPDMdataINITIAL()
//*********************
// Callback called by PDM driver 
{
uint16_t bytesAvailable;

bytesAvailable = PDM.available();

if (bytesAvailable > 0) 
  {
    cMicData = bytesAvailable/2;
    PDM.read(micBuffer, bytesAvailable );
    micDataReady = true;
  }
else cMicData = 0;
}

//**************
void onPDMdata()
//**************
{
uint16_t bytesAvailable;

    // if previous data not yet processed, we ignore this call back to not overwrite data buffer
    if (!fDataProcessed) {
        bytesAvailable = PDM.available();
        if (bytesAvailable > 0) {
            PDM.read(NULL, bytesAvailable); // discard
        }
        return;
    }

    bytesAvailable = PDM.available();

    if (bytesAvailable > 0)
    {
        cMicData = bytesAvailable / 2;
        PDM.read(micBuffer, bytesAvailable);

        micDataReady   = true;
        fDataProcessed = false;   // lock buffer
    }
    else {
        cMicData = 0;
    }
}

//************
void micInit()
//***********
{
  PDM.setBufferSize (MIC_BUFFER_SIZE); // # bytes . Default in PDM lib = 512
  PDM.onReceive (onPDMdata);          // IRQ called function
  PDM.begin (1, SAMPLE_RATE);         // 1 channel mono
  PDM.setGain (MIC_GAIN);             // MUST Be put after PDM.begin ... 80 max but a lot of noise

}

//***************************************
bool readMicRMS(uint16_t *pVolumeReturned)
//*************************************** 
{
  uint16_t rms;
  int16_t integer16;
  uint16_t absVal;
  uint32_t vTotal=0;
  int16_t filtered;
  uint16_t i, iMean;
  int16_t cMean;
  int32_t mean;

  *pVolumeReturned = 0;

#define DC_OFFSET_MEAN_SIZE 128 // 64: reduces offset when no signal, but cuts at around 130hz
int16_t tOffsetsMean[ (MIC_BUFFER_SIZE_WORDS / DC_OFFSET_MEAN_SIZE)+2 ];

  if (cMicData) 
    {
      // compute DC offset
      //----------------------------
      
      for (mean = 0, i = 0, iMean = 0, cMean = 0; i < cMicData; i++, cMean++)
        {        
         if (i && (i % (DC_OFFSET_MEAN_SIZE) == 0) ) 
          {         
            tOffsetsMean[iMean] = mean/cMean; 
            //sprintf (aDebug, "Storage mean-> i:%5d\t mean:%5d\t cMean:%5d\t tOffsetsMean[%d]:%5d", i, mean, cMean, iMean, tOffsetsMean[iMean]); serialPrintln(aDebug);
            mean = 0; cMean = 0; iMean++; 
          }

        mean += micBuffer[i];
        //sprintf(aDebug , "i:%3d, micBuffer[i]:%5d, iMean:%5d, cMean:%5d , tOffsetsMean[iMean]:%5d" , i, micBuffer[i], iMean, cMean, tOffsetsMean[iMean] ); serialPrintln (aDebug);  
        }
      tOffsetsMean[iMean] = mean/cMean;

      //----------- compute overall volume over period 
      for (i = 0 , iMean = 0; i < cMicData; i++) 
        {
          if (i && (i % (DC_OFFSET_MEAN_SIZE) == 0) ) iMean++;
          
          integer16 = micBuffer[i] - tOffsetsMean[iMean];
          absVal = (integer16 > 0)? integer16 : -integer16;
          vTotal+= absVal;

          // -------- Low pass filter NOT USED--------
          //alpha = 0.5f;
          //lpfState = lpfState + alpha * ((float)absVal - lpfState);
          //integer16 = (int16_t)lpfState;

          //sprintf(aDebug, "min:-100\tzero:0\tMax:300\tm:%05d\tf:%05d\tlowPass:%05d\tabs:%05d\tmean:%05d\tiMean:%05d\tcMicData:%5d", micBuffer[i] , filtered, integer16, absVal,tOffsetsMean[iMean] , iMean, cMicData);
          //sprintf (aDebug , "m:-100\tz:0\tM:300\tf:%d\tlp:%d", filtered, integer16);
          //serialPrintln (aDebug);               
      }
      
      *pVolumeReturned = (uint16_t) (vTotal / cMicData);
      cMicData = 0;
      fDataProcessed= true; // IRQ function can fill buffer with new data
      return true;
    } //if cMicData

return false;
}

// ------------------------------------------------------------
// Snore detector using 100 ms "volumeAverage" frames + motion gate
// Motion is derived from gyro norm: gNorm = norm3(gx_dps, gy_dps, gz_dps)
// If motion is above a threshold, snore detection is inhibited.
// ------------------------------------------------------------

typedef struct {
  // Adaptive noise floor (EWMA)
  uint32_t noiseFloor;     // estimated background level
  uint32_t noiseDev;       // EWMA of squared error (rough variance proxy)

  // Detection state
  uint16_t  aboveCount;     // consecutive frames above high threshold
  uint16_t  belowCount;     // consecutive frames below low threshold
  bool     inEvent;        // currently inside a snore event
  uint16_t refractory;     // frames remaining to prevent retrigger

  // Motion gate
  uint16_t motionHold;     // frames remaining to keep detection inhibited after motion
} SnoreDetector;

SnoreDetector snoreInfo;

// Helper: absolute value for int32, safe conversion to uint32
static inline uint32_t u32_abs_i32(int32_t x) { return (uint32_t)(x < 0 ? -x : x); }

// Call once at boot
//***************
void snoreInit()
//**************
{

  snoreInfo.aboveCount  = 0;
  snoreInfo.belowCount  = 0;
  snoreInfo.inEvent     = false;
  snoreInfo.refractory  = 0;
  snoreInfo.motionHold  = 0;
}

//**********************************
void checkSnoringAlert(uint32_t now)
//**********************************
{
cSnoringToTriggerAlert++;

if (lastSnoringMillis && (now - lastSnoringMillis > MAX_ELAPSE_TIME_BETWEEN_SNORES_FOR_ALERTS ) )
 {
  lastSnoringMillis = 0;
  cSnoringToTriggerAlert = 0;
 }
if (cSnoringToTriggerAlert >= COUNTER_SNORING_TO_TRIGGER_ALERT)
  {
    cSnoreAlerts++;
    if (ALERTS_SNORING)
      {
        motorStart(500,MOTOR_SPEED_HIGH);  delay(200);  motorStart(500,MOTOR_SPEED_HIGH);
        if (fPrintDebug) serialPrintln("* * * SNORING VIBRATOR ALERT * * * ");
      }
    cSnoringToTriggerAlert = 0;
  }

lastSnoringMillis = now; 
}

//******************************
bool snoreDetectWithMotionGate()
//******************************
{
uint16_t volume;
uint32_t now;

  if (!readMicRMS (&volume) ) return false;

  computeVariance(volume);
  
  if (fPlotMic) { sprintf(aDebug, "rms:%05d\tthrHigh:%05d\tthrLow:%05d", volume, VOL_THR_HIGH, VOL_THR_LOW);      serialPrintln(aDebug);    }

  now = millis();
  if (millisSnoreIgnoreMicAfterMove && (now < millisSnoreIgnoreMicAfterMove + SNORE_TIMER_AFTER_MOVE) )  return false; // we are still in the refractory period after a body motion, we ignore mic
  if (millisSnoreIgnoreMicAfterSnore && (now < millisSnoreIgnoreMicAfterSnore + SNORE_TIMER_AFTER_SNORE) )  return false;

  //if (snoreInfo.refractory > 0) {  snoreInfo.refractory--;  } // works only if we wake up code every 100ms...

  // Motion gate
  float gNorm = norm3(gx_dps, gy_dps, gz_dps);
  bool motionNow = (gNorm >= MOTION_THRESHOLD); // movement based on gyro

  if (motionNow) // move: ignore sound as body makes noise and we risk counting snoring 
    {
      //snoreInfo.motionHold = MOTION_HOLD_FRAMES;
       //if (snoreInfo.refractory < 5) snoreInfo.refractory = 5;
      //snoreInfo.refractory = SNORING_REFRACTORING_DURATION_;

      snoreInfo.inEvent = false;
      snoreInfo.aboveCount = 0;
      snoreInfo.belowCount = 0;
      millisSnoreIgnoreMicAfterMove = now;
      return false;
     
    } 


  // counts volume above & bellow thresholds
  if (!snoreInfo.inEvent) // no snoring detected
    {
      if ( volume >= volDynThrHigh ) snoreInfo.aboveCount++;
      else if (snoreInfo.aboveCount) snoreInfo.aboveCount--; // to better handle oscillations around threshold

      if (snoreInfo.aboveCount >= MIN_ON_FRAMES) 
        {
          snoreInfo.inEvent = true;
          snoreInfo.belowCount = 0;
          snoreInfo.aboveCount = 0;

          if (fPrintDebug)  serialPrintln("* * SNORING * * ");  
          rgbLedFlash(RGB_LED_RED, 1,1); 
          cSnores++;
          checkSnoringAlert(now);
          
          return true; // * * * * snoring detected * * * * 
        }
      return false;

  } 
  else // snoreInfo.inEvent = true; snoring was previsouly detected
    {
      if (volume <= volDynThrLow) snoreInfo.belowCount++;
      else snoreInfo.belowCount = 0; 

      if (snoreInfo.belowCount >= MIN_OFF_FRAMES) 
        { // we block detection during REFRACT_FRAMES period
          snoreInfo.inEvent = false;
          snoreInfo.belowCount = 0;
          snoreInfo.aboveCount = 0;
          millisSnoreIgnoreMicAfterSnore = now;
        }
    return false;
  }
}

#endif