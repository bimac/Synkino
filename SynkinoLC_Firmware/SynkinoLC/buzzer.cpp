#include <Arduino.h>
#include "buzzer.h"

Buzzer::Buzzer(uint8_t pin) : _pin { pin }  {
  pinMode(pin, OUTPUT);
}

void Buzzer::play(unsigned int frequency) {
  tone(_pin, frequency);
}

void Buzzer::play(unsigned int frequency, unsigned long duration) {
  tone(_pin, frequency, duration);
}

void Buzzer::play(unsigned int frequency, unsigned long duration, bool useDelay) {
  tone(_pin, frequency, duration);
  if (useDelay)
    delay(duration);
}

void Buzzer::playClick() {
  play(2200, 3);
}

void Buzzer::playPress() {
  play(4000, 5);
}

void Buzzer::playConfirm() {
  play(NOTE_C7, 50, true);
  play(NOTE_G7, 50, true);
  play(NOTE_C8, 50);
}

void Buzzer::playHello() {
  play(NOTE_CS6, 85, true);
  play(NOTE_C7, 700, true);
}

void Buzzer::playError() {
  play(NOTE_G2, 250, true);
  play(NOTE_C2, 500, true);
}

void Buzzer::quiet() {
  noTone(_pin);
}
