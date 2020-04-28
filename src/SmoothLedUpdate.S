#include <avr/io.h>

; extern "C" void SmoothLedUpdate8cpb(
;   uint16_t count,  r24
;   Interpolator*,   r22     zero
;   register8_t*     r20
;   uint8_t dt,      r18     r19 tmp
;   uint8_t ditherMask, r16  r17 tmp
;   uint16_t maxValue, r14
;   uint16_t* gammaLut, r12

.global SmoothLedUpdate8cpb
SmoothLedUpdate8cpb:
        push    YL
        push    YH
        push    r2
        movw    Y, r22
        movw    Z, r20
        clr     r22
        clr     r23

        mov     r2, r16
        mov     r16, ZL
        sbrs    r13, 7
        inc     r16

    0:  ldd     r20, Y + 2              ; 2
        ldd     r21, Y + 3              ; 2      
        ; value += (step * dt) >> 8
        ld      r19, Y+                 ; 2
        fmul    r19, r18                ; 2
        adc     r21, r22                ; 1
        add     r20, r1                 ; 1
        adc     r21, r22                ; 1
        ld      r19, Y+                 ; 2
        fmulsu  r19, r18                ; 2
        add     r20, r0                 ; 1
        adc     r21, r1                 ; 1
        ; clamping
        brpl    .+2                     ; 1
        movw    r20, r22                ; 1
        cp      r15, r21                ; 1
        brge    .+2                     ; 1
        movw    r20, r14                ; 1
        st      Y+, r20                 ; 1
        st      Y+, r21                 ; 1          24

        ; gamma correction
        movw    X, r12                  ; 1
        lsl     r21                     ; 1
        add     XL, r21                 ; 1
        adc     XH, r22                 ; 1
        ld      r19, X+                 ; 2/3
        ld      r21, X+                 ; 2/3  8     32

        ; interpolation:
        ld      r0, X+                  ; 2/3
        ld      r17, X                  ; 2/3
        sub     r0, r19                 ; 1
        sbc     r17, r21                ; 1
        ; (lowByte(delta) * lowByte(value)) >> 8
        mul     r0, r20                 ; 2
        add     r19, r1                 ; 1
        adc     r21, r22                ; 1 
        ; highByte(delta) * lowByte(value)
        mulsu   r17, r20                ; 2
        add     r19, r0                 ; 1
        adc     r21, r1                 ; 1   14     46

        ; temporal dithering
        ld      r0, Y                   ; 2
        and     r19, r2                 ; 1
        add     r19, r0                 ; 1
        adc     r21, r22                ; 1
        st      Y+, r19                 ; 1   6      52

        ; timing adjustment:
        ;  read from PROGMEM takes 4 more cycles
        ;  write to SPI requires a 2 cycle gap
        cpi     r16, lo8(USART0_TXDATAL); usart + progmem
        breq    1f
        cpi     r16, lo8(SPI0_DATA)     ; spi + progmem
        breq    1f
        cpi     r16, lo8(USART0_TXDATAL + 1) ; usart + sram
        breq    1f
        nop                             ; spi + sram
        rjmp .
  
        ; write to the LED strip
    1:  st      Z, r21                  ; 1

        sbiw    r24, 1                  ; 2
        brne    0b                      ; 2   5      57/61

        clr     r1
        pop     r2
        pop     YH
        pop     YL
        ret

        ; extra delay cycles for RAM based gamma LUT
    2:  nop
        rjmp    1b
