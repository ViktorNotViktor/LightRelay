#include <Adafruit_SSD1306.h>
#include <DS3231M.h>
#include <EEPROM.h>

#define DECODE_NEC
#include <IRremote.hpp>

// Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define OLED_RESET     -1 //4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

enum ScreenMode { Main, Settings };
ScreenMode g_screen_mode = ScreenMode::Main;

enum Setting { LightLower, LightUpper, StartTime, EndTime, Cooldown, ScreenOff, CurrentDate, CurrentTime, Setting_Size };
Setting g_setting = Setting::LightLower;

enum InputMode { View, Edit };
InputMode g_input_mode = InputMode::View;

uint32_t g_temp32 = 0;
uint32_t g_temp32_limit = 0;

// pins
#define PIN_RELAY 13
#define PIN_LIGHT A0
#define PIN_REMOTE 2

// Relay


// Light sensor
#define LIGHT_AVERAGE_SEC 1
int g_light_val = 0;
int g_light_lower = 0;
int g_light_upper = 0;
int g_light_samples = 0;
unsigned long g_light_accumulated_val = 0;
DateTime g_light_accumulated_start;

// RTC
DS3231M_Class DS3231M;
uint8_t g_start_hour = 0;
uint8_t g_start_minute = 0;
uint8_t g_end_hour = 0;
uint8_t g_end_minute = 0;

uint8_t g_cooldown_mins = 0;
DateTime g_cooldown_start;

uint8_t g_screen_off_mins = 0;
DateTime g_screen_off_start;

// Remote
bool g_is_opened_by_time = false;
bool g_is_opened_by_light = false;
bool g_open_relay = false;

DateTime g_now;
uint16_t g_button = 0;
unsigned long g_last_button_msec = 0;
#define BUTTON_THRESHOLD_MSEC 250

#define BTN_1 0x45
#define BTN_2 0x46
#define BTN_3 0x47
#define BTN_4 0x44
#define BTN_5 0x40
#define BTN_6 0x43
#define BTN_7 0x07
#define BTN_8 0x15
#define BTN_9 0x09
#define BTN_0 0x19
#define BTN_ASTERISK   0x16
#define BTN_HASH 0x0D
#define BTN_UP 0x18
#define BTN_DOWN 0x52
#define BTN_LEFT 0x08
#define BTN_RIGHT 0x5A
#define BTN_OK  0x1C

#define DEBUG
#ifdef DEBUG
#define LOG(x) Serial.print(x);
#define LOGLN(x) Serial.println(x);
#else
#define LOG(x)
#define LOGLN(x)
#endif

/////////////////////////////////////////////////////////
void setup()
{
  // debug console
#ifdef DEBUG
  Serial.begin(9600);
#endif

  // EEPROM
  readEEPROM();

  // Relay
  pinMode(PIN_RELAY, OUTPUT);

  // Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) // 0xD ?
	{
		LOGLN(F("SSD1306 allocation failed"));
  }

	display.clearDisplay();
	display.cp437(true);
	display.display();

  // RTC
  for (int i = 0; i < 5 && !DS3231M.begin(); i++)
  {
    LOGLN("Failed to init RTC");
    display.print("RTC failed ");
    display.display();
    delay(1000);
  }

  g_cooldown_start = DS3231M.now();

  // Light
  pinMode(PIN_LIGHT, INPUT);
  g_light_accumulated_start = DS3231M.now();

  // Remote
  IrReceiver.begin(PIN_REMOTE);
}

////////////////////////////////////////////////////////
// enum EEPROM_Size {
//   Size_LightLower = 2,
//   Size_LightUpper = 2,
//   Size_StartHour = 1,
//   Size_StartMinute = 1,
//   Size_EndHour = 1,
//   Size_EndMinute = 1,
//   Size_Cooldown = 1,
//   Size_ScreenOff = 1,
// };

// enum EEPROM_Address_Size {
//   Addr_LightLower = 0,
//   Addr_LightUpper = Addr_LightLower + Size_LightLower,
//   Addr_StartHour = Addr_LightUpper + Size_LightUpper,
//   Addr_StartMinute = Addr_StartHour + Size_StartHour,
//   Addr_EndHour = Addr_StartMinute + Size_StartMinute,
//   Addr_EndMinute = Addr_EndHour + Size_EndHour,
//   Addr_Cooldown = Addr_EndMinute + Size_EndMinute,
//   Addr_ScreenOff = Addr_Cooldown + Size_Cooldown,
// };

#define SETTING_READ(x) { EEPROM.get(addr, x); addr += sizeof(x); }
#define SETTING_WRITE(x) { EEPROM.put(addr, x); addr += sizeof(x); }

void readEEPROM()
{
  int addr = 0;
  SETTING_READ(g_light_lower);
  SETTING_READ(g_light_upper);
  SETTING_READ(g_start_hour);
  validateHour(g_start_hour);
  SETTING_READ(g_start_minute);
  validateMinute(g_start_minute);
  SETTING_READ(g_end_hour);
  validateHour(g_end_hour);
  SETTING_READ(g_end_minute);
  validateMinute(g_end_minute);
  SETTING_READ(g_cooldown_mins);
  validate2Digit(g_cooldown_mins);
  SETTING_READ(g_screen_off_mins);
  validate2Digit(g_screen_off_mins);
}

void writeEEPROM()
{
  int addr = 0;
  SETTING_WRITE(g_light_lower);
  SETTING_WRITE(g_light_upper);
  SETTING_WRITE(g_start_hour);
  SETTING_WRITE(g_start_minute);
  SETTING_WRITE(g_end_hour);
  SETTING_WRITE(g_end_minute);
  SETTING_WRITE(g_cooldown_mins);
  SETTING_WRITE(g_screen_off_mins);
}

bool validateHour(uint8_t &hour)
{
  bool is_valid = hour <= 24;
  if(!is_valid)
  {
    hour = 0;
  }
  return is_valid;
}

bool validateMinute(uint8_t &minute)
{
  bool is_valid = minute <= 59;
  if(!is_valid)
  {
    minute = 0;
  }
  return is_valid;
}

bool validate2Digit(uint8_t &number)
{
  bool is_valid = number <= 99;
  if(!is_valid)
  {
    number = 0;
  }
  return is_valid;
}


////////////////////////////////////////////////////////
void processRemote()
{
  if (IrReceiver.decode()) {

        /*
         * Print a summary of received data
         */
        if (IrReceiver.decodedIRData.protocol == UNKNOWN) {
           LOGLN(F("IR unknown"));
            // We have an unknown protocol here, print extended info
#ifdef DEBUG            
            IrReceiver.printIRResultRawFormatted(&Serial, true);
#endif            
            IrReceiver.resume(); // Do it here, to preserve raw data for printing with printIRResultRawFormatted()
        } else {
            IrReceiver.resume(); // Early enable receiving of the next IR frame
#ifdef DEBUG            
            IrReceiver.printIRResultShort(&Serial);
            IrReceiver.printIRSendUsage(&Serial);
#endif            
        }
        LOGLN("");

        if (millis() - g_last_button_msec > BUTTON_THRESHOLD_MSEC)
        {
          g_button = IrReceiver.decodedIRData.command;
          g_last_button_msec = millis();
        }
        
        // /*
        //  * Finally, check the received data and perform actions according to the received command
        //  */
        // if (IrReceiver.decodedIRData.command == 0x10) {
        //     // do something
        // } else if (IrReceiver.decodedIRData.command == 0x11) {
        //     // do something else
        // }
    }
}
////////////////////////////////////////////////////////
void resetButton()
{
  g_button = 0;
}

////////////////////////////////////////////////////////
void processRTC()
{
  g_now = DS3231M.now();
  
  int now_minutes = g_now.hour() * 60 + g_now.minute();
  int start_minutes = g_start_hour * 60 + g_start_minute;
  int end_minutes = g_end_hour * 60 + g_end_minute;

  if(start_minutes < end_minutes)
  {
    int32_t current_cooldown_mins = (g_now - g_cooldown_start).totalseconds() / 60;
    g_is_opened_by_time = start_minutes <= now_minutes && now_minutes <= end_minutes
                          && current_cooldown_mins > g_cooldown_mins;
  }
  else
  {
    g_is_opened_by_time = false;
  }
}

////////////////////////////////////////////////////////
void processLightSensor()
{
  int light_sample_val = analogRead(PIN_LIGHT);

  TimeSpan diff = g_now - g_light_accumulated_start; 
  if (diff.totalseconds() < LIGHT_AVERAGE_SEC)
  {
    g_light_accumulated_val += light_sample_val;
    g_light_samples++;
  }
  else
  {
    g_light_val = g_light_accumulated_val / g_light_samples;

    g_light_accumulated_val = light_sample_val;
    g_light_samples = 1;
    g_light_accumulated_start = g_now;

    g_is_opened_by_light = g_light_val >= (g_is_opened_by_light ? g_light_lower : g_light_upper);
  }
}


////////////////////////////////////////////////////////
void processRelay()
{
  g_open_relay = g_is_opened_by_time && g_is_opened_by_light;
  digitalWrite(PIN_RELAY, g_open_relay ? HIGH : LOW);
}

////////////////////////////////////////////////////////
void processDisplay()
{
  switch(g_button)
  {
    case BTN_HASH:
      if(g_screen_mode == ScreenMode::Main)
      {
        g_screen_mode = ScreenMode::Settings;
        g_input_mode = InputMode::View;
      }
      resetButton();
      break;
    
    case BTN_ASTERISK:
      if(g_screen_mode == ScreenMode::Settings)
      {
        if (g_input_mode == InputMode::View)
        {
          g_screen_mode = ScreenMode::Main;
        }
        else
        {
          g_input_mode == InputMode::View;
        }
      }
      resetButton();
      break;
  }

  switch(g_screen_mode)
  {
    case ScreenMode::Main:
      printMainScreen();
      break;

    case ScreenMode::Settings:
      printSettingsScreen();
      break;
  }

  display.display();
}

///////////////////////////////////////////////////////////
void setSelectedColor(bool is_selected)
{
  if(is_selected)
    display.setTextColor(BLACK, WHITE);
  else
    display.setTextColor(WHITE, BLACK);
}

//////////////////////////////////////////////////////////
void gotoChar(uint8_t row, uint8_t col)
{
  display.setCursor(col * 6, row * 8);
}

void printMainScreen()
{
  display.clearDisplay();

  // current time
  setSelectedColor(g_is_opened_by_time);
  display.setCursor(0, 0);
  display.setTextSize(2);
  printTime(g_now.hour(), g_now.minute());

  // start time
  setSelectedColor(false);
  int x = display.getCursorX();
  int y = display.getCursorY();
  display.setTextSize(1);
  printTime(g_start_hour, g_start_minute);

  // cooldown
  display.print(" CD=");
  display.print((g_now - g_cooldown_start).totalseconds() / 60);

  // end time
  display.setCursor(x, y + 8);
  printTime(g_end_hour, g_end_minute);

  // light value
  display.println();
  display.setTextSize(2);
  setSelectedColor(g_is_opened_by_light);
  printLight(g_light_val);

  // lower light bracket
  setSelectedColor(false);
  x = display.getCursorX();
  y = display.getCursorY();
  display.setTextSize(1);
  printLight(g_light_lower);

  display.setCursor(x, y + 8);
  printLight(g_light_upper);
}

void printTime(uint8_t hour, uint8_t minute)
{
  char buffer[6];
  sprintf(buffer, "%02u:%02u", hour, minute);
  display.print(buffer);
}

///////////////////////////////////////////////////////
void printLight(int light)
{
  char buffer[5];
  sprintf(buffer, "%4d", light);
  display.print(buffer);
}

void processViewSettingsButtons()
{
  if (g_button != BTN_RIGHT && g_button != BTN_LEFT && g_button != BTN_DOWN && g_button != BTN_UP
      && g_button != BTN_OK)
    return;

  constexpr uint8_t total_rows = 4;
  constexpr uint8_t total_cols = 3;
  // This array represents position of the settings on the screen.
  // Setting_Size means invalid value.
  Setting pos[total_rows][total_cols] = {
    { LightLower,   LightUpper,   Setting_Size  },
    { StartTime,    EndTime,      Cooldown      },
    { ScreenOff,    Setting_Size, Setting_Size  },
    { CurrentDate,  CurrentTime,  Setting_Size  },
  };

  // find position of the current setting
  uint8_t cur_row = 0, cur_col = 0;
  bool found = false;
  for (uint8_t row = 0; row < total_rows && !found; row++)
  {
    for (uint8_t col = 0; col < total_cols && !found; col++)
    {
      cur_row = row;
      cur_col = col;
      found = pos[row][col] == g_setting;
    }
  }

  auto fnLastInRow = [&](uint8_t row) -> Setting
  {
    // skip invalid values at the end
    uint8_t col = 0;
    for (col = total_cols - 1; col >= 0 && pos[row][col] == Setting_Size; col--);
    return pos[row][col];
  };

  auto fnFirstInLowerRow = [&](uint8_t row) -> Setting
  {
    if (row + 1 <= total_rows - 1)
      return pos[row + 1][0];
    else
      return pos[0][0];
  };

  auto fnFirstInUpperRow = [&](uint8_t row) -> Setting
  {
    if (row - 1 >= 0)
      return pos[row - 1][0];
    else
      return pos[total_rows - 1][0];
  };

  switch (g_button)  
  {
    case BTN_RIGHT:
      LOGLN("BTN_RIGH");
      if (cur_col + 1 < total_cols && pos[cur_row][cur_col + 1] != Setting_Size)
        g_setting = pos[cur_row][cur_col + 1];
      else
        g_setting = fnFirstInLowerRow(cur_row);
      break;

    case BTN_LEFT:
      LOGLN("BTN_LEFT");
      if (cur_col - 1 >= 0)
        g_setting = pos[cur_row][cur_col - 1];
      else
      {
        if (cur_row - 1 >= 0)
          g_setting = fnLastInRow(cur_row - 1);
        else
          g_setting = fnLastInRow(total_rows - 1);
      }
      break;

    case BTN_DOWN:
      LOGLN("BTN_DOWN");
      g_setting = fnFirstInLowerRow(cur_row);
      break;

    case BTN_UP:
      LOGLN("BTN_UP");
      g_setting = fnFirstInUpperRow(cur_row);
      break;

    case BTN_OK:
      LOGLN("BTN_OK");
      switch (g_input_mode)
      {
        case InputMode::View:
          g_input_mode = InputMode::Edit;
          setEditOptions();
          break;
        case InputMode::Edit:
          g_input_mode = InputMode::View;
          break;
      }
      break;
  }

  LOG("InputMode=");
  LOGLN(g_input_mode);

  resetButton();
}

/////////////////////////////////////////////////////////
void setEditOptions()
{
  g_temp32 = 0;

  switch (g_setting)
  {
    case LightLower:
    case LightUpper:
      g_temp32_limit = 1024;
      break;
      
    case StartTime:
    case EndTime:
    case CurrentTime:
      g_temp32_limit = 2359;
      break;
      
    case Cooldown:
    case ScreenOff:
      g_temp32_limit = 99;
      break;

    case CurrentDate:
      g_temp32_limit = 30001231;
      break;
  }
}

void processEditSettingsButtons()
{
  switch (g_button)
  {
    case BTN_OK:
      saveNewValue();
      g_input_mode = InputMode::View;
      break;
    case BTN_0:
      advanceTemp32Value(0);
      break;
    case BTN_1:
      advanceTemp32Value(1);
      break;
    case BTN_2:
      advanceTemp32Value(2);
      break;
    case BTN_3:
      advanceTemp32Value(3);
      break;
    case BTN_4:
      advanceTemp32Value(4);
      break;
    case BTN_5:
      advanceTemp32Value(5);
      break;
    case BTN_6:
      advanceTemp32Value(6);
      break;
    case BTN_7:
      advanceTemp32Value(7);
      break;
    case BTN_8:
      advanceTemp32Value(8);
      break;
    case BTN_9:
      advanceTemp32Value(9);
      break;
    default:
      return;
  }

  resetButton();
}

void saveNewValue()
{
  switch (g_setting)
  {
    case Setting::LightLower:
      g_light_lower = g_temp32;
      break;
    case Setting::LightUpper:
      g_light_upper = g_temp32;
      break;
    case Setting::StartTime:
      getTimeFromTempValue(g_start_hour, g_start_minute);
      break;
    case Setting::EndTime:
      getTimeFromTempValue(g_end_hour, g_end_minute);
      break;
    case Setting::Cooldown:
      g_cooldown_mins = g_temp32;
      break;
    case Setting::ScreenOff:
      g_screen_off_mins = g_temp32;
      break;
    case Setting::CurrentDate:
      {
        uint16_t year = 0;
        uint8_t month = 0, day = 0;
        getDateFromTempValue(year, month, day);
        DateTime dt(year, month, day, g_now.hour(), g_now.minute(), g_now.second());
        DS3231M.adjust(dt);
      }
      break;
    case Setting::CurrentTime:
      {
        uint8_t hour = 0, minute = 0;
        getTimeFromTempValue(hour, minute);
        DateTime dt(g_now.year(), g_now.month(), g_now.day(), hour, minute, 0);
        DS3231M.adjust(dt);
      }
      break;
  }

  writeEEPROM();

  g_temp32 = 0;
  g_temp32_limit = 0;
}

////////////////////////////////////////////////////////
void advanceTemp32Value(uint8_t digit)
{
  uint32_t new32 = g_temp32 * 10 + digit;
  if (new32 <= g_temp32_limit)
  {
    g_temp32 = new32;
  }
}

//void processViewSettingsButtons();
/////////////////////////////////////////////////////////
void printSettingsScreen()
{
  display.clearDisplay();

  // 128x32 pixels
  // 21x4 characters
  //+---------------------+
  //|LLO[XXXX] LHI[XXXX]..|
  //|[HH:NN]-[HH:NN]CD[XX]|
  //|SCR OFF[XX]..........|
  //|[YYYY-MM-DD][HH:NN]..|
  //+---------------------+

  switch (g_input_mode)
  {
    case InputMode::View:
      processViewSettingsButtons();
      break;

    case InputMode::Edit:
      processEditSettingsButtons();
      break;
  }

  
  
  enum Coord { ROW, COL, Coord_Size };
  uint8_t coord[Setting::Setting_Size][Coord_Size] = {
    { 0,  0  },  // Setting::LightLower
    { 0,  10 },  // Setting::LightUpper
    { 1,  0  },  // Setting::StartTime
    { 1,  8  },  // Setting::EndTime
    { 1,  15 },  // Setting::Cooldown
    { 2,  0  },  // Setting::ScreenOff
    { 3,  0  },  // Setting::CurrentDate
    { 3,  12 },  // Setting::CurrentTime
  };

  gotoChar(coord[Setting::LightLower][ROW], coord[Setting::LightLower][COL]);
  printSettingsLight("LLO[", g_light_lower, g_setting == Setting::LightLower);

  gotoChar(coord[Setting::LightUpper][ROW], coord[Setting::LightUpper][COL]);
  printSettingsLight("LHI[", g_light_upper, g_setting == Setting::LightUpper);

  gotoChar(coord[Setting::StartTime][ROW], coord[Setting::StartTime][COL]);
  printSettingsTime(g_start_hour, g_start_minute, g_setting == Setting::StartTime);

  display.print('-');

  gotoChar(coord[Setting::EndTime][ROW], coord[Setting::EndTime][COL]);
  printSettingsTime(g_end_hour, g_end_minute, g_setting == Setting::EndTime);

  gotoChar(coord[Setting::Cooldown][ROW], coord[Setting::Cooldown][COL]);
  printSettingsCooldown(g_setting == Setting::Cooldown);

  gotoChar(coord[Setting::ScreenOff][ROW], coord[Setting::ScreenOff][COL]);
  printSettingsScreenOff(g_setting == Setting::ScreenOff);

  gotoChar(coord[Setting::CurrentDate][ROW], coord[Setting::CurrentDate][COL]);
  printSettingsDate(g_setting == Setting::CurrentDate);

  gotoChar(coord[Setting::CurrentTime][ROW], coord[Setting::CurrentTime][COL]);
  printSettingsTime(g_now.hour(), g_now.minute(), g_setting == Setting::CurrentTime);
}

////////////////////////////////////////////////////////////
void printSetting(const char* prefix, const char* value, const char* suffix, bool is_selected)
{
    setSelectedColor(is_selected);
    display.print(prefix);
    setSelectedColor(is_selected && g_input_mode == InputMode::View);
    display.print(value);
    setSelectedColor(is_selected);
    display.print(suffix);
}
/////////////////////////////////////////////////////////
void getDateFromTempValue(uint16_t& year, uint8_t month, uint8_t day)
{
  year = g_temp32 / 10000;
  month = g_temp32 - year;
  day = g_temp32 - year - month;
}
/////////////////////////////////////////////////////////
void printSettingsDate(bool is_selected)
{
  char buffer[11];

  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  if (g_input_mode == InputMode::Edit)
  {
    getDateFromTempValue(year, month, day);
  }
  else
  {
    year = g_now.year();
    month = g_now.month();
    day = g_now.day();
  }

  snprintf(buffer, 11, "%04u-%02u-%02u", year, month, day);
  printSetting("[", buffer, "]", is_selected);
}

//////////////////////////////////////////////////////////////////////////
void getTimeFromTempValue(uint8_t& hour, uint8_t& minute)
{
  hour = g_temp32 / 100;
  minute = g_temp32 - hour;
}
//////////////////////////////////////////////////////////////////////////
void printSettingsTime(uint8_t hour, uint8_t minute, bool is_selected)
{
  char buffer[6];

  if (g_input_mode == InputMode::Edit)
  {
    getTimeFromTempValue(hour, minute);
  }

  snprintf(buffer, 6, "%02u:%02u", hour, minute);
  printSetting("[", buffer, "]", is_selected);
};

////////////////////////////////////////////////////////////
void printSettingsLight(const char* prefix, int light, bool is_selected)
{
  char buffer[6];
  snprintf(buffer, 5, "%04d", g_input_mode == InputMode::View ? light : g_temp32);
  printSetting(prefix, buffer, "]", is_selected);
};

////////////////////////////////////////////////////////
void printSettingsCooldown(bool is_selected)
{
  char buffer[4];
  snprintf(buffer, 3, "%02d", g_input_mode == InputMode::View ? g_cooldown_mins: g_temp32);
  printSetting("CD[", buffer, "]", is_selected);
};

////////////////////////////////////////////////////////
void printSettingsScreenOff(bool is_selected)
{
  char buffer[4];
  snprintf(buffer, 3, "%02d", g_input_mode == InputMode::View ? g_screen_off_mins : g_temp32);
  printSetting("SCR OFF[", buffer, "]", is_selected);
};

////////////////////////////////////////////////////////
void loop()
{
  processRemote();
  processRTC();
  processLightSensor();

  processRelay();
  processDisplay();
}

