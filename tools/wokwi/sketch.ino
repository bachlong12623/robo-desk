/*
 * RoboDesk Wokwi smoke test — GPIO map matches board_pins.h
 * Import tools/wokwi/diagram.json + this sketch into a Wokwi ESP32-S3 project.
 */
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9
#define PIN_WS2812 38
#define WS2812_COUNT 12
#define PIN_TCRT_L 15
#define PIN_TCRT_R 7
#define PIN_RD04 40
#define PIN_USER_BTN 47
#define PIN_BAT_ADC 10
#define PIN_MOTOR_AIN1 1
#define PIN_MOTOR_AIN2 2
#define PIN_MOTOR_BIN1 41
#define PIN_MOTOR_BIN2 42

Adafruit_NeoPixel ring(WS2812_COUNT, PIN_WS2812, NEO_GRB + NEO_KHZ800);

static void scanI2C(void)
{
  Serial.println("I2C scan (expect 0x68 MPU6050):");
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  0x%02X\n", addr);
      found++;
    }
  }
  if (!found) {
    Serial.println("  (none)");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nRoboDesk Wokwi smoke test");

  pinMode(PIN_TCRT_L, INPUT_PULLUP);
  pinMode(PIN_TCRT_R, INPUT_PULLUP);
  pinMode(PIN_RD04, INPUT_PULLUP);
  pinMode(PIN_USER_BTN, INPUT_PULLUP);

  pinMode(PIN_MOTOR_AIN1, OUTPUT);
  pinMode(PIN_MOTOR_AIN2, OUTPUT);
  pinMode(PIN_MOTOR_BIN1, OUTPUT);
  pinMode(PIN_MOTOR_BIN2, OUTPUT);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  scanI2C();

  ring.begin();
  ring.setBrightness(32);
  ring.setPixelColor(0, ring.Color(0, 80, 0));
  ring.show();
  Serial.println("WS2812 ring: pixel 0 green");
}

void loop() {
  int batt = analogRead(PIN_BAT_ADC);
  Serial.printf(
    "cliff L=%d R=%d presence=%d btn=%d batt_adc=%d\n",
  digitalRead(PIN_TCRT_L), digitalRead(PIN_TCRT_R),
  digitalRead(PIN_RD04), digitalRead(PIN_USER_BTN), batt);

  static uint8_t phase;
  phase = (phase + 1) % 4;
  digitalWrite(PIN_MOTOR_AIN1, phase == 0);
  digitalWrite(PIN_MOTOR_AIN2, phase == 1);
  digitalWrite(PIN_MOTOR_BIN1, phase == 2);
  digitalWrite(PIN_MOTOR_BIN2, phase == 3);

  uint32_t hue = (millis() / 40) % 255;
  ring.setPixelColor(0, ring.gamma32(ring.ColorHSV(hue * 256, 255, 80)));
  ring.show();

  delay(400);
}
