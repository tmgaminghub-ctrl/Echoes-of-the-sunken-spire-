import { chromium } from 'playwright';

// Inspects the LIVE scene of a shipped standalone build: are the character
// materials actually carrying a texture, and does that texture have pixels?
const [url, shot] = process.argv.slice(2);

const b = await chromium.launch({
  args: ['--no-sandbox', '--disable-dev-shm-usage', '--use-gl=angle',
         '--use-angle=swiftshader', '--enable-unsafe-swiftshader',
         '--js-flags=--max-old-space-size=320', '--disable-gpu-compositing']
});
const p = await b.newPage({ viewport: { width: 640, height: 360 } });
const errs = [];
p.on('pageerror', e => errs.push('PAGEERROR ' + e.message.slice(0, 160)));
p.on('console', m => {
  const t = m.text();
  if (m.type() === 'error') errs.push(t.slice(0, 160));
  if (/standalone/.test(t)) console.log('[log]', t);
});

await p.goto(url, { waitUntil: 'domcontentloaded', timeout: 180000 });
const fr = p.frames().find(f => f !== p.mainFrame()) || p.mainFrame();

let ready = false;
for (let i = 0; i < 10; i++) {
  await p.waitForTimeout(2500);
  try {
    ready = await fr.evaluate(() => {
      const t = document.querySelector('#title-screen, #title');
      return !!t && !t.classList.contains('hidden');
    });
  } catch (e) { ready = false; }
  if (ready) break;
}
console.log('[t] title ready:', ready);

if (ready) {
  try { await fr.evaluate(() => document.querySelector('#start-btn').click()); } catch (e) {}
  await p.waitForTimeout(7000);

  const report = await fr.evaluate(() => {
    const g = window.__game;
    if (!g) return { error: 'no window.__game' };

    // sample the average colour of a texture's image, so "map exists" is not
    // confused with "map is a blank white texture"
    const sampleMap = (tex) => {
      try {
        const img = tex.image;
        if (!img) return null;
        const w = img.width || img.videoWidth || 0, h = img.height || img.videoHeight || 0;
        if (!w || !h) return { w, h, note: 'no dimensions' };
        const c = document.createElement('canvas');
        c.width = 32; c.height = 32;
        const ctx = c.getContext('2d');
        if (!ctx) return { w, h, note: 'no 2d ctx' };
        ctx.drawImage(img, 0, 0, 32, 32);
        const d = ctx.getImageData(0, 0, 32, 32).data;
        let r = 0, gg = 0, bb = 0, n = 0;
        for (let i = 0; i < d.length; i += 4) { r += d[i]; gg += d[i + 1]; bb += d[i + 2]; n++; }
        return { w, h, type: img.constructor && img.constructor.name,
                 avg: [Math.round(r / n), Math.round(gg / n), Math.round(bb / n)],
                 distinct: new Set([...Array(64).keys()].map(i => {
                   const d2 = d; const o = i * 4;
                   return d2[o] + ',' + d2[o + 1] + ',' + d2[o + 2];
                 })).size };
      } catch (e) { return { error: String(e).slice(0, 80) }; }
    };

    const meshInfo = (root, label) => {
      const out = { label, meshes: 0, withMap: 0, withoutMap: [], maps: [] };
      if (!root) return out;
      const seen = new Set();
      root.traverse((o) => {
        if (!o.isMesh && !o.isSkinnedMesh) return;
        out.meshes++;
        const mats = Array.isArray(o.material) ? o.material : [o.material];
        for (const m of mats) {
          if (!m) continue;
          if (m.map) {
            out.withMap++;
            if (!seen.has(m.uuid)) {
              seen.add(m.uuid);
              out.maps.push({ mat: m.name || m.type, map: sampleMap(m.map),
                              color: m.color ? [m.color.r, m.color.g, m.color.b].map(v => +v.toFixed(2)) : null });
            }
          } else {
            out.withoutMap.push((o.name || '?') + '/' + (m.name || m.type));
          }
        }
      });
      out.meshesWithNoMap = out.withoutMap.length;
      out.withoutMap = out.withoutMap.slice(0, 6);
      return out;
    };

    const players = [];
    // the player actor plus nearby enemies / NPCs
    try { players.push(meshInfo(g.player && g.player.model, 'player')); } catch (e) {}
    try { if (g.enemies) g.enemies.slice(0, 3).forEach((e, i) => players.push(meshInfo(e.model, 'enemy' + i))); } catch (e) {}
    try { if (g.npcs) g.npcs.slice(0, 2).forEach((n, i) => players.push(meshInfo(n.model || n, 'npc' + i))); } catch (e) {}
    return { players, misses: (window.__assetMisses || []).slice(0, 8),
             inlined: window.__glbTexturesInlined || 0 };
  });

  console.log(JSON.stringify(report, null, 2));

  // zoom the camera onto the character for a visual check
  try {
    await fr.evaluate(() => {
      const g = window.__game;
      if (g && g.camera && g.player) {
        const p = g.player.model ? g.player.model.position : null;
        if (p) { g.camera.position.set(p.x + 1.2, p.y + 1.6, p.z + 2.2); g.camera.lookAt(p.x, p.y + 0.9, p.z); }
      }
    });
    await p.waitForTimeout(900);
  } catch (e) {}
  await p.screenshot({ path: shot });
}
console.log(JSON.stringify({ ready, errors: errs.slice(0, 5) }));
await b.close();
