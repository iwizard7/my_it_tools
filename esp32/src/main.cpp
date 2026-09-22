#include <Arduino.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <mbedtls/sha256.h>
#include <qrcode.h>

namespace {
constexpr char kApName[] = "ESP32-Random-Tools";
constexpr char kApPassword[] = "randomtools";
constexpr char kHostname[] = "esp32-it-tools";
constexpr char kVersion[] = "1.8.3";
constexpr size_t kMaxLength = 256;
constexpr size_t kMaxResults = 20;

WebServer server(80);
Preferences preferences;
String configuredSsid;
String configuredPassword;
bool updateInProgress = false;
size_t updateSize = 0;
esp_qrcode_handle_t generatedQr = nullptr;
unsigned long wifiConnectedAt = 0;
unsigned long generationCount = 0;
unsigned long generationWindowAt = 0;
unsigned long generationWindowCount = 0;
float generationRate = 0;

const char kPage[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 IT Tools</title>
  <style>
    :root { color-scheme: dark; --bg:#111827; --panel:#1f2937; --muted:#9ca3af; --text:#f9fafb; --accent:#8b5cf6; --border:#374151; }
    * { box-sizing:border-box; }
    body { margin:0; min-height:100vh; background:linear-gradient(135deg,#111827,#241548); color:var(--text); font:15px system-ui,-apple-system,sans-serif; }
    main { width:min(920px,calc(100% - 32px)); margin:0 auto; padding:42px 0 60px; }
    h1 { margin:0 0 8px; font-size:clamp(27px,5vw,42px); letter-spacing:-1px; }
    .subtitle { margin:0 0 28px; color:var(--muted); }
    .card { background:rgba(31,41,55,.94); border:1px solid var(--border); border-radius:16px; padding:22px; box-shadow:0 18px 45px #0004; }
    .layout { display:grid; grid-template-columns:210px 1fr; gap:18px; align-items:start; }
    .nav { display:grid; gap:7px; position:sticky; top:16px; }
    .navgroup { display:grid; gap:7px; margin-bottom:10px; }
    .navgroup h3 { color:var(--muted); font-size:11px; letter-spacing:1px; text-transform:uppercase; margin:8px 8px 2px; }
    .nav button { text-align:left; background:#1f2937; border:1px solid var(--border); color:var(--muted); }
    .nav button.active { color:white; background:var(--accent); border-color:var(--accent); }
    .tool { display:none; } .tool.active { display:block; }
    .controls { display:grid; grid-template-columns:repeat(auto-fit,minmax(180px,1fr)); gap:16px; }
    label { display:block; color:var(--muted); font-size:13px; margin-bottom:7px; }
    input, textarea, select { width:100%; padding:11px 12px; border:1px solid var(--border); border-radius:9px; background:#111827; color:var(--text); font-size:15px; }
    textarea { min-height:150px; resize:vertical; font:14px ui-monospace,SFMono-Regular,Menlo,monospace; }
    .checks { display:flex; flex-wrap:wrap; gap:10px 18px; margin:20px 0; }
    .checks label { margin:0; color:var(--text); cursor:pointer; }
    input[type=checkbox] { accent-color:var(--accent); margin-right:7px; }
    button { border:0; border-radius:9px; padding:11px 18px; color:white; background:var(--accent); font-weight:700; cursor:pointer; }
    button:hover { filter:brightness(1.15); } button.secondary { background:#374151; margin-left:8px; }
    .status { color:var(--muted); margin:17px 0 10px; font-size:13px; }
    .result { display:flex; gap:10px; align-items:center; padding:12px; margin:8px 0; background:#111827; border:1px solid var(--border); border-radius:9px; }
    .result code { flex:1; overflow-wrap:anywhere; color:#d8b4fe; font:14px ui-monospace,SFMono-Regular,Menlo,monospace; }
    .output { margin-top:16px; white-space:pre-wrap; overflow-wrap:anywhere; color:#d8b4fe; }
    .hint { color:var(--muted); font-size:13px; margin:0 0 16px; }
    .copy { padding:7px 10px; font-size:12px; background:#374151; flex:none; }
    .help-toggle { position:fixed; right:18px; bottom:18px; z-index:5; border-radius:50%; width:44px; height:44px; padding:0; font-size:20px; box-shadow:0 8px 24px #0006; }
    .help-panel { display:none; position:fixed; right:18px; bottom:72px; z-index:4; width:min(360px,calc(100% - 36px)); max-height:70vh; overflow:auto; background:#111827; border:1px solid var(--border); border-radius:14px; padding:18px; box-shadow:0 18px 45px #0008; }
    .help-panel.open { display:block; } .help-panel h3 { margin-top:0; } .help-panel code { color:#d8b4fe; }
    .script-output { min-height:150px; max-height:420px; overflow:auto; white-space:pre-wrap; overflow-wrap:anywhere; padding:14px; border:1px solid var(--border); border-radius:9px; background:#111827; font:14px ui-monospace,SFMono-Regular,Menlo,monospace; }
    .cyrillic { color:#fb7185; background:#7f1d1d55; border-radius:3px; }
    .latin { color:#60a5fa; background:#1e3a8a55; border-radius:3px; }
    .legend { display:flex; gap:16px; color:var(--muted); font-size:13px; margin:12px 0; }
    .legend span { padding:3px 7px; border-radius:4px; } .legend .cyrillic { color:#fb7185; } .legend .latin { color:#60a5fa; }
    footer { color:var(--muted); text-align:center; margin-top:22px; font-size:12px; }
  </style>
</head>
<body>
<main>
  <h1>ESP32 IT Tools</h1>
  <p class="subtitle">Useful developer tools running locally on your ESP32-C3</p>
  <div class="layout">
    <nav class="nav">
      <div class="navgroup"><h3>Generate</h3><button class="active" data-tool="random">🎲 Random data</button><button data-tool="uuid">🆔 UUID generator</button><button data-tool="token">🔑 Token & password</button><button data-tool="qr">▦ QR generator</button></div>
      <div class="navgroup"><h3>Transform</h3><button data-tool="base64">🔤 Base64</button><button data-tool="url">🔗 URL encoder</button><button data-tool="json">{ } JSON tools</button><button data-tool="stats">📊 Text statistics</button><button data-tool="script">🔤 Script analyzer</button><button data-tool="toolbox">🧰 DevOps Toolkit</button></div>
      <div class="navgroup"><h3>Network</h3><button data-tool="network">🌐 Network diagnostics</button><button data-tool="dns">🧭 DNS Inspector</button></div>
      <div class="navgroup"><h3>DevOps</h3><button data-tool="api">🧪 API console</button><button data-tool="logs">📋 Log analyzer</button><button data-tool="incident">🚨 Incident probe</button><button data-tool="system">📈 Metrics & device</button></div>
    </nav>
    <section class="card">
      <div id="random" class="tool active">
        <h2>Random data generator</h2><p class="hint">Hardware randomness from the ESP32-C3, delivered through the local API.</p>
        <div class="controls"><div><label for="length">String length</label><input id="length" type="number" min="1" max="256" value="32"></div><div><label for="count">Number of results</label><input id="count" type="number" min="1" max="20" value="5"></div></div>
        <div class="checks"><label><input id="upper" type="checkbox" checked>Uppercase</label><label><input id="lower" type="checkbox" checked>Lowercase</label><label><input id="digits" type="checkbox" checked>Numbers</label><label><input id="symbols" type="checkbox" checked>Symbols</label></div>
        <button id="generate">Generate</button><button id="clear" class="secondary">Clear</button><div id="status" class="status">Ready</div><div id="results"></div>
      </div>
      <div id="uuid" class="tool"><h2>UUID v4 generator</h2><p class="hint">Generate universally unique identifiers in the browser.</p><button id="uuidGenerate">Generate UUIDs</button><div id="uuidOutput" class="output"></div></div>
      <div id="token" class="tool"><h2>Token & password generator</h2><p class="hint">Create random passwords or API-style tokens.</p><div class="controls"><div><label for="tokenLength">Length</label><input id="tokenLength" type="number" min="4" max="256" value="32"></div><div><label for="tokenMode">Format</label><select id="tokenMode"><option value="hex">Hexadecimal</option><option value="base64">Base64-like</option><option value="password">Password</option></select></div></div><br><button id="tokenGenerate">Generate token</button><div id="tokenOutput" class="output"></div></div>
      <div id="base64" class="tool"><h2>Base64 converter</h2><p class="hint">Encode or decode UTF-8 text locally in your browser.</p><textarea id="baseInput" placeholder="Enter text..."></textarea><br><br><button id="baseEncode">Encode</button><button id="baseDecode" class="secondary">Decode</button><textarea id="baseOutput" placeholder="Result..."></textarea></div>
      <div id="url" class="tool"><h2>URL encoder / decoder</h2><p class="hint">Safely encode query values or decode URL text.</p><textarea id="urlInput" placeholder="https://example.com/?q=hello world"></textarea><br><br><button id="urlEncode">Encode</button><button id="urlDecode" class="secondary">Decode</button><textarea id="urlOutput" placeholder="Result..."></textarea></div>
      <div id="json" class="tool"><h2>JSON formatter</h2><p class="hint">Format or minify JSON without sending data anywhere.</p><textarea id="jsonInput" placeholder='{"name":"ESP32","online":true}'></textarea><br><br><button id="jsonFormat">Format</button><button id="jsonMinify" class="secondary">Minify</button><textarea id="jsonOutput" placeholder="Result..."></textarea></div>
      <div id="stats" class="tool"><h2>Text statistics</h2><p class="hint">Count characters, words, lines and bytes.</p><textarea id="statsInput" placeholder="Paste text here..."></textarea><br><br><button id="statsRun">Analyze</button><div id="statsOutput" class="output"></div></div>
      <div id="script" class="tool"><h2>Cyrillic & Latin analyzer</h2><p class="hint">Paste text to highlight Cyrillic and Latin characters. The text is processed locally in your browser.</p><textarea id="scriptInput" placeholder="Вставьте большой текст здесь..."></textarea><br><br><button id="scriptAnalyze">Highlight characters</button><button id="scriptClear" class="secondary">Clear</button><div class="legend"><span class="cyrillic">Cyrillic</span><span class="latin">Latin</span><span>Other characters</span></div><div id="scriptStats" class="status"></div><div id="scriptOutput" class="script-output"></div></div>
      <div id="toolbox" class="tool"><h2>DevOps Toolkit</h2><p class="hint">Local helpers for API, Linux, Git, Docker, Kubernetes, CI/CD, secrets and SRE work.</p><select id="toolSelect"><option value="sha256">SHA-256 hash</option><option value="hmac">HMAC note</option><option value="strength">Password strength</option><option value="jwtverify">JWT signature verification</option><option value="secretScan">Secret scanner</option><option value="curl">curl command builder</option><option value="headers">HTTP headers checker</option><option value="jsonvalidate">JSON validator</option><option value="sla">SLA / error budget</option><option value="semver">SemVer comparator</option><option value="commit">Conventional Commit validator</option><option value="changelog">Changelog entry</option><option value="branch">Git branch name</option><option value="docker">Docker image parser</option><option value="compose">Docker Compose service</option><option value="dockerport">Docker port mapping</option><option value="healthcheck">Docker healthcheck</option><option value="k8s">Kubernetes quantity converter</option><option value="k8sdeploy">Kubernetes Deployment</option><option value="k8sservice">Kubernetes Service</option><option value="probes">Kubernetes probes</option><option value="resources">Kubernetes resources</option><option value="kubectl">kubectl command builder</option><option value="cron">Cron template</option><option value="systemd">systemd unit</option><option value="chmod">chmod calculator</option><option value="env">.env validator</option><option value="shellquote">Shell quote</option><option value="sshconfig">SSH config</option><option value="promql">PromQL helper</option><option value="slo">SLO calculator</option><option value="apdex">Apdex calculator</option><option value="alert">Prometheus alert rule</option><option value="logjson">JSON log formatter</option><option value="mask">Log secret masking</option><option value="gha">GitHub Actions workflow</option><option value="artifact">Artifact manifest</option><option value="checklist">Deployment checklist</option><option value="postmortem">Incident postmortem</option><option value="html">HTML entities</option><option value="unicode">Text ↔ Unicode</option><option value="binary">Text ↔ binary</option><option value="hex">Text ↔ hexadecimal</option><option value="case">Case converter</option><option value="slug">Slugify string</option><option value="jwt">JWT decoder</option><option value="urlparse">URL parser</option><option value="jsoncsv">JSON array ↔ CSV</option><option value="ipv4">IPv4 subnet calculator</option><option value="mac">MAC address generator</option><option value="port">Random port generator</option><option value="ipv6">IPv6 ULA generator</option><option value="ulid">ULID generator</option><option value="nanoid">NanoID generator</option><option value="lorem">Lorem Ipsum</option><option value="fake">Fake test data</option><option value="svg">SVG placeholder</option><option value="diff">Simple text diff</option><option value="wifiscan">Wi‑Fi scanner</option><option value="net">Network diagnostic</option><option value="filehash">File SHA-256</option></select><br><br><textarea id="toolInput" placeholder="Input..."></textarea><br><input id="toolFile" type="file" style="margin-top:10px"><br><br><button id="toolRun">Run tool</button><button id="toolClear" class="secondary">Clear</button><div id="toolOutput" class="output"></div></div>
      <div id="qr" class="tool"><h2>QR code generator</h2><p class="hint">Generate a QR code on the ESP32. Maximum payload: 600 characters. Works offline.</p><textarea id="qrInput" placeholder="Text, URL or Wi‑Fi payload..."></textarea><br><br><button id="qrGenerate">Generate QR</button><button id="qrDownload" class="secondary">Download SVG</button><div id="qrStatus" class="status"></div><div id="qrOutput" style="background:white;border-radius:12px;padding:18px;margin-top:16px;text-align:center;min-height:120px"></div></div>
      <div id="network" class="tool"><h2>Network diagnostics</h2><p class="hint">DNS, IP, TCP, HTTP, DHCP, route, mDNS and Wi‑Fi checks from the ESP32 network.</p><div class="controls"><div><label for="networkAction">Diagnostic</label><select id="networkAction"><option value="dns">DNS lookup</option><option value="dnsdetail">DNS details</option><option value="ping">Reachability / latency</option><option value="http">HTTP status</option><option value="tcp">TCP port check</option><option value="batch">Batch port check</option><option value="dhcp">DHCP information</option><option value="route">Route information</option><option value="mdns">mDNS service browser</option><option value="wifiscan">Wi‑Fi scan</option><option value="ipv4">IPv4 subnet calculator</option></select></div><div><label for="networkHost">Hostname, IP or CIDR</label><input id="networkHost" value="example.com" placeholder="example.com or 192.168.1.10/24"></div><div><label for="networkPort">TCP port(s)</label><input id="networkPort" value="80" placeholder="80 or 22,80,443"></div></div><br><button id="runNetwork">Run diagnostic</button><div id="networkStatus" class="status">Ready</div><pre id="networkOutput" class="output"></pre></div>
      <div id="dns" class="tool"><h2>DNS Inspector</h2><p class="hint">Advanced record lookup, reverse DNS, resolver comparison and DNS health run through Debian Gateway.</p><div class="controls"><div><label for="dnsGateway">Gateway URL</label><input id="dnsGateway" value="http://192.168.1.100:8080"></div><div><label for="dnsAction">Operation</label><select id="dnsAction"><option value="lookup">Record lookup</option><option value="reverse">Reverse DNS</option><option value="compare">Resolver comparison</option><option value="health">DNS health</option></select></div><div><label for="dnsName">Domain or IP</label><input id="dnsName" value="example.com"></div></div><br><button id="runDns">Run DNS check</button><div id="dnsStatus" class="status">Gateway required for advanced DNS checks</div><pre id="dnsOutput" class="output"></pre></div>
      <div id="api" class="tool"><h2>Mini API console</h2><p class="hint">HTTP client through the ESP32 network. HTTPS certificate verification is intentionally not included.</p><div class="controls"><div><label for="apiMethod">Method</label><select id="apiMethod"><option>GET</option><option>POST</option><option>PUT</option><option>DELETE</option></select></div><div><label for="apiHost">Host</label><input id="apiHost" value="example.com"></div><div><label for="apiPort">Port</label><input id="apiPort" type="number" value="80"></div></div><br><label for="apiPath">Path</label><input id="apiPath" value="/"><br><br><label for="apiBody">Request body</label><textarea id="apiBody" placeholder='{"hello":"world"}'></textarea><br><br><button id="sendApi">Send request</button><div id="apiStatus" class="status"></div><pre id="apiOutput" class="output"></pre></div>
      <div id="logs" class="tool"><h2>Log analyzer</h2><p class="hint">Paste logs or load a local file. Files are read only in the browser and are not uploaded.</p><input id="logFile" type="file" accept=".log,.txt,.json,.ndjson,.out,text/plain,application/json"><div id="logFileStatus" class="status">No file selected · maximum 2 MB</div><textarea id="logInput" placeholder="2026-09-22T10:00:00Z ERROR api timeout\n2026-09-22T10:00:01Z INFO request ok"></textarea><br><br><button id="analyzeLogs">Analyze logs</button><button id="maskLogs" class="secondary">Mask secrets</button><pre id="logOutput" class="output"></pre></div>
      <div id="incident" class="tool"><h2>DevOps Incident & Network Probe</h2><p class="hint">Run a compact incident report against a service from the ESP32 network.</p><div class="controls"><div><label for="incidentHost">Target hostname or IP</label><input id="incidentHost" value="example.com" placeholder="api.example.com"></div><div><label for="incidentPort">TCP port</label><input id="incidentPort" type="number" min="1" max="65535" value="80"></div><div><label for="incidentPath">HTTP path</label><input id="incidentPath" value="/" placeholder="/health"></div></div><br><button id="runIncident">Run incident checks</button><button id="copyIncident" class="secondary">Copy JSON</button><button id="downloadIncident" class="secondary">Download report</button><div id="incidentStatus" class="status">Ready</div><pre id="incidentOutput" class="output"></pre></div>
      <div id="system" class="tool"><h2>Device & OTA</h2><p class="hint">Live ESP32 status, Wi‑Fi configuration, metrics and wireless firmware updates.</p><button id="refreshSystem">Refresh status</button><div id="systemOutput" class="output">Loading...</div><canvas id="metricsChart" width="700" height="220" style="width:100%;margin-top:16px;background:#111827;border-radius:10px"></canvas><hr><h3>Connect to home Wi‑Fi</h3><p class="hint">The ESP32 access point remains available while connecting.</p><input id="wifiSsid" placeholder="Wi‑Fi network name"><br><br><input id="wifiPassword" type="password" placeholder="Wi‑Fi password"><br><br><button id="saveWifi">Save and restart</button><hr><h3>OTA firmware update</h3><input id="firmware" type="file" accept=".bin"><br><br><button id="uploadFirmware">Upload firmware</button><div id="otaStatus" class="status"></div></div>
    </section>
  </div>
  <footer>ESP32-C3 · local-only tools · Wi-Fi AP: ESP32-Random-Tools · v1.8.3</footer>
</main>
<button id="helpToggle" class="help-toggle" title="Contextual help">?</button>
<aside id="helpPanel" class="help-panel"><h3 id="helpTitle">Help</h3><div id="helpBody"></div></aside>
<script>
const $ = id => document.getElementById(id);
const helpText={random:['Random data','Генерирует строки аппаратным RNG ESP32. Длина 1–256, до 20 результатов.'],uuid:['UUID generator','Создаёт UUID v4. Используйте для request ID, correlation ID и тестовых сущностей.'],token:['Token & password','Генерирует тестовые токены и пароли. Не используйте их как production secrets без дополнительного хранения.'],base64:['Base64','Кодирует и декодирует UTF-8. Base64 не является шифрованием.'],url:['URL encoder','Кодирует query-параметры и декодирует URL-строки.'],json:['JSON tools','Format проверяет JSON и делает его читаемым, Minify убирает лишние пробелы.'],stats:['Text statistics','Считает строки, слова, символы и UTF-8 bytes.'],script:['Cyrillic & Latin analyzer','Подсвечивает кириллические символы красным, латинские синим. Текст остаётся в браузере и не отправляется на устройство.'],toolbox:['DevOps Toolkit','Локальные helpers для curl, Git, Docker, Kubernetes, Linux, CI/CD, SRE и security.'],qr:['QR generator','Создаёт QR на ESP32. Подходит для URL, Wi-Fi payload и коротких текстов.'],network:['Network diagnostics','Быстрые DNS, route/DHCP, TCP, HTTP, mDNS, Wi-Fi и IPv4 проверки. Для сложного DNS используйте DNS Inspector на Debian Gateway.'],dns:['DNS Inspector','Расширенные DNS lookup, reverse DNS, resolver comparison и health checks через Debian Gateway.'],api:['API console','Мини-Postman для HTTP через ESP32. HTTPS/TLS-проверка выполняется в Debian Gateway.'],logs:['Log analyzer','Загрузите локальный лог до 2 MB или вставьте текст. Файл не покидает браузер.'],incident:['Incident probe','Последовательно проверяет gateway, DNS, TCP и HTTP и формирует JSON incident report.'],system:['Metrics & device','Показывает RAM, uptime, температуру, LittleFS, Wi-Fi и OTA.']};
function showHelp(name){const x=helpText[name]||['Help','Для этого раздела справка пока не добавлена.'];$('helpTitle').textContent=x[0];$('helpBody').innerHTML=`<p>${x[1]}</p><p><strong>Privacy:</strong> локальные инструменты не отправляют входные данные наружу.</p>`;}
document.querySelectorAll('[data-tool]').forEach(b => b.onclick = () => { document.querySelectorAll('.nav button').forEach(x => x.classList.remove('active')); document.querySelectorAll('.tool').forEach(x => x.classList.remove('active')); b.classList.add('active'); $(b.dataset.tool).classList.add('active'); showHelp(b.dataset.tool); });
$('helpToggle').onclick=()=>{ $('helpPanel').classList.toggle('open'); };
showHelp('random');
function render(items) {
  $('results').innerHTML = items.map((item, i) => `<div class="result"><code>${item.replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;')}</code><button class="copy" data-index="${i}">Copy</button></div>`).join('');
  document.querySelectorAll('.copy').forEach(b => b.onclick = () => navigator.clipboard.writeText(items[b.dataset.index]).then(() => { b.textContent='Copied'; setTimeout(() => b.textContent='Copy', 900); }));
}
async function generate() {
  const p = new URLSearchParams({ length:$('length').value, count:$('count').value, upper:$('upper').checked, lower:$('lower').checked, digits:$('digits').checked, symbols:$('symbols').checked });
  $('status').textContent = 'Generating...';
  try { const r = await fetch('/api/generate?' + p); const data = await r.json(); if (!r.ok) throw new Error(data.error); render(data.results); $('status').textContent = `${data.results.length} result(s), ${data.length} characters each`; }
  catch (e) { $('status').textContent = e.message; }
}
$('generate').onclick = generate;
$('clear').onclick = () => { $('results').innerHTML=''; $('status').textContent='Ready'; };
function randomBytes(n) { const a=new Uint8Array(n); if (crypto.getRandomValues) crypto.getRandomValues(a); else for(let i=0;i<n;i++) a[i]=Math.random()*256; return a; }
function hex(a) { return [...a].map(x=>x.toString(16).padStart(2,'0')).join(''); }
$('uuidGenerate').onclick = () => { $('uuidOutput').textContent=Array.from({length:5},()=>{let a=randomBytes(16);a[6]=(a[6]&15)|64;a[8]=(a[8]&63)|128;let h=hex(a);return `${h.slice(0,8)}-${h.slice(8,12)}-${h.slice(12,16)}-${h.slice(16,20)}-${h.slice(20)}`;}).join('\n'); };
$('tokenGenerate').onclick = () => { const n=Math.min(256,Math.max(4,+$('tokenLength').value||32)), mode=$('tokenMode').value; const chars=mode==='hex'?'0123456789abcdef':mode==='base64'?'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_':'ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!@#$%^&*'; const a=randomBytes(n); $('tokenOutput').textContent=Array.from(a,x=>chars[x%chars.length]).join(''); };
$('baseEncode').onclick = () => { try { $('baseOutput').value=btoa(unescape(encodeURIComponent($('baseInput').value))); } catch(e) { $('baseOutput').value=e.message; } };
$('baseDecode').onclick = () => { try { $('baseOutput').value=decodeURIComponent(escape(atob($('baseInput').value))); } catch(e) { $('baseOutput').value='Invalid Base64'; } };
$('urlEncode').onclick = () => $('urlOutput').value=encodeURIComponent($('urlInput').value);
$('urlDecode').onclick = () => { try { $('urlOutput').value=decodeURIComponent($('urlInput').value); } catch(e) { $('urlOutput').value='Invalid URL encoding'; } };
function jsonAction(pretty) { try { $('jsonOutput').value=JSON.stringify(JSON.parse($('jsonInput').value),null,pretty?2:0); } catch(e) { $('jsonOutput').value='Invalid JSON: '+e.message; } }
$('jsonFormat').onclick=()=>jsonAction(true); $('jsonMinify').onclick=()=>jsonAction(false);
$('statsRun').onclick=()=>{const t=$('statsInput').value; $('statsOutput').textContent=`Characters: ${t.length}\nWords: ${t.trim()?t.trim().split(/\s+/).length:0}\nLines: ${t? t.split(/\r?\n/).length:0}\nUTF-8 bytes: ${new TextEncoder().encode(t).length}`;};
function escapeHtml(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;').replace(/'/g,'&#39;');}
function isCyrillic(cp){return (cp>=0x0400&&cp<=0x052f)||(cp>=0x2de0&&cp<=0x2dff)||(cp>=0xa640&&cp<=0xa69f);}
function isLatin(cp){return (cp>=0x0041&&cp<=0x005a)||(cp>=0x0061&&cp<=0x007a)||(cp>=0x00c0&&cp<=0x024f)||(cp>=0x1e00&&cp<=0x1eff)||(cp>=0x2c60&&cp<=0x2c7f)||(cp>=0xa720&&cp<=0xa7ff);}
$('scriptAnalyze').onclick=()=>{const text=$('scriptInput').value;if(text.length>1000000){$('scriptStats').textContent='Text is too large (maximum 1 MB)';return;}let cyr=0,lat=0,out='';for(const ch of text){const cp=ch.codePointAt(0),safe=escapeHtml(ch);if(isCyrillic(cp)){cyr++;out+=`<span class="cyrillic">${safe}</span>`;}else if(isLatin(cp)){lat++;out+=`<span class="latin">${safe}</span>`;}else out+=safe;}$('scriptOutput').innerHTML=out;$('scriptStats').textContent=`Characters: ${[...text].length} · Cyrillic: ${cyr} · Latin: ${lat}`;};
$('scriptClear').onclick=()=>{$('scriptInput').value='';$('scriptOutput').textContent='';$('scriptStats').textContent='';};
const b64url=s=>{let x=s.replace(/-/g,'+').replace(/_/g,'/');return decodeURIComponent(escape(atob(x+'='.repeat((4-x.length%4)%4))))};
const b64raw=s=>{let x=s.replace(/-/g,'+').replace(/_/g,'/');return atob(x+'='.repeat((4-x.length%4)%4));};
async function hmac256(secret,data){const k=await crypto.subtle.importKey('raw',new TextEncoder().encode(secret),{name:'HMAC',hash:'SHA-256'},false,['sign']);return new Uint8Array(await crypto.subtle.sign('HMAC',k,new TextEncoder().encode(data)));}
function strength(p){let score=0, reasons=[];if(p.length>=8)score++;else reasons.push('use at least 8 characters');if(p.length>=14)score++;if(/[a-z]/.test(p)&&/[A-Z]/.test(p))score++;else reasons.push('mix upper/lowercase');if(/\d/.test(p))score++;else reasons.push('add numbers');if(/[^A-Za-z0-9]/.test(p))score++;else reasons.push('add symbols');return `Score: ${score}/5 (${['very weak','weak','fair','good','strong','excellent'][score]})\n${reasons.length?'Suggestions: '+reasons.join(', '):'Good password composition'}`;}
function semver(v){return v.replace(/^v/,'').split(/[.+-]/).map(x=>/^\d+$/.test(x)?+x:x);}
function semverCompare(a,b){const x=semver(a),y=semver(b);for(let i=0;i<3;i++){if((x[i]||0)!==(y[i]||0))return (x[i]||0)>(y[i]||0)?1:-1;}return 0;}
function secretScan(t){const patterns=[['AWS access key',/AKIA[0-9A-Z]{16}/g],['GitHub token',/gh[pousr]_[A-Za-z0-9_]{20,}/g],['JWT',/eyJ[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+/g],['Private key',/-----BEGIN [A-Z ]+ PRIVATE KEY-----/g],['Password assignment',/(password|passwd|secret|token)\s*[:=]\s*[^\s]+/gi]];const hits=[];patterns.forEach(([n,re])=>{const m=t.match(re);if(m)hits.push(`${n}: ${m.length} match(es)`);});return hits.length?'Potential secrets found:\n'+hits.join('\n'):'No common secret patterns found';}
const bytesHex=b=>[...b].map(x=>x.toString(16).padStart(2,'0')).join(' ');
function ipv4Calc(v){const [ip,cidr]=v.trim().split('/'), p=ip.split('.').map(Number);if(p.length!==4||p.some(x=>x<0||x>255)||cidr<0||cidr>32)throw Error('Use address/prefix, for example 192.168.1.10/24');const n=p.reduce((a,x)=>(a<<8)|x,0)>>>0, mask=cidr==0?0:(0xffffffff<<(32-cidr))>>>0, net=(n&mask)>>>0, bc=(net|(~mask))>>>0, fmt=x=>[(x>>>24)&255,(x>>>16)&255,(x>>>8)&255,x&255].join('.'), hosts=cidr>=31?Math.max(0,bc-net+1):Math.max(0,bc-net-1);return `Network: ${fmt(net)}\nBroadcast: ${fmt(bc)}\nFirst host: ${fmt(cidr>=31?net:net+1)}\nLast host: ${fmt(cidr>=31?bc:bc-1)}\nMask: ${fmt(mask)}\nUsable hosts: ${hosts}`;}
async function toolboxRun(){const op=$('toolSelect').value,t=$('toolInput').value;try{let out='';
if(op==='sha256'){const r=await fetch('/api/hash',{method:'POST',headers:{'Content-Type':'text/plain'},body:t});out=await r.text();}
else if(op==='hmac'){if(!crypto.subtle)throw Error('HMAC requires a secure browser context');const key=await crypto.subtle.importKey('raw',new TextEncoder().encode('secret'),{name:'HMAC',hash:'SHA-256'},false,['sign']);out=bytesHex(new Uint8Array(await crypto.subtle.sign('HMAC',key,new TextEncoder().encode(t)))).replaceAll(' ','');}
else if(op==='strength'){out=strength(t);}
else if(op==='jwtverify'){const p=t.trim().split(/\n/),jwt=p.pop(),secret=p.join('\n');if(!secret||jwt.split('.').length!==3)throw Error('Put secret on first line and JWT on following line');const parts=jwt.split('.'),sig=await hmac256(secret,parts[0]+'.'+parts[1]),expected=btoa(String.fromCharCode(...sig)).replaceAll('+','-').replaceAll('/','_').replace(/=+$/,'');out=expected===parts[2]?'Valid HS256 signature':'Invalid HS256 signature';}
else if(op==='secretScan'){out=secretScan(t);}
else if(op==='sla'){const p=t.trim().split(/\s+/),slo=+(p[0]||99.9),days=+(p[1]||30),budget=(1-slo/100)*days*24*60;out=`SLO: ${slo}%\nPeriod: ${days} days\nAllowed downtime: ${Math.floor(budget/60)}h ${Math.round(budget%60)}m\nError budget ratio: ${(100-slo).toFixed(3)}%`;}
else if(op==='semver'){const p=t.trim().split(/\s+/);if(p.length<2)throw Error('Enter two versions, e.g. 1.2.0 1.3.0');const c=semverCompare(p[0],p[1]);out=`${p[0]} ${c===0?'=':c>0?'>':'<'} ${p[1]}`;}
else if(op==='commit'){out=/^(feat|fix|docs|refactor|test|chore|ci|perf|build|revert)(\([^)]+\))?!?: .+/.test(t.trim())?'Valid Conventional Commit':'Invalid. Example: feat(api): add health endpoint';}
else if(op==='docker'){const x=t.trim().replace(/^docker:\/\//,'').split('@');const tag=x[0].lastIndexOf(':')>x[0].lastIndexOf('/')?x[0].slice(x[0].lastIndexOf(':')+1):'latest';out=JSON.stringify({reference:x[0],tag,digest:x[1]||null,registry:x[0].includes('/')?x[0].split('/')[0]:'docker.io'},null,2);}
else if(op==='k8s'){const m=t.trim().match(/^([\d.]+)(Ki|Mi|Gi|Ti|m)?$/);if(!m)throw Error('Use values like 500m, 256Mi or 2Gi');const mult={m:.001,Ki:1024,Mi:1048576,Gi:1073741824,Ti:1099511627776};out=`Value: ${t}\nBytes: ${+(m[1])*(mult[m[2]]||1)}`;}
else if(op==='checklist'){out='[ ] Change approved\n[ ] Artifact checksum verified\n[ ] Backup confirmed\n[ ] Rollback plan ready\n[ ] Health endpoint checked\n[ ] Monitoring active\n[ ] Post-deploy smoke test\n[ ] Stakeholders notified';}
else if(op==='curl'){const p=t.trim().split(/\s+/),method=p.shift()||'GET',url=p.shift()||'https://example.com';out=`curl -i -X ${method} '${url}' -H 'Accept: application/json'`+(method==='GET'?'':" -H 'Content-Type: application/json' -d '{}'");}
else if(op==='headers'){const h=t.split(/\r?\n/).map(x=>x.split(':')[0].toLowerCase()),needed=['content-type','cache-control','x-content-type-options','x-frame-options'];out=needed.map(x=>`${h.includes(x)?'PASS':'WARN'} ${x}`).join('\n');}
else if(op==='jsonvalidate'){try{JSON.parse(t);out='Valid JSON';}catch(e){out='Invalid JSON: '+e.message;}}
else if(op==='changelog'){out=`## ${t||'1.0.0'} - ${new Date().toISOString().slice(0,10)}\n\n- Added change description.`;}
else if(op==='branch'){out=(t||'feature/my-change').toLowerCase().replace(/[^a-z0-9/_-]+/g,'-').replace(/^-|-$/g,'');}
else if(op==='compose'){const p=t.trim().split(/\s+/),name=p[0]||'app',image=p[1]||'nginx:latest';out=`services:\n  ${name}:\n    image: ${image}\n    restart: unless-stopped\n    ports:\n      - "8080:80"\n    healthcheck:\n      test: ["CMD", "wget", "--spider", "-q", "http://localhost"]\n      interval: 30s\n      timeout: 5s\n      retries: 3`;}
else if(op==='dockerport'){const p=t.trim().split(/\s+/);out=`ports:\n  - "${p[0]||8080}:${p[1]||80}"\n\nDocker run:\ndocker run -p ${p[0]||8080}:${p[1]||80} IMAGE`;}
else if(op==='healthcheck'){out='healthcheck:\n  test: ["CMD-SHELL", "curl -f http://localhost:8080/health || exit 1"]\n  interval: 30s\n  timeout: 5s\n  retries: 3\n  start_period: 10s';}
else if(op==='k8sdeploy'){const n=t||'app';out=`apiVersion: apps/v1\nkind: Deployment\nmetadata:\n  name: ${n}\nspec:\n  replicas: 2\n  selector:\n    matchLabels:\n      app: ${n}\n  template:\n    metadata:\n      labels:\n        app: ${n}\n    spec:\n      containers:\n        - name: ${n}\n          image: ${n}:latest\n          ports:\n            - containerPort: 8080`;}
else if(op==='k8sservice'){const n=t||'app';out=`apiVersion: v1\nkind: Service\nmetadata:\n  name: ${n}\nspec:\n  selector:\n    app: ${n}\n  ports:\n    - port: 80\n      targetPort: 8080\n  type: ClusterIP`;}
else if(op==='probes'){out='livenessProbe:\n  httpGet:\n    path: /health\n    port: 8080\n  initialDelaySeconds: 10\n  periodSeconds: 10\nreadinessProbe:\n  httpGet:\n    path: /ready\n    port: 8080\n  periodSeconds: 5';}
else if(op==='resources'){out='resources:\n  requests:\n    cpu: 100m\n    memory: 128Mi\n  limits:\n    cpu: 500m\n    memory: 512Mi';}
else if(op==='kubectl'){const p=t||'deployment app';out=`kubectl get ${p} -o wide\nkubectl describe ${p}\nkubectl logs ${p} --tail=100`;}
else if(op==='systemd'){const n=t||'my-service';out=`[Unit]\nDescription=${n}\nAfter=network-online.target\n\n[Service]\nExecStart=/usr/local/bin/${n}\nRestart=on-failure\nRestartSec=5\n\n[Install]\nWantedBy=multi-user.target`;}
else if(op==='chmod'){const n=parseInt(t||'755',8);out=`chmod ${t||'755'} file\nOwner: ${(n>>6)&7}\nGroup: ${(n>>3)&7}\nOther: ${n&7}`;}
else if(op==='env'){const bad=t.split(/\r?\n/).filter(x=>x.trim()&&!/^\s*#/.test(x)&&!/^\s*[A-Za-z_][A-Za-z0-9_]*=.*/.test(x));out=bad.length?'Invalid lines:\n'+bad.join('\n'):'Valid .env syntax';}
else if(op==='shellquote'){out="'"+t.replaceAll("'", "'\\''")+"'";}
else if(op==='sshconfig'){const p=t.trim().split(/\s+/);out=`Host ${p[0]||'server'}\n  HostName ${p[1]||'example.com'}\n  User ${p[2]||'deploy'}\n  Port ${p[3]||22}\n  IdentityFile ~/.ssh/id_ed25519`;}
else if(op==='promql'){out=`rate(http_requests_total{job=\"${t||'api'}\"}[5m])`;}
else if(op==='slo'){const p=t.trim().split(/\s+/),s=+(p[0]||99.9),d=+(p[1]||30),m=(1-s/100)*d*1440;out=`SLO ${s}% over ${d} days\nError budget: ${m.toFixed(1)} minutes\nBurn alert (2x): ${(m/2).toFixed(1)} minutes`;}
else if(op==='apdex'){const p=t.trim().split(/\s+/),sat=+(p[0]||90),tol=+(p[1]||5),total=+(p[2]||100);out=`Apdex: ${((sat+((total-sat-tol)/2))/total).toFixed(3)}\nSatisfied: ${sat}\nTolerating: ${tol}\nFrustrated: ${total-sat-tol}`;}
else if(op==='alert'){const n=t||'HighErrorRate';out=`groups:\n- name: generated\n  rules:\n  - alert: ${n}\n    expr: rate(http_requests_errors_total[5m]) > 0.05\n    for: 10m\n    labels:\n      severity: warning\n    annotations:\n      summary: ${n}`;}
else if(op==='logjson'){out=JSON.stringify({timestamp:new Date().toISOString(),level:'INFO',message:t,service:'unknown'},null,2);}
else if(op==='mask'){out=t.replace(/(password|secret|token|api[_-]?key)\s*[:=]\s*[^\s,]+/gi,'$1=***REDACTED***').replace(/gh[pousr]_[A-Za-z0-9_]+/g,'***TOKEN***');}
else if(op==='gha'){out=`name: CI\non:\n  push:\n    branches: [main]\njobs:\n  build:\n    runs-on: ubuntu-latest\n    steps:\n      - uses: actions/checkout@v4\n      - run: echo "Build ${t||'project'}"`;}
else if(op==='artifact'){const f=$('toolFile').files[0];if(!f)throw Error('Choose an artifact file first');out=JSON.stringify({name:f.name,size:f.size,type:f.type||'unknown',uploadedAt:new Date().toISOString()},null,2);}
else if(op==='postmortem'){out=`# Incident Postmortem\n\n## Summary\n${t||'Describe the incident.'}\n\n## Impact\n-\n\n## Timeline\n- [time] Event\n\n## Root cause\n-\n\n## Corrective actions\n- [ ]\n\n## Lessons learned\n-`;}
else if(op==='html'){out=t.replace(/[&<>"']/g,x=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[x]));}
else if(op==='unicode'){out=t.includes('U+')?String.fromCodePoint(...t.match(/[0-9a-f]{2,6}/gi).map(x=>parseInt(x,16))):[...t].map(x=>'U+'+x.codePointAt(0).toString(16).toUpperCase()).join(' ');}
else if(op==='binary'){out=/^[01\s]+$/.test(t)&&t.trim().length%8===0?new TextDecoder().decode(new Uint8Array(t.trim().split(/\s+/).map(x=>parseInt(x,2)))):[...new TextEncoder().encode(t)].map(x=>x.toString(2).padStart(8,'0')).join(' ');}
else if(op==='hex'){out=/^(?:[0-9a-f]{2}\s*)+$/i.test(t.trim())?new TextDecoder().decode(new Uint8Array(t.trim().split(/\s+/).map(x=>parseInt(x,16)))):bytesHex(new TextEncoder().encode(t));}
else if(op==='case'){out=`UPPER: ${t.toUpperCase()}\nLOWER: ${t.toLowerCase()}\nTitle: ${t.toLowerCase().replace(/\b\w/g,x=>x.toUpperCase())}\ncamel: ${t.toLowerCase().replace(/[^a-z0-9]+(.)/g,(_,x)=>x.toUpperCase()).replace(/^./,x=>x.toLowerCase())}`;}
else if(op==='slug'){out=t.toLowerCase().normalize('NFKD').replace(/[\u0300-\u036f]/g,'').replace(/[^a-z0-9]+/g,'-').replace(/^-|-$/g,'');}
else if(op==='jwt'){const p=t.split('.');if(p.length!==3)throw Error('JWT must have three parts');out=JSON.stringify({header:JSON.parse(b64url(p[0])),payload:JSON.parse(b64url(p[1])),signature:p[2]},null,2);}
else if(op==='urlparse'){const u=new URL(t);out=JSON.stringify({href:u.href,protocol:u.protocol,host:u.host,hostname:u.hostname,port:u.port||'(default)',pathname:u.pathname,search:u.search,hash:u.hash},null,2);}
else if(op==='jsoncsv'){if(t.trim().startsWith('[')){const a=JSON.parse(t), keys=[...new Set(a.flatMap(x=>Object.keys(x)))];out=[keys.join(','),...a.map(x=>keys.map(k=>JSON.stringify(x[k]??'')).join(','))].join('\n');}else{const [h,...rows]=t.trim().split(/\r?\n/),keys=h.split(',');out=JSON.stringify(rows.map(r=>Object.fromEntries(r.split(',').map((x,i)=>[keys[i],x]))),null,2);}}
else if(op==='ipv4'){out=ipv4Calc(t);}
else if(op==='mac'){out=Array.from(randomBytes(6),x=>x.toString(16).padStart(2,'0')).join(':');}
else if(op==='port'){out=String(1024+(randomBytes(2)[0]<<8|randomBytes(2)[1])%64511);}
else if(op==='ipv6'){out='fd'+hex(randomBytes(14)).replaceAll(' ','').match(/.{1,4}/g).join(':');}
else if(op==='ulid'){const time=Date.now().toString(16).padStart(12,'0').toUpperCase(), chars='0123456789ABCDEFGHJKMNPQRSTVWXYZ';out=time+Array.from(randomBytes(10),x=>chars[x%32]).join('');}
else if(op==='nanoid'){const chars='_-0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';out=Array.from(randomBytes(21),x=>chars[x%chars.length]).join('');}
else if(op==='lorem'){out='Lorem ipsum dolor sit amet, consectetur adipiscing elit. '.repeat(8).trim();}
else if(op==='fake'){const n=Math.floor(1000+Math.random()*8999);out=`Name: Alex Developer\nEmail: alex${n}@example.test\nUsername: user_${n}\nIPv4: 192.0.2.${n%254+1}\nUser-Agent: Mozilla/5.0 (ESP32 IT Tools)`;}
else if(op==='svg'){const [w,h]=(t||'640x360').split('x').map(Number);out=`<svg xmlns="http://www.w3.org/2000/svg" width="${w||640}" height="${h||360}" viewBox="0 0 ${w||640} ${h||360}"><rect width="100%" height="100%" fill="#8b5cf6"/><text x="50%" y="50%" fill="white" text-anchor="middle" dominant-baseline="middle">${w||640} × ${h||360}</text></svg>`;}
else if(op==='cron'){out='Every minute: * * * * *\nEvery hour: 0 * * * *\nDaily at midnight: 0 0 * * *\nEvery Sunday: 0 0 * * 0';}
else if(op==='diff'){const parts=t.split(/\r?\n---\r?\n/);if(parts.length!==2)throw Error('Put two texts separated by a line containing ---');const a=parts[0].split(/\r?\n/),b=parts[1].split(/\r?\n/);out=a.map((x,i)=>x===b[i]?'  '+x:'- '+x+'\n+ '+(b[i]||'')).join('\n');}
else if(op==='wifiscan'){const r=await fetch('/api/wifi/scan');out=JSON.stringify(await r.json(),null,2);}
else if(op==='net'){const p=t.trim().split(/\s+/),kind=p.shift()||'dns',host=p.shift()||'';const r=await fetch('/api/net?op='+encodeURIComponent(kind)+'&host='+encodeURIComponent(host)+'&port='+(p[0]||''));out=JSON.stringify(await r.json(),null,2);}
else if(op==='filehash'){const f=$('toolFile').files[0];if(!f)throw Error('Choose a file first');const digest=await crypto.subtle.digest('SHA-256',await f.arrayBuffer());out=`${f.name} (${f.size} bytes)\nSHA-256: ${bytesHex(new Uint8Array(digest)).replaceAll(' ','')}`;}
$('toolOutput').textContent=out;}catch(e){$('toolOutput').textContent='Error: '+e.message;}}
$('toolRun').onclick=toolboxRun; $('toolClear').onclick=()=>{$('toolInput').value='';$('toolOutput').textContent='';};
let qrSvg='';
$('qrGenerate').onclick=async()=>{const text=$('qrInput').value;if(!text){$('qrStatus').textContent='Enter text first';return;}if(text.length>600){$('qrStatus').textContent='Payload is too long (maximum 600 characters)';return;}$('qrStatus').textContent='Generating on ESP32...';try{const r=await fetch('/api/qr?text='+encodeURIComponent(text));if(!r.ok)throw Error(await r.text());qrSvg=await r.text();$('qrOutput').innerHTML=qrSvg;$('qrStatus').textContent='QR generated on the ESP32';}catch(e){$('qrStatus').textContent='QR error: '+e.message;}};
$('qrDownload').onclick=()=>{if(!qrSvg){$('qrStatus').textContent='Generate a QR code first';return;}const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([qrSvg],{type:'image/svg+xml'}));a.download='esp32-qr.svg';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000);};
let incidentReport=null;
$('runNetwork').onclick=async()=>{const op=$('networkAction').value,host=$('networkHost').value.trim(),port=$('networkPort').value;$('networkStatus').textContent='Running diagnostic...';try{let out;if(op==='ipv4'){out=ipv4Calc(host);}else if(op==='batch'){const ports=port.split(',').map(x=>x.trim()).filter(Boolean);const values=await Promise.all(ports.map(async p=>{const r=await fetch(`/api/net?op=tcp&host=${encodeURIComponent(host)}&port=${p}`);return await r.json();}));out=JSON.stringify(values,null,2);}else{const mapped=op==='route'?'dhcp':op==='dnsdetail'?'dhcp':op;const url=op==='wifiscan'?'/api/wifi/scan':`/api/net?op=${encodeURIComponent(mapped)}&host=${encodeURIComponent(host)}&port=${port}`;const r=await fetch(url);out=JSON.stringify(await r.json(),null,2);if(op==='dnsdetail')out+='\n\nDNS detail is included in the DHCP/network response; use DNS lookup for hostname resolution.';} $('networkOutput').textContent=out;$('networkStatus').textContent='Completed';}catch(e){$('networkStatus').textContent='Diagnostic error';$('networkOutput').textContent=e.message;}};
$('runDns').onclick=async()=>{let base=$('dnsGateway').value.replace(/\/$/,'');const action=$('dnsAction').value,name=encodeURIComponent($('dnsName').value);$('dnsStatus').textContent='Running DNS Inspector...';try{const r=await fetch(`${base}/api/dns/${action}?name=${name}`);const x=await r.json();$('dnsOutput').textContent=JSON.stringify(x,null,2);$('dnsStatus').textContent=r.ok?'Completed':'Gateway error';}catch(e){$('dnsStatus').textContent='Gateway unavailable: '+e.message;}};
$('sendApi').onclick=async()=>{const q=new URLSearchParams({method:$('apiMethod').value,host:$('apiHost').value,port:$('apiPort').value,path:$('apiPath').value,body:$('apiBody').value});$('apiStatus').textContent='Sending...';try{const r=await fetch('/api/http?'+q);const x=await r.json();$('apiOutput').textContent=JSON.stringify(x,null,2);$('apiStatus').textContent=(x.ok?'HTTP '+x.status:'Request failed')+' · '+x.latencyMs+' ms';}catch(e){$('apiStatus').textContent='API error: '+e.message;}};
$('logFile').onchange=async()=>{const f=$('logFile').files[0];if(!f)return;if(f.size>2*1024*1024){$('logFileStatus').textContent='File is too large (maximum 2 MB)';$('logFile').value='';return;}try{$('logInput').value=await f.text();$('logFileStatus').textContent=`Loaded ${f.name} · ${f.size} bytes`;}catch(e){$('logFileStatus').textContent='Unable to read file: '+e.message;}};
$('analyzeLogs').onclick=()=>{const t=$('logInput').value,lines=t? t.split(/\r?\n/):[],errors=lines.filter(x=>/\b(error|fatal|critical|exception|fail(ed|ure)?)\b/i.test(x)),warn=lines.filter(x=>/\bwarn(ing)?\b/i.test(x)),info=lines.filter(x=>/\binfo\b/i.test(x));$('logOutput').textContent=`Lines: ${lines.length}\nErrors: ${errors.length}\nWarnings: ${warn.length}\nInfo: ${info.length}\n\nTop errors:\n${errors.slice(0,10).join('\n')}`;};
$('maskLogs').onclick=()=>{$('logInput').value=$('logInput').value.replace(/(password|secret|token|api[_-]?key)\s*[:=]\s*[^\s,]+/gi,'$1=***REDACTED***').replace(/gh[pousr]_[A-Za-z0-9_]+/g,'***TOKEN***');};
$('runIncident').onclick=async()=>{const host=$('incidentHost').value.trim(),port=$('incidentPort').value,path=$('incidentPath').value||'/';if(!host){$('incidentStatus').textContent='Enter a target host';return;}$('incidentStatus').textContent='Running DNS, gateway, TCP and HTTP checks...';try{const r=await fetch('/api/incident?host='+encodeURIComponent(host)+'&port='+port+'&path='+encodeURIComponent(path));incidentReport=await r.json();$('incidentOutput').textContent=JSON.stringify(incidentReport,null,2);$('incidentStatus').textContent=(incidentReport.ok?'PASS':'FAIL')+' · '+incidentReport.durationMs+' ms';}catch(e){$('incidentStatus').textContent='Probe error: '+e.message;}};
$('copyIncident').onclick=()=>{if(incidentReport)navigator.clipboard.writeText(JSON.stringify(incidentReport,null,2));};
$('downloadIncident').onclick=()=>{if(!incidentReport)return;const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([JSON.stringify(incidentReport,null,2)],{type:'application/json'}));a.download='incident-report.json';a.click();};
async function refreshSystem() { try { const x=await (await fetch('/api/system')).json(); $('systemOutput').textContent=`Chip: ${x.chip}\nSDK: ${x.sdk}\nCPU: ${x.cpu} MHz\nFree heap: ${x.heap} bytes\nFlash: ${x.flash} bytes\nUptime: ${x.uptime}s\nAP IP: ${x.apIp}\nStation: ${x.staIp||'not connected'}\nHostname: ${x.hostname}`; } catch(e) { $('systemOutput').textContent='Unable to read device status'; } }
$('refreshSystem').onclick=refreshSystem;
const metricHistory=[];function drawMetrics(){const c=$('metricsChart'),ctx=c.getContext('2d'),w=c.width,h=c.height;ctx.clearRect(0,0,w,h);ctx.strokeStyle='#374151';for(let y=20;y<h;y+=40){ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);ctx.stroke();}if(metricHistory.length<2)return;const max=Math.max(...metricHistory.map(x=>x.heap),1),step=w/(metricHistory.length-1);ctx.strokeStyle='#8b5cf6';ctx.beginPath();metricHistory.forEach((x,i)=>{const y=h-10-(x.heap/max)*(h-25);i?ctx.lineTo(i*step,y):ctx.moveTo(i*step,y);});ctx.stroke();ctx.strokeStyle='#22d3ee';ctx.beginPath();metricHistory.forEach((x,i)=>{const y=h-10-(x.temp/100)*(h-25);i?ctx.lineTo(i*step,y):ctx.moveTo(i*step,y);});ctx.stroke();}
async function pollMetrics(){try{const x=await (await fetch('/api/metrics')).json();metricHistory.push(x);if(metricHistory.length>60)metricHistory.shift();drawMetrics();}catch(e){}} setInterval(pollMetrics,3000);
$('saveWifi').onclick=async()=>{const body=new URLSearchParams({ssid:$('wifiSsid').value,password:$('wifiPassword').value});const r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});$('systemOutput').textContent=await r.text()+'\nThe device is restarting...';};
$('uploadFirmware').onclick=async()=>{const f=$('firmware').files[0];if(!f){$('otaStatus').textContent='Select a .bin file first';return;} $('otaStatus').textContent='Uploading...';const r=await fetch('/api/update',{method:'POST',body:f});$('otaStatus').textContent=await r.text();};
refreshSystem();
generate();
</script>
</body>
</html>
)HTML";

String buildAlphabet() {
  String alphabet;
  if (server.arg("upper") == "true") alphabet += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  if (server.arg("lower") == "true") alphabet += "abcdefghijklmnopqrstuvwxyz";
  if (server.arg("digits") == "true") alphabet += "0123456789";
  if (server.arg("symbols") == "true") alphabet += "!@#$%^&*()-_=+[]{};:,.?/<>";
  return alphabet;
}

void addSecurityHeaders() {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.sendHeader("X-Frame-Options", "SAMEORIGIN");
}

void handlePage() {
  addSecurityHeaders();
  server.send_P(200, "text/html; charset=utf-8", kPage);
}

void handleSystem() {
  const String staIp = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
  String json = String("{\"chip\":\"") + ESP.getChipModel() +
                "\",\"sdk\":\"" + String(ESP.getSdkVersion()) +
                "\",\"cpu\":" + String(ESP.getCpuFreqMHz()) +
                ",\"heap\":" + String(ESP.getFreeHeap()) +
                ",\"flash\":" + String(ESP.getFlashChipSize()) +
                ",\"uptime\":" + String(millis() / 1000) +
                ",\"apIp\":\"" + WiFi.softAPIP().toString() +
                "\",\"staIp\":\"" + staIp +
                "\",\"hostname\":\"" + String(kHostname) + "\",\"version\":\"" + String(kVersion) + "\"}";
  server.send(200, "application/json", json);
}

void handleHash() {
  const String input = server.arg("plain");
  uint8_t digest[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);
  mbedtls_sha256_update_ret(&ctx, reinterpret_cast<const uint8_t *>(input.c_str()), input.length());
  mbedtls_sha256_finish_ret(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  char result[65];
  for (size_t i = 0; i < sizeof(digest); ++i) snprintf(result + i * 2, 3, "%02x", digest[i]);
  result[64] = '\0';
  server.send(200, "text/plain", result);
}

void handleMetrics() {
  if (WiFi.status() == WL_CONNECTED && wifiConnectedAt == 0) wifiConnectedAt = millis();
  const float chipTemp = temperatureRead();
  const size_t total = LittleFS.totalBytes();
  const size_t used = LittleFS.usedBytes();
  String json = "{\"temperature\":" + String(chipTemp, 1) +
                ",\"heap\":" + String(ESP.getFreeHeap()) +
                ",\"uptime\":" + String(millis() / 1000) +
                ",\"generationRate\":" + String(generationRate, 2) +
                ",\"littlefsFree\":" + String(total > used ? total - used : 0) +
                ",\"wifiUptime\":" + String(wifiConnectedAt ? (millis() - wifiConnectedAt) / 1000 : 0) + "}";
  server.send(200, "application/json", json);
}

void handlePrometheus() {
  String body = "# HELP esp32_free_heap_bytes Free heap\n# TYPE esp32_free_heap_bytes gauge\nesp32_free_heap_bytes " + String(ESP.getFreeHeap()) +
                "\n# HELP esp32_uptime_seconds Uptime\n# TYPE esp32_uptime_seconds counter\nesp32_uptime_seconds " + String(millis() / 1000) +
                "\n# HELP esp32_chip_temperature_celsius Chip temperature\n# TYPE esp32_chip_temperature_celsius gauge\nesp32_chip_temperature_celsius " + String(temperatureRead(), 1) +
                "\n# HELP esp32_generation_rate_per_second Random generation rate\n# TYPE esp32_generation_rate_per_second gauge\nesp32_generation_rate_per_second " + String(generationRate, 2) + "\n";
  server.send(200, "text/plain; version=0.0.4", body);
}

String jsonEscape(const String &value) {
  String result;
  result.reserve(value.length() + 16);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c == '\\') result += "\\\\";
    else if (c == '"') result += "\\\"";
    else if (c == '\n') result += "\\n";
    else if (c == '\r') result += "\\r";
    else if (static_cast<uint8_t>(c) >= 32) result += c;
  }
  return result;
}

void handleHttp() {
  const String method = server.arg("method").isEmpty() ? "GET" : server.arg("method");
  const String host = server.arg("host");
  const String path = server.arg("path").isEmpty() ? "/" : server.arg("path");
  const String body = server.arg("body");
  const uint16_t port = static_cast<uint16_t>(constrain(server.arg("port").toInt(), 1, 65535));
  const unsigned long started = millis();
  IPAddress ip;
  WiFiClient client;
  int status = 0;
  String response;
  bool ok = !host.isEmpty() && WiFi.hostByName(host.c_str(), ip) && client.connect(ip, port, 5000);
  if (ok) {
    client.printf("%s %s HTTP/1.0\r\nHost: %s\r\nAccept: application/json, text/plain, */*\r\nConnection: close\r\n", method.c_str(), path.c_str(), host.c_str());
    if (method != "GET" && method != "DELETE") client.printf("Content-Type: application/json\r\nContent-Length: %u\r\n", body.length());
    client.print("\r\n");
    if (method != "GET" && method != "DELETE") client.print(body);
    const String line = client.readStringUntil('\n');
    const int firstSpace = line.indexOf(' ');
    if (firstSpace >= 0) status = line.substring(firstSpace + 1).toInt();
    const unsigned long deadline = millis() + 5000;
    while (client.connected() && millis() < deadline && response.length() < 8192) {
      if (client.available()) response += static_cast<char>(client.read());
      else delay(1);
    }
  }
  client.stop();
  String json = "{\"ok\":" + String(ok ? "true" : "false") + ",\"status\":" + String(status) +
                ",\"host\":\"" + jsonEscape(host) + "\",\"ip\":\"" + ip.toString() +
                "\",\"latencyMs\":" + String(millis() - started) + ",\"response\":\"" + jsonEscape(response) + "\"}";
  server.send(200, "application/json", json);
}

void handleIncident() {
  const String host = server.arg("host");
  const uint16_t port = static_cast<uint16_t>(constrain(server.arg("port").toInt(), 1, 65535));
  const String path = server.arg("path").isEmpty() ? "/" : server.arg("path");
  const unsigned long started = millis();
  IPAddress ip;
  const bool dnsOk = !host.isEmpty() && WiFi.hostByName(host.c_str(), ip);
  unsigned long dnsMs = millis() - started;
  bool tcpOk = false;
  unsigned long tcpMs = 0;
  int httpStatus = 0;
  unsigned long httpMs = 0;
  if (dnsOk) {
    WiFiClient tcp;
    const unsigned long tcpStart = millis();
    tcpOk = tcp.connect(ip, port, 3000);
    tcpMs = millis() - tcpStart;
    tcp.stop();
    WiFiClient http;
    const unsigned long httpStart = millis();
    if (http.connect(ip, 80, 3000)) {
      http.printf("GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path.c_str(), host.c_str());
      const String line = http.readStringUntil('\n');
      const int space = line.indexOf(' ');
      if (space >= 0) httpStatus = line.substring(space + 1).toInt();
    }
    http.stop();
    httpMs = millis() - httpStart;
  }
  const bool gatewayOk = WiFi.status() == WL_CONNECTED && WiFi.gatewayIP() != IPAddress(0, 0, 0, 0);
  const bool ok = dnsOk && tcpOk && httpStatus >= 200 && httpStatus < 500;
  String json = "{\"version\":\"" + String(kVersion) + "\",\"target\":\"" + host + "\",\"ip\":\"" + ip.toString() +
                "\",\"ok\":" + String(ok ? "true" : "false") + ",\"durationMs\":" + String(millis() - started) +
                ",\"checks\":{\"gateway\":{\"ok\":" + String(gatewayOk ? "true" : "false") +
                "},\"dns\":{\"ok\":" + String(dnsOk ? "true" : "false") + ",\"latencyMs\":" + String(dnsMs) +
                "},\"tcp\":{\"ok\":" + String(tcpOk ? "true" : "false") + ",\"port\":" + String(port) + ",\"latencyMs\":" + String(tcpMs) +
                "},\"http\":{\"ok\":" + String(httpStatus >= 200 && httpStatus < 500 ? "true" : "false") + ",\"status\":" + String(httpStatus) + ",\"latencyMs\":" + String(httpMs) + "}}}";
  server.send(200, "application/json", json);
}

void handleNet() {
  const String op = server.arg("op");
  const String host = server.arg("host");
  const unsigned long started = millis();
  if (op == "dhcp") {
    String json = "{\"ip\":\"" + WiFi.localIP().toString() + "\",\"gateway\":\"" + WiFi.gatewayIP().toString() +
                  "\",\"subnet\":\"" + WiFi.subnetMask().toString() + "\",\"dns1\":\"" + WiFi.dnsIP(0).toString() +
                  "\",\"dns2\":\"" + WiFi.dnsIP(1).toString() + "\"}";
    server.send(200, "application/json", json);
    return;
  }
  if (op == "mdns") {
    const int count = MDNS.queryService(host.length() ? host.c_str() : "http", "tcp");
    String json = "{\"count\":" + String(count) + ",\"services\":[";
    for (int i = 0; i < count; ++i) { if (i) json += ','; json += "{\"host\":\"" + MDNS.hostname(i) + "\",\"ip\":\"" + MDNS.IP(i).toString() + "\",\"port\":" + String(MDNS.port(i)) + "}"; }
    json += "]}";
    server.send(200, "application/json", json);
    return;
  }
  if (host.isEmpty()) { server.send(400, "application/json", R"({"error":"host is required"})"); return; }
  IPAddress ip;
  if (!WiFi.hostByName(host.c_str(), ip)) { server.send(200, "application/json", R"({"ok":false,"error":"DNS lookup failed"})"); return; }
  if (op == "dns") {
    server.send(200, "application/json", "{\"ok\":true,\"hostname\":\"" + host + "\",\"ip\":\"" + ip.toString() + "\",\"latencyMs\":" + String(millis() - started) + "}");
    return;
  }
  uint16_t port = op == "http" ? 80 : static_cast<uint16_t>(server.arg("port").toInt());
  if (op == "ping" && port == 0) port = 80;
  WiFiClient client;
  const bool connected = client.connect(ip, port, 3000);
  const unsigned long latency = millis() - started;
  if (op == "http" && connected) {
    client.printf("GET / HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", host.c_str());
    String line = client.readStringUntil('\n');
    client.stop();
    int code = line.indexOf(' ') >= 0 ? line.substring(line.indexOf(' ') + 1).toInt() : 0;
    server.send(200, "application/json", "{\"ok\":true,\"status\":" + String(code) + ",\"latencyMs\":" + String(latency) + "}");
    return;
  }
  client.stop();
  server.send(200, "application/json", "{\"ok\":" + String(connected ? "true" : "false") + ",\"ip\":\"" + ip.toString() + "\",\"port\":" + String(port) + ",\"latencyMs\":" + String(latency) + "}");
}

void handleWifiScan() {
  const int count = WiFi.scanNetworks(false, true);
  String json = "[";
  for (int i = 0; i < count; ++i) {
    if (i) json += ',';
    String ssid = WiFi.SSID(i);
    ssid.replace("\\", "\\\\");
    ssid.replace("\"", "\\\"");
    json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
            ",\"channel\":" + String(WiFi.channel(i)) +
            ",\"encrypted\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  json += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void captureQr(esp_qrcode_handle_t qr) {
  generatedQr = qr;
}

void handleQr() {
  const String text = server.arg("text");
  if (text.isEmpty() || text.length() > 2000) {
    server.send(400, "text/plain", "QR payload must contain 1-2000 characters");
    return;
  }
  generatedQr = nullptr;
  esp_qrcode_config_t config = ESP_QRCODE_CONFIG_DEFAULT();
  config.display_func = captureQr;
  config.max_qrcode_version = 20;
  if (esp_qrcode_generate(&config, text.c_str()) != ESP_OK || generatedQr == nullptr) {
    server.send(400, "text/plain", "Payload is too large for QR generation");
    return;
  }
  const int size = esp_qrcode_get_size(generatedQr);
  String svg;
  svg.reserve(static_cast<size_t>(size * size * 12));
  svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 ";
  svg += String(size + 8) + " " + String(size + 8) + "\" shape-rendering=\"crispEdges\"><rect width=\"100%\" height=\"100%\" fill=\"white\"/><path fill=\"black\" d=\"";
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      if (esp_qrcode_get_module(generatedQr, x, y)) {
        svg += "M" + String(x + 4) + "," + String(y + 4) + "h1v1h-1z";
      }
    }
  }
  svg += "\"/></svg>";
  server.send(200, "image/svg+xml", svg);
}

void handleWifi() {
  const String ssid = server.arg("ssid");
  const String password = server.arg("password");
  if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 63) {
    server.send(400, "text/plain", "Invalid Wi-Fi settings");
    return;
  }
  preferences.begin("network", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();
  server.send(200, "text/plain", "Wi-Fi settings saved");
  delay(500);
  ESP.restart();
}

void handleUpdateUpload() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    updateInProgress = true;
    updateSize = 0;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (upload.status == UPLOAD_FILE_WRITE && updateInProgress) {
    updateSize += Update.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END && updateInProgress) {
    if (!Update.end(true)) {
      Update.printError(Serial);
      updateInProgress = false;
    }
  }
}

void handleUpdateDone() {
  if (updateInProgress && !Update.hasError()) {
    server.send(200, "text/plain", "Firmware uploaded. Restarting...");
    delay(700);
    ESP.restart();
  } else {
    server.send(500, "text/plain", "Firmware update failed");
  }
}

void handleGenerate() {
  const int length = constrain(server.arg("length").toInt(), 1, static_cast<int>(kMaxLength));
  const int count = constrain(server.arg("count").toInt(), 1, static_cast<int>(kMaxResults));
  const String alphabet = buildAlphabet();

  if (alphabet.isEmpty()) {
    server.send(400, "application/json", R"({"error":"Select at least one character set"})");
    return;
  }

  String json = "{\"length\":" + String(length) + ",\"results\":[";
  for (int row = 0; row < count; ++row) {
    String value;
    value.reserve(length);
    for (int i = 0; i < length; ++i) value += alphabet[esp_random() % alphabet.length()];
    if (row) json += ',';
    json += '"' + value + '"';
    Serial.println(value);
    generationCount++;
  }
  if (generationWindowAt == 0) generationWindowAt = millis();
  if (millis() - generationWindowAt >= 1000) {
    generationRate = generationCount * 1000.0f / (millis() - generationWindowAt);
    generationCount = 0;
    generationWindowAt = millis();
  }
  json += "]}";
  server.send(200, "application/json", json);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  WiFi.setHostname(kHostname);
  preferences.begin("network", true);
  configuredSsid = preferences.getString("ssid", "");
  configuredPassword = preferences.getString("password", "");
  preferences.end();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(kApName, kApPassword);
  LittleFS.begin(true);
  if (configuredSsid.length()) {
    Serial.print("Connecting to saved Wi-Fi: ");
    Serial.println(configuredSsid);
    WiFi.begin(configuredSsid.c_str(), configuredPassword.c_str());
    const unsigned long deadline = millis() + 10000;
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
      delay(100);
      Serial.print('.');
    }
    Serial.println();
  }
  MDNS.begin(kHostname);
  server.on("/", HTTP_GET, handlePage);
  server.on("/api/generate", HTTP_GET, handleGenerate);
  server.on("/api/system", HTTP_GET, handleSystem);
  server.on("/api/metrics", HTTP_GET, handleMetrics);
  server.on("/metrics", HTTP_GET, handlePrometheus);
  server.on("/api/http", HTTP_GET, handleHttp);
  server.on("/api/incident", HTTP_GET, handleIncident);
  server.on("/api/net", HTTP_GET, handleNet);
  server.on("/api/hash", HTTP_POST, handleHash);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/qr", HTTP_GET, handleQr);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.on("/api/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
  Serial.println();
  Serial.println("ESP32 Random Data Generator");
  Serial.print("Wi-Fi network: ");
  Serial.println(kApName);
  Serial.print("Password: ");
  Serial.println(kApPassword);
  Serial.print("Open: http://");
  Serial.println(WiFi.softAPIP());
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Home Wi-Fi: http://");
    Serial.println(WiFi.localIP());
  }
  Serial.print("mDNS: http://");
  Serial.print(kHostname);
  Serial.println(".local");
}

void loop() {
  server.handleClient();
}
