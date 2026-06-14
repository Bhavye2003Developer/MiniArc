#include "server/server.h"
#include "thermal/thermal.h"
#include "httplib.h"
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

// ── Embedded frontend ────────────────────────────────────────────────────────

static const char HTML_1[] = R"html(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>miniARC</title>
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
:root{
  --bg:#08080f;--s1:#0d0d1a;--s2:#12122a;--br:#1c1c38;
  --ac:#7c3aed;--ac2:#6d28d9;
  --tx:#e0e0f4;--t2:#7070a0;
  --ub:#1a1740;--ubb:#312e81;
  --cool:#22c55e;--warm:#f59e0b;--hot:#ef4444;--crit:#dc2626;
}
html,body{height:100%;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:var(--bg);color:var(--tx);font-size:15px;line-height:1.6;-webkit-font-smoothing:antialiased;overflow-x:hidden}

/* ── Layout ── */
.app{display:flex;flex-direction:column;height:100vh;max-width:820px;margin:0 auto}

/* ── Header ── */
header{display:flex;align-items:center;gap:10px;padding:10px 18px;border-bottom:1px solid var(--br);background:rgba(8,8,15,.92);backdrop-filter:blur(14px);-webkit-backdrop-filter:blur(14px);position:sticky;top:0;z-index:10;flex-shrink:0}
.logo{display:flex;align-items:center;gap:7px;font-weight:600;font-size:15px;letter-spacing:-.3px;white-space:nowrap;color:var(--tx)}
.logo-icon{color:var(--ac)}
.pills{display:flex;gap:6px;flex:1;overflow:hidden;min-width:0}
.pill{display:flex;align-items:center;gap:5px;font-size:11.5px;font-weight:500;padding:3px 10px;border-radius:999px;background:var(--s2);border:1px solid var(--br);color:var(--t2);white-space:nowrap}
.dot{width:6px;height:6px;border-radius:50%;background:var(--cool);transition:background .4s}
.dot.warm{background:var(--warm)}.dot.hot{background:var(--hot)}.dot.crit{background:var(--crit);animation:pulse 1s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.25}}
.clr{font-size:12px;font-weight:500;padding:5px 13px;border-radius:7px;background:0;border:1px solid var(--br);color:var(--t2);cursor:pointer;transition:all .15s;white-space:nowrap}
.clr:hover{background:var(--s2);color:var(--tx);border-color:var(--ac)}

/* ── Chat area ── */
.chat{flex:1;overflow-y:auto;padding:22px 18px;display:flex;flex-direction:column;gap:18px;scroll-behavior:smooth}
.chat::-webkit-scrollbar{display:none}
.chat{scrollbar-width:none}

/* ── Welcome screen ── */
.welcome{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:14px;margin:auto;text-align:center;padding:48px 20px}
.w-icon{width:54px;height:54px;border-radius:15px;background:linear-gradient(135deg,var(--ac)22,var(--s2));border:1px solid var(--br);display:flex;align-items:center;justify-content:center;font-size:26px;box-shadow:0 0 30px var(--ac)18}
.welcome h2{font-size:19px;font-weight:600;letter-spacing:-.3px}
.welcome p{font-size:13px;color:var(--t2);max-width:270px;line-height:1.55}

/* ── Messages ── */
.msg{display:flex;flex-direction:column;gap:4px;max-width:80%;animation:fadein .22s ease}
@keyframes fadein{from{opacity:0;transform:translateY(7px)}to{opacity:1;transform:none}}
.msg.user{align-self:flex-end;align-items:flex-end}
.msg.ai{align-self:flex-start}
.meta{font-size:11px;color:var(--t2);padding:0 4px;display:flex;align-items:center;gap:6px}
.bubble{padding:11px 15px;border-radius:15px;font-size:14px;line-height:1.68;word-break:break-word;white-space:pre-wrap}
.msg.user .bubble{background:var(--ub);border:1px solid var(--ubb);border-bottom-right-radius:3px;color:#c7d2fe}
.msg.ai .bubble{background:var(--s1);border:1px solid var(--br);border-bottom-left-radius:3px;color:var(--tx)}
.cursor::after{content:'▋';animation:blink .7s step-start infinite;color:var(--ac);font-size:12px;margin-left:1px}
@keyframes blink{0%,100%{opacity:1}50%{opacity:0}}
.rt{font-size:11px;color:var(--t2);padding:2px 4px}

/* ── Input area ── */
.footer{padding:12px 18px 18px;border-top:1px solid var(--br);background:rgba(8,8,15,.92);backdrop-filter:blur(14px);-webkit-backdrop-filter:blur(14px);flex-shrink:0}
.irow{display:flex;align-items:center;gap:8px;background:var(--s1);border:1px solid var(--br);border-radius:14px;padding:9px 10px 9px 15px;transition:border-color .15s,box-shadow .15s}
.irow:focus-within{border-color:var(--ac);box-shadow:0 0 0 3px var(--ac)18}
textarea{flex:1;background:transparent;border:0;outline:0;resize:none;font-family:inherit;font-size:14px;color:var(--tx);line-height:1.5;min-height:21px;max-height:140px;overflow-y:auto;display:block}
textarea::placeholder{color:var(--t2)}
textarea:disabled{opacity:.5;cursor:default}
textarea::-webkit-scrollbar{display:none}
textarea{scrollbar-width:none}
.sbtn{width:35px;height:35px;border-radius:10px;background:var(--ac);border:0;cursor:pointer;display:flex;align-items:center;justify-content:center;color:#fff;transition:all .15s;flex-shrink:0;box-shadow:0 2px 8px var(--ac)44}
.sbtn:hover:not(:disabled){background:var(--ac2);transform:scale(1.07)}
.sbtn:active:not(:disabled){transform:scale(.96)}
.sbtn:disabled{opacity:.3;cursor:not-allowed;transform:none;box-shadow:none}
.hint{font-size:11px;color:var(--t2);text-align:center;margin-top:7px;opacity:.5}

/* ── Loading dots ── */
.thinking{display:flex;gap:4px;align-items:center;padding:12px 16px}
.thinking span{width:6px;height:6px;border-radius:50%;background:var(--t2);animation:dots .9s infinite}
.thinking span:nth-child(2){animation-delay:.15s}
.thinking span:nth-child(3){animation-delay:.3s}
@keyframes dots{0%,80%,100%{transform:scale(.7);opacity:.4}40%{transform:scale(1);opacity:1}}

/* ── Config panel ── */
.overlay{position:fixed;inset:0;background:rgba(0,0,0,.55);z-index:20;opacity:0;pointer-events:none;transition:opacity .2s}
.overlay.open{opacity:1;pointer-events:all}
.cfg-panel{position:fixed;top:0;right:0;height:100%;width:290px;background:var(--s1);border-left:1px solid var(--br);z-index:21;transform:translateX(100%);transition:transform .25s cubic-bezier(.4,0,.2,1);padding:20px;display:flex;flex-direction:column;gap:16px;overflow-y:auto;scrollbar-width:none}
.cfg-panel::-webkit-scrollbar{display:none}
.cfg-panel.open{transform:none}
.cfg-hdr{display:flex;justify-content:space-between;align-items:center;padding-bottom:10px;border-bottom:1px solid var(--br)}
.cfg-hdr h3{font-size:11px;font-weight:600;letter-spacing:.1em;text-transform:uppercase;color:var(--t2)}
.cfg-close{background:transparent;border:0;cursor:pointer;color:var(--t2);font-size:17px;line-height:1;padding:2px 4px;transition:color .15s}
.cfg-close:hover{color:var(--tx)}
.cfg-row{display:flex;flex-direction:column;gap:8px}
.cfg-lbl{font-size:12px;font-weight:500;color:var(--t2);display:flex;justify-content:space-between;align-items:center}
.cfg-val{font-size:12px;color:var(--tx);font-weight:600;font-variant-numeric:tabular-nums;background:var(--s2);padding:2px 8px;border-radius:5px;border:1px solid var(--br);min-width:44px;text-align:center}
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:3px;border-radius:2px;background:var(--br);outline:none;cursor:pointer}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:13px;height:13px;border-radius:50%;background:var(--ac);cursor:pointer;transition:transform .1s,box-shadow .1s}
input[type=range]::-webkit-slider-thumb:hover{transform:scale(1.25);box-shadow:0 0 0 4px var(--ac)33}
input[type=range]::-moz-range-thumb{width:13px;height:13px;border-radius:50%;background:var(--ac);cursor:pointer;border:none}
.cfg-desc{font-size:11px;color:var(--t2);line-height:1.5;opacity:.75}
.cfg-divider{height:1px;background:var(--br);margin:2px 0}
.cfg-actions{position:sticky;bottom:0;background:var(--s1);padding-top:14px;padding-bottom:4px;border-top:1px solid var(--br);display:flex;gap:8px;flex-shrink:0;margin-top:auto}
.cfg-actions button{flex:1;padding:8px 0;border-radius:8px;font-size:12px;font-weight:500;cursor:pointer;transition:all .15s}
.cfg-reset{background:transparent;border:1px solid var(--br);color:var(--t2)}
.cfg-reset:hover{background:var(--s2);color:var(--tx);border-color:var(--ac)}
.cfg-apply{background:var(--ac);border:1px solid var(--ac);color:#fff}
.cfg-apply:hover{background:var(--ac2)}
.cfg-apply:active{transform:scale(.97)}

/* ── Model selector ── */
.cfg-sect-hdr{font-size:11px;font-weight:600;letter-spacing:.08em;text-transform:uppercase;color:var(--t2);margin-bottom:8px}
.cfg-current{font-size:11.5px;color:var(--tx);background:var(--s2);border:1px solid var(--br);border-radius:6px;padding:5px 10px;margin-bottom:8px;word-break:break-all;line-height:1.5}
.cfg-select{width:100%;background:var(--s2);border:1px solid var(--br);border-radius:8px;color:var(--tx);font-size:12px;padding:7px 10px;outline:none;cursor:pointer;transition:border-color .15s}
.cfg-select:focus{border-color:var(--ac)}
.cfg-select option{background:#12122a;color:var(--tx)}
.cfg-load{width:100%;margin-top:8px;padding:8px 0;border-radius:8px;font-size:12px;font-weight:500;cursor:pointer;background:transparent;border:1px solid var(--br);color:var(--tx);transition:all .15s}
.cfg-load:hover:not(:disabled){border-color:var(--ac);color:var(--ac)}
.cfg-load:disabled{opacity:.5;cursor:default}
@media(max-width:580px){.msg{max-width:95%}.pills{display:none}.cfg-panel{width:100%}}
</style>
</head>
<body>
<div class="app">

<header>
  <div class="logo">
    <svg class="logo-icon" width="16" height="16" viewBox="0 0 24 24" fill="currentColor">
      <path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z"/>
    </svg>
    miniARC
  </div>
  <div class="pills">
    <div class="pill"><div class="dot" id="tdot"></div><span id="tlbl">COOL</span></div>
    <div class="pill"><span id="rlbl">&#8212; MB RAM</span></div>
    <div class="pill"><span id="slbl">&#8212; tok/s</span></div>
  </div>
  <button class="clr" id="clrBtn">Clear</button>
  <button class="clr" id="cfgBtn" title="Model config">&#9881;</button>
</header>

<div class="chat" id="chat">
  <div class="welcome" id="welcome">
    <div class="w-icon">&#9889;</div>
    <h2>miniARC</h2>
    <p>On-device AI. Fully offline. Thermal-aware.<br>Ask me anything.</p>
  </div>
</div>

<div class="footer">
  <div class="irow">
    <textarea id="inp" placeholder="Ask anything&#8230;" rows="1"></textarea>
    <button class="sbtn" id="sBtn" title="Send (Enter)">
      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
        <line x1="22" y1="2" x2="11" y2="13"/>
        <polygon points="22 2 15 22 11 13 2 9 22 2"/>
      </svg>
    </button>
  </div>
  <p class="hint">Enter to send &nbsp;&middot;&nbsp; Shift+Enter for new line</p>
</div>

</div><!-- .app -->

<div class="overlay" id="overlay"></div>
<div class="cfg-panel" id="cfgPanel">
  <div class="cfg-hdr">
    <h3>Model Config</h3>
    <button class="cfg-close" id="cfgClose">&#10005;</button>
  </div>

  <div>
    <div class="cfg-sect-hdr">Base Model</div>
    <div class="cfg-current" id="modelCurrent">&#8212;</div>
    <select id="modelSel" class="cfg-select">
      <option value="">Scanning models/&#8230;</option>
    </select>
    <button class="cfg-load" id="modelLoadBtn">Load Model</button>
  </div>

  <div class="cfg-divider"></div>

  <div class="cfg-row">
    <div class="cfg-lbl">Temperature <span class="cfg-val" id="vTemp">0.70</span></div>
    <input type="range" id="sTemp" min="0" max="2" step="0.05" value="0.7">
    <div class="cfg-desc">Higher = more creative &amp; varied. Lower = focused &amp; deterministic.</div>
  </div>

  <div class="cfg-divider"></div>

  <div class="cfg-row">
    <div class="cfg-lbl">Top-K <span class="cfg-val" id="vTopK">40</span></div>
    <input type="range" id="sTopK" min="1" max="100" step="1" value="40">
    <div class="cfg-desc">Restrict sampling to the K most likely next tokens.</div>
  </div>

  <div class="cfg-divider"></div>

  <div class="cfg-row">
    <div class="cfg-lbl">Top-P <span class="cfg-val" id="vTopP">0.95</span></div>
    <input type="range" id="sTopP" min="0" max="1" step="0.01" value="0.95">
    <div class="cfg-desc">Nucleus sampling &mdash; cumulative probability threshold.</div>
  </div>

  <div class="cfg-divider"></div>

  <div class="cfg-row">
    <div class="cfg-lbl">Repeat Penalty <span class="cfg-val" id="vRep">1.15</span></div>
    <input type="range" id="sRep" min="1" max="2" step="0.01" value="1.15">
    <div class="cfg-desc">Penalize recently used tokens. 1.0 = off. Higher = less repetition.</div>
  </div>

  <div class="cfg-divider"></div>

  <div class="cfg-row">
    <div class="cfg-lbl">Max Input Tokens <span class="cfg-val" id="vMaxIn">1500</span></div>
    <input type="range" id="sMaxIn" min="128" max="1792" step="64" value="1500">
    <div class="cfg-desc">Max prompt length before oldest history turns are dropped.</div>
  </div>

  <div class="cfg-divider"></div>

  <div class="cfg-row">
    <div class="cfg-lbl">Max Output Tokens <span class="cfg-val" id="vMax">512</span></div>
    <input type="range" id="sMax" min="64" max="1024" step="32" value="512">
    <div class="cfg-desc">Maximum tokens to generate per response.</div>
  </div>

  <div class="cfg-actions">
    <button class="cfg-reset" id="cfgReset">Reset</button>
    <button class="cfg-apply" id="cfgApply">Apply</button>
  </div>
</div>
)html";

static const char HTML_2[] = R"html(<script>
(function(){
const chat=document.getElementById('chat');
const inp=document.getElementById('inp');
const sBtn=document.getElementById('sBtn');
const clrBtn=document.getElementById('clrBtn');
const cfgBtn=document.getElementById('cfgBtn');
const overlay=document.getElementById('overlay');
const cfgPanel=document.getElementById('cfgPanel');
const cfgClose=document.getElementById('cfgClose');
const cfgApply=document.getElementById('cfgApply');
const cfgReset=document.getElementById('cfgReset');

// ── Config panel ─────────────────────────────────────────────────────────────
const DEFAULTS={temperature:0.7,top_k:40,top_p:0.95,repeat_penalty:1.15,max_prompt_tokens:1500,max_new_tokens:512};
const sliders=[
  {id:'sTemp',vid:'vTemp',key:'temperature',     dec:2},
  {id:'sTopK',vid:'vTopK',key:'top_k',           dec:0},
  {id:'sTopP',vid:'vTopP',key:'top_p',           dec:2},
  {id:'sRep', vid:'vRep', key:'repeat_penalty',  dec:2},
  {id:'sMaxIn',vid:'vMaxIn',key:'max_prompt_tokens',dec:0},
  {id:'sMax', vid:'vMax', key:'max_new_tokens',  dec:0},
];

sliders.forEach(s=>{
  const el=document.getElementById(s.id);
  const vl=document.getElementById(s.vid);
  el.addEventListener('input',()=>{vl.textContent=Number(el.value).toFixed(s.dec);});
});

function openCfg(){cfgPanel.classList.add('open');overlay.classList.add('open');loadCfg();loadModels();}
function closeCfg(){cfgPanel.classList.remove('open');overlay.classList.remove('open');}

async function loadCfg(){
  try{
    const c=await(await fetch('/api/config')).json();
    sliders.forEach(s=>{
      const el=document.getElementById(s.id);
      document.getElementById(s.vid).textContent=Number(c[s.key]).toFixed(s.dec);
      el.value=c[s.key];
    });
  }catch{}
}

async function applyCfg(){
  const body={};
  sliders.forEach(s=>{
    const v=document.getElementById(s.id).value;
    body[s.key]=s.dec===0?parseInt(v,10):parseFloat(v);
  });
  try{await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});}catch{}
  closeCfg();
}

async function resetDefaults(){
  try{await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(DEFAULTS)});}catch{}
  sliders.forEach(s=>{
    document.getElementById(s.id).value=DEFAULTS[s.key];
    document.getElementById(s.vid).textContent=Number(DEFAULTS[s.key]).toFixed(s.dec);
  });
}

cfgBtn.addEventListener('click',openCfg);
cfgClose.addEventListener('click',closeCfg);
overlay.addEventListener('click',closeCfg);
cfgApply.addEventListener('click',applyCfg);
cfgReset.addEventListener('click',resetDefaults);

// ── Model switching ───────────────────────────────────────────────────────────
async function loadModels(){
  try{
    const d=await(await fetch('/api/models')).json();
    document.getElementById('modelCurrent').textContent=d.current;
    const sel=document.getElementById('modelSel');
    sel.innerHTML='';
    if(!d.models||!d.models.length){
      sel.innerHTML='<option value="">No .gguf files found in models/</option>';
      return;
    }
    d.models.forEach(m=>{
      const o=document.createElement('option');
      o.value=m.path;o.textContent=m.name;
      if(m.name===d.current)o.selected=true;
      sel.appendChild(o);
    });
  }catch{}
}

async function switchModel(){
  const path=document.getElementById('modelSel').value;
  if(!path)return;
  const btn=document.getElementById('modelLoadBtn');
  const cur=document.getElementById('modelCurrent');
  btn.disabled=true;btn.textContent='Loading…';
  cur.textContent='Switching model…';
  try{
    const r=await fetch('/api/model',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({path})});
    const d=await r.json();
    if(d.ok){cur.textContent=d.name;closeCfg();}
    else{cur.textContent='Error: '+(d.error||'failed');}
  }catch(e){cur.textContent='Error: '+e.message;}
  btn.disabled=false;btn.textContent='Load Model';
}
document.getElementById('modelLoadBtn').addEventListener('click',switchModel);

// ── Chat helpers ─────────────────────────────────────────────────────────────
function ts(){
  return new Date().toLocaleTimeString('en-US',{hour12:false,hour:'2-digit',minute:'2-digit',second:'2-digit'});
}

function removeWelcome(){
  const w=document.getElementById('welcome');
  if(w)w.remove();
}

function addMsg(role,time){
  removeWelcome();
  const wrap=document.createElement('div');
  wrap.className='msg '+role;
  const meta=document.createElement('div');
  meta.className='meta';
  meta.textContent=(role==='user'?'You':'miniARC')+' \xb7 '+time;
  const bub=document.createElement('div');
  bub.className='bubble';
  wrap.appendChild(meta);
  wrap.appendChild(bub);
  chat.appendChild(wrap);
  scrollBottom();
  return {wrap,bub};
}

function scrollBottom(){chat.scrollTop=chat.scrollHeight;}

function setLocked(v){sBtn.disabled=v;inp.disabled=v;}

async function send(){
  const txt=inp.value.trim();
  if(!txt||sBtn.disabled)return;

  const t=ts();
  addMsg('user',t).bub.textContent=txt;
  inp.value='';
  inp.style.height='';
  setLocked(true);

  const{wrap,bub}=addMsg('ai',ts());

  const dots=document.createElement('div');
  dots.className='thinking';
  dots.innerHTML='<span></span><span></span><span></span>';
  bub.appendChild(dots);

  const t0=performance.now();
  let firstToken=true;

  try{
    const resp=await fetch('/api/chat',{
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({message:txt})
    });
    if(!resp.ok)throw new Error('HTTP '+resp.status);

    const reader=resp.body.getReader();
    const dec=new TextDecoder();
    let buf='';

    while(true){
      const{done,value}=await reader.read();
      if(done)break;
      buf+=dec.decode(value,{stream:true});
      let nl;
      while((nl=buf.indexOf('\n\n'))!==-1){
        const line=buf.slice(0,nl).trim();
        buf=buf.slice(nl+2);
        if(!line.startsWith('data: '))continue;
        const data=line.slice(6);
        if(data==='[DONE]')break;
        let tok;
        try{tok=JSON.parse(data);}catch{continue;}
        if(firstToken){firstToken=false;bub.innerHTML='';bub.classList.add('cursor');}
        bub.textContent+=tok;
        scrollBottom();
      }
    }
  }catch(e){
    bub.innerHTML='';
    bub.textContent='Error: '+e.message;
  }

  bub.classList.remove('cursor');
  if(firstToken){bub.innerHTML='';bub.textContent='(no response)';}

  const el=((performance.now()-t0)/1000).toFixed(1);
  let tpsStr='';
  try{const s=await(await fetch('/api/status')).json();tpsStr=' \xb7 '+s.tps.toFixed(1)+' tok/s';}catch{}
  const rt=document.createElement('div');
  rt.className='rt';rt.textContent=el+'s'+tpsStr;
  wrap.appendChild(rt);

  setLocked(false);
  inp.focus();
}

inp.addEventListener('input',()=>{inp.style.height='0';inp.style.height=Math.min(inp.scrollHeight,140)+'px';});
inp.addEventListener('keydown',e=>{if(e.key==='Enter'&&!e.shiftKey){e.preventDefault();send();}});
sBtn.addEventListener('click',send);

clrBtn.addEventListener('click',async()=>{
  try{await fetch('/api/clear',{method:'POST'});}catch{}
  chat.innerHTML='';
  const w=document.createElement('div');
  w.className='welcome';w.id='welcome';
  w.innerHTML='<div class="w-icon">&#9889;</div><h2>miniARC</h2><p>On-device AI. Fully offline. Thermal-aware.<br>Ask me anything.</p>';
  chat.appendChild(w);
});

// ── Status polling ────────────────────────────────────────────────────────────
async function pollStatus(){
  try{
    const s=await(await fetch('/api/status')).json();
    const lc=s.thermal.toLowerCase()==='critical'?'crit':s.thermal.toLowerCase();
    document.getElementById('tdot').className='dot '+lc;
    document.getElementById('tlbl').textContent=s.thermal;
    document.getElementById('rlbl').textContent=s.ram_mb+' MB RAM';
    document.getElementById('slbl').textContent=s.tps.toFixed(1)+' tok/s';
  }catch{}
}
pollStatus();
setInterval(pollStatus,3000);
inp.focus();
})();
</script>
</body>
</html>
)html";

// ── Helpers ──────────────────────────────────────────────────────────────────

static const char* thermal_str(ThermalState s) {
    switch (s) {
        case ThermalState::COOL:     return "COOL";
        case ThermalState::WARM:     return "WARM";
        case ThermalState::HOT:      return "HOT";
        case ThermalState::CRITICAL: return "CRITICAL";
        default:                     return "COOL";
    }
}

std::string WebServer::json_str(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
    out += '"';
    return out;
}

// Extract a JSON string field value from a body like {"key":"value"}
static std::string json_get(const std::string& body, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t p = body.find(needle);
    if (p == std::string::npos) return "";
    p = body.find(':', p + needle.size());
    if (p == std::string::npos) return "";
    p = body.find('"', p + 1);
    if (p == std::string::npos) return "";
    ++p;
    std::string out;
    while (p < body.size() && body[p] != '"') {
        if (body[p] == '\\' && p + 1 < body.size()) {
            ++p;
            switch (body[p]) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += body[p];
            }
        } else {
            out += body[p];
        }
        ++p;
    }
    return out;
}

// Extract a JSON numeric field value (returns raw text, caller converts)
static std::string json_get_num(const std::string& body, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t p = body.find(needle);
    if (p == std::string::npos) return "";
    p = body.find(':', p + needle.size());
    if (p == std::string::npos) return "";
    ++p;
    while (p < body.size() && (body[p] == ' ' || body[p] == '\t')) ++p;
    std::string out;
    while (p < body.size() && body[p] != ',' && body[p] != '}' &&
           body[p] != ' ' && body[p] != '\n' && body[p] != '\r') {
        out += body[p++];
    }
    return out;
}

// ── WebServer ────────────────────────────────────────────────────────────────

WebServer::WebServer(Engine& engine, Scheduler& scheduler, ThermalMonitor& thermal)
    : m_engine(engine), m_scheduler(scheduler), m_thermal(thermal) {}

void WebServer::run(int port) {
    httplib::Server svr;

    // ── Frontend ─────────────────────────────────────────────────────────────
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        static const std::string html_full = std::string(HTML_1) + HTML_2;
        res.set_content(html_full, "text/html; charset=utf-8");
    });

    // ── Status ───────────────────────────────────────────────────────────────
    svr.Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
        ThermalState ts = m_thermal.current();
        auto p = m_scheduler.current_params();
        std::ostringstream j;
        j << std::fixed << std::setprecision(1);
        j << "{\"thermal\":\"" << thermal_str(ts) << "\""
          << ",\"threads\":"   << p.n_threads
          << ",\"batch\":"     << p.n_batch
          << ",\"paused\":"    << (p.paused ? "true" : "false")
          << ",\"tps\":"       << m_engine.last_tokens_per_sec()
          << ",\"ram_mb\":"    << m_engine.ram_usage_mb()
          << "}";
        res.set_content(j.str(), "application/json");
    });

    // ── Get config ───────────────────────────────────────────────────────────
    svr.Get("/api/config", [this](const httplib::Request&, httplib::Response& res) {
        ModelConfig cfg = m_engine.get_config();
        std::ostringstream j;
        j << std::fixed << std::setprecision(4);
        j << "{\"temperature\":"      << cfg.temperature
          << ",\"top_k\":"            << cfg.top_k
          << ",\"top_p\":"            << cfg.top_p
          << ",\"repeat_penalty\":"   << cfg.repeat_penalty
          << ",\"max_prompt_tokens\":" << cfg.max_prompt_tokens
          << ",\"max_new_tokens\":"   << cfg.max_new_tokens
          << "}";
        res.set_content(j.str(), "application/json");
    });

    // ── Set config ───────────────────────────────────────────────────────────
    svr.Post("/api/config", [this](const httplib::Request& req, httplib::Response& res) {
        ModelConfig cfg = m_engine.get_config();

        auto parse_f = [&](const std::string& k, float& v) {
            std::string s = json_get_num(req.body, k);
            if (!s.empty()) try { v = std::stof(s); } catch (...) {}
        };
        auto parse_i = [&](const std::string& k, int& v) {
            std::string s = json_get_num(req.body, k);
            if (!s.empty()) try { v = std::stoi(s); } catch (...) {}
        };

        parse_f("temperature",      cfg.temperature);
        parse_i("top_k",            cfg.top_k);
        parse_f("top_p",            cfg.top_p);
        parse_f("repeat_penalty",   cfg.repeat_penalty);
        parse_i("max_prompt_tokens", cfg.max_prompt_tokens);
        parse_i("max_new_tokens",   cfg.max_new_tokens);

        // Clamp to sane ranges
        if (cfg.temperature      < 0.0f) cfg.temperature      = 0.0f;
        if (cfg.temperature      > 2.0f) cfg.temperature      = 2.0f;
        if (cfg.top_k            < 1)    cfg.top_k            = 1;
        if (cfg.top_k            > 200)  cfg.top_k            = 200;
        if (cfg.top_p            < 0.0f) cfg.top_p            = 0.0f;
        if (cfg.top_p            > 1.0f) cfg.top_p            = 1.0f;
        if (cfg.repeat_penalty   < 1.0f) cfg.repeat_penalty   = 1.0f;
        if (cfg.repeat_penalty   > 2.0f) cfg.repeat_penalty   = 2.0f;
        if (cfg.max_prompt_tokens < 128)  cfg.max_prompt_tokens = 128;
        if (cfg.max_prompt_tokens > 1792) cfg.max_prompt_tokens = 1792;
        if (cfg.max_new_tokens   < 32)   cfg.max_new_tokens   = 32;
        if (cfg.max_new_tokens   > 2048) cfg.max_new_tokens   = 2048;

        {
            std::lock_guard<std::mutex> lk(m_gen_mutex);
            m_engine.set_config(cfg);
        }

        std::ostringstream j;
        j << std::fixed << std::setprecision(4);
        j << "{\"temperature\":"      << cfg.temperature
          << ",\"top_k\":"            << cfg.top_k
          << ",\"top_p\":"            << cfg.top_p
          << ",\"repeat_penalty\":"   << cfg.repeat_penalty
          << ",\"max_prompt_tokens\":" << cfg.max_prompt_tokens
          << ",\"max_new_tokens\":"   << cfg.max_new_tokens
          << "}";
        res.set_content(j.str(), "application/json");
    });

    // ── Clear history ────────────────────────────────────────────────────────
    svr.Post("/api/clear", [this](const httplib::Request&, httplib::Response& res) {
        m_engine.clear_history();
        res.set_content("{\"ok\":true}", "application/json");
    });

    // ── List available models ─────────────────────────────────────────────────
    svr.Get("/api/models", [this](const httplib::Request&, httplib::Response& res) {
        std::ostringstream j;
        j << "{\"current\":" << json_str(m_engine.current_model_name()) << ",\"models\":[";
        bool first = true;
        fs::path models_dir("models");
        if (fs::exists(models_dir) && fs::is_directory(models_dir)) {
            for (auto& entry : fs::directory_iterator(models_dir)) {
                if (entry.path().extension() == ".gguf") {
                    if (!first) j << ",";
                    first = false;
                    j << "{\"path\":" << json_str(entry.path().string())
                      << ",\"name\":"  << json_str(entry.path().filename().string()) << "}";
                }
            }
        }
        j << "]}";
        res.set_content(j.str(), "application/json");
    });

    // ── Switch model ──────────────────────────────────────────────────────────
    svr.Post("/api/model", [this](const httplib::Request& req, httplib::Response& res) {
        std::string path = json_get(req.body, "path");
        if (path.empty()) {
            res.status = 400;
            res.set_content("{\"ok\":false,\"error\":\"missing path\"}", "application/json");
            return;
        }
        bool ok;
        {
            std::lock_guard<std::mutex> lk(m_gen_mutex);
            ok = m_engine.swap_model(path);
        }
        if (ok) {
            std::ostringstream j;
            j << "{\"ok\":true,\"name\":" << json_str(m_engine.current_model_name()) << "}";
            res.set_content(j.str(), "application/json");
        } else {
            res.status = 500;
            res.set_content("{\"ok\":false,\"error\":\"failed to load model\"}", "application/json");
        }
    });

    // ── Chat (SSE streaming) ─────────────────────────────────────────────────
    svr.Post("/api/chat", [this](const httplib::Request& req, httplib::Response& res) {
        std::string message = json_get(req.body, "message");
        if (message.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"empty message\"}", "application/json");
            return;
        }

        res.set_header("Cache-Control",     "no-cache");
        res.set_header("X-Accel-Buffering", "no");

        res.set_chunked_content_provider("text/event-stream",
            [this, message](size_t /*offset*/, httplib::DataSink& sink) -> bool {
                {
                    std::lock_guard<std::mutex> lk(m_gen_mutex);
                    bool alive = true;
                    m_engine.generate(message, m_scheduler,
                        [&sink, &alive](const std::string& tok) {
                            if (!alive) return;
                            std::string ev = "data: " + WebServer::json_str(tok) + "\n\n";
                            alive = sink.write(ev.data(), ev.size());
                        });
                }
                std::string done = "data: [DONE]\n\n";
                sink.write(done.data(), done.size());
                sink.done();
                return true;
            }
        );
    });

    std::cout << "\n  miniARC web UI  →  http://localhost:" << port << "\n\n";
    if (!svr.listen("0.0.0.0", port)) {
        std::cerr << "miniARC: could not bind to port " << port
                  << " (already in use?). Try --port <N>\n";
    }
}
