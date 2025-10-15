/*
  ESP32 RFID Attendance Logger
  - MFRC522 RFID + I2C LCD + Buzzer + Google Sheets
  - Records UID, Date, In-Time, Out-Time
*/

#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>

// ---------- WiFi + Script ----------
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Your Google Apps Script Web App URL
const char* SCRIPT_URL = "YOUR_SCRIPT_URL_HERE"; // ends with /exec

// ---------- Pin Config ----------
#define SS_PIN      5
#define RST_PIN     4
#define BUZZER_PIN  2
#define SDA_PIN     21
#define SCL_PIN     22

// ---------- LCD ----------
#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// ---------- RFID ----------
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ---------- Buzzer ----------
const int BUZZER_CH = 0;

// ---------- Time (NTP) ----------
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800; // +5:30 for India
const int daylightOffset_sec = 0;

// ---------- UID tracking ----------
String lastUID = "";
bool isIn = true; // toggles IN/OUT

// ---------- Utility ----------
void beep(int freq = 2000, int ms = 120) {
  ledcWriteTone(BUZZER_CH, freq);
  delay(ms);
  ledcWriteTone(BUZZER_CH, 0);
}

String uidToHex(const MFRC522::Uid &uid) {
  String s;
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) s += "0";
    s += String(uid.uidByte[i], HEX);
    if (i < uid.size - 1) s += ":";
  }
  s.toUpperCase();
  return s;
}

void showLCD(const String &l1, const String &l2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(l1);
  lcd.setCursor(0, 1);
  lcd.print(l2);
}

String getFormattedDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "N/A";
  char buffer[12];
  strftime(buffer, sizeof(buffer), "%d-%m-%Y", &timeinfo);
  return String(buffer);
}

String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "N/A";
  char buffer[12];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  return String(buffer);
}

bool sendToSheets(const String &uid, const String &date, const String &inTime, const String &outTime) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (!http.begin(client, SCRIPT_URL)) return false;

  String data = "uid=" + uid + "&date=" + date + "&in=" + inTime + "&out=" + outTime;
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int code = http.POST(data);
  bool ok = (code == 200);
  http.end();
  return ok;
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);

  // LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  showLCD("RFID Attendance", "Starting...");

  // Buzzer
  ledcSetup(BUZZER_CH, 2000, 10);
  ledcAttachPin(BUZZER_PIN, BUZZER_CH);

  // Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  showLCD("WiFi Connecting", "");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  showLCD("WiFi Connected", "");
  delay(1000);

  // Time Sync
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  showLCD("Time Synced", "");
  delay(500);

  // RFID
  SPI.begin(18, 19, 23, SS_PIN);
  mfrc522.PCD_Init();
  showLCD("Scan your card", "");
  Serial.println("System ready");
}

// ---------- Loop ----------
void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  String uid = uidToHex(mfrc522.uid);
  String date = getFormattedDate();
  String timeNow = getFormattedTime();

  Serial.println("Card UID: " + uid);

  String statusText = "";
  String inTime = "";
  String outTime = "";

  // First scan = IN, next scan = OUT
  if (uid != lastUID || isIn) {
    statusText = "IN";
    inTime = timeNow;
    outTime = "";
    beep(2200, 120);
    showLCD("IN: " + uid, timeNow);
    isIn = false;
  } else {
    statusText = "OUT";
    inTime = "";
    outTime = timeNow;
    beep(1800, 120);
    delay(100);
    beep(1800, 120);
    showLCD("OUT: " + uid, timeNow);
    isIn = true;
  }

  lastUID = uid;

  // Send to Google Sheets
  if (WiFi.status() == WL_CONNECTED) {
    bool sent = sendToSheets(uid, date, inTime, outTime);
    if (sent) {
      Serial.println("Data sent successfully");
    } else {
      Serial.println("Failed to send data");
      showLCD("Send Failed", "Check WiFi");
    }
  }

  // Wait before next read
  delay(1500);
  showLCD("Scan your card", "");
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
