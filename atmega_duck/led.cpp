#include "led.h"

#include "NeoPixel.h"
#include "config.h"

namespace led {
    NeoPixel* led { nullptr };

    void begin() {
#ifdef NEOPIXEL
        led = new NeoPixel(NEOPIXEL_NUM, LED_PIN, NEO_GRB + NEO_KHZ800);
        led->begin();
        led->show();
#endif // ifdef ENABLE_NEOPIXEL
    }

    void setColor(int r, int g, int b) {
  #ifdef NEOPIXEL
        if (led) {
            for (size_t i = 0; i<led->numPixels(); i++) {
                led->setPixelColor(i, r, g, b);
            }

            led->show();
        }
#endif // ifdef ENABLE_NEOPIXEL
    }
}