#include <SmoothLedCcl.h>
#include <avr/wdt.h>

// This is an interrupt driven version of the RgbCycle example.
// It only works in SPI mode because it's not currently possible to
// override megaTinyCore's USART DRE interrupt.

// This example cycles WS2812 LEDs through a sequence of 3 colours.
// It does not perform any gamma correction, dithering or interpolation. 
// The colour values are written directly to the LED strip so no
// per-LED workspace memory is required.

#define NUM_LEDS 8
#define LED_CHANNELS 4

volatile uint8_t ledDataPos = 0;
uint8_t ledData[NUM_LEDS * LED_CHANNELS] = { 0 };

// initial colour
uint8_t r = 0, g = 0, b = 16;

SmoothLedCcl leds;

void setup() 
{    
    leds.begin(
        SmoothLedCcl::PA7_LUT1, // pin where LED data line is connected
        SmoothLedCcl::PA3_SPI0_ASYNCCH0); // this pin will be an output but is only used for the clock signal
}

void loop() 
{
    // reset hardware watchdog (might be enabled in fuses)
    wdt_reset();

    // update LED data array
    for (uint8_t i = 0; i < NUM_LEDS; ++i)
    {
        ledData[i * LED_CHANNELS + 0] = g;
        ledData[i * LED_CHANNELS + 1] = r;
        ledData[i * LED_CHANNELS + 2] = b;
    }
    // reset interrupt read position
    ledDataPos = 0;

    // set up USART/SPI for writing to LED strip
    leds.beginTransaction();
    // enable 'data register empty' interrupt to start sending
    SPI0.INTCTRL |= SPI_DREIE_bm;

    
    // we could do other processing while the LED strip is updating here


    // cycle to the next colour and wait 250ms
    uint8_t t = r; r = g; g = b; b = t;
    delay(250);
}

ISR(SPI0_INT_vect)
{
    // clear interrupt flag
    SPI0.INTFLAGS |= SPI_DREIF_bm;

    // send data. needs to be inverted for the CCL setup
    SPI0.DATA = ~ledData[ledDataPos];  

    // turn off the interrupt when we're done
    if (++ledDataPos == sizeof(ledData))
        SPI0.INTCTRL &= ~SPI_DREIE_bm;
}
