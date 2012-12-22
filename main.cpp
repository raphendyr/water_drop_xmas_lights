// there is no better infrastructure for this yet
#define ARDUINO_teensy

#include "yaal/arduino.hh"
#include "yaal/devices/rgbled.hh"
#include "yaal/communication/spi.hh"
#include <util/delay.h>

// with triple 976 bytes
#include "triple.hh"
#include "drop.hh"
#include "circular_buffer.hh"

using namespace yaal;
using namespace yaal::arduino;

//#define TOP_TO_BOTTOM

#define LEDS 32
#define BLOCKS (LEDS/8)
#define DROP_END (LEDS + 10)
//#define MAX (0xff >> 3)
#define MAX 31
#define BRIGNESS 230 // 220
#define DROP_FASTEST 1
#define DROP_SLOWEST 4

BasicRGBLed<D1, D2, D0> rgb;

typedef D21 Latch;
typedef D18 Data;
typedef D15 Clock;

PortB7 pwm;

SPI<Clock, Data, NullPin, Latch> spi;

//typedef Triple<uint8_t> Drop;
typedef CircularBuffer<6, Drop> drop_list_t;

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

    TCCR0B |= _BV(CS00); // 010, pre 1
    //TCCR0B |= _BV(CS01); // 010, pre 8
	//TCCR0B |= _BV(CS01) | _BV(CS00); // 011, pre 64
	//TCCR0B |= _BV(CS02); // 100, pre 256
	//TCCR0B |= _BV(CS02) | _BV(CS00); // 101, prw 1024
}


inline
void pwmWrite(uint8_t* data, uint8_t bits, uint8_t top) {
    for (uint8_t compare = 0; compare < top; compare++) {
        LowPeriod<Latch> latch_low_for_this_block;
#ifdef TOP_TO_BOTTOM
        uint8_t bit = LEDS - 1;
        internal::shiftBitsIf<Clock, Data>(bits, [data, compare, &bit](uint8_t){
            return data[bit--] > compare;
        });
#else
        internal::shiftBitsIf<Clock, Data>(bits, [data, compare](uint8_t i){
            return data[i] > compare;
        });
#endif
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
    uint8_t drop_at, drop_mass;

    if (drops_i < drops_e) {
        Drop& drop = drops[drops_i++];
        drop_at = drop.location();
        drop_mass = drop.mass();
    } else {
        drop_at = (uint8_t)-1;
        drop_mass = 0;
    }

    uint8_t i = DROP_END;
    do {
        i--;
        if (i == drop_at) {
            do {
                val += drop_mass;

                Drop& drop = drops[drops_i++];
                drop_at = drop.location();
                drop_mass = drop.mass();
            } while (i == drop_at && drops_i < drops_e);
        } else if (val) {
            val = (val > 2) ? (val - (val >> 2) - 2) : 0;
        }
        if (i < LEDS) {
            uint8_t place = (i | 0x07) - (i & 0x07);
            state[place] = val;
        }
    } while (i > 0);
}

inline
bool pass_time_on_drops(drop_list_t& drops) {
    bool change = false;
    uint8_t prev_loc = DROP_END;
    if (!drops.empty()) {
        drops.foreach([&](drop_list_t::size_type i, Drop& drop) {
            if (drop.time_step()) {
                change = true;
                if (drop.location() == prev_loc) {
                    Drop& prev = drops[i - 1];
                    uint8_t speed = ( drop.speed() + prev.speed() ) >> 1;
                    prev.speed() = speed;
                    prev.delay() = speed;
                    drop.speed() = speed;
                    drop.delay() = speed;
                }
            }
            prev_loc = drop.location();
        });

        while (!drops.empty() && drops.front().location() >= DROP_END)
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
    uint8_t next_in = 0;
   
    for (;;) {
        // when next drop comes?
        if (!next_in) {
            if (!drops.full()) {
                next_in = random() >> 2;
                uint8_t size = next_in;
                while (size > MAX) size -= MAX;
                if (size < 5)
                    size = 5;
                //uint8_t speed = size >> 2;
                uint8_t speed = (size & 0x3) + 1;
                Drop drop(speed, size);
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
    //pwm = true;
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
