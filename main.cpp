// there is no better infrastructure for this yet
#define ARDUINO_teensy

#include "yaal/arduino.hh"
#include "yaal/devices/rgbled.hh"
#include "yaal/communication/spi.hh"
#include <util/delay.h>

using namespace yaal;
using namespace yaal::arduino;

#define LEDS 16

BasicRGBLed<D1, D2, D0> rgb;

typedef D21 Latch;
typedef D18 Data;
typedef D15 Clock;

SPI<Clock, Data, NullPin, Latch> spi;

inline
void write(uint8_t* data, uint8_t bits) {
    LowPeriod<Latch> latch_low_for_this_block;
    internal::shiftBitsIf<Clock, Data>(bits, [data](uint8_t i){
        uint8_t value = data[i];
        if (value)
            data[i] = value - 1;
        return value;
    });
}

inline
void copy(uint8_t* to, const uint8_t* from, uint8_t bytes) {
    for (uint8_t i = 0; i < bytes; i++) {
        *to++ = *from++;
    }
}

inline
void advance(uint8_t* data, uint8_t end) {
    //uint8_t value = data[0] >> 1, prev;
    uint8_t value, prev;
    {
        uint8_t n = data[0];
        value = (n > 2) ? (n - (n >> 2) - 2) : 0;
    }
    for (uint8_t i = 0; i <= end; i++) {
        prev = data[i];
        data[i] = value;
        value = prev;
    }
    if (end == LEDS-1)
        data[LEDS-1] += prev >> 1;
}


void setup() {
    rgb.setup();
    spi.setup();
}

#define MAX 0x1f

void loop() {
    uint8_t state[LEDS] = {MAX};

    for (uint8_t r = 0; r < LEDS+10; r++) {
        uint8_t end = r < LEDS ? r : LEDS-1;
        uint8_t e = 60 + (LEDS - LEDS/4) - ((end > (LEDS/2) ? LEDS - end : end) >> (LEDS>16?2:1));
        if (r)
            advance(state, end);

        uint8_t round[LEDS];
        for (uint8_t j = 0; j < e; j++) {
            copy(round, state, LEDS);
            for (uint8_t i = 0; i < MAX; i++) {
                write(round, LEDS);
            }
        }
    }
}
