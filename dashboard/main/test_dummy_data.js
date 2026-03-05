<!DOCTYPE html>
<html>
<head>
  <title>BAJA Dashboard - Test Mode</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <style>
    body { 
      font-family: sans-serif; 
      background: #1a1a2e; 
      color: #f1f5f9; 
      padding: 20px; 
      text-align: center;
    }
    .btn {
      padding: 12px 24px;
      margin: 10px;
      background: #c8b6ff;
      border: none;
      border-radius: 8px;
      font-weight: 600;
      cursor: pointer;
      color: #1a1a2e;
    }
    .btn:hover { filter: brightness(1.1); }
    .log {
      max-width: 800px;
      margin: 20px auto;
      background: #1f2937;
      padding: 15px;
      border-radius: 8px;
      text-align: left;
      font-family: monospace;
      font-size: 0.85rem;
      max-height: 300px;
      overflow-y: auto;
    }
  </style>
</head>
<body>
  <h1>🧪 BAJA Dashboard Test Server</h1>
  <p>Simulates ESP32 WebSocket + REST API for dashboard testing</p>
  
  <div>
    <button class="btn" onclick="startServer()">▶ Start Simulation</button>
    <button class="btn" onclick="stopServer()">■ Stop</button>
    <button class="btn" onclick="toggleGPSQuality()">📡 Toggle GPS Quality</button>
  </div>
  
  <div class="log" id="log"></div>
  
  <p style="margin-top:30px;font-size:0.9rem;color:#94a3b8">
    Open <code>index.html</code> in another tab. It will auto-detect test mode.<br>
    Or visit: <a href="http://localhost:8080/index.html" style="color:#a7c7e7">http://localhost:8080/index.html</a>
  </p>

  <script>
    let wsServer = null;
    let httpServer = null;
    let sending = false;
    let goodGPS = true;
    
    function log(msg) {
      const el = document.getElementById('log');
      el.innerHTML = `[${new Date().toLocaleTimeString()}] ${msg}<br>` + el.innerHTML;
      if (el.children.length > 50) el.lastChild.remove();
    }

    function generateFrame() {
      const baseLat = -25.7821, baseLon = 28.2741;
      const time = Date.now() / 1000;
      
      // Simulate vehicle dynamics
      const speed = 8 + 10 * Math.sin(time * 0.3); // 8-18 m/s
      const trackPct = (time * 2) % 100; // 2% per second
      const lap = Math.floor(time / 30) + 1; // New lap every 30s
      
      // GPS with optional degradation
      const noise = goodGPS ? 0.00001 : 0.0005;
      const lat = baseLat + 0.001 * Math.sin(trackPct/100 * Math.PI*2) + (Math.random()-0.5)*noise;
      const lon = baseLon + 0.001 * Math.cos(trackPct/100 * Math.PI*2) + (Math.random()-0.5)*noise;
      
      return {
        ts: Date.now(),
        speed_ms: Math.max(0, speed),
        lat: lat,
        lon: lon,
        track_pct: trackPct,
        lap: lap,
        wheel_slip_l: (Math.random() - 0.5) * 0.08,
        wheel_slip_r: (Math.random() - 0.5) * 0.08,
        session: Math.random() > 0.95 ? (Math.random() > 0.5 ? 'active' : 'idle') : 'active',
        hdop: goodGPS ? (0.7 + Math.random()*0.5) : (2.0 + Math.random()*3.0),
        fix: goodGPS ? (Math.random() > 0.1 ? 3 : 2) : (Math.random() > 0.7 ? 1 : 0)
      };
    }

    function startServer() {
      if (sending) return;
      sending = true;
      log('🚀 Starting simulation server...');
      
      // Mock WebSocket via broadcast to all dashboard tabs
      window.addEventListener('storage', (e) => {
        if (e.key === 'telemetry_frame' && sending) {
          // Dashboard will read this via polling fallback
        }
      });
      
      // Polling fallback for WebSocket (since we can't run real WS in file://)
      window.__testFrames = [];
      
      const interval = setInterval(() => {
        if (!sending) { clearInterval(interval); return; }
        
        const frame = generateFrame();
        window.__testFrames.unshift(frame);
        if (window.__testFrames.length > 10) window.__testFrames.pop();
        
        // Also broadcast via localStorage for dashboard polling
        localStorage.setItem('telemetry_frame', JSON.stringify(frame));
        
        if (Math.random() < 0.1) log(`📡 Broadcasting: ${frame.speed_ms*3.6|0} km/h, lap ${frame.lap}`);
      }, 100); // 10 Hz
      
      log('✓ Simulation running at 10 Hz');
      log('💡 Tip: Press Ctrl+T in dashboard to toggle test mode');
    }

    function stopServer() {
      sending = false;
      log('⏹ Simulation stopped');
    }
    
    function toggleGPSQuality() {
      goodGPS = !goodGPS;
      log(`📡 GPS quality: ${goodGPS ? '✅ Good (HDOP <2)' : '⚠️ Poor (HDOP >2)'}`);
    }

    // REST API mock via Service Worker (for fetch calls from dashboard)
    if ('serviceWorker' in navigator) {
      navigator.serviceWorker.register('data:text/javascript,' + encodeURIComponent(`
        self.addEventListener('fetch', (event) => {
          const url = event.request.url;
          
          // Mock /sessions endpoint
          if (url.includes('/sessions')) {
            const sessions = [
              { id: 'sess_001', name: 'Qualifying Lap', duration: 124, laps: 3, size: 45200 },
              { id: 'sess_002', name: 'Race Heat 1', duration: 487, laps: 8, size: 182400 }
            ];
            
            event.respondWith(Response.json(sessions));
            return;
          }
          
          // Mock /track/map GET
          if (url.includes('/track/map') && event.request.method === 'GET') {
            const track = {
              type: 'LineString',
              coordinates: [
                [28.2741, -25.7821], [28.2745, -25.7818], [28.2752, -25.7822],
                [28.2748, -25.7828], [28.2741, -25.7821] // Closed loop
              ]
            };
            event.respondWith(Response.json(track));
            return;
          }
          
          // Mock POST endpoints
          if (url.includes('/session/start') || url.includes('/session/stop') || url.includes('/track/map')) {
            event.respondWith(new Response('OK', { status: 200 }));
            return;
          }
        });
      `));
    }
  </script>
</body>
</html>