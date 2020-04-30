#include <avr/io.h>

; extern "C" void SmoothLedUpdate8cpb(
;   uint16_t count,  r24
;   Interpolator*,   r22     zero
;   register8_t*     r20
;   uint8_t dt,      r18     r19 tmp
;   uint16_t ditherMask, r16  r17 tmp
;   uint16_t maxValue, r14
;   uint16_t* gammaLut, r12
;   register8_t*     r10  status

.section .text.SmoothLedUpdate8cpb, "ax", @progbits
.global SmoothLedUpdate8cpb
.type SmoothLedUpdate8cpb, @function
SmoothLedUpdate8cpb:
        push    YL
        push    YH
        movw    Y, r22
        movw    Z, r10
        mov     r11, r20
        clr     r22
        clr     r23
        sbiw    r24, 1

    0:  ldd     r20, Y + 2              ; 2
        ldd     r21, Y + 3              ; 2
        ; value += (step * dt) >> 7
        ld      r19, Y+                 ; 2
        fmul    r19, r18                ; 2
        adc     r21, r22                ; 1
        add     r20, r1                 ; 1
        adc     r21, r22                ; 1
        ld      r19, Y+                 ; 2
        fmulsu  r19, r18                ; 2
        add     r20, r0                 ; 1
        adc     r21, r1                 ; 1

        ; clamping (+2 cycles on fail)
        cp      r15, r21                ; 1
        brsh    1f                      ; 2
         movw    r20, r22
         brge    1f
          movw    r20, r14
1:      st      Y+, r20                 ; 1
        st      Y+, r21                 ; 1          22

        ; gamma correction
        movw    X, r12                  ; 1
        lsl     r21                     ; 1
        add     XL, r21                 ; 1
        adc     XH, r22                 ; 1
        ld      r19, X+                 ; 2/3
        ld      r21, X+                 ; 2/3  10    32

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
        adc     r21, r1                 ; 1   16     48

        ; temporal dithering
        ld      r0, Y                   ; 2
        and     r19, r16                ; 1
        add     r19, r0                 ; 1
        adc     r21, r22                ; 1
        st      Y+, r19                 ; 1   6      54

        ; wait for data register empty
        mov     ZL, r10                 ; 1
1:      ld      r0, Z                   ; 2
        sbrs    r0, USART_DREIF_bp      ; 1
        rjmp    1b                      ; 1
        
        ; write to the LED strip
        mov     ZL, r11                 ; 1
        st      Z, r21                  ; 1

        subi    r24, 1                  ; 1
        brcc    0b                      ; 2   10     64
        subi    r25, 1
        brcc    0b

        clr     r1
        pop     YH
        pop     YL
        ret


.section .text.SmoothLedUpdate, "ax", @progbits
.global SmoothLedUpdate
.type SmoothLedUpdate, @function
SmoothLedUpdate:
        push    YL
        push    YH
        movw    Y, r22
        movw    Z, r20
        clr     r22
        clr     r23
        sbiw    r24, 1

    0:  ldd     r20, Y + 2              ; 2
        ldd     r21, Y + 3              ; 2
        ; value += (step * dt) >> 7
        ld      r19, Y+                 ; 2
        fmul    r19, r18                ; 2
        adc     r21, r22                ; 1
        add     r20, r1                 ; 1
        adc     r21, r22                ; 1
        ld      r19, Y+                 ; 2
        fmulsu  r19, r18                ; 2
        add     r20, r0                 ; 1
        adc     r21, r1                 ; 1

        ; clamping (+2 cycles on fail)
        cp      r15, r21                ; 1
        brsh    1f                      ; 2
         movw    r20, r22
         brge    1f
          movw    r20, r14
1:      st      Y+, r20                 ; 1
        st      Y+, r21                 ; 1          22

        ; gamma correction
        movw    X, r12                  ; 1
        lsl     r21                     ; 1
        add     XL, r21                 ; 1
        adc     XH, r22                 ; 1
        ld      r19, X+                 ; 2/3
        ld      r21, X+                 ; 2/3  10    32

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
        adc     r21, r1                 ; 1   16     48

        ; temporal dithering
        ld      r0, Y                   ; 2
        and     r19, r16                ; 1
        add     r19, r0                 ; 1
        adc     r21, r22                ; 1
        st      Y+, r19                 ; 1   6      54
        
        ; write to the output buffer
        st      Z+, r21                 ; 1

        subi    r24, 1                  ; 1
        brcc    0b                      ; 2   10     64
        subi    r25, 1
        brcc    0b

        clr     r1
        pop     YH
        pop     YL
        ret
