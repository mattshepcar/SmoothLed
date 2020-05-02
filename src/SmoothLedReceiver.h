#pragma once

#include "SmoothLedBuffer.h"

template<int TPacketsPerFrame, int TPacketSize, int TNumBufferedFrames>
class SmoothLedReceiver
{
public:
    static const int PacketsPerFrame = TPacketsPerFrame;
    static const int PacketSize = TPacketSize;
    static const int NumBufferedFrames = TNumBufferedFrames;

    SmoothLedReceiver();

    uint8_t  update(uint8_t minUpdatesPerFrame = 15, uint8_t maxPacketInterval = 250);
    void     receive(uint8_t frame, uint8_t packet, const uint8_t* data, uint8_t idealFrameStart = PacketsPerFrame + 4);
    uint8_t* receive(uint8_t frame, uint8_t packet, uint8_t idealFrameStart = PacketsPerFrame + 4);

    SmoothLed& getLeds() { return m_Leds; }

private:
    uint8_t m_FrameCount = 0;
    uint8_t m_LastReceivedFrame = 0;
    uint8_t m_UpdateCount = 0;
    uint8_t m_TimeSinceLastFrame = 0;
    uint8_t m_FrameLength = 0;
    int8_t  m_FrameDrift = 0;

    SmoothLed m_Leds;
    SmoothLed::Interpolator m_Interpolators[PacketSize * PacketsPerFrame];
    SmoothLedBuffer<PacketSize, NumBufferedFrames> m_Buffer[PacketsPerFrame];
};

template<int PacketsPerFrame, int PacketSize, int NumBufferedFrames>
SmoothLedReceiver<PacketsPerFrame, PacketSize, NumBufferedFrames>::SmoothLedReceiver()
:   m_Leds(m_Interpolators, PacketsPerFrame * PacketSize)
{
}

template<int PacketsPerFrame, int PacketSize, int NumBufferedFrames>
uint8_t SmoothLedReceiver<PacketsPerFrame, PacketSize, NumBufferedFrames>::update(
    uint8_t minUpdatesPerFrame, uint8_t maxPacketInterval
)
{
    ++m_UpdateCount;
    ++m_TimeSinceLastFrame;
    if (m_TimeSinceLastFrame >= maxPacketInterval)
    {
        // connection lost, reset 
        m_FrameLength = 0;
        for (auto& buf : m_Buffer)
            buf.reset();
    }

    if (m_UpdateCount >= minUpdatesPerFrame && !m_Leds.isFading())
    {
        ++m_FrameCount;
        m_UpdateCount = 0;
        if (m_FrameLength)
        {
            m_Leds.beginFade(m_FrameLength);
        }
    }

    if (m_UpdateCount < PacketsPerFrame)
    {
        uint16_t index = m_UpdateCount * PacketSize;
        m_Buffer[m_UpdateCount].update(m_FrameCount, m_Leds, index);
    }

    return m_UpdateCount;
}

template<int ppf, int ps, int nbf>
uint8_t* SmoothLedReceiver<ppf, ps, nbf>::receive(uint8_t frame, uint8_t packet,
    uint8_t idealFrameStart)
{
    uint8_t framesElapsed = frame - m_LastReceivedFrame;
    if (framesElapsed > 0)
    {
        m_LastReceivedFrame = frame;

        if (m_FrameLength == 0)
        {
            if (framesElapsed == 1 && m_TimeSinceLastFrame > PacketsPerFrame)
            {
                m_UpdateCount = idealFrameStart + packet;
                m_FrameLength = m_TimeSinceLastFrame;
            }
        }
        else
        {
            int8_t frameDrift = m_UpdateCount - packet - idealFrameStart;
            if (frameDrift > 0)
            {
                if (++m_FrameDrift > 1)
                {
                    --m_FrameLength;
                    m_FrameDrift = 0;
                }
            }
            else if (frameDrift < 0)
            {
                if (--m_FrameDrift < -1)
                {
                    ++m_FrameLength;
                    m_FrameDrift = 0;
                }
            }
            else
            {
                m_FrameDrift = 0;
            }
        }
        m_TimeSinceLastFrame = 0;
    }        
    return m_Buffer[packet].getWriteBuffer(m_FrameCount + NumBufferedFrames + 1);
}

template<int PacketsPerFrame, int PacketSize, int NumBufferedFrames>
void SmoothLedReceiver<PacketsPerFrame, PacketSize, NumBufferedFrames>::receive(uint8_t frame, uint8_t packet,
    const uint8_t* data, uint8_t idealFrameStart)
{
    if (packet < PacketsPerFrame)
        memcpy(receive(frame, packet, idealFrameStart), data, PacketSize);
}
