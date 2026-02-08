static volatile bool tick = false;

// period in ms (can be updated dynamically)
static volatile uint32_t tick_ms = 100;

//***************************************
void setTickPeriodMs(uint32_t ms) 
//***************************************
// Change la période en ms (thread-safe simple)
{
  tick_ms = ms;
}

//***************************************
bool checkRtcEvent()
//***************************************
{
if (tick) 
	{
    tick = false;
	  return true;
	}
else return false;
}

//***************************************
static bool lfclk_start_try(uint32_t src)
//***************************************
{
  NRF_CLOCK->LFCLKSRC = (src << CLOCK_LFCLKSRC_SRC_Pos);
  NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
  NRF_CLOCK->TASKS_LFCLKSTART = 1;

  for (uint32_t i = 0; i < 500000; i++) {
    if (NRF_CLOCK->EVENTS_LFCLKSTARTED) return true;
  }
  return false;
}

//***************************************
static void lfclk_start()
//***************************************
{
  if (!lfclk_start_try(CLOCK_LFCLKSRC_SRC_Xtal)) {
    lfclk_start_try(CLOCK_LFCLKSRC_SRC_RC);
  }
}

//***************************************
extern "C" void RTC2_IRQHandler(void)
//***************************************
{
  if (NRF_RTC2->EVENTS_COMPARE[0]) {
    NRF_RTC2->EVENTS_COMPARE[0] = 0;

    tick = true;

    // cconverts : ms -> ticks RTC (LFCLK = 32768 Hz)
    uint32_t ms = tick_ms; 
   
    // ticks = ms * 32768 / 1000
    // we use maximum few seconds (in debug mode), so, no risk of overflow here
    uint32_t period_ticks = (ms * 32768UL) / 1000UL;
    if (period_ticks == 0) period_ticks = 1;

    NRF_RTC2->CC[0] = (NRF_RTC2->CC[0] + period_ticks) & 0x00FFFFFF;
  }
}

//***************************************
static void rtc2_start()
//***************************************
{
  lfclk_start();

  NRF_RTC2->TASKS_STOP  = 1;
  NRF_RTC2->TASKS_CLEAR = 1;

  NRF_RTC2->PRESCALER = 0; // 32768 Hz

  NRF_RTC2->EVENTS_COMPARE[0] = 0;
  NRF_RTC2->INTENSET = RTC_INTENSET_COMPARE0_Msk;

  NVIC_ClearPendingIRQ(RTC2_IRQn);
  NVIC_SetPriority(RTC2_IRQn, 6);
  NVIC_EnableIRQ(RTC2_IRQn);

  uint32_t ms = tick_ms;
  uint32_t first_ticks = (ms * 32768UL) / 1000UL;

  if (first_ticks == 0) first_ticks = 1;
  NRF_RTC2->CC[0] = (NRF_RTC2->COUNTER + first_ticks) & 0x00FFFFFF;

  NRF_RTC2->TASKS_START = 1;
}

//***************************************
static inline void sleep_until_irq()
//***************************************
{
__SEV();   // force known event
__WFE();   // consumes event
__WFE();   // clean sleep
}
