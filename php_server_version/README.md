# 🌐 RTK GPS Tracker — Mustaqil PHP Server Versiyasi

Ushbu versiya **Google Firebase'siz**, o'zingizning oddiy veb-saytingiz yoki istalgan PHP xostingizda 100% mustaqil ishlaydigan tizimdir.

---

## 📁 Papka tarkibi

| Fayl | Tavsif |
|---|---|
| **`api.php`** | ESP32 dan koordinatalarni qabul qilib, `current_pos.json` va `history.json` ga saqlovchi yengil API |
| **`track_http.ino`** | ESP32 uchun dastur kodi (hech qanday Firebase kutubxonasisiz, faqat standart `HTTPClient`) |
| **`index.html`** | Real vaqtda harakat, RTK status va geodezik o'lchovlarni ko'rsatuvchi Google Satellite gibrid xaritasi |

---

## 🚀 Qanday ishga tushiriladi?

### 1-qadam: Veb-fayllarni xostingga yuklash
1. Xostingizning `public_html` (yoki uning ichidagi masalan `gps/` papkasi)ga quyidagi 2 ta faylni yuklang:
   - `api.php`
   - `index.html`
2. Papkada fayl yozish uchun ruxsat borligiga ishonch hosil qiling (standart xostinglarda `chmod 755` yoki `777` yetarli).

> 💡 **Lokal kompyuterda sinash:** Agar o'zingizda **XAMPP** yoki **OpenServer** o'rnatilgan bo'lsa, fayllarni `htdocs/gps/` papkasiga tashlab, brauzerda `http://localhost/gps/` orqali ishlatishingiz mumkin!

---

### 2-qadam: ESP32 kodini sozlash (`track_http.ino`)
1. Arduino IDE da `track_http.ino` faylini oching.
2. WiFi ma'lumotlaringizni kiriting:
   ```cpp
   const char* ssid     = "WiFi_Nomi";
   const char* password = "WiFi_Paroli";
   ```
3. O'zingizning saytingizdagi `api.php` manzilini kiriting:
   ```cpp
   const char* serverApiUrl = "https://sizningsaytingiz.uz/gps/api.php";
   ```
4. Kodni ESP32 ga yuklang (Hech qanday qo'shimcha Firebase kutubxonasi kerak emas!).

---

### 3-qadam: Kuzatish
1. Brauzerda saytingizni oching: `https://sizningsaytingiz.uz/gps/index.html`
2. ESP32 yoqilishi bilan koordinatalar ekranda jonli paydo bo'ladi.
3. Agar `index.html` boshqa manzilda bo'lsa, xarita panelidagi **⚙ Sozlamalar** tugmasini bosib, `api.php` manzilini to'g'ridan-to'g'ri kiritishingiz mumkin.

---

## 🎯 Afzalliklari
- **Hech qanday cheklovlar yo'q:** Google Firebase tokenlari yoki 30 kunlik qoidalar haqida bosh qotirmaysiz.
- **Tez va barqaror:** ESP32 bir zumda kompilatsiya bo'ladi, xotirasi to'lib qolmaydi.
- **Oson tekshirish:** Brauzerda `https://sizningsaytingiz.uz/gps/current_pos.json` deb yozib, oxirgi koordinata kelganini darhol tekshirishingiz mumkin.
