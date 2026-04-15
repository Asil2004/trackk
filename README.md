# 📡 RTK GPS Tracker — ESP32 + LG290P + Firebase

Real vaqtda **santimetr aniqligida** GPS kuzatuv tizimi. ESP32 mikrokontrolleri LG290P GNSS modulidan joylashuv ma'lumotlarini oladi, NTRIP orqali RTK tuzatishlarni qo'llaydi va natijani Firebase Realtime Database'ga yuboradi.

---

## 📋 Mundarija

- [Umumiy ko'rinish](#umumiy-korinish)
- [Tizim arxitekturasi](#tizim-arxitekturasi)
- [Kerakli komponentlar](#kerakli-komponentlar)
- [Ulanish sxemasi](#ulanish-sxemasi)
- [Dasturiy ta'minot sozlamalari](#dasturiy-taminot-sozlamalari)
- [Firebase sozlash](#firebase-sozlash)
- [NTRIP sozlash](#ntrip-sozlash)
- [O'rnatish va ishga tushirish](#ornatish-va-ishga-tushirish)
- [Ma'lumot formati](#malumot-formati)
- [Muammolarni bartaraf etish](#muammolarni-bartaraf-etish)
- [Litsenziya](#litsenziya)

---

## Umumiy ko'rinish

Oddiy GPS qurilmalar 2–5 metr aniqlik beradi. Bu loyiha esa **RTK (Real-Time Kinematic)** texnologiyasidan foydalanib, aniqlikni **1–3 santimetrga** yetkazadi. Bu quyidagi holatlar uchun foydali:

- Yer uchastkalarini o'lchash (geodeziya)
- Qishloq xo'jaligi texnikasini boshqarish
- Dronlarni aniq boshqarish
- Qurilish maydonlarida joylashuvni aniqlash
- Transport vositalarini real vaqtda kuzatish

---

## Tizim arxitekturasi

```
┌──────────────────┐         RTCM tuzatish         ┌──────────────────┐
│   NTRIP Server   │ ─────────────────────────────► │                  │
│  (rtk2go.com)    │         WiFi orqali            │     ESP32        │
│  SAMARKAND baza  │                                │  mikrokontroller │
└──────────────────┘                                │                  │
                                                    │   ┌──────────┐   │
                                                    │   │ LG290P   │   │
                                                    │   │ GNSS     │◄──┼── Sun'iy yo'ldosh signallari
                                                    │   │ modul    │   │   (GPS, GLONASS, BeiDou, Galileo)
                                                    │   └──────────┘   │
                                                    └────────┬─────────┘
                                                             │ WiFi
                                                             ▼
                                                    ┌──────────────────┐
                                                    │ Firebase Realtime│
                                                    │    Database      │
                                                    ├──────────────────┤
                                                    │ /current_pos     │ ← hozirgi nuqta
                                                    │ /history         │ ← barcha nuqtalar
                                                    └────────┬─────────┘
                                                             │
                                                             ▼
                                                    ┌──────────────────┐
                                                    │  Veb-sahifa /    │
                                                    │  Mobil ilova     │
                                                    └──────────────────┘
```

---

## Kerakli komponentlar

### Apparat (Hardware)

| Komponent | Tavsif |
|-----------|--------|
| **ESP32 DevKit** | WiFi va Bluetooth modulli mikrokontroller |
| **Quectel LG290P** | Ko'p chastotali RTK GNSS moduli |
| **GNSS antenna** | Tashqi faol antenna (L1/L2/L5 tavsiya etiladi) |
| **USB kabel** | ESP32 ni kompyuterga ulash va quvvatlash uchun |
| **Breadboard / simlar** | Ulanishlar uchun |

### Dasturiy ta'minot (Software)

| Dastur | Maqsad |
|--------|--------|
| **Arduino IDE** yoki **PlatformIO** | Kodni yozish va yuklash |
| **TinyGPSPlus** kutubxonasi | NMEA jumlalarini tahlil qilish |
| **FirebaseESP32** kutubxonasi | Firebase bilan aloqa |
| **Firebase Console** | Ma'lumotlar bazasini boshqarish |

---

## Ulanish sxemasi

```
ESP32                LG290P GNSS modul
─────                ─────────────────
GPIO 27 (RX2)  ◄───  TX
GPIO 14 (TX2)  ───►  RX
3.3V           ───►  VCC
GND            ───►  GND
```

> **Muhim:** LG290P moduli 460800 baud tezlikda ishlaydi. Ulanish simlarini imkon qadar qisqa tuting.

---

## Dasturiy ta'minot sozlamalari

### 1. Arduino IDE kutubxonalarini o'rnatish

Arduino IDE da **Sketch → Include Library → Manage Libraries** bo'limiga o'ting va quyidagilarni o'rnating:

- `TinyGPSPlus` — Mikal Hart tomonidan
- `FirebaseESP32` — Mobizt tomonidan

ESP32 board'ini ham qo'shing: **File → Preferences → Additional Board Manager URLs** ga quyidagini qo'shing:

```
https://dl.espressif.com/dl/package_esp32_index.json
```

### 2. Kodni sozlash

`main.ino` faylida quyidagi qatorlarni o'z ma'lumotlaringiz bilan almashtiring:

```cpp
// WiFi
const char* ssid = "WIFI_NOMI";
const char* password = "WIFI_PAROLI";

// Firebase
#define FIREBASE_HOST "SIZNING_LOYIHA.firebasedatabase.app"
#define FIREBASE_AUTH "SIZNING_DATABASE_SECRET"

// NTRIP (agar boshqa baza stansiyasidan foydalansangiz)
const char* mountpoint = "SAMARKAND";
const char* user = "sizning@email.com";
```

---

## Firebase sozlash

1. [Firebase Console](https://console.firebase.google.com/) ga kiring
2. Yangi loyiha yarating
3. **Realtime Database** bo'limiga o'ting va baza yarating
4. Hududni tanlang (masalan, `europe-west1`)
5. Xavfsizlik qoidalarini vaqtincha ochiq qiling (test uchun):

```json
{
  "rules": {
    ".read": true,
    ".write": true
  }
}
```

6. **Project Settings → Service Accounts → Database Secrets** dan kalitni nusxalang va `FIREBASE_AUTH` ga joylashtiring

> **Ogohlantirish:** Ishlab chiqarish muhitida xavfsizlik qoidalarini cheklang. Yuqoridagi qoidalar faqat test uchun.

---

## NTRIP sozlash

Bu loyiha bepul **rtk2go.com** NTRIP caster xizmatidan foydalanadi.

### Mavjud baza stansiyalarini tekshirish

1. Brauzerda `http://rtk2go.com:2101` manzilini oching
2. Sizga yaqin bo'lgan mountpoint nomini toping
3. Koddagi `mountpoint` qiymatini shu nomga o'zgartiring

### Hisob yaratish

- `rtk2go.com` saytiga tashrif buyuring
- Email manzilingiz bilan ro'yxatdan o'ting
- Koddagi `user` maydoniga email manzilingizni yozing

> **Eslatma:** O'zbekistonda SAMARKAND mountpoint'i mavjud. Boshqa hududlar uchun [rtk2go.com](http://www.rtk2go.com/) dan tekshiring.

---

## O'rnatish va ishga tushirish

1. **Repozitoriyani klonlash:**
   ```bash
   git clone https://github.com/SIZNING_USERNAME/rtk-gps-tracker.git
   cd rtk-gps-tracker
   ```

2. **Arduino IDE da faylni oching** va yuqoridagi sozlamalarni kiriting

3. **Board sozlamalari:**
   - Board: `ESP32 Dev Module`
   - Upload Speed: `921600`
   - Flash Frequency: `80MHz`
   - Port: ESP32 ulangan COM portni tanlang

4. **Kodni yuklang** (Upload tugmasi)

5. **Serial Monitor'ni oching** (baud rate: `115200`) va natijalarni kuzating:
   ```
   WiFi ulandi!
   Current position yangilandi
   Tarixga yangi nuqta qo'shildi
   ```

---

## Ma'lumot formati

Firebase'ga quyidagi formatda ma'lumot yoziladi:

### `/current_pos` — hozirgi joylashuv (har safar ustiga yoziladi)

```json
{
  "lat": "39.65432100",
  "lng": "66.95987600",
  "fix": 24
}
```

### `/history` — joylashuv tarixi (har 3 sekundda yangi yozuv qo'shiladi)

```json
{
  "-NxAbCd123": {
    "lat": "39.65432100",
    "lng": "66.95987600",
    "fix": 24
  },
  "-NxAbCd456": {
    "lat": "39.65432200",
    "lng": "66.95987700",
    "fix": 26
  }
}
```

| Maydon | Tavsif |
|--------|--------|
| `lat` | Kenglik (latitude), 8 ta kasr belgisi bilan |
| `lng` | Uzunlik (longitude), 8 ta kasr belgisi bilan |
| `fix` | Ushlanayotgan sun'iy yo'ldoshlar soni (ko'p bo'lsa — aniqlik yuqori) |

---

## Muammolarni bartaraf etish

| Muammo | Yechim |
|--------|--------|
| WiFi ga ulanmayapti | SSID va parolni tekshiring. ESP32 faqat 2.4 GHz WiFi ni qo'llaydi |
| GPS signal yo'q | Antennani ochiq joyga qo'ying. Birinchi fix olish 1–5 daqiqa vaqt olishi mumkin |
| Firebase ga ma'lumot kelmayapti | `FIREBASE_HOST` va `FIREBASE_AUTH` qiymatlarini tekshiring. Database qoidalari ochiq ekanligiga ishonch hosil qiling |
| NTRIP ulanmayapti | Internet aloqasini tekshiring. `rtk2go.com` serveriga ping yuboring. Mountpoint nomini tekshiring |
| Serial Monitor bo'sh | Baud rate `115200` ekanligini tekshiring |
| `fix` qiymati 0 | Antenna ochiq osmonda bo'lishi kerak. NTRIP ulanishi ishlayotganligini tekshiring |
| RTK Fix olinmayapti | Baza stansiyasi 30 km dan uzoq bo'lmasligi kerak. Antenna sifatini tekshiring |

---

## Kelajakdagi rejalar

- [ ] Veb-sahifada xarita ustida real vaqtda kuzatish
- [ ] Batareya quvvatini monitoring qilish
- [ ] Geofence (hudud chegarasi) ogohlantirish tizimi
- [ ] Ma'lumotlarni SD kartaga zaxiralash (offline rejim)
- [ ] OTA (Over-The-Air) dasturiy ta'minot yangilash

---

## Litsenziya

Bu loyiha ochiq kodli bo'lib, shaxsiy va ta'lim maqsadlarida foydalanish uchun mo'ljallangan.

---

> **Muallif:** rasil8004@gmail.com  
> **Baza stansiyasi:** SAMARKAND, O'zbekiston  
> **Texnologiyalar:** ESP32, LG290P, NTRIP, Firebase Realtime Database
