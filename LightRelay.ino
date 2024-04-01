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
#define PIN_RELAY PIN_A0

// Light sensor
#define PIN_LIGHT A0
#define AVERAGE_MS 1000
int g_nLight = 0;
int g_nLightSamples = 0;
unsigned long g_nLightAccumulated = 0;
unsigned long g_nLightAccumulatedStartedMS = 0;

// RTC
DS3231M_Class DS3231M;

// Remote
#define PIN_REMOTE 2

bool g_bOpenedByRTC = false;
bool g_bOpenedByLightSensor = false;
bool g_bOpenedByHisteresis = false;
bool g_bOpenRelay = false;

DateTime g_dtNow;
uint16_t g_nButton = 0;
unsigned long g_nLastButtonPressedMS = 0;
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

        if (millis() - g_nLastButtonPressedMS > BUTTON_THRESHOLD_MS)
        {
          g_nButton = IrReceiver.decodedIRData.command;
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
  g_dtNow = DS3231M.now();
  // const size_t nBufferSize = 32; //TODO: 20
  // char szBuffer[nBufferSize];
  // sprintf(szBuffer, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  // Serial.println(szBuffer);
  // delay(200);
}

////////////////////////////////////////////////////////
void processLightSensor()
{
  int nCurrentLight = analogRead(PIN_LIGHT);

  unsigned long nNowMS = millis();
  if (nNowMS - g_nLightAccumulatedStartedMS < AVERAGE_MS)
  {
    g_nLightAccumulated += nCurrentLight;
    g_nLightSamples++;
  }
  else
  {
    g_nLight = g_nLightAccumulated / g_nLightSamples;
    g_nLightAccumulated = nCurrentLight;
    g_nLightSamples = 1;
    g_nLightAccumulatedStartedMS = nNowMS;
  }
  //Serial.print("Light=");
  //Serial.println(g_nLight);
}

////////////////////////////////////////////////////////
void processHisteresis()
{}

////////////////////////////////////////////////////////
void processRelay()
{
  g_bOpenRelay = g_bOpenedByRTC && g_bOpenedByLightSensor && g_bOpenedByHisteresis;
  digitalWrite(PIN_RELAY, g_bOpenRelay ? HIGH : LOW);
}

////////////////////////////////////////////////////////
void processDisplay()
{
  display.clearDisplay();

	display.setTextSize(1);
	display.setCursor(0, 0);
	display.setTextColor(WHITE);
	const size_t nBufferSize = 20;
  char szBuffer[nBufferSize];
  sprintf(szBuffer, "%04d-%02d-%02d %02d:%02d:%02d", g_dtNow.year(), g_dtNow.month(), g_dtNow.day(), g_dtNow.hour(), g_dtNow.minute(), g_dtNow.second());
  display.println(szBuffer);

  display.setTextSize(1);
  display.print("#");
  display.println(g_nButton, HEX);
  display.setTextSize(2);
  display.print("L=");
  display.print(g_nLight);
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

