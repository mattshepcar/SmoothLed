#include "SmoothLed.h"
#include "SmoothLedMultiply.h"

#ifndef SMOOTHLED_ASM_UPDATE
#define SMOOTHLED_ASM_UPDATE 1
#endif

// Values in this table are inverted and the CCL LUT will flip them back.
const uint16_t SmoothLed::Gamma25[Gamma25Size] =
{
    0xff00, 0xfef4, 0xfebb, 0xfe42, 0xfd79, 0xfc56, 0xfacc, 0xf8d2,
    0xf65f, 0xf36a, 0xefed, 0xebde, 0xe738, 0xe1f3, 0xdc0a, 0xd575,
    0xce2f, 0xc632, 0xbd78, 0xb3fc, 0xa9b8, 0x9ea8, 0x92c6, 0x860e,
    0x787a, 0x6a06, 0x5aad, 0x4a6a, 0x393a, 0x2718, 0x13ff, 0xffeb,
};

SmoothLed::SmoothLed(Interpolator* interpolators, uint16_t numInterpolators,
    DitherBits ditherMask, const uint16_t* gammaLut, uint8_t gammaLutSize)
{
    m_Interpolators = interpolators;
    m_NumInterpolators = numInterpolators;
    m_Time = 0;
    setGammaLut(gammaLut, gammaLutSize);
    setDitherMask(ditherMask);
    uint8_t dither = 0;
    for (uint16_t i = 0; i < numInterpolators; ++i, dither += 26)
        interpolators[i].dither = dither;
}

void SmoothLed::update(uint16_t deltaTime)
{
    isSpi() ? updateSpi(deltaTime) : updateUsart(deltaTime);
}
void SmoothLed::updateSpi(uint16_t deltaTime)
{
    beginTransactionSpi();
    update(deltaTime, SPI0.DATA, SPI0.INTFLAGS);
    endTransactionSpi();
}
void SmoothLed::updateUsart(uint16_t deltaTime)
{
    beginTransactionUsart();
    update(deltaTime, USART0.TXDATAL, USART0.STATUS);
    endTransactionUsart();
}
uint8_t SmoothLed::updateTime(uint16_t deltaTime)
{
    uint8_t lastT = highByte(m_Time);
    m_Time += deltaTime;
    return highByte(m_Time) - lastT;
}

extern "C" void SmoothLedUpdate8cpb(
    uint16_t count, SmoothLed::Interpolator * interpolators,
    register8_t & outport,
    uint8_t dt, uint16_t ditherMask, uint16_t maxValue,
    const uint16_t * gammaLut, register8_t & statusport);

void SmoothLed::update(uint16_t deltaTime, register8_t& data, register8_t& status)
{
    uint8_t dt = updateTime(deltaTime);
    Interpolator* i = getInterpolators();
    uint16_t count = getNumInterpolators();
    uint8_t ditherMask = getDitherMask();
    uint16_t maxvalue = getMaxValue();
    const uint16_t* gammaLut = getGammaLut();
#if SMOOTHLED_ASM_UPDATE
    // 8 cycle per bit loop for maximum throughput at 8MHz
    SmoothLedUpdate8cpb(count, i, data, dt, ditherMask, maxvalue, gammaLut, status);
#else
    do
    {
        uint8_t value = i++->update(dt, gammaLut, maxvalue, ditherMask);
        while ((status & USART_DREIF_bm) == 0) {}
        data = value;
    } while (--count);
#endif
}

uint8_t SmoothLed::Interpolator::update(uint8_t dt, const uint16_t* lut, uint16_t maxvalue, uint8_t ditherMask)
{    
    value = fmac(value, step, dt); //value += (step * dt) >> 7;
    if (highByte(maxvalue) < highByte(value))
        value = value < 0 ? 0 : maxvalue;
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
    return expandRange(value, m_GammaLutSize);
}
void SmoothLed::setTarget(uint16_t index, uint8_t target)
{
    m_Interpolators[index].setTarget(target, m_GammaLutSize);
}
void SmoothLed::setTarget(uint16_t index, uint8_t target, uint16_t fraction)
{
    m_Interpolators[index].setTarget(target, m_GammaLutSize, fraction);
}
void SmoothLed::set(uint16_t index, uint8_t value)
{
    m_Interpolators[index].set(expandRange(value));
}
void SmoothLed::clear(uint8_t value)
{
    clear(0, m_NumInterpolators, value);
}
void SmoothLed::clear(uint16_t index, uint16_t count, uint8_t value)
{
    Interpolator* i = &m_Interpolators[index];
    uint16_t fullvalue = expandRange(value);
    do {
        i++->set(fullvalue);
    } while (--count);
}
void SmoothLed::set(uint16_t index, const uint8_t* values, uint16_t count)
{
    Interpolator* i = &m_Interpolators[index];
    uint8_t range = m_GammaLutSize;
    do {
        i++->set(*values++, range);
    } while (--count);
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
    step = fmul(target - value, fraction);
}
