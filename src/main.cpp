#include <Arduino.h>
#include <libGravity.h>

#include <flexseq/PatternStore.h>

flexseq::PatternStore patternStore;

void setup() {
    gravity.Init();
}

void loop() {
    gravity.Process();
}