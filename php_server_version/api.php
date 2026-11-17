<?php
/**
 * RTK Tracker — Oddiy PHP API
 * Firebase'siz, xostingda ishlaydigan mustaqil backend
 */

header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");
header("Content-Type: application/json; charset=UTF-8");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit;
}

$currentPosFile = __DIR__ . '/current_pos.json';
$historyFile    = __DIR__ . '/history.json';
$maxHistoryPoints = 1500; // Tarixda saqlanadigan maksimal nuqtalar soni

// ============================================================================
// 1. POST SO'ROVI: ESP32 dan yangi GPS koordinatalarini qabul qilish
// ============================================================================
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $rawInput = file_get_contents('php://input');
    $data = json_decode($rawInput, true);

    if (!$data || !isset($data['lat']) || !isset($data['lng'])) {
        http_response_code(400);
        echo json_encode(["status" => "error", "message" => "Yaroqsiz koordinatalar"]);
        exit;
    }

    // Server vaqt tamg'asi (millisekundda)
    if (!isset($data['ts']) || empty($data['ts'])) {
        $data['ts'] = round(microtime(true) * 1000);
    }
    $data['server_time'] = date('Y-m-d H:i:s');

    // A) Hozirgi joylashuvni yangilash (current_pos.json)
    file_put_contents($currentPosFile, json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE), LOCK_EX);

    // B) Tarixga qo'shish (history.json)
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

    // Tarix hajmini cheklash (ortiqcha xotira olmasligi uchun)
    if (count($history) > $maxHistoryPoints) {
        $history = array_slice($history, -$maxHistoryPoints);
    }

    file_put_contents($historyFile, json_encode($history, JSON_UNESCAPED_UNICODE), LOCK_EX);

    echo json_encode(["status" => "success", "message" => "Joylashuv saqlandi"]);
    exit;
}

// ============================================================================
// 2. GET SO'ROVI: Veb-sahifa uchun ma'lumotlarni qaytarish
// ============================================================================
$action = isset($_GET['action']) ? $_GET['action'] : 'pos';

// A) Faqat joriy joylashuvni olish
if ($action === 'pos') {
    if (file_exists($currentPosFile)) {
        echo file_get_contents($currentPosFile);
    } else {
        echo json_encode([
            "status"  => "waiting",
            "message" => "Hali GPS ma'lumoti kelmagan"
        ]);
    }
    exit;
}

// B) Harakatlar tarixini olish
if ($action === 'history') {
    if (file_exists($historyFile)) {
        echo file_get_contents($historyFile);
    } else {
        echo json_encode([]);
    }
    exit;
}

// C) Tarixni tozalash
if ($action === 'clear') {
    if (file_exists($historyFile)) {
        file_put_contents($historyFile, json_encode([]), LOCK_EX);
    }
    echo json_encode(["status" => "success", "message" => "Tarix tozalandi"]);
    exit;
}

http_response_code(404);
echo json_encode(["status" => "error", "message" => "Noma'lum harakat"]);
