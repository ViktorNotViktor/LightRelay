#include <Adafruit_SSD1306.h>
#include <DS3231M.h>

#define DECODE_NEC
#include <IRremote.hpp>

// Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define OLED_RESET     -1 //4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

enum ScreenMode { Main, Settings };
ScreenMode g_screen_mode = ScreenMode::Main;

enum Setting { LightLow, LightHigh, StartTime, EndTime, Cooldown, ScreenOff, CurrentDate, CurrentTime, Setting_Size };
Setting g_setting = Setting::LightLow;

// pins
#define PIN_RELAY 13
#define PIN_LIGHT A0
#define PIN_REMOTE 2

// Relay


// Light sensor
#define AVERAGE_MS 1000
int g_light_val = 0;
int g_light_lower = 0;
int g_light_upper = 0;
int g_light_samples = 0;
unsigned long g_light_accumulated_val = 0;
unsigned long g_light_accumulated_start_msec = 0;

// RTC
DS3231M_Class DS3231M;
uint8_t g_start_hour = 0;
uint8_t g_start_minute = 0;
uint8_t g_end_hour = 0;
uint8_t g_end_minute = 0;

uint8_t g_cooldown_mins = 0;
uint8_t g_screen_off_mins = 0;

// Remote
bool g_is_opened_by_time = false;
bool g_is_opened_by_light = false;
bool g_open_relay = false;

DateTime g_now;
uint16_t g_button = 0;
unsigned long g_last_button_msec = 0;
#define BUTTON_THRESHOLD_MS 200

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


/////////////////////////////////////////////////////////
void setup()
{
  // put your setup code here, to run once:

  // debug console
  Serial.begin(9600);

  // relay
  pinMode(PIN_RELAY, OUTPUT);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) // 0xD ?
	{
		Serial.println(F("SSD1306 allocation failed"));
  }

	display.clearDisplay();
	display.cp437(true);
	display.display();

  // RTC
  for (int i = 0; i < 5 && !DS3231M.begin(); i++)
  {
    Serial.println("Failed to init RTC");
    delay(1000);
  }

  // Light
  pinMode(PIN_LIGHT, INPUT);

  // Remote
  IrReceiver.begin(PIN_REMOTE);
}

////////////////////////////////////////////////////////
void processRemote()
{
  if (IrReceiver.decode()) {

        /*
         * Print a summary of received data
         */
        if (IrReceiver.decodedIRData.protocol == UNKNOWN) {
            Serial.println(F("IR unknown"));
            // We have an unknown protocol here, print extended info
            IrReceiver.printIRResultRawFormatted(&Serial, true);
            IrReceiver.resume(); // Do it here, to preserve raw data for printing with printIRResultRawFormatted()
        } else {
            IrReceiver.resume(); // Early enable receiving of the next IR frame
            IrReceiver.printIRResultShort(&Serial);
            IrReceiver.printIRSendUsage(&Serial);
        }
        Serial.println();

        if (millis() - g_last_button_msec > BUTTON_THRESHOLD_MS)
        {
          g_button = IrReceiver.decodedIRData.command;
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
void processRTC()
{
  g_now = DS3231M.now();
  
  int now_minutes = g_now.hour() * 60 + g_now.minute();
  int start_minutes = g_start_hour * 60 + g_start_minute;
  int end_minutes = g_end_hour * 60 + g_end_minute;

  if(start_minutes < end_minutes)
  {
    g_is_opened_by_time = start_minutes <= now_minutes && now_minutes <= end_minutes;
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

  unsigned long msec = millis();
  if (msec - g_light_accumulated_start_msec < AVERAGE_MS)
  {
    g_light_accumulated_val += light_sample_val;
    g_light_samples++;
  }
  else
  {
    g_light_val = g_light_accumulated_val / g_light_samples;
    g_light_accumulated_val = light_sample_val;
    g_light_samples = 1;
    g_light_accumulated_start_msec = msec;

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

  // TODO: countdown
  display.print(" timer");

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

/////////////////////////////////////////////////////////
void printSettingsScreen()
{
  // 21x4 characters
  //+---------------------+
  //|LLO[XXXX] LHI[XXXX]..|
  //|[HH:NN]-[HH:NN]CD[XX]|
  //|SCR OFF[XX]..........|
  //|[YYYY-MM-DD][HH:NN]..|
  //+---------------------+

  enum Coord { ROW, COL, Coord_Size };
  uint8_t coord[Setting::Setting_Size][Coord_Size] = {
    { 0,  0  },  // Setting::LightLow
    { 0,  10 },  // Setting::LightHigh
    { 1,  0  },  // Setting::StartTime
    { 1,  8  },  // Setting::EndTime
    { 1,  15 },  // Setting::Cooldown
    { 3,  0  },  // Setting::ScreenOff
    { 4,  0  },  // Setting::CurrentDate
    { 4,  12 },  // Setting::CurrentTime
  };
  
  gotoChar(coord[Setting::LightLow][ROW], coord[Setting::LightLow][COL]);
  printSettingsLight("LLO", g_light_lower, g_setting == Setting::LightLow);

  gotoChar(coord[Setting::LightHigh][ROW], coord[Setting::LightHigh][COL]);
  printSettingsLight("LHI", g_light_upper, g_setting == Setting::LightHigh);

  gotoChar(coord[Setting::StartTime][ROW], coord[Setting::StartTime][COL]);
  printSettingsTime(g_start_hour, g_start_minute, g_setting == Setting::StartTime);

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

void printSettingsDate(bool is_selected)
{
  setSelectedColor(false);
  display.print('[');

  setSelectedColor(is_selected);
  char buffer[11];
  sprintf(buffer, "%04u-%02u-%02u", g_now.year(), g_now.month(), g_now.day());
  display.print(buffer);

  setSelectedColor(false);
  display.print(']');
}

//////////////////////////////////////////////////////////////////////////
void printSettingsTime(uint8_t hour, uint8_t minute, bool is_selected)
{
  setSelectedColor(false);
  display.print('[');

  setSelectedColor(is_selected);
  char buffer[6];
  sprintf(buffer, "%02u:%02u", hour, minute);
  display.print(buffer);

  setSelectedColor(false);
  display.print(']');
};

////////////////////////////////////////////////////////////
void printSettingsLight(const char* prefix, int light, bool is_selected)
{
  setSelectedColor(false);
  display.print('[');
  display.print(prefix);

  setSelectedColor(is_selected);
  char buffer[6];
  sprintf(buffer, "%04d", light);
  display.print(buffer);

  setSelectedColor(false);
  display.print(']');
};

////////////////////////////////////////////////////////
void printSettingsCooldown(bool is_selected)
{
  setSelectedColor(false);
  display.print("CD[");

  setSelectedColor(is_selected);
  char buffer[4];
  sprintf(buffer, "%02d", g_cooldown_mins);
  display.print(buffer);

  setSelectedColor(false);
  display.print(']');
};

////////////////////////////////////////////////////////
void printSettingsScreenOff(bool is_selected)
{
  setSelectedColor(false);
  display.print("SCR OFF[");

  setSelectedColor(is_selected);
  char buffer[4];
  sprintf(buffer, "%02d", g_screen_off_mins);
  display.print(buffer);

  setSelectedColor(false);
  display.print(']');
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

