#include <SmoothLed.h>
#include <avr/wdt.h>

#define NUM_LEDS 10
#define LED_CHANNELS 3 // use 4 for RGBW strands

SmoothLed::Interpolator interpolators[NUM_LEDS * LED_CHANNELS];
SmoothLed leds(interpolators, NUM_LEDS * LED_CHANNELS);

void setup() 
{
    leds.clear();
    leds.begin(
        SmoothLedCcl::PA7_LUT1, // pin where LED data line is connected
        SmoothLedCcl::PB1_USART0_ASYNCCH1); // this pin will be an output but is only used for the clock signal
}

void loop() 
{
    wdt_reset();
    
    static uint8_t r = 64, g = 0, b = 0;

    if (!leds.isFading())
    {
        for (uint8_t i = 0; i < NUM_LEDS; ++i)
        {
            leds.setFadeTarget(i * LED_CHANNELS + 0, g);
            leds.setFadeTarget(i * LED_CHANNELS + 1, r);
            leds.setFadeTarget(i * LED_CHANNELS + 2, b);
        }
        leds.beginFade(1000);

        uint8_t t = r; r = g; g = b; b = t;
    }

    leds.update();
    delayMicroseconds(800);
}
