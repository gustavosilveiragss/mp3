#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
using std::max;
using std::min;
#define PROGMEM
#define TWO_PI 6.2831853f
#define pgm_read_byte(a) (*(const unsigned char*)(a))
#define constrain(x, a, b) ((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))
extern uint32_t g_now;
inline uint32_t millis() { return g_now; }
class Print {
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t) = 0;
};
class Stream : public Print {};
struct String : std::string {
    using std::string::string;
    String(const std::string& o) : std::string(o) {}
    void toCharArray(char* b, unsigned n) const { snprintf(b, n, "%s", c_str()); }
};
struct SerialStub {
    template <typename... A> void printf(const char*, A...) {}
    template <typename T> void print(T) {}
    template <typename T> void println(T) {}
};
inline SerialStub Serial;
inline void yield() {}
