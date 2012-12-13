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
#define DROP_END (LEDS + 10)
#define MAX (0xff >> 2)
#define BRIGNESS 20

BasicRGBLed<D1, D2, D0> rgb;

typedef D21 Latch;
typedef D18 Data;
typedef D15 Clock;

PortB7 pwm;

SPI<Clock, Data, NullPin, Latch> spi;

typedef Triple<uint8_t> drop_t;
typedef CircularBuffer<3, drop_t> drop_list_t;

uint8_t random() {
    static uint8_t r;
    uint8_t b1 = r & 0x80;
    uint8_t r2 = r << 1;
    uint8_t b2 = r << 2 & 0x80;
    uint8_t bit = !(b1 ^ b2);
    r = r2 | bit;
    return r;
}


inline void set_prigness(unsigned char);
void set_prigness(unsigned char val) {
	if (val > 255)
		val = 255;
	OCR0A = val;
}

inline void prigness_init(unsigned char);
void prigness_init(unsigned char prigness) {
	// 8 bit, => TOP = 255
	// normal mode
	// PB7
	TCCR0A = _BV(COM0A1) | _BV(COM0A0) | _BV(WGM00) | _BV(WGM01);

	set_prigness(prigness);

	//TCCR0B |= _BV(CS01) | _BV(CS00); // 011, pre 64
	TCCR0B |= _BV(CS02); // 100, pre 256
	//TCCR0B |= _BV(CS02) | _BV(CS00); // 101, prw 1024
}


inline
void pwmWrite(uint8_t* data, uint8_t bits, uint8_t top) {
    for (uint8_t compare = 0; compare < top; compare++) {
        LowPeriod<Latch> latch_low_for_this_block;
        internal::shiftBitsIf<Clock, Data>(bits, [data, compare](uint8_t i){
            return data[i] > compare;
        });
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
        drop_t& drop = drops[drops_i++];
        drop_at = drop.first();
        drop_val = drop.third();
    } else {
        drop_at = (uint8_t)-1;
    }

    uint8_t i = DROP_END;
    do {
        i--;
        if (i == drop_at) {
            do {
                val += drop_val;

                drop_t& drop = drops[drops_i++];
                drop_at = drop.first();
                drop_val = drop.third();
            } while (i == drop_at && drops_i < drops_e);
        } else if (val) {
            val = (val > 2) ? (val - (val >> 2) - 2) : 0;
        }
        if (i < LEDS)
            state[i] = val;
    } while (i > 0);
}

inline
bool pass_time_on_drops(drop_list_t& drops) {
    bool change = false;
    uint8_t prev = DROP_END;
    if (!drops.empty()) {
        drops.foreach([&](drop_list_t::size_type i, drop_t& drop) {
            uint8_t time = --drop.second();
            uint8_t location = drop.first();
            if (!time) {
                drop.first() = ++location;
                drop.second() = drop.third() >> 2;
                if (location > prev) {
                    drop_t& p = drops[i - 1];
                    drop_t t = p;
                    p = drop;
                    drop = t;
                }
                change = true;
            }
            prev = location;
        });

        if (drops.front().first() >= DROP_END)
            drops.erase_front();
    }
    return change;
}

inline
void display(uint8_t* state, uint8_t times) {
    for (uint8_t j = 0; j < times; j++) {
        pwmWrite(state, LEDS, MAX);
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
                uint8_t size = next_in >> 1;
                if (size < 15)
                    size = 15;
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

    pwm.mode = OUTPUT;
    prigness_init(BRIGNESS);
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
