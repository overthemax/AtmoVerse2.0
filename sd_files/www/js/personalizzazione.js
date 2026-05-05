(function(){
  'use strict';

  // Config pannello e griglia
  const PANEL_W = 800;
  const PANEL_H = 480;
  let GRID_COLS = 24; // configurabile da UI
  let ROW_H = 20;     // altezza riga in px (scelta conservativa per e‑ink)
  const MARGIN = 4;   // margine snap

  // Stato UI
  const stage = document.getElementById('stage');
  const canvas = document.getElementById('previewCanvas');
  const ctx = canvas.getContext('2d');
  const overlay = document.getElementById('overlay');
  const zoomEl = document.getElementById('zoom');
  const themeSel = document.getElementById('themeSel');
  const timeSel = document.getElementById('timeSel');
  const gridColsEl = document.getElementById('gridCols');
  const btnSave = document.getElementById('btnSave');
  const btnReset = document.getElementById('btnReset');
  const btnRefresh = document.getElementById('btnRefresh');

  // Layout di default (griglia)
  const defaultLayout = {
    version: 1,
    grid: { cols: 24, row_height: ROW_H, margin: MARGIN },
    items: [
      { id: 'city',   type: 'city',         x: 1,  y: 0,  w: 8,  h: 2, z:2, visible:true, align:'left',  font_size: 20 },
      { id: 'icon',   type: 'weather_icon', x: 8,  y: 2,  w: 8,  h: 8, z:1, visible:true },
      { id: 'temp',   type: 'temperature',  x: 17, y: 2,  w: 6,  h: 2, z:2, visible:true, align:'right', font_size: 24 },
      { id: 'quote',  type: 'quote',        x: 2,  y: 12, w: 20, h: 6, z:1, visible:true, align:'center', font_size: 16 },
      { id: 'clock',  type: 'clock',        x: 2,  y: 20, w: 8,  h: 3, z:2, visible:true, align:'left',  font_size: 18 },
      { id: 'battery',type: 'battery',      x: 20, y: 20, w: 3,  h: 2, z:2, visible:true },
      { id: 'footer', type: 'footer_bar',   x: 0,  y: 27, w: 24, h: 2, z:3, visible:true }
    ]
  };

  let layout = null;

  function colWidth(){ return Math.floor(PANEL_W / GRID_COLS); }
  function toPixels(item){
    return {
      x: Math.round(item.x * colWidth()),
      y: Math.round(item.y * ROW_H),
      w: Math.round(item.w * colWidth()),
      h: Math.round(item.h * ROW_H)
    };
  }

  // Mock dati preview (o reali semplificati)
  function getPreviewData(){
    return {
      city: window.config?.city || 'Milano',
      temp: '22°C',
      quote: 'La semplicità è la massima sofisticazione.',
      quoteAuthor: 'Leonardo da Vinci',
      weatherId: 800, // sereno
      lastUpdate: '13:30',
      ip: '192.168.1.10',
      battery: 82
    };
  }

  // Disegno canvas con palette e‑ink
  function drawPreview(){
    const data = getPreviewData();
    const theme = themeSel.value || 'default';
    const night = timeSel.value === 'night';

    // Sfondo
    ctx.fillStyle = theme === 'eink' ? '#FFFFFF' : '#FFFFFF';
    ctx.fillRect(0,0,PANEL_W,PANEL_H);

    // Griglia (leggera)
    ctx.strokeStyle = '#e6e6e6';
    ctx.lineWidth = 1;
    const cw = colWidth();
    for(let c=1;c<GRID_COLS;c++){
      const x = c * cw + 0.5;
      ctx.beginPath(); ctx.moveTo(x,0); ctx.lineTo(x,PANEL_H); ctx.stroke();
    }
    for(let y=ROW_H;y<PANEL_H;y+=ROW_H){
      ctx.beginPath(); ctx.moveTo(0,y+0.5); ctx.lineTo(PANEL_W,y+0.5); ctx.stroke();
    }

    // Palette testi
    const textColor = '#000000';
    ctx.fillStyle = textColor;
    ctx.strokeStyle = textColor;

    // Render widgets
    layout.items.forEach(it => {
      if (!it.visible) return;
      const r = toPixels(it);

      switch(it.type){
        case 'city': {
          ctx.font = `bold ${it.font_size||20}px sans-serif`;
          ctx.textAlign = it.align==='right'?'right':it.align==='center'?'center':'left';
          const tx = it.align==='right' ? (r.x + r.w) : it.align==='center' ? (r.x + r.w/2) : r.x;
          ctx.fillText(data.city, tx, r.y + Math.min(r.h, it.font_size||20));
          break;
        }
        case 'weather_icon': {
          // Placeholder icona: cerchio/sole o luna
          ctx.beginPath();
          const size = Math.min(r.w, r.h) - 8;
          const cx = r.x + r.w/2; const cy = r.y + r.h/2;
          ctx.arc(cx, cy, size/2, 0, Math.PI*2);
          ctx.stroke();
          if (night) {
            ctx.beginPath(); ctx.arc(cx+6, cy-6, size/4, 0, Math.PI*2); ctx.stroke();
          } else {
            for(let a=0;a<8;a++){
              const ang = a * Math.PI/4; const len = size/2 + 6;
              ctx.beginPath(); ctx.moveTo(cx+Math.cos(ang)*size/2, cy+Math.sin(ang)*size/2);
              ctx.lineTo(cx+Math.cos(ang)*len, cy+Math.sin(ang)*len); ctx.stroke();
            }
          }
          break;
        }
        case 'temperature': {
          ctx.font = `bold ${it.font_size||22}px sans-serif`;
          ctx.textAlign = it.align==='left'?'left':it.align==='center'?'center':'right';
          const tx = it.align==='left' ? r.x : it.align==='center' ? (r.x + r.w/2) : (r.x + r.w);
          ctx.fillText(data.temp, tx, r.y + Math.min(r.h, (it.font_size||22)));
          break;
        }
        case 'quote': {
          ctx.font = `${it.font_size||16}px serif`;
          ctx.textAlign = 'center';
          // Wrap semplice
          const maxW = r.w - 8;
          const words = `${data.quote} — ${data.quoteAuthor}`.split(' ');
          let line = ''; let y = r.y + (it.font_size||16);
          for(const word of words){
            const test = line ? line + ' ' + word : word;
            if (ctx.measureText(test).width > maxW){
              ctx.fillText(line, r.x + r.w/2, y); y += (it.font_size||16) + 4; line = word;
              if (y > r.y + r.h) break;
            } else line = test;
          }
          if (line && y <= r.y + r.h) ctx.fillText(line, r.x + r.w/2, y);
          break;
        }
        case 'clock': {
          ctx.font = `bold ${it.font_size||18}px monospace`;
          ctx.textAlign = 'left';
          ctx.fillText(`Ultimo aggiornamento: ${data.lastUpdate}`, r.x, r.y + (it.font_size||18));
          break;
        }
        case 'battery': {
          // Batteria minimale
          const bw = Math.min(r.w, 60); const bh = Math.min(r.h, 16);
          const bx = r.x; const by = r.y + 4;
          ctx.strokeRect(bx, by, bw, bh);
          ctx.fillRect(bx + bw, by + bh/4, 4, bh/2);
          const lvl = Math.max(0, Math.min(100, data.battery));
          const fill = Math.round((bw-4) * (lvl/100));
          ctx.fillRect(bx+2, by+2, fill, bh-4);
          break;
        }
        case 'footer_bar': {
          ctx.fillRect(r.x, r.y, r.w, r.h);
          ctx.fillStyle = '#FFFFFF';
          ctx.font = '12px sans-serif';
          ctx.textAlign = 'left';
          ctx.fillText(`IP: ${data.ip}`, r.x + 6, r.y + r.h - 6);
          ctx.textAlign = 'right';
          ctx.fillText(`${data.battery}%`, r.x + r.w - 6, r.y + r.h - 6);
          ctx.fillStyle = textColor;
          break;
        }
      }

      // Contorno area widget (aiuta posizionamento)
      ctx.strokeStyle = '#999';
      ctx.strokeRect(r.x, r.y, r.w, r.h);
      ctx.strokeStyle = textColor;
    });
  }

  // Overlay drag&drop
  function rebuildOverlay(){
    overlay.innerHTML = '';
    layout.items.forEach(it => {
      if (!it.visible) return;
      const r = toPixels(it);
      const d = document.createElement('div');
      d.className = 'handle';
      d.dataset.id = it.id;
      d.style.left = r.x + 'px';
      d.style.top = r.y + 'px';
      d.style.width = r.w + 'px';
      d.style.height = r.h + 'px';
      d.title = `${it.type}`;
      makeDraggable(d, it);
      overlay.appendChild(d);
    });
  }

  function snapToGrid(px, py){
    const cw = colWidth();
    const gx = Math.max(0, Math.round(px / cw));
    const gy = Math.max(0, Math.round(py / ROW_H));
    return { gx, gy };
  }

  function makeDraggable(el, item){
    let startX=0, startY=0, origX=0, origY=0, dragging=false;
    function down(e){
      dragging = true;
      const p = (e.touches? e.touches[0] : e);
      startX = p.clientX; startY = p.clientY;
      const rect = el.getBoundingClientRect();
      const parent = overlay.getBoundingClientRect();
      origX = rect.left - parent.left; origY = rect.top - parent.top;
      e.preventDefault();
    }
    function move(e){
      if (!dragging) return;
      const p = (e.touches? e.touches[0] : e);
      const dx = p.clientX - startX; const dy = p.clientY - startY;
      const nx = Math.max(0, Math.min(PANEL_W - el.offsetWidth, origX + dx));
      const ny = Math.max(0, Math.min(PANEL_H - el.offsetHeight, origY + dy));
      const s = snapToGrid(nx, ny);
      item.x = Math.min(Math.max(0, s.gx), Math.max(0, GRID_COLS - item.w));
      item.y = Math.max(0, Math.round(ny / ROW_H));
      const r = toPixels(item);
      el.style.left = r.x + 'px'; el.style.top = r.y + 'px';
      drawPreview();
    }
    function up(){ dragging=false; }
    el.addEventListener('mousedown', down);
    el.addEventListener('touchstart', down, {passive:false});
    window.addEventListener('mousemove', move);
    window.addEventListener('touchmove', move, {passive:false});
    window.addEventListener('mouseup', up);
    window.addEventListener('touchend', up);
  }

  // Persistenza (firmware o locale)
  async function loadLayout(){
    try {
      const r = await fetch('/api/layout');
      if (r.ok) return await r.json();
    } catch(_){}
    const local = localStorage.getItem('layoutV1');
    if (local) {
      try { return JSON.parse(local); } catch(_){}
    }
    return JSON.parse(JSON.stringify(defaultLayout));
  }

  async function saveLayout(){
    layout.grid.cols = GRID_COLS;
    layout.grid.row_height = ROW_H;
    try {
      const r = await fetch('/api/layout', {
        method: 'POST', headers: {'Content-Type':'application/json'},
        body: JSON.stringify(layout)
      });
      if (r.ok) return true;
    } catch(_){ }
    // fallback locale
    localStorage.setItem('layoutV1', JSON.stringify(layout));
    return false;
  }

  function applyZoom(){
    const z = Math.max(50, Math.min(150, parseInt(zoomEl.value||'100',10)));
    stage.style.transformOrigin = 'top left';
    stage.style.transform = `scale(${z/100})`;
  }

  function attachEvents(){
    zoomEl.addEventListener('input', applyZoom);
    gridColsEl.addEventListener('change', () => {
      const v = Math.max(6, Math.min(48, parseInt(gridColsEl.value||'24',10)));
      GRID_COLS = v; drawPreview(); rebuildOverlay();
    });
    themeSel.addEventListener('change', drawPreview);
    timeSel.addEventListener('change', drawPreview);

    btnReset.addEventListener('click', async () => {
      layout = JSON.parse(JSON.stringify(defaultLayout));
      GRID_COLS = layout.grid.cols || 24; gridColsEl.value = GRID_COLS;
      drawPreview(); rebuildOverlay();
    });

    btnSave.addEventListener('click', async () => {
      const ok = await saveLayout();
      if (!ok) alert('Layout salvato localmente. Il firmware non ha risposto a /api/layout.');
    });

    btnRefresh.addEventListener('click', async () => {
      // Forza un aggiornamento del display chiamando display-settings con stato corrente
      try {
        const theme = (localStorage.getItem('theme') || document.documentElement.getAttribute('data-theme') || 'default');
        const display = (document.documentElement.getAttribute('data-display') || 'classic');
        await fetch('/api/display-settings', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({theme, display})});
      } catch(_){}
    });
  }

  // Init
  (async function init(){
    layout = await loadLayout();
    if (!layout.grid) layout.grid = { cols: 24, row_height: ROW_H, margin: MARGIN };
    GRID_COLS = layout.grid.cols || 24;
    gridColsEl.value = GRID_COLS;
    drawPreview();
    rebuildOverlay();
    attachEvents();
    applyZoom();
  })();
})();
