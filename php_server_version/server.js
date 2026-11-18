/**
 * RTK Tracker & Robot — Node.js Lokal Server
 * Hech qanday qo'shimcha paketlar (npm install) kerak emas!
 * Faqat standart Node.js kutubxonalarida ishlaydi.
 */

const http = require('http');
const fs = require('fs');
const path = require('path');
const url = require('url');

const PORT = process.env.PORT || 3000;
const currentPosFile = path.join(__dirname, 'current_pos.json');
const historyFile    = path.join(__dirname, 'history.json');
const targetPosFile  = path.join(__dirname, 'target_pos.json');
const maxHistoryPoints = 1500;
const onlineTimeoutMs  = 6000;

function sendCors(res) {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
}

const server = http.createServer((req, res) => {
    sendCors(res);

    if (req.method === 'OPTIONS') {
        res.writeHead(200);
        res.end();
        return;
    }

    const parsedUrl = url.parse(req.url, true);
    const pathname = parsedUrl.pathname;
    const action = parsedUrl.query.action || 'pos';

    // =========================================================================
    // 1. API SO'ROVLARI (/api.php yoki /api)
    // =========================================================================
    if (pathname === '/api.php' || pathname === '/api' || pathname.endsWith('api.php')) {
        // POST SO'ROVLARI
        if (req.method === 'POST') {
            let body = '';
            req.on('data', chunk => { body += chunk.toString(); });
            req.on('end', () => {
                let data = null;
                try {
                    data = JSON.parse(body);
                } catch (e) {
                    res.writeHead(400, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: 'error', message: 'Yaroqsiz JSON' }));
                    return;
                }

                // A) Maqsad koordinatasini saqlash
                if (action === 'set_target') {
                    if (!data || data.lat === undefined || data.lng === undefined) {
                        res.writeHead(400, { 'Content-Type': 'application/json' });
                        res.end(JSON.stringify({ status: 'error', message: 'Maqsad koordinatasi xato' }));
                        return;
                    }
                    const targetData = {
                        target_lat: parseFloat(data.lat),
                        target_lng: parseFloat(data.lng),
                        set_time: new Date().toISOString(),
                        ts: Date.now()
                    };
                    fs.writeFileSync(targetPosFile, JSON.stringify(targetData, null, 2));
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: 'success', message: 'Robot maqsadi saqlandi', target: targetData }));
                    return;
                }

                // B) ESP32 dan kelgan koordinatalar
                if (!data || data.lat === undefined || data.lng === undefined) {
                    res.writeHead(400, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ status: 'error', message: 'Koordinata topilmadi' }));
                    return;
                }

                const serverNow = Date.now();
                data.server_ts = serverNow;
                data.server_time = new Date().toLocaleTimeString('uz') + ' ' + new Date().toLocaleDateString('uz');

                fs.writeFileSync(currentPosFile, JSON.stringify(data, null, 2));

                // Tarixga qo'shish
                let history = [];
                if (fs.existsSync(historyFile)) {
                    try {
                        history = JSON.parse(fs.readFileSync(historyFile, 'utf8'));
                    } catch (e) { history = []; }
                }

                history.push({
                    lat: parseFloat(data.lat),
                    lng: parseFloat(data.lng),
                    sat: data.sat ? parseInt(data.sat) : 0,
                    fix: data.fix ? parseInt(data.fix) : 0,
                    hdop: data.hdop ? parseFloat(data.hdop) : 0,
                    alt: data.alt ? parseFloat(data.alt) : 0,
                    speed: data.speed ? parseFloat(data.speed) : 0,
                    ts: serverNow
                });

                if (history.length > maxHistoryPoints) {
                    history = history.slice(-maxHistoryPoints);
                }
                fs.writeFileSync(historyFile, JSON.stringify(history));

                console.log(`[ESP32 -> Server] Lat: ${data.lat}, Lng: ${data.lng} | RTK: ${data.fix} | Sat: ${data.sat}`);

                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ status: 'success', message: 'Joylashuv saqlandi' }));
            });
            return;
        }

        // GET SO'ROVLARI
        if (req.method === 'GET') {
            // Robot uchun JSON
            if (action === 'robot') {
                res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
                if (!fs.existsSync(currentPosFile)) {
                    res.end(JSON.stringify({ status: 'waiting', is_online: false, message: 'Kutilmoqda' }));
                    return;
                }
                const current = JSON.parse(fs.readFileSync(currentPosFile, 'utf8'));
                const serverNow = Date.now();
                const ageMs = Math.max(0, serverNow - (current.server_ts || serverNow));
                const isOnline = (ageMs <= onlineTimeoutMs);

                let target = null;
                if (fs.existsSync(targetPosFile)) {
                    try { target = JSON.parse(fs.readFileSync(targetPosFile, 'utf8')); } catch (e) {}
                }

                res.end(JSON.stringify({
                    status: 'ok',
                    is_online: isOnline,
                    age_ms: ageMs,
                    age_sec: +(ageMs / 1000).toFixed(1),
                    lat: parseFloat(current.lat),
                    lng: parseFloat(current.lng),
                    fix: parseInt(current.fix) || 0,
                    fix_str: current.fix_str || 'No Fix',
                    sat: parseInt(current.sat) || 0,
                    hdop: parseFloat(current.hdop) || 99.9,
                    alt: parseFloat(current.alt) || 0,
                    speed: parseFloat(current.speed) || 0,
                    server_time: current.server_time,
                    target: target
                }, null, 2));
                return;
            }

            // Robot uchun Raw CSV
            if (action === 'robot_raw') {
                res.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8' });
                if (!fs.existsSync(currentPosFile)) {
                    res.end('$ROBOT,0,0,0,99.9,0.0,0,9999\n');
                    return;
                }
                const current = JSON.parse(fs.readFileSync(currentPosFile, 'utf8'));
                const serverNow = Date.now();
                const ageMs = Math.max(0, serverNow - (current.server_ts || serverNow));
                const isOnline = (ageMs <= onlineTimeoutMs) ? 1 : 0;
                const ageSec = +(ageMs / 1000).toFixed(1);

                res.end(`$ROBOT,${parseFloat(current.lat).toFixed(8)},${parseFloat(current.lng).toFixed(8)},${current.fix || 0},${parseFloat(current.hdop || 99.9).toFixed(2)},${parseFloat(current.speed || 0).toFixed(1)},${isOnline},${ageSec}\n`);
                return;
            }

            // Veb-sahifa joriy holati
            if (action === 'pos') {
                res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
                if (fs.existsSync(currentPosFile)) {
                    res.end(fs.readFileSync(currentPosFile, 'utf8'));
                } else {
                    res.end(JSON.stringify({ status: 'waiting', message: 'Hali GPS ma\'lumoti kelmagan' }));
                }
                return;
            }

            // Tarix
            if (action === 'history') {
                res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
                if (fs.existsSync(historyFile)) {
                    res.end(fs.readFileSync(historyFile, 'utf8'));
                } else {
                    res.end(JSON.stringify([]));
                }
                return;
            }

            // Tozalash
            if (action === 'clear') {
                if (fs.existsSync(historyFile)) fs.writeFileSync(historyFile, JSON.stringify([]));
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ status: 'success', message: 'Tarix tozalandi' }));
                return;
            }

            if (action === 'clear_target') {
                if (fs.existsSync(targetPosFile)) fs.unlinkSync(targetPosFile);
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ status: 'success', message: 'Maqsad tozalandi' }));
                return;
            }
        }
    }

    // =========================================================================
    // 2. STATIK FAYLLAR (index.html, rasmlar va h.k.)
    // =========================================================================
    let filePath = pathname === '/' ? '/index.html' : pathname;
    const ext = path.extname(filePath);
    const absPath = path.join(__dirname, filePath);

    const mimeTypes = {
        '.html': 'text/html',
        '.css':  'text/css',
        '.js':   'application/javascript',
        '.json': 'application/json',
        '.png':  'image/png',
        '.jpg':  'image/jpeg'
    };

    if (fs.existsSync(absPath) && fs.statSync(absPath).isFile()) {
        res.writeHead(200, { 'Content-Type': mimeTypes[ext] || 'text/plain' });
        fs.createReadStream(absPath).pipe(res);
    } else {
        res.writeHead(404, { 'Content-Type': 'text/plain' });
        res.end('Fayl topilmadi: 404');
    }
});

server.listen(PORT, '0.0.0.0', () => {
    console.log('====================================================');
    console.log(`  RTK GPS & Robot Server ishga tushdi!`);
    console.log(`  Port: ${PORT}`);
    console.log(`  Kompyuterda ochish:   http://localhost:${PORT}`);
    console.log(`  ESP32 uchun manzil:   http://<KOMPYUTER_IP>:${PORT}/api.php`);
    console.log('====================================================');
});
