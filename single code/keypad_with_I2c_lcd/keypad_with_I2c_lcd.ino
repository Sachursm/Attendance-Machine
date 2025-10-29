#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// LCD setup
#define LCD_ADDR 0x27  // Change to 0x3F if your module uses that
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// Keypad setup (4x4)
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// ESP32 pins connected to keypad rows and columns
byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {32, 33, 26, 25};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  Wire.begin(21, 22);   // SDA=21, SCL=22 for ESP32
  lcd.init();           // Initialize LCD
  lcd.backlight();      // Turn on backlight

  lcd.setCursor(0, 0);
  lcd.print("Keypad + LCD");
  lcd.setCursor(0, 1);
  lcd.print("Press a key...");
}

void loop() {
  char key = keypad.getKey();  // Read key press

  if (key) {  // If a key is pressed
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Key Pressed:");
    lcd.setCursor(0, 1);
    lcd.print(key);
  }
}
