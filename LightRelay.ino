#define PIN_RELAY 13

void setup()
{
  // put your setup code here, to run once:
  pinMode(PIN_RELAY, OUTPUT);
}

void loop()
{
  // put your main code here, to run repeatedly:
  digitalWrite(PIN_RELAY, HIGH);
  delay(2000);
  digitalWrite(PIN_RELAY, LOW);
  delay(2000);
}

