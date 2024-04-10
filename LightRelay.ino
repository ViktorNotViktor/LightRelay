#include <Adafruit_SSD1306.h>
#include <DS3231M.h>

#define DECODE_NEC
#include <IRremote.hpp>

// Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define OLED_RESET     -1 //4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Relay
#define PIN_RELAY 13

// Light sensor
#define PIN_LIGHT A0
#define AVERAGE_MS 1000
int g_light_val = 0;
int g_light_samples = 0;
unsigned long g_light_accumulated_val = 0;
unsigned long g_light_accumulated_start_msec = 0;

// RTC
DS3231M_Class DS3231M;
uint8_t g_start_hour = 0, g_start_minute = 0, g_end_hour = 0, g_end_minute = 0;

// Remote
#define PIN_REMOTE 2

bool g_is_opened_by_rtc = false;
bool g_is_opened_by_light = false;
bool g_is_opened_by_histeresis = false;
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
  // const size_t nBufferSize = 32; //TODO: 20
  // char szBuffer[nBufferSize];
  // sprintf(szBuffer, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  // Serial.println(szBuffer);
  // delay(200);
}

////////////////////////////////////////////////////////
void processLightSensor()
{
  int light_sample_val = analogRead(PIN_LIGHT);

  unsigned long nNowMS = millis();
  if (nNowMS - g_light_accumulated_start_msec < AVERAGE_MS)
  {
    g_light_accumulated_val += light_sample_val;
    g_light_samples++;
  }
  else
  {
    g_light_val = g_light_accumulated_val / g_light_samples;
    g_light_accumulated_val = light_sample_val;
    g_light_samples = 1;
    g_light_accumulated_start_msec = nNowMS;
  }
  //Serial.print("Light=");
  //Serial.println(g_light_val);
}

////////////////////////////////////////////////////////
void processHisteresis()
{}

////////////////////////////////////////////////////////
void processRelay()
{
  g_open_relay = g_is_opened_by_rtc && g_is_opened_by_light && g_is_opened_by_histeresis;
  digitalWrite(PIN_RELAY, g_open_relay ? HIGH : LOW);
}

////////////////////////////////////////////////////////
void processDisplay()
{
  display.clearDisplay();
  display.setCursor(0, 0);
	display.setTextSize(1);

  setSelectedColor(g_is_opened_by_rtc);
  
  display.print('T');
  setSelectedColor(false);
  display.print(' ');

  printCurrentTime(false);
  printStartTime(true);
  printEndTime(false);

	// display.setCursor(0, 0);
	// display.setTextColor(WHITE);
	// const size_t buffer_size = 20;
  // char buffer[buffer_size];
  // sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d", g_now.year(), g_now.month(), g_now.day(), g_now.hour(), g_now.minute(), g_now.second());
  // display.println(szBuffer);

  // display.setTextSize(1);
  // display.print("#");
  // display.println(g_button, HEX);
  // display.setTextSize(2);
  // display.print("L=");
  // display.print(g_light_val);
	// int x = display.getCursorX();
	// int y = display.getCursorY();
	// display.setTextSize(1);
	// display.print("CO2");
	// display.setCursor(x, y + 8);
	// display.println("PPM\n");

	// unsigned long lSeconds = (current_msec / 1000) % 60;
	// unsigned long lMinutes = (current_msec / 1000 / 60) % 60;
	// unsigned long lHours = current_msec / 1000 / 60 / 60;

	// constexpr int nBufferSize = 16;
	// char szBuffer[nBufferSize];
	// snprintf(szBuffer, nBufferSize, "%02lu:%02lu:%02lu", lHours, lMinutes, lSeconds);
	// display.print(szBuffer);

	display.display();
}

void setSelectedColor(bool is_selected)
{
  if(is_selected)
    display.setTextColor(BLACK, WHITE);
  else
    display.setTextColor(WHITE, BLACK);
}

void printCurrentTime(bool is_selected)
{
  setSelectedColor(is_selected);

  constexpr int buffer_size = 6;
  char buffer[buffer_size];
  sprintf(buffer, "%02u:%02u", g_now.hour(), g_now.minute());
  display.print(buffer);
}

void printStartTime(bool is_selected)
{
  setSelectedColor(false);
  display.print(" [");

  setSelectedColor(is_selected);

  constexpr int buffer_size = 6;
  char buffer[buffer_size];
  sprintf(buffer, "%02u:%02u", g_start_hour, g_start_minute);
  display.print(buffer);
}

void printEndTime(bool is_selected)
{
  setSelectedColor(false);
  display.print('-');

  setSelectedColor(is_selected);
  constexpr int buffer_size = 6;
  char buffer[buffer_size];
  sprintf(buffer, "%02u:%02u", g_end_hour, g_end_minute);
  display.print(buffer);

  setSelectedColor(false);
  display.print(']');
}

////////////////////////////////////////////////////////
void loop()
{
  processRemote();
  processRTC();
  processLightSensor();
  processHisteresis();

  processRelay();
  processDisplay();
}

