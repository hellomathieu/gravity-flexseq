#include <Arduino.h>
#include <libGravity.h>

void setup() {
    gravity.Init();
}

void loop() {
    gravity.Process();
}