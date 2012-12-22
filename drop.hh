#ifndef __DROP__
#define __DROP__ 1

#include "type_traits.hh"

class Drop {
    uint8_t _location;
    uint8_t _delay;
    uint8_t _speed;
    uint8_t _mass;

public:
    Drop(uint8_t speed, uint8_t mass)
    : _location(0), _delay(speed), _speed(speed), _mass(mass)
    { }

    Drop() {}

    Drop(const Drop& other) {
        *this = other;
    }

    Drop& operator=(const Drop& other) {
        _location = other._location;
        _delay = other._delay;
        _speed = other._speed;
        _mass = other._mass;
        return *this;
    }
    
    bool time_step() {
        _delay--;
        if (!_delay) {
            _location++;
            _delay = _speed;
            return true;
        }
        return false;
    }
    
    uint8_t location() const { return _location; }
    uint8_t& location() { return _location; }
    
    uint8_t delay() const { return _delay; }
    uint8_t& delay() { return _delay; }
    
    uint8_t speed() const { return _speed; }
    uint8_t& speed() { return _speed; }
    
    uint8_t mass() const { return _mass; }
    uint8_t& mass() { return _mass; }
};

#endif
