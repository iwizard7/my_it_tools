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
constexpr char kVersion[] = "1.4.0";
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
    footer { color:var(--muted); text-align:center; margin-top:22px; font-size:12px; }
  </style>
</head>
<body>
<main>
  <h1>ESP32 IT Tools</h1>
  <p class="subtitle">Useful developer tools running locally on your ESP32-C3</p>
  <div class="layout">
    <nav class="nav">
      <button class="active" data-tool="random">🎲 Random data</button>
      <button data-tool="uuid">🆔 UUID generator</button>
      <button data-tool="token">🔑 Token & password</button>
      <button data-tool="base64">🔤 Base64</button>
      <button data-tool="url">🔗 URL encoder</button>
      <button data-tool="json">{ } JSON tools</button>
      <button data-tool="stats">📊 Text statistics</button>
      <button data-tool="toolbox">🧰 More IT tools</button>
      <button data-tool="qr">▦ QR generator</button>
      <button data-tool="system">⚙️ Device & OTA</button>
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
      <div id="toolbox" class="tool"><h2>More IT tools</h2><p class="hint">Offline converters, generators, network helpers and security utilities.</p><select id="toolSelect"><option value="sha256">SHA-256 hash</option><option value="hmac">HMAC note</option><option value="strength">Password strength</option><option value="jwtverify">JWT signature verification</option><option value="html">HTML entities</option><option value="unicode">Text ↔ Unicode</option><option value="binary">Text ↔ binary</option><option value="hex">Text ↔ hexadecimal</option><option value="case">Case converter</option><option value="slug">Slugify string</option><option value="jwt">JWT decoder</option><option value="urlparse">URL parser</option><option value="jsoncsv">JSON array ↔ CSV</option><option value="ipv4">IPv4 subnet calculator</option><option value="mac">MAC address generator</option><option value="port">Random port generator</option><option value="ipv6">IPv6 ULA generator</option><option value="ulid">ULID generator</option><option value="nanoid">NanoID generator</option><option value="lorem">Lorem Ipsum</option><option value="fake">Fake test data</option><option value="svg">SVG placeholder</option><option value="cron">Cron template</option><option value="diff">Simple text diff</option><option value="wifiscan">Wi‑Fi scanner</option><option value="net">Network diagnostic</option><option value="filehash">File SHA-256</option></select><br><br><textarea id="toolInput" placeholder="Input..."></textarea><br><input id="toolFile" type="file" style="margin-top:10px"><br><br><button id="toolRun">Run tool</button><button id="toolClear" class="secondary">Clear</button><div id="toolOutput" class="output"></div></div>
      <div id="qr" class="tool"><h2>QR code generator</h2><p class="hint">Generate a QR code on the ESP32. Maximum payload: 600 characters. Works offline.</p><textarea id="qrInput" placeholder="Text, URL or Wi‑Fi payload..."></textarea><br><br><button id="qrGenerate">Generate QR</button><button id="qrDownload" class="secondary">Download SVG</button><div id="qrStatus" class="status"></div><div id="qrOutput" style="background:white;border-radius:12px;padding:18px;margin-top:16px;text-align:center;min-height:120px"></div></div>
      <div id="system" class="tool"><h2>Device & OTA</h2><p class="hint">Live ESP32 status, Wi‑Fi configuration, metrics and wireless firmware updates.</p><button id="refreshSystem">Refresh status</button><div id="systemOutput" class="output">Loading...</div><canvas id="metricsChart" width="700" height="220" style="width:100%;margin-top:16px;background:#111827;border-radius:10px"></canvas><hr><h3>Connect to home Wi‑Fi</h3><p class="hint">The ESP32 access point remains available while connecting.</p><input id="wifiSsid" placeholder="Wi‑Fi network name"><br><br><input id="wifiPassword" type="password" placeholder="Wi‑Fi password"><br><br><button id="saveWifi">Save and restart</button><hr><h3>OTA firmware update</h3><input id="firmware" type="file" accept=".bin"><br><br><button id="uploadFirmware">Upload firmware</button><div id="otaStatus" class="status"></div></div>
    </section>
  </div>
  <footer>ESP32-C3 · local-only tools · Wi-Fi AP: ESP32-Random-Tools · v1.4.0</footer>
</main>
<script>
const $ = id => document.getElementById(id);
document.querySelectorAll('[data-tool]').forEach(b => b.onclick = () => { document.querySelectorAll('.nav button').forEach(x => x.classList.remove('active')); document.querySelectorAll('.tool').forEach(x => x.classList.remove('active')); b.classList.add('active'); $(b.dataset.tool).classList.add('active'); });
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
const b64url=s=>{let x=s.replace(/-/g,'+').replace(/_/g,'/');return decodeURIComponent(escape(atob(x+'='.repeat((4-x.length%4)%4))))};
const b64raw=s=>{let x=s.replace(/-/g,'+').replace(/_/g,'/');return atob(x+'='.repeat((4-x.length%4)%4));};
async function hmac256(secret,data){const k=await crypto.subtle.importKey('raw',new TextEncoder().encode(secret),{name:'HMAC',hash:'SHA-256'},false,['sign']);return new Uint8Array(await crypto.subtle.sign('HMAC',k,new TextEncoder().encode(data)));}
function strength(p){let score=0, reasons=[];if(p.length>=8)score++;else reasons.push('use at least 8 characters');if(p.length>=14)score++;if(/[a-z]/.test(p)&&/[A-Z]/.test(p))score++;else reasons.push('mix upper/lowercase');if(/\d/.test(p))score++;else reasons.push('add numbers');if(/[^A-Za-z0-9]/.test(p))score++;else reasons.push('add symbols');return `Score: ${score}/5 (${['very weak','weak','fair','good','strong','excellent'][score]})\n${reasons.length?'Suggestions: '+reasons.join(', '):'Good password composition'}`;}
const bytesHex=b=>[...b].map(x=>x.toString(16).padStart(2,'0')).join(' ');
function ipv4Calc(v){const [ip,cidr]=v.trim().split('/'), p=ip.split('.').map(Number);if(p.length!==4||p.some(x=>x<0||x>255)||cidr<0||cidr>32)throw Error('Use address/prefix, for example 192.168.1.10/24');const n=p.reduce((a,x)=>(a<<8)|x,0)>>>0, mask=cidr==0?0:(0xffffffff<<(32-cidr))>>>0, net=(n&mask)>>>0, bc=(net|(~mask))>>>0, fmt=x=>[(x>>>24)&255,(x>>>16)&255,(x>>>8)&255,x&255].join('.'), hosts=cidr>=31?Math.max(0,bc-net+1):Math.max(0,bc-net-1);return `Network: ${fmt(net)}\nBroadcast: ${fmt(bc)}\nFirst host: ${fmt(cidr>=31?net:net+1)}\nLast host: ${fmt(cidr>=31?bc:bc-1)}\nMask: ${fmt(mask)}\nUsable hosts: ${hosts}`;}
async function toolboxRun(){const op=$('toolSelect').value,t=$('toolInput').value;try{let out='';
if(op==='sha256'){const r=await fetch('/api/hash',{method:'POST',headers:{'Content-Type':'text/plain'},body:t});out=await r.text();}
else if(op==='hmac'){if(!crypto.subtle)throw Error('HMAC requires a secure browser context');const key=await crypto.subtle.importKey('raw',new TextEncoder().encode('secret'),{name:'HMAC',hash:'SHA-256'},false,['sign']);out=bytesHex(new Uint8Array(await crypto.subtle.sign('HMAC',key,new TextEncoder().encode(t)))).replaceAll(' ','');}
else if(op==='strength'){out=strength(t);}
else if(op==='jwtverify'){const p=t.trim().split(/\n/),jwt=p.pop(),secret=p.join('\n');if(!secret||jwt.split('.').length!==3)throw Error('Put secret on first line and JWT on following line');const parts=jwt.split('.'),sig=await hmac256(secret,parts[0]+'.'+parts[1]),expected=btoa(String.fromCharCode(...sig)).replaceAll('+','-').replaceAll('/','_').replace(/=+$/,'');out=expected===parts[2]?'Valid HS256 signature':'Invalid HS256 signature';}
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
