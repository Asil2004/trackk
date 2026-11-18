#!/usr/bin/env python3
"""
Qalqon Roboti uchun GPS qabul qiluvchi mijoz (Python)
Serverdan (IP yoki domen orqali) santimetr aniqlikdagi RTK koordinatalarini oladi.
Raspberry Pi, Jetson Nano yoki robotning boshqaruv kompyuterida ishlatish uchun.
"""

import time
import requests
import math

# ==============================================================================
# SOZLAMALAR: O'zingizning serveringiz IP manzili yoki domenini yozing
# Masalan: "http://192.168.1.100/gps/api.php" yoki "https://saytingiz.uz/gps/api.php"
# ==============================================================================
SERVER_API_URL = "http://sizningsaytingiz.uz/gps/api.php"
POLL_INTERVAL_SEC = 1.0  # Har 1 soniyada koordinatani yangilash


def calculate_distance(lat1, lon1, lat2, lon2):
    """Ikki nuqta orasidagi masofani hisoblash (metrlarda)"""
    R = 6371000.0
    d_lat = math.radians(lat2 - lat1)
    d_lon = math.radians(lon2 - lon1)
    a = (math.sin(d_lat / 2) ** 2 +
         math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) *
         math.sin(d_lon / 2) ** 2)
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    return R * c


def get_robot_gps():
    """Serverdan robotning joriy RTK koordinatasini olish"""
    try:
        url = f"{SERVER_API_URL}?action=robot&_={int(time.time()*1000)}"
        response = requests.get(url, timeout=3.0)
        if response.status_code == 200:
            return response.json()
        else:
            print(f"[Xato] Server kodi: {response.status_code}")
            return None
    except Exception as e:
        print(f"[Xato] Ulanib bo'lmadi: {e}")
        return None


def main():
    print("=" * 60)
    print("  Qalqon Roboti — GPS Navigatsiya mijozi ishga tushdi")
    print(f"  Server: {SERVER_API_URL}")
    print("=" * 60)

    while True:
        data = get_robot_gps()

        if data and data.get("status") == "ok":
            lat = data["lat"]
            lng = data["lng"]
            fix = data["fix"]
            fix_str = data["fix_str"]
            hdop = data["hdop"]
            is_online = data["is_online"]
            age_sec = data["age_sec"]

            # Xavfsizlik tekshiruvi: ESP32 aloqasi uzilmaganmi?
            if not is_online:
                print(f"[OGOHLANTIRISH] GPS ma'lumoti eskirgan ({age_sec}s oldin). Robot to'xtatildi!")
            else:
                print(f"[ROBOT GPS] Lat: {lat:.8f}, Lng: {lng:.8f} | Holat: {fix_str} (fix={fix}) | Xato: {hdop:.2f}m")

                # Agar xaritadan robotga maqsad koordinatasi yuborilgan bo'lsa
                target = data.get("target")
                if target and "target_lat" in target and "target_lng" in target:
                    t_lat = target["target_lat"]
                    t_lng = target["target_lng"]
                    dist = calculate_distance(lat, lng, t_lat, t_lng)
                    print(f"  --> Maqsadgacha masofa: {dist:.2f} metr (Maqsad: {t_lat:.8f}, {t_lng:.8f})")

                    if dist < 1.0:
                        print("  --> [YETIB KELDINGIZ] Qalqon roboti maqsad nuqtasiga yetdi!")
        else:
            print("[Kutilmoqda] Serverdan GPS ma'lumoti olinmadi...")

        time.sleep(POLL_INTERVAL_SEC)


if __name__ == "__main__":
    main()
