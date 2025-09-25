# ESP32-eInk-Weather-Display

#### This is a fork of David Bird's excellent project https://github.com/G6EJD/ESP32-e-Paper-Weather-Display. ####

### Changes from the original: ###

1. Weather modules have been moved to a separate file.
2. Improved time handling, with recording to RTC memory so that the device remembers the time even after waking up from deep sleep.
3. The device turns on Wi-Fi less frequently, once every 3 hours, and updates the weather to save battery life.
4. The device wakes up every half hour, as before, to update data from the temperature and humidity sensors.
5. Display of data from sensors and project configuration for a 2.9-inch screen.

### Device components: ###

1. ESP32 C6;
2. 2.9-inch Epaper Module from WeAct (3 colours: white, black, red);
3. Temperature, humidity and pressure sensors: BMP280 + AHT20.
4. 1200mAh 18500 lithium battery.