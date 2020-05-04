// SmoothLED for tinyAVR-0/1 series
// Matt Shepcar 25/04/2020

#pragma once

#include "SmoothLedCcl.h"

class SmoothLed : public SmoothLedCcl
{
public:
    struct Interpolator;

    enum DitherBits { DITHER0 = 0,
        DITHER1 = 0x80, DITHER2 = 0xC0, DITHER3 = 0xE0, DITHER4 = 0xF0,
        DITHER5 = 0xF8, DITHER6 = 0xFC, DITHER7 = 0xFE, DITHER8 = 0xFF };
    static const uint8_t Gamma25Size = 32;
    static const uint16_t Gamma25[Gamma25Size];

    SmoothLed(Interpolator* interpolators, uint16_t numInterpolators,
        DitherBits ditherMask = DITHER5,
        const uint16_t* gammaLut = Gamma25, uint8_t gammaLutSize = Gamma25Size);

    void update(uint8_t* outputBuffer);
    void update();
    void updateSpi();
    void updateUsart();

    void set(uint16_t index, uint8_t value);
    void set(uint16_t index, const uint8_t* values, uint16_t count);
    void clear(uint8_t value = 0);
    void clear(uint16_t index, uint16_t count, uint8_t value = 0);

    void beginFade(uint16_t numFrames);
    void setFadeRate(uint16_t speed);
    void setFadePosition(uint16_t speed);
    uint16_t getFadePosition() const;
    uint16_t getFadeRate() const;
    bool isFading() const;

    void setFadeTarget(uint16_t index, uint8_t target);
    void setFadeTarget(uint16_t index, uint8_t target, uint16_t fraction); // Q1.15 fraction (0x8000 = 1.0)
    void setFadeTarget(uint16_t index, const uint8_t* target, uint16_t count);
    void setFadeTarget(uint16_t index, const uint8_t* target, uint16_t count, uint16_t fraction);
    void clearFadeTarget();
    void clearFadeTarget(uint16_t index, uint16_t count);

    void setGammaLut(const uint16_t* gammaLut, uint8_t numEntries);
    void setDitherMask(DitherBits ditherMask);

    uint8_t         updateTime();

    Interpolator*   getInterpolators();
    Interpolator&   getInterpolator(uint16_t index);
    uint16_t        getNumInterpolators() const;
    const uint16_t* getGammaLut() const;
    uint8_t         getRange() const;
    uint8_t         getDitherMask() const;
    uint16_t        getMaxValue() const;

    uint16_t        expandRange(uint8_t value) const; // convert 8 bit colour to 16 bits
    static uint16_t expandRange(uint8_t value, uint8_t range);

    struct Interpolator
    {
        int16_t step;
        int16_t value;
        uint8_t dither;

        void set(uint8_t value, uint8_t range);
        void set(uint16_t value);

        void setFadeTarget(uint16_t target);
        void setFadeTarget(uint16_t target, uint16_t fraction);
        void setFadeTarget(uint8_t target, uint8_t range);
        void setFadeTarget(uint8_t target, uint8_t range, uint16_t fraction);
        void stop();

        uint8_t update(uint8_t dt, const uint16_t* gammaLut, uint16_t maxValue, uint8_t ditherMask);
    };

private:
    void update(register8_t& data, register8_t& status);

    Interpolator*   m_Interpolators;
    const uint16_t* m_GammaLut;
    uint16_t        m_NumInterpolators;
    uint16_t        m_Time;
    uint16_t        m_DeltaTime;
    uint8_t         m_GammaLutSize;
    uint8_t         m_DitherMask;
};

inline void SmoothLed::Interpolator::setFadeTarget(uint16_t target)
{
    step = target - value;
}
inline void SmoothLed::Interpolator::stop()
{
    step = 0; 
}
inline SmoothLed::Interpolator& SmoothLed::getInterpolator(uint16_t index)
{
    return m_Interpolators[index];
}
inline SmoothLed::Interpolator* SmoothLed::getInterpolators()
{
    return m_Interpolators;
}
inline uint16_t SmoothLed::getNumInterpolators() const
{
    return m_NumInterpolators;
}
inline void SmoothLed::setGammaLut(const uint16_t* gammaLut, uint8_t numEntries)
{
    m_GammaLut = gammaLut;
    m_GammaLutSize = numEntries - 1;
}
inline const uint16_t* SmoothLed::getGammaLut() const
{
    return m_GammaLut;
}
inline uint8_t SmoothLed::getRange() const
{
    return m_GammaLutSize;
}
inline void SmoothLed::setDitherMask(DitherBits ditherMask)
{
    m_DitherMask = ditherMask;
}
inline uint8_t SmoothLed::getDitherMask() const
{
    return m_DitherMask;
}
inline uint16_t SmoothLed::expandRange(uint8_t value, uint8_t range)
{
    // scale [0, 255] to [0, (range << 8) - 1]
    uint16_t scaled = value * range;
    return scaled + (scaled >> 8);
}
inline uint16_t SmoothLed::getMaxValue() const
{
    return m_GammaLutSize * 256 - 1;
}
inline void SmoothLed::Interpolator::set(uint8_t newvalue, uint8_t range)
{
    set(expandRange(newvalue, range));
}
inline void SmoothLed::Interpolator::set(uint16_t newvalue)
{
    value = newvalue;
    step = 0;
}
inline bool SmoothLed::isFading() const
{
    return highByte(m_Time) < 0x80;
}
inline uint16_t SmoothLed::getFadePosition() const
{
    return m_Time;
}
inline uint16_t SmoothLed::getFadeRate() const
{
    return m_DeltaTime;
}
inline void SmoothLed::clearFadeTarget()
{
    clearFadeTarget(0, m_NumInterpolators);
}
