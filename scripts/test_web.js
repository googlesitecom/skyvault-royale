// Verifica SKYVAULT Royale (WASM/WebGL2) en Chromium headless (SwiftShader):
// menu -> preset BAJO -> jugar -> caida -> aterrizaje, con capturas.
const { chromium } = require('playwright');

const sleep = ms => new Promise(r => setTimeout(r, ms));

(async () => {
  const browser = await chromium.launch({
    headless: true,
    args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader',
           '--no-sandbox', '--disable-dev-shm-usage'],
  });
  const page = await browser.newPage({ viewport: { width: 640, height: 360 } });
  const logs = [];
  page.on('console', m => logs.push(`[${m.type()}] ${m.text().slice(0, 160)}`));
  page.on('pageerror', e => logs.push(`[PAGEERROR] ${e.message}`));

  const url = process.env.SV_URL || 'http://localhost:3000/skyvault.html';
  console.log('Abriendo', url);
  await page.goto(url, { waitUntil: 'load', timeout: 120000 });

  // espera a que el overlay se oculte y el fade termine
  for (let i = 0; i < 60; ++i) {
    await sleep(1000);
    const hidden = await page.evaluate(() =>
      document.getElementById('overlay').classList.contains('hidden'));
    if (hidden) break;
  }
  await sleep(2500);   // fade 0.5s + margen
  await page.screenshot({ path: '/home/z/my-project/scripts/shot_menu.png', timeout: 120000 });
  console.log('menu capturado');

  // clic en BAJO (preset rapido para SwiftShader)
  await page.mouse.click(267, 234);
  await sleep(800);
  // clic en JUGAR
  await page.mouse.click(320, 200);
  console.log('partida iniciada');
  await sleep(6000);
  // aterrizaje rapido via puente de debug exportado
  try {
    await page.evaluate(() => Module.ccall('svDebugLand'));
    console.log('svDebugLand invocado');
  } catch (e) { console.log('ccall fallo: ' + e.message); await page.keyboard.press('KeyL'); }
  // esperar hasta 150s a que el estado sea Playing (3)
  let landed = false;
  for (let i = 0; i < 150; ++i) {
    await sleep(1000);
    const st = await page.evaluate(() => Module.ccall('svDebugState'));
    if (st === 3) { landed = true; console.log('ATERRIZADO tras ' + (i + 1) + 's'); break; }
  }
  if (!landed) console.log('NO ATERRIZO en 150s');
  // ligera inclinacion hacia abajo (horizonte visible)
  await page.mouse.move(320, 40, { steps: 2 });
  await sleep(300);
  await page.mouse.move(320, 160, { steps: 8 });
  await sleep(3000);
  await page.screenshot({ path: '/home/z/my-project/scripts/shot_land.png', timeout: 120000 });
  console.log('aterrizaje capturado');
  // andar hacia delante + girar la vista a la derecha
  await page.keyboard.down('KeyW');
  await sleep(25000);
  await page.keyboard.up('KeyW');
  await page.mouse.move(560, 160, { steps: 10 });
  await sleep(8000);
  await page.screenshot({ path: '/home/z/my-project/scripts/shot_walk.png', timeout: 120000 });
  console.log('caminando capturado');

  const errBox = await page.evaluate(() => {
    const el = document.getElementById('err');
    return el && el.style.display !== 'none' ? el.textContent : '';
  });
  if (errBox) console.log('ERRORES EN PANTALLA:\n' + errBox);

  console.log('--- CONSOLA COMPLETA (ultimas 60) ---');
  console.log(logs.slice(-60).join('\n'));

  await browser.close();
})().catch(e => { console.error('FATAL:', e.message); process.exit(1); });
