#include <Wire.h>
#include <MPU6050.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

MPU6050 mpu;
LiquidCrystal_I2C lcd(0x27, 16, 2); // Alamat I2C LCD mungkin berbeda
TinyGPSPlus gps;
SoftwareSerial ss(8, 9); // RX ke pin 9, TX ke pin 8 (GPS)
SoftwareSerial sim800(2, 3); // RX ke pin 7, TX ke pin 6 (SIM800L)

const float accelerationThreshold = 3.0; // Threshold untuk akselerasi (dalam g)
const float tiltThreshold = 45.0;        // Threshold untuk sudut kemiringan (dalam derajat)
const int buzzerPin = 13;
const int buttonPin = 4; // Pin untuk push button+
const unsigned long accidentDelay = 5000; // 5 detik
const unsigned long firstSmsDelay = 10000; // 10 detik untuk SMS pertama
const unsigned long smsInterval = 60000; // 1 menit untuk SMS berikutnya

unsigned long accidentStartTime = 0;
bool accidentDetected = false;
bool smsSent = false;
unsigned long lastSmsTime = 0; // To track when the last SMS was sent

// Declare tilt as a global variable
float tilt = 0;

void setup() {
  Serial.begin(9600); // For Serial Monitor
  ss.begin(9600); // Kecepatan baud untuk GPS
  //sim800.begin(9600); // Kecepatan baud untuk SIM800L
  Wire.begin();
  mpu.initialize();
  lcd.init();
  lcd.backlight();
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP); // Set button pin as input with pull-up resistor
  
  if (mpu.testConnection()) {
    Serial.println("MPU6050 connection successful");
    lcd.setCursor(0, 0);
    lcd.print("MPU6050 Connected");
  } else {
    Serial.println("MPU6050 connection failed");
    lcd.setCursor(0, 0);
    lcd.print("MPU6050 Fail");
    while (1);
  }

  Serial.println("Setup complete, waiting for GPS data...");
  initSIM800(); // Initialize SIM800L
}

void loop() {
  // Check if the button is pressed
  if (digitalRead(buttonPin) == LOW) { // Button pressed (active low)
    accidentDetected = false; // Reset accident detection
    digitalWrite(buzzerPin, LOW); // Turn off buzzer
    Serial.println("Accident detection canceled by user.");
  } else {
    // Membaca data akselerasi dan giroskop
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    
    // Mengonversi nilai akselerasi mentah menjadi g
    float accelX = ax / 16384.0;
    float accelY = ay / 16384.0;
    float accelZ = az / 16384.0;
    
    // Menghitung sudut kemiringan
    tilt = atan2(accelY, sqrt(accelX * accelX + accelZ * accelZ)) * 180 / PI; // Update global tilt
    
    // Menampilkan nilai pada LCD dan Serial Monitor
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("AccX: "); lcd.print(accelX, 1);
    lcd.setCursor(0, 1);
    lcd.print("Tilt: "); lcd.print(tilt, 1);

    Serial.println("=== Sensor MPU6050 ===");
    Serial.print("AccX: "); Serial.print(accelX, 2);
    Serial.print(" g\tAccY: "); Serial.print(accelY, 2);
    Serial.print(" g\tAccZ: "); Serial.print(accelZ, 2);
    Serial.print(" g\nTilt: "); Serial.print(tilt, 2);
    Serial.println(" deg");
    
    // Memeriksa apakah akselerasi atau kemiringan melebihi threshold
    if (abs(accelX) > accelerationThreshold || abs(accelY) > accelerationThreshold || abs(accelZ) > accelerationThreshold || abs(tilt) > tiltThreshold) {
      if (!accidentDetected) {
        accidentDetected = true;
        accidentStartTime = millis();
        smsSent = false; // Reset SMS sent status
        lastSmsTime = 0; // Reset last SMS time
      }
      
      // Check if it's time to send the first SMS
      if (accidentDetected && !smsSent && (millis() - accidentStartTime >= firstSmsDelay)) {
        sendSMS();
        smsSent = true; // Mark SMS as sent
        lastSmsTime = millis(); // Update last SMS time
      }
      
      // Send SMS every minute after the first SMS
      if (smsSent && (millis() - lastSmsTime >= smsInterval)) {
        sendSMS();
        lastSmsTime = millis(); // Update last SMS time
      }

      // Activate buzzer after 5 seconds of accident detection
      if (millis() - accidentStartTime >= accidentDelay) {
        Serial.println("Accident detected due to high acceleration or tilt angle!");
        lcd.setCursor(0, 1);
        lcd.print("Accident!");
        digitalWrite(buzzerPin, HIGH); // Turn on buzzer
      }
    } else {
      accidentDetected = false;
      digitalWrite(buzzerPin, LOW); // Turn off buzzer if stable
      Serial.println("Motorcycle is stable.");
    }
  }

  // Membaca data dari GPS
  readGPS();

  delay(100); // Short delay to allow other processes
}

// Function to initialize SIM800L
void initSIM800() {
  sim800.println("AT"); // Check communication
  delay(100);
  sim800.println("AT+CMGF=1"); // Set SMS mode
  delay(100);
  Serial.println("SIM800L initialized.");
}

// Function to send SMS
void sendSMS() {
  String latitude = String(gps.location.lat(), 6);
  String longitude = String(gps.location.lng(), 6);
  String message = "Peringatan: Terjadi kecelakaan! Lokasi: http://www.google.com/maps/place/" + latitude + "," + longitude;
  
  sim800.print("AT+CMGS=\"+6285257166589\"\r"); // Replace with actual hospital number
  delay(100);
  sim800.print(message);
  delay(100);
  sim800.write(26); // Send Ctrl+Z to indicate end of SMS
  Serial.println("SMS sent: " + message);
}

// Function to read GPS data
void readGPS() {
  // Read GPS data from SoftwareSerial
  while (ss.available() > 0) {
    char c = ss.read();
    gps.encode(c);
  }
  
  // Menampilkan data GPS di Serial Monitor jika ada update
  if (gps.location.isValid()) {
    Serial.println("\n=== Data GPS ===");
    Serial.print("Latitude: "); Serial.println(gps.location.lat(), 6);
    Serial.print("Longitude: "); Serial.println(gps.location.lng(), 6);
    Serial.print("Speed: "); Serial.println(gps.speed.kmph());
    Serial.println("=================\n");
  } else {
    Serial.println("Waiting for GPS fix...");
  }

  // Decode latitude from GPRMC sentence
  if (gps.location.isUpdated()) {
    float latitude = gps.location.lat(); // Latitude in decimal degrees
    Serial.print("Decoded Latitude: "); Serial.println(latitude, 6);
    Serial.print("Tilt: "); Serial.println(tilt, 2); // Now tilt is accessible here
  }
}