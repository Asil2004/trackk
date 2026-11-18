#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <TinyGPSPlus.h>

// ============================================================================
// 1. WiFi SOZLAMALARI
// ============================================================================
const char* ssid     = "cail";
const char* password = "12345678";

// ============================================================================
// 2. SERVER MANZILI (Render.com)
// ============================================================================
const char* serverApiUrl = "https://trackk-gl6l.onrender.com/api.php";

// ============================================================================
// 3. NTRIP SOZLAMALARI (Faqat Baza bo'lsa yoqing)
// ============================================================================
const bool  ENABLE_NTRIP = false; // Baza bo'lmasa false qiling (qotishni va uzilishni yo'qotadi!)
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

// NMEA GNGGA dan RTK parametrlarini ajratib olish:
TinyGPSCustom fixQuality(gps, "GNGGA", 6);
TinyGPSCustom hdopCustom(gps, "GNGGA", 8);
TinyGPSCustom altCustom(gps, "GNGGA", 9);

// ============================================================================
// VAQT VA TEZLIK SOZLAMALARI
// ============================================================================
const unsigned long SEND_INTERVAL_MS = 1000;  // Serverga har 1 soniyada yuborish
const unsigned long NTRIP_RETRY_MS   = 15000;
const unsigned long NTRIP_GGA_MS     = 6000;

unsigned long lastSendTime = 0;
unsigned long lastNtripAttempt = 0;
unsigned long lastNtripGgaTime = 0;
unsigned long totalRtcmBytes = 0;

// Eng so'nggi yangilangan koordinatalar keshi (Buffer drop bo'lmasligi uchun)
double latestLat   = 0.0;
double latestLng   = 0.0;
int    latestSat   = 0;
int    latestFix   = 1;
float  latestHdop  = 99.9;
float  latestAlt   = 0.0;
float  latestSpeed = 0.0;
bool   hasFreshGps = false;

String lastGgaSentence = "";
String rawNmeaBuffer = "";
bool isNtripHeaderSkipped = false;

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
    case 5: return "RTK FLOAT (10-30 sm)";
    case 2: return "DGPS (0.5-1 m)";
    case 1: return "Oddiy GPS (2-5 m)";
    default: return "Signal qidirilmoqda";
  }
}

void maintainNTRIP() {
  if (!ENABLE_NTRIP || WiFi.status() != WL_CONNECTED) return;

  if (!ntripClient.connected()) {
    isNtripHeaderSkipped = false;
    if (millis() - lastNtripAttempt > NTRIP_RETRY_MS) {
      lastNtripAttempt = millis();
      ntripClient.stop();
      if (ntripClient.connect(host, port)) {
        String authStr = base64_encode(String(user) + ":" + pw);
        ntripClient.print(String("GET /") + mountpoint + " HTTP/1.0\r\n" +
                          "User-Agent: NTRIP ESP32_LG290P/2.5\r\n" +
                          "Authorization: Basic " + authStr + "\r\n" +
                          "Accept: rtk/rtcm, */*\r\n" +
                          "Connection: close\r\n\r\n");
      }
    }
    return;
  }

  if (!isNtripHeaderSkipped) {
    while (ntripClient.available()) {
      String line = ntripClient.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) {
        isNtripHeaderSkipped = true;
        break;
      }
    }
    return;
  }

  size_t avail = ntripClient.available();
  if (avail > 0) {
    uint8_t buffer[256];
    size_t bytesRead = ntripClient.read(buffer, min(avail, sizeof(buffer)));
    LG290P.write(buffer, bytesRead);
    totalRtcmBytes += bytesRead;
  }

  if (millis() - lastNtripGgaTime >= NTRIP_GGA_MS) {
    lastNtripGgaTime = millis();
    if (lastGgaSentence.length() > 0) {
      ntripClient.print(lastGgaSentence + "\r\n");
    }
  }
}

// ============================================================================
// SERVERGA TELEMETRIYA YUBORISH (HTTPS Timeout va Barqarorlik)
// ============================================================================
void sendToServer(double lat, double lng, int sat, int fix, float hdop, float alt, float speed) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure(); // SSL sertifikatini qabul qilish

  HTTPClient http;
  http.begin(client, serverApiUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(2500); // 2.5s timeout — hech qachon qotib qolmaydi

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
    Serial.printf("[1s OK] Lat: %.8f, Lng: %.8f | Sat: %d | HDOP: %.2f | Speed: %.1f km/h\n",
                  lat, lng, sat, hdop, speed);
  } else {
    Serial.printf("[HTTP Xato: %d]\n", httpCode);
  }
  http.end();
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  delay(300);
  Serial.println("\n==============================================");
  Serial.println("  RTK GPS Tracker Pro — ESP32 + LG290P (SuperFast)");
  Serial.println("==============================================");

  // 1. UART RX buferini 2048 baytga oshirish (NMEA paketlari yo'qolmasligi uchun)
  LG290P.setRxBufferSize(2048);
  LG290P.begin(460800, SERIAL_8N1, RX2_PIN, TX2_PIN);
  Serial.println("[UART] LG290P RX buferi 2048 baytga oshirildi (460800 baud).");

  // 2. Quectel LG290P ni barcha yo'ldosh tizimlariga sozlash
  delay(150);
  LG290P.println("$PQTMCFGCNST,1,1,1,1,1,1*00"); // GPS, GLO, GAL, BDS, QZSS, NavIC
  delay(100);
  LG290P.println("$PQTMCFGFIXRATE,1000*00");      // 1000 ms = 1 Hz ravon yangilanish

  // 3. WiFi ulanish — UZILIB QOLMASLIGI UCHUN MAXSUS SOZLAMALAR
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true); // Uzilsa avtomatik qayta ulanish
  WiFi.persistent(true);
  WiFi.setSleep(false);        // WiFi modulini uyquga kiritmaslik (ping va uzilishni yo'qotadi!)

  Serial.printf("[WiFi] Ulanmoqda: %s ", ssid);
  WiFi.begin(ssid, password);
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000) {
    delay(300);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
    Serial.printf("\n[WiFi] Ulandi! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    digitalWrite(LED_PIN, LOW);
    Serial.println("\n[WiFi] Ulanish kutilmoqda...");
  }
}

// ============================================================================
// LOOP — MUTLAQO BLOKIROVKASIZ VA UZLUKSIZ O'QISH
// ============================================================================
void loop() {
  // 1. WiFi uzilsa zudlik bilan qayta ulanish
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, LOW);
    static unsigned long lastWifiRetry = 0;
    if (millis() - lastWifiRetry > 3000) {
      lastWifiRetry = millis();
      WiFi.reconnect();
    }
  } else {
    digitalWrite(LED_PIN, HIGH);
  }

  // 2. NTRIP faqat yoqilgan bo'lsa ishlaydi
  if (ENABLE_NTRIP) {
    maintainNTRIP();
  }

  // 3. GNSS modulidan uzluksiz, birorta baytni boy bermasdan o'qish
  while (LG290P.available()) {
    char c = LG290P.read();
    gps.encode(c);

    // Yangi koordinata kelishi bilan keshga saqlash
    if (gps.location.isUpdated()) {
      latestLat   = gps.location.lat();
      latestLng   = gps.location.lng();
      hasFreshGps = true;

      if (gps.satellites.isValid()) latestSat = (int)gps.satellites.value();
      if (fixQuality.isValid())     latestFix = atoi(fixQuality.value());
      if (hdopCustom.isValid())     latestHdop = atof(hdopCustom.value());
      else if (gps.hdop.isValid())  latestHdop = gps.hdop.hdop();
      if (altCustom.isValid())      latestAlt = atof(altCustom.value());
      else if (gps.altitude.isValid()) latestAlt = gps.altitude.meters();
      if (gps.speed.isValid())      latestSpeed = gps.speed.kmph();
    }

    if (ENABLE_NTRIP) {
      if (c == '$') {
        rawNmeaBuffer = "$";
      } else if (rawNmeaBuffer.length() > 0) {
        rawNmeaBuffer += c;
        if (c == '\n') {
          if (rawNmeaBuffer.startsWith("$GNGGA") || rawNmeaBuffer.startsWith("$GPGGA")) {
            lastGgaSentence = rawNmeaBuffer;
            lastGgaSentence.trim();
          }
          rawNmeaBuffer = "";
        }
      }
    }
  }

  // 4. Serverga HAR 1 SONIYADA ravon va kechikishsiz yuborish
  if (millis() - lastSendTime >= SEND_INTERVAL_MS) {
    lastSendTime = millis();

    if (latestLat != 0.0 && latestLng != 0.0) {
      sendToServer(latestLat, latestLng, latestSat, latestFix, latestHdop, latestAlt, latestSpeed);
      hasFreshGps = false;
    }
  }
}
