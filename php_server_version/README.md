# 🌐 RTK GPS Tracker & Qalqon Roboti Navigatsiyasi (Mustaqil PHP Server)

Ushbu tizim **Google Firebase'siz**, o'zingizning shaxsiy veb-saytingiz yoki serveringizda ishlaydi. 
U **bir vaqtning o'zida ikkita vazifani** bajaradi:
1. **Asosiy vazifa:** ESP32 + LG290P modulidan santimetr aniqligidagi RTK GPS ma'lumotlarini serverga uzatish va veb-xaritada jonli ko'rsatish (o'lchovlar, trek yozish, eksport).
2. **Qalqon Roboti uchun GPS:** Robot istalgan joydan turib (Wi-Fi, 4G/LTE yoki lokal tarmoq orqali) serverning IP manzili yoki domeni orqali o'zining santimetr darajasidagi joriy koordinatalarini oladi va avtonom harakatlanadi.

---

## 📁 Papka tarkibi

| Fayl | Vazifasi |
|---|---|
| **`api.php`** | ESP32 dan koordinatalarni qabul qiladi, robot uchun maxsus endpointlarni (`?action=robot`, `?action=robot_raw`) taqdim etadi |
| **`track_http.ino`** | Asosiy ESP32 kodi (LG290P + NTRIP). Bunga hech qanday o'zgartirish kiritilmagan, asosiy vazifasini 100% bajaradi |
| **`index.html`** | Veb-xarita: Jonli kuzatish, o'lchovlar va robot uchun maqsad (Target/Waypoint) belgilash |
| **`robot_client_example.py`** | Qalqon Roboti uchun Python dasturi (Raspberry Pi, Linux yoki robot kompyuteri uchun) |
| **`robot_esp_arduino.ino`** | Qalqon Roboti uchun Arduino / ESP32 mikrokontroller kodi |

---

## 🤖 Qalqon Roboti qanday ulanadi va ma'lumot oladi?

Robot serveringizning **IP manzili** yoki **domeni** orqali oddiy bitta HTTP GET so'rov yuboradi.

### 1-variant: JSON format (Python, ROS, Raspberry Pi uchun)
- **URL so'rovi:** `http://SIZNING_DOMEN_YOKI_IP/gps/api.php?action=robot`
- **Qaytadigan javob:**
```json
{
  "status": "ok",
  "is_online": true,
  "age_sec": 0.4,
  "lat": 39.65432100,
  "lng": 66.95987600,
  "fix": 4,
  "fix_str": "RTK Fixed (1-2 sm)",
  "sat": 26,
  "hdop": 0.58,
  "alt": 718.4,
  "speed": 1.2,
  "server_time": "2026-11-18 10:15:00",
  "target": {
    "target_lat": 39.65480000,
    "target_lng": 66.96020000
  }
}
```

> 🛡 **Xavfsizlik himoyasi (`is_online`):** Agar ESP32 ning quvvati o'chib qolsa yoki aloqa uzilsa, `is_online: false` bo'ladi va robot eski koordinataga qarab adashib ketmasdan, o'z vaqtida xavfsiz to'xtaydi!

### 2-variant: Yengil Raw/CSV format (Arduino, STM32 uchun)
- **URL so'rovi:** `http://SIZNING_DOMEN_YOKI_IP/gps/api.php?action=robot_raw`
- **Qaytadigan javob (atigi 1 qator):**
```text
$ROBOT,39.65432100,66.95987600,4,0.58,1.2,1,0.4
```
*(Format: `$ROBOT,LAT,LNG,RTK_FIX,HDOP,TEZLIK,IS_ONLINE,AGE_SEC`)*

---

## 🎯 Veb-xaritadan Robotga yo'nalish berish (Waypoint)

1. Saytingizdagi `index.html` ni oching.
2. **🎯 Robot Maqsadi** tugmasini bosing va xaritadagi kerakli joyga bosing.
3. Ekranda masofa va maqsad koordinatasi paydo bo'ladi va serverga saqlanadi.
4. Qalqon roboti serverdan ushbu `target` koordinatasini avtomatik qabul qilib, o'sha nuqtaga qarab harakatlanadi!

---

## 📦 Ishga tushirish qadamlari

1. **Xostingga yuklash:** `api.php` va `index.html` ni veb-serveringizga yuklang.
2. **Asosiy ESP32 ni yoqing:** `track_http.ino` yuklangan ESP32 avtomatik ravishda RTK GPS ma'lumotlarini serverga uzatadi.
3. **Robotni ishga tushiring:**
   - Agar robotda Python/Raspberry Pi bo'lsa: `python3 robot_client_example.py`
   - Agar robotda Arduino/ESP bo'lsa: `robot_esp_arduino.ino` ni robot platasiga yuklang.
