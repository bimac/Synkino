#include <Arduino.h>
#include "buzzer.h"

Buzzer::Buzzer(uint8_t pin) {
  this->_pin = pin;
  pinMode(pin, OUTPUT);
}

void Buzzer::play(unsigned int frequency) {
  tone(_pin, frequency);
}

void Buzzer::play(unsigned int frequency, unsigned long duration) {
  tone(_pin, frequency, duration);
  delay(duration);
}

void Buzzer::playClick() {
  this->play(2200, 3);
}

void Buzzer::playConfirm() {
  this->play(NOTE_C7, 50);
  this->play(NOTE_G7, 50);
  this->play(NOTE_C8, 50);
}

void Buzzer::playHello() {
  this->play(NOTE_C7, 25);
  this->play(NOTE_D7, 25);
  this->play(NOTE_E7, 25);
  this->play(NOTE_F7, 25);
}

void Buzzer::playError() {
  this->play(NOTE_G2, 250);
  this->play(NOTE_C2, 500);
}

void Buzzer::quiet() {
  noTone(_pin);
}
