/* ESP Weather Display using an EPD 2.9" Display, obtains data from Open Weather Map, decodes it and then displays it.
  ####################################################################################################################################
  This software, the ideas and concepts is Copyright (c) David Bird 2018. All rights to this software are reserved.

  Any redistribution or reproduction of any part or all of the contents in any form is prohibited other than the following:
  1. You may print or download to a local hard disk extracts for your personal and non-commercial use only.
  2. You may copy the content to individual third parties for their personal use, but only if you acknowledge the author David Bird as the source of the material.
  3. You may not, except with my express written permission, distribute or commercially exploit the content.
  4. You may not transmit it or store it in any other website or other form of electronic retrieval system for commercial purposes.

  The above copyright ('as annotated') notice and this permission notice shall be included in all copies or substantial portions of the Software and where the
  software use is visible to an end-user.

  THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT. FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY
  OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  See more at http://www.dsbird.org.uk
*/

#include "src/owm_credentials.h"
#include <Adafruit_BMP280.h>
#include <Adafruit_AHTX0.h>
#include <ArduinoJson.h>     // https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>
#include "time.h"
#include "esp_sntp.h"
#define ENABLE_GxEPD2_display 0
#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>
//#include "src/lang.h"                 // Localisation (English)
#include "src/lang_gr.h"            // Localisation (German)
#include "src/icons.h"

#define SCREEN_WIDTH   296
#define SCREEN_HEIGHT  128
#define BUILTIN_LED    8
#define I2C_SCL 5
#define I2C_SDA 4

#define EPD_BUSY  7 // violet
#define EPD_RES  18 // orange
#define EPD_DC   15 // white
#define EPD_CS   14 // blue
#define EPD_SCL  19 // green
#define EPD_SDA  20 // yellow
#define EPD_MISO -1 // Master-In Slave-Out not used, as no data from display

enum alignmentType {LEFT, RIGHT, CENTER};

Adafruit_BMP280 bmp;
Adafruit_AHTX0 aht;
GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(EPD_CS, EPD_DC, EPD_RES, EPD_BUSY)); // GDEM029C90 128x296, SSD1680
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;  // Select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
// Using fonts:
// u8g2_font_helvB08_te
// u8g2_font_helvB10_te
// u8g2_font_helvB12_te
// u8g2_font_helvB14_te
// u8g2_font_helvB18_te
// u8g2_font_helvB24_te

//################ VARIABLES ###########################
bool    LargeIcon = true, SmallIcon = false;
#define Large 7    // For best results use odd numbers
#define Small 3    // For best results use odd numbers
String  time_str, date_str, ForecastDay; // strings to hold time and date
bool data_updated = false;
wl_status_t last_wifi_status;
sntp_sync_status_t syncStatus;
//################ PROGRAM VARIABLES and OBJECTS ##########################################
#define max_readings 8 // Daily forecast for 8 days
#include "src/common.h"

uint16_t max_starts_between_refresh = 6;
long SleepDuration = 30; // Sleep time in minutes, aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour
int  WakeupHour    = 7;  // Don't wakeup until after 07:00 to save battery power
int  SleepHour     = 23; // Sleep after (23+1) 00:00 to save battery power

RTC_DATA_ATTR bool clean_start = true;
RTC_DATA_ATTR uint8_t startup_count = 0;
RTC_DATA_ATTR struct tm timeinfo;
RTC_DATA_ATTR String wifi_ssid;
RTC_DATA_ATTR String wifi_pass;

//#########################################################################################
void setup() {
  setCpuFrequencyMhz(10);
  setenv("TZ", Timezone, 1);  // setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset(); // Set the TZ environment variable
  time_t now = time(nullptr);
  localtime_r(&now, &timeinfo);

  Wire.begin(I2C_SDA, I2C_SCL);
  // Serial.begin(9600);
  // while ( !Serial ) delay(100); // Wait for Serial Monitor to open
  if (startup_count >= max_starts_between_refresh) {
    startup_count = 0;
    clean_start = true;
  }
  InitialiseDisplay();
  if (clean_start) {
    bool WakeUp = (WakeupHour > SleepHour)
    ? (timeinfo.tm_hour >= WakeupHour || timeinfo.tm_hour <= SleepHour)
    : (timeinfo.tm_hour >= WakeupHour && timeinfo.tm_hour <= SleepHour);
    if (WakeUp || timeinfo.tm_year < (2025 - 1900) ) { // Check the system time to determine whether this is a new start
      setCpuFrequencyMhz(80);
      if (StartWiFi() == WL_CONNECTED && SetupTime() == true) {
        byte Attempts = 1;
        bool RxWeather = false;
        WiFiClient client;
        while (!RxWeather && Attempts <= 2) { // Try up-to 2 times for Weather and Forecast data
          RxWeather = ReceiveOneCallWeather(client, false);
          Attempts++;
        }
        if (RxWeather) { // Only if received Weather data
          display.fillScreen(GxEPD_WHITE);
          DisplayWeather();
          data_updated = true;
        }
      }
      StopWiFi(); // Reduces power consumption
    }
  }
  ReadDrawSensors();
  BeginSleep();
}
//#########################################################################################
void loop() { // this will never run!
}
//#########################################################################################
void ReadDrawSensors() {
  int8_t  x = 100, y = 16, w = 62, h = 38;
  if (!clean_start || !data_updated) {
    display.setPartialWindow(x, y, w, h);
    display.firstPage();
    display.fillScreen(GxEPD_WHITE);
  }
  u8g2Fonts.setFont(u8g2_font_helvB10_te);
  // Initialize AHT20 and BMP280 sensors
  if (aht.begin() && bmp.begin()) {
    Serial.println("BMP280, AHT20 sensors initialized!");
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                Adafruit_BMP280::SAMPLING_X1,     /* Temp. oversampling */
                Adafruit_BMP280::SAMPLING_X4,    /* Pressure oversampling */
                Adafruit_BMP280::FILTER_X4,      /* Filtering. */
                Adafruit_BMP280::STANDBY_MS_125); /* Standby time. */
    // Read data from AHT20
    sensors_event_t humidity_aht, temp_aht;
    aht.getEvent(&humidity_aht, &temp_aht);
    // Read data from BMP280
    long pressure_bmp = bmp.readPressure();
    float average_temp = (bmp.readTemperature() + temp_aht.temperature) / 2;
    display.drawInvertedBitmap(x, y, epd_house_thermometer, 16, 16, GxEPD_RED);
    drawRedString(x + 16, y + 6, String(average_temp, 1) + "°", LEFT);
    display.drawInvertedBitmap(x, y + 15, epd_house_humidity, 16, 16, GxEPD_RED);
    drawRedString(x + 16, y + 21, String(humidity_aht.relative_humidity, 0) + " %", LEFT);
    u8g2Fonts.setFont(u8g2_font_helvB08_te);
    drawRedString(x + 15, y + 32, String(pressure_bmp / 100.0F, (Units == "M" ? 0 : 1)) + (Units == "M" ? " hPa" : " in"), LEFT);
  } else {
    Serial.println("Failed to initialize BMP280 or AHT20 sensor!");
    drawRedString(x + 16, y + 6, "No data", LEFT);
  }
  if (!clean_start || !data_updated) display.nextPage();
  else display.display(false); // Full screen update mode
}
//#########################################################################################
void BeginSleep() {
  clean_start = false;
  startup_count++;
  display.powerOff();
  esp_sleep_enable_timer_wakeup((SleepDuration * 60) * 1000000LL);
#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, INPUT); // If it's On, turn it off and some boards use GPIO for SPI-SS, which remains low after screen use
  digitalWrite(BUILTIN_LED, HIGH);
#endif
  Serial.println("Entering " + String(SleepDuration) + " minutes of sleep time");
  esp_deep_sleep_start();  // Sleep for e.g. 30 minutes
}
//#########################################################################################
void DisplayWeather() {             // 2.9" e-paper display is 296x128 resolution
  Draw_Heading_Section();           // Top line of the display
  Draw_Main_Weather_Section();      // Centre section of display for Location, temperature, Weather report, Wx Symbol and wind direction
  int Forecast = 1, Dposition = 0;
  while (Forecast <= 3) {
    DisplayForecastWeather(18, 104, Forecast, Dposition, 57); // x,y coordinates, forecast number, position, spacing width
    Dposition++;
    Forecast++;
  }
}
//#########################################################################################
void Draw_Heading_Section() {
  u8g2Fonts.setFont(u8g2_font_helvB08_te);
  drawString(5, 15, City, LEFT);
  drawString(2, 1, time_str + "   " + date_str, LEFT);
  display.drawLine(0, 11, 160 + (Units == "I"?15:0), 11, GxEPD_BLACK);
  DrawBattery(127, 12);
}
//#########################################################################################
void DisplayForecastWeather(int x, int y, int forecast, int Dposition, int fwidth) {
  GetForecastDay(Daily[forecast].Dt);
  x += fwidth * Dposition;
  DisplayConditionsSection(x + 12, y, Daily[forecast].Icon, SmallIcon);
  u8g2Fonts.setFont(u8g2_font_helvB08_te);
  drawString(x + 8, y - 24, ForecastDay, CENTER);
  drawString(x + 18, y + 12, String(Daily[Dposition].High, 0) + "°/" + String(Daily[Dposition].Low, 0) + "°", CENTER);
  display.drawRect(x - 18, y - 27, fwidth, 51, GxEPD_BLACK);
}
//#########################################################################################
void Draw_Main_Weather_Section() {
  DisplayConditionsSection(207, 52, WxConditions[0].Icon, LargeIcon);
  u8g2Fonts.setFont(u8g2_font_helvB14_te);
  drawString(3, 33, String(WxConditions[0].Temperature, 1) + "° / " + String(WxConditions[0].Humidity, 0) + "%", LEFT);
  u8g2Fonts.setFont(u8g2_font_helvB10_te);
  DrawWind(269, 34, WxConditions[0].Winddir, WxConditions[0].Windspeed);
  if (WxConditions[0].Rainfall > 0.005 || WxConditions[0].Snowfall > 0.005) {
    if (WxConditions[0].Rainfall > 0.005) drawString(168, 13, String(WxConditions[0].Rainfall, 1) + (Units == "M" ? "mm " : "in ") + TXT_PRECIPITATION_SOON, LEFT);
    else drawString(168, 13, String(WxConditions[0].Snowfall, 1) + (Units == "M" ? "mm " : "in ") + TXT_PRECIPITATION_SOON, LEFT); // Rain has precedence over snow if both reported!
  }
  DrawPressureTrend(3, 49, WxConditions[0].Pressure, WxConditions[0].Trend);
  u8g2Fonts.setFont(u8g2_font_helvB12_te);
  String Wx_Description = WxConditions[0].Description;
  drawString(2, 64, TitleCase(Wx_Description), LEFT);
  display.drawLine(0, 77, 296, 77, GxEPD_BLACK);
  DisplayAstronomySection(170, 64); // Astronomy section Sun rise/set and Moon phase plus icon
}
//#########################################################################################
void DisplayAstronomySection(int x, int y) {
  display.drawRect(x, y + 13, 126, 51, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB08_te);
  SunRise(x + 64, y + 23);
  drawString(x + 80, y + 20, ConvertUnixTime(WxConditions[0].Sunrise + WxConditions[0].Timezone).substring(0, (Units == "M" ? 5 : 7)), LEFT);
  SunSet(x + 64, y + 38);
  drawString(x + 80, y + 35, ConvertUnixTime(WxConditions[0].Sunset + WxConditions[0].Timezone).substring(0, (Units == "M" ? 5 : 7)), LEFT);
  const int day_utc   = timeinfo.tm_mday;
  const int month_utc = timeinfo.tm_mon + 1;
  const int year_utc  = timeinfo.tm_year + 1900;
  drawString(x + 6, y + 53, MoonPhase(day_utc, month_utc, year_utc, Hemisphere), LEFT);
  DrawMoon(x - 12, y - 1, day_utc, month_utc, year_utc, Hemisphere);
}
//#########################################################################################
String MoonPhase(int d, int m, int y, String hemisphere) {
  int c, e;
  double jd;
  int b;
  if (m < 3) {
    y--;
    m += 12;
  }
  ++m;
  c   = 365.25 * y;
  e   = 30.6  * m;
  jd  = c + e + d - 694039.09;     /* jd is total days elapsed */
  jd /= 29.53059;                        /* divide by the moon cycle (29.53 days) */
  b   = jd;                              /* int(jd) -> b, take integer part of jd */
  jd -= b;                               /* subtract integer part to leave fractional part of original jd */
  b   = jd * 8 + 0.5;                /* scale fraction from 0-8 and round by adding 0.5 */
  b   = b & 7;                           /* 0 and 8 are the same phase so modulo 8 for 0 */
  if (hemisphere == "south") b = 7 - b;
  if (b == 0) return TXT_MOON_NEW;              // New;              0%  illuminated
  if (b == 1) return TXT_MOON_WAXING_CRESCENT;  // Waxing crescent; 25%  illuminated
  if (b == 2) return TXT_MOON_FIRST_QUARTER;    // First quarter;   50%  illuminated
  if (b == 3) return TXT_MOON_WAXING_GIBBOUS;   // Waxing gibbous;  75%  illuminated
  if (b == 4) return TXT_MOON_FULL;             // Full;            100% illuminated
  if (b == 5) return TXT_MOON_WANING_GIBBOUS;   // Waning gibbous;  75%  illuminated
  if (b == 6) return TXT_MOON_THIRD_QUARTER;    // Third quarter;   50%  illuminated
  if (b == 7) return TXT_MOON_WANING_CRESCENT;  // Waning crescent; 25%  illuminated
  return "";
}
//#########################################################################################
void DrawMoon(int x, int y, int dd, int mm, int yy, String hemisphere) {
  const int diameter = 34;
  double Phase = NormalizedMoonPhase(dd, mm, yy);
  hemisphere.toLowerCase();
  if (hemisphere == "south") Phase = 1 - Phase;
  // Draw dark part of moon
  display.fillCircle(x + diameter - 1, y + diameter, diameter / 2, GxEPD_BLACK);
  const int number_of_lines = 90;
  for (double Ypos = 0; Ypos <= number_of_lines / 2; Ypos++) {
    double Xpos = sqrt(number_of_lines / 2 * number_of_lines / 2 - Ypos * Ypos);
    // Determine the edges of the lighted part of the moon
    double Rpos = 2 * Xpos;
    double Xpos1, Xpos2;
    if (Phase < 0.5) {
      Xpos1 = -Xpos;
      Xpos2 = Rpos - 2 * Phase * Rpos - Xpos;
    }
    else {
      Xpos1 = Xpos;
      Xpos2 = Xpos - 2 * Phase * Rpos + Rpos;
    }
    // Draw light part of moon
    double pW1x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW1y = (number_of_lines - Ypos)  / number_of_lines * diameter + y;
    double pW2x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW2y = (number_of_lines - Ypos)  / number_of_lines * diameter + y;
    double pW3x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW3y = (Ypos + number_of_lines)  / number_of_lines * diameter + y;
    double pW4x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW4y = (Ypos + number_of_lines)  / number_of_lines * diameter + y;
    display.drawLine(pW1x, pW1y, pW2x, pW2y, GxEPD_WHITE);
    display.drawLine(pW3x, pW3y, pW4x, pW4y, GxEPD_WHITE);
  }
  display.drawCircle(x + diameter - 1, y + diameter, diameter / 2, GxEPD_BLACK);
}
//#########################################################################################
void DrawWind(int x, int y, float angle, float windspeed) {
#define Cradius 18
  float dx = Cradius * cos((angle - 90) * PI / 180) + x; // calculate X position
  float dy = Cradius * sin((angle - 90) * PI / 180) + y; // calculate Y position
  arrow(x, y, Cradius - 15, angle, 10, 20); // Show wind direction on outer circle
  display.drawCircle(x, y, Cradius + 2, GxEPD_BLACK);
  for (int m = 0; m < 360; m = m + 45) {
    dx = Cradius * cos(m * PI / 180); // calculate X position
    dy = Cradius * sin(m * PI / 180); // calculate Y position
    display.drawLine(x + dx, y + dy, x + dx * 0.8, y + dy * 0.8, GxEPD_BLACK);
  }
  u8g2Fonts.setFont(u8g2_font_helvB10_te);
  drawString(x - 7 + (WindDegToDirection(angle).length() < 2 ? 10 : 0), y + Cradius + 15, WindDegToDirection(angle), CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB08_te);
  drawString(x + 5, y - Cradius - 16, String(windspeed, 1) + (Units == "M" ? " m/s" : " mph"), CENTER);
}
//#########################################################################################
String WindDegToDirection(float winddirection) {
  if (winddirection >= 348.75 || winddirection < 11.25)  return TXT_N;
  if (winddirection >=  11.25 && winddirection < 33.75)  return TXT_NNE;
  if (winddirection >=  33.75 && winddirection < 56.25)  return TXT_NE;
  if (winddirection >=  56.25 && winddirection < 78.75)  return TXT_ENE;
  if (winddirection >=  78.75 && winddirection < 101.25) return TXT_E;
  if (winddirection >= 101.25 && winddirection < 123.75) return TXT_ESE;
  if (winddirection >= 123.75 && winddirection < 146.25) return TXT_SE;
  if (winddirection >= 146.25 && winddirection < 168.75) return TXT_SSE;
  if (winddirection >= 168.75 && winddirection < 191.25) return TXT_S;
  if (winddirection >= 191.25 && winddirection < 213.75) return TXT_SSW;
  if (winddirection >= 213.75 && winddirection < 236.25) return TXT_SW;
  if (winddirection >= 236.25 && winddirection < 258.75) return TXT_WSW;
  if (winddirection >= 258.75 && winddirection < 281.25) return TXT_W;
  if (winddirection >= 281.25 && winddirection < 303.75) return TXT_WNW;
  if (winddirection >= 303.75 && winddirection < 326.25) return TXT_NW;
  if (winddirection >= 326.25 && winddirection < 348.75) return TXT_NNW;
  return "?";
}
//#########################################################################################
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength) {
  // x,y is the centre poistion of the arrow and asize is the radius out from the x,y position
  // aangle is angle to draw the pointer at e.g. at 45° for NW
  // pwidth is the pointer width in pixels
  // plength is the pointer length in pixels
  float dx = (asize + 28) * cos((aangle - 90) * PI / 180) + x; // calculate X position
  float dy = (asize + 28) * sin((aangle - 90) * PI / 180) + y; // calculate Y position
  float x1 = 0;         float y1 = plength;
  float x2 = pwidth / 2;  float y2 = pwidth / 2;
  float x3 = -pwidth / 2; float y3 = pwidth / 2;
  float angle = aangle * PI / 180;
  float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
  float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
  float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
  float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
  float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
  float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
  display.fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2, GxEPD_BLACK);
}
//#########################################################################################
void DrawPressureTrend(int x, int y, float pressure, String slope) {
  drawString(x, y - 2, String(pressure, (Units == "M" ? 0 : 1)) + (Units == "M" ? " hPa" : " in"), LEFT);
  x = x + 52 - (Units == "M" ? 0 : 15); y = y + 3;
  if      (slope == "+") {
    display.drawLine(x,  y, x + 4, y - 4, GxEPD_BLACK);
    display.drawLine(x + 4, y - 4, x + 8, y, GxEPD_BLACK);
  }
  else if (slope == "0") {
    display.drawLine(x + 3, y - 4, x + 8, y, GxEPD_BLACK);
    display.drawLine(x + 3, y + 4, x + 8, y, GxEPD_BLACK);
  }
  else if (slope == "-") {
    display.drawLine(x,  y, x + 4, y + 4, GxEPD_BLACK);
    display.drawLine(x + 4, y + 4, x + 8, y, GxEPD_BLACK);
  }
}
//#########################################################################################
uint8_t StartWiFi() {
  Serial.println("\r\nConnecting to: " + String(ssid));
  WiFi.disconnect();
  WiFi.setTxPower(WIFI_POWER_2dBm);
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.print("STA: Failed!\n");
    WiFi.disconnect(false);
    delay(500);
    WiFi.reconnect();
  }
  last_wifi_status = WiFi.status();
  if (last_wifi_status != WL_CONNECTED) {
    Serial.println("WiFi connection *** FAILED ***");
  }
  return last_wifi_status;
}
//#########################################################################################
void StopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}
//#########################################################################################
boolean SetupTime() {
  char   time_output[30], day_output[30], update_time[30];
  configTzTime(Timezone, ntpServer, "time.google.com"); // (const char* Timezone, ntpServer)
  for(auto count = 0; count < 200; count++) {
    syncStatus = esp_sntp_get_sync_status();
    if (syncStatus == SNTP_SYNC_STATUS_COMPLETED) break;
    delay(100);
  }
  if (syncStatus != SNTP_SYNC_STATUS_COMPLETED) {
    Serial.println("*** Failed to sync time! ***");
    return false;
  }
  Serial.println("*** Time Synced! ***");
  time_t now = time(nullptr);
  localtime_r(&now, &timeinfo);
  // See http://www.cplusplus.com/reference/ctime/strftime/
  if (Units == "M") {
    if ((Language == "CZ") || (Language == "DE") || (Language == "NL") || (Language == "PL")) {
      sprintf(day_output, "%s, %02u. %s %04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900); // day_output >> Sa., 20. Sep 2025 <<
    }
    else
    {
      sprintf(day_output, "%s  %02u-%s-%04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    }
    strftime(update_time, sizeof(update_time), "%H:%M:%S", &timeinfo);  // Creates: '@ 14:05:49'   and change from 30 to 8 <<<
    sprintf(time_output, "%s", update_time);
  }
  else
  {
    strftime(day_output, sizeof(day_output), "%a %b-%d-%Y", &timeinfo); // Creates  'Sat Sep-20-2025'
    strftime(update_time, sizeof(update_time), "%r", &timeinfo);        // Creates: '02:05:49pm'
    sprintf(time_output, "%s", update_time);
  }
  date_str = day_output;
  time_str = time_output;
  return true;
}
//#########################################################################################
void DrawBattery(int x, int y) {
  uint8_t percentage = 100;
  float voltage = analogRead(35) / 4096.0 * 7.46;
  if (voltage > 1 ) { // Only display if there is a valid reading
    Serial.println("Voltage = " + String(voltage));
    percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
    if (voltage >= 4.20) percentage = 100;
    if (voltage <= 3.50) percentage = 0;
    display.drawRect(x + 15, y - 12, 19, 10, GxEPD_BLACK);
    display.fillRect(x + 34, y - 10, 2, 5, GxEPD_BLACK);
    display.fillRect(x + 17, y - 10, 15 * percentage / 100.0, 6, GxEPD_BLACK);
    // drawString(x + 60, y - 11, String(percentage) + "%", RIGHT);
    // drawString(x + 13, y + 5,  String(voltage, 2) + "v", CENTER);
  }
}
//#########################################################################################
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  // Draw cloud outer
  display.fillCircle(x - scale * 3, y, scale, GxEPD_BLACK);                // Left most circle
  display.fillCircle(x + scale * 3, y, scale, GxEPD_BLACK);                // Right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4, GxEPD_BLACK);    // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, GxEPD_BLACK); // Right middle upper circle
  display.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, GxEPD_BLACK); // Upper and lower lines
  // Clear cloud inner
  display.fillCircle(x - scale * 3, y, scale - linesize, GxEPD_WHITE);            // Clear left most circle
  display.fillCircle(x + scale * 3, y, scale - linesize, GxEPD_WHITE);            // Clear right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4 - linesize, GxEPD_WHITE);  // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, GxEPD_WHITE); // Right middle upper circle
  display.fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2, GxEPD_WHITE); // Upper and lower lines
}
//#########################################################################################
void addraindrop(int x, int y, int scale) {
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y , GxEPD_BLACK);
  x = x + scale * 1.6; y = y + scale / 3;
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y , GxEPD_BLACK);
}
//#########################################################################################
void addrain(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) scale *= 1.34;
  for (int d = 0; d < 4; d++) {
    addraindrop(x + scale * (7.8 - d * 1.95) - scale * 5.2, y + scale * 2.1 - scale / 6, scale / 1.6);
  }
}
//#########################################################################################
void addsnow(int x, int y, int scale, bool IconSize) {
  int dxo, dyo, dxi, dyi;
  for (int flakes = 0; flakes < 5; flakes++) {
    for (int i = 0; i < 360; i = i + 45) {
      dxo = 0.5 * scale * cos((i - 90) * 3.14 / 180); dxi = dxo * 0.1;
      dyo = 0.5 * scale * sin((i - 90) * 3.14 / 180); dyi = dyo * 0.1;
      display.drawLine(dxo + x + flakes * 1.5 * scale - scale * 3, dyo + y + scale * 2, dxi + x + 0 + flakes * 1.5 * scale - scale * 3, dyi + y + scale * 2, GxEPD_BLACK);
    }
  }
}
//#########################################################################################
void addtstorm(int x, int y, int scale) {
  y = y + scale / 2;
  for (int i = 0; i < 5; i++) {
    display.drawLine(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale, GxEPD_BLACK);
      display.drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale, GxEPD_BLACK);
    }
    display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1, GxEPD_BLACK);
      display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2, GxEPD_BLACK);
    }
    display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5, GxEPD_BLACK);
      display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5, GxEPD_BLACK);
    }
  }
}
//#########################################################################################
void addsun(int x, int y, int scale, bool IconSize) {
  int linesize = 3;
  if (IconSize == SmallIcon) linesize = 1;
  display.fillRect(x - scale * 2, y, scale * 4, linesize, GxEPD_BLACK);
  display.fillRect(x, y - scale * 2, linesize, scale * 4, GxEPD_BLACK);
  display.drawLine(x - scale * 1.3, y - scale * 1.3, x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
  display.drawLine(x - scale * 1.3, y + scale * 1.3, x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
  if (IconSize == LargeIcon) {
    display.drawLine(1 + x - scale * 1.3, y - scale * 1.3, 1 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(2 + x - scale * 1.3, y - scale * 1.3, 2 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(3 + x - scale * 1.3, y - scale * 1.3, 3 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(1 + x - scale * 1.3, y + scale * 1.3, 1 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
    display.drawLine(2 + x - scale * 1.3, y + scale * 1.3, 2 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
    display.drawLine(3 + x - scale * 1.3, y + scale * 1.3, 3 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
  }
  display.fillCircle(x, y, scale * 1.3, GxEPD_WHITE);
  display.fillCircle(x, y, scale, GxEPD_BLACK);
  display.fillCircle(x, y, scale - linesize, GxEPD_WHITE);
}
//#########################################################################################
void addfog(int x, int y, int scale, int linesize, bool IconSize) {
  if (IconSize == SmallIcon) {
    linesize = 1;
    y = y - 1;
  } else y = y - 3;
  for (int i = 0; i < 6; i++) {
    display.fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.6, scale * 6, linesize, GxEPD_BLACK);
  }
}
//#########################################################################################
void addmoon(int x, int y, int scale, bool IconSize) {
  if (IconSize == LargeIcon) {
    x = x - 5; y = y + 5;
    display.fillCircle(x - 21, y - 23, scale, GxEPD_BLACK);
    display.fillCircle(x - 14, y - 23, scale * 1.7, GxEPD_WHITE);
  }
  else
  {
    display.fillCircle(x - 16, y - 11, scale, GxEPD_BLACK);
    display.fillCircle(x - 11, y - 11, scale * 1.7, GxEPD_WHITE);
  }
}
//#########################################################################################
void SunRise(int x, int y) {
  u8g2Fonts.setFont(u8g2_font_helvB08_te);
  addsun(x, y, 3, SmallIcon);
  display.drawFastVLine(x + 10, y - 6, 10, GxEPD_BLACK);
  display.drawLine(x + 10 - 3, y - 6 + 3, x + 10, y - 6, GxEPD_BLACK);
  display.drawLine(x + 10 + 3, y - 6 + 3, x + 10, y - 6, GxEPD_BLACK);
}
//#########################################################################################
void SunSet(int x, int y) {
  u8g2Fonts.setFont(u8g2_font_helvB08_te);
  addsun(x, y, 3, SmallIcon);
  display.drawFastVLine(x + 10, y - 6, 10, GxEPD_BLACK);
  display.drawLine(x + 10 - 3, y + 4 - 3, x + 10, y + 4, GxEPD_BLACK);
  display.drawLine(x + 10 + 3, y + 4 - 3, x + 10, y + 4, GxEPD_BLACK);
}
//#########################################################################################
void drawString(int x, int y, String text, alignmentType alignment) {
  int16_t  x1, y1; // The bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (alignment == RIGHT)  x = x - w;
  if (alignment == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y + h);
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.print(text);
}
//#########################################################################################
void drawRedString(int x, int y, String text, alignmentType alignment) {
  int16_t  x1, y1; // The bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (alignment == RIGHT)  x = x - w;
  if (alignment == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y + h);
  u8g2Fonts.setForegroundColor(GxEPD_RED);
  u8g2Fonts.print(text);
}
//#########################################################################################
void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignmentType alignment) {
  int16_t  x1, y1; // The bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (alignment == RIGHT)  x = x - w;
  if (alignment == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y);
  if (text.length() > text_width * 2) {
    u8g2Fonts.setFont(u8g2_font_helvB10_te);
    text_width = 42;
    y = y - 3;
  }
  u8g2Fonts.println(text.substring(0, text_width));
  if (text.length() > text_width) {
    u8g2Fonts.setCursor(x, y + h + 15);
    String secondLine = text.substring(text_width);
    secondLine.trim(); // Remove any leading spaces
    u8g2Fonts.println(secondLine);
  }
}
//#########################################################################################
String GetForecastDay(int unix_time) {
  // Returns either '21:12  ' or ' 09:12pm' depending on Units mode
  time_t tm = unix_time;
  struct tm *now_tm = localtime(&tm);
  char output[40], FDay[40];
  if (Units == "M") {
    strftime(output, sizeof(output), "%H:%M %d/%m/%y", now_tm);
    strftime(FDay, sizeof(FDay), "%w", now_tm);
  }
  else {
    strftime(output, sizeof(output), "%I:%M%p %m/%d/%y", now_tm);
    strftime(FDay, sizeof(FDay), "%w", now_tm);
  }
  ForecastDay = weekday_D[String(FDay).toInt()];
  return output;
}

#include "src/weather_conditions.h"

//#########################################################################################
void DisplayConditionsSection(int x, int y, String IconName, bool IconSize) {
  Serial.println("Icon name: " + IconName);
  if      (IconName == "01d" || IconName == "01n")  ClearSky(x, y, IconSize, IconName);
  else if (IconName == "02d" || IconName == "02n")  FewClouds(x, y, IconSize, IconName);
  else if (IconName == "03d" || IconName == "03n")  ScatteredClouds(x, y, IconSize, IconName);
  else if (IconName == "04d" || IconName == "04n")  BrokenClouds(x, y, IconSize, IconName);
  else if (IconName == "09d" || IconName == "09n")  ChanceRain(x, y, IconSize, IconName);
  else if (IconName == "10d" || IconName == "10n")  Rain(x, y, IconSize, IconName);
  else if (IconName == "11d" || IconName == "11n")  Tstorms(x, y, IconSize, IconName);
  else if (IconName == "13d" || IconName == "13n")  Snow(x, y, IconSize, IconName);
  else if (IconName == "50d" || IconName == "50n")  Mist(x, y, IconSize, IconName);
  else                                              Nodata(x, y, IconSize, IconName);
}
//#########################################################################################
void InitialiseDisplay() {
  display.init(115200, false, 50, false);    // The 3-color display doesn't have a fast local screen refresh function, so the false flag is used for initial. Otherwise, the clean_start variable should be used
  SPI.end();
  SPI.begin(EPD_SCL, EPD_MISO, EPD_SDA, EPD_CS);
  display.setRotation(1);                    // Use 1 or 3 for landscape modes
  u8g2Fonts.begin(display);                  // connect u8g2 procedures to Adafruit GFX
  u8g2Fonts.setFontMode(1);                  // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);             // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK); // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE); // apply Adafruit GFX color
  u8g2Fonts.setFont(u8g2_font_helvB10_te);   // Explore u8g2 fonts from here: https://github.com/olikraus/u8g2/wiki/fntlistall
}

