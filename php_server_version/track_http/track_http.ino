#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <TinyGPSPlus.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ============================================================================
// 1. WiFi SOZLAMALARI (Telefoningiz Hotspot nomini to'g'ri yozing!)
// ============================================================================
// DIQQAT: Telefoningiz hotspot nomini va parolini tekshiring:
const char* ssid     = "cail";
const char* password = "12345678";

// ============================================================================
// 2. SERVER MANZILI (Render.com)
// ============================================================================
const char* serverApiUrl = "https://trackk-gl6l.onrender.com/api.php";

// ============================================================================
// 3. NTRIP SOZLAMALARI (Faqat Baza bo'lsa yoqing)
// ============================================================================
const bool  ENABLE_NTRIP = false; // Baza bo'lmasa doim FALSE tursin!
const char* host         = "rtk2go.com";
const int   port         = 2101;
const char* mountpoint   = "SAMARKAND";
const char* user         = "rasil8004@gmail.com";
const char* pw           = "none";

// ============================================================================
// 4. USKUNA PINLARI VA OBYEKTLAR
// ============================================================================
#define RX2_PIN 27 // LG290P TX -> ESP32 RX2
#define TX2_PIN 14 // LG290P RX -> ESP32 TX2
#define LED_PIN 2  // ESP32 ichki holat LED chirog'i

HardwareSerial LG290P(2);
WiFiClient ntripClient;
TinyGPSPlus gps;

TinyGPSCustom fixQuality(gps, "GNGGA", 6);
TinyGPSCustom hdopCustom(gps, "GNGGA", 8);
TinyGPSCustom altCustom(gps, "GNGGA", 9);

// ============================================================================
// VAQT VA TEZLIK SOZLAMALARI
// ============================================================================
const unsigned long SEND_INTERVAL_MS = 1000;  // Serverga har 1 soniyada yuborish
unsigned long lastSendTime = 0;
unsigned long lastWifiRetry = 0;

// Eng so'nggi yangilangan koordinatalar keshi
double latestLat   = 0.0;
double latestLng   = 0.0;
int    latestSat   = 0;
int    latestFix   = 1;
float  latestHdop  = 99.9;
float  latestAlt   = 0.0;
float  latestSpeed = 0.0;

// Base64
String base64_encode(String text) {
  String res = "";
  static const char cb64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  for (int i = 0; i < text.length(); i += 3) {
    uint32_t buffer = (text[i] << 16) | 
                      ((i + 1 < text.length() ? (uint8_t)text[i+1] : 0) << 8) | 
                      (i + 2 < text.length() ? (uint8_t)text[i+2] : 0);
    res += cb64[(buffer >> 18) & 0x3F];
    res += cb64[(buffer >> 12) & 0x3F];
    res += (i + 1 < text.length()) ? cb64[(buffer >> 6) & 0x3F] : '=';
    res += (i + 2 < text.length()) ? cb64[buffer & 0x3F] : '=';
  }
  return res;
}

const char* getFixTypeStr(int fixCode) {
  switch (fixCode) {
    case 4: return "RTK FIXED (1-2 sm)";
    case 5: return "RTK/PPP FLOAT (10-30 sm)";
    case 2: return "DGPS/HAS (20 sm - 1 m)";
    case 1: return "Oddiy 3D (Ko'p chastota)";
    default: return "Signal qidirilmoqda";
  }
}

// Quectel LG290P NMEA buyruqlarini aniq XOR checksum bilan jo'natish funksiyasi
void sendQuectelCmd(const String& cmd) {
  String core = cmd;
  if (core.startsWith("$")) core = core.substring(1);
  int starIdx = core.indexOf('*');
  if (starIdx >= 0) core = core.substring(0, starIdx);

  uint8_t checksum = 0;
  for (unsigned int i = 0; i < core.length(); i++) {
    checksum ^= core[i];
  }

  char buf[8];
  sprintf(buf, "*%02X", checksum);
  String fullCmd = "$" + core + String(buf);
  LG290P.println(fullCmd);
  Serial.print("[LG290P CMD] ");
  Serial.println(fullCmd);

  // Moduldan tasdiq (ACK) javobini o'qish
  unsigned long t0 = millis();
  while (millis() - t0 < 250) {
    while (LG290P.available()) {
      char c = LG290P.read();
      Serial.write(c);
    }
    delay(10);
  }
}

// ============================================================================
// SERVERGA TELEMETRIYA YUBORISH (Xotira va Tok himoyasi bilan)
// ============================================================================
void sendToServer(double lat, double lng, int sat, int fix, float hdop, float alt, float speed) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ALOQA XATO] WiFi uzilgan! Qayta ulanish kutilmoqda...");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();               // Render sertifikatini tekshirmaslik
  client.setBufferSizes(1024, 512);   // Xotira (Heap) to'lib ketishini oldini olish

  HTTPClient http;
  http.begin(client, serverApiUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(2500); // 2.5s dan ortiq qotmaslik

  String payload = "{";
  payload += "\"lat\":\"" + String(lat, 8) + "\",";
  payload += "\"lng\":\"" + String(lng, 8) + "\",";
  payload += "\"sat\":" + String(sat) + ",";
  payload += "\"fix\":" + String(fix) + ",";
  payload += "\"fix_str\":\"" + String(getFixTypeStr(fix)) + "\",";
  payload += "\"hdop\":" + String(hdop, 2) + ",";
  payload += "\"alt\":" + String(alt, 1) + ",";
  payload += "\"speed\":" + String(speed, 1);
  payload += "}";

  int httpCode = http.POST(payload);

  if (httpCode == HTTP_CODE_OK || httpCode == 201) {
    Serial.printf("[1s OK] Lat: %.8f, Lng: %.8f | Sat: %d | HDOP: %.2f | Heap: %d\n",
                  lat, lng, sat, hdop, ESP.getFreeHeap());
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Miltillash = aloqa bor
  } else {
    Serial.printf("[HTTP XATO: %d] Aloqada xatolik!\n", httpCode);
  }
  http.end();
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  // 1. Tok yetishmovchiligida ESP32 ning o'chib-yonishini to'xtatish (Brownout Disable)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  delay(300);
  Serial.println("\n==============================================");
  Serial.println("  RTK GPS Tracker Pro — ESP32 + LG290P        ");
  Serial.println("==============================================");

  // 2. UART buferi (Paketlar yo'qolmasligi uchun 2048 bayt)
  LG290P.setRxBufferSize(2048);
  LG290P.begin(460800, SERIAL_8N1, RX2_PIN, TX2_PIN);
  Serial.println("[UART] LG290P ulandi (460800 baud, 2048B bufer).");

  // 3. Quectel LG290P sozlamalari (Galileo HAS va Ko'p chastotali yuqori aniqlik)
  delay(200);
  Serial.println("[LG290P] Galileo HAS va barcha sun'iy yo'ldoshlar yoqilmoqda...");
  sendQuectelCmd("PQTMCFGCNST,1,1,1,1,1,1"); // Barcha yo'ldoshlar: GPS, GLO, GAL, BDS, QZSS, NavIC
  delay(100);
  sendQuectelCmd("PQTMCFGSBAS,1");            // SBAS differensial tizimi
  delay(100);
  sendQuectelCmd("PQTMCFGPPP,W,2,1");        // Galileo HAS (High Accuracy Service - E6B bepul PPP)
  delay(100);
  sendQuectelCmd("PQTMCFGFIXRATE,1000");     // 1000 ms (1 Hz) yangilanish
  delay(100);
  sendQuectelCmd("PQTMSAVEPAR");             // Sozlamalarni modul xotirasiga saqlash (NVM)
  delay(150);

  // 4. WiFi ulanish — UZILIB QOLMASLIK REJIMI
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true); // Uzilsa darhol o'zi qayta ulanadi
  WiFi.persistent(true);
  WiFi.setSleep(false);        // WiFi modulining uyqu rejimini o'chirish (aloqa uzilmasligi uchun!)

  Serial.printf("[WiFi] Ulanmoqda: \"%s\" ...\n", ssid);
  WiFi.begin(ssid, password);

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000) {
    delay(400);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
    Serial.printf("\n[WiFi OK] Ulandi! IP: %s (Signal: %d dBm)\n", 
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    digitalWrite(LED_PIN, LOW);
    Serial.println("\n[WiFi XATO] Ulanib bo'lmadi! Hotspot nomi va parolini tekshiring!");
  }
}

// ============================================================================
// LOOP — MUTLAQO BLOKIROVKASIZ
// ============================================================================
void loop() {
  // 1. WiFi uzilib qolsa darhol qayta ulash watchdog
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, LOW);
    if (millis() - lastWifiRetry > 2000) {
      lastWifiRetry = millis();
      Serial.println("[WiFi] Uzilish aniqlandi! Qayta ulanmoqda...");
      WiFi.reconnect();
    }
  }

  // 2. GNSS modulidan uzluksiz o'qish
  while (LG290P.available()) {
    char c = LG290P.read();
    gps.encode(c);

    if (gps.location.isUpdated()) {
      latestLat   = gps.location.lat();
      latestLng   = gps.location.lng();
      if (gps.satellites.isValid()) latestSat = (int)gps.satellites.value();
      if (fixQuality.isValid())     latestFix = atoi(fixQuality.value());
      if (hdopCustom.isValid())     latestHdop = atof(hdopCustom.value());
      else if (gps.hdop.isValid())  latestHdop = gps.hdop.hdop();
      if (altCustom.isValid())      latestAlt = atof(altCustom.value());
      else if (gps.altitude.isValid()) latestAlt = gps.altitude.meters();
      if (gps.speed.isValid())      latestSpeed = gps.speed.kmph();
    }
  }

  // 3. Serverga HAR 1 SONIYADA yuborish
  if (millis() - lastSendTime >= SEND_INTERVAL_MS) {
    lastSendTime = millis();

    if (latestLat != 0.0 && latestLng != 0.0) {
      sendToServer(latestLat, latestLng, latestSat, latestFix, latestHdop, latestAlt, latestSpeed);
    } else {
      Serial.println("[GPS] Sun'iy yo'ldoshlar koordinatasi kutilmoqda...");
    }
  }
}
