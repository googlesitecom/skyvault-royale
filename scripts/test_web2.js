// Verificacion v2 de SKYVAULT Royale (WASM/WebGL2) en Chromium headless:
// menu -> BAJO -> JUGAR -> aterrizaje debug -> capturas de juego/build.
// Fix vs v1: ccall('svDebugState','number') para leer el estado de verdad.
const { chromium } = require('playwright');

const sleep = ms => new Promise(r => setTimeout(r, ms));
const SHOT = n => `/home/z/my-project/scripts/v2_${n}.png`;

(async () => {
  const browser = await chromium.launch({
    headless: true,
    args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader',
           '--no-sandbox', '--disable-dev-shm-usage',
           '--disable-background-timer-throttling',
           '--disable-backgrounding-occluded-windows',
           '--disable-renderer-backgrounding'],
  });
  const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
  const logs = [];
  page.on('console', m => logs.push(`[${m.type()}] ${m.text().slice(0, 200)}`));
  page.on('pageerror', e => logs.push(`[PAGEERROR] ${e.message}`));
  await page.bringToFront().catch(() => {});

  const url = process.env.SV_URL || 'http://localhost:3000/skyvault.html';
  console.log('Abriendo', url);
  await page.goto(url, { waitUntil: 'load', timeout: 120000 });

  for (let i = 0; i < 60; ++i) {
    await sleep(1000);
    const hidden = await page.evaluate(() =>
      document.getElementById('overlay').classList.contains('hidden'));
    if (hidden) break;
  }
  await sleep(2500);
  await page.screenshot({ path: SHOT('menu'), timeout: 120000 });
  console.log('menu capturado');

  // preset BAJO (rapido en SwiftShader) y JUGAR
  await page.mouse.click(590, 458);   // BAJO
  await sleep(600);
  await page.mouse.click(640, 370);   // JUGAR
  console.log('partida iniciada');
  await sleep(6000);

  // aterrizaje rapido via puente de debug
  try { await page.evaluate(() => Module.ccall('svDebugLand')); console.log('svDebugLand OK'); }
  catch (e) { console.log('ccall svDebugLand fallo: ' + e.message); }

  let landed = false, stVal = -1;
  for (let i = 0; i < 240; ++i) {
    await sleep(1000);
    stVal = await page.evaluate(() => Module.ccall('svDebugState', 'number'));
    if (i % 15 === 14) console.log('  sondeo ' + (i + 1) + 's: estado=' + stVal);
    if (stVal === 3) { landed = true; console.log('ATERRIZADO tras ' + (i + 1) + 's'); break; }
  }
  console.log('estado final del sondeo: ' + stVal + (landed ? '' : ' (esperado 3)'));
  await sleep(1500);

  // capturar el raton (click) y mirar un poco hacia abajo: terreno + personaje
  await page.mouse.click(640, 360);
  await sleep(700);
  await page.mouse.move(640, 300, { steps: 2 });
  await sleep(200);
  await page.mouse.move(640, 390, { steps: 8 });
  await sleep(2000);
  await page.screenshot({ path: SHOT('ingame'), timeout: 120000 });
  console.log('ingame capturado');

  // andar hacia delante 8 s (terreno en movimiento)
  await page.keyboard.down('KeyW');
  await sleep(8000);
  await page.keyboard.up('KeyW');
  await sleep(1500);
  await page.screenshot({ path: SHOT('walk'), timeout: 120000 });
  console.log('walk capturado');

  // modo construccion con fantasma de pared
  await page.keyboard.press('KeyQ');
  await sleep(1200);
  await page.screenshot({ path: SHOT('build'), timeout: 120000 });
  console.log('build capturado');
  await page.keyboard.press('KeyQ');

  const errBox = await page.evaluate(() => {
    const el = document.getElementById('err');
    return el && el.style.display !== 'none' ? el.textContent : '';
  });
  if (errBox) console.log('ERRORES EN PANTALLA:\n' + errBox);

  console.log('--- CONSOLA (ultimas 45) ---');
  console.log(logs.slice(-45).join('\n'));

  await browser.close();
})().catch(e => { console.error('FATAL:', e.message); process.exit(1); });
