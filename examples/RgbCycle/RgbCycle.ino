#include <SmoothLedCcl.h>
#include <avr/wdt.h>

#define NUM_LEDS 25

SmoothLedCcl leds;

void setup() 
{
    leds.begin(
        SmoothLedCcl::PA7_LUT1, // pin where LED data line is connected
        SmoothLedCcl::PB1_USART0_ASYNCCH1); // this pin will be an output but is only used for the clock signal
}

void loop() 
{
    wdt_reset();
    
    static int8_t r = 0, g = 0, b = 16;

    // have to reinitialize Serial because SmoothLedCcl uses it to write LED data
    Serial.begin(115200);
    Serial.printf("SmoothLed #%02x%02x%02x\n", r, g, b);
    Serial.flush();

    leds.beginTransaction();
    for(uint8_t i = 0; i < NUM_LEDS; ++i)
    {
        leds.write(g);
        leds.write(r);
        leds.write(b);
        //leds.write(0);  // for RGBW strips
    }
    leds.endTransaction();

    uint8_t t = r; r = g; g = b; b = t;
    delay(250);
}
