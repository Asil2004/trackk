#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <TinyGPSPlus.h>

// ============================================================================
// 1. WiFi SOZLAMALARI
// ============================================================================
const char* ssid     = "cail";
const char* password = "111111";

// ============================================================================
// 2. O'ZINGIZNING SAYTINGIZ / SERVERINGIZ MANZILI
// api.php joylashgan to'liq URL manzilini kiriting
// ============================================================================
const char* serverApiUrl = "https://trackk-gl6l.onrender.com/api.php";

// ============================================================================
// 3. NTRIP SOZLAMALARI (RTK Tuzatishlar uchun)
// ============================================================================
const char* host       = "rtk2go.com";
const int   port       = 2101;
const char* mountpoint = "SAMARKAND";
const char* user       = "rasil8004@gmail.com";
const char* pw         = "none";

// ============================================================================
// 4. USKUNA (PIN) VA OBYEKTLAR
// ============================================================================
#define RX2_PIN 27 // LG290P TX -> ESP32 RX2
#define TX2_PIN 14 // LG290P RX -> ESP32 TX2
#define LED_PIN 2  // ESP32 ichki holat LED chirog'i

HardwareSerial LG290P(2);
WiFiClient ntripClient;
TinyGPSPlus gps;

// NMEA GNGGA dan RTK parametrlarini ajratib olish
TinyGPSCustom fixQuality(gps, "GNGGA", 6); // 1=GPS, 2=DGPS, 4=RTK Fixed, 5=RTK Float
TinyGPSCustom hdopCustom(gps, "GNGGA", 8); // HDOP aniqligi
TinyGPSCustom altCustom(gps, "GNGGA", 9);  // Dengiz sathidan balandlik

// ============================================================================
// VAQT VA FILTRLAR
// ============================================================================
const unsigned long SEND_INTERVAL_MS = 1500;  // Har 1.5 soniyada serverga yuborish
const unsigned long NTRIP_RETRY_MS   = 10000; // NTRIP uzilsa har 10 soniyada ulanish

unsigned long lastSendTime = 0;
unsigned long lastNtripAttempt = 0;

// Base64 kodlash (NTRIP Basic Auth uchun)
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

// RTK Fix holati matni
const char* getFixTypeStr(int fixCode) {
  switch (fixCode) {
    case 4: return "RTK Fixed (1-2 sm)";
    case 5: return "RTK Float (10-30 sm)";
    case 2: return "DGPS (0.5-1 m)";
    case 1: return "Oddiy GPS (2-5 m)";
    default: return "Signal qidirilmoqda";
  }
}

// Non-blocking NTRIP Caster ulanishi
void maintainNTRIP() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (!ntripClient.connected()) {
    if (millis() - lastNtripAttempt > NTRIP_RETRY_MS) {
      lastNtripAttempt = millis();
      Serial.printf("[NTRIP] Ulanish: %s:%d/%s...\n", host, port, mountpoint);
      
      ntripClient.stop();
      if (ntripClient.connect(host, port)) {
        String authStr = base64_encode(String(user) + ":" + pw);
        ntripClient.print(String("GET /") + mountpoint + " HTTP/1.0\r\n" +
                          "User-Agent: NTRIP ESP32_LG290P/2.0\r\n" +
                          "Authorization: Basic " + authStr + "\r\n" +
                          "Connection: close\r\n\r\n");
        Serial.println("[NTRIP] Baza stansiyasidan tuzatishlar olinmoqda!");
      } else {
        Serial.println("[NTRIP] Ulanib bo'lmadi.");
      }
    }
  }

  // RTCM paketlarini LG290P moduliga uzatish
  while (ntripClient.available()) {
    LG290P.write(ntripClient.read());
  }
}

// Saytingizdagi api.php ga HTTP POST orqali yuborish
void sendToServer(double lat, double lng, int sat, int fix, float hdop, float alt, float speed) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure(); // Render HTTPS sertifikatini qabul qilish

  HTTPClient http;
  http.begin(client, serverApiUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(4000); // 4 soniya timeout

  // JSON formatdagi ma'lumot
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
    Serial.printf("[HTTP OK %d] Lat: %.8f, Lng: %.8f | Sat: %d | Status: %s | HDOP: %.2f\n",
                  httpCode, lat, lng, sat, getFixTypeStr(fix), hdop);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Holat LED miltillashi
  } else {
    Serial.printf("[HTTP Xato: %d] %s\n", httpCode, http.errorToString(httpCode).c_str());
  }

  http.end();
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  delay(500);
  Serial.println("\n==============================================");
  Serial.println("  RTK GPS Tracker — HTTP Standalone Server    ");
  Serial.println("==============================================");

  // GNSS UART
  LG290P.begin(460800, SERIAL_8N1, RX2_PIN, TX2_PIN);
  Serial.println("[UART] LG290P ulandi (460800 baud).");

  // WiFi ulanish
  Serial.printf("[WiFi] Ulanmoqda: %s ", ssid);
  WiFi.begin(ssid, password);
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000) {
    delay(400);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
    Serial.printf("\n[WiFi] Ulandi! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    digitalWrite(LED_PIN, LOW);
    Serial.println("\n[WiFi] Kutilmoqda, fon rejimida ulanadi.");
  }
}

void loop() {
  // 1. NTRIP aloqasini ushlab turish
  maintainNTRIP();

  // 2. GNSS modulidan baytlarni o'qish
  while (LG290P.available()) {
    char c = LG290P.read();
    gps.encode(c);
  }

  // 3. Serverga har SEND_INTERVAL_MS oralig'ida yuborish
  if (millis() - lastSendTime >= SEND_INTERVAL_MS) {
    lastSendTime = millis();

    if (gps.location.isValid() && gps.location.isUpdated()) {
      double currentLat = gps.location.lat();
      double currentLng = gps.location.lng();
      int satellites    = gps.satellites.isValid() ? (int)gps.satellites.value() : 0;
      int rtkFix        = fixQuality.isValid() ? atoi(fixQuality.value()) : 0;
      float hdop        = hdopCustom.isValid() ? atof(hdopCustom.value()) : (gps.hdop.isValid() ? gps.hdop.hdop() : 99.9);
      float altitude    = altCustom.isValid() ? atof(altCustom.value()) : (gps.altitude.isValid() ? gps.altitude.meters() : 0.0);
      float speedKmh    = gps.speed.isValid() ? gps.speed.kmph() : 0.0;

      sendToServer(currentLat, currentLng, satellites, rtkFix, hdop, altitude, speedKmh);

    } else if (!gps.location.isValid()) {
      Serial.println("[GPS] Sun'iy yo'ldoshlar signali kutilmoqda...");
    }
  }
}
