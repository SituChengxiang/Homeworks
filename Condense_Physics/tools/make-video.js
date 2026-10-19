#!/usr/bin/env node
// 用无头 Chrome 把 ?film=1 的三个页面逐帧截图，再用 ffmpeg 合成 mp4 / gif。
// 不需要网络，也不需要额外依赖：Node 自带 fetch + WebSocket，直接讲 CDP。
//
//   node tools/make-video.js                     # 三个页面全部渲染
//   node tools/make-video.js hcp                 # 只渲一个
//   node tools/make-video.js --smoke 12          # 只截 12 帧，检查管线通不通
//   node tools/make-video.js --gif hcp           # 顺便出 gif（文件会大很多）
import { spawn, spawnSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';

const ROOT = path.resolve(import.meta.dirname, '..');
const CHROME = process.env.CHROME || '/opt/google/chrome-unstable/chrome';
const PORT = Number(process.env.PORT || 9777);
const W = Number(process.env.W || 1280);
const H = Number(process.env.H || 720);
const FPS = Number(process.env.FPS || 30);
const OUT = path.join(ROOT, 'videos');

const argv = process.argv.slice(2);
const arg = (name, dflt) => {
  const i = argv.indexOf(`--${name}`);
  return i < 0 ? dflt : argv[i + 1];
};
const wantGif = argv.includes('--gif');
const smoke = argv.includes('--smoke') ? Number(arg('smoke', 12)) : 0;
const picks = argv.filter((a) => !a.startsWith('--') && !/^\d+$/.test(a));

const ease = (k) => 1 - Math.pow(1 - k, 3);

const SCENES = [
  { key: 'crystal', file: 'crystal-cells.html', title: 'SC / BCC / FCC 与 Wigner–Seitz', build: crystalFilm },
  { key: 'diamond', file: 'diamond-zincblende.html', title: '金刚石 / ZnS：沿体对角线平移 ¼', build: diamondFilm },
  { key: 'hcp', file: 'hcp-close-packed.html', title: '六方密堆积 HCP 与堆积率', build: hcpFilm },
].filter((s) => !picks.length || picks.some((p) => s.key.startsWith(p)));

/* ---------------- 每条片子的分镜 ---------------- */

function crystalFilm(meta) {
  const per = 15;                     // 每步停留帧数
  const frames = [];
  const types = ['SC', 'BCC', 'FCC'];
  for (const [ti, type] of types.entries()) {
    const n = meta.steps[type];
    for (let s = 0; s < n; s++) {
      const from = s === 0 ? meta.defaultRadius : meta.radius[type][s - 1];
      const to = meta.radius[type][s];
      for (let f = 0; f < per; f++) {
        const k = f / (per - 1);
        frames.push({
          type,
          step: s,
          radiusTweenTo: from + (to - from) * ease(k),
          cam: { az: 0.75 + ti * 2.09 + s * 0.055 + k * 0.055, po: 1.1, dist: 9.3 },
        });
      }
    }
  }
  return frames;
}

function diamondFilm(meta) {
  const frames = [];
  const sweep = 54;
  for (const mat of meta.materials) {
    // 0 → ¼：两套 FCC 滑开，键长正好等于位移
    for (let f = 0; f < sweep; f++) {
      const k = ease(f / (sweep - 1));
      frames.push({ mat, t: 0.25 * k, cam: { az: 0.9, po: 1.05, dist: 9.6 } });
    }
    for (let f = 0; f < 26; f++)
      frames.push({ mat, t: 0.25, cam: { az: 0.9 + f * 0.02, po: 1.05, dist: 9.6 } });
    // 继续推到 ½：两套 FCC 合成简单立方
    for (let f = 0; f < 34; f++) {
      const k = ease(f / 33);
      frames.push({ mat, t: 0.25 + 0.25 * k, cam: { az: 1.42 + k * 0.5, po: 1.05, dist: 9.6 } });
    }
    for (let f = 0; f < 16; f++)
      frames.push({ mat, t: 0.5, cam: { az: 1.92, po: 1.05, dist: 9.6 } });
    frames.push({ mat: meta.materials[0], t: 0.25 });   // 复位，避免下一个材料沿用奇怪状态
  }
  return frames;
}

/**
 * HCP 的分镜：结构六方对称，c 轴是 z，而相机的极角以 y 为极轴——
 * po = π/2、az = π/2 才是"沿 c 轴俯视六角"。每步按要看的东西单独配机位。
 */
const HCP_CAM = [
  { az: Math.PI / 2, po: 0.62 },   // 1 密置层：俯视，看清 6 个贴 1 个
  { az: Math.PI / 2, po: 0.55 },   // 2 空隙：俯视看两种三角形
  { az: 0.95, po: 1.15 },          // 3 第二层：斜侧，看得见"落在凹陷里"
  { az: 0.75, po: 1.22 },          // 4 ABAB：侧面看层序
  { az: Math.PI / 2, po: 0.6 },    // 5 棱柱：俯视六边形
  { az: 0.7, po: 1.05 },           // 6 份额：斜看楔形
  { az: 0.98, po: 1.28 },          // 7 正四面体：让四条棱摊开
  { az: 0.72, po: 1.12 },          // 8 配位数 12
  { az: 0.85, po: 1.18 },          // 9 键
  { az: Math.PI / 2, po: 0.66 },   // 10 堆积率：俯视，棱柱轮廓最清楚
  { az: 0.68, po: 1.16 },          // 11 非理想轴比
];

function hcpFilm(meta) {
  const per = 24;
  const frames = [];
  for (let s = 0; s < meta.steps; s++) {
    const base = HCP_CAM[s % HCP_CAM.length];
    for (let f = 0; f < per; f++) {
      const k = f / (per - 1);
      frames.push({
        step: s,
        cam: { az: base.az + k * 0.16, po: base.po, dist: 9.6 },
      });
    }
  }
  // 收尾：拖轴比看堆积率掉下来
  const sweep = 60;
  const last = HCP_CAM[HCP_CAM.length - 1];
  for (let f = 0; f < sweep; f++) {
    const k = f / (sweep - 1);
    frames.push({
      step: meta.steps - 1,
      ca: meta.idealCa + (1.88 - meta.idealCa) * k,
      cam: { az: last.az + k * 0.7, po: last.po, dist: 9.6 },
    });
  }
  return frames;
}

/* ---------------- CDP 客户端 ---------------- */

class CDP {
  constructor(url) {
    this.ws = new WebSocket(url);
    this.id = 0;
    this.pending = new Map();
    this.events = [];
    this.ws.onmessage = (e) => {
      const msg = JSON.parse(e.data);
      if (msg.id && this.pending.has(msg.id)) {
        const { resolve, reject } = this.pending.get(msg.id);
        this.pending.delete(msg.id);
        msg.error ? reject(new Error(msg.error.message)) : resolve(msg.result);
      } else if (msg.method) {
        this.events.push(msg);
      }
    };
  }

  open() {
    return new Promise((res, rej) => { this.ws.onopen = res; this.ws.onerror = () => rej(new Error('WebSocket 连接失败')); });
  }

  send(method, params = {}) {
    const id = ++this.id;
    this.ws.send(JSON.stringify({ id, method, params }));
    return new Promise((res, rej) => this.pending.set(id, { resolve: res, reject: rej }));
  }

  async wait(method, timeout = 15000) {
    const t0 = Date.now();
    while (Date.now() - t0 < timeout) {
      if (this.events.some((e) => e.method === method)) return true;
      await new Promise((r) => setTimeout(r, 80));
    }
    return false;
  }
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function fetchJson(url, tries = 60) {
  for (let i = 0; i < tries; i++) {
    try {
      const r = await fetch(url);
      if (r.ok) return await r.json();
    } catch { /* chrome 还没起来 */ }
    await sleep(250);
  }
  throw new Error(`拿不到 ${url}`);
}

/* ---------------- 主流程 ---------------- */

fs.mkdirSync(OUT, { recursive: true });
const frameRoot = path.join(OUT, '_frames');
fs.rmSync(frameRoot, { recursive: true, force: true });

const chrome = spawn(CHROME, [
  '--headless=new',
  `--remote-debugging-port=${PORT}`,
  '--no-sandbox',
  '--disable-dev-shm-usage',
  '--hide-scrollbars',
  '--force-device-scale-factor=1',
  '--use-gl=angle',
  '--use-angle=swiftshader',
  '--enable-unsafe-swiftshader',
  '--disable-features=Translate',
  '--window-size=' + W + ',' + H,
], { stdio: ['ignore', 'pipe', 'pipe'] });

let stderr = '';
chrome.stderr.on('data', (d) => { stderr += d.toString(); });
chrome.on('exit', (code) => { if (code !== 0 && code !== null) console.error('Chrome 退出', code, stderr.slice(0, 400)); });

process.on('exit', () => chrome.kill('SIGTERM'));

const targets = await fetchJson(`http://127.0.0.1:${PORT}/json/list`);
const page = targets.find((t) => t.type === 'page');
if (!page) throw new Error('没有可用的 page target');
const cdp = new CDP(page.webSocketDebuggerUrl);
await cdp.open();

await cdp.send('Page.enable');
await cdp.send('Runtime.enable');
await cdp.send('Emulation.setDeviceMetricsOverride', { width: W, height: H, deviceScaleFactor: 1, mobile: false });

const results = [];
for (const scene of SCENES) {
  const url = `file://${path.join(ROOT, scene.file)}?film=1`;
  console.log(`\n▶ ${scene.file}`);
  await cdp.send('Page.navigate', { url });
  await cdp.wait('Page.loadEventFired');

  // 等摄制接口就位（film 模式会改布局，留点时间给 ResizeObserver）
  let meta = null;
  for (let i = 0; i < 40 && !meta; i++) {
    const r = await cdp.send('Runtime.evaluate', { expression: 'window.__film ? JSON.stringify(window.__film.meta) : ""', returnByValue: true });
    meta = r.result.value ? JSON.parse(r.result.value) : null;
    if (!meta) await sleep(200);
  }
  if (!meta) throw new Error(`${scene.file} 没有 __film 接口`);
  await sleep(400);

  let frames = scene.build(meta);
  if (smoke) {
    // 冒烟测试要均匀取样整条时间线，否则只会看到开头那一步，看不出构图问题
    const step = Math.max(1, Math.floor(frames.length / smoke));
    frames = frames.filter((_, i) => i % step === 0).slice(0, smoke);
  }
  const dir = path.join(frameRoot, scene.key);
  fs.mkdirSync(dir, { recursive: true });

  const t0 = Date.now();
  for (let i = 0; i < frames.length; i++) {
    await cdp.send('Runtime.evaluate', { expression: `window.__film.set(${JSON.stringify(frames[i])})` });
    const shot = await cdp.send('Page.captureScreenshot', { format: 'png' });
    fs.writeFileSync(path.join(dir, `${String(i).padStart(4, '0')}.png`), Buffer.from(shot.data, 'base64'));
    if ((i + 1) % 100 === 0 || i + 1 === frames.length) {
      const per = (Date.now() - t0) / (i + 1);
      console.log(`   ${i + 1}/${frames.length} 帧（${per.toFixed(0)} ms/帧，预计还需 ${(((frames.length - i - 1) * per) / 1000).toFixed(0)} s）`);
    }
  }

  const mp4 = path.join(OUT, `${scene.key}.mp4`);
  const enc = spawnSync('ffmpeg', ['-y', '-framerate', String(FPS), '-i', path.join(dir, '%04d.png'),
    '-c:v', 'libx264', '-preset', 'medium', '-crf', '20', '-pix_fmt', 'yuv420p',
    '-movflags', '+faststart', mp4], { encoding: 'utf8' });
  if (enc.status !== 0) { console.error(enc.stderr.slice(-800)); throw new Error('ffmpeg 合成失败'); }
  const kb = fs.statSync(mp4).size / 1024;
  console.log(`   ✅ ${path.relative(ROOT, mp4)}  ${frames.length} 帧 ≈ ${(frames.length / FPS).toFixed(1)} s  ${kb.toFixed(0)} KB`);
  const row = { scene: scene.key, frames: frames.length, seconds: +(frames.length / FPS).toFixed(1), mp4: `${(kb / 1024).toFixed(2)} MB` };

  if (wantGif) {
    const gif = path.join(OUT, `${scene.key}.gif`);
    const vf = 'fps=12,scale=720:-1:flags=lanczos,split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=5';
    const g = spawnSync('ffmpeg', ['-y', '-i', mp4, '-vf', vf, gif], { encoding: 'utf8' });
    if (g.status !== 0) { console.error(g.stderr.slice(-600)); }
    else {
      const gkb = fs.statSync(gif).size / 1024 / 1024;
      row.gif = `${gkb.toFixed(1)} MB`;
      console.log(`   ✅ ${path.relative(ROOT, gif)}  ${gkb.toFixed(1)} MB`);
    }
  }
  results.push(row);
}

console.log('\n—— 汇总 ——');
console.table(results);
if (!smoke) fs.rmSync(frameRoot, { recursive: true, force: true });
cdp.ws.close();
chrome.kill('SIGTERM');
