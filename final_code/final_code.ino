/*
  ESP32 All-in-One Attendance (Fingerprint + RFID) with Keypad/LCD/Buzzer
  - Default: Fingerprint attendance
  - 'C' toggles RFID <-> Fingerprint mode
  - 'A' enroll fingerprint ID (admin PIN -> enter ID -> enroll)
  - 'D' delete fingerprint ID (admin PIN -> enter ID -> delete)
  - 'B' link workflow: admin PIN -> enter Finger ID -> scan RFID -> POST link
  - Posts to Google Apps Script:
      Attendance: {"uid":"FP:<id>" | "<rfid_uid>","token":"12354"}
      Link: {"action":"link","finger_id":<num>,"rfid_uid":"<uid>","token":"12354"}

  Pins:
    Fingerprint UART2: RX=16 (to sensor TX), TX=17 (to sensor RX)
    Keypad rows: 13,12,14,27   cols: 32,33,26,25
    I2C LCD: SDA=21, SCL=22
    RC522: SS=5, RST=4, SCK=18, MISO=19, MOSI=23
    Buzzer: 15 (tone/noTone)
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_Fingerprint.h>

// ---------- WiFi / Web App ----------
const char* WIFI_SSID           = "GNXS-13D910";
const char* WIFI_PASSWORD       = "12345678";
const char* SCRIPT_WEB_APP_URL  = "https://script.google.com/macros/s/AKfycby8MyadXZtytMw2I6KJ65W6MPVuJ7TC9d5XiM5KUNKX8CLU7KrYU8VEZNP1NRPi36KAkQ/exec";
const char* TOKEN_SHARED_SECRET = "12354";

// ---------- Admin / ID constraints ----------
const char*  ADMIN_PIN   = "1234";
const uint8_t PIN_LENGTH = 4;
const uint8_t MAX_TRIES  = 3;
const uint16_t MIN_ID    = 1;
const uint16_t MAX_ID    = 127;  // typical Adafruit sensor capacity

// ---------- Pins ----------
const int FP_RX = 16;   // Sensor TX -> ESP32 RX2
const int FP_TX = 17;   // Sensor RX -> ESP32 TX2
const int I2C_SDA = 21;
const int I2C_SCL = 22;
const int BUZZER_PIN = 15;

// RC522 wiring (ESP32 HSPI)
#define RFID_SS   5
#define RFID_RST  4
// SCK=18, MISO=19, MOSI=23 set in SPI.begin(18,19,23,SS)

// ---------- Peripherals ----------
HardwareSerial FPSerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&FPSerial);
MFRC522 rfid(RFID_SS, RFID_RST);

#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// ---------- Keypad (rows 13,12,14,27 ; cols 32,33,26,25) ----------
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {32, 33, 26, 25};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ---------- Buzzer (tone/noTone) ----------
void buzzTone(int freq, int ms) { tone(BUZZER_PIN, freq); delay(ms); noTone(BUZZER_PIN); }
void beepOK()       { buzzTone(2200, 90); }
void beepError()    { buzzTone(700, 120); }
void beepDenied()   { buzzTone(600, 120); delay(70); buzzTone(500, 150); }
void beepDoubleOK() { buzzTone(1800, 80); delay(60); buzzTone(2000, 80); }

// ---------- UI helpers ----------
void lcdTwo(const String& a, const String& b="") {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print(a.substring(0, LCD_COLS));
  lcd.setCursor(0,1); lcd.print(b.substring(0, LCD_COLS));
}
void lcdTwoLines(const String& a, const String& b="") { lcdTwo(a, b); } // alias

// ---------- Modes & State ----------
enum AttMode { MODE_FINGER, MODE_RFID };
AttMode attMode = MODE_FINGER;

enum Flow {
  FLOW_IDLE,
  FLOW_WAIT_ADMIN,    // entering admin PIN
  FLOW_GET_ID,        // entering target ID for enroll/delete
  FLOW_ENROLLING,     // doEnroll(targetID)
  FLOW_DELETING,      // doDelete(targetID)
  FLOW_LINK_FP_ID,    // entering FP ID for link
  FLOW_LINK_WAIT_CARD // waiting for RFID scan
};
Flow flow = FLOW_IDLE;

// High-level operation selected before PIN
enum Operation { OP_NONE, OP_ENROLL, OP_DELETE, OP_LINK };
Operation op = OP_NONE;

bool pendingEnroll = false;
bool pendingDelete = false;

String   inputBuf;         // buffer for PIN / ID typing
uint16_t targetID   = 0;   // for enroll/delete
uint8_t  wrongTries = 0;

uint16_t linkFPID = 0;     // for link flow (B)

// ---------- Screens ----------
void showHome() {
  String top = (attMode == MODE_FINGER) ? "Finger Mode" : "RFID Mode";
  lcdTwo(top, "A=Add D=Del B=Link C=Toggle");
}
void showPINPrompt() {
  String mask(inputBuf.length(), '*');
  lcdTwo("Admin PIN:"+mask, "*=OK  #=Cancel");
}
void showIDPrompt(bool delMode) {
  lcdTwo(delMode ? "Del ID:"+inputBuf : "Enroll ID:"+inputBuf, "*=OK  #=Cancel");
}
void showLinkIDPrompt() {
  lcdTwo("Link FP ID:"+inputBuf, "*=OK  #=Cancel");
}

// ---------- Helpers ----------
String fpUID(int id) { return "FP:" + String(id); }

String rfidUID(const MFRC522::Uid &u) {
  String s;
  for (byte i=0;i<u.size;i++) {
    if (u.uidByte[i] < 0x10) s += "0";
    s += String(u.uidByte[i], HEX);
    if (i < u.size-1) s += ":";
  }
  s.toUpperCase();
  return s;
}

bool parseUint16(const String& s, uint16_t& out) {
  if (s.length()==0) return false;
  for (char c: s) if (!isDigit(c)) return false;
  long v = s.toInt();
  if (v < 1 || v > 65535) return false;
  out = (uint16_t)v; return true;
}
bool parseIDFromInput(uint16_t &id) {
  if (!parseUint16(inputBuf, id)) return false;
  if (id < MIN_ID || id > MAX_ID) return false;
  return true;
}

// ---------- HTTP ----------
bool postJSON(const String& json) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] WiFi not connected");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);

  HTTPClient http;
  if (!http.begin(client, SCRIPT_WEB_APP_URL)) {
    Serial.println("[HTTP] begin failed");
    return false;
  }

  // Important for Google Apps Script deployments that issue redirects
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  http.addHeader("Content-Type","application/json");

  Serial.println("[HTTP] Sending: " + json);
  
  int code = http.POST(json);
  String resp = http.getString();
  http.end();

  Serial.printf("[HTTP] Response code=%d\n", code);
  Serial.println("[HTTP] Response body: " + resp);

  // Success if:
  //  - 2xx (200–299) OR
  //  - 3xx (300–399) (some GAS setups redirect) OR
  //  - body contains `"ok":true`
  bool okCode = (code >= 200 && code < 400);
  bool okBody = (resp.indexOf("\"ok\":true") != -1) || (resp.indexOf("\"ok\": true") != -1);

  Serial.printf("[HTTP] okCode=%d, okBody=%d\n", okCode, okBody);

  return okCode || okBody;
}

bool postAttendanceUID(const String& uid) {
  String payload = String("{\"uid\":\"") + uid + "\",\"token\":\"" + TOKEN_SHARED_SECRET + "\"}";
  return postJSON(payload);
}
bool postLink(uint16_t finger_id, const String& rfid_uid) {
  String payload = String("{\"action\":\"link\",\"finger_id\":") + finger_id +
                   ",\"rfid_uid\":\"" + rfid_uid + "\",\"token\":\"" + TOKEN_SHARED_SECRET + "\"}";
  return postJSON(payload);
}

// ---------- Fingerprint ops ----------
uint8_t captureAndConvert(uint8_t slot) {
  lcdTwo("Place finger...");
  while (true) {
    uint8_t p = finger.getImage();
    if (p == FINGERPRINT_OK) break;
    else if (p == FINGERPRINT_NOFINGER) { delay(40); }
    else return p;
  }
  return finger.image2Tz(slot);
}
bool doEnroll(uint16_t id) {
  if (captureAndConvert(1) != FINGERPRINT_OK) { lcdTwo("Scan 1 failed"); beepError(); delay(900); return false; }
  lcdTwo("Remove finger");
  delay(600);
  while (finger.getImage() != FINGERPRINT_NOFINGER) delay(40);
  if (captureAndConvert(2) != FINGERPRINT_OK) { lcdTwo("Scan 2 failed"); beepError(); delay(900); return false; }
  if (finger.createModel() != FINGERPRINT_OK)  { lcdTwo("No match"); beepError(); delay(900); return false; }
  if (finger.storeModel(id) == FINGERPRINT_OK) { lcdTwo("Enroll OK","ID="+String(id)); beepDoubleOK(); delay(900); return true; }
  lcdTwo("Store failed"); beepError(); delay(900); return false;
}
bool doDelete(uint16_t id) {
  if (finger.deleteModel(id) == FINGERPRINT_OK) { lcdTwo("Deleted","ID="+String(id)); beepOK(); delay(900); return true; }
  lcdTwo("Delete failed"); beepError(); delay(900); return false;
}
int16_t fpMatchOnce() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -2;
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return -3;
  return finger.fingerID;
}

// ---------- RFID (one-shot) ----------
bool readOneCard(String& out) {
  if (!rfid.PICC_IsNewCardPresent()) return false;
  if (!rfid.PICC_ReadCardSerial())   return false;
  out = rfidUID(rfid.uid);
  rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
  return true;
}

// ---------- Keys ----------
void showIDPrompt(bool delMode); // fwd

void handleKey(char k) {
  if (!k) return;

  // During typing states
  if (flow == FLOW_WAIT_ADMIN || flow == FLOW_GET_ID || flow == FLOW_LINK_FP_ID) {
    if (k == '#') { // cancel and go home
      inputBuf = "";
      targetID = 0;
      linkFPID = 0;
      wrongTries = 0;
      flow = FLOW_IDLE;
      op = OP_NONE;
      lcdTwo("Cancelled","Returning home...");
      beepError();
      delay(600);
      showHome();
      return;
    }

    if (k == '*') { // confirm
      if (flow == FLOW_WAIT_ADMIN) {
        if (inputBuf.length() != PIN_LENGTH) {
          lcdTwo(inputBuf.length() < PIN_LENGTH ? "Incomplete Code" : "Invalid Code");
          beepError(); delay(900); showPINPrompt(); return;
        }
        if (inputBuf == ADMIN_PIN) {
          beepOK(); wrongTries = 0; inputBuf = "";
          if      (op == OP_ENROLL) { flow = FLOW_GET_ID;       showIDPrompt(false); }
          else if (op == OP_DELETE) { flow = FLOW_GET_ID;       showIDPrompt(true);  }
          else if (op == OP_LINK)   { flow = FLOW_LINK_FP_ID;   showLinkIDPrompt();  }
          else                      { flow = FLOW_IDLE;         showHome();          }
        } else {
          lcdTwo("Invalid Code","Try again");
          beepError(); wrongTries++; inputBuf = ""; showPINPrompt();
          if (wrongTries >= MAX_TRIES) {
            lcdTwo("Access Denied","");
            beepDenied(); delay(1000);
            wrongTries = 0; flow = FLOW_IDLE; op = OP_NONE; showHome();
          }
        }
        return;
      }

      if (flow == FLOW_GET_ID) {
        // Only for ENROLL/DELETE
        if (parseIDFromInput(targetID)) {
          beepOK();
          flow = (op == OP_ENROLL) ? FLOW_ENROLLING : FLOW_DELETING;
        } else {
          lcdTwo("Invalid ID", String(MIN_ID) + "-" + String(MAX_ID));
          beepError(); delay(900); inputBuf = ""; showIDPrompt(op == OP_DELETE);
        }
        return;
      }

      if (flow == FLOW_LINK_FP_ID) {
        // Only for LINK
        if (parseUint16(inputBuf, linkFPID) && linkFPID >= MIN_ID && linkFPID <= MAX_ID) {
          beepOK(); inputBuf = "";
          flow = FLOW_LINK_WAIT_CARD;
          lcdTwo("Scan RFID card","(10s)");
        } else {
          lcdTwo("Invalid FP ID", String(MIN_ID) + "-" + String(MAX_ID));
          beepError(); delay(900); inputBuf = ""; showLinkIDPrompt();
        }
        return;
      }
    }

    if (isDigit(k)) {
      if (flow == FLOW_WAIT_ADMIN && inputBuf.length() >= PIN_LENGTH) {
        lcdTwo("Invalid Code","Try again");
        beepError(); delay(900); inputBuf = ""; showPINPrompt();
        return;
      }
      inputBuf += k;
      if      (flow == FLOW_WAIT_ADMIN)      showPINPrompt();
      else if (flow == FLOW_GET_ID)          showIDPrompt(op == OP_DELETE);
      else if (flow == FLOW_LINK_FP_ID)      showLinkIDPrompt();
    }
    return;
  }

  // While waiting for card in link flow, allow # to cancel
  if (flow == FLOW_LINK_WAIT_CARD) {
    if (k == '#') {
      inputBuf = "";
      linkFPID = 0;
      flow = FLOW_IDLE;
      op = OP_NONE;
      lcdTwo("Link Cancelled","Returning home...");
      beepError();
      delay(600);
      showHome();
    }
    return;
  }

  // Idle hotkeys (work in BOTH modes)
  if (k == 'C') {
    // Toggle mode between Fingerprint and RFID
    attMode = (attMode == MODE_FINGER) ? MODE_RFID : MODE_FINGER;
    beepOK(); showHome(); return;
  }
  if (k == 'B') {
    // LINK: admin -> FP ID -> scan card
    pendingEnroll = false; pendingDelete = false;
    op = OP_LINK;
    inputBuf = ""; flow = FLOW_WAIT_ADMIN;
    lcdTwo("Link Mode","Enter Admin PIN"); delay(200); showPINPrompt();
    return;
  }
  if (k == 'A') {
    // ENROLL
    pendingEnroll = true; pendingDelete = false;
    op = OP_ENROLL;
    inputBuf = ""; flow = FLOW_WAIT_ADMIN;
    lcdTwo("Enroll Mode","Enter Admin PIN"); delay(200); showPINPrompt();
    return;
  }
  if (k == 'D') {
    // DELETE
    pendingEnroll = false; pendingDelete = true;
    op = OP_DELETE;
    inputBuf = ""; flow = FLOW_WAIT_ADMIN;
    lcdTwo("Delete Mode","Enter Admin PIN"); delay(200); showPINPrompt();
    return;
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);

  // LCD
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init(); lcd.backlight();
  lcdTwo("Booting...");

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (int i=0;i<40 && WiFi.status()!=WL_CONNECTED;i++) delay(250);
  if (WiFi.status()==WL_CONNECTED) { lcdTwo("WiFi OK", WiFi.localIP().toString()); beepOK(); }
  else { lcdTwo("WiFi Failed","Offline mode"); beepError(); }
  delay(800);

  // Fingerprint
  FPSerial.begin(57600, SERIAL_8N1, FP_RX, FP_TX);
  delay(120);
  finger.begin(57600);
  delay(150);
  if (finger.verifyPassword()) { lcdTwo("FP sensor ready"); beepOK(); }
  else { lcdTwo("FP sensor error"); beepError(); }
  delay(600);

  // RFID
  SPI.begin(18,19,23, RFID_SS);
  rfid.PCD_Init();

  showHome();
}

// ---------- Loop ----------
void loop() {
  handleKey(keypad.getKey());

  // Enroll/Delete actions
  if (flow == FLOW_ENROLLING) {
    bool ok = doEnroll(targetID);
    targetID = 0; inputBuf = ""; flow = FLOW_IDLE; op = OP_NONE; showHome(); if (!ok) beepError();
    return;
  }
  if (flow == FLOW_DELETING) {
    bool ok = doDelete(targetID);
    targetID = 0; inputBuf = ""; flow = FLOW_IDLE; op = OP_NONE; showHome(); if (!ok) beepError();
    return;
  }

  // Link: waiting for RFID card
  if (flow == FLOW_LINK_WAIT_CARD) {
    String uid; uint32_t t0=millis();
    while (millis()-t0 < 10000) {
      if (readOneCard(uid)) break;
      delay(10);
    }
    if (uid.length()==0) { 
      lcdTwo("No card","Link cancelled"); 
      beepError(); 
      delay(900); 
      flow = FLOW_IDLE; 
      op = OP_NONE; 
      showHome(); 
    } else {
      lcdTwo("Linking...","FP:"+String(linkFPID));
      bool ok = postLink(linkFPID, uid);
      if (ok) { 
        lcdTwo("Link Success","FP:"+String(linkFPID)+" -> "+uid.substring(0,8)); 
        beepDoubleOK(); 
      } else { 
        lcdTwo("Link Failed","Check WiFi/URL"); 
        beepError(); 
      }
      delay(1500); 
      flow = FLOW_IDLE; 
      op = OP_NONE; 
      showHome();
    }
    return;
  }

  // Attendance by mode (only when idle)
  if (flow != FLOW_IDLE) return;

  if (attMode == MODE_FINGER) {
    int16_t id = fpMatchOnce();
    if (id >= 0) {
      String uid = fpUID(id);
      lcdTwo("FP matched", "ID="+String(id));
      beepOK();
      postAttendanceUID(uid); // server handles IN/OUT & Users ensure
      delay(900);
      showHome();
    } else if (id == -3) {
      // Fingerprint scanned but not recognized
      lcdTwo("Invalid Finger", "Not registered");
      beepDenied();
      delay(1200);
      showHome();
    }
  } else { // MODE_RFID
    String uid;
    if (readOneCard(uid)) {
      lcdTwo("Card", uid);
      beepOK();
      postAttendanceUID(uid); // server handles IN/OUT & Users ensure
      delay(900);
      showHome();
    }
  }

  delay(10);
}