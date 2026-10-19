// 金刚石 / 闪锌矿：两套 FCC 沿体对角线 [111] 错开 ¼ 的平移演示
// 性能取向：场景对象一次建好，平移只改位置/缩放/缓冲区，不重新分配 GPU 资源。
import * as THREE from 'three';
import {
  A, HALF, v,
  atomsFor, fccBasis, shiftedBasis, basisDistance, pairsWithin, sphereVolume, fccAtomSet,
  fractionalOf,
} from './geometry.js';
import {
  makeStage, startLoop, unitSphere, stdMat, lineMat, labelSprite, sharePlanes, installFilmAPI,
  tubeEdges, updateTubes,
} from './view.js';

const el = (id) => document.getElementById(id);

/* =========================================================
   材料数据（真实晶格常数与共价/离子半径，单位 Å）
   ========================================================= */
const MATERIALS = {
  C: {
    label: '金刚石 C', space: 'Fd-3m', a: 3.567, kind: 'diamond',
    nameA: 'C（顶点 + 面心）', nameB: 'C（体内 4 个）',
    rA: 0.77, rB: 0.77, colA: 0x8fa6c4, colB: 0xdce7f7,
    note: '共价半径 0.77 Å：两个碳沿键方向正好接触，2r = 1.54 Å = √3a/4。',
  },
  Si: {
    label: '晶体 Si', space: 'Fd-3m', a: 5.431, kind: 'diamond',
    nameA: 'Si（顶点 + 面心）', nameB: 'Si（体内 4 个）',
    rA: 1.17, rB: 1.17, colA: 0x9f7aea, colB: 0xd8c4fb,
    note: 'Ge（a = 5.658 Å，r = 1.22 Å）同样是金刚石型结构，IV 族半导体都靠这套骨架。',
  },
  Zn: {
    label: '闪锌矿 ZnS', space: 'F-43m', a: 5.406, kind: 'zincblende',
    nameA: 'S²⁻（顶点 + 面心）', nameB: 'Zn²⁺（体内 4 个）',
    rA: 1.70, rB: 0.74, colA: 0xfacc15, colB: 0xa78bfa,
    note: '两套亚点阵换成不同离子：S²⁻（1.70 Å）做面心立方骨架，Zn²⁺（0.74 Å）填进一半的四面体空隙。Zn–S 键长 2.34 Å，离子半径之和 2.44 Å 略大，属经验半径的正常偏差。',
  },
};

/* =========================================================
   场景
   ========================================================= */
const { renderer, scene, camera, controls, root, dirty } = makeStage({ canvasId: 'scene' });

const aCell = atomsFor('FCC').map((x) => x.pos);        // 顶点 8 + 面心 6
const bBase = shiftedBasis(0, A);                       // B 亚点阵的 4 个基元（t=0 参考）
const Z_AXIS = new THREE.Vector3(0, 0, 1);
const DIR111 = v(1, 1, 1).normalize();
const CORNER = v(-HALF, -HALF, -HALF);
const B_MAX_BONDS = 64;                                  // 一条线 = 2 个顶点

/* 状态 */
const state = {
  mat: 'C',
  t: 0.25,
  cell: true, showA: true, showB: true, bonds: true,
  diag: true, cloud: true, share: false,
  range: 2, autoRotate: false,
};

const params = new URLSearchParams(location.search);
const P = (k) => params.get(k);
// 材料代号大小写不敏感：?m=zn / ?m=Zn / ?m=Si 都能认
const KEY_OF = Object.fromEntries(Object.keys(MATERIALS).map((k) => [k.toUpperCase(), k]));
if (KEY_OF[(P('m') || '').toUpperCase()]) state.mat = KEY_OF[P('m').toUpperCase()];
if (params.has('t')) state.t = Math.min(0.5, Math.max(0, parseFloat(P('t')) || 0));
for (const [q, k] of [['cell', 'cell'], ['a', 'showA'], ['b', 'showB'], ['bonds', 'bonds'],
  ['diag', 'diag'], ['cloud', 'cloud'], ['share', 'share'], ['rot', 'autoRotate']]) {
  if (params.has(q)) state[k] = P(q) !== '0';
}
if (params.has('range')) state.range = Math.min(3, Math.max(1, parseFloat(P('range')) || 2));

/** 当前 t 下位于本胞内的 B 亚点阵原子（4 个） */
const bPositions = (t) => shiftedBasis(t, A).map((p) => p.clone().sub(v(HALF, HALF, HALF)));

/** 真实半径 → 显示尺寸：r_显示 = (r_真实 / a) × A */
function radii() {
  const m = MATERIALS[state.mat];
  return { rA: (m.rA / m.a) * A, rB: (m.rB / m.a) * A };
}

/* =========================================================
   一次建好的场景对象
   ========================================================= */
// 棱框/键/对角线一律用细圆柱：1 像素的 LineSegments 在视频里会被球完全挡住
const cubeEdges = [];
for (const a of [-HALF, HALF]) for (const b of [-HALF, HALF]) {
  cubeEdges.push([v(a, b, -HALF), v(a, b, HALF)]);
  cubeEdges.push([v(a, -HALF, b), v(a, HALF, b)]);
  cubeEdges.push([v(-HALF, a, b), v(HALF, a, b)]);
}
const cell = tubeEdges('cell', cubeEdges, 0.022, 0xcbd5e1, 0.95);

/** A 亚点阵：顶点 + 面心；share 模式下裁成本胞内的份额 */
const groupA = new THREE.Group();
const meshA = aCell.map((pos) => {
  const m = new THREE.Mesh(unitSphere(), stdMat('plain', { roughness: 0.42, metalness: 0.15 }));
  m.position.copy(pos);
  groupA.add(m);
  return m;
});
const groupAGhost = new THREE.Group();
const ghostA = aCell.map((pos) => {
  const m = new THREE.Mesh(unitSphere(), stdMat('ghost', {
    color: 0x5b6b8c, roughness: 0.9, transparent: true, opacity: 0.13, depthWrite: false,
  }));
  m.position.copy(pos);
  m.visible = false;
  groupAGhost.add(m);
  return m;
});

/** B 亚点阵：体内 4 个原子 */
const groupB = new THREE.Group();
const meshB = bBase.map(() => {
  const m = new THREE.Mesh(unitSphere(), stdMat('plain', { roughness: 0.42, metalness: 0.15 }));
  groupB.add(m);
  return m;
});

/** 最近邻键（A–B）：容量固定，逐帧只改实例矩阵 */
const bonds = tubeEdges('bond',
  Array.from({ length: B_MAX_BONDS }, () => [v(0, 0, 0), v(0, 0, 0)]), 0.022, 0xa8b6cc, 0.95);

/** 体对角线参考 */
const diag = new THREE.Group();
diag.add(tubeEdges('diag', [[CORNER, v(HALF, HALF, HALF)]], 0.014, 0x94a3b8, 0.6));
const quarterTick = new THREE.Mesh(unitSphere(16, 12), stdMat('tick', {
  color: 0x22d3ee, emissive: 0x22d3ee, emissiveIntensity: 0.7,
}));
quarterTick.scale.setScalar(0.08);
quarterTick.position.copy(CORNER).addScaledVector(DIR111, basisDistance(0.25, A));
diag.add(quarterTick);
const quarterLabel = labelSprite('¼ a', '#7dd3fc', 96, 56, 0.5);
quarterLabel.position.copy(quarterTick.position).add(v(0, 0.34, 0));
diag.add(quarterLabel);
const shiftArrow = new THREE.ArrowHelper(DIR111, CORNER, 0.001, 0x22d3ee, 0.16, 0.09);
diag.add(shiftArrow);
const shiftLabel = labelSprite('|Δ| = √3·t·a', '#67e8f9', 220, 56, 0.42);
diag.add(shiftLabel);

/** 两套亚点阵铺出去的点云（InstancedMesh，容量按最大范围预留） */
const CLOUD_MAX = fccAtomSet(3 * A).length;
const cloudA = new THREE.InstancedMesh(unitSphere(12, 10), stdMat('cloudA', {
  color: 0x9fb0cf, roughness: 0.6, transparent: true, opacity: 0.4,
}), CLOUD_MAX);
const cloudB = new THREE.InstancedMesh(unitSphere(12, 10), stdMat('cloudB', {
  color: 0x67e8f9, roughness: 0.6, transparent: true, opacity: 0.38,
}), CLOUD_MAX);
const dummy = new THREE.Object3D();

cloudA.userData.noFrame = cloudB.userData.noFrame = true;   // 背景点云不参与自动取景
root.add(cloudA, cloudB, cell, diag, bonds, groupAGhost, groupA, groupB);

/* =========================================================
   更新：平移 / 换材料 / 图层开关
   ========================================================= */
function applyT() {
  const t = state.t;
  const b = bPositions(t);
  meshB.forEach((m, i) => m.position.copy(b[i]));

  // 点云 B：整支亚点阵沿 [111] 平移
  const shift = v(t, t, t).multiplyScalar(A);
  const pts = fccAtomSet(state.range * A);
  cloudA.count = cloudB.count = pts.length;
  pts.forEach((p, i) => {
    dummy.position.copy(p);
    dummy.scale.setScalar(0.062);
    dummy.updateMatrix();
    cloudA.setMatrixAt(i, dummy.matrix);
    dummy.position.add(shift);
    dummy.updateMatrix();
    cloudB.setMatrixAt(i, dummy.matrix);
  });
  cloudA.instanceMatrix.needsUpdate = true;
  cloudB.instanceMatrix.needsUpdate = true;

  // 平移矢量与标签
  const len = basisDistance(t, A);
  shiftArrowLength(len);
  shiftLabel.position.copy(CORNER).addScaledVector(DIR111, Math.max(len, 0.001)).add(v(0, 0.34, 0));
  shiftLabel.visible = state.diag && len > 1e-3;

  // 最近邻键
  const list = state.bonds && t > 0.08 ? pairsWithin(aCell, b, 0.55 * A).bonds : [];
  updateTubes(bonds, list, 0.022);

  dirty.set();
}

function shiftArrowLength(len) {
  shiftArrow.setLength(Math.max(len, 0.001), 0.16, 0.09);
}

function applyMaterial() {
  const m = MATERIALS[state.mat];
  const { rA, rB } = radii();
  const matA = stdMat(`atom|${state.mat}A`, { color: m.colA, roughness: 0.42, metalness: 0.15 });
  const matB = stdMat(`atom|${state.mat}B`, { color: m.colB, roughness: 0.42, metalness: 0.15 });
  meshA.forEach((mesh) => { mesh.material = matA; mesh.scale.setScalar(rA); });
  meshB.forEach((mesh) => { mesh.material = matB; mesh.scale.setScalar(rB); });
  ghostA.forEach((mesh) => mesh.scale.setScalar(rA));
  applyShare();
  refreshInfo();
  dirty.set();
}

/** 份额视图：给 A 的原子挂上裁剪面，并显示虚影 */
function applyShare() {
  const on = state.share;
  groupAGhost.visible = on;
  ghostA.forEach((mesh) => { mesh.visible = on; });
  const m = MATERIALS[state.mat];
  meshA.forEach((mesh, i) => {
    const clip = sharePlanes(aCell[i], HALF);
    const sign = [0, 1, 2].map((k) => Math.sign(aCell[i].getComponent(k))).join(',');
    mesh.material = on
      ? stdMat(`share|${state.mat}|${sign}`, {
        color: m.colA, roughness: 0.42, metalness: 0.15,
        side: THREE.DoubleSide, clippingPlanes: clip,
      })
      : stdMat(`atom|${state.mat}A`, { color: m.colA, roughness: 0.42, metalness: 0.15 });
  });
  dirty.set();
}

function applyVisibility() {
  cell.visible = state.cell;
  groupA.visible = state.showA;
  groupAGhost.visible = state.showA && state.share;
  ghostA.forEach((mesh) => { mesh.visible = state.showA && state.share; });
  groupB.visible = state.showB;
  bonds.visible = state.bonds;
  diag.visible = state.diag;
  shiftLabel.visible = state.diag && state.t > 0.002;
  cloudA.visible = state.cloud && state.showA;
  cloudB.visible = state.cloud && state.showB;
  dirty.set();
}

function applyRange() {
  // 点云容量已按最大范围预留，改范围只需重填矩阵并把多余的实例隐藏
  applyT();
}

/* =========================================================
   信息面板
   ========================================================= */
const ang = (x) => `${x.toFixed(3)} Å`;

function fracText(p) {
  return `(${fractionalOf(p).map((c) => c.toFixed(2)).join(', ')})`;
}

let lastInfoKey = '';

function refreshInfo() {
  const m = MATERIALS[state.mat];
  const d = (Math.sqrt(3) / 4) * m.a;
  const shift = Math.sqrt(3) * state.t * m.a;
  const near = (x) => Math.abs(state.t - x) < 0.006;

  // 左侧读数每帧都要动；表格只在换材料时重排
  el('tBig').textContent = `t = ${state.t.toFixed(3)} a`;
  el('tSub').textContent = near(0)
    ? '两套 FCC 完全重合：基元的两个原子叠在同一处，键长为 0'
    : near(0.25)
      ? `到位：|Δ| = √3a/4 = ${ang(d)}，正是最近邻键长`
      : near(0.5)
        ? '两套 FCC 合成一个简单立方：间距 a/2，配位变成 6'
        : state.t < 0.25
          ? `|Δ| = √3·t·a = ${ang(shift)}，离到位还差 ${ang(d - shift)}`
          : `越过 ¼：|Δ| = ${ang(shift)}，键长应为 ${ang(d)}`;

  let capTitle, capText;
  if (near(0)) {
    capTitle = 't = 0　两套 FCC 重合';
    capText = '第二套亚点阵还没移动，基元里的两个原子落在同一个位置。真实晶体不会这样——把其中一个沿体对角线推开，才得到金刚石型结构。';
  } else if (near(0.25)) {
    capTitle = 't = ¼　金刚石型结构';
    capText = '位移正好是 ¼ 条体对角线，长度 √3a/4 等于键长。于是每个原子恰好 4 个近邻、键角 109.47°，成正四面体——这就是金刚石（C、Si、Ge）与闪锌矿（ZnS）的骨架。';
  } else if (near(0.5)) {
    capTitle = 't = ½　合并成简单立方';
    capText = 'B 平移 (½,½,½)a 后，坐标和为奇、偶的两组格点刚好互补，并集成为一个间距 a/2 的简单立方格子：每个原子 6 个近邻、八面体配位，四面体网络消失。';
  } else if (state.t < 0.25) {
    capTitle = '滑动中　球体互相重叠';
    capText = '此时 A–B 最短距离只有 √3·t·a，比键长短，球体彼此穿插。继续推到 t = ¼，才会变成 4 配位的四面体网络。';
  } else {
    capTitle = '越过 ¼　四面体正在瓦解';
    capText = '再往前推，离中心原子最近的不再是那 4 个异种原子；到 t = ½ 时两套 FCC 合并成简单立方（6 配位）。金刚石型结构只在 t = ¼ 出现。';
  }
  el('capTitle').textContent = capTitle;
  el('capText').textContent = capText;
  el('posB').textContent = shiftedBasis(state.t, A).map(fracText).join('  ');

  const infoKey = state.mat;
  if (infoKey === lastInfoKey) return;
  lastInfoKey = infoKey;

  const pack = m.kind === 'zincblende'
    ? (4 * (sphereVolume(m.rA) + sphereVolume(m.rB))) / m.a ** 3
    : (8 * sphereVolume(m.rA)) / m.a ** 3;
  const hex = (c) => `#${c.toString(16).padStart(6, '0')}`;

  el('matName').textContent = m.label;
  el('legendA').innerHTML = `<span class="dot" style="background:${hex(m.colA)}"></span>${m.nameA}`;
  el('legendB').innerHTML = `<span class="dot" style="background:${hex(m.colB)}"></span>${m.nameB}`;

  const rows = [
    ['结构类型', `${m.kind === 'zincblende' ? '闪锌矿 zinc blende' : '金刚石型 diamond'} · ${m.space}`],
    ['晶格常数 a', ang(m.a)],
    ['亚点阵', '两套面心立方，沿 [111] 错开 ¼ 体对角线'],
    ['键长 √3a/4', ang(d)],
    ['同种最近邻 a/√2', ang(m.a / Math.SQRT2)],
    ['每胞原子数', m.kind === 'zincblende' ? '4 个 Zn + 4 个 S = 8' : '4 + 4 = 8 个'],
    ['配位数', '4，正四面体 · 键角 109.47°'],
    ['份额', '顶点 8×1/8 + 面心 6×1/2 = 4，体内 4×1 = 4'],
    ['堆积率（真实半径）', `${(pack * 100).toFixed(1)}%`],
  ];
  el('infoRows').innerHTML = rows
    .map(([k, val]) => `<div class="row"><span class="k">${k}</span><span class="val">${val}</span></div>`)
    .join('');
  el('posA').textContent = fccBasis(A).map(fracText).join('  ');
  el('infoNote').textContent = m.note;

  document.querySelectorAll('#matSel button').forEach((btn) =>
    btn.classList.toggle('active', btn.dataset.m === state.mat)
  );
}

/* =========================================================
   交互
   ========================================================= */
document.querySelectorAll('#matSel button').forEach((btn) => {
  btn.addEventListener('click', () => {
    state.mat = btn.dataset.m;
    applyMaterial();
    applyVisibility();
    applyT();
  });
});

for (const [id, key] of [['sCell', 'cell'], ['sA', 'showA'], ['sB', 'showB'], ['sBonds', 'bonds'],
  ['sDiag', 'diag'], ['sCloud', 'cloud'], ['sShare', 'share']]) {
  const box = el(id);
  box.checked = state[key];
  box.addEventListener('change', () => {
    state[key] = box.checked;
    if (key === 'share') applyShare();
    if (key === 'bonds') applyT();
    applyVisibility();
  });
}

const tSlider = el('tSlider');
tSlider.value = state.t;

function setT(t, fromSlider = false) {
  state.t = Math.min(0.5, Math.max(0, t));
  if (!fromSlider) tSlider.value = String(state.t);
  el('tSliderVal').textContent = state.t.toFixed(3);
  applyT();
  refreshInfo();
}
tSlider.addEventListener('input', () => {
  goal = null;
  setT(parseFloat(tSlider.value), true);
});

// 平移动画：把第二套 FCC 沿体对角线推向 ¼（缓动，到位后停住）
let goal = null;
let goalFrom = 0;
let goalT0 = 0;
const GOAL_MS = 1800;
const easeInOut = (k) => (k < 0.5 ? 4 * k * k * k : 1 - Math.pow(-2 * k + 2, 3) / 2);

function startSlide(to) {
  goalFrom = state.t;
  goal = to;
  goalT0 = performance.now();
}

function stepSlide(now) {
  if (goal === null) return false;
  const k = Math.min(1, (now - goalT0) / GOAL_MS);
  setT(goalFrom + (goal - goalFrom) * easeInOut(k));
  if (k === 1) goal = null;
  return true;
}

el('slideBtn').addEventListener('click', () => {
  if (Math.abs(state.t - 0.25) < 0.005) setT(0);   // 已到位：退回起点再演一遍
  startSlide(0.25);
});
el('zeroBtn').addEventListener('click', () => startSlide(0));
el('quarterBtn').addEventListener('click', () => startSlide(0.25));

el('view111').addEventListener('click', () => {
  camera.position.copy(DIR111.clone().multiplyScalar(9.4));
  controls.target.set(0, 0, 0);
  controls.update();
});

const rangeSlider = el('range');
rangeSlider.value = state.range;
rangeSlider.addEventListener('input', () => {
  state.range = parseFloat(rangeSlider.value);
  el('rangeVal').textContent = state.range.toFixed(1) + 'a';
  applyRange();
});
el('rangeVal').textContent = state.range.toFixed(1) + 'a';

const rot = el('sRotate');
rot.checked = state.autoRotate;
rot.addEventListener('change', () => {
  state.autoRotate = rot.checked;
  controls.autoRotate = state.autoRotate;
});

el('resetView').addEventListener('click', () => {
  camera.position.set(5.4, 4.0, 6.6);
  controls.target.set(0, 0, 0);
  controls.update();
});

addEventListener('keydown', (e) => {
  const step = e.shiftKey ? 0.001 : 0.005;
  if (e.key === 'ArrowRight') { goal = null; setT(state.t + step); e.preventDefault(); }
  else if (e.key === 'ArrowLeft') { goal = null; setT(state.t - step); e.preventDefault(); }
  else if (e.key === 'ArrowUp') { startSlide(0.25); e.preventDefault(); }
  else if (e.key === 'ArrowDown') { startSlide(0); e.preventDefault(); }
});

/* =========================================================
   启动 + 按需渲染
   ========================================================= */
applyMaterial();
applyVisibility();
setT(state.t);
el('tSliderVal').textContent = state.t.toFixed(3);

startLoop({
  renderer, scene, camera, controls, dirty,
  onFrame: (now) => stepSlide(now),
});

/* 摄制模式：?film=1 暴露 window.__film.set({mat, t, cam}) */
if (params.has('film')) {
  installFilmAPI({
    camera, controls, renderer, scene, root,
    meta: { materials: Object.keys(MATERIALS), labels: Object.fromEntries(Object.keys(MATERIALS).map((k) => [k, MATERIALS[k].label])) },
    apply: (p) => {
      if (p.mat && MATERIALS[p.mat]) {
        state.mat = p.mat;
        applyMaterial();
        applyVisibility();
      }
      if (typeof p.t === 'number') setT(Math.min(0.5, Math.max(0, p.t)), true);
      for (const k of ['cell', 'showA', 'showB', 'bonds', 'diag', 'cloud', 'share']) {
        if (k in p) {
          state[k] = !!p[k];
          if (k === 'share') applyShare();
        }
      }
      applyVisibility();
      applyT();
    },
  });
}
