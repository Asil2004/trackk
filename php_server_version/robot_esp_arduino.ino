#include <WiFi.h>
#include <HTTPClient.h>

// ============================================================================
// QALQON ROBOTI UCHUN GPS QABUL QILUVCHI MIKROKONTROLLER KODI
// Robotning ikkinchi ESP32 yoki WiFi moduli bor kontrolleri uchun
// Serverdan (IP yoki domen) RTK GPS koordinatalarini o'qiydi
// ============================================================================

const char* ssid     = "WiFi_Nomi";
const char* password = "WiFi_Paroli";

// Serverdagi api.php manzili (IP yoki domen)
const char* serverUrl = "http://sizningsaytingiz.uz/gps/api.php?action=robot_raw";

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[QALQON ROBOTI] GPS qabul qiluvchi ishga tushmoqda...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Ulandi! IP: " + WiFi.localIP().toString());
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.setTimeout(2500);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      payload.trim();

      // Format: $ROBOT,LAT,LNG,FIX,HDOP,SPEED,IS_ONLINE,AGE_SEC
      if (payload.startsWith("$ROBOT")) {
        int idx1 = payload.indexOf(',');
        int idx2 = payload.indexOf(',', idx1 + 1);
        int idx3 = payload.indexOf(',', idx2 + 1);
        int idx4 = payload.indexOf(',', idx3 + 1);
        int idx5 = payload.indexOf(',', idx4 + 1);
        int idx6 = payload.indexOf(',', idx5 + 1);
        int idx7 = payload.indexOf(',', idx6 + 1);

        double lat    = payload.substring(idx1 + 1, idx2).toDouble();
        double lng    = payload.substring(idx2 + 1, idx3).toDouble();
        int fix       = payload.substring(idx3 + 1, idx4).toInt();
        float hdop    = payload.substring(idx4 + 1, idx5).toFloat();
        float speed   = payload.substring(idx5 + 1, idx6).toFloat();
        int isOnline  = payload.substring(idx6 + 1, idx7).toInt();
        float ageSec  = payload.substring(idx7 + 1).toFloat();

        if (isOnline == 1) {
          Serial.printf("[ROBOT GPS] Lat: %.8f | Lng: %.8f | RTK: %d | HDOP: %.2f | Tezlik: %.1f km/h\n",
                        lat, lng, fix, hdop, speed);

          // BU YERDA ROBOT MOTORLARINI BOSHQARISH VA HARAKAT KODI YOZILADI
          // Masalan: robot_drive_to(lat, lng, target_lat, target_lng);
        } else {
          Serial.printf("[XAVFSIZLIK] GPS ma'lumoti eskirgan (%.1f soniya oldin). Robot to'xtatildi!\n", ageSec);
        }
      }
    } else {
      Serial.printf("[HTTP Xato] Kod: %d\n", httpCode);
    }
    http.end();
  }

  delay(1000); // Har 1 soniyada koordinatani yangilash
}
