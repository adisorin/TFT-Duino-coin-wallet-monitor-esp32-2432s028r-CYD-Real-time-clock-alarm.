#include <Arduino.h>
#include <driver/ledc.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <EEPROM.h>
#include <time.h>
/// *** Board: ESP32-2432S028 CYD *** ///
// ==== WiFi ====
WiFiMulti wifiMulti;

// ==== Duino-Coin ====
const char* ducoUser = "my_cool_adis";// user duco cange"*******"
String apiUrl = String("https://server.duinocoin.com/balances/") + ducoUser;
String minersApiUrl = String("https://server.duinocoin.com/users/") + ducoUser;

// ==== TICKER PRETURI (BUSTIERA) ====
String pricesText = "* * * * PRICE TICKER * * * * ";
int tickerX = 320;              // porneste din dreapta ecranului
unsigned long lastTickerScroll = 0;

// ==== PINURI ====
#define PIN_LDR        34    // ADC1_CH6

// ==== ADC ====
const int ADC_SAMPLES = 8;

// ==== DEFINIRE CULORI ILI9341 (RGB565) ====
#define ILI9341_BLACK       0x0000
#define ILI9341_WHITE       0xFFFF
#define ILI9341_RED         0xF800
#define ILI9341_GREEN       0x07E0
#define ILI9341_BLUE        0x001F
#define ILI9341_CYAN        0x07FF
#define ILI9341_MAGENTA     0xF81F
#define ILI9341_YELLOW      0xFFE0
#define ILI9341_ORANGE      0xFD20
#define ILI9341_LIGHTGREY   0xC618
#define ILI9341_DARKGREY    0x7BEF
#define ILI9341_GREY        0x8410
#define ILI9341_NAVY        0x000F
#define ILI9341_MAROON      0x7800
#define ILI9341_PURPLE      0x780F
#define ILI9341_OLIVE       0x7BE0
#define ILI9341_LIGHTGREY   0xC618

//////////////////////////////VOLUM AUDIO////////////////////////
#define ADDR_ALARM_VOL  32  // adresa pentru volum
int alarmVolume = 15;      // volum implicit (0-255)

#define VOL_DOWN_X 15
#define VOL_DOWN_Y 152
#define VOL_DOWN_W 45
#define VOL_DOWN_H 30

#define VOL_UP_X 260
#define VOL_UP_Y 152
#define VOL_UP_W 45
#define VOL_UP_H 30
/////////////////////////////////////////////////////////////////

// ==== PWM (ESP-IDF style) - pentru backlight (canal 0) ====
#define LEDC_CHANNEL_BACKLIGHT   LEDC_CHANNEL_0
#define LEDC_TIMER     LEDC_TIMER_0
#define LEDC_MODE      LEDC_HIGH_SPEED_MODE
#define LEDC_RES       LEDC_TIMER_8_BIT
#define LEDC_FREQ      10000

// ==== Alarm (buzzer) - folosim un alt canal PWM ====
//#define ALARM_PIN 26               // IO26 buzzer
#define DAC_AUDIO_PIN 26

// --- TFT (ILI9341) pins ---
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  -1
#define PIN_BACKLIGHT  21    // Pin PWM backlight

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

// --- Touch (XPT2046) pins ---
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
SPIClass touchscreenSPI(HSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

// ==== NTP ====
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600;
const int daylightOffset_sec = 0;
time_t lastSynced = 0;
unsigned long lastMillis = 0;
bool timeValid = false;

// ==== Timere ====
unsigned long lastWifiCheck   = 0;
unsigned long lastSyncAttempt = 0;
unsigned long lastApiCheck    = 0;

// ==== EEPROM ====
#define EEPROM_SIZE 128
#define ADDR_FLAG   0
#define ADDR_MINX   4
#define ADDR_MAXX   8
#define ADDR_MINY   12
#define ADDR_MAXY   16

// adrese pentru alarmă
#define ADDR_ALARM_H 20
#define ADDR_ALARM_M 24
#define ADDR_ALARM_EN 28

// ==== Touch calibrare ====
int TS_MINX = 200, TS_MAXX = 3800;
int TS_MINY = 200, TS_MAXY = 3800;
bool calibrated = false;

// ==== Struct TouchPoint ====
struct TouchPoint {
  uint16_t x;
  uint16_t y;
  bool touched;
};

// ====================== FUNCȚII ======================

// ==== smoothing ====(factor de netezire al nivelului de lumină)
float smoothBrightness = 0.5;
const float SMOOTH_FACTOR = 0.03;

// ==== auto-calibrare LDR ====
int ldrMin = 4095;
int ldrMax = 0;

// ==== Citire medie LDR ====
int readLdrAvg() {
  long sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(PIN_LDR);
    delay(5);
  }
  return (int)(sum / ADC_SAMPLES);
}

// ==== Citire Touch ====
bool readTouchPoint(TouchPoint &p) {
  if (!ts.touched()) return false;
  TS_Point pt = ts.getPoint();

  if (!calibrated) {
    p.x = pt.x;
    p.y = pt.y;
  } else {
    p.x = map(pt.x, TS_MINX, TS_MAXX, 0, 320);
    p.y = map(pt.y, TS_MINY, TS_MAXY, 0, 240);
  }
  p.touched = true;
  // Debounce mic
  delay(75); //50
  return true;
}
// ==== Calibrare touch ====
void calibrareTouch() {
  int16_t minX = 9999, maxX = 0, minY = 9999, maxY = 0;

  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_RED);
  tft.setTextSize(2);
  int puncte[4][2] = {
    {2, 3},
    {318, 3},
    {2, 236},
    {318, 236}
  };

  for (int i = 0; i < 4; i++) {
    tft.fillScreen(ILI9341_BLACK);
    //desenConturEcran();
    tft.setCursor(60, 100);
    tft.println("Atinge punctul!");
    tft.fillCircle(puncte[i][0], puncte[i][1], 2, ILI9341_RED);

    bool ok = false;
    while (!ok) {
      if (ts.touched()) {
        TS_Point p = ts.getPoint();
        minX = (p.x < minX) ? p.x : minX;
        maxX = (p.x > maxX) ? p.x : maxX;
        minY = (p.y < minY) ? p.y : minY;
        maxY = (p.y > maxY) ? p.y : maxY;
        ok = true;
        delay(500);
      }
    }
  }

  TS_MINX = minX;
  TS_MAXX = maxX;
  TS_MINY = minY;
  TS_MAXY = maxY;
  calibrated = true;

  int flag = 1234;
  EEPROM.put(ADDR_FLAG, flag);
  EEPROM.put(ADDR_MINX, TS_MINX);
  EEPROM.put(ADDR_MAXX, TS_MAXX);
  EEPROM.put(ADDR_MINY, TS_MINY);
  EEPROM.put(ADDR_MAXY, TS_MAXY);
  EEPROM.commit();

  tft.fillScreen(ILI9341_BLACK);
  desenConturEcran();
  tft.setTextColor(ILI9341_GREEN);
  tft.setCursor(40, 100);
  tft.println("Calibrare terminata!");
  delay(100);
}

// ==== WiFi scan results buffer ====
int scanCount = 0;
#define MAX_SCAN_SHOW 7

// ====================== ALARMA ======================
int alarmHour = 7;
int alarmMinute = 0;
bool alarmEnabled = true;
bool alarmActive = false;
unsigned long lastBeep = 0;

// buton alarm pe UI
#define ALARM_BTN_X 87
#define ALARM_BTN_Y 155
#define ALARM_BTN_W 145
#define ALARM_BTN_H 30

// --- Buton 1 ---
bool butonVizibil = true;
unsigned long momentAscuns = 0;
#define BTN_X 100
#define BTN_Y 190
#define BTN_W 120
#define BTN_H 40

// --- Buton 2 schimbare pagină ---
#define BTN2_X 235
#define BTN2_Y 190
#define BTN2_W 75
#define BTN2_H 40

// --- Buton 3 WiFi ---
#define BTN3_X 10
#define BTN3_Y 190
#define BTN3_W 75
#define BTN3_H 40

int paginaCurenta = 1; // 1 = sold, 2 = minerii, 3 = WiFi setup

// ==== Desenare butoane ====
void desenButon() {
  if (butonVizibil) {
    tft.fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 15, ILI9341_BLUE);
    tft.setCursor(BTN_X + 20, BTN_Y + 12);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.print("PAY-15'");
  } else {
    tft.fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 15, ILI9341_GREEN);
    tft.setCursor(BTN_X + 20, BTN_Y + 12);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_BLACK);
    tft.print("GiftPay");
  }
}
void desenButonPagina() {
  tft.fillRoundRect(BTN2_X, BTN2_Y, BTN2_W, BTN2_H, 15, ILI9341_ORANGE);
  tft.setCursor(BTN2_X + 2, BTN2_Y + 12);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_BLACK);
  if (paginaCurenta == 1) {
    tft.print("miners");
  } else {
    tft.print("wallet");
  }
}
void desenButonWifi() {
  tft.fillRoundRect(BTN3_X, BTN3_Y, BTN3_W, BTN3_H, 15, ILI9341_MAGENTA);
  tft.setCursor(BTN3_X + 12, BTN3_Y + 12);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_BLACK);
  tft.print("WiFi");
}
////////////////////////////BUTOANE VOLUM/////////////////////////////////
void afiseazaButoaneVolum() {
  // "-" (micșorare)
  tft.fillRoundRect(VOL_DOWN_X, VOL_DOWN_Y, VOL_DOWN_W, VOL_DOWN_H, 8, ILI9341_BLUE);
  tft.setTextSize(3);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(VOL_DOWN_X + 8, VOL_DOWN_Y + 5);
  tft.print("V-");

  // "+" (mărire)
  tft.fillRoundRect(VOL_UP_X, VOL_UP_Y, VOL_UP_W, VOL_UP_H, 8, ILI9341_BLUE);
  tft.setTextSize(3);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(VOL_UP_X + 8, VOL_UP_Y + 5);
  tft.print("V+");

  // text volum numeric
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tft.setCursor(170, 15);
  tft.fillRect(165, VOL_UP_Y - 145, 90, 25, ILI9341_BLACK);//BLUE PENTRU POZITIONARE
  tft.printf("Vol:%d", alarmVolume);
}
///////////////////////////////////////////////////////////////////////////
// ==== Desenează contur ecran ====
void desenConturEcran() {
  tft.drawRect(0, 0, tft.width(), tft.height(), ILI9341_BLUE);
  afiseazaNivelWiFi(273, 24);
}

// ==== Afiseaza ceas ====
void actualizeazaTimpLocal() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    lastSynced = mktime(&timeinfo);
    lastMillis = millis();
    timeValid = true;
  }
}
///////////////////////////////ORA   ORA   ORA   ORA////////////////
void afiseazaCeas() {
  if (!timeValid) {
    tft.setCursor(80, 100);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
    tft.println("No Time Sync!");
    return;
  }
  time_t now = lastSynced + (millis() - lastMillis) / 1000;
  struct tm *timeinfo = localtime(&now);
  char timp[16];
  strftime(timp, sizeof(timp), "%H:%M:%S", timeinfo);
  tft.setCursor(20, 105);
  tft.setTextSize(6);
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tft.print(timp);
}

// ==== Afiseaza info alarma (pe pagina principala) ====
void afiseazaInfoAlarma() {
  // zona rezervata: x=10..160, y=100..130
  tft.fillRect(ALARM_BTN_X, ALARM_BTN_Y, ALARM_BTN_W, ALARM_BTN_H, ILI9341_BLACK);
  tft.drawRoundRect(ALARM_BTN_X, ALARM_BTN_Y, ALARM_BTN_W, ALARM_BTN_H, 6, ILI9341_WHITE);
  tft.setCursor(ALARM_BTN_X + 6, ALARM_BTN_Y + 7);
  tft.setTextSize(2);
  if (alarmEnabled) tft.setTextColor(ILI9341_BLUE);
  else tft.setTextColor(ILI9341_RED);

  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", alarmHour, alarmMinute);
  tft.print("AL ");
  tft.print(buf);

// indicator ON/OFF la dreapta — culoare în funcție de volum
  tft.setCursor(ALARM_BTN_X + ALARM_BTN_W - 40, ALARM_BTN_Y + 7);
  tft.setTextSize(2);

  if  (alarmVolume == 0) {
    tft.setTextColor(ILI9341_RED); // Volum = 0 → roșu
    tft.print("OFF");
  } else {
    tft.setCursor(ALARM_BTN_X + ALARM_BTN_W - 35, ALARM_BTN_Y + 7);
    tft.setTextColor(ILI9341_GREEN); // Volum > 0 → verde
    tft.print("ON");
  }
}
////////////////////////////  indicator wifi  ////////////////////

// ==== Afișează indicator WiFi discret ====
void afiseazaNivelWiFi(int x, int y) {
  // Poziția de start (x, y) este colțul stânga-jos al indicatorului
  int dbm = WiFi.RSSI();
  int nivel = 0;

  if (WiFi.status() != WL_CONNECTED) {
    nivel = 0;
  } else if (dbm > -50) nivel = 5;
  else if (dbm > -60) nivel = 4;
  else if (dbm > -70) nivel = 3;
  else if (dbm > -80) nivel = 2;
  else if (dbm > -90) nivel = 1;
  else nivel = 0;

  // dimensiuni bare
  int latime = 6;
  int spatiu = 3;
  int inaltimeMax = 20;

  // șterge zona precedentă
  tft.fillRect(x, y - inaltimeMax, 5 * (latime + spatiu), inaltimeMax + 2, ILI9341_BLACK);

  // desenează barele
  for (int i = 0; i < 5; i++) {
    int h = (i + 1) * (inaltimeMax / 5);
    uint16_t culoare = (i < nivel) ? ILI9341_GREEN : ILI9341_DARKGREY;
    tft.fillRect(x + i * (latime + spatiu), y - h, latime, h, culoare);
  }
}
////////////////////////////End indicator wifi/////////////////////

////////Generare tonuri DAC inloc de PWM
void beepDAC(int freq, int duration) {
  int delayMicro = 1000000 / (freq * 2);
  unsigned long tEnd = millis() + duration;

  while (millis() < tEnd) {
    dacWrite(DAC_AUDIO_PIN, alarmVolume); // folosește volumul setat
    delayMicroseconds(delayMicro);
    dacWrite(DAC_AUDIO_PIN, 0);
    delayMicroseconds(delayMicro);
  }
  dacWrite(DAC_AUDIO_PIN, 0);
}
// ==== Minerii activi ====
void afiseazaMineriActivi() {
  if (WiFi.status() != WL_CONNECTED) {
    tft.fillScreen(ILI9341_BLACK);
    desenConturEcran();
    tft.setTextColor(ILI9341_RED);
    tft.setTextSize(2);
    tft.setCursor(80, 100);
    tft.println("No WiFi!");
    return;
  }

  HTTPClient http;
  http.begin(minersApiUrl);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (!err) {
      JsonArray miners = doc["result"]["miners"];
      int y = 15;
      tft.setTextSize(1);

      int totalMineri = miners.size(); // 🆕 număr total mineri activi
      // Afișăm titlul cu numărul de mineri
      tft.setCursor(180, y);
      tft.setTextSize(2);
      tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
      tft.print(" ");
      tft.println(totalMineri);
      y += 25;

      if (miners.size() == 0) {
        tft.setCursor(10, y);
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
        tft.println("Niciun miner activ");
      } else {
        for (int i = 0; i < miners.size() && i < 20; i++) {
          String id = miners[i]["identifier"].as<String>();
          float hashrate = miners[i]["hashrate"].as<float>();
          String pool = miners[i]["pool"].as<String>(); // 🆕 extragem piscina

          tft.setCursor(10, y);
          tft.setTextSize(1);
          tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);

          // Afișează minerul, viteza și pool-ul pe o singură linie
          tft.print(id);
          tft.print(": ");
          tft.print(hashrate, 1);
          tft.print(" H/s");

          tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
          tft.print("  ");
          tft.println(pool);

          y += 12;
        }
      }

    } 

  http.end();
}
}
//Bustiera
void updatePricesFromServer() {

  if (WiFi.status() != WL_CONNECTED) {
    pricesText = "No WiFi connection";
    tickerX = 320;
    return;
  }

  HTTPClient http;
  String pricesUrl = String("https://server.duinocoin.com/v2/users/") + ducoUser;
  http.begin(pricesUrl);

  int httpCode = http.GET();

  // Dacă serverul nu răspunde corect
  if (httpCode != 200) {
    pricesText = "No server data";
    tickerX = 320;
    http.end();
    return;
  }

  String payload = http.getString();
  JsonDocument doc;

  // Dacă JSON invalid
  if (deserializeJson(doc, payload) != DeserializationError::Ok) {
    pricesText = "No server data";
    tickerX = 320;
    http.end();
    return;
  }

  JsonObject prices = doc["result"]["prices"];

  // Dacă nu există prețuri
  if (prices.isNull() || prices.size() == 0) {
    pricesText = "No server data";
    tickerX = 320;
    http.end();
    return;
  }

  // Dacă totul e ok → afișăm prețurile
  pricesText = "";
  for (JsonPair kv : prices) {
    float price = kv.value().as<float>();
    if (price > 0.0f) {
      pricesText += String(kv.key().c_str());
      pricesText += ": $";
      pricesText += String(price, 8);
      pricesText += "   ";
    }
  }

  tickerX = 320;
  http.end();
}

//bustiera
void drawTicker() {
  int tickerY = 80;
  int tickerH = 18;
  int tickerMargin = 15;
  int tickerWidth = 320 - 2 * tickerMargin;
  int rightLimit = tickerMargin + tickerWidth;

  // Bustiera
  tft.fillRect(rightLimit - 2, tickerY, 2, tickerH, ILI9341_BLACK);
  tft.setTextSize(2);
  if (pricesText == "No server data" || pricesText == "No WiFi connection")
      tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
  else
      tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setTextWrap(false);

  // Scroll
  if (millis() - lastTickerScroll > 30) {
    tickerX -= 2;
    lastTickerScroll = millis();
  }

  int charWidth = 12;
  int textWidth = pricesText.length() * charWidth;

  // Reset când iese complet din stânga
  if (tickerX + textWidth < tickerMargin) {
    tickerX = rightLimit;
  }

  // === CLIPARE STÂNGA + DREAPTA ===
  int x = tickerX;

  for (uint16_t i = 0; i < pricesText.length(); i++) {

    if (x + charWidth > tickerMargin && x < rightLimit) {
      tft.setCursor(x, tickerY + 1);
      tft.print(pricesText[i]);
    }

    x += charWidth;
  }
}

////////////////////////////////// ANIMATIE DUINO-COIN ////////////////////

void animatieLogoDuino() {
  tft.fillScreen(ILI9341_BLACK);
  desenConturEcran();

  // fade-in backlight (efect frumos)
  for (int i = 0; i <= 255; i += 10) {
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_BACKLIGHT, i);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_BACKLIGHT);
    delay(10);
  }

  // logo simplu Duino-Coin
  for (int r = 10; r <= 80; r += 10) {
    tft.drawCircle(160, 120, r, ILI9341_BLUE);
    delay(60);
  }

  // scriem textul central
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextSize(4);
  tft.setCursor(40, 70);
  tft.print("DUINO-COIN");

  delay(500);

  // efect mic de “pulse”
  for (int k = 0; k < 3; k++) {
    tft.fillCircle(160, 120, 20, ILI9341_ORANGE);
    delay(150);
    tft.fillCircle(160, 120, 20, ILI9341_BLACK);
    tft.drawCircle(160, 120, 20, ILI9341_YELLOW);
    delay(150);
  }

  // text “Powered by ESP32”
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_MAGENTA);
  tft.setCursor(100, 170);
  tft.print("By Sor Adi");
  delay(1500);

  // fade-out backlight (opțional)
  for (int i = 255; i >= 80; i -= 10) {
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_BACKLIGHT, i);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_BACKLIGHT);
    delay(15);
  }
  // curățăm ecranul
  tft.fillScreen(ILI9341_BLACK);
  desenConturEcran();
  delay(150);
    // logo simplu Duino-Coin
  for (int r = 10; r <= 80; r += 10) {
    tft.drawCircle(160, 120, r, ILI9341_BLUE);
    delay(500);
  }
  tft.fillScreen(ILI9341_BLACK);
  desenConturEcran();
  afiseazaCeas();
}
///////////////////////////////// ANIMATIE END //////////////////////////////
// ==== Afiseaza lista WiFi (pagina 3) ====
void afiseazaListaWiFi() {
  tft.fillScreen(ILI9341_BLACK);
  desenConturEcran();
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 10);
  tft.println("Scanare retele...");

  // Scan pe loc (sincron)
  int n = WiFi.scanNetworks();
  scanCount = (n > 0) ? n : 0;

  tft.fillScreen(ILI9341_BLACK);
  desenConturEcran();
  tft.setTextSize(2);
  if (scanCount == 0) {
    tft.setCursor(20, 100);
    tft.setTextColor(ILI9341_RED);
    tft.println("Nicio retea gasita");
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(15, 150);
    tft.setTextSize(2);
    tft.println("Apasa WiFi pentru re-scan");
    return;
  }
  int show = min(scanCount, MAX_SCAN_SHOW);
for (int i = 0; i < show; i++) {
    int y = 20 + i * 25;
    tft.setCursor(10, y);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_CYAN);
    
    String ssid = WiFi.SSID(i);
    if (ssid.length() > 22) ssid = ssid.substring(0, 22);
    
    tft.print(String(i + 1) + ". ");
    tft.print(ssid);

    // Citim puterea semnalului pentru rețeaua "i" și o salvăm în "dbm"
    int dbm = WiFi.RSSI(i); 

    // ACUM FOLOSIM "dbm" (de exemplu, să îl afișăm pe ecran)
    tft.setCursor(180, y); // punem cursorul undeva înainte de textul "encrypt"
    tft.setTextColor(ILI9341_WHITE);
    tft.print(String(dbm) + "dBm"); // Afișează valoarea (ex: -65dBm)

    // Partea cu criptarea
    String enc = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "enrypt";
    tft.setCursor(220, y);
    tft.setTextColor(ILI9341_YELLOW);
    tft.print(enc);
}

  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(75, 5);
  tft.println("Atinge pentru parola");
}
// ==== Tastatura virtuală cu Caps Lock (fără flicker) ====
// Returnează textul introdus (parola) sau "" dacă utilizatorul a apăsat EXIT.
String tastaturaVirtuala(String header) {

  // --- DEFINIȚII DE BAZĂ ---
  // Matricea de taste (4 rânduri × 12 coloane)
  const char keys[4][12] = {
    {'1','2','3','4','5','6','7','8','9','0','-','='},
    {'q','w','e','r','t','y','u','i','o','p','[',']'},
    {'a','s','d','f','g','h','j','k','l',';','\'','\\'},
    {'z','x','c','v','b','n','m',',','.','/','@','#'}
  };
  const int cols = 12;  // număr coloane
  const int rows = 4;   // număr rânduri

  String parola = "";   // textul introdus
  bool done = false;    // dacă s-a apăsat OK
  bool caps = false;    // stare CAPS LOCK (inițial litere mici)

  // --- DIMENSIUNI ȘI POZIȚII ---
  const int keyW = 22;        // lățime tastă
  const int keyH = 28;        // înălțime tastă
  const int startX = 6;       // coloană de start
  const int startY = 60;      // rând de start pentru tastatură
  const int fx = 6;           // coordonata X pentru butoane funcționale
  const int fy = startY + rows * (keyH + 6) + 6; // coordonata Y pentru butoane funcționale
  // --- DESENARE ECRAN INIȚIALĂ ---
  tft.fillScreen(ILI9341_BLACK);
  desenConturEcran();
  // Titlu / antet (textul trecut ca parametru)
  tft.setCursor(10, 8);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.print(header);
  // Zonă pentru afișarea parolei (golită la început)
  tft.fillRect(10, 30, 300, 24, ILI9341_BLACK);
  tft.setCursor(10, 30);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_GREEN);
  // --- FUNCȚIE INTERNĂ: desenare tastatură în funcție de starea CAPS ---
  auto drawKeyboard = [&](bool capsOn){
    for (int r = 0; r < rows; r++) {
      for (int c = 0; c < cols; c++) {
        int bx = startX + c * (keyW + 4);
        int by = startY + r * (keyH + 6);
        // fundal negru + contur galben
        tft.fillRoundRect(bx, by, keyW, keyH, 4, ILI9341_BLACK);
        tft.drawRoundRect(bx, by, keyW, keyH, 4, ILI9341_YELLOW);
        char ch = keys[r][c];
        // dacă este activ CAPS, transformăm literele în majuscule
        if (capsOn && isalpha(ch)) ch = toupper(ch);
        // scriem caracterul
        tft.setCursor(bx + 5, by + 6);
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE);
        tft.print(String(ch));
      }
    }
  };
  drawKeyboard(false); // desen inițial (litere mici)
  // --- BUTOANE FUNCȚIONALE (DEL, SPACE, CAPS, OK, EXIT) ---
  // DEL
  tft.fillRoundRect(fx, fy, 80, 30, 6, ILI9341_ORANGE);
  tft.setCursor(fx + 25, fy + 8);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_BLACK);
  tft.print("DEL");

  // SPACE
  tft.fillRoundRect(fx + 90, fy, 100, 30, 6, ILI9341_BLUE);
  tft.setCursor(fx + 110, fy + 8);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.print("SPACE");

  // CAPS LOCK
  tft.fillRoundRect(fx + 200, fy, 60, 30, 6, ILI9341_DARKGREY);
  tft.setCursor(fx + 215, fy + 8);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.print("CAP");

  // OK
  tft.fillRoundRect(fx + 270, fy, 38, 30, 6, ILI9341_GREEN);
  tft.setCursor(fx + 280, fy + 8);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_BLACK);
  tft.print("OK");

  // EXIT
  tft.fillRoundRect(fx, fy + 40, 100, 30, 6, ILI9341_RED);
  tft.setCursor(fx + 18, fy + 48);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.print("EXIT");

  // --- FUNCȚIE INTERNĂ: mapare coordonate touch la coordonate ecran ---
  auto mapPoint = [&](TS_Point p)->TouchPoint {
    TouchPoint out;
    if (!calibrated) {
      out.x = p.x;
      out.y = p.y;
    } else {
      out.x = map(p.x, TS_MINX, TS_MAXX, 0, 320);
      out.y = map(p.y, TS_MINY, TS_MAXY, 0, 240);
    }
    out.touched = true;
    return out;
  };

  // --- BUCLA PRINCIPALĂ DE CITIRE TOUCH ---
  while (!done) {

    // actualizez zona parolei (doar acea parte, nu tot ecranul!)
    tft.fillRect(10, 30, 300, 24, ILI9341_BLACK);
    tft.setCursor(10, 30);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_GREEN);
    String show = parola;
    if (show.length() > 20) show = show.substring(show.length() - 20);
    tft.print(show);

    // citire touch
    if (ts.touched()) {
      TS_Point rawp = ts.getPoint();
      TouchPoint p = mapPoint(rawp);
      int x = p.x, y = p.y;
      // --- Buton DEL ---
      if (x >= fx && x <= fx + 80 && y >= fy && y <= fy + 30) {
        beepDAC(2000, 40);
        if (parola.length() > 0) parola.remove(parola.length() - 1);
        while (ts.touched()) delay(5); // aștept eliberare
        continue;
      }
      // --- Buton SPACE ---
      if (x >= fx + 90 && x <= fx + 190 && y >= fy && y <= fy + 30) {
        beepDAC(2000, 40);
        parola += ' ';
        while (ts.touched()) delay(5);
        continue;
      }
      // --- Buton CAPS LOCK ---
      if (x >= fx + 200 && x <= fx + 260 && y >= fy && y <= fy + 30) {
        beepDAC(2000, 40);
        // schimb stare CAPS
        caps = !caps;
        // schimb culoarea butonului (galben = activ)
        uint16_t color = caps ? ILI9341_YELLOW : ILI9341_DARKGREY;
        tft.fillRoundRect(fx + 200, fy, 60, 30, 6, color);
        tft.setCursor(fx + 210, fy + 8);
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_BLACK);
        tft.print("CAP");
        // redesenez doar caracterele tastelor (nu tot ecranul)
        drawKeyboard(caps);
        while (ts.touched()) delay(5);
        continue;
      }
      // --- Buton OK ---
      if (x >= fx + 270 && x <= fx + 315 && y >= fy && y <= fy + 30) {
        beepDAC(2000, 40);
        done = true;
        while (ts.touched()) delay(5);
        break;
      }
      // --- Buton EXIT ---
      if (x >= fx && x <= fx + 100 && y >= fy + 40 && y <= fy + 70) {
        beepDAC(2000, 40);
        while (ts.touched()) delay(5);
        return ""; // ieșire fără rezultat
      }
      // --- Taste normale (litere/simboluri) ---
      for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
          int bx = startX + c * (keyW + 4);
          int by = startY + r * (keyH + 6);
          if (x >= bx && x <= bx + keyW && y >= by && y <= by + keyH) {
            beepDAC(2000, 40);
            char ch = keys[r][c];
            if (caps && isalpha(ch)) ch = toupper(ch);
            parola += ch;

            // feedback vizual: scurt highlight alb
            tft.fillRoundRect(bx, by, keyW, keyH, 4, ILI9341_WHITE);
            tft.setCursor(bx + 5, by + 6);
            tft.setTextSize(2);
            tft.setTextColor(ILI9341_BLACK);
            tft.print(String(ch));
            delay(100);

            // restaurare tasta
            tft.fillRoundRect(bx, by, keyW, keyH, 4, ILI9341_BLACK);
            tft.drawRoundRect(bx, by, keyW, keyH, 4, ILI9341_YELLOW);
            tft.setCursor(bx + 5, by + 6);
            tft.setTextSize(2);
            tft.setTextColor(ILI9341_WHITE);
            tft.print(String(ch));

            while (ts.touched()) delay(5); // aștept eliberare
          }
        }
      }
    }

    delay(10); // mică pauză pentru stabilitate touch
  }
  return parola;
}
// ==== Conectare la SSID cu parola ====
void conectareWiFi(const String &ssid, const String &parola) {
  tft.fillScreen(ILI9341_BLACK);
  desenConturEcran();
  tft.setCursor(10, 30);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.print("Conectare la:");
  tft.setCursor(10, 60);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_CYAN);
  tft.println(ssid);

  WiFi.begin(ssid.c_str(), parola.c_str());
  unsigned long start = millis();
  tft.setCursor(10, 100);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_YELLOW);
  tft.print("In curs...");
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    tft.print(".");
    delay(1000);
  }
  tft.fillRect(10, 100, 300, 40, ILI9341_BLACK);
  tft.setCursor(10, 100);
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_GREEN);
    tft.println("Conectat!");
    beepDAC(1000, 50);  // ton 1
    delay(10);
    beepDAC(2000, 50);  // ton 1
    delay(10);
    beepDAC(2500, 50);  // ton 2
    
    delay(1500);
  } else {
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_RED);
    tft.println("Eroare conectare");
    beepDAC(300, 1000);
    delay(1500);
  }
  // După încercare, revenim la pagina principală (1)
  paginaCurenta = 1;
  tft.fillScreen(ILI9341_BLACK);
  desenConturEcran();
  desenButon();
  desenButonPagina();
  desenButonWifi();
}
// ==== Verificare touch ====
void verificaTouch() {
  TouchPoint p;
  if (readTouchPoint(p) && p.touched && calibrated) {
    beepDAC(2000, 40);  // bip scurt la orice atingere
    // Dacă alarma e activă, orice atingere oprește alarma
    if (alarmActive) {
      // oprim sunetul și afisarea
      alarmActive = false;
      dacWrite(DAC_AUDIO_PIN, 0); // oprit la pornire
      tft.fillRect(0, 180, 320, 60, ILI9341_BLACK);
      desenButon();
      desenButonPagina();
      desenButonWifi();
      delay(300);
      return;
    }
    // Buton 1
    if (p.x > BTN_X && p.x < (BTN_X + BTN_W) &&
        p.y > BTN_Y && p.y < (BTN_Y + BTN_H)) {
      beepDAC(2000, 40);

      butonVizibil = !butonVizibil;
      momentAscuns = millis();
      desenButon();
      desenButonPagina();
      desenButonWifi();
      delay(700);
      return;
    }
    // Buton 2 (pagina)
    if (p.x > BTN2_X && p.x < (BTN2_X + BTN2_W) &&
        p.y > BTN2_Y && p.y < (BTN2_Y + BTN2_H)) {
      beepDAC(2000, 40);

      paginaCurenta = (paginaCurenta == 1 ? 2 : 1);
      tft.fillScreen(ILI9341_BLACK);
      desenConturEcran();
      desenButon();
      desenButonPagina();
      desenButonWifi();
      lastApiCheck = 0;
      delay(20);
      return;
    }
    // Buton 3 (WiFi)
    if (p.x > BTN3_X && p.x < (BTN3_X + BTN3_W) &&
        p.y > BTN3_Y && p.y < (BTN3_Y + BTN3_H)) {
      beepDAC(2000, 40);

      paginaCurenta = 3;
      tft.fillScreen(ILI9341_BLACK);
      desenConturEcran();
      afiseazaListaWiFi();
      delay(20);
      return;
    }
    // Daca suntem in pagina WiFi, verificam selectie retea
    if (paginaCurenta == 3) {
      if (scanCount > 0) {
        int show = min(scanCount, MAX_SCAN_SHOW);
        for (int i = 0; i < show; i++) {
          int yStart = 20 + i * 25;
          if (p.y >= yStart && p.y <= yStart + 20) {
            String ssid = WiFi.SSID(i);
            // cer parola
            String header = String("SSID: ") + ssid;
            String parola = tastaturaVirtuala(header);
            if (parola.length() > 0) {
              conectareWiFi(ssid, parola);
            } else {
              // utilizator a ales EXIT -> revenim la lista
              afiseazaListaWiFi();
            }
            return;
          }
        }
        // daca apasa jos de lista (de ex. pentru re-scan), detectam
        if (p.y > 200) {
          // re-scan
          afiseazaListaWiFi();
          return;
        }
      } else {
        // daca nu sunt retele, o atingere re-scan
        afiseazaListaWiFi();
        return;
      }
    }
    // Daca suntem in pagina principala, verificam buton ALARM
    if (paginaCurenta == 1) {
///////////////////////////////////VOLUM SCALAT//////////////////////
      // Buton volum +
      if (p.x >= VOL_UP_X && p.x <= VOL_UP_X + VOL_UP_W &&
          p.y >= VOL_UP_Y && p.y <= VOL_UP_Y + VOL_UP_H) {
        alarmVolume = min(25*4, alarmVolume + 1);
        EEPROM.put(ADDR_ALARM_VOL, alarmVolume);
        EEPROM.commit();
        afiseazaButoaneVolum();
        delay(70);
        return;
      }

      // Buton volum -
      if (p.x >= VOL_DOWN_X && p.x <= VOL_DOWN_X + VOL_DOWN_W &&
          p.y >= VOL_DOWN_Y && p.y <= VOL_DOWN_Y + VOL_DOWN_H) {
        alarmVolume = max(0, alarmVolume - 1);
        EEPROM.put(ADDR_ALARM_VOL, alarmVolume);
        EEPROM.commit();
        afiseazaButoaneVolum();
        delay(70);
        return;
      }
////////////////////////////////////////////////////////////////////////
      if (p.x >= ALARM_BTN_X && p.x <= ALARM_BTN_X + ALARM_BTN_W &&
          p.y >= ALARM_BTN_Y && p.y <= ALARM_BTN_Y + ALARM_BTN_H) {
        // atingere pe zona ALARM -> deschidem tastatura pentru setare HH:MM
        String result = tastaturaVirtuala("Seteaza alarma HH:MM");
        if (result.length() > 0) {
          // incercam parse "HH:MM"
          int hh = -1, mm = -1;
          int colon = result.indexOf(':');
          if (colon > 0) {
            String sH = result.substring(0, colon);
            String sM = result.substring(colon + 1);
            hh = sH.toInt();
            mm = sM.toInt();
          } else {
            // user poate fi tastat fara : (ex "0730" sau "7 30") - incercam fallback
            if (result.length() == 4) {
              hh = result.substring(0,2).toInt();
              mm = result.substring(2,4).toInt();
            }
          }
          if (hh >= 0 && hh < 24 && mm >= 0 && mm < 60) {
            alarmHour = hh;
            alarmMinute = mm;
            alarmEnabled = true;
            // salvam in EEPROM
            EEPROM.put(ADDR_ALARM_H, alarmHour);
            EEPROM.put(ADDR_ALARM_M, alarmMinute);
            EEPROM.put(ADDR_ALARM_EN, alarmEnabled ? 1 : 0);
            EEPROM.commit();
            tft.fillRect(0, 160, 320, 60, ILI9341_BLACK);
            desenButon();
            desenButonPagina();
            desenButonWifi();
            afiseazaInfoAlarma();
            delay(300);
            
            //paginaCurenta = 1;//anulat
            tft.fillScreen(ILI9341_BLACK);
            desenConturEcran();
            desenButon();
            desenButonPagina();
            desenButonWifi();
            afiseazaInfoAlarma();
            lastApiCheck = 0;
            delay(20);
            return;
          } else {
            // eroare input -> afisam mesaj scurt
            tft.fillRect(0, 160, 320, 60, ILI9341_BLACK);
            tft.setCursor(40, 180);
            tft.setTextSize(2);
            tft.setTextColor(ILI9341_RED);
            tft.println("Format invalid!");
            delay(1000);
            // redraw
            //paginaCurenta = 1;//anulat
            tft.fillScreen(ILI9341_BLACK);
            desenConturEcran();
            desenButon();
            desenButonPagina();
            desenButonWifi();
            afiseazaInfoAlarma();
            lastApiCheck = 0;
            delay(20);
          }
        } else {
          // user a iesit la tastatura -> doar revenim
            //paginaCurenta = 1;//anulat
            tft.fillScreen(ILI9341_BLACK);
            desenConturEcran();
            desenButon();
            desenButonPagina();
            desenButonWifi();
            afiseazaInfoAlarma();
            lastApiCheck = 0;
            delay(20);
        }
        return;
      }

      // daca apasa in partea dreapta mica a butonului, schimbam ON/OFF
      int toggleX = ALARM_BTN_X + ALARM_BTN_W - 50;
      int toggleW = 50;
      if (p.x >= toggleX && p.x <= toggleX + toggleW &&
          p.y >= ALARM_BTN_Y && p.y <= ALARM_BTN_Y + ALARM_BTN_H) {
        alarmEnabled = !alarmEnabled;
        EEPROM.put(ADDR_ALARM_EN, alarmEnabled ? 1 : 0);
        EEPROM.commit();
        afiseazaInfoAlarma();
        return;
      }
      afiseazaButoaneVolum();
    }
  }
}
// ====================== ALARMA: control & sunet ======================
void startAlarm() {
  alarmActive = true;
  lastBeep = millis();
  tft.fillRect(0, 180, 320, 60, ILI9341_RED);
  tft.setCursor(50, 200);
  tft.setTextSize(3);
  tft.setTextColor(ILI9341_WHITE);
  tft.print("ALARM!");
}
void stopAlarm() {
  alarmActive = false;
  dacWrite(DAC_AUDIO_PIN, 0);   // oprit
}
void checkAlarm() {
  if (!alarmEnabled || alarmActive == true || !timeValid) return;

  time_t now = lastSynced + (millis() - lastMillis) / 1000;
  struct tm *timeinfo = localtime(&now);

  // declansam la momentul exact (la secunda 0..4 incluse pentru a fi toleranti)
  if (timeinfo->tm_hour == alarmHour && timeinfo->tm_min == alarmMinute && timeinfo->tm_sec < 5) {
    startAlarm();
  }
}
////////////////////////PlayMario/////////////////////////////
// Frecvențele notelor (Hz) – doar principalele note din melodia Super Mario
#define NOTE_B0 31
#define NOTE_C1 33
#define NOTE_CS1 35
#define NOTE_D1 37
#define NOTE_DS1 39
#define NOTE_E1 41
#define NOTE_F1 44
#define NOTE_FS1 46
#define NOTE_G1 49
#define NOTE_GS1 52
#define NOTE_A1 55
#define NOTE_AS1 58
#define NOTE_B1 62
#define NOTE_C2 65
#define NOTE_CS2 69
#define NOTE_D2 73
#define NOTE_DS2 78
#define NOTE_E2 82
#define NOTE_F2 87
#define NOTE_FS2 93
#define NOTE_G2 98
#define NOTE_GS2 104
#define NOTE_A2 110
#define NOTE_AS2 117
#define NOTE_B2 123
#define NOTE_C3 131
#define NOTE_CS3 139
#define NOTE_D3 147
#define NOTE_DS3 156
#define NOTE_E3 165
#define NOTE_F3 175
#define NOTE_FS3 185
#define NOTE_G3 196
#define NOTE_GS3 208
#define NOTE_A3 220
#define NOTE_AS3 233
#define NOTE_B3 247
#define NOTE_C4 262
#define NOTE_CS4 277
#define NOTE_D4 294
#define NOTE_DS4 311
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_FS4 370
#define NOTE_G4 392
#define NOTE_GS4 415
#define NOTE_A4 440
#define NOTE_AS4 466
#define NOTE_B4 494
#define NOTE_C5 523
#define NOTE_CS5 554
#define NOTE_D5 587
#define NOTE_DS5 622
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_FS5 740
#define NOTE_G5 784
#define NOTE_GS5 831
#define NOTE_A5 880
#define NOTE_AS5 932
#define NOTE_B5 988

// Melodia (note)
int marioMelody[] = {
  NOTE_E5, NOTE_E5, 0, NOTE_E5,
  0, NOTE_C5, NOTE_E5, 0,
  NOTE_G5, 0, 0,  0,
  NOTE_G4, 0, 0, 0
};

// Durata fiecărei note (în ms)
int marioDurations[] = {
  150, 150, 150, 150,
  150, 150, 150, 150,
  300, 150, 150, 150,
  300, 150, 150, 150
};

const int marioLength = sizeof(marioMelody) / sizeof(int);

void playMario() {
  if (!alarmActive) return; // verificăm alarma

  for (int i = 0; i < marioLength; i++) {
    int note = marioMelody[i];
    int duration = marioDurations[i];
    if (note == 0) { // pauză
      dacWrite(DAC_AUDIO_PIN, 0);
      delay(duration);
    } else {
      int delayMicro = 1000000 / (note * 2); // jumătate de perioadă
      unsigned long tEnd = millis() + duration;
      
      while (millis() < tEnd) {
        dacWrite(DAC_AUDIO_PIN, alarmVolume); // folosește volumul setat
        delayMicroseconds(delayMicro);
        dacWrite(DAC_AUDIO_PIN, 0);         // semnal jos
        delayMicroseconds(delayMicro);
      }
    }
    dacWrite(DAC_AUDIO_PIN, 0);
    delay(20); // mică pauză între note
  }
}
///////////////////////////////MARIO END////////////////////////////////
// ====================== SETUP ======================
void setup() {
  Serial.begin(115200);
  delay(50);
    // 1. Declarăm o variabilă care să stocheze flag-ul citit (de tip int sau uint16_t ca să suporte 1234)
  int flagValue = 0;
  EEPROM.get(ADDR_FLAG, flagValue); 

  // 2. Acum comparăm variabila corectă
  if (flagValue == 1234) {
      EEPROM.get(ADDR_ALARM_VOL, alarmVolume);
      
      // Constrângem volumul între 10 și 255 în siguranță
      if (alarmVolume < 10) {
          alarmVolume = 180;
      }
  }

  // Config PWM timer (folosit și pentru backlight)
  ledc_timer_config_t ledc_timer = {
      .speed_mode = LEDC_MODE,
      .duty_resolution = LEDC_RES,
      .timer_num = LEDC_TIMER,
      .freq_hz = LEDC_FREQ,
      .clk_cfg = LEDC_AUTO_CLK,
      .deconfigure = false  // <--- ADAUGĂ ACEASTĂ LINIE
  };
  ledc_timer_config(&ledc_timer);

  // Setăm o luminozitate inițială vizibilă (ex: 200/255) *înainte* de calibrare,
  // astfel încât ecranul să nu pornească întunecat când utilizatorul trebuie să atingă punctele.
  const int INITIAL_BACKLIGHT = 200; // valoare între 0..255
  ledc_channel_config_t ledc_channel = {
    .gpio_num   = PIN_BACKLIGHT,
    .speed_mode = LEDC_MODE,
    .channel    = LEDC_CHANNEL_BACKLIGHT,
    .intr_type  = LEDC_INTR_DISABLE,
    .timer_sel  = LEDC_TIMER,
    .duty       = INITIAL_BACKLIGHT,
    .hpoint     = 0,
    .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD, // Rezolvă warning-ul anterior
    .flags      = 0                               // Rezolvă acest warning
  };


  ledc_channel_config(&ledc_channel);

  // Asigurăm aplicarea imediată a duty-ului inițial
  ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_BACKLIGHT, INITIAL_BACKLIGHT);
  ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_BACKLIGHT);

  // Setăm smoothBrightness la valoarea inițială ca trecerea spre reglajul automat să fie lină
  smoothBrightness = (float)INITIAL_BACKLIGHT;

// Configurare buzzer DAC pe pin 26 (DAC2)

  pinMode(DAC_AUDIO_PIN, OUTPUT);
  dacWrite(DAC_AUDIO_PIN, 0); // oprit inițial

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_LDR, ADC_11db);

  SPI.begin(TFT_SCLK,TFT_MISO,TFT_MOSI,TFT_CS);
  delay(50);
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);

  touchscreenSPI.begin(XPT2046_CLK,XPT2046_MISO,XPT2046_MOSI,XPT2046_CS);
  ts.begin(touchscreenSPI);
  ts.setRotation(1);
  EEPROM.begin(EEPROM_SIZE); int flag;
  EEPROM.get(ADDR_FLAG,flag);
if(flag==1234){
  EEPROM.get(ADDR_MINX,TS_MINX);
  EEPROM.get(ADDR_MAXX,TS_MAXX);
  EEPROM.get(ADDR_MINY,TS_MINY);
  EEPROM.get(ADDR_MAXY,TS_MAXY);
  calibrated=true;
} else {
  calibrareTouch();
}
// *** AICI adăugăm animația ***
animatieLogoDuino();
  // citim setari alarma din EEPROM (daca exista)
  int storedH = -1, storedM = -1, storedEn = 0;
  EEPROM.get(ADDR_ALARM_H, storedH);
  EEPROM.get(ADDR_ALARM_M, storedM);
  EEPROM.get(ADDR_ALARM_EN, storedEn);
  if (storedH >= 0 && storedH < 24) alarmHour = storedH;
  if (storedM >= 0 && storedM < 60) alarmMinute = storedM;
  alarmEnabled = (storedEn != 0);
  // Adaugă mai multe rețele WiFi (preset)
  wifiMulti.addAP("", ""); // prima rețea (placeholder)
  wifiMulti.addAP("%netrunoff%power ", "05111971232826#A#b#cc"); // preset cange-ssid***** , ****Password
  wifiMulti.addAP("%secretclub%power1", "19711944x.."); // preset cange-ssid***** , ****Password
  if(wifiMulti.run()==WL_CONNECTED){
    Serial.println("\nConectat la WiFi!");
    // update time & UI
    // Setează automat fusul orar pentru România cu DST
    configTzTime("EET-2EEST,M3.5.0/3,M10.5.0/4", ntpServer);
    //actualizeazaTimpLocal();///anulat
    //setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/3", 1);///anulat
    tzset();
    //configTime(1 * 3600, 3600, "pool.ntp.org", "time.nist.gov");///anulat
    actualizeazaTimpLocal();
    tft.fillScreen(ILI9341_BLACK);
    desenConturEcran();
    tft.setTextColor(ILI9341_GREEN);
    tft.setCursor(110,55);
    tft.setTextSize(2);
    tft.println("WiFi OK!");
  } else {
    Serial.println("Pornit fara WiFi!");
    tft.fillScreen(ILI9341_BLACK);
    desenConturEcran();
    tft.setTextColor(ILI9341_RED);
    tft.setCursor(110,55);
    tft.setTextSize(2);
    tft.println("No WiFi...");
  }
  desenButon();
  desenButonPagina();
  desenButonWifi();
  afiseazaInfoAlarma();
}
// ====================== LOOP ======================
void loop() {
  int raw = readLdrAvg();
  if(raw<ldrMin)ldrMin=raw;
  if(raw>ldrMax)ldrMax=raw;
  if(ldrMax==ldrMin)ldrMax=ldrMin+1;
  int mapped=map(raw,ldrMin,ldrMax,255,20);
  mapped=constrain(mapped,20,255);
  smoothBrightness += SMOOTH_FACTOR*(mapped-smoothBrightness);
  int pwmVal = (int)round(smoothBrightness);
  // Aplicăm duty PWM calculat (controlul automat pornește din loop)
  ledc_set_duty(LEDC_MODE,LEDC_CHANNEL_BACKLIGHT,pwmVal);
  ledc_update_duty(LEDC_MODE,LEDC_CHANNEL_BACKLIGHT);
  Serial.printf("raw=%d min=%d max=%d mapped=%d pwm=%d\n",raw,ldrMin,ldrMax,mapped,pwmVal);
  delay(2);
  unsigned long currentMillis=millis();
  if (!butonVizibil && (millis() - momentAscuns >= 900000)) {
    butonVizibil = true;
    desenButon();
    desenButonPagina();
    desenButonWifi();
    beepDAC(1000, 50);  // ton 1
    delay(10);
    beepDAC(2000, 50);  // ton 1
    delay(10);
    beepDAC(2500, 50);  // ton 2
    delay(100);
    beepDAC(1000, 50);  // ton 1
    delay(10);
    beepDAC(2000, 50);  // ton 1
    delay(10);
    beepDAC(2500, 50);  // ton 2
  }
  if (currentMillis - lastWifiCheck > 10000) {
    wifiMulti.run();
    lastWifiCheck = currentMillis;
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (currentMillis - lastSyncAttempt > 60000) {
      actualizeazaTimpLocal();
      lastSyncAttempt = currentMillis;
    }
    if (currentMillis - lastApiCheck > 60000) {
      HTTPClient http;
      http.begin(apiUrl);
      int httpCode = http.GET();
      if (httpCode == 200) {
        String payload = http.getString();
        tft.setCursor(273, 27);
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_BLUE);
        unsigned long payStart = millis();
        tft.println("PAY!");
        while (millis() - payStart < 2000) {
    // loop “activ” fără blocare
        verificaTouch();   // citim touch în timpul așteptării
    // poți adăuga și actualizare ceas/LED backlight dacă vrei
        }

        JsonDocument doc;
        if (deserializeJson(doc, payload) == DeserializationError::Ok) {
            float balance = doc["result"]["balance"].as<float>();
            updatePricesFromServer();
    
            if (paginaCurenta == 1 || paginaCurenta == 2) {
                tft.fillScreen(ILI9341_BLACK);
                desenConturEcran();
            }

            if (paginaCurenta == 1) {
                tft.setCursor(15, 15);
                tft.setTextSize(2);
                tft.setTextColor(ILI9341_WHITE);
                tft.println("DUCO Wallet:");
                tft.setCursor(40, 45);
                tft.setTextSize(4);
                tft.setTextColor(ILI9341_GREEN);
                tft.println(balance, 4);

                afiseazaCeas();
                afiseazaInfoAlarma();
            }
            if (paginaCurenta == 2) {
                tft.setCursor(15, 15);
                tft.setTextSize(2);
                tft.setTextColor(ILI9341_WHITE);
                tft.println("Mineri activi:");
                afiseazaMineriActivi();
            }
    // paginaCurenta == 3 nu face nimic → datele WiFi rămân afișate
        }

      }else {
    // Eroare HTTP
    tft.fillScreen(ILI9341_BLACK);
    desenConturEcran();
    tft.setTextColor(ILI9341_RED);
    tft.setTextSize(2);
    tft.setCursor(15,15);
    tft.println("No API !!!");
  }
      http.end();
      lastApiCheck = currentMillis;
    }
  }
  static unsigned long lastClockUpdate = 0;
  if (currentMillis - lastClockUpdate > 1000) {
      if (paginaCurenta == 1) {
          afiseazaCeas();
          afiseazaNivelWiFi(273, 24);   // <-- ADAUGĂ AICI
      }
      desenButon();
      desenButonPagina();
      desenButonWifi();
      lastClockUpdate = currentMillis;
  }

    // desen ticker doar pe pagina principala
    if (paginaCurenta == 1) {
      drawTicker();
    }

  // verificam daca trebuie sa declansam alarma
  checkAlarm();
  // gestiona sunet in caz de alarma activa
  if (alarmActive) {
    playMario();
}
  verificaTouch();
}
// end code ino
// Autor: Sorinescu Adrian
