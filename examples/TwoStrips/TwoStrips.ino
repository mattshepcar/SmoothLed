#include <SmoothLed.h>
#include <avr/wdt.h>
#include <avr/sleep.h>

// This example updates two WS2812B LED strips at the same time, 
// fading them between 3 different colours.  Interpolation, dithering 
// and gamma correction are employed to ensure a smooth fade at low 
// intensities.

// A device with two TCB timers is required such as an ATtiny1614.
// Both strips are updated in parallel which is only faster when running
// at 16MHz or above.  One strip uses SPI and the other uses USART.

// The main reason you might want to update two strips at once instead
// of just connecting them together is to improve the update frequency
// if the strips are long (80+ LEDs?) so that you get better dithering.

#define NUM_LEDS 50
#define LED_CHANNELS 3 // use 4 for RGBW strands
#define UPDATE_INTERVAL_US 2000 // how long between updates
#define USE_TCA0_TIMING 1 // use timer interrupt for more accurate timing
uint8_t r = 0x30, g = 0, b = 0; // initial colour

// Each LED requires 5 bytes of memory to store its current
// and target colours and its dithering state.
SmoothLed::Interpolator interpolators0[NUM_LEDS * LED_CHANNELS];
SmoothLed leds0(interpolators0, NUM_LEDS * LED_CHANNELS);
SmoothLed::Interpolator interpolators1[NUM_LEDS * LED_CHANNELS];
SmoothLed leds1(interpolators1, NUM_LEDS * LED_CHANNELS);

void setupPeriodicTimer(int microSeconds);
void waitForTimer();

void setup()
{
    // initialise all LED values to 0
    leds0.clear(); 
    leds1.clear();

    leds0.begin(
        SmoothLedCcl::PA4_LUT0, // pin where first LED data line is connected
        SmoothLedCcl::PA3_SPI0_ASYNCCH0,  // this pin will be an output but is only used for the clock signal
        TCB0);
    leds1.begin(
        SmoothLedCcl::PA7_LUT1, // pin where second LED data line is connected
        SmoothLedCcl::PB1_USART0_ASYNCCH1, // this pin will be an output but is only used for the clock signal
        TCB1); // need to specify secondary timer

    setupPeriodicTimer(UPDATE_INTERVAL_US);
}

void loop()
{
    // wait for 1kHz interval
    waitForTimer();

    // reset hardware watchdog (might be enabled in fuses)
    wdt_reset();

    if (!leds0.isFading())
    {
        // set target colours
        for (uint8_t i = 0; i < NUM_LEDS; ++i)
        {
            leds0.setFadeTarget(i * LED_CHANNELS + 0, g);
            leds0.setFadeTarget(i * LED_CHANNELS + 1, r);
            leds0.setFadeTarget(i * LED_CHANNELS + 2, b);

            leds1.setFadeTarget(i * LED_CHANNELS + 0, b);
            leds1.setFadeTarget(i * LED_CHANNELS + 1, g);
            leds1.setFadeTarget(i * LED_CHANNELS + 2, r);
        }
        // fade over next 500 updates (1 second at 500Hz)
        leds0.beginFade(500);
        leds1.beginFade(500);

        // cycle to next colour
        uint8_t t = r; r = g; g = b; b = t;
    }

    // update fade and write dithered & gamma corrected values to LED strips
    uint8_t deltaTime = leds0.updateTime();
    uint8_t n = NUM_LEDS * LED_CHANNELS;
    const uint16_t* gammaLut = leds0.getGammaLut();
    uint16_t maxValue = leds0.getMaxValue();
    uint8_t ditherMask = leds0.getDitherMask();
    SmoothLed::Interpolator* i0 = leds0.getInterpolators();
    SmoothLed::Interpolator* i1 = leds1.getInterpolators();
    leds0.beginTransactionSpi();
    leds1.beginTransactionUsart();
    do {
        leds0.writeSpi(i0++->update(deltaTime, gammaLut, maxValue, ditherMask));
        leds1.writeUsart(i1++->update(deltaTime, gammaLut, maxValue, ditherMask));
    } while (--n);
    leds0.endTransactionSpi();
    leds1.endTransactionUsart();

    delayMicroseconds(50);
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
    // roughly 10us per 8 bits of data and 50us to reset
    int timeToWriteLeds = NUM_LEDS * LED_CHANNELS * 10 + 50;
    delayMicroseconds(max(0, UPDATE_INTERVAL_US - timeToWriteLeds));
}
#endif
