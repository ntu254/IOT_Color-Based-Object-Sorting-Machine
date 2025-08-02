#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// Pin definitions
#define BUZZER 6       // Buzzer pin
#define SERVO_PIN 9    // First servo pin (color indicator)
#define BLOCKER_SERVO_PIN 5 // Second servo pin (blocker)
#define BT_TX 2        // Arduino TX -> HC-05 RX
#define BT_RX 3        // Arduino RX -> HC-05 TX (via voltage divider)

// Initialize components
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_101MS, TCS34725_GAIN_60X);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo colorServo;      // First servo for color indication
Servo blockerServo;    // Second servo for blocking mechanism
SoftwareSerial BT(BT_RX, BT_TX);

// Variables
String lastColor = "";
const int R_OFFSET = 20;
const int G_OFFSET = 8;
const int B_OFFSET = 0;
bool btConnected = false;
unsigned long lastBtAttempt = 0;
const unsigned long BT_RETRY_INTERVAL = 5000; // 5 seconds

void setup() {
  Serial.begin(9600);
  BT.begin(38400);  // HC-05 default baud rate
  
  // Initialize color sensor
  if (!tcs.begin()) {
    Serial.println(" Color sensor error!");
    while(1);
  }
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.print("Initializing...");
  
  // Initialize outputs
  pinMode(BUZZER, OUTPUT);
  colorServo.attach(SERVO_PIN);
  blockerServo.attach(BLOCKER_SERVO_PIN);
  colorServo.write(0);
  blockerServo.write(0); // Start with blocker open
  
  // Initial Bluetooth connection check
  checkBluetoothConnection();
}

void loop() {
  // Read color values
  float red, green, blue;
  tcs.getRGB(&red, &green, &blue);
  
  // Normalize and calibrate
  int R = constrain(int(red) - R_OFFSET, 0, 255);
  int G = constrain(int(green) - G_OFFSET, 0, 255);
  int B = constrain(int(blue) - B_OFFSET, 0, 255);

  // Noise filtering
  R = (R < 10) ? 0 : R;
  G = (G < 10) ? 0 : G;
  B = (B < 10) ? 0 : B;

  // Identify color
  String color = identifyColor(R, G, B);

  // Update only when color changes
  if (color != lastColor) {
    updateDisplay(R, G, B, color);
    controlOutputs(color);
    if (btConnected) {
      sendBluetoothData(R, G, B, color);
    }
    lastColor = color;
  }
  
  // Periodically check Bluetooth connection
  if (millis() - lastBtAttempt > BT_RETRY_INTERVAL) {
    checkBluetoothConnection();
    lastBtAttempt = millis();
  }
  
  delay(100);
}

String identifyColor(int R, int G, int B) {
  if ((R - G > 40) && (R - B > 40)) return "Red";
  if ((G - B > 15) && (G - R > 15)) return "Green";
  if ((B - G > 3) && (B - R > 2)) return "Blue";
  return "Unknown";
}

void updateDisplay(int R, int G, int B, String color) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Color: " + color);
  lcd.setCursor(0, 1);
  lcd.print("R:" + String(R) + " G:" + String(G) + " B:" + String(B));
  if (!btConnected) {
    lcd.print(" BT:Off");
  }
  
  Serial.print("R:"); Serial.print(R);
  Serial.print(" G:"); Serial.print(G);
  Serial.print(" B:"); Serial.print(B);
  Serial.print(" Color:"); Serial.println(color);
}

void controlOutputs(String color) {
  if (color == "Red") {
    colorServo.write(0);
    blockerServo.write(0); // Close blocker
    delay(300);
    beepBuzzer(1000, 300);
  } 
  else if (color == "Green") {
    colorServo.write(60);
    blockerServo.write(0); // Close blocker
    delay(300);
    beepBuzzer(1500, 120);
  } 
  else if (color == "Blue") {
    colorServo.write(180);
    blockerServo.write(0); // Close blocker
    delay(300);
    beepBuzzer(2000, 300);
  } 
  else {
    colorServo.write(0);
    blockerServo.write(180); // Open blocker for unknown colors
    noTone(BUZZER);
    delay(100);
  }
}

void beepBuzzer(int freq, int duration) {
  for (int i = 0; i < 3; i++) {
    tone(BUZZER, freq, duration/3);
    delay(duration/3 + 50);
  }
}

void sendBluetoothData(int R, int G, int B, String color) {
  String data = color + "|" + String(R) + "," + String(G) + "," + String(B);
  BT.println(data);
}

void checkBluetoothConnection() {
  BT.println("AT");
  delay(100);
  
  if (BT.available()) {
    String response = BT.readString();
    if (response.indexOf("OK") != -1) {
      if (!btConnected) {
        btConnected = true;
        lcd.setCursor(10, 1);
        lcd.print("BT:On ");
        Serial.println("Bluetooth connected!");
        beepBuzzer(2000, 200);
      }
    } else {
      btConnected = false;
    }
  } else {
    btConnected = false;
  }
}