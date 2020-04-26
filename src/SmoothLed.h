// SmoothLED for tinyAVR-0/1 series
// Matt Shepcar 25/04/2020

#ifndef _SmoothLed_h
#define _SmoothLed_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

class SmoothLed
{
public:
    struct Interpolator
    {
        int16_t step;
        int16_t value;
        uint8_t dither;
        uint8_t update(uint8_t dt, const uint16_t* gammaLut, uint8_t ditherMask);
        void setTarget(uint8_t target, uint8_t fraction = 0x40);
        void setTarget(uint8_t target, uint16_t fraction);
        void stop();
    };

    enum OutputPin
    {
        PA4, // LUT0 output
        PA7, // LUT1 output
        PA2_ASYNCCH0, PA2_ASYNCCH1, PA2_ASYNCCH2, PA2_ASYNCCH3, // EVOUT0
        PB2_ASYNCCH0, PB2_ASYNCCH1, PB2_ASYNCCH2, PB2_ASYNCCH3, // EVOUT1
#ifdef PORTC
        PB4, // LUT0 alt output
        PC1, // LUT1 alt output
        PC2_ASYNCCH0, PC2_ASYNCCH1, PC2_ASYNCCH2, PC2_ASYNCCH3, // EVOUT2
#endif
    };
    enum ClockPin
    {
        SPI0_PA3,
        USART0_PA3,
        USART0_PB1,
#ifdef PORTC
        SPI0_PC0,
#endif
    };

    static const uint16_t Gamma25[];

    SmoothLed(Interpolator* interpolators, uint16_t numInterpolators,
              const uint16_t* gammaLut = Gamma25, uint8_t ditherMask = 0xF8);

    void begin(OutputPin outpin = PA4,
               ClockPin sckpin = USART0_PB1,
               uint8_t tcb = 0, uint8_t lut = 0);
    void beginSecondary();
    
    void setGammaLut(const uint16_t* gammaLut);
    void setDitherMask(uint8_t ditherMask);
    void update(uint16_t deltaTime);
    Interpolator* getInterpolators();

    static void bitBang(uint8_t pin, const uint8_t* pixels, uint16_t numbytes);

private:
    Interpolator* m_Interpolators;
    uint16_t m_NumInterpolators;
    uint16_t m_Time;
    const uint16_t* m_GammaLut;
    uint8_t m_DitherMask;
    bool m_Usart;
};

inline SmoothLed::Interpolator* SmoothLed::getInterpolators()
{
    return m_Interpolators;
}
inline void SmoothLed::setGammaLut(const uint16_t* gammaLut)
{
    m_GammaLut = gammaLut;
}
inline void SmoothLed::setDitherMask(uint8_t ditherMask)
{
    m_DitherMask = ditherMask;
}
inline void SmoothLed::beginSecondary()
{
    begin(PA7, SPI0_PA3, 1, 1);
}
inline void SmoothLed::Interpolator::stop() 
{
    //value = 0;
    step = 0; 
}
inline void SmoothLed::Interpolator::setTarget(uint8_t target, uint8_t fraction)
{
    step = target * fraction - value;
}

#endif
