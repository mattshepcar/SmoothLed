#pragma once

namespace smoothled {

inline int16_t mac(int16_t value, int16_t a, uint8_t b)
{
    // value += (a * b) >> 8   9 cycles
    asm volatile(R"(
        mulsu   %B[a], %[b] ; (signed) ah * b
        add     %A[value], r0
        adc     %B[value], r1

        mul     %A[a], %[b] ; al * b
        add     %A[value], r1
        clr     __zero_reg__
        adc     %B[value], __zero_reg__
    )" : [value] "+&r" (value)
       : [a] "a" (a),
         [b] "a" (b)
       : "r0", "r1", "cc");
    return value;
}

inline int16_t fmac(int16_t value, int16_t a, uint8_t b)
{
    // value += (a * b) >> 7   10 cycles
    asm volatile(R"(
        fmul    %A[a], %[b] ; (al * b) << 1
        adc     %B[value], %[zero]
        add     %A[value], r1
        adc     %B[value], %[zero]

        fmulsu  %B[a], %[b] ; ((signed) ah * b) << 1
        add     %A[value], r0
        adc     %B[value], r1

        clr     __zero_reg__
    )" : [value] "+&r" (value)
       : [a] "a" (a),
         [b] "a" (b),
         [zero] "r" (0)
       : "r0", "r1", "cc");
    return value;
}

inline int16_t fmul(int16_t delta, uint16_t fraction)
{
    // (delta * fraction) >> 15  (20 cycles)
    int16_t result;
    uint16_t resultLo;
    asm volatile(R"(
        fmulsu  %B[delta], %B[fraction] ; ((signed) ah * bh) << 1
        movw    %[result], r0
        fmul    %A[delta], %A[fraction] ; (al * bl) << 1
        adc     %A[result], %[zero]
        movw    %[resultLo], r0
        fmulsu  %B[delta], %A[fraction] ; ((signed) ah * bl) << 1
        sbc     %B[result], %[zero]
        add     %B[resultLo], r0
        adc     %A[result], r1
        adc     %B[result], %[zero]
        fmul    %A[delta], %B[fraction] ; (al * bh) << 1
        adc     %B[result], %[zero]
        add     %B[resultLo], r0
        adc     %A[result], r1
        adc     %B[result], %[zero]
        clr     __zero_reg__
    )" : [result] "=&r" (result),
         [resultLo] "=&r" (resultLo)
       : [delta] "a" (delta),
         [fraction] "a" (fraction),
         [zero] "r" (0)
       : "r0", "r1", "cc");
    return result;
}

inline uint16_t lerp(uint16_t a, uint16_t b, uint8_t t)
{
    return mac(a, b - a, t);
}

} // namespace smoothled
