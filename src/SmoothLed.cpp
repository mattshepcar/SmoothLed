#include "SmoothLed.h"
#include "SmoothLedMultiply.h"

// Values in this table are inverted and the CCL LUT will flip them back.
const uint16_t SmoothLed::Gamma25[Gamma25Size] =
{
    0xff00, 0xfef4, 0xfebb, 0xfe42, 0xfd79, 0xfc56, 0xfacc, 0xf8d2,
    0xf65f, 0xf36a, 0xefed, 0xebde, 0xe738, 0xe1f3, 0xdc0a, 0xd575,
    0xce2f, 0xc632, 0xbd78, 0xb3fc, 0xa9b8, 0x9ea8, 0x92c6, 0x860e,
    0x787a, 0x6a06, 0x5aad, 0x4a6a, 0x393a, 0x2718, 0x13ff, 0xffeb,
};

SmoothLed::SmoothLed(Interpolator* interpolators, uint16_t numInterpolators,
    uint8_t ditherMask, const uint16_t* gammaLut, uint8_t gammaLutSize)
{
    m_Interpolators = interpolators;
    m_NumInterpolators = numInterpolators;
    m_Time = 0;
    setGammaLut(gammaLut, gammaLutSize);
    m_DitherMask = ditherMask;
    uint8_t dither = 0;
    for (uint16_t i = 0; i < numInterpolators; ++i, dither += 26)
        m_Interpolators[i].dither = dither;
}

extern "C" void SmoothLedUpdate8cpb(
    uint16_t count,
    SmoothLed::Interpolator * interpolators,
    register8_t * outport,
    uint8_t dt,
    uint8_t ditherMask,
    uint16_t maxValue,
    const uint16_t * gammaLut);

void SmoothLed::update(uint16_t deltaTime)
{
    uint8_t lastT = highByte(m_Time);
    m_Time += deltaTime;
    uint8_t dt = highByte(m_Time) - lastT;

    register8_t* outport;
    register8_t* statusport;
    if (m_Spi)
    {
        SPI0.CTRLA = m_Spi;
        SPI0.CTRLB = SPI_SSD_bm | SPI_MODE_0_gc | SPI_BUFEN_bm;
        outport = &SPI0.DATA;
        statusport = &SPI0.INTFLAGS;
    }
    else
    {
        USART0.BAUD = m_HighCycles * 64;
        USART0.CTRLC = USART_CMODE_MSPI_gc | USART_UCPHA_bm;
        USART0.CTRLB = USART_TXEN_bm;
        outport = &USART0.TXDATAL;
        statusport = &USART0.STATUS;
    }
    Interpolator* i = m_Interpolators;
    uint16_t count = m_NumInterpolators;
    uint8_t ditherMask = m_DitherMask;
    uint8_t maxvalue = m_GammaLutSize - 1;
    const uint16_t* gammaLut = m_GammaLut;
    CCL.CTRLA |= CCL_ENABLE_bm;
    if (m_HighCycles <= 4)
    {
        // cycle counted loop for maximum throughput
        SmoothLedUpdate8cpb(count, i, outport, dt, ditherMask, maxvalue * 256 - 1, gammaLut);
    }
    else
    {
        do
        {
            while (!(*statusport & USART_DREIF_bm)) {}
            *outport = i++->update(dt, gammaLut, maxvalue, ditherMask);
        } while (--count);
    }
    delayMicroseconds(10);
    CCL.CTRLA &= ~CCL_ENABLE_bm;
}

uint8_t SmoothLed::Interpolator::update(uint8_t dt, const uint16_t* lut, uint16_t maxvalue, uint8_t ditherMask)
{    
    value = fmac(value, step, dt); //value += (step * dt) >> 7;
    if (value < 0)
        value = 0;
    if (highByte(maxvalue) <= highByte(value))
        value = maxvalue;
    lut += highByte(value);
    uint16_t corrected = lerp(lut[0], lut[1], lowByte(value)) + dither;
    dither = lowByte(corrected) & ditherMask;
    return highByte(corrected);
}
void SmoothLed::Interpolator::setTarget(uint8_t target, uint8_t range)
{
    setTarget(expandRange(target, range));
}
void SmoothLed::Interpolator::setTarget(uint8_t target, uint8_t range, uint16_t fraction)
{
    setTarget(expandRange(target, range), fraction);
}
uint16_t SmoothLed::expandRange(uint8_t value) const
{
    return Interpolator::expandRange(value, m_GammaLutSize);
}
void SmoothLed::setTarget(uint16_t index, uint8_t target)
{
    m_Interpolators[index].setTarget(target, m_GammaLutSize);
}
void SmoothLed::setTarget(uint16_t index, uint8_t target, uint16_t fraction)
{
    m_Interpolators[index].setTarget(target, m_GammaLutSize, fraction);
}
void SmoothLed::setTarget(uint16_t index, const uint8_t* target, uint16_t count)
{
    Interpolator* i = &m_Interpolators[index];
    uint8_t range = m_GammaLutSize;
    do {
        i++->setTarget(*target++, range);
    } while (--count);
}
void SmoothLed::setTarget(uint16_t index, const uint8_t* target, uint16_t count, uint16_t fraction)
{
    Interpolator* i = &m_Interpolators[index];
    uint8_t range = m_GammaLutSize;
    do {
        i++->setTarget(*target++, range, fraction);
    } while (--count);
}

void SmoothLed::Interpolator::setTarget(uint16_t target, uint16_t fraction)
{
    value = target;
    step = 0;
    //step = fmul(target - value, fraction);
}
