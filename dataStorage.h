/* FLASH Info global:
- Flash total size : 1 Mo (1024 Ko)
- Adressing : 0x0000_0000 → 0x000F_FFFF
= write by word of 32bits
- erase par page of 4k only

Page # | start of pages addresses (hex)
-----------------------------------
  0    | 0x000ED000
  1    | 0x000EE000
  2    | 0x000EF000
  3    | 0x000F0000
  4    | 0x000F1000
  5    | 0x000F2000
  6    | 0x000F3000
-----------------------------------
End of free user zone : 0x000F4000 (excluded)
WARNING: 0xF4000 seems used (bootloader?). Dump:
0x000F4000: 00 00 04 20 19 AE 0F 00 41 AE 0F 00 43 AE 0F 00 
0x000F4010: 45 AE 0F 00 47 AE 0F 00 49 AE 0F 00 00 00 00 00 
0x000F4020: 00 00 00 00 00 00 00 00 00 00 00 00 19 4B 0F 00 
0x000F4030: 4D AE 0F 00 00 00 00 00 4F AE 0F 00 B1 4C 0F 00 
*/

static constexpr uint32_t FLASH_PAGE_SIZE = 4096;//0x1000
static constexpr uint32_t FIRST_USER_FLASH_ADDR = 0x000ED000;
static constexpr uint32_t LAST_USER_FLASH_ADDR = (0x000F3000); // F3000 for tests !! => real: 0x000F4000; but: in case of bug, just after we have code for Boot. so, overwrite = board has to be reflashed (PAINFUL)
static constexpr uint32_t USER_FLASH_SIZE = (LAST_USER_FLASH_ADDR - FIRST_USER_FLASH_ADDR); // normally: 0x0007000;

// user area on flash = 0x7000 - 7 pages of 4 KB (7 × 4096= 28672 bytes. 1 record = 8 bytes => max 3584 records, ie 59.7 recording hours with 1 record per mn
// if log during 12h: 60*12 = 720 records per night, ie 5 nights

//#define COUNT_USER_FLASH_PAGES 7

// signature to scan the headers
#define MAGIC_NUMBER_HEAD_1 0xF1
#define MAGIC_NUMBER_HEAD_2 0xF2
#define MAGIC_NUMBER_HEAD_3 0xF3

//=> !! WARNING: MUST be multiple of 4 bytes, to simplify management of Flash writing blocs (based on 'words'multiple of 4 bytes) !!
typedef struct __attribute__((packed, aligned(4))) { // keep those attribues packed & aligned
//typedef struct {
  uint16_t time;           // seconds since startup (0..43200) => lately replaced by Accelero Max
  uint8_t  position;      // 0..3 (values POS_*) => also used for LOG records types
  uint8_t  breathPerMinut;  // 0..255
  uint8_t  breathMax; // 00..60, or more? max <255 sec...
  uint8_t  cSnores; //cMoves;
  uint16_t cSmallMoves; 
}recordData_t;

static_assert(sizeof(recordData_t) == 8, "recordData_t must be 8 bytes");
static_assert(alignof(recordData_t) == 4, "recordData_t must be 4-byte aligned");


typedef struct __attribute__((packed, aligned(4))) { // keep those attribues packed & aligned
//typedef struct {
  uint16_t sText;         // optional free text stored after this header        
  uint8_t  recordType;    // same field as 'position'of recordData, with specific values to make difference. LOG_SUCCESS, INFO, WARNING, ERROR
  uint32_t millis;        // since startup
  uint8_t  code;          // id / error code / log code of the Log type/ number
}recordLog_t;

static_assert(sizeof(recordLog_t) == 8, "recordLog_t must be 8 bytes");
static_assert(alignof(recordLog_t) == 4, "recordLog_t must be 4-byte aligned");

//!! WARNING: this structure size MUST be multiple of 4 bytes!!
//=> 12 bytes
typedef struct __attribute__((packed, aligned(4))) {
  uint8_t magic1;
  uint8_t magic2;
  uint8_t magic3;
  uint16_t counter; // incrmental number to detect the last chain of records that was written on flash
  uint16_t cRecords; // # of data records after this header
  int8_t biasGx;
  int8_t biasGy;
  int8_t biasGz;
  uint16_t vPower;// v*1000
} recordHead_t;

static_assert(sizeof(recordHead_t) == 12, "recordHead_t must be 12 bytes");
static_assert(alignof(recordHead_t) == 4, "recordHead_t must be 4-byte aligned");


//****************************
// GLOBAL VARIABLES
//****************************
uint32_t flash_cRecords;
uint32_t flash_lastCounter;
uint32_t flash_addrFirstDataRecord;
uint32_t flash_nextWriteAddr;
recordData_t tabRecords[N_RECORDS_MAX];
uint16_t cRecords = 0;
uint16_t previousI = 0;

extern bool searchFlashRecords();
extern void readDataRecords( bool fOverwriteTabRecords);

//***************************************
static inline void nvmc_wait_ready(void) {
  while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
    __NOP();
  }
}

//********************************************************************************
bool flash_write_words(uint32_t dst_addr, const uint32_t *data, size_t words) {
//********************************************************************************
char a[255];
 if (fPrintDebug)
  {
      sprintf (a, ">> Call to flash_write_words (dst_addr = 0x%08lX, words(4B)=%d. size (Bytes)=%d). ", dst_addr, words , words*4 );
      serialPrint(a);
  }

if ( !data || words == 0) return false;
if ( dst_addr & 0x3) return false;           // alignment 4 bytes mandatory

uint32_t bytes = words * 4u; // check potentialoverflow, if parameter passed is crasy
if (bytes / 4u != words) return false; // overflow multiplication

if (dst_addr < FIRST_USER_FLASH_ADDR) return false;
if (dst_addr + (words * 4)  > LAST_USER_FLASH_ADDR) return false;

uint32_t maxBytes = LAST_USER_FLASH_ADDR - dst_addr;
if (bytes > maxBytes) return false;

 if (fPrintDebug)  {
      serialPrintln( " All checks OK");
  }

  __disable_irq();

  // Autorize to write on flash
  NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos);
  __DSB();
  __ISB();
  nvmc_wait_ready();

  // Writes each word
  for (size_t i = 0; i < words; i++) {
    ((volatile uint32_t *)dst_addr)[i] = data[i];
    __DSB();
    __ISB();
    nvmc_wait_ready();
  }

  // Move back to read only
  NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);
  __DSB();
  __ISB();
  nvmc_wait_ready();

  __enable_irq();

  return true;
}

//*****************************************
bool flash_erase_page(uint32_t page_addr) {
//*****************************************

  if (page_addr & (FLASH_PAGE_SIZE - 1)) return false;// reject if not aligned on a page address 0x1000, 0x2000, etc
  if ( (page_addr < FIRST_USER_FLASH_ADDR) || (page_addr >= LAST_USER_FLASH_ADDR) ) return false;

  __disable_irq();

  NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Een << NVMC_CONFIG_WEN_Pos);
  __DSB(); __ISB();
  nvmc_wait_ready();

  NRF_NVMC->ERASEPAGE = page_addr;
  __DSB(); __ISB();
  nvmc_wait_ready();

  NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);
  __DSB(); __ISB();
  nvmc_wait_ready();

  __enable_irq();
  return true;
}


//******************************************************
void eraseFlashPagesBeforeWrite (uint32_t addrWrite, uint16_t sWrite)
//******************************************************
{
  char a[256];

  if (sWrite == 0) return;
  if (addrWrite < FIRST_USER_FLASH_ADDR) return;
  
  uint32_t endAddr = addrWrite + (uint32_t)sWrite;

  if (endAddr < addrWrite) return;           // overflow
  if (endAddr >= LAST_USER_FLASH_ADDR) endAddr = LAST_USER_FLASH_ADDR -1; // -1 otherwise we delete non authorized page

  // Aligner sur pages
  uint32_t pageStart = addrWrite & ~(FLASH_PAGE_SIZE - 1);
  //uint32_t pageEnd   = (endAddr + (FLASH_PAGE_SIZE - 1)) & ~(FLASH_PAGE_SIZE - 1); // BUG!!
  uint32_t pageEnd   = endAddr   & ~(FLASH_PAGE_SIZE - 1);

  if (fPrintDebug)
  {
      sprintf (a, "OK (addrWrite= 0x%08lX, sWrite=%d). pageStart = 0x%08lX, pageEnd = 0x%08lX, endAddr = 0x%08lX", addrWrite , sWrite , pageStart, pageEnd , endAddr );
      serialPrintln(a);
  }

  if ( (addrWrite & (FLASH_PAGE_SIZE - 1)) != 0) // addrWrite is NOT at a start of page
   {
    pageStart += FLASH_PAGE_SIZE; // no need to delete it: it had been previsouly deleted during the previous Erase/write process.
    //if (fPrintDebug) { serialPrintln ("->addrWrite is NOT at a start of page: pageStart +=FLASH_PAGE_SIZE ");}
   }

  for (uint32_t page = pageStart; page <= pageEnd; page += FLASH_PAGE_SIZE) // we include pageEnd (<=)
  {
    if (fPrintDebug)   {
       sprintf (a, ">> Erase Flash Page. @: 0x%08lX", page);
       serialPrintln(a);
    }

    (void)flash_erase_page(page);

  }

}

//*********************
void storeDataRecords()
//*********************
// ASSUMPTION: we have read the data before to get info on data structure
{
uint32_t addr = FIRST_USER_FLASH_ADDR;
recordHead_t header,headerRead;
bool ret;
uint32_t sWrite;
uint16_t cRecordsEndFlash;
uint16_t cRecordsLastBloc;
char  a[256];

if (!cRecords) return; // nothing to write

readDataRecords(false); // to force upating the value of flash_nextWriteAddr by reading flash last data (without loading them)

if (flash_nextWriteAddr == 0) addr = FIRST_USER_FLASH_ADDR; // nothing was loaded from flash
else addr = flash_nextWriteAddr; // points to the free address where to write the sequence of data

flash_lastCounter++;

if (fPrintDebug) { sprintf(a , ">> Call to StoreDataRecords. cRecords = %d, flash_cRecords = %d, flash_Counter=%d, flash_nextWriteAddr= 0x%08lX", cRecords, flash_cRecords, flash_lastCounter, flash_nextWriteAddr); serialPrintln(a); }
sWrite = cRecords * (sizeof (recordData_t));

if (sWrite > USER_FLASH_SIZE - sizeof(header)) // if total including header does not fit in the user flash size, we suppress lasts records
{
  // we truncate
  cRecords = (USER_FLASH_SIZE - sizeof (recordHead_t)) / sizeof(recordData_t); 
  //cRecords-=1; //in case we are at the limit & have a bug on limits...
  sWrite = cRecords * (sizeof (recordData_t));
  flash_nextWriteAddr = FIRST_USER_FLASH_ADDR;
  addr = FIRST_USER_FLASH_ADDR;
  flash_lastCounter = 0; 

  if (fPrintDebug)
    {
      sprintf(a, "storeDataRecords - WARNING: data too big. Truncate to %d records (%d bytes)", cRecords , sWrite);
      serialPrintln(a); 
    }
}

header.magic1 = MAGIC_NUMBER_HEAD_1;
header.magic2 = MAGIC_NUMBER_HEAD_2;
header.magic3 = MAGIC_NUMBER_HEAD_3;

header.counter = flash_lastCounter;
header.cRecords = cRecords;

header.biasGx = biasGx;
header.biasGy = biasGy;
header.biasGz = biasGz;

//vPower = readVIN();
header.vPower = (uint16_t) (vPower *1000);

eraseFlashPagesBeforeWrite(addr , sizeof (header) );

ret = flash_write_words( addr, (uint32_t*)&header, sizeof (header)/4);

addr += sizeof(recordHead_t); // move at start of data records

cRecordsEndFlash = 0;
cRecordsLastBloc = cRecords; // by default, we consider that there is only 1 bloc to write and no segmentation

if (addr + sWrite > LAST_USER_FLASH_ADDR) // data is too long to fit inside the flash user space=> need to cut in 2 blocs. eg: F3000 + 1000 = F4000 => it fits in 1 bloc. 
 {
  cRecordsEndFlash= (LAST_USER_FLASH_ADDR - addr) / sizeof (recordData_t);

  if (fPrintDebug)    {
      sprintf(a, "storeDataRecords - Need to cut data in 2 blocs. First bloc: %d records. size=%d", cRecordsEndFlash , cRecordsEndFlash *sizeof(recordData_t));
      serialPrintln(a); 
    }

  eraseFlashPagesBeforeWrite(addr , cRecordsEndFlash * (sizeof (recordData_t)) );
  ret = flash_write_words( addr , (uint32_t*)&tabRecords[0], cRecordsEndFlash * (sizeof (recordData_t)/4));
  
  cRecordsLastBloc = cRecords - cRecordsEndFlash;// remaining records to store at the begining
  addr = FIRST_USER_FLASH_ADDR; // move to the begining of user flash to write the second bloc of data
 }

eraseFlashPagesBeforeWrite(addr , cRecordsLastBloc * (sizeof (recordData_t) ));
// in case it is the 2nd bloc, we dont write a new header before the 2nd data bloc
ret = flash_write_words( addr , (uint32_t*)&tabRecords[cRecordsEndFlash], cRecordsLastBloc * (sizeof (recordData_t)/4) );


  if (fPrintDebug)    {
      sprintf(a, "storeDataRecords - Last (or only) bloc: %d records. Size = %d", cRecordsLastBloc , cRecordsLastBloc *sizeof(recordData_t) );
      serialPrintln(a); 
    }

}

//********************
bool searchFlashRecords()
//********************
{
  uint32_t addr;
  recordHead_t *p;
  char a[256];
  uint16_t flash_counterMax = 0;
  uint32_t flash_addrHeaderWithCounterMax = -1;
  uint16_t i;

p = (recordHead_t *) FIRST_USER_FLASH_ADDR;

if (fPrintDebug) serialPrintln ( "FileID| #Records | Voltage | storage@");

// search for the header record with the highest value of incremental counter (written the latest normally)
for ( addr = FIRST_USER_FLASH_ADDR, i = 0; addr < LAST_USER_FLASH_ADDR ; addr += 4, i++ )// we progress by bloc of 4 bytes (1 word for Flash)
  {
   p= (recordHead_t *) addr;

   if ( (p->magic1 == MAGIC_NUMBER_HEAD_1) && (p->magic2 == MAGIC_NUMBER_HEAD_2) && (p->magic3 ==MAGIC_NUMBER_HEAD_3) )
    {
      if (fPrintDebug)
        {
            sprintf (a, "%5d | %4d     | %4d    | 0x%08lX" , p->counter, p->cRecords, p->vPower , addr);
            serialPrintln (a);
        }

      if (counterToRead && (counterToRead == p->counter) ) // we search for a specific counter, we found it
      {
          flash_addrHeaderWithCounterMax = addr;
          addr = LAST_USER_FLASH_ADDR; // dirty way to force exiting the for loop...
      }
      else if ( (i == 0) || (p->counter > flash_counterMax)) // in case the first bloc that we read is a header
        {

          flash_counterMax = p->counter;
          flash_addrHeaderWithCounterMax = addr;         
        } 
    }
  }

if (fPrintDebug && fMaxTrace) // display Flash info
{
  serialPrintln("===============================================");
  serialPrintln(" Flash memory - User storage info: ");
  sprintf (a, " User Memory @start: 0x%08lX, @end: 0x%08lX , Length: 0x%04X (%d bytes) , #Flash Pages of 4K: %d" , FIRST_USER_FLASH_ADDR , LAST_USER_FLASH_ADDR, LAST_USER_FLASH_ADDR- FIRST_USER_FLASH_ADDR , LAST_USER_FLASH_ADDR- FIRST_USER_FLASH_ADDR , (LAST_USER_FLASH_ADDR - FIRST_USER_FLASH_ADDR)/4096); 
  serialPrintln (a);
  sprintf (a, "# of data records space on Flash: %d", i);
  serialPrintln (a);
}

if (flash_addrHeaderWithCounterMax == -1) // no data found
 {
  flash_lastCounter = 0;
  flash_cRecords = 0;

  if (fPrintDebug)      {     serialPrintln("* WARNING *: no data found on flash");serialPrintln("===============================================");    }
  
  return false;
 }

else // there are valid data records on flash 
  {
    p = (recordHead_t *) flash_addrHeaderWithCounterMax;

    // Sure? overwrite Gyrobias?
    biasGx = p-> biasGx;
    biasGy = p-> biasGy;
    biasGz = p-> biasGz;

    // !!! MODE: we append new records after the ones in flash=> CHANGE AFTER ??
    flash_cRecords = p->cRecords;
    flash_lastCounter = p->counter;
    flash_addrFirstDataRecord = flash_addrHeaderWithCounterMax + (sizeof (recordHead_t) );

    if (flash_addrFirstDataRecord >= LAST_USER_FLASH_ADDR) // we reached the end of user memory. The first record is at the begining of the circular storage
    {
      flash_addrFirstDataRecord = FIRST_USER_FLASH_ADDR;
    }

    if (fMaxTrace)
    {
      sprintf (a, "Highest counter Data header found on flash at @ 0x%08lX . flash_lastCounter# = %d. flash_cRecords = %d", flash_addrHeaderWithCounterMax , flash_lastCounter , flash_cRecords);
      serialPrintln(a);
    }
  }

return true;
}


//********************
void readDataRecords( bool fOverwriteTabRecords)
//********************
{
uint8_t *pAddrDest;
uint16_t sBloc, sDataRecords;
char a[256];

// BUG: precedant write ecrit 4 bytes plus loin que calculé dans cette fonction...

if (!searchFlashRecords()) // no data on flash
{
  flash_nextWriteAddr = FIRST_USER_FLASH_ADDR;
  //flash_lastPageOfFoundData = -1;
  // nothing else??
  if (fOverwriteTabRecords) cRecords = 0;
}

// flash_cRecords, flash_addrFirstDataRecord  are set by function searchFlashRecords

else if (flash_cRecords)
  {
    if (fOverwriteTabRecords) cRecords = flash_cRecords;

    sDataRecords = flash_cRecords * (sizeof (recordData_t));

    if ( flash_addrFirstDataRecord + sDataRecords <= LAST_USER_FLASH_ADDR )// data fits in one single bloc, doesnt reach end of circular storage. Eg: F3000+ 1000 = F4000 fits 
      {
        sBloc = sDataRecords;
        flash_nextWriteAddr = flash_addrFirstDataRecord + sDataRecords;

        // if next write of the header overflaws limit => move to the start of flash for next write 
        if ( flash_nextWriteAddr + sizeof (recordHead_t) >= LAST_USER_FLASH_ADDR) flash_nextWriteAddr= FIRST_USER_FLASH_ADDR;
        
      }
    else
      {
        sBloc = LAST_USER_FLASH_ADDR - flash_addrFirstDataRecord; // first part of records at the end of circular storage
        sBloc = (sBloc / sizeof (recordData_t)) * sizeof (recordData_t); // works as records are multiple of 2...
      }

    if (fOverwriteTabRecords) { if (sBloc) memcpy(&tabRecords[0], (void *) flash_addrFirstDataRecord , sBloc);}

    if (sBloc < sDataRecords) // Data in 2 blocs- end of data is at the begining of circular storage. Append it to tabRecords
      {
        pAddrDest = (uint8_t *) tabRecords;
        pAddrDest += sBloc; // points to the next record to be written in tabRecords

        sBloc = sDataRecords - sBloc;
        
        if (fOverwriteTabRecords) memcpy(pAddrDest , (void *) FIRST_USER_FLASH_ADDR , sBloc);

        flash_nextWriteAddr = FIRST_USER_FLASH_ADDR + sBloc;
      }
 
  }
else // cRecords on flash = 0
{
  flash_nextWriteAddr= flash_addrFirstDataRecord;
}

if (fOverwriteTabRecords) cRecords = flash_cRecords;
if (fMaxTrace)
{
  sprintf (a, ">> Call to readDataRecords. flash_nextWriteAddr= 0x%08lX , cRecords = %d , flash_cRecords = %d ", flash_nextWriteAddr, cRecords , flash_cRecords );
  serialPrintln (a);
}

// should we ignore the test for each start of page? as we dont delete if it is a start of page. => if (flash_nextWriteAddr != FIRST_USER_FLASH_ADDR)
recordHead_t *p;
p = (recordHead_t*) flash_nextWriteAddr;
if ( (counterToRead == 0) && (p->magic1 != 0xFF || p->magic2 != 0xFF ||p->magic3 != 0xFF) )
 {
  if (fPrintDebug) { serialPrintln (" * WARNING * : flash_nextWriteAddr is not empty! means storage structure pb IF flash_nextWriteAddr is not aligned om a page start ");}
 }
  
}

//*****************************************
void millisToHMSms(uint32_t ms, char* buf)
//*****************************************
// buf must be minimum 13 bytes: "HH:MM:SS.mmm\0"
{
  uint32_t totalSeconds = ms / 1000;
  uint32_t milliseconds = ms % 1000;

  uint32_t seconds = totalSeconds % 60;
  uint32_t minutes = (totalSeconds / 60) % 60;
  uint32_t hours   = (totalSeconds / 3600);

  // HH:MM:SS.mmm
  sprintf(buf, "%02lu:%02lu:%02lu.%03lu",
           (unsigned long)hours,
           (unsigned long)minutes,
           (unsigned long)seconds,
           (unsigned long)milliseconds);
}

//********************
void displayLog()
//********************
{
recordLog_t *pLog;
uint16_t cRecForText = 0;

char aMillis[15];
uint i;

serialPrintln("====== LOG ======");
serialPrintln(" Time        | Type\t| Code & Text");

for (i=0; i< cRecords ; i++)
  {
    if ( tabRecords[i].position < LOGTYPE_SUCCESS) continue;
  
    pLog = (recordLog_t*)(&tabRecords[i]);
    millisToHMSms(pLog->millis , aMillis);

    sprintf(aDebug, "%s | %s  | %s\t", aMillis, aPosText[pLog->recordType], aLogText[pLog->code] );
    if (pLog->sText)
      {
         cRecForText = (pLog->sText + sizeof (recordLog_t) - 1u) / sizeof (recordLog_t);  // to add 1 record, as any started record <8 bytes is counted
         i+= cRecForText;
         strcat (aDebug , "-> ");
         strncat (aDebug , (const char*) (pLog+1) , pLog->sText);
      }
    serialPrintln(aDebug);
  }

serialPrintln("=================");
}

//********************
void displayRecordsScreen()
//********************
{
char a[256];
uint i;
recordLog_t *pLog;
uint16_t cRecForText = 0;

sprintf ( a, "====== Data Records ======" , cRecords);
serialPrintln(a);

serialPrintln("Rec | XLmax | Pos | Bpm | Bmax | cSnores | cSmallMoves");

for (i=0; i< cRecords ; i++)
  {

    if ( tabRecords[i].position >= LOGTYPE_SUCCESS) // we jump over log
      {
  
        pLog = (recordLog_t*)(&tabRecords[i]);

        if (pLog->sText)
          {
            cRecForText = (pLog->sText + sizeof (recordLog_t) - 1u) / sizeof (recordLog_t);  // to add 1 record, as any started record <8 bytes is counted
            i+= cRecForText;
          }
      }
    else
      {
        sprintf(a, "%4d| %5d | %2d  | %3d | %3d  | %5d  | %5d", i , tabRecords[i].time, tabRecords[i].position, tabRecords[i].breathPerMinut , tabRecords[i].breathMax, tabRecords[i].cSnores, tabRecords[i].cSmallMoves);
        serialPrintln(a);
      }
  }
}

//********************
void displayRecordsForCSV()
//********************
{
char a[256];
uint i;

serialPrintln("Index;AccelMax;Position;BreathPerMinute;BreathMax;cSnores;cSmallMoves");

for (i=0; i< cRecords ; i++)
  {
    if ( tabRecords[i].position >= LOGTYPE_SUCCESS) continue;
    sprintf(a, "%03d;%02d;%02d;%02d;%02d;%02d;%02d", i , tabRecords[i].time, tabRecords[i].position, tabRecords[i].breathPerMinut , tabRecords[i].breathMax, tabRecords[i].cSnores , tabRecords[i].cSmallMoves);
    serialPrintln(a);
  }
}
extern uint16_t cSnoreAlerts;
//******************************
void addDataRecord()
//******************************
{
if(cRecords < N_RECORDS_MAX)
 {
  tabRecords[cRecords].time = acceleroMax; // for time storage: (millis() - programStartMs) / 1000;
  acceleroMax = 0;

  tabRecords[cRecords].position = position; 
  tabRecords[cRecords].breathPerMinut = breathCount;
  tabRecords[cRecords].breathMax = breathMax/1000;
  tabRecords[cRecords].cSnores = cSnores;
  tabRecords[cRecords].cSmallMoves = cSnoreAlerts; //cSmallMoves;
  cSnoreAlerts = 0;
  
  if (fPrintDebug) 
    {
      serialPrint("[Store record # "); serialPrint (cRecords); serialPrint(" ]: "); 
      serialPrint (tabRecords[cRecords].time); serialPrint ("|");
      serialPrint (tabRecords[cRecords].position);serialPrint ("|");
      serialPrint (tabRecords[cRecords].breathPerMinut);serialPrint ("|");
      serialPrint (tabRecords[cRecords].breathMax);serialPrint ("|");
      serialPrint (tabRecords[cRecords].cSnores); serialPrint ("|");
      serialPrintln (tabRecords[cRecords].cSmallMoves);

    }
  cRecords++;
 }

}

//******************************
void addLogRecord(uint8_t type , uint8_t code , char *pText)
//******************************
{
recordLog_t *pLog;
uint16_t cRecForText = 0;
uint16_t sText= 0;

if(cRecords >= N_RECORDS_MAX) return;

if (pText != NULL) sText = strlen(pText);
if (sText) // there is a free text to append, compute how many records it requires
  {
    cRecForText = (sText + sizeof (recordLog_t) - 1u) / sizeof (recordLog_t);  // to add 1 record, as any started record <8 bytes is counted
    if (cRecords + cRecForText >= N_RECORDS_MAX) { sText = 0; cRecForText = 0; } // simply removes the text
  }

pLog = (recordLog_t*)(&tabRecords[cRecords]);

pLog->recordType = type; 
pLog->code = code;
pLog->millis = millis();
pLog->sText = sText;

if (sText)  {    memcpy ( (pLog+1) , pText , sText);  }

cRecords++;// for header
cRecords += cRecForText;// for text

}

extern uint16_t readI();

//*************************************
void addPowerRecord(const char * pText)
//*************************************
{
  uint16_t p;
  if (!fLogPower) return;
  
  p = readI();
  sprintf (aDebug , "%5dmA (%+5d) %s", p, p - previousI , pText );
  addLogRecord(LOGTYPE_INFO , LOG_POWER_CONSUMPTION , aDebug);
  previousI = p;
}

//******************************
void testFlash()
//******************************
{
uint16_t i;
if (fPrintDebug)
{
  serialPrintln(">> Call to testFlash");
}
for ( cRecords =0, i = 0; cRecords < 600 ; cRecords++) //600 = 10 hours= 1 good night
 {
  tabRecords[cRecords].time = i;
  tabRecords[cRecords].position = i+1; 
  tabRecords[cRecords].breathPerMinut = i+2;
  tabRecords[cRecords].breathMax = i+3;
  i+=4;
 }

//searchFlashRecords();
storeDataRecords();
//searchFlashRecords();
//displayRecordsScreen();

}