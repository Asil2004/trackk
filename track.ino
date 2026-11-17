#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

// ============================================================================
// 1. WiFi SOZLAMALARI
// ============================================================================
const char* ssid     = "Redmi Note 14";
const char* password = "11111111";

// ============================================================================
// 2. FIREBASE SOZLAMALARI
// Realtime Database manzili (https:// siz va oxirida / siz)
// ============================================================================
#define FIREBASE_HOST "gps-trackker-default-rtdb.europe-west1.firebasedatabase.app"
// Database Secret (Legacy) yoki Web API Key
#define FIREBASE_AUTH "HUv475VnVrSyXE0t35ompyfWNDP5c5xnRLJs6AYF"

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
#define LED_PIN 2  // ESP32 ichki ko'k chirog'i (holat indikatori)

HardwareSerial LG290P(2);
WiFiClient ntripClient;
TinyGPSPlus gps;

// NMEA GNGGA orqali RTK Fix sifatini va parametrlarni aniqlash:
// 6-maydon: Fix Quality (0=Invalid, 1=GPS, 2=DGPS, 4=RTK Fixed, 5=RTK Float)
TinyGPSCustom fixQuality(gps, "GNGGA", 6);
// 8-maydon: HDOP (gorizontal aniqlik ko'rsatkichi)
TinyGPSCustom hdopCustom(gps, "GNGGA", 8);
// 9-maydon: Dengiz sathidan balandlik (metrlarda)
TinyGPSCustom altCustom(gps, "GNGGA", 9);

FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;

// ============================================================================
// SOZLAMALAR VA CHEKLOVLAR
// ============================================================================
const unsigned long SEND_INTERVAL_MS = 2000;    // Har 2 soniyada yangilash
const unsigned long NTRIP_RETRY_MS   = 10000;   // NTRIP uzilsa har 10 soniyada qayta ulanish
const double MIN_DISTANCE_MOVE_M     = 1.0;     // Tarixga yozish uchun minimal harakat (metr)

unsigned long lastSendTime = 0;
unsigned long lastNtripAttempt = 0;
double lastSavedLat = 0.0;
double lastSavedLng = 0.0;

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

// Ikki koordinata orasidagi masofa (Haversine formulasi, metrda)
double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
  if (lat1 == 0.0 || lon1 == 0.0) return 999999.0;
  double dLat = (lat2 - lat1) * DEG_TO_RAD;
  double dLon = (lon2 - lon1) * DEG_TO_RAD;
  double a = sin(dLat / 2) * sin(dLat / 2) +
             cos(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) *
             sin(dLon / 2) * sin(dLon / 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return 6371000.0 * c;
}

// RTK Fix sifatini matn ko'rinishiga o'tkazish
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
      Serial.printf("[NTRIP] Serverga ulanish: %s:%d (Mountpoint: %s)...\n", host, port, mountpoint);
      
      ntripClient.stop();
      if (ntripClient.connect(host, port)) {
        String authStr = base64_encode(String(user) + ":" + pw);
        ntripClient.print(String("GET /") + mountpoint + " HTTP/1.0\r\n" +
                          "User-Agent: NTRIP ESP32_LG290P/1.1\r\n" +
                          "Authorization: Basic " + authStr + "\r\n" +
                          "Connection: close\r\n\r\n");
        Serial.println("[NTRIP] RTCM tuzatishlar oqimi boshlandi!");
      } else {
        Serial.println("[NTRIP] Ulanib bo'lmadi, keyinroq qayta uriniladi.");
      }
    }
  }

  // Kelgan RTCM tuzatish baytlarini LG290P moduliga yuborish
  while (ntripClient.available()) {
    LG290P.write(ntripClient.read());
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  delay(500);
  Serial.println("\n======================================");
  Serial.println("  RTK GPS Tracker — ESP32 + LG290P    ");
  Serial.println("======================================");

  // LG290P GNSS moduli bilan UART aloqasi
  LG290P.begin(460800, SERIAL_8N1, RX2_PIN, TX2_PIN);
  Serial.println("[UART] LG290P ulandi (460800 baud).");

  // WiFi ga ulanish
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
    Serial.println("\n[WiFi] Ulanish vaqti tugadi, orqa fonda qayta ulanadi.");
  }

  // Firebase konfiguratsiyasi
  Serial.println("[Firebase] Initsializatsiya...");
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  
  // Timeout sozlamalari
  config.timeout.wifiReconnect = 5000;
  config.timeout.socketConnection = 5000;
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(1024);

  Serial.println("[Tizim] Tayyor! GPS ma'lumotlari kutilmoqda...\n");
}

void loop() {
  // 1. NTRIP ulanishini boshqarish
  maintainNTRIP();

  // 2. GNSS modulidan ma'lumotlarni o'qish va parse qilish
  while (LG290P.available()) {
    char c = LG290P.read();
    gps.encode(c);
  }

  // 3. Har SEND_INTERVAL_MS vaqt oralig'ida Firebase'ga yuborish
  if (millis() - lastSendTime >= SEND_INTERVAL_MS) {
    lastSendTime = millis();

    if (gps.location.isValid() && gps.location.isUpdated()) {
      double currentLat = gps.location.lat();
      double currentLng = gps.location.lng();
      int satellites    = gps.satellites.isValid() ? (int)gps.satellites.value() : 0;
      
      // RTK Fix sifati (0=Yo'q, 1=GPS, 2=DGPS, 4=RTK Fix, 5=RTK Float)
      int rtkFix = fixQuality.isValid() ? atoi(fixQuality.value()) : 0;
      float hdop = hdopCustom.isValid() ? atof(hdopCustom.value()) : (gps.hdop.isValid() ? gps.hdop.hdop() : 99.9);
      float altitude = altCustom.isValid() ? atof(altCustom.value()) : (gps.altitude.isValid() ? gps.altitude.meters() : 0.0);
      float speedKmh = gps.speed.isValid() ? gps.speed.kmph() : 0.0;

      // JSON obyekt tayyorlash
      FirebaseJson json;
      json.set("lat", String(currentLat, 8));
      json.set("lng", String(currentLng, 8));
      json.set("sat", satellites);
      json.set("fix", rtkFix);
      json.set("fix_str", getFixTypeStr(rtkFix));
      json.set("hdop", hdop);
      json.set("alt", altitude);
      json.set("speed", speedKmh);
      
      // Firebase Server Timestamp (aniq server vaqti uchun)
      FirebaseJson tsObj;
      tsObj.set(".sv", "timestamp");
      json.set("ts", tsObj);

      // A) Hozirgi joylashuvni yangilash (/current_pos)
      bool ok = false;
      #if defined(FIREBASE_ESP32_VERSION)
        ok = Firebase.RTDB.setJSON(&fbdo, "/current_pos", &json);
      #else
        // Moslik uchun tekshiruv (har ikkala usulda ham ishlaydi)
        ok = Firebase.setJSON(fbdo, "/current_pos", json);
      #endif

      if (ok) {
        Serial.printf("[GPS] Lat: %.8f, Lng: %.8f | Sat: %d | Status: %s | HDOP: %.2f\n", 
                      currentLat, currentLng, satellites, getFixTypeStr(rtkFix), hdop);
      } else {
        Serial.printf("[Firebase Xato] /current_pos: %s\n", fbdo.errorReason().c_str());
      }

      // B) Tarixga faqat harakat sodir bo'lganda qo'shish (/history)
      // Bu bazaning keraksiz to'lib ketishining oldini oladi!
      double distMoved = calculateDistance(lastSavedLat, lastSavedLng, currentLat, currentLng);
      if (distMoved >= MIN_DISTANCE_MOVE_M) {
        lastSavedLat = currentLat;
        lastSavedLng = currentLng;

        #if defined(FIREBASE_ESP32_VERSION)
          Firebase.RTDB.pushJSON(&fbdo, "/history", &json);
        #else
          Firebase.pushJSON(fbdo, "/history", json);
        #endif

        Serial.printf("[Tarix] Yangi nuqta yozildi (+%.1fm siljish)\n", distMoved);
      }

      // Status LED ni miltillatish
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));

    } else if (!gps.location.isValid()) {
      Serial.println("[GPS] Sun'iy yo'ldoshlar signali kutilmoqda...");
    }
  }
}
