#include <SmoothLed.h>
#include <avr/wdt.h>
#include <avr/sleep.h>

// This example fades WS2812B LEDs between 3 different colours.
// Interpolation, dithering and gamma correction are employed to
// ensure a smooth fade at low intensities.

#define NUM_LEDS 8
#define LED_CHANNELS 4 // use 4 for RGBW strands
#define UPDATE_INTERVAL_US 1000 // how long between updates
//#define USE_TCA0_TIMING 1 // use timer interrupt for more accurate timing
uint8_t r = 0x30, g = 0, b = 0; // initial colour

// Each LED requires 5 bytes of memory to store its current
// and target colours and its dithering state.
SmoothLed::Interpolator interpolators[NUM_LEDS * LED_CHANNELS];
SmoothLed leds(interpolators, NUM_LEDS * LED_CHANNELS);

void setupPeriodicTimer(int microSeconds);
void waitForTimer();

void setup()
{
    // initialise all LED values to 0
    leds.clear(); 

    leds.begin(
        SmoothLedCcl::PA7_LUT1, // pin where LED data line is connected
        SmoothLedCcl::PB1_USART0_ASYNCCH1); // this pin will be an output but is only used for the clock signal

    setupPeriodicTimer(UPDATE_INTERVAL_US);
}

void loop()
{
    // wait for 1kHz interval
    waitForTimer();

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
        // fade over next 1000 updates (1 second at 1kHz)
        leds.beginFade(1000);

        // cycle to next colour
        uint8_t t = r; r = g; g = b; b = t;
    }

    // update fade and write dithered & gamma corrected values to LED strip
    leds.update();
}

#if USE_TCA0_TIMING
void setupPeriodicTimer(int microSeconds)
{
    // turn off split mode as per megaTinyCore guide
    TCA0.SPLIT.CTRLA = 0;
    TCA0.SPLIT.CTRLESET = TCA_SPLIT_CMD_RESET_gc | 0x03;
    TCA0.SPLIT.CTRLD = 0;

    // set up periodic timer
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
    TCA0.SINGLE.PER = (F_CPU * 1e-6 * microSeconds + 15) / 16 - 1; 
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV16_gc | TCA_SINGLE_ENABLE_bm;
}
volatile bool wakeup = false;
ISR(TCA0_OVF_vect)
{
    wakeup = true;
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}
void waitForTimer()
{
    while (!wakeup)
        sleep_mode();
    wakeup = false;
}
#else
void setupPeriodicTimer(int){}
void waitForTimer()
{
    // roughly 10us per 8 bits of data
    int timeToWriteLeds = NUM_LEDS * LED_CHANNELS * 10;
    // need a minimum of 50us to reset LEDs
    delayMicroseconds(max(50, UPDATE_INTERVAL_US - timeToWriteLeds));
}
#endif
