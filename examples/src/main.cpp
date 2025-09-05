#include <Arduino.h>
#include <eb/DevicePortal.h>

using namespace earbrain;

DevicePortal portal;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  delay(1000);

  Serial.print("Hello: ");
  Serial.println(DevicePortal::hello());
  Serial.print("Version: ");
  Serial.println(DevicePortal::version());

  Serial.println("Setup complete!");
}

void loop() {
  static unsigned long lastPrint = 0;
  unsigned long now = millis();

  if (now - lastPrint >= 5000) {
    Serial.print(DevicePortal::hello());
    Serial.println(DevicePortal::version());
    lastPrint = now;
  }
}
