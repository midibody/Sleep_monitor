#define NO_CLICK 0
#define LONG_CLICK  1
#define SHORT_CLICK 2

// LED COLORS
#define RGB_LED_RED       0b0000001
#define RGB_LED_GREEN     0b0000010
#define RGB_LED_BLUE      0b0000100

#define RGB_LED_WHITE     0b0000111
#define RGB_LED_YELLOW    0b0000011
#define RGB_LED_CYAN      0b0000110
#define RGB_LED_PURPLE    0b0000101


static bool     gBtnLast      = false;
static bool     gBtnActive    = false;
static bool     gLongFired    = false;
static uint32_t gPressStartMs = 0;

static const uint32_t LONG_CLICK_MIN_MS = 1500;
static const uint32_t DEBOUNCE_MS       = 30;

char aInputText[64];

// Wrapper for functions Serial.print(ln)
#ifdef USE_USB

template<typename T>
inline size_t serialPrint(const T& value) {
  return Serial.print(value);
}

template<typename T>
inline size_t serialPrint(const T& value, int base) {
  return Serial.print(value, base);
}

inline size_t serialPrint(double value, int digits) {
  return Serial.print(value, digits);
}

inline size_t serialPrint(const char* s) {
  return Serial.print(s);
}

template<typename T>
inline size_t serialPrintln(const T& value) {
  return Serial.println(value);
}

template<typename T>
inline size_t serialPrintln(const T& value, int base) {
  return Serial.println(value, base);
}

inline size_t serialPrintln(double value, int digits) {
  return Serial.println(value, digits);
}

inline size_t serialPrintln(const char* s) {
  return Serial.println(s);
}

inline size_t serialPrintln(void) {
  return Serial.println();
}

#else

template<typename T>
inline size_t serialPrint(const T&) { return 0; }

template<typename T>
inline size_t serialPrint(const T&, int) { return 0; }

inline size_t serialPrint(double, int) { return 0; }

inline size_t serialPrint(const char*) { return 0; }

template<typename T>
inline size_t serialPrintln(const T&) { return 0; }

template<typename T>
inline size_t serialPrintln(const T&, int) { return 0; }

inline size_t serialPrintln(double, int) { return 0; }

inline size_t serialPrintln(const char*) { return 0; }

inline size_t serialPrintln(void) { return 0; }

#endif

//**********************
void myDelay(uint32_t ms)
{
  vTaskDelay(pdMS_TO_TICKS(ms));
}

#ifdef USE_USB 

//***************************************
uint16_t readSerialInput()
{
  uint8_t idx = 0;
  char c;

  aInputText[0] = 0;

  while (Serial.available() > 0) {
    c = Serial.read();

    // Fin de ligne → chaîne terminée
    if (c == '\n' || c == '\r') {
      if (idx > 0) {
        aInputText[idx] = '\0'; 
      }
    }
    else {
     
      if (idx < sizeof(aInputText) - 1) {
        aInputText[idx++] = c;
      }
      
    }
  }
return idx;
}

#else
uint16_t readSerialInput()
{ return 0; }

#endif

//***************************************
void dump_memory(uint32_t addr, size_t n) 
//***************************************
{
  char a[16];
  char aLineText[17];
  uint16_t j = 0;

  const uint8_t *p = (const uint8_t*)(uintptr_t)addr;

  for (size_t i = 0; i < n; i++) {
    if (i % 16 == 0) 
      { 
        
        aLineText[j]=0;
        j = 0;
        if (i) serialPrint (aLineText);
        sprintf (a, "\n0x%08lX: ", (unsigned long)(addr + i) ); serialPrint (a); 
      }
    sprintf(a, "%02X ", p[i]); serialPrint(a);
    if (p[i] >= ' ' && p[i]<'z' ) aLineText[j] = p[i];
    else aLineText[j] = '.';
    j++;

  }
  serialPrint (aLineText);
  serialPrintln();
}

//*************************************

void toBinary8(uint8_t v, char *buf) {
  for (int i = 7; i >= 0; i--) {
    buf[7 - i] = (v & (1 << i)) ? '1' : '0';
  }
  buf[8] = '\0';
}

//**********************************
void flashLed(uint16_t c, uint16_t d)
//**********************************
{
  uint16_t i;

  for (i=0; i < c ; i++)
   {
    digitalWrite(LED_BUILTIN, LOW); // lOW -> light ON...
    delay(5);
    digitalWrite(LED_BUILTIN, HIGH);
    if (i<c-1) delay (d); 
   }
}

//***********
void rgbLedInit()
//**********
{
  pinMode (LED_RED, OUTPUT);
  pinMode (LED_GREEN, OUTPUT);
  pinMode (LED_BLUE, OUTPUT);

  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
}

//******************************
void rgbLedSet (uint8_t color , bool on)
//******************************
{
  if (color & RGB_LED_RED) digitalWrite(LED_RED, !on);
  if (color & RGB_LED_GREEN) digitalWrite(LED_GREEN, !on);
  if (color & RGB_LED_BLUE) digitalWrite(LED_BLUE, !on);
}

//**********************************
void rgbLedFlash(uint8_t color, uint16_t count, uint16_t d)
//**********************************
{
  uint16_t i;

  for (i=0; i < count ; i++)
   {
    rgbLedSet(color , true); 
    delay(5);// Led on during 5 ms
    rgbLedSet(color , false); 
    if ( i < count - 1) delay (d); 
   }
}

//=================================================
void motorStart(uint16_t duration, uint16_t pwm)
//=================================================
{
  if (!fEnableVibrations) return;

  if (pwm > 255) pwm = 255;

  analogWrite(MOTOR_PIN, pwm);

  delay(duration);

  analogWrite(MOTOR_PIN, 0);

}

//********************************
bool fSwitchedPressed(uint16_t &v)
{
  v = NO_CLICK;

  const bool btn = (digitalRead(SWITCH_PIN) == LOW); 

  // pressed
  if (btn && !gBtnLast) {
    gPressStartMs = millis();
    gBtnActive = true;
    gLongFired = false;
  }

  // while pressed to check long press duration
  if (btn && gBtnActive && !gLongFired) {

    const uint32_t dt = millis() - gPressStartMs;
    if (dt >= LONG_CLICK_MIN_MS) {
      gLongFired = true;
      v = LONG_CLICK;
      gBtnLast = btn;
    
      return true;
    }
  }

  // release
  if (!btn && gBtnLast && gBtnActive) {
    const uint32_t dt = millis() - gPressStartMs;
    gBtnActive = false;

    if (gLongFired) {
      gBtnLast = btn;
      return false;
    }

    if (dt >= DEBOUNCE_MS) {
      v = SHORT_CLICK;
      gBtnLast = btn;
      return true;
    }
  }

  gBtnLast = btn;
  return false;
}

/*
//**********************
bool serialIsActive()
//**********************
{
  // "Serial" vrai si l'USB CDC est up sur beaucoup de cores
  // DTR = terminal ouvert (Moniteur Série / PuTTY etc.)
  // Si dtr() n’existe pas sur ton core, commente la ligne et garde juste (bool)Serial.
  return (bool)Serial && Serial.dtr();
}


//******************************
bool isSoftDeviceEnabled(void) 
//******************************
{
  uint8_t enabled = 0;
  if (sd_softdevice_is_enabled(&enabled) != NRF_SUCCESS) {
    return false;
  }
  return (enabled != 0);
}
*/

//**********************
bool wokeFromSystemOff()
//**********************
{
  return (NRF_POWER->RESETREAS & POWER_RESETREAS_OFF_Msk);
}

//*********************************
// MEDIAN FILTER
//*********************************
#define MEDIAN_N 5   // 3, 5, 7...

typedef struct {
  float buf[MEDIAN_N];
  uint8_t idx;
  bool full;
} MedianFilterN;

//*********************************
void medianInit(MedianFilterN &f) {
  f.idx = 0;
  f.full = false;
  for (uint8_t i = 0; i < MEDIAN_N; i++) f.buf[i] = 0.0f;
}

//*********************************
static inline void sortFloatArray(float *a, uint8_t n) {
  for (uint8_t i = 1; i < n; i++) {
    float key = a[i];
    int8_t j = i - 1;
    while (j >= 0 && a[j] > key) {
      a[j + 1] = a[j];
      j--;
    }
    a[j + 1] = key;
  }
}

//*********************************
float medianUpdate(MedianFilterN &f, float x) {
// to filter signal crasy peaks
  f.buf[f.idx++] = x;
  if (f.idx >= MEDIAN_N) {
    f.idx = 0;
    f.full = true;
  }

  if (!f.full) return x;  // no enough samples

  float tmp[MEDIAN_N];
  for (uint8_t i = 0; i < MEDIAN_N; i++)
    tmp[i] = f.buf[i];

  sortFloatArray(tmp, MEDIAN_N);

  return tmp[MEDIAN_N / 2];
}

//***********LINEAR REGRESSION to detect variations of Accelero signal ***************

#define SLOPE_WINDOW_SIZE 10  // amount of points to compute slope

// Structure for linear regression
typedef struct {
  float buffer[SLOPE_WINDOW_SIZE];
  uint8_t index;
  bool full;
  float sum_x;      
  float sum_x2;   
} SlidingSlope;

//******************************************
void sliding_slope_init(SlidingSlope* ss) {
//******************************************
  for (uint8_t i = 0; i < SLOPE_WINDOW_SIZE; i++) {
    ss->buffer[i] = 0.0f;
  }
  ss->index = 0;
  ss->full = false;
  
  // pre computes sums (optimisation)
  // sum_x = 0 + 1 + 2 + ... + (N-1) = N*(N-1)/2
  ss->sum_x = (float)(SLOPE_WINDOW_SIZE * (SLOPE_WINDOW_SIZE - 1)) / 2.0f;
  
  // sum_x2 = 0² + 1² + 2² + ... + (N-1)² = N*(N-1)*(2N-1)/6
  ss->sum_x2 = (float)(SLOPE_WINDOW_SIZE * (SLOPE_WINDOW_SIZE - 1) * (2 * SLOPE_WINDOW_SIZE - 1)) / 6.0f;
}

//***********************************************
// linear regression computing
float sliding_slope_update(SlidingSlope* ss, float y) {
//***********************************************

  // add new value in circular buffer
  ss->buffer[ss->index] = y;
  ss->index++;
  
  if (ss->index >= SLOPE_WINDOW_SIZE) {
    ss->index = 0;
    ss->full = true;
  }
  
  // wait to get N points
  if (!ss->full) {
    return 0.0f;
  }
  
  // linear regression: y = a*x + b
  // Formula: a = (N*sum(xy) - sum(x)*sum(y)) / (N*sum(x²) - sum(x)²)
  
  float sum_y = 0.0f;
  float sum_xy = 0.0f;
  
  // scan buffer
  for (uint8_t i = 0; i < SLOPE_WINDOW_SIZE; i++) {
    uint8_t buf_idx = (ss->index + i) % SLOPE_WINDOW_SIZE;
    float y_val = ss->buffer[buf_idx];
    
    sum_y += y_val;
    sum_xy += (float)i * y_val;
  }
  
  // computes slope
  float N = (float)SLOPE_WINDOW_SIZE;
  float numerator = N * sum_xy - ss->sum_x * sum_y;
  float denominator = N * ss->sum_x2 - ss->sum_x * ss->sum_x;
  
  //float slope = numerator / denominator; initial code from claude
  float slope = numerator; // no need to devide by a constant in our context

  return slope;
}

