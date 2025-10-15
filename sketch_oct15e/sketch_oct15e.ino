/*
When we say “LEDC tone generator”, we’re simply using one LEDC channel to produce a square wave at a given frequency.
A buzzer (especially a passive buzzer) makes a sound when it gets a fast on/off voltage wave — exactly what PWM does.
Instead of using Arduino’s simple tone() function (which doesn’t exist on ESP32),
we use ledcWriteTone(channel, frequency) to create the same effect — but better and non-blocking.

ledcSetup(channel, base_frequency, resolution_bits);
channel → choose 0–15

base_frequency → the default or max frequency (e.g. 2000 Hz)

resolution_bits → precision (e.g. 8–10 bits)

Attach a pin to that channel
ledcAttachPin(pin_number, channel);

Play a tone
ledcWriteTone(channel, frequency);

Stop the tone
ledcWriteTone(channel, 0); // 0 frequency = stop
*/
#define BUZZER_PIN 2
#define BUZZER_CH 0

void setup() {
  ledcSetup(BUZZER_CH, 2000, 10);
  ledcAttachPin(BUZZER_PIN, BUZZER_CH);
}

void loop() {
  ledcWriteTone(BUZZER_CH, 1000); // Play 1kHz
  delay(500);
  ledcWriteTone(BUZZER_CH, 2000); // Play 2kHz
  delay(500);
  ledcWriteTone(BUZZER_CH, 0);    // Stop
  delay(1000);
}

