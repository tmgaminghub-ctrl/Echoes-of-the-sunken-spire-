import { chromium } from 'playwright';
const url = process.argv[2], out = process.argv[3];
const exe = '/home/user/.cache/ms-playwright/chromium-1248/chrome-linux64/chrome';
const b = await chromium.launch({ executablePath: exe, args: ['--no-sandbox','--disable-gpu','--use-gl=swiftshader','--js-flags=--max-old-space-size=420','--disable-dev-shm-usage'] });
const p = await b.newPage({ viewport: { width: 900, height: 620 } });
p.on('console', m => { const t = m.text(); if (/standalone|THREE|error|\[tex\]/i.test(t)) console.log('[c]', t); });
p.on('pageerror', e => console.log('[err]', e.message));
await p.goto(url, { waitUntil: 'load', timeout: 120000 });
for (let i = 0; i < 12; i++) {
  await p.waitForTimeout(2500);
  if (await p.evaluate(() => !!(window.__game && window.__game.player && window.__game.player.model))) break;
}
try { await p.getByText('BEGIN YOUR TALE', { exact: false }).first().click({ timeout: 15000 }); console.log('clicked begin'); } catch (e) { console.log('no begin button', e.message); }
await p.waitForTimeout(6000);
const info = await p.evaluate(() => {
  const g = window.__game; if (!g) return { err: 'no __game' };
  const THREE = g.THREE || null;
  const pm = g.player.model;
  const p = pm.position || (pm.parent && pm.parent.position);
  // put the camera 3m in front of the character, looking at its chest
  const cam = g.camera;
  const tgt = { x: p.x, y: p.y + 1.0, z: p.z };
  if (cam) {
    cam.position.set(p.x + 2.2, p.y + 1.7, p.z + 2.2);
    cam.lookAt(tgt.x, tgt.y, tgt.z);
    cam.updateMatrixWorld(true);
  }
  // hide any HUD so the shot is clean
  document.querySelectorAll('canvas + *').forEach(() => {});
  document.getElementById('hud') && (document.getElementById('hud').style.display = 'none');
  return { pos: p, camAt: cam && cam.position.toArray(), misses: window.__assetMisses, inlined: window.__glbTexturesInlined,
           meta: (window.__game.player.model.userData && Object.keys(window.__game.player.model.userData)) };
});
console.log(JSON.stringify(info));
await p.waitForTimeout(1500);
await p.evaluate(() => { const g = window.__game; const p = g.player.model.position; if (g.camera) { g.camera.position.set(p.x + 2.2, p.y + 1.7, p.z + 2.2); g.camera.lookAt(p.x, p.y + 1.0, p.z); } });
await p.waitForTimeout(1200);
await p.evaluate(() => {
  const g = window.__game;
  if (!g || !g.camera) return;
  // take over the camera every frame so the game loop can't snap it back
  g.__camLock = true;
  const old = g.update || null;
  const lock = () => {
    const pm = g.player && g.player.model; if (!pm) return;
    const q = pm.position || pm.parent.position;
    g.camera.position.set(q.x + 1.7, q.y + 1.5, q.z + 1.7);
    g.camera.lookAt(q.x, q.y + 0.95, q.z);
    requestAnimationFrame(lock);
  };
  lock();
});
await p.waitForTimeout(2500);
const rep = await p.evaluate(() => window.__textureReport ? window.__textureReport() : 'NO REPORT FN');
console.log('REPORT', JSON.stringify(rep));
console.log('BADGE', await p.evaluate(() => (document.getElementById('standalone-badge') || {}).textContent));
await p.screenshot({ path: out });
console.log('shot', out);
await b.close();
