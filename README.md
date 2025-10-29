# 📘 RFID & Fingerprint Attendance System (ESP32 WROOM)

A smart **biometric attendance system** built with **ESP32 WROOM**, supporting both **RFID (RC522)** and **Fingerprint (R307)** authentication.  
Attendance is automatically logged to **Google Sheets** with **real-time syncing**.  
It includes a **4x4 Keypad**, **I2C LCD Display**, and **Buzzer** for a complete interactive experience.

---

## 🧠 Overview

This system allows employees or students to mark attendance using either:
- **Fingerprint Authentication (R307)**
- **RFID Card Authentication (RC522)**

Each record is directly uploaded to a **Google Sheet**, containing:
```
UID | FingerID | Name | Date | IN Time | OUT Time
```

It also includes an **“Attendance Report”** tab to filter data by **person and date range**, automatically calculating total working hours.

---

## 🎯 Features

✅ **Dual Authentication**
- Fingerprint (R307 sensor)
- RFID card (RC522 module)
- Switch modes easily with keypad button **C**

✅ **Google Sheets Integration**
- Real-time attendance sync
- Fully cloud-based and editable
- Uses `google_sheets.txt` Apps Script

✅ **Keypad Controls**
| Key | Function |
|------|-----------|
| **A** | Add new fingerprint (after admin code) |
| **B** | Bind RFID card to a fingerprint ID |
| **C** | Switch between Fingerprint and RFID mode |
| **D** | Delete fingerprint |
| **\** | Confirm |
| **#*** | Cancel |
| **Admin Code:** `1234` |

✅ **User Feedback**
- LCD for live status and prompts  
- Buzzer for confirmation sounds  

✅ **Auto Attendance Logic**
- 1st scan = **IN Time**
- 2nd scan = **OUT Time**

---

## 🔧 Components Used

| Component | Quantity | Description |
|------------|-----------|-------------|
| ESP32 WROOM | 1 | Main microcontroller |
| R307 Fingerprint Sensor | 1 | Captures fingerprint images |
| RC522 RFID Reader | 1 | Reads 13.56MHz RFID cards |
| I2C LCD Display (16x2 or 20x4) | 1 | Shows messages and status |
| 4x4 Matrix Keypad | 1 | Input for operations |
| Active Buzzer | 1 | Audio feedback |
| Jumper Wires | — | Male-Male / Male-Female |
| Power Supply (5V, 2A) | 1 | For stable system operation |

---

## ⚙️ Step-by-Step Connections

### 🧩 1. ESP32 and 4x4 Keypad

| Keypad Pin | ESP32 Pin | Description |
|-------------|------------|-------------|
| Row 1 | **GPIO 13** | Keypad Row 1 |
| Row 2 | **GPIO 12** | Keypad Row 2 |
| Row 3 | **GPIO 14** | Keypad Row 3 |
| Row 4 | **GPIO 27** | Keypad Row 4 |
| Col 1 | **GPIO 32** | Keypad Column 1 |
| Col 2 | **GPIO 33** | Keypad Column 2 |
| Col 3 | **GPIO 26** | Keypad Column 3 |
| Col 4 | **GPIO 25** | Keypad Column 4 |

📸 *Screenshot:* ![image alt](https://github.com/Sachursm/Attendance-Machine/blob/master/circuit_images/esp32_keypad.png?raw=true)

---

### 🖥️ 2. ESP32 and I2C LCD Display

| LCD Pin | ESP32 Pin | Description |
|----------|------------|-------------|
| VCC | **5V** | Power supply |
| GND | **GND** | Ground |
| SDA | **GPIO 21** | I2C data line |
| SCL | **GPIO 22** | I2C clock line |

📸 *Screenshot:* ![image alt](https://github.com/Sachursm/Attendance-Machine/blob/master/circuit_images/esp32_lcd.png?raw=true)

**Tip:**  
If LCD does not display, check I2C address (`0x27` or `0x3F`).

---

### 🔍 3. ESP32 and RC522 RFID Reader

| RC522 Pin | ESP32 Pin | Description |
|------------|------------|-------------|
| SDA (SS) | **GPIO 5** | SPI Slave Select |
| RST | **GPIO 4** | Reset pin |
| SCK | **GPIO 18** | SPI Clock |
| MISO | **GPIO 19** | SPI Master In Slave Out |
| MOSI | **GPIO 23** | SPI Master Out Slave In |
| VCC | **3.3V** | Power (⚠️ use 3.3V only) |
| GND | **GND** | Ground |

📸 *Screenshot:* ![image alt](https://github.com/Sachursm/Attendance-Machine/blob/master/circuit_images/esp32_rfid.png?raw=true)
**Note:** RC522 must always be connected to **3.3V**, not 5V.

---

### 🧠 4. ESP32 and R307 Fingerprint Sensor

| R307 Pin | ESP32 Pin | Description |
|-----------|------------|-------------|
| TX (Yellow) | **GPIO 16 (RX2)** | Data from Fingerprint to ESP32 |
| RX (White) | **GPIO 17 (TX2)** | Data from ESP32 to Fingerprint |
| VCC (Red) | **5V** | Power |
| GND (Black) | **GND** | Ground |

📸 *Screenshot:* ![image alt](https://github.com/Sachursm/Attendance-Machine/blob/master/circuit_images/esp32_R307S.png?raw=true)

---

### 🔔 5. ESP32 and Buzzer

| Buzzer Pin | ESP32 Pin | Description |
|-------------|------------|-------------|
| + (Positive) | **GPIO 15** | Control pin |
| - (Negative) | **GND** | Ground |

📸 *Screenshot:* ![image alt](https://github.com/Sachursm/Attendance-Machine/blob/master/circuit_images/esp32_buzzer.png?raw=true)

---

## ⚡ Power Connections Summary

| Power Source | Connected To |
|---------------|---------------|
| **5V** | LCD VCC, Fingerprint VCC |
| **3.3V** | RC522 VCC |
| **GND** | All components (common ground) |

**Recommendation:**  
Use a **5V 2A power supply** to ensure reliable operation.

---

## 🧰 Software Setup

### 1️⃣ Install ESP32 Board in Arduino IDE

**File → Preferences → Additional Boards URL:**
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```


Then go to:
**Tools → Board → Boards Manager → Search “ESP32” → Install.**

---

### 2️⃣ Install Required Libraries

| Library | Author |
|----------|---------|
| Adafruit Fingerprint Sensor Library | Adafruit |
| MFRC522 | GithubCommunity |
| LiquidCrystal_I2C | Frank de Brabander |
| Keypad | Mark Stanley & Alexander Brevig |
| WiFi, HTTPClient | Built-in with ESP32 |

---

### 3️⃣ Setup Google Sheets

#### Step 1: Create Spreadsheet
Create a new spreadsheet named **`rfidfn_attendance`**

Add three sheets:
- `Attendance`
- `Users`
- `Attendance Report`

**Attendance Sheet Columns:**
```
UID | FingerID | Name | Date | IN Time | OUT Time
```


**Users Sheet Columns:**
```
UID | Name | FingerID
```


---

#### Step 2: Google Apps Script

1. Go to **Extensions → Apps Script**
2. Delete existing code
3. Paste the code from your file: **`google_sheets.txt`**
4. Click **Deploy → New Deployment**
5. Choose **Web app**
6. Set:
   - Execute as: **Me**
   - Who has access: **Anyone**
7. Click **Deploy** and copy the **Web App URL**

---

#### Step 3: Configure ESP32 Code

Update these in your `.ino` sketch:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
String GOOGLE_SCRIPT_ID = "YOUR_WEB_APP_URL";
```
Then upload the code to ESP32 using Arduino IDE.
Monitor output via Serial Monitor (115200 baud).

🧾 Attendance Report Setup (Google Sheets)
Step 1: Sheet Layout
Row	Content
1	Merge A1:G1 → “Attendance Report”
3	B3 → Dropdown for person name
5	B5 → Start Date, E5 → End Date
7	Headers → UID, FingerID, Name, Date, IN Time, OUT Time, Total
Step 2: Dropdown Setup
```
Select cell B3

Go to Data → Data validation

Range: Attendance!C2:C

Enable “Show dropdown list in cell”
```
Step 3: Date Pickers
```
Format B5 and E5 as Date (Format → Number → Date)
```
Step 4: Filter Formula
```
In A8, paste:
```
```excel
=FILTER(Attendance!A2:F, (Attendance!C2:C = B3) * (Attendance!D2:D >= B5) * (Attendance!D2:D <= E5))
```
Step 5: Total Duration

In G8, paste:
```excel
=IF(A8<>"", F8-E8, "")
```
Format G column → Duration

✅ Done — Now your attendance report auto-updates by name and date range.

```bash
attendance_machine/
│
├── circuit_images/             # All wiring screenshots
│   ├── keypad_connection.png
│   ├── lcd_connection.png
│   ├── rfid_connection.png
│   ├── fingerprint_connection.png
│   ├── buzzer_connection.png
│   ├── power_distribution.png
│   └── attendance_report_example.png
│
├── final_code/                 # Final integrated ESP32 project
├── libraries/                  # Custom libraries
├── single code/                # Submodules (e.g., RFID only, LCD only)
│   ├── buzzer_with_LEDC/
│   ├── keypad_with_i2c_lcd/
│   ├── fingerprint_sensor_send_googlesheet/
│   ├── rfid_send_uid_googlesheet/
│   └── etc.
├── google_sheets.txt           # Google Apps Script
├── .gitignore
└── README.md                   # This documentation
```
🔊 Buzzer Feedback
Event	Sound	Description
```
Success	Single short beep	Action completed
Error	3 short beeps	Operation failed
Mode Change	2 short beeps	Mode switched
Invalid Input	Long beep	Unauthorized or wrong input
```

🔐 Security
Aspect	Recommendation
```
Admin Code	Default: 1234 → Change before deployment
WiFi Credentials	Store securely
Google Sheet	“Anyone” access for API only
Physical Protection	Prevent direct ESP32 access
```

📊 Example Google Sheet Data

Attendance Sheet
```
UID	FingerID	Name	Date	IN Time	OUT Time
9F:05:AE:1F	FP-1	sachu	2025-10-20	00:44:21	17:38:37
9F:05:AE:1F	FP-1	sachu	2025-10-29	16:39:34	17:12:52
82:AF:7D:00	FP-2	sam	2025-10-29	16:44:35	16:59:16

```
---
🎓 Educational Objective

This project teaches:

IoT and cloud data logging

Dual biometric authentication

ESP32 + Google Sheets integration

Hardware interfacing with real-time systems

Perfect for:

Schools and colleges

Mini projects

Makers and IoT enthusiasts
---
🧑‍💻 Author

Name: Sachu Retna SM


License: Open Source (Educational Use)
