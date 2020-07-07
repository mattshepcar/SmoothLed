// SmoothLED for tinyAVR-0/1 series
// Matt Shepcar 25/04/2020

#pragma once

#include <Arduino.h>

class SmoothLedCcl
{
public:
    // output pins tied to a specific CCL LUT
    enum OutputPinLut { PA4_LUT0, PB4_LUT0, PA7_LUT1, PC1_LUT1 };
    // output pins using user specified LUT and async event channel
    enum OutputPinEvent { PA2, PB2, PC2 };
    enum EventChannel { ASYNCCH0, ASYNCCH1, ASYNCCH2, ASYNCCH3 };
    enum Lut { LUT0, LUT1 };
    // clock pins are tied to a specific usart/spi peripheral and async event channel
    enum ClockSetting { PA3_USART0_ASYNCCH0, PA3_SPI0_ASYNCCH0,
                        PB1_USART0_ASYNCCH1, PC0_SPI0_ASYNCCH2 };

    void begin(OutputPinLut outpin = PA4_LUT0, ClockSetting sck = PB1_USART0_ASYNCCH1,
        volatile TCB_t& tcb = TCB0, int lowPulseNs = 200, int highPulseNs = 600);
    void begin(OutputPinEvent outpin, ClockSetting sck = PA3_USART0_ASYNCCH0,
        volatile TCB_t& tcb = TCB0, Lut lut = LUT0, EventChannel channel = ASYNCCH3, 
        int lowPulseNs = 200, int highPulseNs = 600);

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

    // internal peripheral setup used by begin:
    void beginTimer(ClockSetting sck, volatile TCB_t& tcb, int lowPulseNs, int highPulseNs);
    void beginCclLut(Lut lut, volatile TCB_t& tcb, bool enable = false);
    static void beginEvent(OutputPinEvent outpin, Lut lut, EventChannel channel);
    static void enableOutput(OutputPinLut outpin);
    static void disableOutput(OutputPinLut outpin);
    static void enableOutput(OutputPinEvent outpin);
    static void disableOutput(OutputPinEvent outpin);

private:
    static int nsToCycles(int ns);

    uint8_t m_Spi;
    uint8_t m_HighCycles;
};

inline void SmoothLedCcl::begin(OutputPinLut outpin, ClockSetting sck,
    volatile TCB_t& tcb, int lowPulseNs, int highPulseNs)
{
    beginTimer(sck, tcb, lowPulseNs, highPulseNs);
    beginCclLut(outpin == PA4_LUT0 || outpin == PB4_LUT0 ? LUT0 : LUT1, tcb);
    enableOutput(outpin);
}

inline void SmoothLedCcl::begin(OutputPinEvent outpin, ClockSetting sck, volatile TCB_t& tcb,
    Lut lut, EventChannel channel, int lowPulseNs, int highPulseNs)
{
    beginTimer(sck, tcb, lowPulseNs, highPulseNs);
    beginCclLut(lut, tcb, true);
    beginEvent(outpin, lut, channel);
    enableOutput(outpin);
}

inline void SmoothLedCcl::beginTimer(ClockSetting sck, volatile TCB_t& tcb,
    int lowPulseNs, int highPulseNs)
{
#ifdef TCB1
    register8_t& TCBEV = &tcb == &TCB0 ? EVSYS_ASYNCUSER0 : EVSYS_ASYNCUSER11;
#else
    register8_t& TCBEV = EVSYS_ASYNCUSER0;
#endif
    switch (sck)
    {
    case PA3_USART0_ASYNCCH0:
        PORTMUX.CTRLB |= PORTMUX_USART0_ALTERNATE_gc;
        // falls through
    case PA3_SPI0_ASYNCCH0:
        EVSYS_ASYNCCH0 = EVSYS_ASYNCCH0_PORTA_PIN3_gc;
        TCBEV = EVSYS_ASYNCUSER0_ASYNCCH0_gc;
        VPORTA.DIR |= _BV(3);
        break;
    case PB1_USART0_ASYNCCH1:
        EVSYS_ASYNCCH1 = EVSYS_ASYNCCH1_PORTB_PIN1_gc;
        TCBEV = EVSYS_ASYNCUSER0_ASYNCCH1_gc;
        VPORTB.DIR |= _BV(1);
        break;
    case PC0_SPI0_ASYNCCH2:
#ifdef VPORTC
        PORTMUX.CTRLB |= PORTMUX_SPI0_ALTERNATE_gc;
        EVSYS_ASYNCCH2 = EVSYS_ASYNCCH2_PORTC_PIN0_gc;
        TCBEV = EVSYS_ASYNCUSER0_ASYNCCH2_gc;
        VPORTC.DIR |= _BV(0);
#endif
        break;
    }
    uint8_t highCycles = max(nsToCycles(highPulseNs), 4);
    if (sck == PA3_USART0_ASYNCCH0 || sck == PB1_USART0_ASYNCCH1)
    {
        m_HighCycles = highCycles;
        m_Spi = 0;
    }
    else if (highCycles <= 4)
    {
        m_HighCycles = 4;
        m_Spi = SPI_CLK2X_bm | SPI_PRESC_DIV16_gc | SPI_MASTER_bm | SPI_ENABLE_bm;
    }
    else if (highCycles <= 8)
    {
        m_HighCycles = 8;
        m_Spi = SPI_PRESC_DIV16_gc | SPI_MASTER_bm | SPI_ENABLE_bm;
    }
    else
    {
        m_HighCycles = 16;
        m_Spi = SPI_CLK2X_bm | SPI_PRESC_DIV64_gc | SPI_MASTER_bm | SPI_ENABLE_bm;
    }
    tcb.EVCTRL = TCB_CAPTEI_bm;
    tcb.CCMP = tcb.CNT = max(nsToCycles(lowPulseNs) - 1, 1) + max(m_HighCycles - 5, 0);
    tcb.CTRLB = TCB_CNTMODE_SINGLE_gc | TCB_ASYNC_bm;
    tcb.CTRLA = TCB_CLKSEL_CLKDIV1_gc | TCB_ENABLE_bm;
}

#if !defined(CCL_INSEL2_TCB1_gc)
#define	CCL_INSEL2_TCB1_gc	(0x0D) // TODO: don't think this is actually valid...
#endif

inline void SmoothLedCcl::beginCclLut(Lut lut, volatile TCB_t& tcb, bool enable)
{
    register8_t* LUT = lut == LUT0 ? &CCL.LUT0CTRLA : &CCL.LUT1CTRLA;
    LUT[3] = 0b01010001; // !SCK && (!MOSI || TCB)
    LUT[2] = &tcb == &TCB0 ? CCL_INSEL2_TCB0_gc : CCL_INSEL2_TCB1_gc;
    LUT[1] = m_Spi ? CCL_INSEL1_SPI0_gc | CCL_INSEL0_SPI0_gc
                   : CCL_INSEL1_USART0_gc | CCL_INSEL0_USART0_gc;
    if (enable)
        LUT[0] = CCL_ENABLE_bm;
}

inline void SmoothLedCcl::beginEvent(OutputPinEvent outpin, Lut lut, EventChannel channel)
{
    (&EVSYS_ASYNCCH0)[channel] = EVSYS_ASYNCCH0_CCL_LUT0_gc + lut;
    (&EVSYS_ASYNCUSER8)[outpin] = EVSYS_ASYNCUSER8_ASYNCCH0_gc + channel;
}

inline void SmoothLedCcl::enableOutput(OutputPinLut outpin)
{
    switch (outpin)
    {
    case PA4_LUT0: 
        VPORTA.DIR |= _BV(4); 
        CCL.LUT0CTRLA = CCL_ENABLE_bm | CCL_OUTEN_bm; 
        break;
    case PA7_LUT1: 
        VPORTA.DIR |= _BV(7); 
        CCL.LUT1CTRLA = CCL_ENABLE_bm | CCL_OUTEN_bm; 
        break;
    case PB4_LUT0: 
        VPORTB.DIR |= _BV(4); 
        PORTMUX.CTRLA |= PORTMUX_LUT0_bm; 
        CCL.LUT0CTRLA = CCL_ENABLE_bm | CCL_OUTEN_bm; 
        break;
    case PC1_LUT1: 
#ifdef VPORTC
        VPORTC.DIR |= _BV(1); 
        PORTMUX.CTRLA |= PORTMUX_LUT1_bm; 
        CCL.LUT1CTRLA = CCL_ENABLE_bm | CCL_OUTEN_bm; 
#endif
        break;
    }
}
inline void SmoothLedCcl::disableOutput(OutputPinLut outpin)
{
    switch (outpin)
    {
    case PA4_LUT0: CCL.LUT0CTRLA = 0; break;
    case PA7_LUT1: CCL.LUT1CTRLA = 0; break;
    case PB4_LUT0: CCL.LUT0CTRLA = 0; PORTMUX.CTRLA &= ~PORTMUX_LUT0_bm; break;
    case PC1_LUT1: CCL.LUT1CTRLA = 0; PORTMUX.CTRLA &= ~PORTMUX_LUT1_bm; break;
    }
}
inline void SmoothLedCcl::enableOutput(OutputPinEvent outpin)
{
    PORTMUX.CTRLA |= (PORTMUX_EVOUT0_bm << outpin);
    switch (outpin)
    {
    case PA2: VPORTA.DIR |= _BV(2); break;
    case PB2: VPORTB.DIR |= _BV(2); break;
    case PC2: 
#ifdef VPORTC
        VPORTC.DIR |= _BV(2); 
#endif
        break;
    }
}
inline void SmoothLedCcl::disableOutput(OutputPinEvent outpin)
{
    PORTMUX.CTRLA &= ~(PORTMUX_EVOUT0_bm << outpin);
}
inline void SmoothLedCcl::beginTransaction()
{
    if (m_Spi)
        beginTransactionSpi();
    else
        beginTransactionUsart();
}
inline void SmoothLedCcl::endTransaction()
{
    if (m_Spi)
        endTransactionSpi();
    else
        endTransactionUsart();
}
inline void SmoothLedCcl::write(uint8_t value)
{
    if (m_Spi)
        writeSpi(value);
    else
        writeUsart(value);
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
inline int SmoothLedCcl::nsToCycles(int ns)
{
    return (ns * (F_CPU / 1000000) + 500) / 1000;
}
inline bool SmoothLedCcl::isSpi() const
{
    return m_Spi != 0;
}
