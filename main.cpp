// there is no better infrastructure for this yet
#define ARDUINO_teensy

#include "yaal/arduino.hh"
#include "yaal/devices/rgbled.hh"
#include "yaal/communication/spi.hh"
#include <util/delay.h>

#include "triple.hh"
#include "circular_buffer.hh"

using namespace yaal;
using namespace yaal::arduino;

#define LEDS 16
#define BLOCKS (LEDS/8)
#define MAX 0x1f

BasicRGBLed<D1, D2, D0> rgb;

typedef D21 Latch;
typedef D18 Data;
typedef D15 Clock;

SPI<Clock, Data, NullPin, Latch> spi;

typedef Triple<uint8_t> drop_t;
typedef CircularBuffer<4, drop_t> drop_list_t;

uint8_t random() {
    static uint8_t r;
    uint8_t b1 = r & 0x80;
    uint8_t r2 = r << 1;
    uint8_t b2 = r << 2 & 0x80;
    uint8_t bit = !(b1 ^ b2);
    r = r2 | bit;
    return r;
}

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

inline
void update_state(drop_list_t& drops, uint8_t* state) {
    uint8_t val = 0;
    uint8_t drops_i = 0;
    uint8_t drops_e = drops.size();
    uint8_t drop_at;
    uint8_t drop_val;

    if (drops_i < drops_e) {
        drop_t drop = drops[drops_i++];
        drop_at = drop.first();
        drop_val = drop.third();
    } else {
        drop_at = LEDS;
    }

    uint8_t i = LEDS;
    do {
        i--;
        if (i == drop_at) {
            val += drop_val;
            if (drops_i < drops_e) {
                drop_t drop = drops[drops_i++];
                drop_at = drop.first();
                drop_val = drop.third();
            }
        } else if (val) {
            val = (val > 2) ? (val - (val >> 2) - 2) : 0;
        }
        state[i] = val;
    } while (i > 0);
}

inline
bool pass_time_on_drops(drop_list_t& drops) {
    bool change = false;
    bool* change_p = &change;
    if (!drops.empty()) {
        drops.foreach([change_p](drop_t& drop) {
            uint8_t time = drop.second();
            time--;
            if (!time) {
                drop.first()++;
                time = drop.third() >> 2;
                *change_p = true;
            }
            drop.second() = time;
        });

        if (drops.front().first() >= LEDS)
            drops.erase_front();
    }
    return change;
}

inline
void display(uint8_t* state, uint8_t times) {
    uint8_t round[LEDS];
    for (uint8_t j = 0; j < times; j++) {
        copy(round, state, LEDS);
        for (uint8_t i = 0; i < MAX; i++) {
            write(round, LEDS);
        }
    }
}


void main() {
    drop_list_t drops;
    uint8_t state[LEDS];
    uint8_t next_in = 1;
   
    for (;;) {
        // when next drop comes?
        if (!next_in) {
            if (!drops.full()) {
                next_in = random() >> 1;
                uint8_t size = next_in & MAX;
                if (size < 12)
                    size = 12;
                drop_t drop(0, size >> 2, size);
                drops.push_back(drop);
            }
        } else {
            next_in--;
        }
    
        // work on drops
        bool change = pass_time_on_drops(drops);
        if (change) {
            rgb.green();
            update_state(drops, state);
            rgb.off();
        }
    
        // lets now display it ;)
        display(state, 32);
    }
}


void setup() {
    rgb.setup();
    spi.setup();
}


void loop() {
    uint8_t state[LEDS] = {MAX};

    for (uint8_t r = 0; r < LEDS+10; r++) {
        uint8_t end = r < LEDS ? r : LEDS-1;
        uint8_t e = 60 + (LEDS - LEDS/4) - ((end > (LEDS/2) ? LEDS - end : end) >> (LEDS>16?2:1));

        // advance on non first round
        if (r)
            advance(state, end);

        // display
        display(state, e);
    }
}
