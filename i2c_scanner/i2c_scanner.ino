#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(D2, D1);
  Serial.println("\nI2C Scanner");
}

void loop() {
  byte count = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found device at 0x");
      Serial.println(addr, HEX);
      count++;
    }
  }
  if (count == 0)
    Serial.println("No I2C devices found");
  else
    Serial.print("Found ");
  Serial.print(count);
  Serial.println(" device(s)");
  delay(3000);
}
