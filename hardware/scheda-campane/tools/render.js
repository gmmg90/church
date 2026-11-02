// Converte gli SVG esportati da KiCad in PNG (anteprime per la revisione).
// Uso: node render.js <input.svg> <output.png> [larghezza_px] [sfondo]
const { chromium } = require('playwright');
const fs = require('fs');

(async () => {
  const [inp, out, widthArg, bg] = process.argv.slice(2);
  const width = parseInt(widthArg || '1600', 10);
  const svg = fs.readFileSync(inp, 'utf8');
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width, height: 400 } });
  await page.setContent(`<!doctype html><html><body style="margin:0;background:${bg || '#0b3d1f'}">
    <div id="w" style="width:${width}px">${svg}</div></body></html>`);
  await page.evaluate(() => {
    const s = document.querySelector('svg');
    s.setAttribute('width', '100%');
    s.removeAttribute('height');
  });
  const el = await page.$('#w');
  await el.screenshot({ path: out });
  await browser.close();
})();
