// tests power:
// avec Serial:             9.5mA   3.5
// Sans Serial:             8.1     1.9 (moin 1.4 mA) =. mais apres un moment ca remonte...
// Avec serial, sans IMU    8.7     2.8 (moins 0.7mA) mais pas de data IMU au reveil...
// avec wire.begin et .end  8.7     3.5
// et en plus coupures pins 7.4     2.0 Mieux :) I2C semble coupé
// wire.end +pins cut+no usb 7.4    1.9 

/* Xiao Sense has pins to read VBAT voltage by software, cool
definitions in variant.h:
#define VBAT_ENABLE             (14)    // Output LOW to enable reading of the BAT voltage.
                                        // https://wiki.seeedstudio.com/XIAO_BLE#q3-what-are-the-considerations-when-using-xiao-nrf52840-sense-for-battery-charging

#define PIN_CHARGING_CURRENT    (22)    // Battery Charging current
                                        // https://wiki.seeedstudio.com/XIAO_BLE#battery-charging-current

#define PIN_VBAT                (32)    // Read the BAT voltage.
                                        // https://wiki.seeedstudio.com/XIAO_BLE#q3-what-are-the-considerations-when-using-xiao

definitions in variant.cpp:
PinDescription g_APinDescription[] = {
  ....
  // VBAT_READ / Analog
  { P0_31, NULL, NULL, NULL },     // D32/VBAT_READ  
  ....
  // VBAT
  { P0_14, NULL, NULL, NULL },     // D31/VBAT_ENABLE

*/

//**************
float readVBAT()
//**************
{
  analogReadResolution(12); // 0..4095

  // activate voltage devider (P0.14 à 0)
  pinMode(VBAT_ENABLE, OUTPUT);
  
  digitalWrite(VBAT_ENABLE, LOW);
  
  delayMicroseconds(20); 

  uint16_t raw = analogRead(PIN_VBAT);

  // cut divider bridge, moves back to Hi-Z
  pinMode(VBAT_ENABLE, INPUT);
  
  // Conversion: 3.64v

  const float k = 0.32f ; //0.3307f;
  float vadc = (raw * 3.6f) / 4095.0f;
  float vbat = vadc / k;

  return vbat;
}


//**************
float readVIN()
//**************
{
    NRF_SAADC->ENABLE = 1; // starts ADC converter 

  analogReadResolution(12); // 0..4095

  pinMode (VIN_ENABLE_PIN, OUTPUT);
  
  digitalWrite(VIN_ENABLE_PIN, LOW);
  
  delayMicroseconds(20);   
  uint16_t raw = analogRead(VIN_VOLTAGE_PIN);

  NRF_SAADC->ENABLE = 0; // stops ADC converter

  pinMode (VIN_ENABLE_PIN , INPUT);
  
  // Conversion:
  // Vadc = raw/4095 * 3.6V
  // Vbat = Vadc / k, avec k = 510k/(1M+510k) = 0.337748
  const float k = 0.3307f;
  float vadc = (raw * 3.6f) / 4095.0f;
  float vbat = vadc / k;

  return vbat;
}


//**************
float readVIN2()
//**************
{
    NRF_SAADC->ENABLE = 1; // starts ADC converter 

  analogReadResolution(12); // 0..4095

  pinMode (VIN2_ENABLE_PIN, OUTPUT);
  
  digitalWrite(VIN2_ENABLE_PIN, LOW);
  
  delayMicroseconds(20);

  uint16_t raw = analogRead(VIN2_VOLTAGE_PIN);

  NRF_SAADC->ENABLE = 0; // stops ADC converter

  pinMode (VIN2_ENABLE_PIN , INPUT);
  
  // Conversion:
  // Vadc = raw/4095 * 3.6V
  // Vbat = Vadc / k, avec k = 510k/(1M+510k) = 0.337748
  const float k = 0.3307f;
  float vadc = (raw * 3.6f) / 4095.0f;
  float vbat = vadc / k;

  return vbat;
}

//**************
uint16_t readI()
{
uint16_t v1,v2, min1, min2, max1, max2;
uint16_t i= 0;
uint16_t v;
float av1,av2;
uint8_t c;

  NRF_SAADC->ENABLE = 1; // starts ADC converter 

  analogReadResolution(12); // 0..4095

  pinMode (VIN_ENABLE_PIN, OUTPUT);
  pinMode (VIN2_ENABLE_PIN, OUTPUT); 
  digitalWrite(VIN_ENABLE_PIN, LOW);
  digitalWrite(VIN2_ENABLE_PIN, LOW);  

  delayMicroseconds(20); 

  v1 = analogRead(VIN_VOLTAGE_PIN);
  v2 = analogRead(VIN2_VOLTAGE_PIN);

  for (av1 = 0, av2=0, min1 = 0xFFFF, min2 = 0xFFFF, max1=0, max2 = 0, c = 0 ; c<10; c++) 
    {
      v1 = analogRead(VIN_VOLTAGE_PIN);
      //delayMicroseconds(50); 
      v2 = analogRead(VIN2_VOLTAGE_PIN);

      if (v1 < min1) min1 = v1;
      if (v2 < min2) min2 = v2;
      if (v1 > max1) max1 = v1;
      if (v2 > max2) max2 = v2;
      av1 += v1;
      av2+= v2;

      //delayMicroseconds(50); 
    }

  av1 = av1*100; // mV -> from V to mV: x 1000 . to devide by number of samples added and cimpute average: /10
  av2 = av2*100; // mV

  av1 = av1 * (3.6f / 4095.0f) * 2; // *2: the resistors dividing R1=R2= 100k -> V.out = V.in x [R2 / (R1+R2)]
  av2 = av2 * (3.6f / 4095.0f) * 2; // 3.6f = max voltage. Should be 3.3 byt surprinsgly reality says 3.6 ratio...

  i = (int16_t) (10*(av1 - av2) ); //micro Amperes
  
  NRF_SAADC->ENABLE = 0; // stops ADC converter

  pinMode (VIN_ENABLE_PIN , INPUT);
  pinMode (VIN2_ENABLE_PIN , INPUT);

  if (1)  {
    sprintf (aDebug, "V1:%d\tV2:%d\tV1-V2:%d\tMax1-Min1:%d\tMax2-Min2:%d\tuA:%d",(int32_t)av1, (int32_t)av2,(int32_t)(av1-av2), max1-min1, max2-min2, i);
    serialPrintln (aDebug);
    }

return i;
}


///***************************************
static inline void microphoneHwOff(void)
//***************************************
{
  // Stop PDM (safe even if it was not running)
  NRF_PDM->TASKS_STOP = 1;

  // Optional: wait until the peripheral is really stopped (EVENTS_STOPPED)
  // NRF_PDM->EVENTS_STOPPED = 0;
  // while (NRF_PDM->EVENTS_STOPPED == 0) { /* spin */ }

  // Remember which pins PDM is actually using
  uint32_t clk_pin = NRF_PDM->PSEL.CLK;  // GPIO number (0..47 depending on mapping)
  uint32_t din_pin = NRF_PDM->PSEL.DIN;

  // Disconnect pins from PDM first (prevents the peripheral from owning them)
  // (On nRF52, writing 0xFFFFFFFF to PSEL typically means "disconnected")
  NRF_PDM->PSEL.CLK = 0xFFFFFFFF;
  NRF_PDM->PSEL.DIN = 0xFFFFFFFF;

  // Disable the peripheral
  NRF_PDM->ENABLE = 0;

  // Put used pins in Hi-Z (only the ones PDM was actually using)
  if (clk_pin != 0xFFFFFFFF) {
    NRF_GPIO_Type *port = (clk_pin < 32) ? NRF_P0 : NRF_P1;
    uint32_t idx = (clk_pin < 32) ? clk_pin : (clk_pin - 32);
    port->PIN_CNF[idx] =
      (GPIO_PIN_CNF_DIR_Input       << GPIO_PIN_CNF_DIR_Pos) |
      (GPIO_PIN_CNF_INPUT_Connect   << GPIO_PIN_CNF_INPUT_Pos) | // keep input buffer connected (harmless)
      (GPIO_PIN_CNF_PULL_Disabled   << GPIO_PIN_CNF_PULL_Pos) |
      (GPIO_PIN_CNF_DRIVE_S0S1      << GPIO_PIN_CNF_DRIVE_Pos) |
      (GPIO_PIN_CNF_SENSE_Disabled  << GPIO_PIN_CNF_SENSE_Pos);
  }

  if (din_pin != 0xFFFFFFFF) {
    NRF_GPIO_Type *port = (din_pin < 32) ? NRF_P0 : NRF_P1;
    uint32_t idx = (din_pin < 32) ? din_pin : (din_pin - 32);
    port->PIN_CNF[idx] =
      (GPIO_PIN_CNF_DIR_Input       << GPIO_PIN_CNF_DIR_Pos) |
      (GPIO_PIN_CNF_INPUT_Connect   << GPIO_PIN_CNF_INPUT_Pos) |
      (GPIO_PIN_CNF_PULL_Disabled   << GPIO_PIN_CNF_PULL_Pos) |
      (GPIO_PIN_CNF_DRIVE_S0S1      << GPIO_PIN_CNF_DRIVE_Pos) |
      (GPIO_PIN_CNF_SENSE_Disabled  << GPIO_PIN_CNF_SENSE_Pos);
  }
}

//------------------------------------------------------------
// Disable Serial1 (UARTE0) at hardware level
// - Stops RX/TX
// - Disables UARTE peripheral
// - Disconnects pins from peripheral
// - Leaves GPIOs in Hi-Z
//------------------------------------------------------------
static inline void uart1HwOff(void)
{
  // 1) Stop ongoing RX/TX (safe even if not running)
  NRF_UARTE0->TASKS_STOPRX = 1;
  NRF_UARTE0->TASKS_STOPTX = 1;

  // 2) Disable UARTE peripheral (key step)
  NRF_UARTE0->ENABLE = 0;

  // 3) Disconnect all UART pins from the peripheral
  NRF_UARTE0->PSEL.RXD = 0xFFFFFFFF;
  NRF_UARTE0->PSEL.TXD = 0xFFFFFFFF;
  NRF_UARTE0->PSEL.CTS = 0xFFFFFFFF;
  NRF_UARTE0->PSEL.RTS = 0xFFFFFFFF;

  // 4) (Optional but recommended) Put the pins in Hi-Z
  // If you know the exact pins used by Serial1 on your board,
  // you can set them explicitly to INPUT / no pull.
  // Otherwise, leaving them disconnected is already enough.
}

// XIAO nRF52840 Sense (variant.h excerpt you pasted)
// Wire  (I2C0): SDA=D4, SCL=D5
// Wire1 (I2C1): SDA=D17, SCL=D16
//
// NOTE: Dxx are Arduino pin numbers. We must translate to nRF GPIO using g_ADigitalPinMap[].

static inline uint32_t arduinoPinToNrfPin(uint8_t aPin)
{
  // g_ADigitalPinMap[] is provided by the nRF52 core (variant mapping)
  return g_ADigitalPinMap[aPin]; // e.g. returns (port*32 + pin)
}

static inline void nrfPinToPortIndex(uint32_t nrf_pin, NRF_GPIO_Type** port, uint32_t* idx)
{
  if (nrf_pin < 32) { *port = NRF_P0; *idx = nrf_pin; }
  else              { *port = NRF_P1; *idx = nrf_pin - 32; }
}

static inline void gpio_hiz_nrf(uint32_t nrf_pin)
{
  NRF_GPIO_Type* port; uint32_t idx;
  nrfPinToPortIndex(nrf_pin, &port, &idx);

  port->PIN_CNF[idx] =
      (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
      (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
      (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
      (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
      (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
}

static inline void gpio_i2c_od_nrf(uint32_t nrf_pin)
{
  NRF_GPIO_Type* port; uint32_t idx;
  nrfPinToPortIndex(nrf_pin, &port, &idx);

  // Open-drain style drive for I2C (S0D1 is commonly used)
  port->PIN_CNF[idx] =
      (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
      (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
      (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
      (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos) |
      (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
}

// ------------------------------------------------------------
// Disable I2C hardware and release pins (Hi-Z)
// - Disables TWIM0 + TWIM1 (safe even if unused)
// - Releases both Wire (D4/D5) and Wire1 (D17/D16) pins
// ------------------------------------------------------------
static inline void i2c_hw_off(void)
{
  /*
  // Stop/disable TWIM0
  NRF_TWIM0->TASKS_STOP = 1;
  NRF_TWIM0->ENABLE = 0;
  NRF_TWIM0->PSEL.SDA = 0xFFFFFFFF;
  NRF_TWIM0->PSEL.SCL = 0xFFFFFFFF;

  // Stop/disable TWIM1
  NRF_TWIM1->TASKS_STOP = 1;
  NRF_TWIM1->ENABLE = 0;
  NRF_TWIM1->PSEL.SDA = 0xFFFFFFFF;
  NRF_TWIM1->PSEL.SCL = 0xFFFFFFFF;
*/

  // Release Wire pins (D4/D5)
  gpio_hiz_nrf(arduinoPinToNrfPin(PIN_WIRE_SDA)); // D4
  gpio_hiz_nrf(arduinoPinToNrfPin(PIN_WIRE_SCL)); // D5

  // Release Wire1 pins (D17/D16)
  gpio_hiz_nrf(arduinoPinToNrfPin(PIN_WIRE1_SDA)); // D17
  gpio_hiz_nrf(arduinoPinToNrfPin(PIN_WIRE1_SCL)); // D16
}

// ------------------------------------------------------------
// Enable I2C hardware and restore pins for Wire (TWIM0)
// - Configures pins for open-drain style
// - Routes TWIM0 to Wire pins (D4/D5)
// - Sets 400 kHz (change to K100 if needed)
// ------------------------------------------------------------
static inline void i2c_hw_on(void)
{
  const uint32_t sda_nrf = arduinoPinToNrfPin(PIN_WIRE_SDA); // D4
  const uint32_t scl_nrf = arduinoPinToNrfPin(PIN_WIRE_SCL); // D5

  // Configure GPIO electrical mode for I2C
  gpio_i2c_od_nrf(sda_nrf);
  gpio_i2c_od_nrf(scl_nrf);

  // Route pins to TWIM0 (PSEL expects port-relative pin number, not "port*32+pin")
  // In nRF, PSEL uses bits: [4:0] pin number + [5] port (implementation varies by SDK),
  // but CMSIS structs for nRF52 cores expect absolute pin index in many Arduino cores.
  // For this core, the safe way is to write the same value used by g_ADigitalPinMap (absolute).
  NRF_TWIM0->PSEL.SDA = sda_nrf;
  NRF_TWIM0->PSEL.SCL = scl_nrf;

  NRF_TWIM0->FREQUENCY = TWIM_FREQUENCY_FREQUENCY_K400;
  NRF_TWIM0->ENABLE    = TWIM_ENABLE_ENABLE_Enabled;
}

static inline void i2c_hw_off_safe(void)
{
  // If Wire is active in libs, call this only when you know no I2C transfer is ongoing.

  // Disable TWIM0 without STOP (avoids waiting on bus state)
  NRF_TWIM0->ENABLE = 0;
  NRF_TWIM0->PSEL.SDA = 0xFFFFFFFF;
  NRF_TWIM0->PSEL.SCL = 0xFFFFFFFF;

  // Do NOT touch TWIM1 unless you explicitly use Wire1
  // NRF_TWIM1->ENABLE = 0;
  // NRF_TWIM1->PSEL.SDA = 0xFFFFFFFF;
  // NRF_TWIM1->PSEL.SCL = 0xFFFFFFFF;

  // Put Wire pins in Hi-Z (using your variant pins D4/D5)
  uint32_t sda_nrf = g_ADigitalPinMap[PIN_WIRE_SDA];
  uint32_t scl_nrf = g_ADigitalPinMap[PIN_WIRE_SCL];

  NRF_GPIO_Type* port; uint32_t idx;

  port = (sda_nrf < 32) ? NRF_P0 : NRF_P1; idx = (sda_nrf < 32) ? sda_nrf : (sda_nrf - 32);
  port->PIN_CNF[idx] =
      (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
      (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
      (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos);

  port = (scl_nrf < 32) ? NRF_P0 : NRF_P1; idx = (scl_nrf < 32) ? scl_nrf : (scl_nrf - 32);
  port->PIN_CNF[idx] =
      (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
      (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
      (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos);
}

static inline void i2c_hw_on_safe(void)
{
  uint32_t sda_nrf = g_ADigitalPinMap[PIN_WIRE_SDA];
  uint32_t scl_nrf = g_ADigitalPinMap[PIN_WIRE_SCL];

  NRF_TWIM0->PSEL.SDA = sda_nrf;
  NRF_TWIM0->PSEL.SCL = scl_nrf;
  NRF_TWIM0->FREQUENCY = TWIM_FREQUENCY_FREQUENCY_K400;
  NRF_TWIM0->ENABLE = TWIM_ENABLE_ENABLE_Enabled;
}

//***********************************************
static inline void i2c_kill_twim(NRF_TWIM_Type* T)
{
  // Stop + disable
  T->TASKS_STOP = 1;
  T->ENABLE = 0;

  // Disconnect pins from peripheral (avoid peripheral holding them)
  T->PSEL.SCL = 0xFFFFFFFF;
  T->PSEL.SDA = 0xFFFFFFFF;
}

//*******************************
void gotoSystemOff_WakeupOnSwitch()
//******************************
{
  // Avoid Serial if usb is not active...
  if (fPrintDebug) {
    serialPrintln("SYSTEMOFF called ...");
    //Serial.flush();
    delay(20);

  }

  //microphoneHwOff(); // seems useless
    __disable_irq();//seems useless

  systemOffIMU();
  //uart1HwOff();
  //i2c_hw_off();

  i2c_kill_twim(NRF_TWIM0);
#ifdef NRF_TWIM1
  i2c_kill_twim(NRF_TWIM1);
#endif

  // Put SDA/SCL in hi-Z, no pull
  pinMode(PIN_WIRE_SDA, INPUT);
  pinMode(PIN_WIRE_SCL, INPUT);
  // Si tu utilises nrf_gpio_cfg_input:
  // nrf_gpio_cfg_input(PIN_WIRE_SDA, NRF_GPIO_PIN_NOPULL);
  // nrf_gpio_cfg_input(PIN_WIRE_SCL, NRF_GPIO_PIN_NOPULL);
  
  NRF_CLOCK->TASKS_HFCLKSTOP = 1;
  NRF_POWER->TASKS_LOWPWR = 1;

  // --- 1) Configure Pin as input with pull-up (Arduino side) ---
  // Button assumed between Dxx and GND
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // --- 2) Get the NRF GPIO number corresponding to Arduino D9 ---
  // Works on cores exposing g_ADigitalPinMap[]
  uint32_t nrf_pin = g_ADigitalPinMap[SWITCH_PIN];

  // --- 3) Clear any previous GPIO wake-up latches ---
  // If the pin is already LOW when entering SYSTEMOFF,
  // the MCU could wake up immediately without this.
  NRF_GPIO->LATCH = 0xFFFFFFFF;
  (void)NRF_GPIO->LATCH;

  // --- 4) Configure GPIO hardware SENSE ---
  // Wake up on LOW level, keep internal pull-up enabled
  NRF_GPIO->PIN_CNF[nrf_pin] =
      (GPIO_PIN_CNF_DIR_Input     << GPIO_PIN_CNF_DIR_Pos)   |
      (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
      (GPIO_PIN_CNF_PULL_Pullup   << GPIO_PIN_CNF_PULL_Pos)  |
      (GPIO_PIN_CNF_DRIVE_S0S1    << GPIO_PIN_CNF_DRIVE_Pos) |
      (GPIO_PIN_CNF_SENSE_Low     << GPIO_PIN_CNF_SENSE_Pos);

  // --- 5) Enter SYSTEM OFF mode ---
  // CPU, RAM and peripherals are powered down
  // GPIO SENSE remains active and will trigger a RESET on wake-up
  //sd_power_system_off(); // Not needed normally except if in BLE


    __DSB();
  NRF_POWER->SYSTEMOFF = 1;

  // Execution never reaches here
  while (1) 
  { 
    //__DSB();// sure?
    __WFE();
    //__WFE(); 
    for(;;) { __WFE(); }
  }

}

