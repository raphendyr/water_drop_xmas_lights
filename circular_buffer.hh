#ifndef __YAAL_CIRCULAR_BUFFER__
#define __YAAL_CIRCULAR_BUFFER__ 1

#include <inttypes.h>

// for api: http://www.boost.org/doc/libs/1_52_0/libs/circular_buffer/doc/circular_buffer.html

template< uint8_t buffer_size, typename elem_t = uint8_t >
class CircularBuffer {
    uint8_t tail;
    uint8_t head;
    uint8_t count;
    elem_t buffer[buffer_size];
 
    inline
    void forward(uint8_t& i) const {
        i++;
        if (i == buffer_size)
            i = 0;
    }

    inline
    void backward(uint8_t& i) const {
        if (i == 0)
            i = buffer_size - 1;
        else
            i--;
        // FIXME: witch one make faster code?
        /*
        if (i == 0)
            i = size;
        i--;
        */
    }

    inline
    uint8_t forward_modulo(uint8_t i, uint8_t n) const {
        uint8_t o = buffer_size - i;
        if (n >= o)
            i = n - o;
        else
            i = i + n;
        return i;
    }

public:
    CircularBuffer() : tail(0), head(0), count(0) {}

    // FIXME: better function name
    uint8_t freeSpace() const {
        return buffer_size - count;
    }

    bool full() const {
        return count == buffer_size;
    }

    bool empty() const {
        return count == 0;
    }

    uint8_t size() const {
        return count;
    }
    
    uint8_t capacity() const {
        return buffer_size;
    }

    uint8_t clear() {
        head = 0;
        tail = 0;
        count = 0;
    }

    void push_back(elem_t elem) {
        buffer[head] = elem;
        forward(head);
        if (full())
            forward(tail);
        else
            count++;
    }

    void push_front(elem_t elem) {
        backward(tail);
        buffer[tail] = elem;
        if (full())
            backward(head);
        else
            count++;
    }

    elem_t pop_front() {
        elem_t elem = buffer[tail];
        if (!empty()) {
            count--;
            forward(tail);
        }
        return elem;
    }

    elem_t pop_back() {
        if (!empty()) {
            count--;
            backward(head);
        }
        return buffer[head];
    }

    elem_t& at(uint8_t i) {
        return buffer[forward_modulo(tail, i)];
    }

    const elem_t& at(uint8_t i) const {
        return buffer[forward_modulo(tail, i)];
    }

    elem_t& operator[] (uint8_t i) {
        return at(i);
    }

    const elem_t& operator[] (uint8_t i) const {
        return at(i);
    }


    // iters:
    // begin
    // end
    // rbegin
    // rend
    // front
    // back
};

#endif
