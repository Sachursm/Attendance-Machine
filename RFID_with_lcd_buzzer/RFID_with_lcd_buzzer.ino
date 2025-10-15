#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ===== Pin Definitions =====
#define SS_PIN    5     // SDA pin on RFID  
#define RST_PIN   4     // SCK - 18, MOSI - 23, MISO - 19
#define BUZZER_PIN 2
#define SDA_PIN   21    // I2C SDA
#define SCL_PIN   22    // I2C SCL

// ===== LCD Configuration =====
#define LCD_ADDR  0x27  // Change to 0x3F if your module uses that
#define LCD_COLS  16
#define LCD_ROWS  2
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// ===== RFID =====
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ===== Buzzer (using LEDC tone generator) =====
const int BUZZER_CH = 0;  // LEDC channel 0

// ===== Helper: Beep Function =====
void beep(uint16_t freq = 2000, uint16_t ms = 120) {
  ledcWriteTone(BUZZER_CH, freq);
  delay(ms);
  ledcWriteTone(BUZZER_CH, 0);
}

// ===== Convert UID to HEX string =====
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

// ===== LCD Helper =====
void showMessage(const char *line1, const char *line2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void setup() {
  Serial.begin(115200);

  // Initialize I2C LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  showMessage("RFID Reader", "Initializing...");

  // Initialize Buzzer
  ledcSetup(BUZZER_CH, 2000, 10); // Channel, Base Freq, Resolution
  ledcAttachPin(BUZZER_PIN, BUZZER_CH);

  // Initialize RFID (SPI)
  SPI.begin(18, 19, 23, 5); // SCK, MISO, MOSI, SS
  mfrc522.PCD_Init();
  delay(100);

  showMessage("RFID Ready", "Scan your card");
  Serial.println("Place your RFID card near the reader...");
}

void loop() {
  // Wait for a new card
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  // Get UID
  String uidHex = uidToHex(mfrc522.uid);
  Serial.print("Card UID: ");
  Serial.println(uidHex);

  // Display UID on LCD
  showMessage("Card Detected!", "");
  beep(2200, 150);

  lcd.setCursor(0, 1);
  if (uidHex.length() > LCD_COLS) {
    lcd.print(uidHex.substring(uidHex.length() - LCD_COLS));
  } else {
    lcd.print(uidHex);
  }

  // Halt communication with card
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  delay(1500);
  showMessage("RFID Ready", "Scan your card");
}
