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
    struct Interpolator;

    static const uint8_t Gamma25Size = 32;
    static const uint16_t Gamma25[Gamma25Size];
    enum OutputPin { PA2, PA4, PA7, PB2, PB4, PC1, PC2 };
    enum ClockPin { USART0_PA3, SPI0_PA3, USART0_PB1, SPI0_PC0 };
    enum Lut { LUT0, LUT1 };

    SmoothLed(Interpolator* interpolators, uint16_t numInterpolators,
        uint8_t ditherMask = 0xF8,
        const uint16_t* gammaLut = Gamma25, uint8_t gammaLutSize = Gamma25Size);
    void begin(OutputPin outpin = PA4,
               ClockPin sckpin = USART0_PB1,
               volatile TCB_t& tcb = TCB0, Lut lut = LUT0,
               int highPulseNs = 550, int lowPulseNs = 250);
    void beginSecondary();
    
    void setGammaLut(const uint16_t* gammaLut, uint8_t numEntries);
    void setDitherMask(uint8_t ditherMask);
    void update(uint16_t deltaTime);
    Interpolator* getInterpolators();
    Interpolator& getInterpolator(uint16_t index);

    uint8_t getRange() const;
    uint16_t expandRange(uint8_t value) const; // convert 8 bit colour to 16 bits

    void setTarget(uint16_t index, uint8_t target);
    void setTarget(uint16_t index, uint8_t target, uint16_t fraction); // 1.15 fraction (0x8000 = 1.0)
    void setTarget(uint16_t index, const uint8_t* target, uint16_t count);
    void setTarget(uint16_t index, const uint8_t* target, uint16_t count, uint16_t fraction);

    struct Interpolator
    {
        int16_t step;
        int16_t value;
        uint8_t dither;

        void setTarget(uint16_t target);
        void setTarget(uint16_t target, uint16_t fraction);
        void setTarget(uint8_t target, uint8_t range);
        void setTarget(uint8_t target, uint8_t range, uint16_t fraction);
        void stop();

        static uint16_t expandRange(uint8_t value, uint8_t maxValue);

        uint8_t update(uint8_t dt, const uint16_t* gammaLut, uint16_t maxValue, uint8_t ditherMask);
    };

private:
    Interpolator* m_Interpolators;
    uint16_t m_NumInterpolators;
    uint16_t m_Time;
    const uint16_t* m_GammaLut;
    uint8_t m_GammaLutSize;
    uint8_t m_DitherMask;
    uint8_t m_Spi;
    uint8_t m_HighCycles;
};

inline SmoothLed::Interpolator* SmoothLed::getInterpolators()
{
    return m_Interpolators;
}
inline void SmoothLed::setGammaLut(const uint16_t* gammaLut, uint8_t numEntries)
{
    m_GammaLut = gammaLut;
    m_GammaLutSize = numEntries - 1;
}
inline void SmoothLed::setDitherMask(uint8_t ditherMask)
{
    m_DitherMask = ditherMask;
}
inline void SmoothLed::beginSecondary()
{
    begin(PA7, SPI0_PA3, TCB1, LUT1);
}
inline void SmoothLed::Interpolator::setTarget(uint16_t target)
{
    step = target - value;
}
inline void SmoothLed::Interpolator::stop()
{
    step = 0; 
    //value = 0;
}
inline SmoothLed::Interpolator& SmoothLed::getInterpolator(uint16_t index)
{
    return m_Interpolators[index];
}
inline uint8_t SmoothLed::getRange() const
{
    return m_GammaLutSize;
}
inline uint16_t SmoothLed::Interpolator::expandRange(uint8_t value, uint8_t maxvalue)
{
    // scale [0, 255] to [0, (maxvalue << 8) - 1]
    uint16_t scaled = value * maxvalue;
    return scaled + (scaled >> 8);
}
inline void setEventOutputCclLut(uint8_t evout, uint8_t lut)
{
    EVSYS_ASYNCCH3 = EVSYS_ASYNCCH3_CCL_LUT0_gc + lut;
    (&EVSYS_ASYNCUSER8)[evout] = EVSYS_ASYNCUSER8_ASYNCCH3_gc;
    PORTMUX.CTRLA |= PORTMUX_EVOUT0_bm << evout;
}
inline int NsToCycles(int ns) 
{
    return (ns * F_CPU + 500L*1000*1000) / (1000L*1000*1000);
}

#if !defined(CCL_INSEL2_TCB1_gc)
#define	CCL_INSEL2_TCB1_gc	(0x0D) // TODO: don't think this is actually valid...
#endif

inline void SmoothLed::begin(OutputPin outpin, ClockPin sckpin, 
    volatile TCB_t& tcb, Lut lut, int highPulseNs, int lowPulseNs)
{
    register8_t& TCBEV = &tcb == &TCB0 ? EVSYS_ASYNCUSER0 : EVSYS_ASYNCUSER11;
    bool usart = false;
    switch (sckpin)
    {
    case USART0_PA3:
        usart = true;
        PORTMUX.CTRLB |= PORTMUX_USART0_ALTERNATE_gc;
        //PORTA.PIN3CTRL |= PORT_INVEN_bm;
    case SPI0_PA3:
        EVSYS_ASYNCCH0 = EVSYS_ASYNCCH0_PORTA_PIN3_gc;
        TCBEV = EVSYS_ASYNCUSER0_ASYNCCH0_gc;
        break;
    case USART0_PB1:
        usart = true;
        EVSYS_ASYNCCH1 = EVSYS_ASYNCCH1_PORTB_PIN1_gc;
        TCBEV = EVSYS_ASYNCUSER0_ASYNCCH1_gc;
        //PORTB.PIN1CTRL |= PORT_INVEN_bm;
        break;
#ifdef PORTC
    case SPI0_PC0:
        EVSYS_ASYNCCH2 = EVSYS_ASYNCCH2_PORTC_PIN0_gc;
        TCBEV = EVSYS_ASYNCUSER0_ASYNCCH2_gc;
        PORTMUX.CTRLB |= PORTMUX_SPI0_ALTERNATE_gc;
        break;
#endif
    default: break;
    }
    uint8_t highCycles = max(NsToCycles(highPulseNs), 4);
    if (usart)
    {
        m_Spi = 0;
    }
    else if (highCycles <= 4)
    {
        highCycles = 4;
        m_Spi = SPI_CLK2X_bm | SPI_PRESC_DIV16_gc | SPI_MASTER_bm | SPI_ENABLE_bm;
    }
    else if (highCycles <= 8)
    {
        highCycles = 8;
        m_Spi = SPI_PRESC_DIV16_gc | SPI_MASTER_bm | SPI_ENABLE_bm;
    }
    else
    {
        highCycles = 16;
        m_Spi = SPI_CLK2X_bm | SPI_PRESC_DIV64_gc | SPI_MASTER_bm | SPI_ENABLE_bm;
    }
    m_HighCycles = highCycles;
    tcb.EVCTRL = TCB_CAPTEI_bm;
    tcb.CCMP = TCB0.CNT = max(NsToCycles(lowPulseNs) - 1, 1) + max(highCycles - 5, 0);
    tcb.CTRLB = TCB_CNTMODE_SINGLE_gc | TCB_ASYNC_bm;
    tcb.CTRLA = TCB_CLKSEL_CLKDIV1_gc | TCB_ENABLE_bm;

    if (outpin == PA4) lut = LUT0;
    else if (outpin == PA7) lut = LUT1;
#ifdef PORTC
    else if (outpin == PB4) lut = LUT0;
    else if (outpin == PC1) lut = LUT1;
#endif

    register8_t* LUT = lut == LUT0 ? &CCL.LUT0CTRLA : &CCL.LUT1CTRLA;
    LUT[3] = 0b01010001;
    LUT[2] = &tcb == &TCB0 ? CCL_INSEL2_TCB0_gc : CCL_INSEL2_TCB1_gc;
    LUT[1] = usart ? CCL_INSEL1_USART0_gc | CCL_INSEL0_USART0_gc
                   : CCL_INSEL1_SPI0_gc | CCL_INSEL0_SPI0_gc;
    LUT[0] = CCL_ENABLE_bm;

    switch (outpin)
    {
    case PA4: CCL.LUT0CTRLA |= CCL_OUTEN_bm; break;
    case PA7: CCL.LUT1CTRLA |= CCL_OUTEN_bm; break;
    case PA2: setEventOutputCclLut(0, lut); break;
    case PB2: setEventOutputCclLut(1, lut); break;
#ifdef PORTC
    case PB4: PORTMUX.CTRLA |= PORTMUX_LUT0_bm; CCL.LUT0CTRLA |= CCL_OUTEN_bm; break;
    case PC1: PORTMUX.CTRLA |= PORTMUX_LUT1_bm; CCL.LUT1CTRLA |= CCL_OUTEN_bm; break;
    case PC2: setEventOutputCclLut(2, lut); break;
#endif
    default: break;
    }
}

#endif
