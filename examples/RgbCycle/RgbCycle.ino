#include <SmoothLedCcl.h>
#include <avr/wdt.h>

// This example cycles WS2812 LEDs through a sequence of 3 colours.
// It does not perform any gamma correction, dithering or interpolation. 
// The colour values are written directly to the LED strip so no
// per-LED workspace memory is required.

#define NUM_LEDS 25

// initial colour
uint8_t r = 0, g = 0, b = 16;

SmoothLedCcl leds;

void setup() 
{
    leds.begin(
        SmoothLedCcl::PA7_LUT1, // pin where LED data line is connected
        SmoothLedCcl::PB1_USART0_ASYNCCH1); // this pin will be an output but is only used for the clock signal
}

void printHexDigit(uint8_t value)
{
    Serial.write(value >= 10 ? value + 'A' - 10 : value + '0');
}
void printHex(uint8_t value)
{
    printHexDigit(value >> 4);
    printHexDigit(value & 15);
}

void loop() 
{
    // reset hardware watchdog (might be enabled in fuses)
    wdt_reset();
    
    // display the current colour on the serial port
    // have to reinitialize Serial because SmoothLedCcl uses it to write LED data
    Serial.begin(115200);
    Serial.print("SmoothLed #");
    printHex(r);
    printHex(g);
    printHex(b);
    Serial.println();
    Serial.flush();

    // set up USART/SPI for writing to LED strip
    leds.beginTransaction();
    for(uint8_t i = 0; i < NUM_LEDS; ++i)
    {
        // write the LED colour directly
        leds.write(g);
        leds.write(r);
        leds.write(b);
        //leds.write(0);  // for RGBW strips
    }
    leds.endTransaction();

    // cycle to the next colour and wait 250ms
    uint8_t t = r; r = g; g = b; b = t;
    delay(250);
}
