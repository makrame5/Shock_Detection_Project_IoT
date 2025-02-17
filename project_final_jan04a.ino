#include "arduino_secrets.h"
#include "arduino_secrets.h"
#include "thingProperties.h"
#include <Wire.h>

// MPU6050 configuration
#define MPU6050_ADDR      0x68
#define ACCEL_XOUT_H      0x3B
#define TEMP_OUT_H        0x41    // Temperature data register
#define MPU_PWR_MGMT_1    0x6B
#define MPU_ACCEL_CONFIG  0x1C

// Shock detection configuration
#define SHOCK_THRESHOLD   2.0    // Threshold in g (adjust based on your needs)
#define SHOCK_DURATION    500    // Minimum time between shock detections (ms)
#define MOVING_AVG_SIZE   8      // Size of moving average window for noise reduction

// Temperature configuration
#define TEMP_READ_INTERVAL 1000  // Temperature reading interval in ms

// Error handling
#define MAX_I2C_RETRIES   3

// Global variables
unsigned long lastShockTime = 0;
unsigned long lastTempReadTime = 0;
      
float accXHistory[MOVING_AVG_SIZE] = {0};
float accYHistory[MOVING_AVG_SIZE] = {0};
float accZHistory[MOVING_AVG_SIZE] = {0};
uint8_t historyIndex = 0;
  float accX;
  float accY;
  float accZ;
const int ledPin = 17;

bool initMPU6050() {
  Wire.begin();
  
  // Try multiple times to initialize the sensor
  for (int retry = 0; retry < MAX_I2C_RETRIES; retry++) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(MPU_PWR_MGMT_1);
    Wire.write(0); // Wake up MPU6050
    if (Wire.endTransmission(true) == 0) {
      // Set accelerometer range to ±2g
      Wire.beginTransmission(MPU6050_ADDR);
      Wire.write(MPU_ACCEL_CONFIG);
      Wire.write(0x00);
      Wire.endTransmission(true);
      
      Serial.println("MPU6050 initialized successfully!");
      return true;
    }
    delay(100);
  }
  
  Serial.println("Error: Failed to initialize MPU6050!");
  return false;
}

float readTemperature() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(TEMP_OUT_H);
  if (Wire.endTransmission(false) != 0) {
    Serial.println("Error: Failed to start temperature reading!");
    return -273.15; // Return an impossible temperature to indicate error
  }
  
  if (Wire.requestFrom(MPU6050_ADDR, 2, true) != 2) {
    Serial.println("Error: Failed to read complete temperature data!");
    return -273.15;
  }
  
  // Read temperature data
  int16_t rawTemp = (Wire.read() << 8) | Wire.read();
  
  // Convert raw temperature data to degrees Celsius
  // Formula from MPU6050 datasheet
  float tempC = (rawTemp / 340.0) + 36.53;
  
  return tempC;
}

void updateMovingAverage(float x, float y, float z) {
  accXHistory[historyIndex] = x;
  accYHistory[historyIndex] = y;
  accZHistory[historyIndex] = z;
  historyIndex = (historyIndex + 1) % MOVING_AVG_SIZE;
}

float getSmoothedValue(float* history) {
  float sum = 0;
  for (int i = 0; i < MOVING_AVG_SIZE; i++) {
    sum += history[i];
  }
  return sum / MOVING_AVG_SIZE;
}

float calculateMagnitude(float x, float y, float z) {
  return sqrt(x*x + y*y + z*z);
}

bool readAccelerometer(float& x, float& y, float& z) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(ACCEL_XOUT_H);
  if (Wire.endTransmission(false) != 0) {
    Serial.println("Error: Failed to start accelerometer reading!");
    return false;
  }
  
  if (Wire.requestFrom(MPU6050_ADDR, 6, true) != 6) {
    Serial.println("Error: Failed to read complete accelerometer data!");
    return false;
  }
  
  int16_t rawX = (Wire.read() << 8) | Wire.read();
  int16_t rawY = (Wire.read() << 8) | Wire.read();
  int16_t rawZ = (Wire.read() << 8) | Wire.read();
  
  // Convert to g (±2g range)
  x = rawX / 16384.0;
  y = rawY / 16384.0;
  z = rawZ / 16384.0;
  
  return true;
}

bool detectShock(float magnitude) {
  unsigned long currentTime = millis();
  
  if (magnitude > SHOCK_THRESHOLD && (currentTime - lastShockTime) > SHOCK_DURATION) {
    lastShockTime = currentTime;
    
    Serial.println("\n------------------------");
    Serial.println("SHOCK DETECTED!");
    Serial.printf("Magnitude: %.2f g\n", magnitude);
    Serial.printf("Temperature: %.2f °C\n", temperature);
    Serial.println("------------------------\n");
    
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  pinMode(ledPin, OUTPUT);
  if (!initMPU6050()) {
    while (1) {
      // Blink LED or provide other visual feedback
      delay(1000);
    }
  }
  
  // Initialize Arduino Cloud properties
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();
}

void loop() {
  ArduinoCloud.update();
  
  // Read temperature at specified intervals
  if (millis() - lastTempReadTime >= TEMP_READ_INTERVAL) {
    temperature = readTemperature();
    lastTempReadTime = millis();
  }
  
  float rawAccX, rawAccY, rawAccZ;
  if (!readAccelerometer(rawAccX, rawAccY, rawAccZ)) {
    delay(100);
    return;
  }
  
  // Update moving average
  updateMovingAverage(rawAccX, rawAccY, rawAccZ);
  
  // Get smoothed values
   accX = getSmoothedValue(accXHistory);
   accY = getSmoothedValue(accYHistory);
   accZ = getSmoothedValue(accZHistory);
  
  // Calculate total acceleration magnitude using smoothed values
  magnitude = calculateMagnitude(accX, accY, accZ);
  
  // Check for shock and update shockDetected variable
  shockDetected = detectShock(magnitude);
  if (magnitude >= 2.0){
   digitalWrite(ledPin, HIGH);
   lED = true;
  }else{
   digitalWrite(ledPin, LOW);
   lED = false;
  }
  // Print sensor data every 500ms
  static unsigned long lastPrintTime = 0;
  if (millis() - lastPrintTime > 500) {
    Serial.printf("AccX: %.2f g, AccY: %.2f g, AccZ: %.2f g, Magnitude: %.2f g, Temp: %.2f °C\n",
                 accX, accY, accZ, magnitude, temperature);
    lastPrintTime = millis();
  }
  
  delay(10); // Short delay for stability
}