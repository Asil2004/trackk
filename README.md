# 📡 RTK GPS Tracker Pro — ESP32 + LG290P + Firebase

Real vaqt rejimida **santimetr aniqligida (1–2 sm)** ishlovchi GPS/GNSS kuzatuv va geodezik o'lchov tizimi. 
ESP32 mikrokontrolleri Quectel LG290P ko'p chastotali GNSS modulidan ma'lumotlarni qabul qiladi, NTRIP orqali RTCM tuzatishlarni qo'llaydi va natijalarni Firebase Realtime Database'ga yuboradi. Veb-interfeys esa xaritada real vaqtda harakat, RTK holati va masofalarni ko'rsatadi.

---

## 📋 Mundarija

- [Umumiy ko'rinish va RTK](#umumiy-korinish-va-rtk)
- [Tizim arxitekturasi](#tizim-arxitekturasi)
- [Kerakli komponentlar](#kerakli-komponentlar)
- [Ulanish sxemasi](#ulanish-sxemasi)
- [Firebase sozlash va xavfsizlik qoidalari](#firebase-sozlash-va-xavfsizlik-qoidalari)
- [NTRIP sozlash](#ntrip-sozlash)
- [ESP32 dasturiy ta'minoti](#esp32-dasturiy-taminoti)
- [Ma'lumotlar formati](#malumotlar-formati)
- [Veb-interfeys imkoniyatlari](#veb-interfeys-imkoniyatlari)
- [Muammolarni bartaraf etish](#muammolarni-bartaraf-etish)
- [Litsenziya](#litsenziya)

---

## Umumiy ko'rinish va RTK

Oddiy smartfon yoki avtomobil GPS qurilmalari 2–5 metr xatolik beradi. Ushbu loyiha **RTK (Real-Time Kinematic)** texnologiyasi orqali **1–2 santimetr** aniqlikni ta'minlaydi.

### RTK Fix holatlari:
| Holat kodi | Turi | Aniqlik darajasi | Tavsif |
|:---:|:---|:---:|:---|
| **4** | **RTK FIXED** | **1 – 2 sm** | Eng yuqori santimetr aniqligi. Fazaviy tuzatishlar to'liq ushlangan. |
| **5** | **RTK FLOAT** | **10 – 30 sm** | Dekimetr aniqligi. Sun'iy yo'ldosh fazalari hisoblanmoqda. |
| **2** | **DGPS** | **0.5 – 1 m** | Differensial GPS tuzatishlar bilan. |
| **1** | **ODDIY GPS** | **2 – 5 m** | Avtonom GNSS holati (NTRIP tuzatishlarsiz). |
| **0** | **NO FIX** | — | Sun'iy yo'ldosh qidirilmoqda yoki signal yo'q. |

---

## Tizim arxitekturasi

```
┌────────────────────────┐         RTCM 3.x tuzatishlar         ┌────────────────────────┐
│     NTRIP Server       │ ───────────────────────────────────► │                        │
│   (rtk2go.com caster)  │             WiFi orqali              │      ESP32 DevKit      │
│   SAMARKAND bazasi     │                                      │    (Dual Core 240MHz)  │
└────────────────────────┘                                      │                        │
                                                                │   ┌────────────────┐   │
                                                                │   │ Quectel LG290P │   │
                                                                │   │ GNSS Modul     │◄──┼── GNSS Signallari
                                                                │   │ L1/L2/L5       │   │   (GPS, GLONASS,
                                                                │   └────────────────┘   │    Galileo, BeiDou)
                                                                └───────────┬────────────┘
                                                                            │ WiFi (HTTPS/WSS)
                                                                            ▼
                                                                ┌────────────────────────┐
                                                                │    Firebase Realtime   │
                                                                │        Database        │
                                                                ├────────────────────────┤
                                                                │ /current_pos (Jonli)   │
                                                                │ /history (Harakatlar)  │
                                                                └───────────┬────────────┘
                                                                            │ Real-time Stream
                                                                            ▼
                                                                ┌────────────────────────┐
                                                                │    RTK Web Tracker     │
                                                                │  (Leaflet + Live GUI)  │
                                                                └────────────────────────┘
```

---

## Kerakli komponentlar

### Uskunalar (Hardware)
- **ESP32 DevKit v1** (30 yoki 38 pinli)
- **Quectel LG290P** yuqori aniqlikdagi ko'p chastotali RTK moduli
- **Ko'p chastotali GNSS antenna** (L1/L2/L5 qabul qiluvchi spiral yoki faol antenna)
- **Micro-USB / Type-C kabel**
- **Ulanish simlari (Jumper wires)**

### Dasturiy ta'minot (Software)
- **Arduino IDE** (v2.x tavsiya etiladi)
- `TinyGPSPlus` kutubxonasi
- `FirebaseESP32` (yoki `Firebase-ESP-Client`) kutubxonasi

---

## Ulanish sxemasi

```
ESP32                    Quectel LG290P
─────────────────        ─────────────────
GPIO 27 (RX2)     ◄────  TX
GPIO 14 (TX2)     ────►  RX
3.3V yoki 5V      ────►  VCC (Modul talabiga ko'ra)
GND               ────►  GND
```

> ⚠️ **Muhim:** LG290P moduli sukut bo'yicha **460800 baud** tezlikda ishlaydi. Simlar uzunligi 15-20 sm dan oshmasligi lozim.

---

## Firebase sozlash va xavfsizlik qoidalari

### 1. Loyiha yaratish
1. [Firebase Console](https://console.firebase.google.com/) sahifasiga kiring.
2. Yangi loyiha yarating (masalan, `rtk-gps-tracker`).
3. **Build → Realtime Database** bo'limiga kiring va baza hosil qiling.
4. Baza joylashuvini tanlang (masalan, `europe-west1`).

### 2. Xavfsizlik qoidalarini kiritish
Firebase Realtime Database → **Rules** bo'limiga kiring va quyidagi `database.rules.json` qoidasini saqlang:

```json
{
  "rules": {
    "current_pos": {
      ".read": true,
      ".write": true,
      ".validate": "newData.hasChildren(['lat', 'lng'])"
    },
    "history": {
      ".read": true,
      ".write": true,
      ".indexOn": ["ts", "lat", "lng"],
      "$item": {
        ".validate": "newData.hasChildren(['lat', 'lng'])"
      }
    }
  }
}
```

Ushbu qoida faqat to'g'ri koordinata (`lat` va `lng`) kelgandagina yozishga ruxsat beradi va tizimni ortiqcha xato ma'lumotlardan himoyalaydi.

---

## NTRIP sozlash

1. [rtk2go.com](http://www.rtk2go.com/) saytidan ro'yxatdan o'ting.
2. Ro'yxatdan o'tgan emailingizni `track.ino` dagi `user` maydoniga yozing:
   ```cpp
   const char* host       = "rtk2go.com";
   const int   port       = 2101;
   const char* mountpoint = "SAMARKAND"; // O'zingizga yaqin baza
   const char* user       = "sizning_emailingiz@gmail.com";
   const char* pw         = "none";
   ```

---

## ESP32 dasturiy ta'minoti

`track.ino` kodi quyidagi muhim yangilanishlarga ega:
- **Non-blocking NTRIP:** Aloqa uzilsa, ESP32 tiqilib (hang) qolmaydi, GPS ma'lumotlari uzluksiz o'qiladi.
- **Haqiqiy RTK Sifat Detektori:** `GNGGA` NMEA jumlasidan to'g'ridan-to'g'ri RTK Fixed (4), RTK Float (5), DGPS (2), GPS (1) holatlari ajratib olinadi.
- **Harakatlanish filtri (Deadband):** Qurilma harakatsiz turganida `/history` tuguniga minglab bir xil nuqtalar yozilmaydi. Faqat belgilangan minimal masofa (1 metr) siljigandagina tarix saqlanadi.
- **Server Timestamp:** Har bir yozuvga Firebase serverining aniq vaqt tamg'asi (`{".sv": "timestamp"}`) qo'shiladi.

---

## Ma'lumotlar formati

### `/current_pos` — Joriy joylashuv (Jonli)
```json
{
  "lat": "39.65432100",
  "lng": "66.95987600",
  "sat": 26,
  "fix": 4,
  "fix_str": "RTK Fixed (1-2 sm)",
  "hdop": 0.58,
  "alt": 718.4,
  "speed": 12.4,
  "ts": 1731868000000
}
```

### `/history` — Harakatlar tarixi
```json
{
  "-OH1aBcDeFgHiJkLmNoP": {
    "lat": "39.65432100",
    "lng": "66.95987600",
    "sat": 26,
    "fix": 4,
    "fix_str": "RTK Fixed (1-2 sm)",
    "hdop": 0.58,
    "alt": 718.4,
    "ts": 1731868000000
  }
}
```

---

## Veb-interfeys imkoniyatlari (`index.html`)

1. **Jonli monitoring:** Google Satellite gibrid xaritasida qurilmaning aniq harakati va holati.
2. **RTK Status Indikatori:** Ekranda RTK FIXED (yashil), RTK FLOAT (sariq) va GPS holatlarining vizual ko'rsatkichlari.
3. **Geodezik o'lchov (Measure Tool):**
   - Istalgancha nuqtalar qo'yib, har bir oraliq masofani hisoblash.
   - Maydon yuzasini kvadrat metr ($m^2$) va gektarda ($ga$) hisoblash.
   - O'lchov natijalarini alohida CSV shaklida yuklab olish.
4. **Eksport imkoniyatlari:**
   - **GeoJSON:** GIS, QGIS va ArcGIS dasturlari uchun.
   - **CSV:** Excel va jadval tahlillari uchun.
   - **GPX:** GPS navigatorlar uchun.
   - **Skrinshot (PNG):** Xarita va barcha statistikalarni bir tugma bilan rasm qilib saqlash.
5. **Sozlamalar (⚙):** Brauzer orqali Firebase Database URL manzilini o'zgartirish va saqlash imkoniyati.

---

## Muammolarni bartaraf etish

| Muammo | Sababi | Yechim |
|---|---|---|
| **Firebase da Permission Denied** | Database Rules yopiq yoki muddati o'tgan | `database.rules.json` dagi qoidalarni Realtime Database → Rules bo'limiga yuklang. |
| **Baza tez to'lib ketyapti** | Har soniyada takroriy nuqtalar yozilgan | Yangi koddagi masofa filtri (`MIN_DISTANCE_MOVE_M = 1.0`) va veb-interfeysdagi buffer to'g'rilandi. |
| **RTK Fix (4) olinmayapti** | NTRIP server bilan aloqa yo'q yoki baza uzoqda | `rtk2go.com` da `SAMARKAND` mountpoint faolligini tekshiring. Antenna ochiq osmon ostida bo'lishi kerak. |
| **Serial Monitor'da noaniq belgilar** | Noto'g'ri baud rate | Serial monitor tezligini `115200` ga, LG290P tezligini esa `460800` ga sozlang. |
| **Kutubxona xatosi (compile error)** | `FirebaseESP32` versiya mos kelmasligi | Arduino IDE da `Firebase ESP32 Client by Mobizt` kutubxonasini yangilang. |

---

## Litsenziya
Ushbu loyiha ochiq kodli bo'lib, MIT litsenziyasi asosida tarqatiladi.
