// 作业 A · 反轴 n̄ 与等效点系
//
// 布局：左 3D、右「上下圈展开图 + 教材俯视图角标」，底下字幕条（两端就是翻页）与分段进度轨。
// 数据流是单向的：state → (3D 图层, 2D 模型, 字幕, 轨道, 数据表)。
// 任何「几个点、几处、几对」都不写在文案里，全部由 lib/ 现算。
import '../../design/tokens.css';
import * as THREE from 'three';
import { ID3, invertStep, over } from '../../lib/matrix.js';
import { barOrbit, barGroup, powGen } from '../../lib/rotoinversion.js';
import { orbitModel, selectPoint } from '../../lib/projection.js';
import { COLORS as C } from '../../design/tokens.js';
import { createSymbolPanel } from '../../design/symbols2d.js';
import {
  makeStage, startLoop, clearGroup, cachedObject, makeAppear,
  unitSphere, stdMat,
} from '../../src/view.js';
import { STEPS, LAYERS, SUMMARY_ORDER } from './steps.js';
import { BUILDERS, PM } from './render3d.js';

const el = (id) => document.getElementById(id);

/* token 的基准单位必须由舞台实际尺寸回写：CSS 里那个 --u: 9px 只是兜底，
   不接线的话小窗口上字号全部超尺寸，整块布局会被字幕条挤扁。 */
function syncUnit() {
  const box = el('stage').getBoundingClientRect();
  const u = Math.max(2, Math.min(box.width, box.height) / 100);
  document.documentElement.style.setProperty('--u', `${u.toFixed(3)}px`);
}
new ResizeObserver(syncUnit).observe(el('stage'));
syncUnit();
const clamp = (x, lo, hi) => Math.min(hi, Math.max(lo, x));
const fmt = (x) => { const r = Math.round(x * 100) / 100; return (Object.is(r, -0) ? 0 : r).toFixed(2); };

/* =========================================================
   状态
   ========================================================= */
const state = {
  step: 0,
  n: STEPS[0].set.n,
  k: STEPS[0].set.k || 0,
  sel: null,                 // 选中的等效点（orbit.pts 的下标），三处共用
  over: {},                  // 图层手动覆盖（只影响当前这一步）
  drawer: null,              // 'layers' | 'data' | null
  playing: false,
  clean: false,
};

const stepNow = () => STEPS[state.step];
const layersNow = () => ({ ...LAYERS, ...stepNow().layers, ...state.over });
const orderOf = (n) => barGroup(n).props.order;

/* =========================================================
   3D
   ========================================================= */
const { renderer, scene, camera, controls, root, dirty } = makeStage({ canvasId: 'scene', fogDensity: 0.006 });
camera.up.set(0, 0, 1);                 // 反轴是竖着的
camera.position.set(5.6, -5.6, 3.2);
controls.target.set(0, 0, 0);
controls.update();

const appear = makeAppear();
let shown = new Set();
let liveNode = null;
let pose = ID3;

function haloAt(p, color) {
  const m = new THREE.Mesh(unitSphere(16, 12), stdMat(`halo-sel|${color}`, {
    color, emissive: color, emissiveIntensity: 0.6, transparent: true, opacity: 0.5, roughness: 0.5,
  }));
  m.position.set(p[0], p[1], p[2]);
  m.scale.setScalar(0.22);
  return m;
}

function rebuild3d(animate = false) {
  clearGroup(root);
  liveNode = null;
  const L = layersNow();
  const n = state.n;
  const added = [];
  for (const key of Object.keys(LAYERS)) {
    if (!L[key]) continue;
    const o = cachedObject(`${key}|${n}|${state.k}`, () => BUILDERS[key](n, state.k));
    added.push(o);
    root.add(o);
    if (key === 'live') { liveNode = o; setPose(pose); }
  }
  if (state.sel !== null) {
    const o = cachedObject(`sel|${n}|${state.sel}`, () => {
      const g = new THREE.Group();
      const orbit = barOrbit(n, PM);
      const ids = new Set([state.sel]);
      const pair = orbit.pairs.find(([a, b]) => a === state.sel || b === state.sel);
      if (pair) { ids.add(pair[0]); ids.add(pair[1]); }
      [...ids].forEach((i) => {
        const p = orbit.pts[i] && orbit.pts[i].p;
        if (p) g.add(haloAt(p, orbit.pts[i].det > 0 ? C.accent : C.improper));
      });
      return g;
    });
    added.push(o);
    root.add(o);
  }
  const fresh = added.filter((o) => !shown.has(o));
  shown = new Set(added);
  if (animate) appear.begin(root, fresh); else appear.settle();
  dirty.set();
}

function setPose(M) {
  pose = M;
  if (!liveNode) return;
  liveNode.matrixAutoUpdate = false;
  liveNode.matrix.set(
    M[0][0], M[0][1], M[0][2], 0,
    M[1][0], M[1][1], M[1][2], 0,
    M[2][0], M[2][1], M[2][2], 0,
    0, 0, 0, 1
  );
  liveNode.matrixWorldNeedsUpdate = true;
  dirty.set();
}

/* 施加一次 n̄ 的动画：转 (360/n)·t 同时整体按 (1−2t) 缩放 —— 反轴的定义照原样演 */
const anim = { active: false, from: ID3, to: ID3, t0: 0, dur: 780 };
function animateTo(k) {
  anim.from = powGen(state.n, state.k);
  anim.to = powGen(state.n, k);
  anim.t0 = performance.now();
  anim.active = true;
  state.k = k;
}
function tickAnim(now) {
  if (!anim.active) return false;
  const t = clamp((now - anim.t0) / anim.dur, 0, 1);
  const e = 1 - Math.pow(1 - t, 3);
  setPose(t >= 1 ? anim.to : invertStep(state.n, e, anim.from));
  if (t >= 1) anim.active = false;
  return true;
}

/* 点 3D 里的等效点 → 三向联动 */
const ray = new THREE.Raycaster();
const ndc = new THREE.Vector2();
el('scene').addEventListener('click', (e) => {
  const box = e.target.getBoundingClientRect();
  ndc.set(((e.clientX - box.left) / box.width) * 2 - 1, -((e.clientY - box.top) / box.height) * 2 + 1);
  ray.setFromCamera(ndc, camera);
  const hit = ray.intersectObjects(root.children, true)
    .find((h) => h.object.userData.pickId !== undefined);
  if (hit) select(hit.object.userData.pickId);
});

/* =========================================================
   2D：展开图为王，教材俯视图缩成角标
   ========================================================= */
const fanPanel = createSymbolPanel(el('fan'), { onPick: select });
const topPanel = createSymbolPanel(el('top'), { onPick: select });
topPanel.setInset(false);              // 角标自己内部没有角标

function draw2d() {
  const orbit = barOrbit(state.n, PM);
  const fan = orbitModel(orbit, `${over(state.n)} 上下圈展开`, { view: 'fan' });
  const top = orbitModel(orbit, `${over(state.n)} 俯视（教材画法）`);
  // 角标只负责「教材里那张图长什么样」：标题、副标题、图例都剥掉，否则小格子里全是字
  top.heading = '';
  top.sub = '';
  top.legend = false;
  // 画布图例只在设计系统的对照页里出现；讲课这一页字越少越好，符号含义在图层抽屉里
  fan.legend = false;
  if (state.sel !== null) { selectPoint(fan, state.sel); selectPoint(top, state.sel); }
  fanPanel.render(fan);
  topPanel.render(top);
  return { fan, top };
}

/* =========================================================
   字幕条、进度轨、抽屉
   ========================================================= */
function renderChrome() {
  const s = stepNow();
  el('capIdx').textContent = `第 ${state.step + 1} / ${STEPS.length} 步`;
  el('capTitle').textContent = s.title;
  // 统计量取自展开图模型：上圈/下圈/对齐是它数出来的，俯视落几处是那张图标出来的
  const st = current2d.fan.stats;
  el('capNote').innerHTML = `<b style="color:var(--c-accent)">${s.punch}</b>　`
    + `<span style="color:var(--c-muted)">${st.points} 点 · 上圈 ${st.up} / 下圈 ${st.down}`
    + ` · 俯视落 ${current2d.top.stats.spots} 处 · 下圈落在径向线上 ${st.aligned} 个</span><br>${s.note}`;

  const prev = el('prev');
  const next = el('next');
  prev.disabled = state.step === 0;
  next.disabled = state.step === STEPS.length - 1;
  el('nextPeek').textContent = state.step < STEPS.length - 1 ? STEPS[state.step + 1].key : '';
  prev.querySelector('span:last-child').textContent = state.step > 0 ? STEPS[state.step - 1].key : '上一步';
  next.querySelector('span:nth-child(2)').textContent = '下一步';

  // 8 段步骤 + 两个抽屉按钮：轨道是 grid，列数得跟着条目数走，否则多出来的两个会溢出
  el('rail').style.gridTemplateColumns = `repeat(${STEPS.length}, 1fr) auto auto`;
  el('rail').innerHTML = STEPS.map((st, i) => `<button class="seg${i === state.step ? ' now' : ''}${i < state.step ? ' done' : ''}"
      data-i="${i}"><i></i>${st.key}</button>`).join('')
    + `<button class="seg" data-drawer="layers"><i style="opacity:0"></i>图层 L</button>`
    + `<button class="seg" data-drawer="data"><i style="opacity:0"></i>数据 D</button>`;

  el('badge').textContent = `3D · ${over(state.n)} · |G| = ${barGroup(state.n).props.order}`;
  renderLayers();
  renderData();
}

function renderLayers() {
  const L = layersNow();
  const boxes = Object.keys(LAYERS).map((key) => `<div class="sw"><label for="ly-${key}">${LAYER_NAME[key]}</label>
    <input type="checkbox" id="ly-${key}" data-layer="${key}" ${L[key] ? 'checked' : ''}><span class="tk"></span></div>`).join('');
  const axes = SUMMARY_ORDER.concat([5]).map((n) => `<button class="nav" data-n="${n}"
    style="padding:calc(var(--u)*.5) calc(var(--u)*1.1);font-size:var(--fs-body);${n === state.n ? 'background:rgba(34,211,238,.3)' : ''}">${over(n)}</button>`).join(' ');
  el('layers').innerHTML = `<h3>反轴</h3><div style="display:flex;gap:calc(var(--u)*.5);flex-wrap:wrap">${axes}</div>
    <h3>图层</h3>${boxes}
    <div class="sw" style="display:block">
      <label for="kSlider">施加次数 k = <b style="color:var(--c-accent)">${state.k}</b> / ${orderOf(state.n)}</label>
      <input type="range" id="kSlider" min="0" max="${orderOf(state.n)}" step="1" value="${state.k}" style="width:100%">
    </div>
    <h3>图上这些符号</h3><p class="keys">
    展开图：上圈 = z&gt;0、下圈 = z&lt;0，<b>高度换成半径</b>，所以 6 个点就是 6 个点；
    细径向线是上圈的方位角，<b>下圈点落在径向上 = 上下同方位角 = 有 m⊥</b>；
    虚线穿过中心 = 那一对关于 i。蓝点 = 真旋转得到，红点 = 带反映/反演得到，小指针 = 手性。
    右下角是教材俯视图（半填圆点表 z 的正负），上下成对的两点在那里会重合。</p>
    <h3>键盘</h3><p class="keys"><b>→ / 空格</b> 下一步 · <b>←</b> 上一步 · <b>1 2 3 4 6</b> 换反轴 ·
    <b>L</b> 图层 · <b>D</b> 数据 · <b>F</b> 纯净 · <b>Esc</b> 取消选中</p>`;
  el('layers').classList.toggle('open', state.drawer === 'layers');
}

function renderData() {
  const orbit = barOrbit(state.n, PM);
  const rows = orbit.pts.map((x, i) => {
    const rot = Math.round((x.k * 360 / state.n) % 360);
    return `<tr data-sel="${i}"${state.sel === i ? ' class="sel"' : ''}>
      <td>${over(state.n)}<sup>${x.k}</sup></td>
      <td>${x.det > 0 ? `转 ${rot}°` : `转 ${rot}° + 反演`}</td>
      <td>(${fmt(x.p[0])}, ${fmt(x.p[1])}, ${fmt(x.p[2])})</td>
      <td>${x.det > 0 ? '+' : '−'}</td></tr>`;
  }).join('');
  const sum = SUMMARY_ORDER.map((n) => {
    const g = barGroup(n).props;
    return `<tr${n === state.n ? ' class="sel"' : ''} data-n="${n}"><td>${over(n)}</td>
      <td>${g.order}</td><td>${g.maxProper}</td><td>${g.hasI ? '✓' : '—'}</td><td>${g.hasMirrorPerp ? '✓' : '—'}</td></tr>`;
  }).join('');
  el('data').innerHTML = `<h3>${over(state.n)} 的等效点系（点一行可高亮三处）</h3>
    <table class="data"><tr><th>幂次</th><th>做了什么</th><th>P 落到</th><th>det</th></tr>${rows}</table>
    <h3>五种反轴对照（数字全部现算）</h3>
    <table class="data"><tr><th></th><th>阶</th><th>真轴</th><th>i</th><th>m⊥</th></tr>${sum}</table>
    <p class="keys">读法：$\bar{3}$ 那行是「有 i、无 m⊥」，$\bar{6}$ 那行正好相反 —— 这就是 3 + i 与 3/m 的区别。</p>`;
  el('data').classList.toggle('open', state.drawer === 'data');
}

const LAYER_NAME = {
  axis: '反轴与记号', trace: '定义路径（弧 + 穿心线）', motif: '原始基元 P',
  orbit: '全部等效点', numbers: '编号', pairs: '配对关系',
  mirror: '水平镜面 m⊥', center: '对称中心 i', live: '一步一步施加', ghost: '已走过的虚影',
};

/* =========================================================
   动作
   ========================================================= */
let current2d = draw2d();

function refresh(animate = false) {
  current2d = draw2d();
  rebuild3d(animate);
  renderChrome();
}

function select(id) {
  state.sel = state.sel === id ? null : id;
  refresh(false);
}

function gotoStep(i) {
  state.step = clamp(i, 0, STEPS.length - 1);
  state.over = {};
  state.sel = null;
  const set = stepNow().set;
  const nChanged = set.n !== state.n;
  state.n = set.n;
  state.k = set.k || 0;
  pose = powGen(state.n, state.k);
  if (nChanged) anim.active = false;
  refresh(true);
}

function setN(n) {
  state.n = clamp(n, 1, 6);
  state.k = clamp(state.k, 0, orderOf(state.n));
  state.sel = null;
  pose = powGen(state.n, state.k);
  refresh(true);
}

function setK(k) {
  const want = clamp(k, 0, orderOf(state.n));
  if (want === state.k) return;
  if (layersNow().live) animateTo(want); else { state.k = want; pose = powGen(state.n, want); }
  refresh(false);
}

function toggleDrawer(which) {
  state.drawer = state.drawer === which ? null : which;
  renderChrome();
}

/* 事件：一律委托，DOM 是每次刷新生成的 */
el('rail').addEventListener('click', (e) => {
  const b = e.target.closest('button');
  if (!b) return;
  if (b.dataset.drawer) toggleDrawer(b.dataset.drawer);
  else gotoStep(Number(b.dataset.i));
});
el('prev').addEventListener('click', () => gotoStep(state.step - 1));
el('next').addEventListener('click', () => gotoStep(state.step + 1));
el('layers').addEventListener('change', (e) => {
  const t = e.target;
  if (t.dataset.layer) { state.over[t.dataset.layer] = t.checked; refresh(true); }
});
el('layers').addEventListener('input', (e) => { if (e.target.id === 'kSlider') setK(Number(e.target.value)); });
el('layers').addEventListener('click', (e) => {
  const b = e.target.closest('button[data-n]');
  if (b) setN(Number(b.dataset.n));
});
el('data').addEventListener('click', (e) => {
  const row = e.target.closest('tr[data-sel]');
  if (row) { select(Number(row.dataset.sel)); return; }
  const axis = e.target.closest('tr[data-n]');
  if (axis) setN(Number(axis.dataset.n));
});

const AXIS_STEP = { 1: 1, 2: 2, 3: 3, 4: 4, 6: 5 };   // 数字键 → 讲这根反轴的那一步
addEventListener('keydown', (e) => {
  if (e.key === 'ArrowRight' || e.key === ' ' || e.key === 'PageDown') { gotoStep(state.step + 1); e.preventDefault(); }
  else if (e.key === 'ArrowLeft' || e.key === 'PageUp') { gotoStep(state.step - 1); e.preventDefault(); }
  else if (/^[1-6]$/.test(e.key) && AXIS_STEP[e.key] !== undefined) gotoStep(AXIS_STEP[e.key]);
  else if (e.key === 'l' || e.key === 'L') toggleDrawer('layers');
  else if (e.key === 'd' || e.key === 'D') toggleDrawer('data');
  else if (e.key === 'f' || e.key === 'F') { state.clean = !state.clean; document.body.classList.toggle('clean', state.clean); }
  else if (e.key === 'h' || e.key === 'H') toggleDrawer('layers');
  else if (e.key === 'Escape') { state.sel = null; refresh(false); }
});

/* =========================================================
   播放：每步停 9 秒，够把一句话说完
   ========================================================= */
const PLAY_MS = 9000;
let playedAt = performance.now();
function tickPlay(now) {
  if (!state.playing) return;
  if (now - playedAt < PLAY_MS) return;
  playedAt = now;
  if (state.step >= STEPS.length - 1) { state.playing = false; return; }
  gotoStep(state.step + 1);
}
el('capText').addEventListener('click', () => { state.playing = !state.playing; playedAt = performance.now(); });

refresh(false);
startLoop({
  renderer, scene, camera, controls, dirty,
  onFrame: (now) => {
    tickPlay(now);
    const moved = tickAnim(now);
    return appear.tick(now) || moved;
  },
});
