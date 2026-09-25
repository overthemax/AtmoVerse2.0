#include "WebMinimal.h"
#include <WiFi.h>
#include "Config.h"

// NOTA: Funzioni minimali per risparmiare spazio nel firmware.
// I contenuti web sono stati spostati sulla SD card nella cartella /www/

// Pagina principale (stub minimo)
String generateMinimalMainPage() {
  return "<!DOCTYPE html><html><body><h1>AtmoVerse</h1><p>Verifica SD card</p></body></html>";
}

// Pagina citazioni (stub minimo)
String generateMinimalQuotesPage() {
  return "<!DOCTYPE html><html><body><h1>Citazioni</h1><p>Verifica SD card</p></body></html>";
}

// Pagina info (stub minimo)
String generateMinimalInfoPage() {
  return "<!DOCTYPE html><html><body><h1>Info</h1></body></html>";
}

// Pagina setup WiFi (stub minimo)
String generateMinimalSetupPage(String ssid) {
  String html = "<!DOCTYPE html><html><body><h1>Setup WiFi</h1>";
  html += "<form action='/connect' method='post'>";
  html += "<input name='ssid' value='" + ssid + "'>";
  html += "<input name='password' type='password'>";
  html += "<button>Connetti</button></form></body></html>";
  return html;
}

// Pagina di configurazione minima contenuta nel firmware (vedi WebMinimal.h)
static const char FALLBACK_SETUP_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="it"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>AtmoVerse - Configurazione</title>
<style>
:root{--bg:#f4f1ea;--s:#fff;--t:#2b2620;--m:#6f665a;--b:#e4ddd0;--a:#7a5c3a;--at:#fff}
@media(prefers-color-scheme:dark){:root{--bg:#171512;--s:#221f1b;--t:#efe9df;--m:#a99f90;--b:#3a342d;--a:#d2a877;--at:#1b1712}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--t);font:16px/1.5 -apple-system,system-ui,Roboto,Arial,sans-serif}
main{max-width:520px;margin:0 auto;padding:16px 16px 40px}h1{font-size:1.5rem;margin:8px 0 4px}
.card{background:var(--s);border:1px solid var(--b);border-radius:14px;padding:16px;margin:16px 0}
label{display:block;font-weight:600;margin:12px 0 6px}
input{width:100%;min-height:48px;padding:12px;font:inherit;font-size:16px;color:var(--t);background:var(--bg);border:1px solid var(--b);border-radius:10px}
button{width:100%;min-height:48px;margin-top:16px;font:inherit;font-weight:600;color:var(--at);background:var(--a);border:0;border-radius:12px}
button.sec{background:transparent;color:var(--t);border:1px solid var(--b);margin-top:8px}
.m{color:var(--m);font-size:.9rem}.net{display:flex;justify-content:space-between;padding:12px 4px;border-bottom:1px solid var(--b);cursor:pointer}
#msg{margin-top:12px;font-weight:600}
</style></head><body><main>
<h1>AtmoVerse</h1>
<p class="m">Configurazione iniziale. Dopo il salvataggio AtmoVerse si collega alla rete e scarica da solo il resto del software.</p>
<form id="f" class="card" novalidate>
<label for="ssid">Rete WiFi di casa</label>
<input id="ssid" autocomplete="off" autocapitalize="off" spellcheck="false" placeholder="Nome della rete">
<button type="button" class="sec" id="scan">Cerca reti</button>
<div id="nets"></div>
<label for="pwd">Password WiFi</label>
<input id="pwd" type="password" autocomplete="off" autocapitalize="off" placeholder="Password della rete">
<label for="city">Città</label>
<input id="city" placeholder="Es. Roma">
<label for="key">API key OpenWeatherMap</label>
<input id="key" autocomplete="off" autocapitalize="off" spellcheck="false" placeholder="Gratuita su openweathermap.org">
<button type="submit" id="save">Salva e collega</button>
<div id="msg" role="status"></div>
</form>
</main><script>
var $=function(i){return document.getElementById(i)};
fetch('/api/settings').then(function(r){return r.json()}).then(function(s){
 if(s.ssid)$('ssid').value=s.ssid;if(s.city)$('city').value=s.city;
 if(s.api_key_set)$('key').placeholder='Già configurata, lascia vuoto per mantenerla';
}).catch(function(){});
$('scan').onclick=function(){
 var b=$('scan');b.disabled=true;b.textContent='Ricerca…';
 fetch('/api/wifi-scan').then(function(r){return r.json()}).then(function(d){
  var box=$('nets');box.innerHTML='';
  (d.networks||[]).sort(function(a,b){return b.rssi-a.rssi}).forEach(function(n){
   var e=document.createElement('div');e.className='net';
   var a=document.createElement('span');a.textContent=n.ssid;
   var c=document.createElement('span');c.className='m';c.textContent=((n.isSecure||n.encryption==='secured')?'🔒 ':'')+n.rssi+' dBm';
   e.appendChild(a);e.appendChild(c);
   e.onclick=function(){$('ssid').value=n.ssid;box.innerHTML='';$('pwd').focus()};
   box.appendChild(e);
  });
 }).catch(function(){$('nets').textContent='Ricerca non riuscita'}).then(function(){b.disabled=false;b.textContent='Cerca reti'});
};
$('f').onsubmit=function(ev){
 ev.preventDefault();
 var ssid=$('ssid').value.trim();
 if(!ssid){$('msg').textContent='Inserisci il nome della rete WiFi.';return}
 $('save').disabled=true;$('msg').textContent='Salvataggio…';
 fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},
  body:JSON.stringify({ssid:ssid,password:$('pwd').value,city:$('city').value.trim(),api_key:$('key').value.trim()})})
 .then(function(r){return r.json()}).then(function(res){
  if(res.success===false)throw new Error(res.message);
  $('f').innerHTML='<h2>Impostazioni salvate ✅</h2><p>AtmoVerse si riavvia e si collega a “'+ssid.replace(/[<>&"]/g,'')+'”. Ricollega il telefono alla rete di casa. Se è disponibile un aggiornamento, il display mostrerà il download e il dispositivo si riavvierà da solo.</p>';
 }).catch(function(e){$('msg').textContent=e.message||'Errore nel salvataggio, riprova.';$('save').disabled=false});
};
</script></body></html>)rawliteral";

void sendFallbackSetupPage(WiFiClient& client) {
  size_t len = strlen_P(FALLBACK_SETUP_PAGE);
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html; charset=utf-8"));
  client.print(F("Content-Length: "));
  client.println(len);
  client.println(F("Cache-Control: no-store"));
  client.println(F("Connection: close"));
  client.println();
  client.write((const uint8_t*)FALLBACK_SETUP_PAGE, len);
  client.stop();
}
