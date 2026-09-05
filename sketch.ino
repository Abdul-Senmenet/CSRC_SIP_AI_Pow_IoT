#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <MAX30105.h>

// -------- PIN DEFINITIONS --------
#define MPU_ADDR     0x68
#define TMP117_ADDR  0x49

const int gsrPin   = A0;
const int ECG_PIN  = A1;
const int LO_PLUS  = 10;
const int LO_MINUS = 11;

// -------- DISPLAY --------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// -------- MPU6050 --------
int16_t accX, accY, accZ;
int16_t gyroX, gyroY, gyroZ;

float ax, ay, az;
float gx, gy, gz;

// -------- SENSORS --------
MAX30105 particleSensor;

int gsrValue;
int ecgValue;

bool leadsOff = false;

float temperature = 0.0;

// -------- CLASSIFICATION DISPLAY STATE --------
String currentLabel = "---";
float currentConfidence = 0.0;

// -------- AGGREGATE STATE (5s majority vote) --------
String aggregateState = "---";

// -------- LLM MESSAGE OVERLAY --------
String llmMessage = "";
bool llmMessageActive = false;
unsigned long llmMessageStart = 0;
const unsigned long LLM_DISPLAY_DURATION = 8000; // show LLM message for 8s

// --------------------------------------------------
// CLEAN FLOAT FUNCTION
// --------------------------------------------------

float cleanFloat(float v) {

  if (isnan(v) || isinf(v)) {
    return 0.0;
  }

  if (v > 100000 || v < -100000) {
    return 0.0;
  }

  return v;
}

// --------------------------------------------------
// BRIDGE-CALLABLE: RECEIVE CLASSIFICATION FROM PYTHON
// --------------------------------------------------

void showClassification(String label, float confidence) {
  currentLabel = label;
  currentConfidence = confidence;
}

void showAggregateState(String state) {
  aggregateState = state;
}

void showLLMMessage(String message) {
  llmMessage = message;
  llmMessageActive = true;
  llmMessageStart = millis();
}

// --------------------------------------------------
// SETUP
// --------------------------------------------------

void setup() {

  Monitor.begin(115200);

  Bridge.begin();

  Bridge.provide("show_classification", showClassification);
  Bridge.provide("show_aggregate_state", showAggregateState);
  Bridge.provide("show_llm_message", showLLMMessage);

  Wire.begin();

  pinMode(LO_PLUS, INPUT);
  pinMode(LO_MINUS, INPUT);

  // OLED
  u8g2.begin();
  u8g2.setFont(u8g2_font_5x8_tr);

  // MAX30102
  if (!particleSensor.begin()) {
    while (1);
  }

  // MPU6050 wake up
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  // Accel ±2g
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x00);
  Wire.endTransmission(true);

  // Gyro ±250°/s
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B);
  Wire.write(0x00);
  Wire.endTransmission(true);
}

// --------------------------------------------------
// MPU6050
// --------------------------------------------------

void readMPU() {

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);

  Wire.requestFrom(MPU_ADDR, 14, true);

  if (Wire.available() == 14) {

    accX  = Wire.read() << 8 | Wire.read();
    accY  = Wire.read() << 8 | Wire.read();
    accZ  = Wire.read() << 8 | Wire.read();

    Wire.read();
    Wire.read();

    gyroX = Wire.read() << 8 | Wire.read();
    gyroY = Wire.read() << 8 | Wire.read();
    gyroZ = Wire.read() << 8 | Wire.read();

    ax = accX / 16384.0;
    ay = accY / 16384.0;
    az = accZ / 16384.0;

    gx = gyroX / 131.0;
    gy = gyroY / 131.0;
    gz = gyroZ / 131.0;
  }
}

// --------------------------------------------------
// TMP117
// --------------------------------------------------

void readTMP117() {

  Wire.beginTransmission(TMP117_ADDR);
  Wire.write(0x00);

  if (Wire.endTransmission(false) != 0) {
    temperature = 0.0;
    return;
  }

  Wire.requestFrom(TMP117_ADDR, 2);

  if (Wire.available() == 2) {

    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();

    int16_t rawTemp = (msb << 8) | lsb;

    temperature = rawTemp * 0.0078125;
  }
  else {
    temperature = 0.0;
  }
}

// --------------------------------------------------
// OLED - MAIN STATE PAGE (only thing shown by default)
// --------------------------------------------------

void drawStatePage() {

  u8g2.setCursor(0, 10);
  u8g2.print("-- WEARABLE STATE --");

  u8g2.setFont(u8g2_font_7x14_tr);
  u8g2.setCursor(0, 30);
  u8g2.print(currentLabel);
  u8g2.setFont(u8g2_font_5x8_tr);

  u8g2.setCursor(0, 42);
  u8g2.print("Confidence: ");
  u8g2.print(currentConfidence * 100, 1);
  u8g2.print("%");

  u8g2.setCursor(0, 56);
  u8g2.print("5s avg: ");
  u8g2.print(aggregateState);
}

// --------------------------------------------------
// WORD-WRAPPED TEXT DRAWING (for LLM messages)
// --------------------------------------------------

void drawWrappedText(String text, int yStart, int lineHeight, int maxCharsPerLine, int maxLines) {
  int lineCount = 0;
  int start = 0;
  int len = text.length();

  while (start < len && lineCount < maxLines) {
    int end = start + maxCharsPerLine;

    if (end >= len) {
      end = len;
    } else {
      // try to break at last space before maxCharsPerLine
      int lastSpace = text.lastIndexOf(' ', end);
      if (lastSpace > start) {
        end = lastSpace;
      }
    }

    String line = text.substring(start, end);
    line.trim();

    u8g2.setCursor(0, yStart + (lineCount * lineHeight));
    u8g2.print(line);

    start = end + 1;
    lineCount++;
  }
}

// --------------------------------------------------
// OLED PAGE - LLM MESSAGE OVERLAY
// --------------------------------------------------

void drawLLMMessagePage() {
  u8g2.setCursor(0, 9);
  u8g2.print("-- ASSISTANT --");

  // 5x8 font: ~21 chars fit on 128px width
  drawWrappedText(llmMessage, 20, 10, 21, 4);
}

// --------------------------------------------------
// LOOP
// --------------------------------------------------

void loop() {

  // -------- SENSOR READINGS --------

  gsrValue = analogRead(gsrPin);

  leadsOff =
      (digitalRead(LO_PLUS) == HIGH ||
       digitalRead(LO_MINUS) == HIGH);

  ecgValue = leadsOff ? 0 : analogRead(ECG_PIN);

  readMPU();

  readTMP117();

  auto sample = particleSensor.readSample();

  long red = sample.red;
  long ir  = sample.ir;

  // -------- CLEAN SENSOR VALUES --------

  ax = cleanFloat(ax);
  ay = cleanFloat(ay);
  az = cleanFloat(az);

  gx = cleanFloat(gx);
  gy = cleanFloat(gy);
  gz = cleanFloat(gz);

  temperature = cleanFloat(temperature);

  // -------- BRIDGE LOGGING --------

  Bridge.call(
      "log_sample",
      (float)red,
      (float)ir,
      ax,
      ay,
      az,
      gx,
      gy,
      gz,
      (int)gsrValue,
      (int)ecgValue,
      (int)leadsOff,
      temperature
  );

  // --------------------------------------------------
  // EDGE IMPULSE SERIAL OUTPUT
  // --------------------------------------------------

  // --------------------------------------------------
  // SAFE INTEGER SERIAL OUTPUT FOR EDGE IMPULSE
  // --------------------------------------------------

  int ax_i = (int)(ax * 1000);
  int ay_i = (int)(ay * 1000);
  int az_i = (int)(az * 1000);

  int gx_i = (int)(gx * 1000);
  int gy_i = (int)(gy * 1000);
  int gz_i = (int)(gz * 1000);

  int temp_i = (int)(temperature * 1000);

  // overflow protection
  if (ax_i > 100000 || ax_i < -100000) ax_i = 0;
  if (ay_i > 100000 || ay_i < -100000) ay_i = 0;
  if (az_i > 100000 || az_i < -100000) az_i = 0;

  if (gx_i > 100000 || gx_i < -100000) gx_i = 0;
  if (gy_i > 100000 || gy_i < -100000) gy_i = 0;
  if (gz_i > 100000 || gz_i < -100000) gz_i = 0;

  if (temp_i > 100000 || temp_i < -100000) temp_i = 0;

  Monitor.print(red);
  Monitor.print(",");

  Monitor.print(ir);
  Monitor.print(",");

  Monitor.print(ax_i);
  Monitor.print(",");

  Monitor.print(ay_i);
  Monitor.print(",");

  Monitor.print(az_i);
  Monitor.print(",");

  Monitor.print(gx_i);
  Monitor.print(",");

  Monitor.print(gy_i);
  Monitor.print(",");

  Monitor.print(gz_i);
  Monitor.print(",");

  Monitor.print(gsrValue);
  Monitor.print(",");

  Monitor.print(ecgValue);
  Monitor.print(",");

  Monitor.print((int)leadsOff);
  Monitor.print(",");

  Monitor.println(temp_i);

  // -------- LLM MESSAGE TIMEOUT CHECK --------

  if (llmMessageActive && (millis() - llmMessageStart >= LLM_DISPLAY_DURATION)) {
    llmMessageActive = false;
  }

  // -------- OLED DRAW --------

  u8g2.clearBuffer();

  if (llmMessageActive) {
    // LLM message overrides the state page while active
    drawLLMMessagePage();
  } else {
    // Default: always show state page (no rotation, no other sensor pages)
    drawStatePage();
  }

  u8g2.sendBuffer();

  delay(50);
}
