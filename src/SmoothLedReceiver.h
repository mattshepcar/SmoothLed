#pragma once

#include "SmoothLedBuffer.h"

template<int PacketsPerFrame, int PacketSize, int NumBufferedFrames>
class SmoothLedReceiver
{
public:
    SmoothLedReceiver() : m_Leds(m_Interpolators, PacketsPerFrame * PacketSize) {}

    uint8_t  update(uint8_t minUpdatesPerFrame = 25, uint16_t maxPacketInterval = 250);
    uint8_t* receive(uint8_t frame, uint8_t packet, uint8_t idealFrameStart = 0x40);
    void     receive(uint8_t frame, uint8_t packet, const uint8_t* data, uint8_t idealFrameStart = 0x40);

    SmoothLed& getLeds() { return m_Leds; }

private:
    uint8_t m_Frame = 0;
    uint8_t m_UpdateCount = 0;
    uint8_t m_LastReceivedFrame = 0;
    uint8_t m_TimeSinceLastFrame = 0;
    int16_t m_LastError = 0;
    int16_t m_ErrorI = 0;

    typedef SmoothLedBuffer<PacketSize, NumBufferedFrames> Buffer;

    SmoothLed m_Leds;
    SmoothLed::Interpolator m_Interpolators[PacketSize * PacketsPerFrame];
    Buffer m_Buffer[PacketsPerFrame];
};

template<int PacketsPerFrame, int PacketSize, int NumBufferedFrames>
uint8_t SmoothLedReceiver<PacketsPerFrame, PacketSize, NumBufferedFrames>::update(
    uint8_t minUpdatesPerFrame, uint16_t maxPacketInterval)
{
    ++m_UpdateCount;
    ++m_TimeSinceLastFrame;
    if (m_TimeSinceLastFrame >= maxPacketInterval)
    {
        // connection lost, reset 
        m_Leds.setFadeRate(0);
    }

    if (!m_Leds.isFading() && m_UpdateCount >= minUpdatesPerFrame)
    {
        m_Leds.setFadePosition(m_Leds.getFadePosition() - 0x8000);
        m_UpdateCount = 0;
        ++m_Frame;
    }

    if (m_UpdateCount < PacketsPerFrame)
    {
        uint16_t index = m_UpdateCount * PacketSize;
        m_Buffer[m_UpdateCount].update(m_Frame, m_Leds, index);
    }

    return m_UpdateCount;
}

template<int PacketsPerFrame, int PacketSize, int NumBufferedFrames>
uint8_t* SmoothLedReceiver<PacketsPerFrame, PacketSize, NumBufferedFrames>::receive(
    uint8_t frame, uint8_t packet, uint8_t idealFrameStart)
{
    uint8_t framesElapsed = frame - m_LastReceivedFrame;
    if (framesElapsed > 0 && packet < 3)
    {
        m_LastReceivedFrame = frame;

        uint16_t fadeRate = m_Leds.getFadeRate();
        if (fadeRate == 0)
        {
            uint8_t frameLength = m_TimeSinceLastFrame / framesElapsed;
            if (frameLength >= PacketsPerFrame * 2)
            {
                m_Frame = frame - NumBufferedFrames;
                m_LastError = 0;
                m_ErrorI = 0;
                m_Leds.beginFade(frameLength);
                m_Leds.setFadePosition(idealFrameStart << 8);
                m_UpdateCount = (idealFrameStart * frameLength) >> 8;
                for (Buffer& buffer : m_Buffer)
                    buffer.reset();
            }
        }
        else 
        {
            uint16_t estimate = ((m_Frame + NumBufferedFrames) << 8) + highByte(m_Leds.getFadePosition() << 1);
            uint16_t actual = (frame << 8) + idealFrameStart;
            int16_t error = actual - estimate;
            // proportional error
            fadeRate += error >> 2; 
            // integral error
            m_ErrorI += (error * m_TimeSinceLastFrame) >> 4;
            fadeRate += m_ErrorI >> 8;
            // differential error
            fadeRate += (error - m_LastError) * 64 / m_TimeSinceLastFrame; 
            m_LastError = error;
            m_Leds.setFadeRate(fadeRate);
        }
        m_TimeSinceLastFrame = 0;
    }
    return m_Buffer[packet].getWriteBuffer(frame);
}

template<int PacketsPerFrame, int PacketSize, int NumBufferedFrames>
inline void SmoothLedReceiver<PacketsPerFrame, PacketSize, NumBufferedFrames>::receive(uint8_t frame, uint8_t packet,
    const uint8_t* data, uint8_t idealFrameStart)
{
    memcpy(receive(frame, packet, idealFrameStart), data, PacketSize);
}
