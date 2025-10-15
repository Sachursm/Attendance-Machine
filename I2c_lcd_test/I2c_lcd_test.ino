#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Use your LCD address from scanner (usually 0x27 or 0x3F)
#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

void setup() {
  Wire.begin(21, 22);   // SDA = 21, SCL = 22
  lcd.init();            // Initialize the LCD
  lcd.backlight();       // Turn on the backlight

  // Clear the screen
  lcd.clear();

  // Print anything you want
  lcd.setCursor(0, 0);   // (column, row)
  lcd.print("ESP32 I2C LCD");

  lcd.setCursor(0, 1);
  lcd.print("Hello, World!");
}

void loop() {
  // Example: change message every 2 seconds
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Testing LCD...");
  lcd.setCursor(0, 1);
  lcd.print("Count: ");
  static int count = 0;
  lcd.print(count++);
}
