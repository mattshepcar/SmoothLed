// SmoothLED for tinyAVR-0/1 series
// Matt Shepcar 25/04/2020

#ifndef _SmoothLedCcl_h
#define _SmoothLedCcl_h

#include "arduino.h"

class SmoothLedCcl
{
public:
    enum OutputPin { PA2, PA4, PA7, PB2, PB4, PC1, PC2 };
    enum ClockPin { USART0_PA3, SPI0_PA3, USART0_PB1, SPI0_PC0 };
    enum Lut { LUT0, LUT1 };

    void begin(OutputPin outpin = PA4,
        ClockPin sckpin = USART0_PB1,
        int lowPulseNs = 200, int highPulseNs = 500,
        volatile TCB_t& tcb = TCB0, Lut lut = LUT0);
    void beginSecondary(OutputPin outpin = PA7,
        ClockPin sckpin = SPI0_PA3,
        int lowPulseNs = 200, int highPulseNs = 500,
        volatile TCB_t& tcb = TCB1, Lut lut = LUT1);
    
    void beginTransaction();
    void write(uint8_t value);
    void endTransaction();

    // if you know you're using SPI or USART these can be smaller:
    void beginTransactionSpi();
    void writeSpi(uint8_t value);
    void endTransactionSpi();
    void beginTransactionUsart();
    void writeUsart(uint8_t value);
    void endTransactionUsart();

    bool isSpi() const;

private:
    static int nsToCycles(int ns);
    static void setEventOutputCclLut(uint8_t evout, uint8_t lut);

    uint8_t m_Spi;
    uint8_t m_HighCycles;
};

inline void SmoothLedCcl::beginSecondary(OutputPin outpin,
    ClockPin sckpin, int lowPulseNs, int highPulseNs,
    volatile TCB_t& tcb, Lut lut)
{
    begin(outpin, sckpin, lowPulseNs, highPulseNs, tcb, lut);
}

inline void SmoothLedCcl::setEventOutputCclLut(uint8_t evout, uint8_t lut)
{
    EVSYS_ASYNCCH3 = EVSYS_ASYNCCH3_CCL_LUT0_gc + lut;
    (&EVSYS_ASYNCUSER8)[evout] = EVSYS_ASYNCUSER8_ASYNCCH3_gc;
    PORTMUX.CTRLA |= PORTMUX_EVOUT0_bm << evout;
}
inline int SmoothLedCcl::nsToCycles(int ns)
{
    return (ns * (F_CPU / 1000000) + 500) / 1000;
}
inline bool SmoothLedCcl::isSpi() const
{
    return m_Spi != 0;
}

#if !defined(CCL_INSEL2_TCB1_gc)
#define	CCL_INSEL2_TCB1_gc	(0x0D) // TODO: don't think this is actually valid...
#endif

inline void SmoothLedCcl::begin(OutputPin outpin, ClockPin sckpin, 
    int lowPulseNs, int highPulseNs, volatile TCB_t& tcb, Lut lut)
{
    register8_t& TCBEV = &tcb == &TCB0 ? EVSYS_ASYNCUSER0 : EVSYS_ASYNCUSER11;
    bool usart = false;
    switch (sckpin)
    {
    case USART0_PA3:
        usart = true;
        PORTMUX.CTRLB |= PORTMUX_USART0_ALTERNATE_gc;
    case SPI0_PA3:
        EVSYS_ASYNCCH0 = EVSYS_ASYNCCH0_PORTA_PIN3_gc;
        TCBEV = EVSYS_ASYNCUSER0_ASYNCCH0_gc;
        break;
    case USART0_PB1:
        usart = true;
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
    default: break;
    }
    uint8_t highCycles = max(nsToCycles(highPulseNs), 4);
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
    tcb.CCMP = TCB0.CNT = max(nsToCycles(lowPulseNs) - 1, 1) + max(highCycles - 5, 0);
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

inline void SmoothLedCcl::beginTransaction()
{
    m_Spi ? beginTransactionSpi() : beginTransactionUsart();
}
inline void SmoothLedCcl::endTransaction()
{
    m_Spi ? endTransactionSpi() : endTransactionUsart();
}
inline void SmoothLedCcl::write(uint8_t value)
{
    m_Spi ? writeSpi(value) : writeUsart(value);
}
inline void SmoothLedCcl::beginTransactionSpi()
{
    SPI0.CTRLB = SPI_SSD_bm | SPI_MODE_0_gc | SPI_BUFEN_bm;
    SPI0.CTRLA = m_Spi;
    SPI0.INTFLAGS = SPI_TXCIF_bm;
    CCL.CTRLA |= CCL_ENABLE_bm;
}
inline void SmoothLedCcl::writeSpi(uint8_t value)
{
    while ((SPI0.INTFLAGS & SPI_DREIF_bm) == 0) {}
    SPI0.DATA = value;
}
inline void SmoothLedCcl::endTransactionSpi()
{
    while ((SPI0.INTFLAGS & SPI_TXCIF_bm) == 0) {}
    CCL.CTRLA &= ~CCL_ENABLE_bm;
}
inline void SmoothLedCcl::beginTransactionUsart()
{
    USART0.BAUD = m_HighCycles * 64;
    USART0.CTRLC = USART_CMODE_MSPI_gc | USART_UCPHA_bm;
    USART0.CTRLB = USART_TXEN_bm;
    USART0.STATUS = USART_TXCIF_bm;
    CCL.CTRLA |= CCL_ENABLE_bm;
}
inline void SmoothLedCcl::writeUsart(uint8_t value)
{
    while ((USART0.STATUS & USART_DREIF_bm) == 0) {}
    USART0.TXDATAL = value;
}
inline void SmoothLedCcl::endTransactionUsart()
{
    while ((USART0.STATUS & USART_TXCIF_bm) == 0) {}
    CCL.CTRLA &= ~CCL_ENABLE_bm;
}

#endif // _SmoothLedCcl_h
