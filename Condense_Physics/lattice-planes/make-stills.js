#!/usr/bin/env node
// 用无头 Chrome 把 ?film=1 的成品逐组截图，输出 lattice-planes/stills/*.png。
// 既是交作业的图（六组 + 组合），也是这一页的视觉验收：图不对，一眼就看得出来。
//
//   node lattice-planes/make-stills.js           # 七张标准图
//   node lattice-planes/make-stills.js 111       # 换个晶面族
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';

const ROOT = path.resolve(import.meta.dirname, '..');
const CHROME = process.env.CHROME || '/opt/google/chrome-unstable/chrome';
const PORT = Number(process.env.PORT || 9781);
const W = Number(process.env.W || 1600);
const H = Number(process.env.H || 900);
const argv = process.argv.slice(2);
const SMOKE = argv.includes('--smoke');
const FAM = argv.find((a) => !a.startsWith('--')) || '110';
const OUT = path.join(ROOT, 'lattice-planes', 'stills');

const chrome = spawn(CHROME, [
  '--headless=new', `--remote-debugging-port=${PORT}`, '--no-sandbox', '--disable-dev-shm-usage',
  '--hide-scrollbars', '--force-device-scale-factor=1', '--use-gl=angle', '--use-angle=swiftshader',
  '--enable-unsafe-swiftshader', `--window-size=${W},${H}`,
], { stdio: ['ignore', 'ignore', 'pipe'] });
let err = '';
chrome.stderr.on('data', (d) => { err += d; });
process.on('exit', () => chrome.kill('SIGTERM'));

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function getJson(url, tries = 60) {
  for (let i = 0; i < tries; i++) {
    try { const r = await fetch(url); if (r.ok) return await r.json(); } catch { /* 还没起来 */ }
    await sleep(250);
  }
  throw new Error(`拿不到 ${url}\n${err.slice(0, 500)}`);
}

class CDP {
  constructor(url) {
    this.ws = new WebSocket(url);
    this.id = 0;
    this.pending = new Map();
    this.events = [];
    this.ws.onmessage = (e) => {
      const m = JSON.parse(e.data);
      if (m.id && this.pending.has(m.id)) {
        const { resolve, reject } = this.pending.get(m.id);
        this.pending.delete(m.id);
        m.error ? reject(new Error(m.error.message)) : resolve(m.result);
      } else if (m.method) this.events.push(m);
    };
  }
  open() { return new Promise((res, rej) => { this.ws.onopen = res; this.ws.onerror = () => rej(new Error('WS 失败')); }); }
  send(method, params = {}) {
    const id = ++this.id;
    this.ws.send(JSON.stringify({ id, method, params }));
    return new Promise((res, rej) => this.pending.set(id, { resolve: res, reject: rej }));
  }
  async wait(method, timeout = 15000) {
    const t0 = Date.now();
    while (Date.now() - t0 < timeout) {
      if (this.events.some((e) => e.method === method)) return true;
      await sleep(80);
    }
    return false;
  }
}

fs.mkdirSync(OUT, { recursive: true });
const targets = await getJson(`http://127.0.0.1:${PORT}/json/list`);
const cdp = new CDP(targets.find((t) => t.type === 'page').webSocketDebuggerUrl);
await cdp.open();
await cdp.send('Page.enable');
await cdp.send('Emulation.setDeviceMetricsOverride', { width: W, height: H, deviceScaleFactor: 1, mobile: false });

await cdp.send('Page.navigate', { url: `file://${path.join(ROOT, 'lattice-planes', 'plane-110.html')}?film=1&fam=${FAM}` });
await cdp.wait('Page.loadEventFired');
for (let i = 0; i < 40; i++) {
  const r = await cdp.send('Runtime.evaluate', { expression: 'window.__film ? JSON.stringify(window.__film.meta) : ""', returnByValue: true });
  if (r.result.value) break;
  await sleep(200);
}
const meta = JSON.parse((await cdp.send('Runtime.evaluate', { expression: 'JSON.stringify(window.__film.meta)', returnByValue: true })).result.value);
const errors = [];
await cdp.send('Runtime.evaluate', { expression: "window.__err=[];addEventListener('error',e=>window.__err.push(String(e.message)))" });

// 六组 + 组合各一张；组合再来一张把星状/法线球/晶带都打开的。
// 每组的机位由那一组的法线算出来：固定机位下总有一组面正好被看成一条缝。
const camFor = (n) => {
  // 视线与面法线成约 50°：正对法线两张面会互相挡住，掠着看又成一条缝
  const d = [0.9 * n[0] + 0.7 * n[1], 0.9 * n[1] - 0.7 * n[0], 0.5];
  return { az: Math.atan2(d[2], d[0]), po: Math.acos(d[1] / Math.hypot(...d)), dist: 5.4 };
};
const CAM_FORM = { az: Math.atan2(0.55, 0.75), po: Math.acos(-0.62 / Math.hypot(0.75, 0.62, 0.55)), dist: 6.0 };

const allOn = Object.fromEntries(meta.layers.map((k) => [k, true]));
const shots = SMOKE
  // 冒烟：每一组把图层逐个打开，专抓「只在运行时才炸」的引用错误（面板、图层、晶带都走一遍）
  ? [...Array(meta.groups).keys()].flatMap((g) => [
    { group: g, demo: false, layers: allOn },
    ...meta.layers.map((k) => ({ group: g, demo: false, layers: { [k]: true } })),
    ...meta.zones.map((z, i) => ({ group: g, zone: i, demo: false, layers: { zone: true, form: true, pair: true } })),
  ])
  : [
    ...[...Array(meta.groups - 1).keys()].map((g) => ({ group: g, demo: true, cam: camFor(meta.normals[g]) })),
    { group: meta.groups - 1, demo: true, cam: CAM_FORM },
    // 最后一张是「组合 + 法线端点 + 晶带」：全开会把原子、星状面、文字全叠成一团，那不是要给老师看的图
    { group: meta.groups - 1, demo: false, cam: CAM_FORM, out: 'g7-form-sphere-zone',
      layers: { cell: true, points: true, pair: true, labels: true, normal: true, form: true, sphere: true, zone: true } },
  ];

let bad = 0;
for (const s of shots) {
  const set = await cdp.send('Runtime.evaluate', { expression: `window.__film.set(${JSON.stringify(s)})` });
  // 抛异常时画面会停在上一张，字节数一模一样——必须当场报出来
  if (set.exceptionDetails) {
    bad++;
    console.error(`  ✗ ${JSON.stringify(s)} → ${set.exceptionDetails.exception?.description || set.exceptionDetails.text}`);
    if (!SMOKE) continue;
  }
  if (SMOKE) continue;
  await sleep(120);
  const shot = await cdp.send('Page.captureScreenshot', { format: 'png' });
  const name = `${s.out || `g${s.group + 1}`}.png`;
  fs.writeFileSync(path.join(OUT, `plane-${FAM}-${name}`), Buffer.from(shot.data, 'base64'));
  console.log(`  wrote lattice-planes/stills/plane-${FAM}-${name}`);
}
if (SMOKE) console.log(bad ? `${bad} 种图层组合报错 ❌` : `${shots.length} 种图层组合全部画得出来 ✅`);
if (bad) process.exitCode = 1;
const jsErr = await cdp.send('Runtime.evaluate', { expression: 'JSON.stringify(window.__err)', returnByValue: true });
if (JSON.parse(jsErr.result.value).length) errors.push(...JSON.parse(jsErr.result.value));
console.log(errors.length ? `页面报错：${errors.join(' | ')}` : '页面没有报错 ✅');
cdp.ws.close();
chrome.kill('SIGTERM');
