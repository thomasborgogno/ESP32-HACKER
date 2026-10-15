# HACKER — a Wi-Fi mentalism prop on an ESP32

[![Donate with PayPal](https://img.shields.io/badge/Donate-PayPal-00457C?logo=paypal&logoColor=white)](https://paypal.me/thomasborgogno)

Turn an ESP32 into a magic prop: type a "prediction" on your phone and a few seconds later it shows up
**dozens of times in the Wi-Fi list of any phone nearby** — a playing card, a word, a name, a short sentence.

The ESP32 broadcasts fake Wi-Fi *beacon frames*. No network really exists and nobody can connect to the fake
ones; they are only announcements, so they vanish as soon as you switch the device off.

## The effect

1. A spectator picks a card (or thinks of a word, a name, a number...).
2. You secretly type the prediction on your phone, in the ESP32's web page, and press **Start**.
3. You ask the spectator to open the Wi-Fi settings on their own phone.
4. The list is flooded with networks named after their choice, e.g. 64 × `Ace of Hearts`, with padlocks.

## Features

- **Fake networks with any name**, up to 32 bytes (SSID limit).
- **Many networks at once**, up to **100**, all looking like they have the same name.
  Android merges networks with identical names into a single row, so each fake network gets an invisible
  suffix (zero-width characters) that makes it a distinct SSID while looking identical.
- **Live limit in the UI**: the invisible suffix costs space inside the 32-byte SSID, so the longer your text,
  the fewer networks are possible. The page updates the maximum as you type (default 100):

  | Text length | Max networks |
  |---|---|
  | up to 11 chars | 100 |
  | 12–14 | 64 |
  | 15–17 | 32 |
  | 18–20 | 16 |
  | 21–23 | 8 |
  | 24–26 | 4 |
  | 27–29 | 2 |
  | 30–32 | 1 |

  Accented letters and emoji take more than one byte, so the limit is reached sooner.
- **Protected look** (on by default): the fake networks are announced as WPA2, so they show a padlock.
  Untick the box for open networks.
- **Phone-friendly control page** served by the ESP32 itself, no app needed.
- **Hidden control network**: the Wi-Fi network you use to control the ESP32 does not appear in Wi-Fi lists.
- **Changeable control password**, stored in the ESP32's flash (survives power-offs), from the page's
  **Settings** button (top right).

## Hardware

- An **ESP32** board. I developed and tested it on an **ESP32-S3** (DevKitC-1 clone with **4 MB flash**), but
  it should be adaptable to other ESP32 boards (see [Using another ESP32 board](#using-another-esp32-board)).
- A USB cable for flashing, and a power bank or any USB power source for the show.

## Build and flash

The project uses [PlatformIO](https://platformio.org/) (CLI or the VS Code extension).

```bash
pio run -t upload
```

If the board does not enter flashing mode, hold **BOOT**, press **RESET**, then release **BOOT**.

Notes:

- `platformio.ini` assumes **4 MB of flash**. If your board has 8 MB or more, edit `board_upload.flash_size`
  (and the partition table if you want to use the extra space). A wrong value makes the firmware crash at boot
  with `Detected size(4096k) smaller than the size in the binary image header`.
- The upload port is auto-detected. If you have several boards connected, add `upload_port = COMx`
  (or `/dev/ttyACMx`) to `platformio.ini`.

### Using another ESP32 board

Only `platformio.ini` is board-specific. To adapt the project:

1. Change `board` to the one that matches your hardware (see the
   [PlatformIO board list](https://docs.platformio.org/en/latest/boards/index.html#espressif-32)),
   e.g. `esp32dev` for a classic ESP32, `esp32-c3-devkitm-1` for an ESP32-C3.
2. Set `board_upload.flash_size` to your board's flash size.
3. Build and flash as above.

The code uses only the Arduino Wi-Fi API and `esp_wifi_80211_tx()`, which are available on the ESP32 family
that has Wi-Fi. I have **not tested** other chips, so behaviour may differ. The fake networks use the 2.4 GHz
band only.

## First-time setup: the hidden control network

After flashing, the ESP32 creates a **hidden** Wi-Fi network:

| Setting | Value |
|---|---|
| Network name (SSID) | `ESP32` |
| Security | WPA2-Personal (WPA2-PSK) |
| Default password | `00000000` |
| Control page | `http://192.168.1.1` |

Because the network is hidden, it does **not appear in the Wi-Fi list**. You have to add it manually, once.

**Android** (menus vary slightly between manufacturers):

1. Settings → Network & internet → Wi-Fi → scroll down and tap **Add network**.
2. Network name: `ESP32`.
3. Security: **WPA/WPA2-Personal**.
4. Password: `00000000`.
5. Open **Advanced options** and set **Hidden network** to **Yes**.
6. Save. Android may warn that the network has no internet: stay connected.
7. Open `http://192.168.1.1` in a browser (use `http://`, not `https://`).

**iPhone:** Settings → Wi-Fi → **Other...**, enter the name `ESP32`, Security **WPA2 Personal**, and the password.
Then open `http://192.168.1.1` in Safari.

Once saved, the phone reconnects automatically whenever the ESP32 is powered nearby.

If the connection fails, check these first: exact name (case-sensitive), security set to WPA2 and not "none",
the *Hidden network* option enabled, and mobile data turned off while you use the page.

> **Tip:** if you cannot connect to the hidden network, set `CTRL_HIDDEN` to `false` in `src/main.cpp`,
> reflash, and the `ESP32` network will show up in the list so you can check that everything else works.
> Set it back to `true` for the real show.

## Using it

1. Power the ESP32 and connect your phone to the `ESP32` network.
2. Open `http://192.168.1.1`.
3. Type the prediction. The *Number of networks* field shows the maximum allowed for that text.
4. Choose the number of networks and whether they look protected.
5. Press **Start**. Press **Stop** to make them all disappear.

Settings (top right) lets you change the control password. Pick one with 8 to 63 characters. After saving, the
network restarts, your phone disconnects, and you must update the saved password on the phone.

## HTTP API

The control page is a thin client over a small HTTP API, so you can drive the device from your own app or script
(for example an Android app) without changing the firmware. The base URL is `http://192.168.1.1`
(plain HTTP, no authentication: access is controlled by the Wi-Fi password of the control network).

| Request | Description |
|---|---|
| `GET /status` | Current state as JSON |
| `POST /set` | Start or stop the fake networks, and set their name, number and look |
| `POST /pass` | Change the control network password |
| `GET /` | The web page. Any unknown path returns the same page |

Bodies are `application/x-www-form-urlencoded`, encoded in UTF-8.

### `GET /status`

```json
{"on":true,"ssid":"Ace of Hearts","count":64,"secured":true}
```

`ssid` is the base text, without the invisible suffix. After a reboot the device reports `on: false`,
an empty `ssid` and `count: 50`.

### `POST /set`

All fields are optional: a field you leave out keeps its previous value.

| Field | Meaning |
|---|---|
| `on` | `1` to broadcast, `0` to stop |
| `t` | Prediction text, up to 32 bytes (longer text is cut). An empty text stops the broadcast |
| `n` | Number of networks, 1 to 100. Reduced automatically to the maximum allowed by the text length (see the table above) |
| `s` | `1` for protected-looking networks, `0` for open ones. Any value other than `0` means protected |

```bash
curl -X POST http://192.168.1.1/set -d "on=1" --data-urlencode "t=Ace of Hearts" -d "n=100" -d "s=1"
curl -X POST http://192.168.1.1/set -d "on=0"   # stop, keeps text and settings
curl -X POST http://192.168.1.1/set -d "on=1"   # restart with the previous text and settings
```

The response is the same JSON as `GET /status`, with the values actually applied (for example, `count` shows the
number of networks after the limit for the text length).

### `POST /pass`

| Field | Meaning |
|---|---|
| `p` | New password, 8 to 63 characters |

```bash
curl -X POST http://192.168.1.1/pass -d "p=my-new-password"
```

Returns `200 {"ok":true}` and saves the password to flash, or `400 {"ok":false,"error":"password_length"}` if the
length is invalid. About one second later the control network restarts with the new password, so the client is
disconnected and has to reconnect.

### Notes for Android apps

- Android blocks plain HTTP by default: allow cleartext traffic to `192.168.1.1`
  (`android:usesCleartextTraffic="true"` or a Network Security Config).
- The control network has no internet, so Android may send requests over mobile data. Bind the app's traffic to the
  Wi-Fi network (`ConnectivityManager.bindProcessToNetwork()`), or requests will not reach the ESP32.
- `WifiNetworkSpecifier` can connect to the hidden network programmatically (`setIsHiddenSsid(true)`, WPA2 passphrase),
  so users do not have to add it by hand.

## Change the password and IMPORTANT security notes

- The default password `00000000` is public (it is in this README). **Change it** from **Settings** before using the
  device in public: anyone who joins the control network can change the prediction and the password.
- The password is stored in the ESP32's flash. To go back to the default, erase the flash and reflash:
  `pio run -t erase`, then `pio run -t upload`.
- The control page runs over plain HTTP on a private network of its own. It is not hardened for hostile environments.

## Configuration

Edit the constants at the top of `src/main.cpp`:

| Constant | Default | Meaning |
|---|---|---|
| `CTRL_SSID` | `"ESP32"` | Name of the control network |
| `DEFAULT_PASS` | `"00000000"` | Initial control password |
| `CTRL_HIDDEN` | `true` | Hide the control network |
| `CHANNEL` | `1` | 2.4 GHz channel used for the control network and the fake networks |
| `MAX_COUNT` | `100` | Maximum number of fake networks |
| IP address in `startAP()` | `192.168.1.1` | Address of the control page |

## How it works

- The ESP32 runs in access-point mode and serves the control page with the Arduino `WebServer`.
- Fake networks are raw IEEE 802.11 **beacon frames** sent every ~100 ms with `esp_wifi_80211_tx()`.
  Each network gets its own BSSID (MAC address), derived from its index.
- To get one row per network on Android, the SSID of network *i* is the prediction followed by the binary
  representation of *i* written with two invisible characters (U+200B and U+200C, 3 bytes each).
- With "protected" on, the beacon sets the privacy bit and includes an RSN element (WPA2-PSK, CCMP).
- Only the 2.4 GHz band is used.

## Limits

- Phones refresh their Wi-Fi list slowly. Start the networks a few seconds before the reveal, and rehearse.
- Different phones and Android versions group and sort networks differently. iPhones may show fewer rows.
- The fake networks disappear when the ESP32 loses power.

## Support the project

If this project made your show better, you can [buy me a coffee with PayPal](https://paypal.me/thomasborgogno).
Thank you!

## Legal and ethical note

Broadcasting fake Wi-Fi beacons is harmless at small scale, but it can annoy people looking for their own network
and the rules on radio transmissions differ from country to country. Use it only for performances to an audience
that expects it, at low volume, not in crowded public places, and not near critical equipment. You are responsible
for how you use it. Do not use it to impersonate real networks or to mislead people.
