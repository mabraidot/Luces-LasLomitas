/*
  Page.h - La pagina web, como cadena en PROGMEM.

  Un solo archivo, sin recursos externos, pensada para el celular. Se sirve con
  send_P para no armar un String gigante en el heap.

  NO se cachea: el uso es esporadico -- pueden pasar semanas sin que nadie la
  abra mientras las luces funcionan solas --, asi que cualquier max-age vence
  antes del proximo uso. Lo unico que lograria es mostrar una version vieja
  despues de reprogramar la placa. El icono y el manifiesto si se cachean.

  El bloque de diagnostico existe porque en la instalacion no hay monitor
  serie: la placa va enchufada a la pared. El motivo del ultimo reinicio es el
  dato clave para detectar brownouts provocados por la bobina del rele.
*/

#ifndef PAGE_H
#define PAGE_H

#include <Arduino.h>

static const char PAGE_HTML[] PROGMEM = R"PAGE(<!DOCTYPE html>
<html lang="es"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Las Lomitas</title>
<meta name="theme-color" content="#0d1117">
<link rel="manifest" href="/m.webmanifest">
<link rel="apple-touch-icon" href="/i.png">
<link rel="icon" type="image/png" sizes="512x512" href="/i.png">
<style>
:root{--bg:#0d1117;--pan:#161b22;--ln:#232a33;--tx:#c9d1d9;--mu:#8b949e;--am:#ffb84d;--bad:#f8776d}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--tx);
 font:15px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
 display:flex;flex-direction:column;align-items:center;padding:22px 16px 40px}
h1{font-size:26px;font-weight:700;letter-spacing:.07em;margin:0;color:var(--tx)}
.tag{font-size:11px;font-weight:500;letter-spacing:.2em;text-transform:uppercase;
 color:var(--mu);margin:3px 0 0}
.wrap{width:100%;max-width:420px}
.mid{text-align:center}
#bulb{cursor:pointer;-webkit-tap-highlight-color:transparent;user-select:none;
 margin:14px 0 2px;width:150px;height:auto;touch-action:manipulation}
/* Sin recuadro al tocar: el foco es un boton por rol, y el navegador le pone
   el anillo de foco tambien con el dedo. Con :focus-visible el anillo queda
   solo para quien navega con teclado, que es a quien le sirve. */
#bulb:focus{outline:none}
#bulb:focus-visible{outline:2px solid var(--am);outline-offset:8px;border-radius:14px}
.glass{fill:#2b323c;transition:fill .18s}
.fil{fill:none;stroke:#4a525e;stroke-width:4;stroke-linecap:round;transition:stroke .18s}
.cap{fill:#464f5b}
#bulb.on .glass{fill:var(--am)}
#bulb.on .fil{stroke:#fff6e0}
#bulb.on{filter:drop-shadow(0 0 12px rgba(255,184,77,.85)) drop-shadow(0 0 38px rgba(255,160,40,.45))}
#st{font-size:20px;font-weight:600;margin:6px 0 2px}
.hint{font-size:12px;color:var(--mu);margin:0 0 20px}
.card{background:var(--pan);border:1px solid var(--ln);border-radius:12px;padding:4px 14px;margin-bottom:12px}
.row,.frow{display:flex;justify-content:space-between;gap:12px;
 border-bottom:1px solid var(--ln)}
.row{align-items:baseline;padding:10px 0}
.frow{align-items:center;padding:9px 0}
.row:last-child,.frow:last-child{border-bottom:0}
.k{color:var(--mu);font-size:13px;flex:0 0 auto}
.v{text-align:right;font-variant-numeric:tabular-nums;word-break:break-word}
.bad{color:var(--bad)}
.sub{display:block;font-size:11px;color:var(--mu);font-variant-numeric:tabular-nums}
label{color:var(--mu);font-size:13px}
input{background:#0b0f14;color:var(--tx);border:1px solid var(--ln);border-radius:7px;
 padding:7px 8px;font:14px inherit;width:92px;text-align:right;
 font-variant-numeric:tabular-nums}
input[type=time]{width:112px}
button{background:var(--pan);color:var(--tx);border:1px solid var(--ln);border-radius:8px;
 padding:10px 12px;font:13px inherit;cursor:pointer}
button.pri{background:var(--am);color:#1a1205;border-color:var(--am);font-weight:600}
button.mini{padding:6px 9px;font-size:12px;margin-left:6px}
.btns{display:flex;gap:8px;margin:4px 0}
.btns button{flex:1}
.note{font-size:11px;color:var(--mu);margin:8px 2px 14px;line-height:1.5}
.note.ok{color:var(--am)}
.note.err{color:var(--bad)}
/* Fila de pestanas: dos botones al ancho de la tarjeta. Antes eran <summary>
   de un <details> y con el dedo costaba acertarles. */
.tabs{margin:0 0 12px}
.tabs button{padding:12px 10px;font-size:14px}
.tabs button[aria-expanded=true]{border-color:var(--am);color:var(--am)}
#log{background:#010409;border:1px solid var(--ln);border-radius:8px;margin:10px 0;
 padding:10px;font:11px/1.45 ui-monospace,Menlo,Consolas,monospace;color:#9fb0c0;
 white-space:pre-wrap;overflow-x:auto;max-height:230px;overflow-y:auto}
</style></head><body>

<div class="wrap mid">
<h1>LAS LOMITAS</h1>
<p class="tag">Luces exteriores</p>

<svg id="bulb" viewBox="0 0 120 176" aria-label="Interruptor de luz" role="button" tabindex="0">
 <circle class="glass" cx="60" cy="50" r="36"/>
 <path class="glass" d="M40 78Q44 92 44 96h32q0-4 4-18z"/>
 <path class="fil" d="M50 62 54 44Q60 36 66 44l4 18"/>
 <rect class="cap" x="43" y="98" width="34" height="9" rx="3"/>
 <rect class="cap" x="43" y="110" width="34" height="9" rx="3"/>
 <path class="cap" d="M47 122h26l-3 13a10 10 0 0 1-20 0z"/>
</svg>
<div id="st">--</div>
<p class="hint">Tocá el foco para cambiar</p>
</div>

<div class="wrap">
 <div class="card">
  <div class="row"><span class="k">Fecha y hora</span><span class="v" id="dt">--</span></div>
  <div class="row"><span class="k">Nivel de luz</span><span class="v" id="lvl">--<span class="sub" id="raw"></span></span></div>
  <div class="row"><span class="k">Modo</span><span class="v" id="mode">--<span class="sub" id="arm"></span></span></div>
  <div class="row"><span class="k">Último cambio</span><span class="v" id="why">--</span></div>
 </div>

 <div class="btns tabs">
  <button id="bcf" aria-expanded="false" aria-controls="cf">Ajustes</button>
  <button id="bdg" aria-expanded="false" aria-controls="dg">Diagnóstico</button>
 </div>

 <div id="cf" hidden>
  <div class="card">
   <div class="frow"><label for="th">Umbral de luz<span class="sub" id="thnow"></span></label><input id="th" type="number" min="0" max="100"></div>
   <div class="frow"><label for="hy">Histéresis de rearme</label><input id="hy" type="number" min="0" max="50"></div>
   <div class="frow"><label for="bt">Hora de dormir</label><input id="bt" type="time"></div>
   <div class="frow"><label for="w0">Ventana segura desde</label><input id="w0" type="time"></div>
   <div class="frow"><label for="w1">Ventana segura hasta</label><input id="w1" type="time"></div>
   <div class="frow"><label for="mx">Máximo encendido (horas)</label><input id="mx" type="number" min="0" max="24"></div>
  </div>
  <p class="note">Las luces se encienden al bajar del umbral, solo entre el inicio de la ventana y la hora de dormir. Al final de la ventana se apagan siempre.</p>
  <p class="note" id="hywarn"></p>

  <div class="card">
   <div class="frow"><label for="rn">Crudo de noche</label><span><input id="rn" type="number" min="0" max="1023"><button class="mini" id="usern">actual</button></span></div>
   <div class="frow"><label for="rd">Crudo de día</label><span><input id="rd" type="number" min="0" max="1023"><button class="mini" id="userd">actual</button></span></div>
  </div>
  <p class="note">El crudo de noche se toma con la lámpara apagada. El de día, al mediodía y con el sensor ya montado en su lugar definitivo.</p>

  <div class="btns"><button class="pri" id="save">Guardar</button><button id="sett">Poner en hora</button></div>
  <p class="note" id="msg"></p>
 </div>

 <div id="dg" hidden>
  <div class="card">
   <div class="row"><span class="k">Último reinicio</span><span class="v" id="rst">--</span></div>
   <div class="row"><span class="k">Encendida hace</span><span class="v" id="up">--</span></div>
   <div class="row"><span class="k">Sincronizó hace</span><span class="v" id="ntp">--</span></div>
   <div class="row"><span class="k">RTC contra NTP</span><span class="v" id="ntpd">--</span></div>
   <div class="row"><span class="k">Llave</span><span class="v" id="sw">--</span></div>
   <div class="row"><span class="k">WiFi</span><span class="v" id="wifi">--</span></div>
   <div class="row"><span class="k">Aporte de la lámpara</span><span class="v" id="lb">--</span></div>
   <div class="row"><span class="k">Apagado al aclarar</span><span class="v" id="cs">--</span></div>
   <div class="row"><span class="k">Memoria libre</span><span class="v" id="heap">--</span></div>
  </div>
  <div id="log">--</div>
 </div>
</div>

<script>
var $=function(i){return document.getElementById(i)},on=false,timer=null,logT=null;
var last=null,cfgOpen=false;

function paint(){
 $('bulb').classList.toggle('on',on);
 $('st').textContent=on?'Encendidas':'Apagadas';
 $('st').style.color=on?'var(--am)':'';
}
function pad(n){return (n<10?'0':'')+n}
function hhmm(m){return pad(Math.floor(m/60))+':'+pad(m%60)}
function mins(v){var p=(v||'0:0').split(':');return (+p[0])*60+(+p[1])}
function dur(s){
 var d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);
 if(d)return d+'d '+h+'h';
 if(h)return h+'h '+m+'m';
 return m+'m '+(s%60)+'s';
}
function msg(t,cls){$('msg').textContent=t;$('msg').className='note'+(cls?' '+cls:'')}

/* El formulario se llena solo al abrirlo, nunca mientras esta abierto: si se
   refrescara con cada sondeo, pisaria lo que se esta tipeando. */
function fill(d){
 $('th').value=d.th; $('hy').value=d.hy;
 $('bt').value=hhmm(d.bt); $('w0').value=hhmm(d.w0); $('w1').value=hhmm(d.w1);
 $('mx').value=Math.round(d.mx/60);
 $('rn').value=d.rn; $('rd').value=d.rd;
}

function show(d){
 last=d;
 on=!!d.on; paint();
 $('lvl').firstChild.textContent=d.rdy?(d.l+' / 100'):'midiendo…';
 /* Las cuentas de confirmacion. Sin mostrarlas, los 30 segundos del atardecer
    o los 10 minutos de "aclaro" se ven igual que un aparato roto. */
 var extra='';
 if(d.cd>=0)extra=' · oscureciendo, '+d.cd+' s';
 else if(d.cd===-2)extra=' · bajo el umbral, sin disparar';
 else if(d.cu>=0)extra=' · aclarando, '+(d.cu>=60?Math.ceil(d.cu/60)+' min':d.cu+' s');
 else if(d.cu===-2)extra=' · apagado al aclarar desactivado';
 $('raw').textContent='crudo '+d.r+extra;
 $('dt').textContent=d.t;
 $('dt').className=d.rtc?'v':'v bad';
 $('mode').firstChild.textContent=d.mode;
 $('mode').className=d.rtc?'v':'v bad';
 $('arm').textContent=d.arm?'disparador armado':'disparador desarmado';
 $('why').textContent=d.why;
 $('thnow').textContent='ahora '+d.l;
 /* La histeresis tiene dos usos: rearmar el disparador y habilitar el apagado
    por "aclaro". Para el segundo tiene que superar lo que la lampara le suma
    al sensor, o el apagado realimentaria. */
 /* El aviso sale de la COMPROBACION, no de la medición: el aparato apagó por
    exceso de luz y verificó que el nivel caía al hacerlo, o sea que lo que lo
    tenía alto era su propia lámpara. */
 $('hywarn').textContent=d.cs
  ?('Al apagar por exceso de luz, el nivel cayó: lo que lo tenía alto era la '
    +'propia lámpara. Con una histéresis de '+d.hy+' las luces no se pueden '
    +'apagar al aclarar, así que quedó desactivado. Subila (la lámpara suma '
    +'unos '+d.lb+' puntos) o ponele una visera al sensor. Guardar vuelve a '
    +'intentarlo.')
  :'';
 $('hywarn').className=d.cs?'note err':'note';
 $('ntp').textContent=d.ntp<0?'nunca':dur(d.ntp);
 /* Con signo: el RTC atrasa cuando NTP va adelante. La serie de estos valores
    cada 6 h es lo que da la deriva del cristal en segundos por dia. */
 $('ntpd').textContent=d.ntpd===null?'--'
  :(d.ntpd?(d.ntpd>0?'atrasa ':'adelanta ')+Math.abs(d.ntpd)+' s':'en hora');
 $('sw').textContent=d.sw?'cerrada':'abierta';
 $('wifi').textContent=d.ip+' · '+d.db+' dBm';
 $('rst').textContent=d.rst;
 $('up').textContent=dur(d.up);
 $('heap').textContent=d.heap+' B';
 /* Informativo: cuanto se ilumina el sensor a si mismo con la lampara. Un
    valor alto sugiere ponerle una visera. Ya no bloquea nada: la garantía
    contra el bucle es la comprobación al apagar, no este número. */
 $('lb').textContent=d.lb+' puntos';
 $('lb').className='v';
 $('cs').textContent=d.cs?'desactivado':'activo';
 $('cs').className=d.cs?'v bad':'v';
 if(!cfgOpen)fill(d);
}

function tick(){
 fetch('/s',{cache:'no-store'}).then(function(r){return r.json()})
  .then(show).catch(function(){}).then(plan);
}
/* setTimeout encadenado, no setInterval: si un pedido tarda, con setInterval
   los siguientes se apilan y saturan la ESP. */
function plan(){clearTimeout(timer);if(!document.hidden)timer=setTimeout(tick,2000)}

function tickLog(){
 if($('dg').hidden||document.hidden){logT=setTimeout(tickLog,2000);return}
 fetch('/l',{cache:'no-store'}).then(function(r){return r.json()}).then(function(d){
  $('log').textContent=d.lines.join('\n');
 }).catch(function(){}).then(function(){logT=setTimeout(tickLog,5000)});
}

function toggle(){
 on=!on; paint();                    /* optimista: se siente inmediato */
 fetch('/t',{method:'POST'}).then(function(r){return r.json()})
  .then(show).catch(function(){});
}
$('bulb').addEventListener('click',toggle);
$('bulb').addEventListener('keydown',function(e){
 if(e.key==='Enter'||e.key===' '){e.preventDefault();toggle()}
});

/* Una sola seccion abierta a la vez: abrir una cierra la otra. */
function panel(id){
 var abrir=$(id).hidden;
 $('cf').hidden=$('dg').hidden=true;
 $('bcf').setAttribute('aria-expanded','false');
 $('bdg').setAttribute('aria-expanded','false');
 if(abrir){
  $(id).hidden=false;
  $('b'+id).setAttribute('aria-expanded','true');
 }
 cfgOpen=!$('cf').hidden;
 if(cfgOpen&&last){fill(last);msg('')}
}
$('bcf').addEventListener('click',function(){panel('cf')});
$('bdg').addEventListener('click',function(){panel('dg')});

$('usern').addEventListener('click',function(){if(last)$('rn').value=last.r});
$('userd').addEventListener('click',function(){if(last)$('rd').value=last.r});

$('save').addEventListener('click',function(){
 var q='th='+(+$('th').value)+'&hy='+(+$('hy').value)
  +'&bt='+mins($('bt').value)+'&w0='+mins($('w0').value)+'&w1='+mins($('w1').value)
  +'&mx='+((+$('mx').value)*60)
  +'&rn='+(+$('rn').value)+'&rd='+(+$('rd').value);
 msg('Guardando…');
 fetch('/c?'+q,{method:'POST'}).then(function(r){return r.json()}).then(function(d){
  show(d); fill(d);              /* la placa puede haber recortado algun valor */
  msg('Guardado','ok');
 }).catch(function(){msg('No se pudo guardar','err')});
});

$('sett').addEventListener('click',function(){
 var n=new Date();
 /* Se mandan los campos ya en hora LOCAL. getTime() devuelve UTC, y
    convertirlo en el micro es arriesgar un error de tres horas. */
 var q='h='+n.getHours()+'&i='+n.getMinutes()+'&s='+n.getSeconds()
  +'&d='+n.getDate()+'&mo='+(n.getMonth()+1)+'&y='+n.getFullYear();
 msg('Poniendo en hora…');
 fetch('/h?'+q,{method:'POST'}).then(function(r){
  if(!r.ok)throw 0; return r.json();
 }).then(function(d){show(d);msg('Reloj puesto en hora','ok')})
  .catch(function(){msg('El RTC no responde','err')});
});

/* Pausar el sondeo con la pestaña oculta: un celular con la pagina en segundo
   plano no tiene por que seguir golpeando la placa. */
document.addEventListener('visibilitychange',function(){if(!document.hidden)tick()});
tick(); tickLog();
</script>
</body></html>)PAGE";

#endif // PAGE_H
