/*
  ESP32 + Fingerprint + Keypad + I2C LCD + Buzzer
  - Default: scan & match
  - 'A' = Enroll (admin PIN=1234, exactly 4 digits)
  - 'D' = Delete (admin PIN=1234, exactly 4 digits)
  - '*' confirm | '#' backspace
  - PIN rules:
      * Only 4 digits allowed
      * If <4 and '*' -> "Incomplete Code"
      * If >4 at any time -> "Invalid Code" and reset
      * 3 wrong tries -> "Access Denied" and return home
  - Buzzer patterns:
      * success beep on correct actions/match
      * error beeps on wrong/invalid/denied
*/

#include <Keypad.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- Config ----------
const char*  ADMIN_PIN   = "1234";
const uint8_t PIN_LENGTH = 4;
const uint8_t MAX_TRIES  = 3;
const uint16_t MIN_ID    = 1;
const uint16_t MAX_ID    = 127;

// ---------- Pins ----------
const int FP_RX = 16; // sensor TX -> ESP32 RX2
const int FP_TX = 17; // sensor RX -> ESP32 TX2
const int BUZZER_PIN = 15;  // ✅ Changed from 4 → 15
const int I2C_SDA = 21;
const int I2C_SCL = 22;

// ---------- Fingerprint (UART2) ----------
HardwareSerial FPSerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&FPSerial);

// ---------- Keypad ----------
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','D'},
  {'7','8','9','C'},
  {'*','0','#','B'}
};
byte rowPins[ROWS] = {32, 33, 25, 26};
byte colPins[COLS] = {27, 14, 13, 5};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ---------- LCD ----------
#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// ---------- Buzzer (LEDC) ----------
const int BUZZER_CH = 0;
void buzzTone(int freq, int ms) {
  ledcWriteTone(BUZZER_CH, freq);
  delay(ms);
  ledcWriteTone(BUZZER_CH, 0);
}
void beepOK()           { buzzTone(2200, 90); }
void beepError()        { buzzTone(700, 120); }
void beepDenied()       { buzzTone(600, 120); delay(80); buzzTone(500, 160); }
void beepDoubleOK()     { buzzTone(1800, 90); delay(70); buzzTone(2000, 90); }

// ---------- State Machine ----------
enum Mode {
  MODE_SCAN,
  MODE_WAIT_ADMIN,
  MODE_GET_ID,
  MODE_ENROLLING,
  MODE_DELETING
};
Mode mode = MODE_SCAN;
bool pendingEnroll = false;
bool pendingDelete = false;

String   inputBuf;
uint16_t targetID   = 0;
uint8_t  wrongTries = 0;

// ---------- LCD Helpers ----------
void lcdTwoLines(const String &l1, const String &l2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(l1.substring(0, LCD_COLS));
  lcd.setCursor(0, 1); lcd.print(l2.substring(0, LCD_COLS));
}
void showHome() { lcdTwoLines("Scan Finger", "A-Add  D-Del"); }
void showPINPrompt() {
  String mask(inputBuf.length(), '*');
  lcdTwoLines("Admin PIN:" + mask, "*=OK  #=Back");
}
void showIDPrompt(bool delMode) {
  lcdTwoLines(delMode ? "Del ID:" + inputBuf : "Enroll ID:" + inputBuf, "*=OK  #=Back");
}

// ---------- FP Ops ----------
uint8_t captureAndConvert(uint8_t slot) {
  lcdTwoLines("Place finger...", " ");
  while (true) {
    uint8_t p = finger.getImage();
    if (p == FINGERPRINT_OK) break;
    else if (p == FINGERPRINT_NOFINGER) { delay(50); }
    else return p;
  }
  return finger.image2Tz(slot);
}

bool doEnroll(uint16_t id) {
  if (captureAndConvert(1) != FINGERPRINT_OK) { lcdTwoLines("Scan 1 failed"); beepError(); delay(900); return false; }
  lcdTwoLines("Remove finger", " ");
  delay(600);
  while (finger.getImage() != FINGERPRINT_NOFINGER) delay(50);
  if (captureAndConvert(2) != FINGERPRINT_OK) { lcdTwoLines("Scan 2 failed"); beepError(); delay(900); return false; }

  if (finger.createModel() != FINGERPRINT_OK) { lcdTwoLines("No match"); beepError(); delay(900); return false; }
  if (finger.storeModel(id) == FINGERPRINT_OK) { lcdTwoLines("Enroll Success", "ID=" + String(id)); beepDoubleOK(); delay(900); return true; }

  lcdTwoLines("Store failed"); beepError(); delay(900); return false;
}

bool doDelete(uint16_t id) {
  if (finger.deleteModel(id) == FINGERPRINT_OK) {
    lcdTwoLines("Deleted", "ID=" + String(id)); beepOK(); delay(900); return true;
  }
  lcdTwoLines("Delete failed"); beepError(); delay(900); return false;
}

int16_t doMatch() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -2;
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return -3;
  return finger.fingerID;
}

bool parseIDFromInput(uint16_t &id) {
  if (inputBuf.length() == 0) return false;
  for (char c : inputBuf) if (!isDigit(c)) return false;
  long v = inputBuf.toInt();
  if (v < MIN_ID || v > MAX_ID) return false;
  id = (uint16_t)v;
  return true;
}

// ---------- Keys ----------
void resetPINFlowInvalid() {
  lcdTwoLines("Invalid Code", "Try again");
  beepError();
  delay(1000);
  inputBuf = "";
  showPINPrompt();
}

void handleKey(char k) {
  if (!k) return;

  if (mode == MODE_WAIT_ADMIN || mode == MODE_GET_ID) {
    if (k == '#') {
      if (inputBuf.length()) inputBuf.remove(inputBuf.length() - 1);
      (mode == MODE_WAIT_ADMIN) ? showPINPrompt() : showIDPrompt(pendingDelete);
      return;
    }

    if (k == '*') {
      if (mode == MODE_WAIT_ADMIN) {
        if (inputBuf.length() < PIN_LENGTH) {
          lcdTwoLines("Incomplete Code", "Enter 4 digits");
          beepError();
          delay(900);
          showPINPrompt();
          return;
        }
        if (inputBuf.length() > PIN_LENGTH) {
          resetPINFlowInvalid();
          wrongTries++;
          if (wrongTries >= MAX_TRIES) {
            lcdTwoLines("Access Denied", "");
            beepDenied();
            delay(1200);
            wrongTries = 0; mode = MODE_SCAN; showHome();
          }
          return;
        }

        if (inputBuf == ADMIN_PIN) {
          beepOK();
          wrongTries = 0;
          inputBuf = "";
          mode = MODE_GET_ID;
          showIDPrompt(pendingDelete);
        } else {
          resetPINFlowInvalid();
          wrongTries++;
          if (wrongTries >= MAX_TRIES) {
            lcdTwoLines("Access Denied", "");
            beepDenied();
            delay(1200);
            wrongTries = 0; mode = MODE_SCAN; showHome();
          }
        }
      } else {
        if (parseIDFromInput(targetID)) {
          (pendingEnroll) ? (mode = MODE_ENROLLING) : (mode = MODE_DELETING);
          beepOK();
        } else {
          lcdTwoLines("Invalid ID", String(MIN_ID) + "-" + String(MAX_ID));
          beepError();
          delay(900);
          inputBuf = "";
          showIDPrompt(pendingDelete);
        }
      }
      return;
    }

    if (isDigit(k)) {
      if (mode == MODE_WAIT_ADMIN) {
        if (inputBuf.length() >= PIN_LENGTH) {
          resetPINFlowInvalid();
          return;
        }
      }
      inputBuf += k;
      (mode == MODE_WAIT_ADMIN) ? showPINPrompt() : showIDPrompt(pendingDelete);
    }
    return;
  }

  if (mode == MODE_SCAN) {
    if (k == 'A') {
      pendingEnroll = true; pendingDelete = false;
      inputBuf = ""; mode = MODE_WAIT_ADMIN;
      lcdTwoLines("Enroll Mode", "Enter Admin PIN");
      delay(450); showPINPrompt();
      return;
    }
    if (k == 'D') {
      pendingEnroll = false; pendingDelete = true;
      inputBuf = ""; mode = MODE_WAIT_ADMIN;
      lcdTwoLines("Delete Mode", "Enter Admin PIN");
      delay(450); showPINPrompt();
      return;
    }
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(150);

  // LCD
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init(); lcd.backlight();
  lcdTwoLines("Initializing...", "");

  // Buzzer
  ledcSetup(BUZZER_CH, 2000, 10);
  ledcAttachPin(BUZZER_PIN, BUZZER_CH);  // ✅ uses pin 15 now

  // Fingerprint
  FPSerial.begin(57600, SERIAL_8N1, FP_RX, FP_TX);
  delay(120);
  finger.begin(57600);
  delay(150);
  if (finger.verifyPassword()) { lcdTwoLines("Sensor Ready", ""); beepOK(); }
  else { lcdTwoLines("Sensor Error", "Check wiring"); beepError(); }

  delay(900);
  showHome();
}

// ---------- Loop ----------
void loop() {
  handleKey(keypad.getKey());

  switch (mode) {
    case MODE_SCAN: {
      int16_t id = doMatch();
      if (id >= 0) {
        lcdTwoLines("Matched ID=" + String(id), "A-Add  D-Del");
        beepOK();
        delay(900);
        showHome();
      }
    } break;

    case MODE_ENROLLING: {
      bool ok = doEnroll(targetID);
      pendingEnroll = false; targetID = 0; inputBuf = "";
      mode = MODE_SCAN;
      showHome();
      if (!ok) beepError();
    } break;

    case MODE_DELETING: {
      bool ok = doDelete(targetID);
      pendingDelete = false; targetID = 0; inputBuf = "";
      mode = MODE_SCAN;
      showHome();
      if (!ok) beepError();
    } break;

    default: break;
  }

  delay(15);
}

