#include <Arduino.h>
#include <libGravity.h>

#include <flexseq/PatternBank.h>

flexseq::PatternBank patternBank;

void setup() {
    gravity.Init();
}

void loop() {
    gravity.Process();
}
