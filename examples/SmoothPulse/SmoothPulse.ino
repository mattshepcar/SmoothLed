#include <SmoothLed.h>
#include <avr/wdt.h>

// This example fades WS2812B LEDs between 3 different colours.
// Interpolation, dithering and gamma correction are employed to
// ensure a smooth fade.

#define NUM_LEDS 8
#define LED_CHANNELS 4 // use 4 for RGBW strands
uint8_t r = 0x30, g = 0, b = 0; // initial colour

// Each LED requires 5 bytes of memory to store its current
// and target colours and its dithering state.
SmoothLed::Interpolator interpolators[NUM_LEDS * LED_CHANNELS];
SmoothLed leds(interpolators, NUM_LEDS * LED_CHANNELS);

void setup()
{
    leds.clear(); // initialise all LED values to 0
    leds.begin(
        SmoothLedCcl::PA7_LUT1, // pin where LED data line is connected
        SmoothLedCcl::PB1_USART0_ASYNCCH1); // this pin will be an output but is only used for the clock signal
    leds.update(); // write current values to strip
}

void loop()
{
    // reset hardware watchdog (might be enabled in fuses)
    wdt_reset();

    if (!leds.isFading())
    {
        // set target colours
        for (uint8_t i = 0; i < NUM_LEDS; ++i)
        {
            leds.setFadeTarget(i * LED_CHANNELS + 0, g);
            leds.setFadeTarget(i * LED_CHANNELS + 1, r);
            leds.setFadeTarget(i * LED_CHANNELS + 2, b);
        }
        // fade over next 5000 updates, about 5 seconds at 1kHz
        leds.beginFade(5000);

        // cycle to next colour
        uint8_t t = r; r = g; g = b; b = t;
    }

    // update at around 1kHz for smooth dithering
    leds.update();
    // delay must be at least 50 microseconds to reset WS2812B
    delayMicroseconds(100);
}
