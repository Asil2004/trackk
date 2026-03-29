#include <TinyGPSPlus.h>

#include <WiFi.h>
#include <FirebaseESP32.h> // Sizdagi kutubxona nomi


// 1. WiFi ma'lumotlari
const char* ssid = "Redmi Note 14";
const char* password = "11111111";

// 2. Firebase ma'lumotlari
#define FIREBASE_HOST "gps-trackker-default-rtdb.europe-west1.firebasedatabase.app" 
#define FIREBASE_AUTH "HUv475VnVrSyXE0t35ompyfWNDP5c5xnRLJs6AYF" 

// 3. NTRIP Sozlamalari
const char* host = "rtk2go.com";
const int port = 2101;
const char* mountpoint = "SAMARKAND";
const char* user = "rasil8004@gmail.com";
const char* pw = "none";

WiFiClient ntripClient;
HardwareSerial LG290P(2);
TinyGPSPlus gps;
FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;

// Base64 kodlash
String base64_encode(String text) {
  String res = "";
  static const char cb64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  for (int i = 0; i < text.length(); i += 3) {
    uint32_t buffer = (text[i] << 16) | ((i+1 < text.length() ? text[i+1] : 0) << 8) | (i+2 < text.length() ? text[i+2] : 0);
    res += cb64[(buffer >> 18) & 0x3F];
    res += cb64[(buffer >> 12) & 0x3F];
    res += (i + 1 < text.length()) ? cb64[(buffer >> 6) & 0x3F] : '=';
    res += (i + 2 < text.length()) ? cb64[buffer & 0x3F] : '=';
  }
  return res;
}

void setup() {
  Serial.begin(115200);
  LG290P.begin(460800, SERIAL_8N1, 27, 14); 

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi ulandi!");

  // Firebase sozlamalari (Yangi versiya uchun moslangan)
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  // NTRIP ulanishi
  if (!ntripClient.connected()) {
    if (ntripClient.connect(host, port)) {
      String authStr = base64_encode(String(user) + ":" + pw);
      ntripClient.print(String("GET /") + mountpoint + " HTTP/1.0\r\n" +
                        "Authorization: Basic " + authStr + "\r\n" +
                        "Connection: close\r\n\r\n");
    }
  }

  while (ntripClient.available()) { LG290P.write(ntripClient.read()); }

  // GPS va Ma'lumot yuborish
  while (LG290P.available()) {
    if (gps.encode(LG290P.read())) {
      if (gps.location.isUpdated()) {
        static unsigned long lastSend = 0;
        if (millis() - lastSend > 3000) { 
          
          FirebaseJson json;
          json.set("lat", String(gps.location.lat(), 8));
          json.set("lng", String(gps.location.lng(), 8));
          json.set("fix", (int)gps.satellites.value()); 

          // DIQQAT: Eskirgan RTDB.setJSON o'rniga to'g'ridan-to'g'ri funksiyalar:
          if (Firebase.setJSON(fbdo, "/current_pos", json)) {
             Serial.println("Current position yangilandi");
          }
          
          if (Firebase.pushJSON(fbdo, "/history", json)) {
             Serial.println("Tarixga yangi nuqta qo'shildi");
          }

          lastSend = millis();
        }
      }
    }
  }
}