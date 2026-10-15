// Magic Wi-Fi: trasmette tante reti Wi-Fi fittizie (beacon) con lo stesso nome, scritto da una pagina web.
// Collegati dal telefono alla rete di controllo, apri http://192.168.4.1 e scrivi le predizioni.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_wifi.h>

// Rete di controllo (la password deve avere almeno 8 caratteri)
static const char *CTRL_SSID = "ESP-Control";
static const char *CTRL_PASS = "magia1234";
static const bool CTRL_HIDDEN = false;  // true = rete di controllo nascosta (devi aggiungerla a mano sul telefono)
static const uint8_t CHANNEL = 1;

static const int MAX_COUNT = 100;
static const int MAX_LEN = 32;

WebServer server(80);

static String ssid;                 // nome base scelto dall'utente
static String variants[MAX_COUNT];  // nome + suffisso invisibile, uno per rete
static int netCount = 50;
static bool running = false;

static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="it"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Predizione</title>
<style>
body{font-family:system-ui,sans-serif;margin:0;padding:16px;background:#111;color:#eee}
h1{font-size:1.2rem}
input[type=text]{width:100%;box-sizing:border-box;font-size:1.1rem;padding:10px;border-radius:8px;border:1px solid #444;background:#222;color:#fff}
input[type=number]{width:5em;font-size:1.1rem;padding:6px;border-radius:8px;border:1px solid #444;background:#222;color:#fff}
button{width:100%;padding:14px;margin-top:10px;font-size:1.1rem;border:0;border-radius:8px;color:#fff}
#go{background:#2e7d32}#stop{background:#b71c1c}
p{color:#aaa;font-size:.9rem}
#st{font-weight:bold}
</style></head><body>
<h1>Predizione Wi-Fi</h1>
<p>Scrivi la predizione: verrà trasmessa come tante reti con lo stesso nome. Più il testo è lungo, meno reti si possono generare.</p>
<input id="t" type="text" placeholder="Asso di Cuori" autocomplete="off" oninput="upd()">
<p>Numero di reti: <input id="n" type="number" min="1" max="100" value="100" oninput="pref=Number(this.value)||1;upd()"> <span id="lim"></span></p>
<button id="go" onclick="send(1)">Invia / Avvia</button>
<button id="stop" onclick="send(0)">Spegni</button>
<p>Stato: <span id="st">...</span></p>
<script>
var pref=100,$=function(i){return document.getElementById(i)};
function bytes(s){return new TextEncoder().encode(s).length}
// ogni rete usa un suffisso invisibile di 3 byte per bit: il testo e il suffisso stanno in 32 byte
function maxNets(L){var b=Math.floor((32-L)/3);return b>=7?100:(1<<b)}
function upd(){
  var t=Array.from($('t').value);
  while(bytes(t.join(''))>32)t.pop();
  if(t.join('')!==$('t').value)$('t').value=t.join('');
  var L=bytes(t.join('')),m=maxNets(L),n=$('n');
  n.max=m;n.value=Math.max(1,Math.min(pref,m));
  $('lim').textContent='(max '+m+' con questo testo)';
}
function send(on){
  upd();
  var b=new URLSearchParams();b.set('on',on);b.set('t',$('t').value);b.set('n',$('n').value);
  fetch('/set',{method:'POST',body:b}).then(r=>r.text()).then(x=>$('st').textContent=x);
}
fetch('/status').then(r=>r.json()).then(j=>{$('t').value=j.ssid;pref=j.on?j.count:100;upd();$('st').textContent=j.on?('attivo ('+j.count+' reti)'):'spento'});
upd();
</script></body></html>)HTML";

static void handleRoot() { server.send_P(200, "text/html; charset=utf-8", PAGE); }

static String jsonEscape(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c >= 0x20) o += c;
  }
  return o;
}

static void handleStatus() {
  String j = "{\"on\":";
  j += running ? "true" : "false";
  j += ",\"ssid\":\"" + jsonEscape(ssid) + "\",\"count\":" + netCount + "}";
  server.send(200, "application/json", j);
}

static void handleSet() {
  running = server.arg("on") == "1";
  String line = server.arg("t");
  line.replace("\r", "");
  line.replace("\n", " ");
  // la pagina limita già testo e numero di reti; qui ripeto gli stessi controlli per sicurezza
  if ((int)line.length() > MAX_LEN) {
    int cut = MAX_LEN;
    while (cut > 0 && (((uint8_t)line[cut]) & 0xC0) == 0x80) cut--;
    line = line.substring(0, cut);
  }
  ssid = line;

  // Android raggruppa le reti con lo stesso nome: aggiungo un suffisso invisibile diverso per ogni rete
  // (indice in binario con U+200B / U+200C, 3 byte ciascuno), così sembrano uguali ma sono nomi distinti.
  // I byte rimasti dopo il testo fissano quanti bit, e quindi quante reti, si possono usare.
  int bitsAllowed = (MAX_LEN - (int)ssid.length()) / 3;
  int maxNets = bitsAllowed >= 7 ? MAX_COUNT : (1 << bitsAllowed);
  int n = server.arg("n").toInt();
  netCount = n < 1 ? 1 : (n > maxNets ? maxNets : n);
  int bits = 0;
  while ((1 << bits) < netCount) bits++;
  for (int i = 0; i < netCount; i++) {
    String v = ssid;
    for (int b = bits - 1; b >= 0; b--) v += ((i >> b) & 1) ? "\xE2\x80\x8C" : "\xE2\x80\x8B";
    variants[i] = v;
  }
  if (ssid.length() == 0) running = false;
  server.send(200, "text/plain; charset=utf-8", running ? String("attivo (") + netCount + " reti)" : "spento");
}

// Invia un beacon con SSID e BSSID derivato dall'indice, così ogni nome è una rete distinta.
static void sendBeacon(const String &ssid, int idx) {
  uint8_t pkt[128];
  int n = 0;
  pkt[n++] = 0x80; pkt[n++] = 0x00;                         // frame control: beacon
  pkt[n++] = 0x00; pkt[n++] = 0x00;                         // duration
  for (int i = 0; i < 6; i++) pkt[n++] = 0xFF;              // destinazione: broadcast
  uint8_t mac[6] = {0x02, 0x13, 0x37, 0xA0, 0x00, (uint8_t)idx};
  for (int i = 0; i < 6; i++) pkt[n++] = mac[i];            // sorgente
  for (int i = 0; i < 6; i++) pkt[n++] = mac[i];            // BSSID
  pkt[n++] = 0x00; pkt[n++] = 0x00;                         // sequence
  for (int i = 0; i < 8; i++) pkt[n++] = 0x00;              // timestamp
  pkt[n++] = 0x64; pkt[n++] = 0x00;                         // intervallo beacon 100 TU
  pkt[n++] = 0x01; pkt[n++] = 0x04;                         // capability: ESS, rete aperta
  pkt[n++] = 0x00; pkt[n++] = (uint8_t)ssid.length();       // tag SSID
  for (size_t i = 0; i < ssid.length(); i++) pkt[n++] = (uint8_t)ssid[i];
  static const uint8_t rates[] = {0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C};
  for (uint8_t b : rates) pkt[n++] = b;                     // tag supported rates
  pkt[n++] = 0x03; pkt[n++] = 0x01; pkt[n++] = CHANNEL;     // tag canale
  esp_wifi_80211_tx(WIFI_IF_AP, pkt, n, false);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(CTRL_SSID, CTRL_PASS, CHANNEL, CTRL_HIDDEN ? 1 : 0, 4);
  Serial.printf("\nControllo: rete '%s', pagina http://%s\n", CTRL_SSID, WiFi.softAPIP().toString().c_str());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/set", HTTP_POST, handleSet);
  server.onNotFound(handleRoot);  // pagina di accesso automatico del telefono
  server.begin();
}

void loop() {
  server.handleClient();
  static uint32_t last = 0;
  if (running && millis() - last >= 100) {
    last = millis();
    for (int i = 0; i < netCount; i++) {
      sendBeacon(variants[i], i + 1);
      delay(2);
      server.handleClient();
    }
  }
}
