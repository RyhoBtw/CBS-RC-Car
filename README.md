# CBS RC Car

A Repository for documenting the build of a simple rc car using the ESP32 WROOM 30-pin

![circuit](https://github.com/RyhoBtw/CBS-RC-Car/blob/main/circuit.svg)

The Motor Controller (H-Bridge) takes 5V as an input so we need level shifter to shift the 3.3V Output from the ESP32 up to 5V. 

2 capacitors were added for voltage stabilization. Resistors were added as well to the output of the Microcontroller, to pull down the electric potential so that no faulty inputs occrue.

The Internal LED showes if the ESP is connected to a accsses point.
-> stays on if connected otherwise it blinks 

### Speed and Directions

For now the speed and direction values ranges from 0 to 200 due to power limitations of the Motor Controller.  Center value 'stop' or 'straight' respectively mapping of those values to the PWM values 0 to maxSpeed: `int pwmValue = map(value, 0, 200, 0, maxSpeed);`

### User Interface Notes

Google Chrome may have issues rendering the vertical slider. Alternative browsers (Firefox, Opera, Safari) are currently recommended.
On PC all Browsers should work. 

### Potential Refinements

Due to lag of beeing able to test the code I won't update it, but it is possible to make some refinments to it such as:

- Using the ESP32's internal pull-down resistors (with `pinMode(PIN_OUT, INPUT_PULLDOWN)`) for a cleaner circuit.
- Explore a cross-platform PWM solution for better compatibility because the way PWM is currently implementet it only works for ESP32


## Libraries

For the display it's necessary to import the SSD1306Wire library. You can find it in the Arduino Library Manager, where it is called `ESP8266_and_ESP32_OLED_driver_for_SSD1306_displays`. 

The Async Web Server is implemented as well by just importing `ESPAsyncWebServer` and `AsyncTCP` in the Library manager. 
