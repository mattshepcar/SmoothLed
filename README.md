# SmoothLed
Arduino library for FadeCandy style control of single-wire-based LED neopixels and WS2812B strips with megaTinyCore (AVR 0/1 series ATtiny1614 etc)

This library drives WS2812 style LEDs with gamma correction, interpolation and temporal dithering (inspired by FadeCandy).  This gives good quality output at low intensities with smooth fades and no 'stair stepping' effect.  There is minimal CPU overhead compared to the usual bit banging LED libraries.  To achieve this the SPI (or USART in MSPI mode) and CCL peripherals on the AVR 0/1 series are used.  

# Pin selection

An output pin must be assigned for the SPI clock (PA3, PB1 or PC0) but the USART/SPI data line is not used: instead the LED strip is connected to either EVOUT or LUT out (choice of PA2, PA4, PA7, PB2, PB4, PC1, PC2).  The USART/SPI peripheral can still be used when the LEDs are not being updated so long as it can cope with SCK/XCK being driven when the LEDs are updated which should be fine if you have a chip select pin for your SPI devices.

USART mode gives better control of the output bitrate so it's best to use the library in that mode.  The timings for a low bit and a high bit can be adjusted for the particular type of LED you are using as parameters to the `begin` function. 



# Dithering

To benefit from temporal dithering the LED strip must be updated at a high frequency, preferably above 400Hz.  The slower you update the LEDs the more noticable flickering at low intensities will be.  This can be partially mitigated by adjusting the dither mask with setDitherMask or in the SmoothLed constructor. Fewer bits will result in less flickering but more 'stair steppy' fades.

# Gamma correction

You can supply a custom gamma correction table with the setGammaLut function.  Use the python script in the SmoothLed/extras folder to generate a new table.