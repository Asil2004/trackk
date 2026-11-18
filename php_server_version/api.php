<?php
/**
 * RTK Tracker & Robot Navigation API
 * ESP32 va Qalqon Roboti uchun markaziy server kodi
 */

header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit;
}

$currentPosFile   = __DIR__ . '/current_pos.json';
$historyFile      = __DIR__ . '/history.json';
$targetPosFile    = __DIR__ . '/target_pos.json';
$maxHistoryPoints = 1500; // Tarixda saqlanadigan maksimal nuqtalar soni
$onlineTimeoutMs  = 6000; // 6 soniya ichida yangilansa — robot online hisoblanadi

// ============================================================================
// 1. POST SO'ROVLARI (ESP32 yoki Veb-xaritadan yozish)
// ============================================================================
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $rawInput = file_get_contents('php://input');
    $data = json_decode($rawInput, true);

    // A) Robot uchun maqsad koordinatasini saqlash (?action=set_target)
    if (isset($_GET['action']) && $_GET['action'] === 'set_target') {
        header("Content-Type: application/json; charset=UTF-8");
        if (!$data || !isset($data['lat']) || !isset($data['lng'])) {
            http_response_code(400);
            echo json_encode(["status" => "error", "message" => "Maqsad koordinatalari noto'g'ri"]);
            exit;
        }
        $targetData = [
            "target_lat" => (float)$data['lat'],
            "target_lng" => (float)$data['lng'],
            "set_time"   => date('Y-m-d H:i:s'),
            "ts"         => round(microtime(true) * 1000)
        ];
        file_put_contents($targetPosFile, json_encode($targetData, JSON_PRETTY_PRINT), LOCK_EX);
        echo json_encode(["status" => "success", "message" => "Robot maqsadi saqlandi", "target" => $targetData]);
        exit;
    }

    // B) ESP32 dan yangi GPS koordinatalarini qabul qilish (Asosiy funksiya)
    header("Content-Type: application/json; charset=UTF-8");
    if (!$data || !isset($data['lat']) || !isset($data['lng'])) {
        http_response_code(400);
        echo json_encode(["status" => "error", "message" => "Yaroqsiz koordinatalar"]);
        exit;
    }

    // Server vaqt tamg'asi (millisekundda)
    $serverNow = round(microtime(true) * 1000);
    if (!isset($data['ts']) || empty($data['ts'])) {
        $data['ts'] = $serverNow;
    }
    $data['server_ts']   = $serverNow;
    $data['server_time'] = date('Y-m-d H:i:s');

    // 1) Hozirgi joylashuvni yangilash (current_pos.json)
    file_put_contents($currentPosFile, json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE), LOCK_EX);

    // 2) Tarixga qo'shish (history.json)
    $history = [];
    if (file_exists($historyFile)) {
        $existingHistory = json_decode(file_get_contents($historyFile), true);
        if (is_array($existingHistory)) {
            $history = $existingHistory;
        }
    }

    $history[] = [
        "lat"   => (float)$data['lat'],
        "lng"   => (float)$data['lng'],
        "sat"   => isset($data['sat']) ? (int)$data['sat'] : 0,
        "fix"   => isset($data['fix']) ? (int)$data['fix'] : 0,
        "hdop"  => isset($data['hdop']) ? (float)$data['hdop'] : 0,
        "alt"   => isset($data['alt']) ? (float)$data['alt'] : 0,
        "speed" => isset($data['speed']) ? (float)$data['speed'] : 0,
        "ts"    => $data['ts']
    ];

    if (count($history) > $maxHistoryPoints) {
        $history = array_slice($history, -$maxHistoryPoints);
    }
    file_put_contents($historyFile, json_encode($history, JSON_UNESCAPED_UNICODE), LOCK_EX);

    echo json_encode(["status" => "success", "message" => "Joylashuv saqlandi"]);
    exit;
}

// ============================================================================
// 2. GET SO'ROVLARI: Veb-sahifa va Robot uchun ma'lumot berish
// ============================================================================
$action = isset($_GET['action']) ? $_GET['action'] : 'pos';

// A) ROBOT UCHUN MAXSUS JSON FORMAT (?action=robot)
// Qalqon roboti IP yoki domen orqali o'qiydigan asosiy endpoint
if ($action === 'robot') {
    header("Content-Type: application/json; charset=UTF-8");
    if (!file_exists($currentPosFile)) {
        echo json_encode([
            "status"    => "waiting",
            "is_online" => false,
            "message"   => "Hali GPS koordinatalari kelmagan"
        ]);
        exit;
    }

    $current = json_decode(file_get_contents($currentPosFile), true);
    $serverNow = round(microtime(true) * 1000);
    $posTs = isset($current['server_ts']) ? $current['server_ts'] : (isset($current['ts']) ? $current['ts'] : $serverNow);
    $ageMs = max(0, $serverNow - $posTs);
    $isOnline = ($ageMs <= $onlineTimeoutMs);

    // Robot uchun maqsad bormi?
    $target = null;
    if (file_exists($targetPosFile)) {
        $target = json_decode(file_get_contents($targetPosFile), true);
    }

    echo json_encode([
        "status"      => "ok",
        "is_online"   => $isOnline,
        "age_ms"      => $ageMs,
        "age_sec"     => round($ageMs / 1000, 1),
        "lat"         => (float)$current['lat'],
        "lng"         => (float)$current['lng'],
        "fix"         => isset($current['fix']) ? (int)$current['fix'] : 0,
        "fix_str"     => isset($current['fix_str']) ? $current['fix_str'] : 'No Fix',
        "sat"         => isset($current['sat']) ? (int)$current['sat'] : 0,
        "hdop"        => isset($current['hdop']) ? (float)$current['hdop'] : 99.9,
        "alt"         => isset($current['alt']) ? (float)$current['alt'] : 0.0,
        "speed"       => isset($current['speed']) ? (float)$current['speed'] : 0.0,
        "server_time" => isset($current['server_time']) ? $current['server_time'] : date('Y-m-d H:i:s'),
        "target"      => $target
    ], JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE);
    exit;
}

// B) ROBOT UCHUN BIR QATORLIK RAW / CSV FORMAT (?action=robot_raw)
// Kichik mikrokontrollerlar (Arduino/ESP/STM) uchun JSON siz, 0 ms da o'qish formati:
// $ROBOT,LAT,LNG,FIX,HDOP,SPEED,IS_ONLINE,AGE_SEC
if ($action === 'robot_raw') {
    header("Content-Type: text/plain; charset=UTF-8");
    if (!file_exists($currentPosFile)) {
        echo "\$ROBOT,0,0,0,99.9,0.0,0,9999\n";
        exit;
    }
    $current = json_decode(file_get_contents($currentPosFile), true);
    $serverNow = round(microtime(true) * 1000);
    $posTs = isset($current['server_ts']) ? $current['server_ts'] : $serverNow;
    $ageMs = max(0, $serverNow - $posTs);
    $isOnline = ($ageMs <= $onlineTimeoutMs) ? 1 : 0;
    $ageSec = round($ageMs / 1000, 1);

    printf("\$ROBOT,%.8f,%.8f,%d,%.2f,%.1f,%d,%.1f\n",
        (float)$current['lat'],
        (float)$current['lng'],
        isset($current['fix']) ? (int)$current['fix'] : 0,
        isset($current['hdop']) ? (float)$current['hdop'] : 99.9,
        isset($current['speed']) ? (float)$current['speed'] : 0.0,
        $isOnline,
        $ageSec
    );
    exit;
}

// C) Veb-sahifa uchun joriy joylashuv (?action=pos)
if ($action === 'pos') {
    header("Content-Type: application/json; charset=UTF-8");
    if (file_exists($currentPosFile)) {
        echo file_get_contents($currentPosFile);
    } else {
        echo json_encode(["status" => "waiting", "message" => "Hali GPS ma'lumoti kelmagan"]);
    }
    exit;
}

// D) Harakatlar tarixini olish (?action=history)
if ($action === 'history') {
    header("Content-Type: application/json; charset=UTF-8");
    if (file_exists($historyFile)) {
        echo file_get_contents($historyFile);
    } else {
        echo json_encode([]);
    }
    exit;
}

// E) Tarixni tozalash (?action=clear)
if ($action === 'clear') {
    header("Content-Type: application/json; charset=UTF-8");
    if (file_exists($historyFile)) {
        file_put_contents($historyFile, json_encode([]), LOCK_EX);
    }
    echo json_encode(["status" => "success", "message" => "Tarix tozalandi"]);
    exit;
}

// F) Maqsad koordinatasini olish (?action=get_target)
if ($action === 'get_target') {
    header("Content-Type: application/json; charset=UTF-8");
    if (file_exists($targetPosFile)) {
        echo file_get_contents($targetPosFile);
    } else {
        echo json_encode(["status" => "none", "target" => null]);
    }
    exit;
}

// G) Maqsad koordinatasini tozalash (?action=clear_target)
if ($action === 'clear_target') {
    header("Content-Type: application/json; charset=UTF-8");
    if (file_exists($targetPosFile)) {
        unlink($targetPosFile);
    }
    echo json_encode(["status" => "success", "message" => "Maqsad tozalandi"]);
    exit;
}

header("Content-Type: application/json; charset=UTF-8");
http_response_code(404);
echo json_encode(["status" => "error", "message" => "Noma'lum buyruq"]);
