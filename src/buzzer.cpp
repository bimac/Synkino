#include <Arduino.h>
#include "buzzer.h"

Buzzer::Buzzer(uint8_t pin) : _pin { pin }  {
  pinMode(pin, OUTPUT);
  melodyTimer = new OneShotTimer(TCK);
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

void Buzzer::melody(uint8_t n, unsigned int *frequency, unsigned long *duration) {
  melody(n, 0, frequency, duration);
}

void Buzzer::melody(uint8_t n, uint8_t i, unsigned int *frequency, unsigned long *duration) {
  tone(_pin, frequency[i], duration[i]);
  i++;
  if (i < n) {
    melodyTimer->begin([=] { melody(n, i, frequency, duration); });
    melodyTimer->trigger(duration[i-1] * 1000);
  }
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
  static unsigned int freq[] = {NOTE_G2, NOTE_C2};
  static unsigned long dur[] = {250,     500};
  melody(2, freq, dur);
}

void Buzzer::quiet() {
  noTone(_pin);
}
