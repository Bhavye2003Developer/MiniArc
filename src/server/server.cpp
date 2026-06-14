#include "server/server.h"
#include "thermal/thermal.h"
#include "httplib.h"
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <iostream>

// ── Embedded frontend ────────────────────────────────────────────────────────

static const char* HTML = R"html(<!DOCTYPE html>
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
html,body{height:100%;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:var(--bg);color:var(--tx);font-size:15px;line-height:1.6;-webkit-font-smoothing:antialiased}

/* ── Layout ── */
.app{display:flex;flex-direction:column;height:100vh;max-width:820px;margin:0 auto}

/* ── Header ── */
header{display:flex;align-items:center;gap:10px;padding:10px 18px;border-bottom:1px solid var(--br);background:rgba(8,8,15,.92);backdrop-filter:blur(14px);-webkit-backdrop-filter:blur(14px);position:sticky;top:0;z-index:10;flex-shrink:0}
.logo{display:flex;align-items:center;gap:7px;font-weight:600;font-size:15px;letter-spacing:-.3px;white-space:nowrap;color:var(--tx)}
.logo-icon{color:var(--ac)}
.pills{display:flex;gap:6px;flex:1;overflow:hidden}
.pill{display:flex;align-items:center;gap:5px;font-size:11.5px;font-weight:500;padding:3px 10px;border-radius:999px;background:var(--s2);border:1px solid var(--br);color:var(--t2);white-space:nowrap}
.dot{width:6px;height:6px;border-radius:50%;background:var(--cool);transition:background .4s}
.dot.warm{background:var(--warm)}.dot.hot{background:var(--hot)}.dot.crit{background:var(--crit);animation:pulse 1s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.25}}
.clr{font-size:12px;font-weight:500;padding:5px 13px;border-radius:7px;background:0;border:1px solid var(--br);color:var(--t2);cursor:pointer;transition:all .15s;white-space:nowrap}
.clr:hover{background:var(--s2);color:var(--tx);border-color:var(--ac)}

/* ── Chat area ── */
.chat{flex:1;overflow-y:auto;padding:22px 18px;display:flex;flex-direction:column;gap:18px;scroll-behavior:smooth}
.chat::-webkit-scrollbar{width:4px}
.chat::-webkit-scrollbar-thumb{background:var(--br);border-radius:4px}
.chat{scrollbar-width:thin;scrollbar-color:var(--br) transparent}

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
.irow{display:flex;align-items:flex-end;gap:8px;background:var(--s1);border:1px solid var(--br);border-radius:14px;padding:9px 10px 9px 15px;transition:border-color .15s,box-shadow .15s}
.irow:focus-within{border-color:var(--ac);box-shadow:0 0 0 3px var(--ac)18}
textarea{flex:1;background:0;border:0;outline:0;resize:none;font-family:inherit;font-size:14px;color:var(--tx);line-height:1.6;max-height:140px;overflow-y:auto}
textarea::placeholder{color:var(--t2)}
textarea::-webkit-scrollbar{width:3px}
textarea::-webkit-scrollbar-thumb{background:var(--br);border-radius:3px}
.sbtn{width:35px;height:35px;border-radius:10px;background:var(--ac);border:0;cursor:pointer;display:flex;align-items:center;justify-content:center;color:#fff;transition:all .15s;flex-shrink:0;box-shadow:0 2px 8px var(--ac)44}
.sbtn:hover:not(:disabled){background:var(--ac2);transform:scale(1.07)}
.sbtn:active:not(:disabled){transform:scale(.96)}
.sbtn:disabled{opacity:.3;cursor:not-allowed;transform:none;box-shadow:none}
.hint{font-size:11px;color:var(--t2);text-align:center;margin-top:7px;opacity:.5}

/* ── Loading dots (while AI is generating) ── */
.thinking{display:flex;gap:4px;align-items:center;padding:12px 16px}
.thinking span{width:6px;height:6px;border-radius:50%;background:var(--t2);animation:dots .9s infinite}
.thinking span:nth-child(2){animation-delay:.15s}
.thinking span:nth-child(3){animation-delay:.3s}
@keyframes dots{0%,80%,100%{transform:scale(.7);opacity:.4}40%{transform:scale(1);opacity:1}}

@media(max-width:580px){.msg{max-width:95%}.pills{display:none}}
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
<script>
(function(){
const chat=document.getElementById('chat');
const inp=document.getElementById('inp');
const sBtn=document.getElementById('sBtn');
const clrBtn=document.getElementById('clrBtn');

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

function setLocked(v){
  sBtn.disabled=v;
  inp.disabled=v;
}

async function send(){
  const txt=inp.value.trim();
  if(!txt||sBtn.disabled)return;

  const t=ts();
  addMsg('user',t).bub.textContent=txt;
  inp.value='';
  inp.style.height='auto';
  setLocked(true);

  const at=ts();
  const{wrap,bub}=addMsg('ai',at);

  // Show thinking dots while waiting for first token
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
        if(firstToken){
          firstToken=false;
          bub.innerHTML='';   // remove thinking dots
          bub.classList.add('cursor');
        }
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
  // Fetch fresh tps after generation
  let tpsStr='';
  try{
    const s=await(await fetch('/api/status')).json();
    tpsStr=' \xb7 '+s.tps.toFixed(1)+' tok/s';
  }catch{}
  const rt=document.createElement('div');
  rt.className='rt';
  rt.textContent=el+'s'+tpsStr;
  wrap.appendChild(rt);

  setLocked(false);
  inp.focus();
}

// Auto-resize textarea
inp.addEventListener('input',()=>{
  inp.style.height='auto';
  inp.style.height=Math.min(inp.scrollHeight,140)+'px';
});

inp.addEventListener('keydown',e=>{
  if(e.key==='Enter'&&!e.shiftKey){e.preventDefault();send();}
});
sBtn.addEventListener('click',send);

clrBtn.addEventListener('click',async()=>{
  try{await fetch('/api/clear',{method:'POST'});}catch{}
  chat.innerHTML='';
  const w=document.createElement('div');
  w.className='welcome'; w.id='welcome';
  w.innerHTML='<div class="w-icon">&#9889;</div><h2>miniARC</h2><p>On-device AI. Fully offline. Thermal-aware.<br>Ask me anything.</p>';
  chat.appendChild(w);
});

// Status polling
async function pollStatus(){
  try{
    const r=await fetch('/api/status');
    const s=await r.json();
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

// Extract a string field value from a minimal JSON body like {"key":"value"}
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

// ── WebServer ────────────────────────────────────────────────────────────────

WebServer::WebServer(Engine& engine, Scheduler& scheduler, ThermalMonitor& thermal)
    : m_engine(engine), m_scheduler(scheduler), m_thermal(thermal) {}

void WebServer::run(int port) {
    httplib::Server svr;

    // ── Frontend ─────────────────────────────────────────────────────────────
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(HTML, "text/html; charset=utf-8");
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

    // ── Clear history ────────────────────────────────────────────────────────
    svr.Post("/api/clear", [this](const httplib::Request&, httplib::Response& res) {
        m_engine.clear_history();
        res.set_content("{\"ok\":true}", "application/json");
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
