#pragma once

#include "SmoothLed.h"

template<int Size, int NumBufferedFrames>
class SmoothLedBuffer
{
public:
    SmoothLedBuffer();

    void     reset();
    uint8_t* getWriteBuffer(uint8_t time);
    void     update(uint8_t time, SmoothLed& leds, uint16_t startIndex = 0);
    uint8_t  getUsedEntries() const;

private:
    uint8_t m_ReadIndex;
    uint8_t m_WriteIndex;
    uint8_t m_UsedEntries;
    uint8_t m_TimeToRun;
    uint8_t m_Time[NumBufferedFrames];
    uint8_t m_Data[NumBufferedFrames][Size];
};

template<int Size, int NumBufferedFrames>
inline SmoothLedBuffer<Size, NumBufferedFrames>::SmoothLedBuffer()
{
    reset();
}
template<int Size, int NumBufferedFrames>
inline void SmoothLedBuffer<Size, NumBufferedFrames>::reset()
{
    m_ReadIndex = 0;
    m_WriteIndex = -1;
    m_UsedEntries = 0;
    m_TimeToRun = 0;
}
template<int Size, int NumBufferedFrames>
uint8_t* SmoothLedBuffer<Size, NumBufferedFrames>::getWriteBuffer(uint8_t time)
{
    if (m_UsedEntries == 0 || m_Time[m_WriteIndex] != time)
    {
        if (m_UsedEntries < NumBufferedFrames)
            ++m_UsedEntries;
        else if (++m_ReadIndex == NumBufferedFrames)
            m_ReadIndex = 0;
        if (++m_WriteIndex == NumBufferedFrames)
            m_WriteIndex = 0;
        m_Time[m_WriteIndex] = time;
    }
    return m_Data[m_WriteIndex];
}
template<int Size, int NumBufferedFrames>
void SmoothLedBuffer<Size, NumBufferedFrames>::update(uint8_t time, SmoothLed& leds, uint16_t startIndex)
{
    if (m_TimeToRun > 0 && --m_TimeToRun > 0)
        return;
    const uint8_t* data = nullptr;
    while (m_TimeToRun <= 0 && m_UsedEntries)
    {
        data = m_Data[m_ReadIndex];
        m_TimeToRun = m_Time[m_ReadIndex] - time;
        if (++m_ReadIndex == NumBufferedFrames) 
            m_ReadIndex = 0;
        --m_UsedEntries;
    }
    if (m_TimeToRun > 0)
        leds.setFadeTarget(startIndex, data, Size, uint16_t(0x8000) / m_TimeToRun);
    else
        leds.clearFadeTarget(startIndex, Size);
}
template<int Size, int NumBufferedFrames>
inline uint8_t SmoothLedBuffer<Size, NumBufferedFrames>::getUsedEntries() const
{
    return m_UsedEntries;
}
