const char PAGE[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <style>
    #status { font-size: 1.6rem; font-weight: bold; color: white;
              padding: 16px; border-radius: 12px; background: gray;
              transition: background 0.5s; }
  </style>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4"></script>
</head>
<body style="font-family:sans-serif; text-align:center">
  <h1>My Room</h1>
  <div id="status">Loading…</div>
  <p>
    Temp: <b id="t">--</b> &nbsp;
    Humidity: <b id="h">--</b> &nbsp;
    Dew point: <b id="dp">--</b>
  </p>
  <canvas id="chart"></canvas>

  <script>
        // How musty is it? Returns [message, color] for a dew point in °F
    function level(dp) {
      if (dp >= 70) return ["It's NASTY in here 🤢", '#8a6d1d'];  // dark mustard
      if (dp >= 65) return ['Musty 😓',              '#c9a227'];  // mustard yellow
      if (dp >= 60) return ['Getting sticky',        '#d4b84a'];  // light mustard
      if (dp >= 50) return ['Fresh 🙂',              '#3b82f6'];  // blue
      return              ['Crisp & dry 😎',         '#1d4ed8'];  // deep blue
    }

    const INTERVAL_SEC = 5;   // MUST match SAMPLE_MS on the Arduino (60000 ms = 60 s)

    // 1. Create an empty line graph on the <canvas>
    const chart = new Chart(document.getElementById('chart'), {
      type: 'line',
      data: {
        labels: [],                                          // x-axis: times
                datasets: [{
          label: 'Dew point (°F)',
          data: [],
          borderWidth: 3,                                         // thicker line
          pointRadius: 0,                                         // no dots
          tension: 0.3,                                           // slightly curved
          segment: { borderColor: c => level(c.p1.parsed.y)[1] }  // color by mustiness
        }]
      }
    });

    // 2. Ask the Arduino for the history and draw it
    async function loadHistory() {
      const response = await fetch('/history');   // ask the Arduino
      const values = await response.json();      // turn "[55.1,55.3]" into a real list

      const now = Date.now();                      // phone's clock, in ms
      chart.data.labels = values.map((v, i) =>
        new Date(now - (values.length - 1 - i) * INTERVAL_SEC * 1000)
          .toLocaleTimeString([], { hour: 'numeric', minute: '2-digit' })
      );
      chart.data.datasets[0].data = values;
      chart.update();                              // redraw
    }

    loadHistory();   // run it once when the page opens
        // Ask the Arduino for the latest reading and show it
    async function loadLatest() {
      try {
        // ...everything you already have in here...
      
      const response = await fetch('/data');
      const d = await response.json();      // {f: 72.5, h: 45, dp: 49.8}

      document.getElementById('t').textContent  = d.f.toFixed(0) + '°F';
      document.getElementById('h').textContent  = d.h.toFixed(0) + '%';
      document.getElementById('dp').textContent = d.dp.toFixed(0) + '°F';
      }
      catch (e) {
        document.getElementById('status').textContent = 'Lost connection – retrying…';
      }
      
    }
    setInterval(loadHistory, INTERVAL_SEC * 1000);   // redraw every interval
        loadLatest();                      // once right away
              const [msg, color] = level(d.dp);              // unpack both answers
      const s = document.getElementById('status');
      s.textContent = msg;                            // change the banner text
      s.style.background = color;                     // change its color
    setInterval(loadLatest, 5000);     // then every 5 s
  </script>
</body>
</html>
)rawliteral";
