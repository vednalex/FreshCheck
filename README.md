# FreshCheck

A Wi-Fi room monitor built on the **Arduino UNO R4 WiFi** and a **DHT11** temperature/humidity sensor. The Arduino hosts its own web dashboard, so any phone or laptop on the same Wi-Fi network can open it in a browser. You don't need an app, a cloud account, or a USB cable.

The dashboard shows:

- **Live temperature, humidity, and dew point**, updated every 5 seconds
- **A color-coded status banner**, from blue ("Fresh 🙂") to mustard ("It's NASTY in here 🤢")
- **A 3-hour graph of dew point** with the line colored by how musty the room was at each moment

> **Why dew point?** Relative humidity depends on temperature, so the same "50%" can feel fine or swampy. Dew point measures the actual amount of water in the air, which makes it a better indicator of how muggy or musty a room feels.

---

## Table of Contents

1. [How to Use](#1-how-to-use)
2. [Main Sketch – `FreshCheck.ino`](#2-main-sketch--freshcheckino)
3. [Wi-Fi Credentials – `arduino_secrets.h`](#3-wi-fi-credentials--arduino_secretsh)
4. [Web Page – `page.h`](#4-web-page--pageh)
5. [Customizing](#5-customizing)
6. [Troubleshooting](#6-troubleshooting)
7. [Limitations](#7-limitations)

---

## 1. How to Use

### Hardware

| Part | Notes |
|---|---|
| Arduino UNO R4 WiFi | Has built-in Wi-Fi (ESP32-S3 module) |
| DHT11 sensor | 3-pin module or 4-pin bare sensor |
| Jumper wires | 3 wires |
| USB-C cable | For uploading. Afterwards, any USB phone charger can power it |
| 10 kΩ resistor | **Only** for a bare 4-pin DHT11 (modules already have one) |

### Wiring

**3-pin DHT11 module** (pins usually labeled `S`/`DATA`, `+`/`VCC`, `-`/`GND`):

| DHT11 pin | Arduino UNO R4 WiFi pin |
|---|---|
| `S` / `DATA` / `OUT` | **D11** |
| `+` / `VCC` | **5V** |
| `-` / `GND` | **GND** |

**Bare 4-pin DHT11** (pins numbered 1 → 4, left to right, with the grille facing you):

| DHT11 pin | Connect to |
|---|---|
| 1 – VCC | **5V** |
| 2 – DATA | **D11**, plus a **10 kΩ resistor from DATA to 5V** (pull-up) |
| 3 – NC | Not connected |
| 4 – GND | **GND** |

> To use a different data pin, change `#define DHTPIN 11` at the top of the sketch.

### Software setup

1. **Install the [Arduino IDE](https://www.arduino.cc/en/software)** (version 2.x recommended).
2. **Install the board package:** *Tools → Board → Boards Manager*, search **"Arduino UNO R4 Boards"**, and install. This also installs the `WiFiS3` library.
3. **Install the sensor library:** *Tools → Manage Libraries*, search **"DHT sensor library"** by **Adafruit**, and install it. When asked, also install its dependency, **"Adafruit Unified Sensor"**.
4. **Download this repository** and open `FreshCheck/FreshCheck.ino` in the Arduino IDE.
5. **Add your Wi-Fi credentials** (see [section 3](#3-wi-fi-credentials--arduino_secretsh)):
   - Copy `arduino_secrets.example.h` and rename the copy to **`arduino_secrets.h`**
   - Fill in your network name and password
6. **Select the board and port:** *Tools → Board → Arduino UNO R4 WiFi*, then *Tools → Port*.
7. **Upload** (→ button).

### Opening the dashboard

1. Open the **Serial Monitor** (*Tools → Serial Monitor*) and set it to **9600 baud**.
2. Wait for:
   ```
   Connecting to YOUR_WIFI_NAME
   Connection Secured! IP Address: 192.168.1.42
   ```
3. On a phone or laptop **connected to the same Wi-Fi**, open a browser and go to:
   ```
   http://192.168.1.42
   ```
   (Use your own IP, and type `http://`, not `https://`.)
4. **Optional:** use *Add to Home Screen* (Safari share menu / Chrome ⋮ menu) so it opens like an app.

> 💡 The router may assign a different IP after a restart. To keep it fixed, set up a **DHCP reservation** for the Arduino in your router's settings.

---

## 2. Main Sketch – `FreshCheck.ino`

The main sketch does three jobs at once: **reads the sensor**, **remembers the last 3 hours**, and **runs a small web server**. It uses `millis()` timers instead of `delay()`, so the loop never blocks and the board can answer the browser at any moment.

### Configuration (top of file)

| Setting | Default | Meaning |
|---|---|---|
| `DHTPIN` | `11` | Pin the DHT11 data wire is connected to |
| `DHTTYPE` | `DHT11` | Sensor model (change to `DHT22` if you upgrade) |
| `READ_MS` | `2000` | Read the sensor every 2 s |
| `SAMPLE_MS` | `60000` | Save a graph point every 60 s |
| `HIST` | `180` | Number of saved points (180 × 1 min = 3 hours) |

### Functions

| Function | What it does |
|---|---|
| `calcDewPointF(tC, rh)` | Calculates dew point from °C and % humidity using the **Magnus formula**, and returns °F. |
| `readSensor()` | Reads the DHT11 once, converts °C → °F, and calculates dew point. If a read fails (`NaN`), it keeps the last good values so the dashboard never shows `nan`. |
| `saveSample()` | Stores the current dew point in the history **circular buffer**. |
| `sendPage(client)` | Sends the dashboard web page from `page.h`. |
| `sendData(client)` | Sends the latest reading as JSON. |
| `sendHistory(client)` | Sends the saved dew points as a JSON array, oldest → newest. |

### Dew point (Magnus formula)

$$\gamma = \ln\left(\frac{RH}{100}\right) + \frac{a \cdot T}{b + T} \qquad T_{dew} = \frac{b \cdot \gamma}{a - \gamma}$$

where `T` is in °C, `RH` is in %, `a = 17.62`, and `b = 243.12`. The result is converted to °F.

### 3-hour history (circular buffer)

Readings are stored in a fixed array of 180 slots. `histNext` points to the slot written next and wraps from 179 back to 0 using `% HIST`, so once the array is full the newest reading overwrites the oldest. `histCount` tracks how many slots hold real data. When sending, the code computes the oldest slot with:

```cpp
int oldest = (histNext - histCount + HIST) % HIST;
```

and walks forward from there, so the graph is always in time order.

### Web server and routes

The board runs a `WiFiServer` on **port 80**. For every browser connection, it reads the first line of the HTTP request (e.g. `GET /data HTTP/1.1`) and replies based on the path:

| Route | Returns | Example |
|---|---|---|
| `/` | The dashboard page (HTML + JavaScript) | — |
| `/data` | Latest reading (JSON) | `{"f":72.5,"h":45,"dp":49.8}` |
| `/history` | Last 3 hours of dew points, oldest → newest (JSON) | `[55.1,55.3,56.0]` |

### `setup()`

1. Starts Serial at 9600 baud.
2. Connects to Wi-Fi (retrying every 5 s) and waits for an IP address.
3. Starts the DHT11, takes the first reading, and saves the first graph point.
4. Starts the web server.

### `loop()`: three independent jobs

```
loop():
  1. Is a browser connected?          → read the request, send /, /data or /history
  2. Has READ_MS passed?               → readSensor(), print to Serial
  3. Has SAMPLE_MS passed?             → saveSample()
```

Each job checks its own timer, so none of them waits on the others.

---

## 3. Wi-Fi Credentials – `arduino_secrets.h`

This file holds your **Wi-Fi network name and password**. It's kept separate from the main code so the code can be shared publicly without exposing your password.

> 🔒 **`arduino_secrets.h` is private and is NOT included in this repository.** It's listed in `.gitignore` so it can't be uploaded by accident.

### Setup

1. Copy **`arduino_secrets.example.h`** into the sketch folder.
2. Rename the copy to **`arduino_secrets.h`**.
3. Replace the placeholders:

```cpp
#define SECRET_SSID "YOUR_WIFI_NAME"
#define SECRET_PASS "YOUR_WIFI_PASSWORD"
```

The main sketch loads it with `#include "arduino_secrets.h"` and uses `SECRET_SSID` and `SECRET_PASS` when connecting.

### Network requirements

- **2.4 GHz only.** The UNO R4 WiFi can't join 5 GHz networks. If your router shows a separate `-5G` network, use the other one.
- **No login-page or enterprise networks.** Networks that need a username or a sign-in page (school, hotel, eduroam) won't work. A home network or a phone hotspot will.
- The viewing device must be on the **same network** as the Arduino.

---

## 4. Web Page – `page.h`

`page.h` contains the entire dashboard (HTML, CSS, and JavaScript) stored in one C++ constant, `PAGE`, using a **raw string literal**:

```cpp
const char PAGE[] = R"rawliteral(
  <!DOCTYPE html> ... the whole web page ...
)rawliteral";
```

Raw string literals let the page be written as normal HTML/JavaScript without escaping quotes. When a browser requests `/`, `sendPage()` sends this text.

> **Why a separate file?** The Arduino IDE automatically scans `.ino` files for functions and generates declarations for them. Its scanner doesn't understand raw strings, so it mistakes JavaScript like `async function loadHistory()` for C++ and the build fails. Keeping the page in a `.h` file hides it from the scanner. It also keeps the web code separate from the Arduino code.

### What the page does (all of this runs in the browser, not on the Arduino)

| Piece | Purpose |
|---|---|
| **Chart.js** (loaded from a CDN) | Draws the line graph. The viewing device needs internet access to download it. |
| `level(dp)` | Turns a dew point into a status message and a color (see the scale below). |
| `loadHistory()` | Fetches `/history` and redraws the graph. The Arduino has no clock, so the browser calculates each point's time from its own clock and `INTERVAL_SEC`. |
| `loadLatest()` | Fetches `/data`, updates the numbers, and recolors the banner. Wrapped in `try/catch`, so it shows "Lost connection – retrying…" if the board goes offline. |
| `setInterval(...)` | **Polling:** `loadLatest` runs every 5 s and `loadHistory` every `INTERVAL_SEC`, so the page updates without refreshing. |
| `segment.borderColor` | Colors each piece of the graph line by that moment's dew point. |

### Comfort scale

| Dew point (°F) | Status | Color |
|---|---|---|
| < 50 | Crisp & dry 😎 | Deep blue |
| 50 – 59 | Fresh 🙂 | Blue |
| 60 – 64 | Getting sticky | Light mustard |
| 65 – 69 | Musty 😓 | Mustard |
| ≥ 70 | It's NASTY in here 🤢 | Dark mustard |

---

## 5. Customizing

- **Sample rate / history length:** change `SAMPLE_MS` and `HIST` in the sketch.
  ⚠️ **`INTERVAL_SEC` in `page.h` must match `SAMPLE_MS`** (e.g. `60000` ms ↔ `60` s), or the graph's time labels will be wrong.
  Example: 5-minute samples for 3 hours → `SAMPLE_MS = 300000`, `HIST = 36`, `INTERVAL_SEC = 300`.
- **Fast testing:** set `SAMPLE_MS = 5000` and `INTERVAL_SEC = 5` to watch the graph fill in seconds.
- **Thresholds and colors:** edit the `level()` function in `page.h`. Colors are hex codes (`#RRGGBB`).

---

## 6. Troubleshooting

| Problem | Likely cause / fix |
|---|---|
| Stuck on `Connection Failed, retrying...` | Wrong name or password (case-sensitive), a 5 GHz network, or a login-page/enterprise network |
| IP shows `0.0.0.0` | Press the reset button on the board |
| Browser says "site can't be reached" | Phone on cellular data or a different Wi-Fi network; typed `https://` instead of `http://`; guest networks often block devices from talking to each other |
| Page loads but graph is blank | Viewing device has no internet (can't download Chart.js). On a laptop, check the browser console (right-click → Inspect → Console) |
| Values show `nan` or `DHT read failed` in Serial | Check wiring and the data pin; a bare 4-pin sensor needs the 10 kΩ pull-up |
| Garbled characters in Serial Monitor | Set the Serial Monitor to **9600 baud** |
| Graph times look wrong | `INTERVAL_SEC` in `page.h` doesn't match `SAMPLE_MS` |

---

## 7. Limitations

- **History resets on power loss or re-upload.** Readings are stored in RAM.
- **Local network only.** The dashboard can only be opened from devices on the same Wi-Fi network.
- **DHT11 accuracy** is about ±2 °C and ±5 % RH, so watch trends rather than exact values. A DHT22 is a drop-in upgrade (change `DHTTYPE`).

---

## Repository Structure

```
FreshCheck/
├── FreshCheck.ino             # Main sketch: sensor, history, web server
├── page.h                     # Dashboard web page (HTML/CSS/JavaScript)
├── arduino_secrets.example.h  # Template for Wi-Fi credentials
└── arduino_secrets.h          # YOUR credentials (create this, not committed)
.gitignore                     # Keeps arduino_secrets.h out of Git
README.md
```
