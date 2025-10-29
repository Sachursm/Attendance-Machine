#include <Keypad.h>

// ---- Key layout for 4x4 keypad
const byte ROWS = 4; // four rows
const byte COLS = 4; // four columns

// Update to match your keypad legends if different
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// ---- ESP32 GPIOs (rows then columns)
byte rowPins[ROWS] = {13, 12, 14, 27};   // R1, R2, R3, R4
byte colPins[COLS] = {32, 33, 26, 25};    // C1, C2, C3, C4

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Optional: basic debounce & hold timing (tweak if needed)
const unsigned long DEBOUNCE_MS = 25;
const unsigned long HOLD_MS     = 500;

void setup() {
  Serial.begin(115200);
  keypad.setDebounceTime(DEBOUNCE_MS);
  keypad.setHoldTime(HOLD_MS);
  Serial.println("ESP32 Keypad test ready. Press keys...");
}

void loop() {
  // Non-blocking: returns only when something changes
  char key = keypad.getKey();

  if (key) {
    Serial.print("Key pressed: ");
    Serial.println(key);
  }

  // If you want full event types (PRESSED, RELEASED, HOLD), use the listener version below
}
