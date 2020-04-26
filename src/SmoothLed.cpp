#include "SmoothLed.h"

#define GAMMA_BITS 6

#define LOW_PULSE 250 // <400 
#define HIGH_PULSE 550 // >500
#define	RESET_CODE (50)

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define NS_TO_CYCLES(ns) (((ns) * F_CPU + 500000000) / 1000000000)
#define HIGH_CYCLES MAX(NS_TO_CYCLES(HIGH_PULSE), 4)
#define UARTX_BAUD (HIGH_CYCLES << 6)
#define TCB_DELAY  MAX(HIGH_CYCLES - 5, 0)
#if HIGH_CYCLES <= 4
#define SPI_FREQ (SPI_CLK2X_bm | SPI_PRESC_DIV16_gc)
#define SPI_TCB_DELAY 0
#elif HIGH_CYCLES <= 8
#define SPI_FREQ (SPI_PRESC_DIV16_gc)
#define SPI_TCB_DELAY 4
#else
#define SPI_FREQ (SPI_CLK2X_bm | SPI_PRESC_DIV64_gc)
#define SPI_TCB_DELAY 12
#endif

const uint16_t SmoothLed::Gamma25[(1 << GAMMA_BITS) + 1] =
{
    0x0000, 0x0002, 0x000b, 0x001f, 0x0040, 0x0070, 0x00b1, 0x0105,
    0x016c, 0x01e9, 0x027c, 0x0327, 0x03ec, 0x04ca, 0x05c3, 0x06d9,
    0x080c, 0x095d, 0x0acd, 0x0c5e, 0x0e0f, 0x0fe2, 0x11d7, 0x13f0,
    0x162d, 0x188f, 0x1b16, 0x1dc5, 0x209a, 0x2397, 0x26bd, 0x2a0c,
    0x2d85, 0x3129, 0x34f9, 0x38f4, 0x3d1c, 0x4171, 0x45f4, 0x4aa5,
    0x4f86, 0x5496, 0x59d7, 0x5f48, 0x64eb, 0x6ac0, 0x70c8, 0x7703,
    0x7d71, 0x8414, 0x8aec, 0x91f8, 0x993b, 0xa0b4, 0xa865, 0xb04c,
    0xb86c, 0xc0c4, 0xc955, 0xd21f, 0xdb23, 0xe462, 0xeddc, 0xf791,
    0xff00,
};

SmoothLed::SmoothLed(Interpolator* interpolators, uint16_t numInterpolators,
    const uint16_t* gammaLut, uint8_t ditherMask)
{
    m_Interpolators = interpolators;
    m_NumInterpolators = numInterpolators;
    m_Time = 0;
    m_GammaLut = gammaLut;
    m_DitherMask = ditherMask;
    uint8_t dither = 0;
    for (uint16_t i = 0; i < numInterpolators; ++i, dither += 26)
        m_Interpolators[i].dither = dither;
}

void SmoothLed::update(uint16_t deltaTime)
{
    uint8_t lastT = highByte(m_Time);
    m_Time += deltaTime;
    uint8_t dt = highByte(m_Time) - lastT;

    volatile uint8_t* outport;
    if (m_Usart)
    {
        USART0.BAUD = UARTX_BAUD;
        USART0.CTRLC = USART_CMODE_MSPI_gc | USART_UCPHA_bm;
        USART0.CTRLB = USART_TXEN_bm;
        outport = &USART0.TXDATAL;
    }
    else
    {
        SPI0.CTRLA = SPI_MASTER_bm | SPI_FREQ | SPI_ENABLE_bm;
        SPI0.CTRLB = SPI_SSD_bm | SPI_MODE_1_gc | SPI_BUFEN_bm;
        outport = &SPI0.DATA;
    }
    Interpolator* i = m_Interpolators;
    uint16_t count = m_NumInterpolators;
    uint8_t ditherMask = m_DitherMask;
    const uint16_t* gammaLut = m_GammaLut;
#if HIGH_CYCLES <= 4
    // cycle counted loop for maximum throughput
    // TODO: add delay loop for other CPU/LED speeds
    uint8_t tmp;
    uint16_t value;
    uint16_t* ptr;
    asm volatile(R"(
    0:  ldd     %A[value], %a[interpolator] + 2 ; 2
        ldd     %B[value], %a[interpolator] + 3 ; 2      
        ; value += (step * dt) >> 8
        ld      %[tmp], %a[interpolator]+       ; 2
        mul     %[tmp], %[dt]                   ; 2
        add     %A[value], r1                   ; 1
        adc     %B[value], %[zero]              ; 1
        ld      %[tmp], %a[interpolator]+       ; 2
        mulsu   %[tmp], %[dt]                   ; 2
        add     %A[value], r0                   ; 1
        adc     %B[value], r1                   ; 1
        ; clamping
        brpl    .+2                             ; 1
        movw    %[value], %[zero]               ; 1
        sbrc    %B[value], %[lutbits]           ; 1
        movw    %[value], %[maxvalue]           ; 1
        st      %a[interpolator]+, %A[value]    ; 1
        st      %a[interpolator]+, %B[value]    ; 1   22

        ; gamma correction
        movw    %[ptr], %[lut]                  ; 1
        lsl     %B[value]                       ; 1
        add     %A[ptr], %B[value]              ; 1
        adc     %B[ptr], %[zero]                ; 1
        ld      %[tmp], %a[ptr]+                ; 2/3
        ld      %B[value], %a[ptr]+             ; 2/3  8      30

        ; interpolation:
        ld      r0, %a[ptr]+                    ; 2/3
        ld      %[ptr], %a[ptr]                 ; 2/3
        sub     r0, %[tmp]                      ; 1
        sbc     %[ptr], %B[value]               ; 1
        ; (lowByte(delta) * lowByte(value)) >> 8
        mul     r0, %A[value]                   ; 2
        add     %[tmp], r1                      ; 1
        adc     %B[value], %[zero]              ; 1 
        ; highByte(delta) * lowByte(value)
        mul     %[ptr], %A[value]               ; 2
        add     %[tmp], r0                      ; 1
        adc     %B[value], r1                   ; 1   14     44

        ; temporal dithering
        ld      r0, %a[interpolator]            ; 2
        and     %[tmp], %[ditherMask]           ; 1
        add     %[tmp], r0                      ; 1
        adc     %B[value], %[zero]              ; 1
        ; clamp
        sbc     %B[value], %[zero]              ; 1
        st      %a[interpolator]+, %[tmp]       ; 1    7

        ; timing adjustment to 64 cycles
        ; if lut is not in PROGMEM we need 4 extra cycles
        sbrs    %B[lut], 7                      ; 1
        rjmp    2f                              ; 1  
  
        ; write to the LED strip
    1:  com     %B[value]                       ; 1
        movw    %a[ptr], %[outport]             ; 1
        st      %a[ptr], %B[value]              ; 1

        sbiw    %[loopCount], 1                 ; 2
        brne    0b                              ; 2    7

        clr     __zero_reg__
        rjmp    0f

        ; extra delay cycles for RAM based gamma LUT
    2:  nop
        rjmp    1b
    0:
    )"  : // outputs
        [interpolator] "+&b" (i), // LDD
        [loopCount] "+&w" (count), // SBIW
        [value] "=&r" (value),
        [ptr] "=&e" (ptr), // LD
        [tmp] "=&a" (tmp) // MULSU
        // inputs
      : [dt] "a" (dt), // MULSU
        [lut] "r" (m_GammaLut),
        [lutbits] "i" (GAMMA_BITS),
        [outport] "r" (outport),
        [ditherMask] "r" (ditherMask),
        [zero] "r" (0),
        [maxvalue] "r" ((1 << GAMMA_BITS) - 1)
      : "r0", "r1", "cc"
    );       
#else
    do
    {
        uint8_t value = i++->update(dt, gammaLut, ditherMask);
        *outport = ~value;
    } while (--count);
#endif
    if (m_Usart)
    {
        delayMicroseconds(10);
        USART0.CTRLB = 0;
    }
    else
    {
        delayMicroseconds(10);       
        SPI0.CTRLA = 0;
    }
}

static inline uint16_t lerp(uint16_t a, uint16_t b, uint8_t t)
{
    uint16_t delta = b - a;
    a += highByte(lowByte(delta) * t);
    a += highByte(delta) * t;
    return a;
}

static inline uint16_t mul(int16_t a, uint8_t b)
{
    return highByte(lowByte(a) * b) + int8_t(a >> 8) * b;
}

uint8_t SmoothLed::Interpolator::update(uint8_t dt, const uint16_t* lut, uint8_t ditherMask)
{
    value += mul(step, dt);
    if (value < 0)
        value = 0;
    if (value >= (1 << GAMMA_BITS))
        value = (1 << GAMMA_BITS) - 1;
    lut += highByte(value);
    uint16_t corrected = lerp(lut[0], lut[1], lowByte(value)) + dither;
    dither = lowByte(corrected) & ditherMask;
    return highByte(corrected);
}

void SmoothLed::Interpolator::setTarget(uint8_t target, uint16_t fraction)
{
    uint8_t current = highByte(value << (8 - GAMMA_BITS));
#if 1
    uint8_t sign;
    asm volatile(R"(
        sub     %[delta], %[current]
        sbc     %[sign], %[sign]
        mul     %[fractionHi], %[delta]               
        movw    %[step], r0
        mul     %[fractionLo], %[delta]
        add     %A[step], r1
        clr     __zero_reg__
        adc     %B[step], __zero_reg__
        ; handle negative delta
        sbrc    %[sign], 7
        sub     %A[step], %[fractionLo]
        sbrc    %[sign], 7
        sbc     %B[step], %[fractionHi]
    )": [step] "=&w" (step),
        [delta] "=&r" (target),
        [sign] "=&r" (sign)
      : [fractionHi] "r" (highByte(fraction)),
        [fractionLo] "r" (lowByte(fraction)),
        [current] "r" (current),
        "[delta]" (target)
      : "r0", "r1", "cc");
#else    
    int16_t delta = target - current;
    step = (delta * fraction) >> 8;
    //step = delta * (fraction >> 8);
    //step = mul(fraction, delta);
    //if (target < current)
    //    step -= fraction; // sign extend
#endif
}


void SmoothLed::begin(OutputPin outpin, ClockPin sckpin, uint8_t tcb, uint8_t lut)
{
    volatile uint8_t& TCBEV = tcb == 0 ? EVSYS_ASYNCUSER0 : EVSYS_ASYNCUSER11;
    m_Usart = false;
    switch (sckpin)
    {
    case USART0_PA3:
        m_Usart = true;
        PORTMUX.CTRLB |= PORTMUX_USART0_ALTERNATE_gc;
    case SPI0_PA3:
        EVSYS_ASYNCCH0 = EVSYS_ASYNCCH0_PORTA_PIN3_gc;
        TCBEV = EVSYS_ASYNCUSER0_ASYNCCH0_gc;
        break;
    case USART0_PB1:
        m_Usart = true;
        EVSYS_ASYNCCH1 = EVSYS_ASYNCCH1_PORTB_PIN1_gc;
        TCBEV = EVSYS_ASYNCUSER0_ASYNCCH1_gc;
        break;
#ifdef PORTC
    case SPI0_PC0:
        EVSYS_ASYNCCH2 = EVSYS_ASYNCCH2_PORTC_PIN0_gc;
        TCBEV = EVSYS_ASYNCUSER0_ASYNCCH2_gc;
        PORTMUX.CTRLB |= PORTMUX_SPI0_ALTERNATE_gc;
        break;
#endif
    }
    volatile TCB_t& TCB = tcb == 0 ? TCB0 : TCB1;
    TCB.EVCTRL = TCB_CAPTEI_bm;
    TCB.CCMP = TCB0.CNT = (m_Usart ? TCB_DELAY : SPI_TCB_DELAY) +// use enough time to get to SCK=0
        MAX(NS_TO_CYCLES(LOW_PULSE) - 1, 1);
    TCB.CTRLB = TCB_CNTMODE_SINGLE_gc | TCB_ASYNC_bm;
    TCB.CTRLA = TCB_CLKSEL_CLKDIV1_gc | TCB_ENABLE_bm;

    if (outpin == PA4) lut = 0;
    else if (outpin == PA7) lut = 1;
#ifdef PORTC
    else if (outpin == PB4) lut = 0;
    else if (outpin == PC1) lut = 1;
#endif

#if !defined(CCL_INSEL2_TCB1_gc)
#define	CCL_INSEL2_TCB1_gc	(0x0D) // TODO: don't think this is actually valid...
#endif

    volatile uint8_t* LUT = lut ? &CCL.LUT1CTRLA : &CCL.LUT0CTRLA;
    LUT[3] = m_Usart ? 0b01010001 : 0b01010100;
    LUT[2] = tcb ? CCL_INSEL2_TCB1_gc : CCL_INSEL2_TCB0_gc;
    LUT[1] = m_Usart ? (CCL_INSEL1_USART0_gc | CCL_INSEL0_USART0_gc) : (CCL_INSEL1_SPI0_gc | CCL_INSEL0_SPI0_gc);
    LUT[0] = CCL_ENABLE_bm;

    switch (outpin)
    {
    case PA2_ASYNCCH0: case PA2_ASYNCCH1: case PA2_ASYNCCH2: case PA2_ASYNCCH3:
        (&EVSYS_ASYNCCH0)[outpin - PA2_ASYNCCH0] = EVSYS_ASYNCCH0_CCL_LUT0_gc + lut;
        EVSYS_ASYNCUSER8 = EVSYS_ASYNCUSER8_ASYNCCH0_gc + outpin - PA2_ASYNCCH0;
        PORTMUX.CTRLA |= PORTMUX_EVOUT0_bm;
        break;
    case PB2_ASYNCCH0: case PB2_ASYNCCH1: case PB2_ASYNCCH2: case PB2_ASYNCCH3:
        (&EVSYS_ASYNCCH0)[outpin - PB2_ASYNCCH0] = EVSYS_ASYNCCH0_CCL_LUT0_gc + lut;
        EVSYS_ASYNCUSER9 = EVSYS_ASYNCUSER9_ASYNCCH0_gc + outpin - PB2_ASYNCCH0;
        PORTMUX.CTRLA |= PORTMUX_EVOUT1_bm;
        break;
#ifdef PORTC
    case PC2_ASYNCCH0: case PC2_ASYNCCH1: case PC2_ASYNCCH2: case PC2_ASYNCCH3:
        (&EVSYS_ASYNCCH0)[outpin - PC2_ASYNCCH0] = EVSYS_ASYNCCH0_CCL_LUT0_gc + lut;
        EVSYS_ASYNCUSER10 = EVSYS_ASYNCUSER10_ASYNCCH0_gc + outpin - PC2_ASYNCCH0;
        PORTMUX.CTRLA |= PORTMUX_EVOUT2_bm;
        break;
    case PIN_PB4:
        PORTMUX.CTRLA |= PORTMUX_LUT0_bm;
#endif
    case PA4:
        CCL.LUT0CTRLA |= CCL_OUTEN_bm;
        break;
#ifdef PORTC
    case PC1:
        PORTMUX.CTRLA |= PORTMUX_LUT1_bm;
#endif
    case PA7:
        CCL.LUT1CTRLA |= CCL_OUTEN_bm;
        break;
    }
    CCL.CTRLA = CCL_ENABLE_bm;
}

void SmoothLed::bitBang(uint8_t pin, const uint8_t* pixels, uint16_t numbytes)
{
#if F_CPU == 8000000
    uint8_t pinMask = digitalPinToBitMask(pin);
    volatile uint8_t* port = portOutputRegister(digitalPinToPort(pin));

    // 8 instruction clocks per bit: HHxxxLLL
    // OUT instructions:             ^ ^  ^   (T=0,2,5)
    cli();
    uint8_t hi = *port |  pinMask;
    uint8_t lo = *port & ~pinMask;
    uint8_t b = *pixels++;
    uint8_t n1 = b & 0x80 ? hi : lo;
    uint8_t n2;
    asm volatile(R"(
    0:  ; Bit 7:
        st   %[port], %[hi]
        mov  %[n2], %[lo]
        st   %[port], %[n1]
        sbrc %[byte], 6
        mov  %[n2], %[hi]
        st   %[port], %[lo]
        rjmp .        
        ; Bit 6:
        st   %[port], %[hi]
        mov  %[n1], %[lo]
        st   %[port], %[n2]
        sbrc %[byte], 5
        mov  %[n1], %[hi]
        st   %[port], %[lo]
        rjmp .
        ; Bit 5:
        st   %[port], %[hi]
        mov  %[n2], %[lo]
        st   %[port], %[n1]
        sbrc %[byte], 4
        mov  %[n2], %[hi]
        st   %[port], %[lo]
        rjmp .
        ; Bit 4:
        st   %[port], %[hi]
        mov  %[n1], %[lo]
        st   %[port], %[n2]
        sbrc %[byte], 3
        mov  %[n1], %[hi]
        st   %[port], %[lo]
        rjmp .
        ; Bit 3:
        st   %[port], %[hi]
        mov  %[n2], %[lo]
        st   %[port], %[n1]
        sbrc %[byte], 2
        mov  %[n2], %[hi]
        st   %[port], %[lo]
        rjmp .
        ; Bit 2:
        st   %[port], %[hi]
        mov  %[n1], %[lo]
        st   %[port], %[n2]
        sbrc %[byte], 1
        mov  %[n1], %[hi]
        st   %[port], %[lo]
        sbiw %[count], 1
        ; Bit 1:
        st   %[port], %[hi]
        mov  %[n2], %[lo]
        st   %[port], %[n1]
        sbrc %[byte], 0
        mov  %[n2], %[hi]
        st   %[port], %[lo]
        ld   %[byte], %a[ptr]+
        ; Bit 0:
        st   %[port], %[hi]
        mov  %[n1], %[lo]
        st   %[port], %[n2]
        sbrc %[byte], 7
        mov  %[n1], %[hi]
        st   %[port], %[lo]
        brne 0b
    )": [byte]  "+&r" (b),
        [n1]    "+&r" (n1),
        [n2]    "=&r" (n2),
        [count] "+&w" (numbytes)
    :   [port]  "e" (port),
        [ptr]   "e" (pixels),
        [hi]    "r" (hi),
        [lo]    "r" (lo));
    sei();
#else
    // not implemented
#endif
}
