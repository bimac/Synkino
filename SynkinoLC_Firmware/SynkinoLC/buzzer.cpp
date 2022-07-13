#include <Arduino.h>
#include "buzzer.h"

Buzzer::Buzzer(uint8_t pin) {
  _pin = pin;
  pinMode(pin, OUTPUT);
}

void Buzzer::play(unsigned int frequency) {
  tone(_pin, frequency);
}

void Buzzer::play(unsigned int frequency, unsigned long duration) {
  tone(_pin, frequency, duration);
}

void Buzzer::playDelay(unsigned int frequency, unsigned long duration) {
  tone(_pin, frequency, duration);
  delay(duration);
}

void Buzzer::playClick() {
  play(2200, 3);
}

void Buzzer::playConfirm() {
  playDelay(NOTE_C7, 50);
  playDelay(NOTE_G7, 50);
  playDelay(NOTE_C8, 50);
}

void Buzzer::playHello() {
  playDelay(NOTE_C7, 25);
  playDelay(NOTE_D7, 25);
  playDelay(NOTE_E7, 25);
  playDelay(NOTE_F7, 25);
}

void Buzzer::playError() {
  playDelay(NOTE_G2, 250);
  playDelay(NOTE_C2, 500);
}

void Buzzer::quiet() {
  noTone(_pin);
}
