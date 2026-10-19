// 六方密堆积 HCP：从密置层堆出六方晶胞，并把堆积率的推导逐行代入数字。
import * as THREE from 'three';
import {
  A, v,
  IDEAL_CA, hcpAtoms, hcpLattice, hcpDistances, hcpRadius, hcpPackingFraction,
  hcpTetra, hexPrismVolume, HCP_SHARE, HCP_METALS, sphereVolume, nearestNeighborPairs, hcpSharePlanes,
} from './geometry.js';
import { HCP_LAYERS, hcpSteps } from './hcp-steps.js';
import {
  makeStage, startLoop, clearGroup, cachedObject, makeAppear, installFilmAPI,
  unitSphere, stdMat, lineMat, labelSprite, tubeEdges,
} from './view.js';

const el = (id) => document.getElementById(id);

const COL = {
  layerA: 0x6ea8fe,
  layerB: 0x4ade80,
  hollowUsed: 0xfb7185,
  hollowFree: 0x64748b,
  prism: 0xcbd5e1,
  bond: 0x9fb0cf,
  tetra: 0xfbbf24,
  guide: 0x22d3ee,
  baseLine: 0x67e8f9,
  axis: 0x94a3b8,
  cloud: 0x9fb0cf,
  ghost: 0x5b6b8c,
  core: 0x22d3ee,
  nbr: 0x93c5fd,
};

/* =========================================================
   状态
   ========================================================= */
const state = {
  preset: 'ideal',
  ca: IDEAL_CA,
  demo: false,
  step: 0,
  playing: false,
  free: {
    ...HCP_LAYERS,
    layerA: true, layerB: true, layerA2: true, prism: true, bonds: true, cloud: true, labels: true,
  },
};

const params = new URLSearchParams(location.search);
const P = (k) => params.get(k);
const PRESETS = Object.fromEntries(HCP_METALS.map((m) => [m.key.toUpperCase(), m.key]));
const want = (P('m') || '').toUpperCase();
if (PRESETS[want]) {
  state.preset = PRESETS[want];
  const metal = HCP_METALS.find((m) => m.key === state.preset);
  if (metal.a) state.ca = metal.c / metal.a;
}
if (params.has('ca')) {
  state.ca = Math.min(1.95, Math.max(1.45, parseFloat(P('ca')) || IDEAL_CA));
  state.preset = 'custom';
}
if (params.has('demo')) state.demo = P('demo') !== '0';
if (params.has('step')) state.step = Math.max(0, (parseInt(P('step'), 10) || 1) - 1);

/** 当前几何：显示单位里 a = A，c = ca·a；刚球半径由“最先相切的那组邻居”决定 */
function geom() {
  const a = A;
  const c = a * state.ca;
  return { a, c, R: hcpRadius(a, state.ca), d: hcpDistances(a, state.ca), atoms: hcpAtoms(a, state.ca) };
}

function byLayer(atoms, c) {
  const at = (z) => (x) => Math.abs(x.pos.z - z) < c * 0.01;
  return { A: atoms.filter(at(0)), B: atoms.filter(at(c / 2)), A2: atoms.filter(at(c)) };
}

/* =========================================================
   图层
   ========================================================= */
const q3 = (x) => x.toFixed(3);
const planeCache = new Map();

/**
 * 份额裁剪面：直接用 geometry.js 的 hcpSharePlanes（单一来源），这里只把它变成 THREE.Plane。
 * 侧面到轴心的距离是六边形的内切距 √3a/2 —— 写成 a/2 会把 1/6 切成别的比例，
 * test/verify.js 用蒙特卡洛体积守住这件事。
 */
function sharePlanesHCP(item, a, c) {
  const spec = hcpSharePlanes(item, a, c / a);
  const key = spec.map((s) => `${s.n.map((x) => x.toFixed(3)).join(',')}@${s.d.toFixed(3)}`).join(';');
  const hit = planeCache.get(key);
  if (hit) return hit;
  const out = spec.map((s) => new THREE.Plane(new THREE.Vector3(s.n[0], s.n[1], s.n[2]), s.d));
  planeCache.set(key, out);
  return out;
}

/** 线段一律用细圆柱画：1 像素的 LineSegments 在视频里会被球完全挡住 */
function lineSet(segments, color, key, opacity = 0.9, radius = 0.018) {
  return tubeEdges(key, segments, radius, color, opacity);
}

/** 一组裁剪面的唯一标识（不同裁剪面不能共用同一个材质） */
const clipKey = (clip) => clip.map((p) =>
  `${p.normal.x.toFixed(2)},${p.normal.y.toFixed(2)},${p.normal.z.toFixed(2)},${p.constant.toFixed(2)}`).join(';');

function atomLayer(items, color, R, share, tag, a, c) {
  const g = new THREE.Group();
  g.name = 'atoms';
  for (const item of items) {
    const clip = share ? sharePlanesHCP(item, a, c) : [];
    const mesh = new THREE.Mesh(unitSphere(), stdMat(`atom|${tag}|${color}|${clipKey(clip)}`, {
      color, roughness: 0.42, metalness: 0.15,
      side: clip.length ? THREE.DoubleSide : THREE.FrontSide,
      clippingPlanes: clip,
    }));
    mesh.position.copy(item.pos);
    mesh.scale.setScalar(R);
    mesh.userData.grow = true;
    g.add(mesh);

    if (clip.length) {
      const ghost = new THREE.Mesh(unitSphere(), stdMat('ghost', {
        color: COL.ghost, roughness: 0.9, transparent: true, opacity: 0.12, depthWrite: false,
      }));
      ghost.position.copy(item.pos);
      ghost.scale.setScalar(R);
      g.add(ghost);
    }
  }
  return g;
}

function makePrism(a, c) {
  const seg = [];
  const ring = (z) => {
    for (let k = 0; k < 6; k++) {
      const t0 = (k * Math.PI) / 3;
      const t1 = ((k + 1) * Math.PI) / 3;
      seg.push([v(a * Math.cos(t0), a * Math.sin(t0), z), v(a * Math.cos(t1), a * Math.sin(t1), z)]);
    }
  };
  ring(0);
  ring(c);
  for (let k = 0; k < 6; k++) {
    const t = (k * Math.PI) / 3;
    const p = v(a * Math.cos(t), a * Math.sin(t), 0);
    seg.push([p, p.clone().setZ(c)]);
  }
  const g = new THREE.Group();
  g.name = 'prism';
  const frame = lineSet(seg, COL.prism, 'prism', 0.95, 0.026);
  // 棱框穿过相切球的球心，正常深度测试下会被球完全挡住；
  // 线框本身不新增几何信息，所以让它始终画在最前，任何机位都看得见。
  frame.renderOrder = 4;
  frame.material.depthTest = false;
  g.add(frame);
  return g;
}

/** 底面 6 个三角形空隙：交替两种，只有其中 3 个能坐第二层 */
function makeHollows(a, R) {
  const g = new THREE.Group();
  const rHollow = a / Math.sqrt(3);
  for (let k = 0; k < 6; k++) {
    const th = Math.PI / 6 + (k * Math.PI) / 3;
    const used = k % 2 === 0;
    const dot = new THREE.Mesh(unitSphere(16, 12), stdMat(`hollow|${used ? 'used' : 'free'}`, {
      color: used ? COL.hollowUsed : COL.hollowFree,
      emissive: used ? COL.hollowUsed : COL.hollowFree,
      emissiveIntensity: used ? 0.55 : 0.15,
      roughness: 0.5, transparent: true, opacity: used ? 0.95 : 0.5,
    }));
    dot.position.set(rHollow * Math.cos(th), rHollow * Math.sin(th), R * 0.6);
    dot.scale.setScalar(R * (used ? 0.3 : 0.22));
    dot.userData.grow = true;
    g.add(dot);
  }
  const lab = labelSprite('尖朝上 / 尖朝下 各占一半', '#fde68a', 380, 64, 0.45);
  lab.position.set(0, -a * 1.35, R * 1.9);
  g.add(lab);
  return g;
}

function makeBonds(items, a, d) {
  const list = items.map((x) => x.pos);
  const { pairs } = nearestNeighborPairs(list, Math.min(a, d.betweenLayers) * 1.05);
  return lineSet(pairs, COL.bond, 'bond', 0.92, 0.02);
}

/** 中心原子 + 完整点阵里数出来的 12 个最近邻 */
function makeCoordination(a, ca, R) {
  const g = new THREE.Group();
  const lat = hcpLattice(a, ca, 2.4 * a);
  const dmin = Math.min(a, hcpDistances(a, ca).betweenLayers);
  const nbrs = lat.filter((p) => p.length() > 1e-9 && p.length() <= dmin * 1.02);

  const core = new THREE.Mesh(unitSphere(), stdMat('core', {
    color: COL.core, emissive: COL.core, emissiveIntensity: 0.4, roughness: 0.35,
  }));
  core.scale.setScalar(R);
  core.userData.grow = true;
  g.add(core);

  for (const p of nbrs) {
    const m = new THREE.Mesh(unitSphere(), stdMat('nbr', {
      color: 0x67e8f9, roughness: 0.45, transparent: true, opacity: 0.42,
      emissive: 0x22d3ee, emissiveIntensity: 0.25,
    }));
    m.position.copy(p);
    m.scale.setScalar(R * 0.88);
    m.userData.grow = true;
    g.add(m);
  }
  g.add(lineSet(nbrs.map((p) => [v(0, 0, 0), p]), COL.guide, 'nbr-link', 0.85, 0.016));
  const lab = labelSprite(`配位数 ${nbrs.length}：同层 6 + 上 3 + 下 3`, '#a5f3fc', 470, 64, 0.5);
  lab.position.set(0, 0, a * 1.3);
  lab.renderOrder = 6;
  lab.material.depthTest = false;      // 标注浮在最前，别埋进球堆
  g.add(lab);
  return g;
}

/** 正四面体：3 个同层球心 + 1 个层间球心，推出 c/a */
function makeTetra(a, ca, R) {
  const g = new THREE.Group();
  const c = a * ca;
  const tri = [v(0, 0, 0), v(a, 0, 0), v(a / 2, (Math.sqrt(3) / 2) * a, 0)];
  const centroid = v(0, 0, 0);
  tri.forEach((p) => centroid.add(p));
  centroid.multiplyScalar(1 / 3);
  const top = hcpAtoms(a, ca).find((x) => x.kind === 'mid' &&
    Math.abs(x.pos.x - centroid.x) < 1e-6 && Math.abs(x.pos.y - centroid.y) < 1e-6).pos;

  const edges = [
    [tri[0], tri[1]], [tri[1], tri[2]], [tri[2], tri[0]],
    [tri[0], top], [tri[1], top], [tri[2], top],
  ];
  g.add(lineSet(edges, COL.tetra, 'tetra-edge', 0.98, 0.022));
  g.add(lineSet([[top, centroid]], COL.guide, 'tetra-height', 0.95, 0.018));           // 层间距
  g.add(lineSet([[centroid, tri[0]]], COL.baseLine, 'tetra-base', 0.9, 0.016));        // a/√3
  g.add(lineSet([[v(0, 0, 0), v(0, 0, c)]], COL.axis, 'c-axis', 0.6, 0.014));          // c 轴

  for (const p of [...tri, top]) {
    const m = new THREE.Mesh(unitSphere(), stdMat('tetra-atom', {
      color: COL.tetra, roughness: 0.4, transparent: true, opacity: 0.92,
    }));
    m.position.copy(p);
    m.scale.setScalar(R * 0.45);
    m.userData.grow = true;
    g.add(m);
  }

  const t = hcpTetra(a);
  const float = (sprite) => { sprite.renderOrder = 6; sprite.material.depthTest = false; return sprite; };
  const labA = labelSprite('a', '#fde68a', 64, 64, 0.45);
  labA.position.copy(tri[1].clone().lerp(top, 0.5)).add(v(0.3, 0, 0.1));
  g.add(float(labA));
  const labH = labelSprite(`层间距 = √(2/3)·a = ${t.height.toFixed(2)}`, '#7dd3fc', 430, 64, 0.42);
  labH.position.copy(top.clone().lerp(centroid, 0.5)).add(v(0.9, 0, 0));
  g.add(float(labH));
  const labR = labelSprite('a/√3', '#7dd3fc', 110, 56, 0.42);
  labR.position.copy(centroid.clone().lerp(tri[0], 0.5)).add(v(0, -0.32, 0));
  g.add(float(labR));
  const labC = labelSprite(`c = 2 × 层间距 → c/a = ${ca.toFixed(3)}（理想 ${IDEAL_CA.toFixed(3)}）`,
    '#a5f3c4', 620, 64, 0.42);
  labC.position.set(-a * 0.1, -a * 1.3, c * 0.75);
  g.add(float(labC));
  return g;
}

function makeCloud(a, ca, maxR) {
  const pts = hcpLattice(a, ca, maxR);
  const R = hcpRadius(a, ca);
  const inst = new THREE.InstancedMesh(unitSphere(12, 10), stdMat('cloud', {
    color: COL.cloud, roughness: 0.6, transparent: true, opacity: 0.3,
  }), pts.length);
  const dummy = new THREE.Object3D();
  pts.forEach((p, i) => {
    dummy.position.copy(p);
    dummy.scale.setScalar(R * 0.5);
    dummy.updateMatrix();
    inst.setMatrixAt(i, dummy.matrix);
  });
  inst.instanceMatrix.needsUpdate = true;
  inst.userData.noFrame = true;       // 背景点云不参与自动取景
  return inst;
}

/** 层名标签 A / B / A，以及份额标签 1/6 · 1/2 · 1 */
function makeLabels(a, c, R, share) {
  const g = new THREE.Group();
  if (!share) {
    for (const [name, z, color] of [['A', 0, '#9ec3ff'], ['B', c / 2, '#a5f3c4'], ['A', c, '#9ec3ff']]) {
      const s = labelSprite(name, color, 64, 64, 0.66);
      s.position.set(-a * 1.3, -a * 1.3, z);
      g.add(s);
    }
    const seq = labelSprite('ABAB…  =  六方密堆积', '#e2e8f0', 420, 64, 0.42);
    seq.position.set(a * 0.9, -a * 1.45, c * 1.12);
    g.add(seq);
  } else {
    const css = { corner: '#c7d2fe', base: '#fde68a', mid: '#a5f3c4' };
    const text = (f) => (f === 1 ? '1' : f === 1 / 2 ? '1/2' : '1/6');
    for (const item of hcpAtoms(a, state.ca)) {
      const dir = item.kind === 'mid' ? v(0, 0, 1) : v(item.pos.x, item.pos.y, 0);
      if (dir.lengthSq() < 1e-9) dir.set(1, 0, 0);
      dir.normalize();
      const lab = labelSprite(text(HCP_SHARE[item.kind]), css[item.kind], 96, 56, 0.42);
      lab.position.copy(item.pos).addScaledVector(dir, R * 1.5 + 0.12);
      g.add(lab);
    }
    const sum = labelSprite('12×(1/6) + 2×(1/2) + 3×1 = 6', '#e2e8f0', 470, 64, 0.45);
    sum.position.set(0, 0, c + R * 1.6);
    g.add(sum);
  }
  return g;
}

/* =========================================================
   舞台 / 渲染
   ========================================================= */
const { renderer, scene, camera, controls, root, dirty } = makeStage({ canvasId: 'scene', fogDensity: 0.013 });
const appear = makeAppear();
let shownObjects = new Set();      // 上一步挂着的图层对象，用于只动画新出现的部分
camera.position.set(6.4, 5.6, 7.6);

function currentLayers() {
  if (!state.demo) return { ...HCP_LAYERS, ...state.free };
  const steps = hcpSteps();
  return { ...HCP_LAYERS, ...steps[Math.min(state.step, steps.length - 1)].layers };
}

function rebuild(animate = false) {
  clearGroup(root);
  const L = currentLayers();
  const { a, c, R, d, atoms } = geom();
  const layers = byLayer(atoms, c);
  const k = q3(state.ca);

  const added = [];
  const add = (key, on, make) => {
    if (!on) return;
    const o = cachedObject(key, make);
    added.push(o);
    root.add(o);
  };

  add(`cloud|${k}`, L.cloud, () => makeCloud(a, state.ca, 2.6 * a));
  add(`prism|${k}`, L.prism || L.share, () => makePrism(a, c));
  add(`hollows|${k}`, L.hollows, () => makeHollows(a, R));
  add(`tetra|${k}`, L.tetra, () => makeTetra(a, state.ca, R));
  add(`bonds|${k}`, L.bonds, () => makeBonds([...layers.A, ...layers.B, ...layers.A2], a, d));
  add(`nn|${k}`, L.nn, () => makeCoordination(a, state.ca, R));

  // 画辅助结构（四面体、配位高亮、键）的那几步把球收小：
  // 键连的是相切球的球心，球满尺寸时键必然整条埋在球里，等于没画。
  const shown = (L.tetra || L.nn || L.bonds) && !L.share ? R * 0.34 : R;
  const rk = q3(shown);
  if (L.share) {
    add(`shareA|${k}|${rk}`, true, () => atomLayer(layers.A, COL.layerA, shown, true, 'A', a, c));
    add(`shareA2|${k}|${rk}`, true, () => atomLayer(layers.A2, COL.layerA, shown, true, 'A2', a, c));
    add(`shareB|${k}|${rk}`, true, () => atomLayer(layers.B, COL.layerB, shown, false, 'B', a, c));
    add(`shareLabels|${k}|${rk}`, true, () => makeLabels(a, c, shown, true));
  } else {
    add(`layerA|${k}|${rk}`, L.layerA, () => atomLayer(layers.A, COL.layerA, shown, false, 'A', a, c));
    add(`layerB|${k}|${rk}`, L.layerB, () => atomLayer(layers.B, COL.layerB, shown, false, 'B', a, c));
    add(`layerA2|${k}|${rk}`, L.layerA2, () => atomLayer(layers.A2, COL.layerA, shown, false, 'A2', a, c));
    add(`labels|${k}|${rk}`, L.labels, () => makeLabels(a, c, shown, false));
  }

  root.position.z = -c / 2;                 // 让棱柱绕自己的中心旋转
  // 只让这一步新出现的图层做生长/淡入，已经在那儿的保持不动
  const fresh = added.filter((o) => !shownObjects.has(o));
  shownObjects = new Set(added);
  if (animate) appear.begin(root, fresh); else appear.settle();
  dirty.set();
  refreshPanels();
}

/* =========================================================
   面板：堆积率逐行代入
   ========================================================= */
let lastPanelKey = '';

function refreshPanels() {
  const L = currentLayers();
  const { a, c, R, d } = geom();
  const metal = HCP_METALS.find((m) => m.key === state.preset) || HCP_METALS[0];
  const realA = metal.a || 3.209;
  const toA = realA / a;                     // 显示单位 → Å（以该金属的 a 为标尺）

  el('caReadout').textContent = `c/a = ${state.ca.toFixed(4)}`;
  el('caNote').textContent = Math.abs(state.ca - IDEAL_CA) < 0.004
    ? '理想轴比：层内、层间同时相切，堆积率取到上限'
    : state.ca > IDEAL_CA
      ? '偏长：层内先相切，层间还留着一道缝'
      : '偏扁：层间先相切，球被挤进相邻层之间';

  const apf = hcpPackingFraction(a, state.ca);
  const ideal = Math.PI / (3 * Math.SQRT2);
  const rows = [
    ['晶格常数 a（同层最近邻）', `${(a * toA).toFixed(3)} Å`],
    ['c = (c/a)·a', `${(c * toA).toFixed(3)} Å`],
    ['层间球心距离 √(a²/3 + c²/4)', `${(d.betweenLayers * toA).toFixed(3)} Å`],
    ['刚球半径 R（最先相切决定）', `${(R * toA).toFixed(3)} Å`],
    ['本胞原子数', '12×(1/6) + 2×(1/2) + 3×1 = <b>6</b>'],
    ['6 个球体积 6·(4/3)πR³', `${(6 * sphereVolume(R * toA)).toFixed(2)} Å³`],
    ['棱柱体积 (3√3/2)a²c', `${(hexPrismVolume(a * toA, state.ca)).toFixed(2)} Å³`],
    ['堆积率 APF', `<b>${(apf * 100).toFixed(2)}%</b>`],
    ['理想上限 π/(3√2) = √2π/6', `${(ideal * 100).toFixed(2)}%（与 FCC 同一个数）`],
  ];
  el('rows').innerHTML = rows.map(([kk, val]) =>
    `<div class="row"><span class="k">${kk}</span><span class="val">${val}</span></div>`).join('');
  el('materialNote').textContent = metal.note;

  const legend = el('nnLegend');
  const legendKey = `${L.tetra ? 1 : 0}${L.nn ? 1 : 0}`;
  legend.style.display = legendKey === '00' ? 'none' : 'block';
  if (legendKey !== '00') {
    legend.innerHTML = '<div class="legend-title">这一层在画什么</div>' +
      (L.tetra ? '<div class="lg"><i style="background:#fbbf24"></i>正四面体：4 个相切球心，棱长 a</div>' +
        '<div class="lg"><i style="background:#22d3ee"></i>层间距 = √(2/3)·a；c = 2 倍层间距</div>' : '') +
      (L.nn ? '<div class="lg"><i style="background:#93c5fd"></i>中心原子的 12 个最近邻（同层 6 + 上 3 + 下 3）</div>' : '');
  }

  const steps = hcpSteps();
  const i = Math.min(state.step, steps.length - 1);
  const s = steps[i];
  const panelKey = `${state.demo}|${i}|${state.playing}|${state.preset}`;
  if (panelKey === lastPanelKey) return;
  lastPanelKey = panelKey;

  el('stageBadge').textContent = state.demo
    ? `第 ${i + 1}/${steps.length} 步 · ${s.title}` : '拖拽旋转 · 滚轮缩放 · 右键平移';
  el('stepPanel').style.display = state.demo ? 'block' : 'none';
  el('hint').style.display = state.demo ? 'none' : 'block';
  el('stepTitle').textContent = `${i + 1} / ${steps.length}　${s.title}`;
  el('stepText').textContent = s.text;
  el('stepDots').innerHTML = steps.map((st, kk) =>
    `<button class="dot-btn${kk === i ? ' on' : ''}" data-i="${kk}" title="${kk + 1}. ${st.title}">${kk + 1}</button>`).join('');
  el('stepPrev').disabled = !state.demo || i === 0;
  el('stepNext').disabled = !state.demo || i === steps.length - 1;
  el('stepPlay').textContent = state.playing ? '❚❚ 暂停' : '▶ 自动播放';
  el('stepPlay').classList.toggle('on', state.playing);
  el('tDemo').checked = state.demo;
  document.querySelectorAll('.manual-only').forEach((nd) => nd.classList.toggle('dimmed', state.demo));
  document.querySelectorAll('#matSel button').forEach((b) =>
    b.classList.toggle('active', b.dataset.m === state.preset));
}

/* =========================================================
   交互
   ========================================================= */
let stepShownAt = 0;

function setStep(i) {
  state.step = Math.max(0, Math.min(hcpSteps().length - 1, i));
  stepShownAt = performance.now();
  rebuild(true);
}

function pickPreset(key) {
  const metal = HCP_METALS.find((m) => m.key === key);
  state.preset = key;
  state.ca = metal && metal.a ? metal.c / metal.a : IDEAL_CA;
  el('caSlider').value = String(state.ca);
  el('caSliderVal').textContent = state.ca.toFixed(3);
  rebuild(state.demo);
}

document.querySelectorAll('#matSel button').forEach((b) =>
  b.addEventListener('click', () => pickPreset(b.dataset.m)));

el('tDemo').addEventListener('change', () => {
  state.demo = el('tDemo').checked;
  if (!state.demo) state.playing = false;
  if (state.demo) setStep(0); else rebuild();
});
el('stepPrev').addEventListener('click', () => setStep(state.step - 1));
el('stepNext').addEventListener('click', () => setStep(state.step + 1));
el('stepDots').addEventListener('click', (e) => {
  const b = e.target.closest('button');
  if (b) setStep(Number(b.dataset.i));
});
el('stepPlay').addEventListener('click', () => {
  state.playing = !state.playing;
  stepShownAt = performance.now();
  lastPanelKey = '';
  refreshPanels();
});

const STEP_MS = 5400;
function tickPlay(now) {
  if (!state.demo || !state.playing) return;
  if (now - stepShownAt < STEP_MS) return;
  if (state.step >= hcpSteps().length - 1) {
    state.playing = false;
    lastPanelKey = '';
    refreshPanels();
    return;
  }
  setStep(state.step + 1);
}

const caSlider = el('caSlider');
caSlider.value = String(state.ca);
el('caSliderVal').textContent = state.ca.toFixed(3);
caSlider.addEventListener('input', () => {
  state.ca = Math.min(1.95, Math.max(1.45, parseFloat(caSlider.value)));
  state.preset = 'custom';
  el('caSliderVal').textContent = state.ca.toFixed(3);
  lastPanelKey = '';
  rebuild(state.demo);
});

el('idealBtn').addEventListener('click', () => pickPreset('ideal'));

for (const [id, key] of Object.entries({
  hA: 'layerA', hB: 'layerB', hA2: 'layerA2', hHollow: 'hollows', hPrism: 'prism',
  hShare: 'share', hBonds: 'bonds', hNN: 'nn', hTetra: 'tetra', hCloud: 'cloud', hLabels: 'labels',
})) {
  const box = el(id);
  box.checked = state.free[key];
  box.addEventListener('change', () => {
    state.free[key] = box.checked;
    rebuild();
  });
}

el('hRotate').addEventListener('change', (e) => { controls.autoRotate = e.target.checked; });

el('topView').addEventListener('click', () => {
  camera.position.set(0.01, 0.01, 13);
  controls.target.set(0, 0, 0);
  controls.update();
});
el('sideView').addEventListener('click', () => {
  camera.position.set(12, 0.01, 0.5);
  controls.target.set(0, 0, 0);
  controls.update();
});
el('resetView').addEventListener('click', () => {
  camera.position.set(6.4, 5.6, 7.6);
  controls.target.set(0, 0, 0);
  controls.update();
});

addEventListener('keydown', (e) => {
  if (!state.demo) return;
  if (e.key === 'ArrowRight') { setStep(state.step + 1); e.preventDefault(); }
  else if (e.key === 'ArrowLeft') { setStep(state.step - 1); e.preventDefault(); }
  else if (e.key === ' ') {
    state.playing = !state.playing;
    stepShownAt = performance.now();
    lastPanelKey = '';
    refreshPanels();
    e.preventDefault();
  }
});

rebuild();
startLoop({
  renderer, scene, camera, controls, dirty,
  onFrame: (now) => { tickPlay(now); return appear.tick(now); },
});

/* 摄制模式：?film=1 暴露 window.__film.set({step, ca, cam}) */
if (params.has('film')) {
  installFilmAPI({
    camera, controls, renderer, scene, root,
    meta: { steps: hcpSteps().length, titles: hcpSteps().map((s) => s.title), idealCa: IDEAL_CA },
    apply: (p) => {
      if (typeof p.ca === 'number') state.ca = p.ca;
      state.demo = p.demo === undefined ? true : !!p.demo;
      if (typeof p.step === 'number') state.step = p.step;
      rebuild(false);
      appear.settle();
    },
  });
}
