#include "header.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

Servo feederServo;
FeederStatus feeder; // external - available in all cpp files

// timing constants
unsigned long lastReadingUpdate = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastDayCycleReset = 0;
const unsigned long interval1 = 1000; // 1 second
const unsigned long interval2 = 500;  // 0.5 second
const int PWM_CHANNEL = 0;
const int freq = 50;       // 50Hz for servo
const int resolution = 16; // 16-bit resolution

void setup()
{
  Serial.begin(115200);
  Serial.println("\n\n=== SMART CAT FEEDER STARTING ===\n");

  setupWiFi();
  setupMQTT();
  initOLED();                        // Initialize OLED display
  pinMode(SWITCH_PIN, INPUT_PULLUP); // Initialize slide switch
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Initialize button
  pinMode(POT_PIN, INPUT);           // Initialize potentiometer
  pinMode(LED_PIN, OUTPUT);          // initialize LED 
  digitalWrite(LED_PIN, LOW);        // Turn off initially
  ledcSetup(PWM_CHANNEL, freq, resolution);
  ledcAttachPin(SERVO_PIN, PWM_CHANNEL);     // Manually attach PWM for servo
  feederServo.attach(SERVO_PIN, 1000, 2000); // Initialize servo
  delay(500);                                // wait for power to stabilize
  feederServo.write(90);

  initHX711();

  feeder.Mode = getFeedingMode();
  feeder.portionSize = getPortionFromPot();
  Serial.println("mode: " + String(feeder.Mode == 1 ? "MANUAL" : "SCHEDULED"));
  Serial.println("portion: " + String(feeder.portionSize) + "g\n");

  displayWelcomeScreen();
  feederInit();

  Serial.println("=== INITIALIZATION COMPLETE ===\n");
  delay(1000);
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
}

void loop()
{
  mqttClient.loop();
  if (feeder.Mode == 1) {// maunal mode
    if (detectButtonPress()) {
      addFoodToBowl();
    }
  }
  else { // scheduled mode
    unsigned long currentMillis = millis();
    if (currentMillis - feeder.lastFeedTime >= feeder.feedInterval) { // check if time to feed
      feeder.lastFeedTime = currentMillis; // Update last feed time
      addFoodToBowl();
    }
  }
  // timing logic for display and reading updates
  unsigned long currentMillis = millis();

  if (currentMillis - lastDisplayUpdate >= interval1) {
    lastDisplayUpdate = currentMillis;
    // updateWeightLevels(); // uncomment if using real hardware
    checkTankLevelAndAlert();
    functionsUpdate();
  }
  // next day cycle update
  if (millis() - lastDayCycleReset >= DAY_CYCLE_MS) {
    resetFeederForNextDay();
    lastDayCycleReset = millis(); // Update last reset time
  }
  simulateEating();
}
