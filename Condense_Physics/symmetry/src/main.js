// 反轴 n̄ 与正四面体 24 个对称操作的可视化。
// 页面分两段：bar（3̄ = 3 + i、4̄ = 4 + i、6̄ = ?）与 td（4̄3m 的 24 个操作）。
// 矩阵、等效点系、顶点置换全部由 src/group.js 现算，这里只负责把结果摆进三维空间。
import * as THREE from 'three';
import {
  ID3, mul, applyM, same, over, dirName,
  barGroup, barOrbit, tdOps, tdClasses,
  TETRA_VERTS, TETRA_FACES, MIRROR_110, animPath, invertStep,
} from './group.js';
import { BAR_LAYERS, TD_LAYERS, stepsOf } from './steps.js';
import {
  makeStage, startLoop, clearGroup, cachedObject, makeAppear, installFilmAPI,
  unitSphere, unitCircle, stdMat, labelSprite, tubeEdges,
} from '../../src/view.js';

const el = (id) => document.getElementById(id);
const clamp = (x, lo, hi) => Math.min(hi, Math.max(lo, x));

const COL = {
  axis: 0x22d3ee,
  motif: 0x22d3ee,
  image: 0xc7d2fe,
  imageImp: 0xfca5a5,
  ghost: 0x64748b,
  pairI: 0xfb7185,
  pairM: 0x4ade80,
  mirror: 0x4ade80,
  center: 0xfb7185,
  cube: 0xcbd5e1,
  tetraEdge: 0x9fb0cf,
  axis4: 0xfbbf24,
  axis3: 0x60a5fa,
  x: 0xf87171,
  y: 0x4ade80,
  z: 0x60a5fa,
  faces: [0x22d3ee, 0xfbbf24, 0xa78bfa, 0xf472b6],
  verts: [0x6ea8fe, 0xfdb454, 0x4ade80, 0xf472b6],
};

/** 一般位置的取样点：三个坐标互不相等、都不为零，才看得出每一步操作 */
const PM = [1.15, 0.45, 0.78];
const TRIAD = 0.42;                    // 手性标架每根棍的长度
const AXIS_LEN = 2.25;
const AXES_100 = [[1, 0, 0], [0, 1, 0], [0, 0, 1]];
const STEP_MS = 7000;                  // 分步演示自动播放的停留时间
const OP_MS = 1900;                    // 依次播放 24 个操作的停留时间
const ANIM_MS = 820;

const fmt = (x) => {
  const r = Math.round(x * 100) / 100;
  return (Object.is(r, -0) ? 0 : r).toFixed(2);
};
const coordText = (p) => `(${fmt(p[0])}, ${fmt(p[1])}, ${fmt(p[2])})`;
/** 面板里的反轴记号走 CSS 上横线；画布贴图里用组合字符 over() */
const barHtml = (n) => `<span class="ov">${n}</span>`;
const v3 = (p) => new THREE.Vector3(p[0], p[1], p[2]);
const OPS = tdOps();
const CLASSES = tdClasses();

/* 图层开关的 id ↔ 图层名。必须在使用之前声明：首次 rebuild() 就会走到面板刷新。 */
const BAR_TOGGLE = {
  bAxis: 'axis', bMotif: 'motif', bOrbit: 'orbit', bCurrent: 'current', bGhost: 'ghost',
  bPairs: 'pairs', bMirror: 'mirror', bCenter: 'center', bNumbers: 'numbers',
};
const TD_TOGGLE = {
  hCube: 'cube', hFaces: 'faces', hEdges: 'edges', hVerts: 'verts', hLabels: 'labels',
  hTriad: 'triad', hGhost: 'ghost', hAxis4: 'axis4', hAxis3: 'axis3', hMirrors: 'mirrors',
};



/* =========================================================
   状态
   ========================================================= */
const state = {
  mode: 'bar',                 // bar = 反轴；td = 四面体的 24 个操作
  demo: false,
  step: 0,
  playing: false,
  n: 4,                        // 当前反轴 n̄
  k: 0,                        // 已经施加的次数（图层 current 用）
  op: 24 - 1,                  // 当前 Td 操作下标（默认选一个 S4，动画最直观）
  cycling: false,
  freeBar: {
    ...BAR_LAYERS, axis: true, motif: true, orbit: true, numbers: true,
    pairs: true, mirror: true, center: true,
  },
  freeTd: {
    ...TD_LAYERS, cube: true, faces: true, edges: true, verts: true,
    labels: true, triad: true, ghost: true,
  },
};

const params = new URLSearchParams(location.search);
const P = (x) => params.get(x);
if (P('mode') === 'td') state.mode = 'td';
if (params.has('n')) state.n = clamp(parseInt(P('n'), 10) || 4, 1, 6);
if (params.has('op')) state.op = clamp(parseInt(P('op'), 10) || 0, 0, OPS.length - 1);
if (params.has('demo')) state.demo = P('demo') !== '0';
if (params.has('step')) state.step = Math.max(0, (parseInt(P('step'), 10) || 1) - 1);

/* =========================================================
   画图小件
   ========================================================= */
/** 一根对称轴：轴线 + 末端的小记号（4 次画菱形、3 次画三角形）+ 文字 */
function axisRod(dir, len, color, key, { marker = 0, label = '', labelColor = '#e2e8f0' } = {}) {
  const g = new THREE.Group();
  const d = v3(dir).normalize();
  g.add(tubeEdges(`axis|${key}`, [[d.clone().multiplyScalar(-len), d.clone().multiplyScalar(len)]], 0.022, color, 0.95));
  if (marker) {
    const disc = new THREE.Mesh(unitCircle(marker), stdMat(`mark|${key}|${marker}`, {
      color, roughness: 0.5, metalness: 0.1, side: THREE.DoubleSide,
    }));
    disc.position.copy(d.clone().multiplyScalar(len));
    disc.lookAt(d.clone().multiplyScalar(2 * len));
    disc.scale.setScalar(0.14);
    disc.userData.grow = true;
    g.add(disc);
  }
  if (label) {
    const s = labelSprite(label, labelColor, 128, 96, 0.6);
    s.position.copy(d.clone().multiplyScalar(len + 0.34));
    g.add(s);
  }
  return g;
}

/** 过原点、法线为 normal 的半透明圆片 + 一圈边 */
function planeDisc(normal, radius, key, color = COL.mirror, opacity = 0.15) {
  const g = new THREE.Group();
  const n = v3(normal).normalize();
  const disc = new THREE.Mesh(unitCircle(44), stdMat(`disc|${key}`, {
    color, roughness: 0.9, transparent: true, opacity, side: THREE.DoubleSide, depthWrite: false,
  }));
  disc.scale.setScalar(radius);
  disc.lookAt(n);
  disc.userData.grow = true;
  g.add(disc);
  const rim = new THREE.Group();
  rim.lookAt(n);
  const segs = [];
  const N = 44;
  for (let i = 0; i < N; i++) {
    const a = (i / N) * Math.PI * 2;
    const b = ((i + 1) / N) * Math.PI * 2;
    segs.push([new THREE.Vector3(radius * Math.cos(a), radius * Math.sin(a), 0),
      new THREE.Vector3(radius * Math.cos(b), radius * Math.sin(b), 0)]);
  }
  rim.add(tubeEdges(`rim|${key}`, segs, 0.011, color, 0.55));
  g.add(rim);
  return g;
}

/** 若干姿态下的「红绿蓝三面角」线段：按颜色分成三组，每组一个 InstancedMesh */
function sticksFor(mats, len, base = PM) {
  const dirs = [[1, 0, 0], [0, 1, 0], [0, 0, 1]];
  const out = [[], [], []];
  for (const M of mats) {
    const p = applyM(M, base);
    dirs.forEach((e, i) => {
      const d = applyM(M, e);
      out[i].push([v3(p), v3(p.map((x, j) => x + d[j] * len))]);
    });
  }
  return out;
}

function sphereAt(p, color, key, r = 0.13, opacity = 1, emissive = 0) {
  const m = new THREE.Mesh(unitSphere(20, 16), stdMat(`sph|${key}`, {
    color, roughness: 0.4, metalness: 0.12,
    emissive: emissive || color, emissiveIntensity: emissive ? 0.5 : 0.12,
    transparent: opacity < 1, opacity,
  }));
  m.position.copy(v3(p));
  m.scale.setScalar(r);
  m.userData.grow = true;
  return m;
}

function spriteIn(node, text, color, w, h, scale, pos) {
  const s = labelSprite(text, color, w, h, scale);
  s.position.copy(v3(pos));
  node.add(s);
  return s;
}

/* =========================================================
   反轴那一段的图层
   ========================================================= */
const genCache = new Map();
const genOf = (n) => {
  if (!genCache.has(n)) genCache.set(n, barGroup(n).gen);
  return genCache.get(n);
};
/** n̄ 连做 k 次（k = 群的阶时正好回到单位阵） */
function powGen(n, k) {
  let M = ID3;
  for (let i = 0; i < k; i++) M = mul(genOf(n), M);
  return M;
}

function barFrame(n) {
  const { props } = barGroup(n);
  const g = new THREE.Group();
  g.add(axisRod([0, 0, 1], AXIS_LEN, COL.axis, `bar${n}`, { label: over(n), labelColor: '#a5f3fc' }));
  // 原点就是反演中心：群里真的有 i 时才高亮并贴标签，没有就只留一个灰点
  g.add(sphereAt([0, 0, 0], props.hasI ? COL.center : 0x475569, `org|${n}|${props.hasI}`, 0.09, props.hasI ? 1 : 0.55));
  if (props.hasI) spriteIn(g, 'i ∈ 群', '#fda4af', 200, 64, 0.4, [0.55, 0.55, -0.42]);
  if (props.hasMirrorPerp) {
    g.add(planeDisc([0, 0, 1], 1.95, `mh${n}`, COL.mirror, 0.16));
    spriteIn(g, 'm⊥ ∈ 群', '#a5f3c4', 200, 64, 0.4, [1.78, 0.52, 0.16]);
  }
  if (!props.hasI && !props.hasMirrorPerp) {
    spriteIn(g, '轴上找不到 i，也找不到 m⊥', '#fda4af', 460, 64, 0.42, [0, 0, -3.55]);
  }
  return g;
}

/** 原始基元：一个点 + 挂在它上面的右手系三面角 */
function barMotif(n) {
  const g = new THREE.Group();
  g.add(sphereAt(PM, COL.motif, `mo|${n}`, 0.15));
  sticksFor([ID3], TRIAD).forEach((segs, i) => {
    g.add(tubeEdges(`mo-st|${n}|${i}`, segs, 0.036, [COL.x, COL.y, COL.z][i], 0.98));
  });
  spriteIn(g, 'P（右手系）', '#a5f3fc', 260, 64, 0.4, [PM[0] + 0.15, PM[1], PM[2] + 0.42]);
  return g;
}

/** 整个群作用出来的等效点系（恒等那一个就是原始基元，不重复画） */
function barOrbitLayer(n) {
  const orbit = barOrbit(n, PM);
  const imgs = orbit.pts.filter((x) => !same(x.M, ID3));
  const g = new THREE.Group();
  sticksFor(imgs.map((x) => x.M), TRIAD).forEach((segs, i) => {
    g.add(tubeEdges(`ob-st|${n}|${i}`, segs, 0.03, [COL.x, COL.y, COL.z][i], 0.82));
  });
  for (const x of imgs) {
    g.add(sphereAt(x.p, x.det > 0 ? COL.image : COL.imageImp, `ob|${n}|${x.k}`, 0.12, 0.95));
  }
  return g;
}

function barNumbers(n) {
  const orbit = barOrbit(n, PM);
  const g = new THREE.Group();
  for (const x of orbit.pts) {
    if (same(x.M, ID3)) continue;
    spriteIn(g, String(x.k), '#e2e8f0', 84, 84, 0.4, [x.p[0], x.p[1], x.p[2] + 0.3]);
  }
  return g;
}

/** 等效点的两两关系：3̄ 给出关于 i 的对，6̄ 给出关于 m⊥ 的对 */
function barPairs(n) {
  const orbit = barOrbit(n, PM);
  const g = new THREE.Group();
  if (!orbit.pairs.length) {
    spriteIn(g, '等效点配不成关于 i 或 m⊥ 的对', '#fda4af', 560, 64, 0.4, [0, 0, -2.55]);
    return g;
  }
  const tag = orbit.pairs[0][2];
  const color = tag === 'i' ? COL.pairI : COL.pairM;
  const segs = orbit.pairs.map(([a, b]) => [v3(orbit.pts[a].p), v3(orbit.pts[b].p)]);
  g.add(tubeEdges(`pr|${n}`, segs, 0.017, color, 0.8));
  const [a, b] = orbit.pairs[0];
  const mid = v3(orbit.pts[a].p).lerp(v3(orbit.pts[b].p), 0.5);
  const text = tag === 'i'
    ? `${orbit.pairs.length} 对点关于原点对称 → 群里有 i`
    : `${orbit.pairs.length} 对点关于水平面对称 → 群里有 m⊥`;
  const s = spriteIn(g, text, '#e2e8f0', 660, 68, 0.42, [-2.05, -1.55, 1.05]);
  s.renderOrder = 5;
  return g;
}

/** 一步一步施加：走过的点留虚影，当前那个由姿态矩阵带动 */
function barGhost(n, k) {
  const g = new THREE.Group();
  for (let j = 1; j < k; j++) {
    const M = powGen(n, j);
    const p = applyM(M, PM);
    g.add(sphereAt(p, COL.ghost, `gh|${n}|${j}`, 0.1, 0.4));
    sticksFor([M], TRIAD * 0.8).forEach((segs, i) => {
      g.add(tubeEdges(`gh-st|${n}|${j}|${i}`, segs, 0.022, [COL.x, COL.y, COL.z][i], 0.3));
    });
  }
  spriteIn(g, `已施加 ${k} / ${barGroup(n).props.order} 次`, '#a5f3fc', 300, 64, 0.42, [0, -1.75, 2.05]);
  return g;
}

function barCaption(n) {
  const { props } = barGroup(n);
  const g = new THREE.Group();
  const s = spriteIn(g, props.equiv.split('——')[0], '#d8fbff', 980, 88, 0.46, [1.1, 0, -AXIS_LEN - 0.25]);
  s.renderOrder = 6;
  s.material.depthTest = false;
  return g;
}

/* =========================================================
   四面体那一段的图层
   ========================================================= */
const cubeEdges = () => {
  const seg = [];
  for (const a of [-1, 1]) for (const b of [-1, 1]) {
    seg.push([v3([a, b, -1]), v3([a, b, 1])]);
    seg.push([v3([a, -1, b]), v3([a, 1, b])]);
    seg.push([v3([-1, a, b]), v3([1, a, b])]);
  }
  return seg;
};

const tetraEdges = () => {
  const seg = [];
  for (let i = 0; i < 4; i++) for (let j = i + 1; j < 4; j++) seg.push([v3(TETRA_VERTS[i]), v3(TETRA_VERTS[j])]);
  return seg;
};

/** 一个面 = 4 个顶点去掉 1 个撑起来的三角片；双面材质，翻过去也看得见 */
function faceMesh(i) {
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(
    new Float32Array(TETRA_FACES[i].flatMap((k) => TETRA_VERTS[k])), 3));
  geo.computeVertexNormals();
  const m = new THREE.Mesh(geo, stdMat(`face|${i}`, {
    color: COL.faces[i], roughness: 0.55, metalness: 0.05,
    transparent: true, opacity: 0.5, side: THREE.DoubleSide,
  }));
  m.userData.grow = true;
  return m;
}

/** 四面体本体：会被姿态矩阵整体带动 */
function tetraBody({ faces, edges, verts, triad }) {
  const g = new THREE.Group();
  if (faces) for (let i = 0; i < 4; i++) g.add(faceMesh(i));
  if (edges) g.add(tubeEdges('te', tetraEdges(), 0.028, COL.tetraEdge, 0.95));
  if (verts) TETRA_VERTS.forEach((p, i) => g.add(sphereAt(p, COL.verts[i], `tv|${i}`, 0.14)));
  if (triad) {
    // 体心的 RGB 三面角：右手系；做过一次 det = −1 的操作就翻成左手系
    sticksFor([ID3], 0.6, [0, 0, 0]).forEach((segs, i) => {
      g.add(tubeEdges(`ct|${i}`, segs, 0.032, [COL.x, COL.y, COL.z][i], 0.98));
    });
    g.add(sphereAt([0, 0, 0], 0xffffff, 'ct-center', 0.05, 0.9));
  }
  return g;
}

/** 顶点编号不跟着压扁动画一起被压：每帧按姿态矩阵摆到该在的位置 */
function vertexLabels() {
  const g = new THREE.Group();
  g.name = 'vlabels';
  TETRA_VERTS.forEach((p, i) => spriteIn(g, String(i + 1), '#e2e8f0', 84, 84, 0.44, p.map((x) => x * 1.26)));
  return g;
}

/** 步骤 5 的那一行结论：浮在四面体上方 */
function tdCaption() {
  const g = new THREE.Group();
  const s = spriteIn(g, '1 + 8 + 3 + 6 + 6 = 24　→　国际记号 4̄3m（Td）', '#d8fbff', 900, 88, 0.48, [0.9, 0, -2.6]);
  s.renderOrder = 6;
  s.material.depthTest = false;
  return g;
}

const tdGhost = () => tubeEdges('gh-te', tetraEdges(), 0.016, COL.ghost, 0.45);
const tdCube = () => tubeEdges('cube', cubeEdges(), 0.02, COL.cube, 0.5);
const tdAxis4 = () => {
  const g = new THREE.Group();
  AXES_100.forEach((a, i) => g.add(axisRod(a, 1.8, COL.axis4, `4-${i}`, { marker: 4, label: over('4'), labelColor: '#fde68a' })));
  return g;
};
const tdAxis3 = () => {
  const g = new THREE.Group();
  TETRA_VERTS.forEach((a, i) => g.add(axisRod(a, 2.1, COL.axis3, `3-${i}`, { marker: 3, label: '3', labelColor: '#bfdbfe' })));
  return g;
};
const tdMirrors = () => {
  const g = new THREE.Group();
  MIRROR_110.forEach((n, i) => g.add(planeDisc(n, 1.45, `sd${i}`, 0x4ade80, 0.12)));
  return g;
};

/* =========================================================
   舞台 / 姿态动画
   ========================================================= */
const { renderer, scene, camera, controls, root, dirty } = makeStage({ canvasId: 'scene', fogDensity: 0.008 });
const appear = makeAppear();
let shownObjects = new Set();
camera.up.set(0, 0, 1);            // 反轴是竖着的：把相机的“上”对齐世界 z
const CAM = { bar: [5.6, -5.6, 3.4], td: [5.2, -5.2, 4.6] };
const setView = () => {
  camera.position.set(...CAM[state.mode]);
  controls.target.set(0, 0, 0);
  controls.update();
};
setView();

const pose = { bar: ID3, td: ID3 };
let liveBar = null;        // 反轴：正在被带动的那个基元
let liveTd = null;         // 四面体本体
let liveLabels = null;     // 顶点编号

const anim = { active: false, kind: 'bar', base: ID3, target: ID3, n: 4, t0: 0 };
const ease = (t) => 1 - Math.pow(1 - t, 3);

function startAnim(kind, target, base, n = state.n) {
  anim.kind = kind;
  anim.target = target;
  anim.base = base;
  anim.n = n;
  anim.t0 = performance.now();
  anim.active = true;
  dirty.set();
}

function setMat(node, M) {
  node.matrixAutoUpdate = false;
  node.matrix.set(
    M[0][0], M[0][1], M[0][2], 0,
    M[1][0], M[1][1], M[1][2], 0,
    M[2][0], M[2][1], M[2][2], 0,
    0, 0, 0, 1
  );
  node.matrixWorldNeedsUpdate = true;
}

/** 按当前姿态把活着的节点摆好（重建之后、动画每一帧都要调） */
function applyPoses() {
  if (liveTd) {
    setMat(liveTd, pose.td);
    if (liveLabels) {
      TETRA_VERTS.forEach((p, i) => {
        const q = applyM(pose.td, p);
        liveLabels.children[i].position.copy(v3(q).multiplyScalar(1.26));
      });
    }
  }
  if (liveBar) setMat(liveBar, pose.bar);
}

function tickAnim(now) {
  if (!anim.active) return false;
  const t = ease(clamp((now - anim.t0) / ANIM_MS, 0, 1));
  if (anim.kind === 'bar') {
    pose.bar = invertStep(anim.n, t, anim.base);
    if (t >= 1) { pose.bar = powGen(anim.n, state.k); anim.active = false; }
  } else {
    pose.td = animPath(anim.target, t);
    if (t >= 1) anim.active = false;
  }
  applyPoses();
  return true;
}

/* =========================================================
   重建场景
   ========================================================= */
function currentLayers() {
  const dict = state.mode === 'bar' ? BAR_LAYERS : TD_LAYERS;
  const free = state.mode === 'bar' ? state.freeBar : state.freeTd;
  if (!state.demo) return { ...dict, ...free };
  const steps = stepsOf(state.mode);
  return { ...dict, ...steps[Math.min(state.step, steps.length - 1)].layers };
}

function rebuild(animate = false) {
  clearGroup(root);
  liveBar = liveTd = liveLabels = null;
  const L = currentLayers();
  const added = [];
  const add = (key, on, make) => {
    if (!on) return null;
    const o = cachedObject(key, make);
    o.name = key;                     // 图层名留在场景图上，调试和摄制时一眼能看出这一步画了什么
    added.push(o);
    root.add(o);
    return o;
  };

  if (state.mode === 'bar') {
    const n = state.n;
    add(`frame|${n}`, L.axis, () => barFrame(n));
    add(`motif|${n}`, L.motif, () => barMotif(n));
    add(`orbit|${n}`, L.orbit, () => barOrbitLayer(n));
    add(`num|${n}`, L.numbers, () => barNumbers(n));
    add(`pairs|${n}`, L.pairs, () => barPairs(n));
    add(`cap|${n}`, L.caption, () => barCaption(n));
    if (L.current) {
      add(`ghost|${n}|${state.k}`, L.ghost, () => barGhost(n, state.k));
      liveBar = add(`live|${n}`, true, () => barMotif(`${n}live`));
      setMat(liveBar, pose.bar);
    }
  } else {
    add('cube', L.cube, tdCube);
    add('tdghost', L.ghost, tdGhost);
    add('ax4', L.axis4, tdAxis4);
    add('ax3', L.axis3, tdAxis3);
    add('sd', L.mirrors, tdMirrors);
    add('tdcap', L.caption, tdCaption);
    const body = add(`tetra|${L.faces}${L.edges}${L.verts}${L.triad}`,
      L.faces || L.edges || L.verts || L.triad,
      () => tetraBody(L));
    if (body) { liveTd = body; setMat(liveTd, pose.td); }
    if (L.labels && liveTd) {
      liveLabels = add('vlabels', true, vertexLabels);
      applyPoses();
    }
  }

  const fresh = added.filter((o) => o && !shownObjects.has(o));
  shownObjects = new Set(added.filter(Boolean));
  if (animate) appear.begin(root, fresh); else appear.settle();
  dirty.set();
  refreshPanels();
}

/* =========================================================
   右侧面板
   ========================================================= */
let lastPanelKey = '';

function rowsHtml(rows) {
  return rows.map(([k, val]) => `<div class="row"><span class="k">${k}</span><span class="val">${val}</span></div>`).join('');
}

const yesno = (b) => (b ? '<span class="yes">有</span>' : '<span class="no">没有</span>');

function barTableHtml(n) {
  const orbit = barOrbit(n, PM);
  const rows = orbit.pts.map(({ k, M, p, det: d }) => {
    const rot = Math.round((k * 360 / n) % 360);
    const op = d > 0 ? `转 ${rot}°` : `转 ${rot}° + 反演`;
    const cls = same(M, ID3) ? ' class="hl"' : '';
    return `<tr${cls}><td>${over(n)}<sup>${k}</td><td>${op}</td><td>${coordText(p)}</td><td>${d > 0 ? '+' : '−'}</td></tr>`;
  }).join('');
  return `<table class="tbl"><tr><th>幂次</th><th>等于做了什么</th><th>P 落到哪</th><th>det</th></tr>${rows}</table>`;
}

function classTableHtml(activeCls) {
  const rows = CLASSES.map((c) => `<tr${c.cls === activeCls ? ' class="hl"' : ''}>
      <td>${c.name}</td><td>${c.items.length}</td>
      <td>${c.items.every((o) => o.det > 0) ? '真旋转' : c.items.every((o) => o.det < 0) ? '非真' : '混合'}</td>
    </tr>`).join('');
  return `<table class="tbl"><tr><th>共轭类</th><th>个数</th><th>类型</th></tr>${rows}
    <tr><td><b>合计 = |Td|</b></td><td><b>24</b></td><td>真 12 / 非真 12</td></tr></table>`;
}

function matrixHtml(M) {
  return `<table class="mx"><tbody>${M.map((r) => `<tr>${r.map((x) => `<td>${fmt(x)}</td>`).join('')}</tr>`).join('')}</tbody></table>`;
}

function refreshPanels() {
  const steps = stepsOf(state.mode);
  const i = Math.min(state.step, steps.length - 1);
  const s = steps[i];

  el('stageBadge').textContent = state.demo
    ? `第 ${i + 1}/${steps.length} 步 · ${s.title}`
    : (state.mode === 'bar' ? '反轴 n̄ = 旋转 + 反演' : '四面体的 24 个对称操作');
  el('stepPanel').style.display = state.demo ? 'block' : 'none';
  el('hint').style.display = state.demo ? 'none' : 'block';
  el('hint').textContent = state.mode === 'bar'
    ? '等效点系、群里的元素、配对关系全部由矩阵现算'
    : '点左边任意一个操作，四面体照着它变形';
  el('stepTitle').textContent = `${i + 1} / ${steps.length}　${s.title}`;
  el('stepText').textContent = s.text;
  el('stepDots').innerHTML = steps.map((st, j) =>
    `<button class="dot-btn${j === i ? ' on' : ''}" data-i="${j}" title="${j + 1}. ${st.title}">${j + 1}</button>`).join('');
  el('stepPrev').disabled = !state.demo || i === 0;
  el('stepNext').disabled = !state.demo || i === steps.length - 1;
  el('stepPlay').textContent = state.playing ? '❚❚ 暂停' : '▶ 自动播放';
  el('stepPlay').classList.toggle('on', state.playing);
  el('tDemo').checked = state.demo;
  document.querySelectorAll('#modeSel button').forEach((b) => b.classList.toggle('active', b.dataset.mode === state.mode));
  document.querySelectorAll('#barCtl, #tdCtl').forEach((nd) => {
    nd.style.display = (nd.id === 'barCtl') === (state.mode === 'bar') ? 'block' : 'none';
  });
  document.querySelectorAll('.manual-only').forEach((nd) => nd.classList.toggle('dimmed', state.demo));

  const panelKey = `${state.mode}|${state.n}|${state.k}|${state.op}|${state.demo}|${i}`;
  if (panelKey !== lastPanelKey) {
    lastPanelKey = panelKey;
    if (state.mode === 'bar') refreshBarPanels();
    else refreshTdPanels();
  }
}

function refreshBarPanels() {
  const n = state.n;
  const { props, elems } = barGroup(n);
  const orbit = barOrbit(n, PM);
  el('equiv').innerHTML = `<b>${over(n)}</b> 由 ${elems.length} 个操作组成 —— ${props.equiv}`;
  el('infoTitle').innerHTML = `反轴 ${barHtml(n)} 生成的群`;
  el('infoRows').innerHTML = rowsHtml([
    ['生成操作', `绕轴转 ${fmt(360 / n)}°，再过原点反演`],
    ['群的阶 |G|', `<b>${props.order}</b>`],
    ['一般位置的等效点数', `${orbit.pts.length} 个`],
    ['群里的真旋转', props.maxProper > 1 ? `最高 ${props.maxProper} 次轴（${props.properCount} 个真操作）` : `只有 E（${props.properCount} 个真操作）`],
    ['含对称中心 i？', yesno(props.hasI)],
    ['含垂直于轴的镜面 m⊥？', yesno(props.hasMirrorPerp)],
    ['等价记号', `<b>${props.equiv}</b>`],
  ]);
  el('infoTable').innerHTML = barTableHtml(n);
  el('infoNote').innerHTML = {
    1: '1̄ 就是反演本身：转 360° 是白转，剩下关于原点的一次翻面。',
    2: '2̄ 转 180° 把 x、y 变号，反演再把 z 也变号，合起来只有 z 变号——这就是对水平面的反映。所以「2 次反轴」和「镜面」是同一个东西，晶体学表里从不写 2̄。',
    3: '3̄ 的 6 个操作 = {E, 3, 3²} ∪ {i, i·3, i·3²}：真旋转部分是一根 3 次轴，另一半就是它乘上反演。老师写「3 + i」就是这个意思，它也等于 6 次的转动反映轴 S6。',
    4: '4̄ 做 4 次就回原位，只有 4 个等效点；把它拆开既得不到 4 次真轴，也得不到 i 或镜面——所以 4̄ 是真正独立的对称元素，也是立方晶系（金刚石、闪锌矿、NaCl）里最容易被忽略的一个。',
    5: '5̄ 在数学上完全成立（阶 10，= 5 + i），但 5 次轴填不满周期格子，所以晶体学里没有它——这里列出来只是为了说明「反轴」这套记号本身不限于晶体学。',
    6: '6̄ 的答案：转 60° 再反演，偶数次幂回到真旋转，而这里出现的真旋转只有 120°、240°，即一根 3 次轴；奇数次幂给出「对水平面的反映」和它乘 3 次轴。整个群 = 3 次轴 + 垂直镜面 = 3/m（C3h），里面既没有 6 次操作也没有对称中心。老师留的问号就填 3/m。',
  }[n];
  el('howto').innerHTML = `<b>怎么看这一段</b>：${barHtml(n)} 的定义是「绕竖轴转 360/${n}，再关于原点反演」。先把 <b>全部等效点</b> 打开，数一数一共几个点；
    再开 <b>等效点配对关系</b>，看这些点是成对地关于原点对称（有 i）还是关于水平面对称（有 m⊥）。
    最后开 <b>一步一步施加</b>，拖 k 滑杆连做几次，看第 ${props.order} 次正好回到 P——这就是群的阶。
    键：<b>←/→</b> 换步骤，<b>1…6</b> 换反轴，<b>空格</b> 播放/暂停。`;
  el('kSlider').max = String(props.order);
  el('kSlider').value = String(clamp(state.k, 0, props.order));
  el('kVal').textContent = `${state.k}`;
  document.querySelectorAll('#nSel button').forEach((b) => b.classList.toggle('active', Number(b.dataset.n) === n));
  syncToggles(state.mode === 'bar' ? BAR_TOGGLE : TD_TOGGLE, state.mode === 'bar' ? state.freeBar : state.freeTd);
}

function refreshTdPanels() {
  const o = OPS[state.op];
  el('infoTitle').innerHTML = `操作 ${o.label}${o.alt ? `　<span class="sub">${o.alt}</span>` : ''}`;
  el('infoRows').innerHTML = rowsHtml([
    ['共轭类', `<b>${o.cls}</b>（这一类有 ${o.clsCount} 个）`],
    ['类型', o.det > 0 ? '<span class="yes">真旋转 det = +1</span>' : '<span class="no">非真操作 det = −1</span>'],
    ['对称元素', o.mirrorNormal ? `镜面法线 ${dirName(o.mirrorNormal)}` : `轴 ${dirName(o.axis || [0, 0, 1])}`],
    ['转动反映角', `${fmt(o.animDeg)}°`],
    ['顶点置换', `<b>${o.permText}</b>（${o.parity > 0 ? '偶置换' : '奇置换'}）`],
    ['矩阵', matrixHtml(o.M)],
  ]);
  el('infoTable').innerHTML = classTableHtml(o.cls);
  el('infoNote').textContent = o.note;
  el('howto').innerHTML = `<b>怎么看这一段</b>：左边 24 个按钮按共轭类排好，点一个就看四面体怎么做这个操作；
    同时注意体心那个 RGB 三面角——做完 det = −1 的操作它变成左手系。
    开 <b>3 条 4̄ 轴</b>、<b>4 条 3 次轴</b>、<b>6 个 σd 镜面</b> 三个图层，就把国际记号 ${barHtml('4')}3m 的三档位置全指出来了。
    键：<b>[ / ]</b> 换操作，<b>←/→</b> 换步骤，<b>空格</b> 播放/暂停。`;
  el('equiv').innerHTML = `1 + 8 + 3 + 6 + 6 = <b>24</b> 个操作 = 点群 Td = ${barHtml('4')}3m`;
  document.querySelectorAll('#opGrid .op-btn').forEach((b) => b.classList.toggle('active', Number(b.dataset.i) === state.op));
  syncToggles(state.mode === 'bar' ? BAR_TOGGLE : TD_TOGGLE, state.mode === 'bar' ? state.freeBar : state.freeTd);
}

/* =========================================================
   交互
   ========================================================= */
function syncToggles(map, store) {
  for (const [id, key] of Object.entries(map)) {
    const box = el(id);
    if (box && box.checked !== !!store[key]) box.checked = !!store[key];
  }
}

function buildOpGrid() {
  el('opGrid').innerHTML = CLASSES.map((c) => `
    <div class="cls-group">
      <div class="cls-head"><b>${c.name}</b><span>${c.items.length} 个</span></div>
      <div class="ops">${c.items.map((o) => `<button class="op-btn${o.det < 0 ? ' imp' : ''}"
        data-i="${OPS.indexOf(o)}" title="${o.note}">${o.label}<i>${o.permText}</i></button>`).join('')}</div>
    </div>`).join('');
}

function setN(n, replay = false) {
  state.n = clamp(n, 1, 6);
  const order = barGroup(state.n).props.order;
  state.k = clamp(state.k, 0, order);
  if (replay && state.freeBar.current) startAnim('bar', powGen(state.n, state.k), powGen(state.n, Math.max(0, state.k - 1)));
  rebuild(state.demo);
}

function setK(k) {
  const order = barGroup(state.n).props.order;
  const prev = state.k;
  state.k = clamp(k, 0, order);
  if (state.k === prev) return;
  pose.bar = powGen(state.n, prev);
  if (!state.demo && state.freeBar.current) startAnim('bar', powGen(state.n, state.k), powGen(state.n, prev));
  else { pose.bar = powGen(state.n, state.k); }
  lastPanelKey = '';
  rebuild(true);
}

function setOp(v) {
  let idx = state.op;
  if (v === null) { idx = -1; }
  else if (typeof v === 'number') idx = clamp(v, 0, OPS.length - 1);
  else if (typeof v === 'string') {
    const c = CLASSES.find((x) => x.cls === v);
    idx = c ? OPS.indexOf(c.items[v === 'C3' ? 1 : 0]) : state.op;
  }
  if (idx < 0) {
    pose.td = ID3;
    state.op = clamp(state.op, 0, OPS.length - 1);
    startAnim('td', ID3);
    lastPanelKey = '';
    rebuild(true);
    return;
  }
  state.op = idx;
  startAnim('td', OPS[idx].M);
  lastPanelKey = '';
  refreshPanels();
  dirty.set();
}

function pickOp(idx) { setOp(idx); }

function setStep(i) {
  const steps = stepsOf(state.mode);
  state.step = clamp(i, 0, steps.length - 1);
  stepShownAt = performance.now();
  const s = steps[state.step];
  if (s.set) {
    if (typeof s.set.n === 'number') { state.n = s.set.n; state.k = clamp(s.set.k || 0, 0, barGroup(state.n).props.order); }
    else if (typeof s.set.k === 'number') state.k = clamp(s.set.k, 0, barGroup(state.n).props.order);
    if (s.set.op !== undefined) {
      // op: null = 这一步只看结构不看操作，把面板也退回 E，免得写着 S4 而画面没动
      if (s.set.op === null) { pose.td = ID3; state.op = 0; }
      else state.op = typeof s.set.op === 'number' ? s.set.op
        : OPS.indexOf(CLASSES.find((c) => c.cls === s.set.op).items[s.set.op === 'C3' ? 1 : 0]);
    }
  }
  pose.bar = powGen(state.n, state.k);
  if (state.mode === 'td' && state.demo) {
    const st = steps[state.step];
    pose.td = st.set && st.set.op !== undefined && st.set.op !== null ? OPS[state.op].M : ID3;
  }
  rebuild(true);
}

function switchMode(mode) {
  if (state.mode === mode) return;
  state.mode = mode;
  state.step = 0;
  state.playing = false;
  state.cycling = false;
  lastPanelKey = '';
  rebuild(false);
  setView();
  // 切到 Td 就把当前选中的操作演一遍：右侧面板写的是哪个操作，画面就得是哪个
  if (mode === 'td') startAnim('td', OPS[state.op].M);
}

const stepShownAt0 = performance.now();
let stepShownAt = stepShownAt0;
let cycleAt = stepShownAt0;

buildOpGrid();
rebuild(false);
if (state.mode === 'td') startAnim('td', OPS[state.op].M);
applyPoses();
startLoop({
  renderer, scene, camera, controls, dirty,
  onFrame: (now) => {
    tickPlay(now);
    const moved = tickAnim(now);
    return appear.tick(now) || moved;      // 漏了这一句，新图层会永远停在「还没长出来」
  },
});

function tickPlay(now) {
  if (state.demo && state.playing) {
    if (now - stepShownAt < STEP_MS) return;
    const steps = stepsOf(state.mode);
    if (state.step >= steps.length - 1) { state.playing = false; lastPanelKey = ''; refreshPanels(); return; }
    setStep(state.step + 1);
    return;
  }
  if (state.cycling) {
    if (now - cycleAt < OP_MS) return;
    cycleAt = now;
    setOp((state.op + 1) % OPS.length);
  }
}

document.querySelectorAll('#modeSel button').forEach((b) =>
  b.addEventListener('click', () => switchMode(b.dataset.mode)));
document.querySelectorAll('#nSel button').forEach((b) =>
  b.addEventListener('click', () => setN(Number(b.dataset.n), true)));

el('kSlider').addEventListener('input', () => setK(Number(el('kSlider').value)));

el('tDemo').addEventListener('change', () => {
  state.demo = el('tDemo').checked;
  if (!state.demo) state.playing = false;
  if (state.demo) setStep(state.step); else rebuild();
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

el('opGrid').addEventListener('click', (e) => {
  const b = e.target.closest('button');
  if (b) { state.cycling = false; el('opPlay').classList.remove('on'); pickOp(Number(b.dataset.i)); }
});
el('opPrev').addEventListener('click', () => setOp((state.op + OPS.length - 1) % OPS.length));
el('opNext').addEventListener('click', () => setOp((state.op + 1) % OPS.length));
el('opPlay').addEventListener('click', () => {
  state.cycling = !state.cycling;
  cycleAt = performance.now();
  el('opPlay').textContent = state.cycling ? '❚❚ 停止播放' : '▶ 依次播放 24 个';
  el('opPlay').classList.toggle('on', state.cycling);
});
el('poseReset').addEventListener('click', () => { pose.td = ID3; startAnim('td', ID3); dirty.set(); });

for (const [id, key] of Object.entries({ ...BAR_TOGGLE, ...TD_TOGGLE })) {
  const box = el(id);
  if (!box) continue;
  box.checked = !!state.freeBar[key] || !!state.freeTd[key];
  box.addEventListener('change', () => {
    if (id.startsWith('b')) state.freeBar[key] = box.checked;
    else state.freeTd[key] = box.checked;
    rebuild(false);
  });
}
syncToggles(BAR_TOGGLE, state.freeBar);
syncToggles(TD_TOGGLE, state.freeTd);

el('tRotate').addEventListener('change', (e) => { controls.autoRotate = e.target.checked; });
el('resetView').addEventListener('click', setView);
el('alongView').addEventListener('click', () => {
  camera.position.set(1.15, 1.15, 8.2);        // 沿轴俯视：等效点系压成一个正多边形，转角一眼可见
  controls.target.set(0, 0, 0);
  controls.update();
});

addEventListener('keydown', (e) => {
  const steps = stepsOf(state.mode);
  if (e.key === 'ArrowRight' && state.demo) { setStep(state.step + 1); e.preventDefault(); }
  else if (e.key === 'ArrowLeft' && state.demo) { setStep(state.step - 1); e.preventDefault(); }
  else if (e.key === ' ') {
    if (state.demo) { state.playing = !state.playing; stepShownAt = performance.now(); }
    else { state.cycling = !state.cycling; cycleAt = performance.now(); el('opPlay').click(); }
    lastPanelKey = ''; refreshPanels(); e.preventDefault();
  } else if (e.key === ']') { setOp((state.op + 1) % OPS.length); }
  else if (e.key === '[') { setOp((state.op + OPS.length - 1) % OPS.length); }
  else if (/^[1-6]$/.test(e.key) && state.mode === 'bar') { setN(Number(e.key), true); }
  else if ((e.key === 'PageDown' || e.key === 'PageUp') && state.demo) {
    setStep(state.step + (e.key === 'PageDown' ? 1 : -1));
  }
});

/* 摄制模式：?film=1 暴露 window.__film.set({mode, step, n, k, op, cam}) */
if (params.has('film')) {
  installFilmAPI({
    camera, controls, renderer, scene, root,
    meta: {
      modes: ['bar', 'td'],
      barSteps: stepsOf('bar').length,
      tdSteps: stepsOf('td').length,
      ops: OPS.map((o) => o.label),
    },
    apply: (p) => {
      if (p.mode) state.mode = p.mode;
      state.demo = p.demo === undefined ? true : !!p.demo;
      if (typeof p.n === 'number') state.n = clamp(p.n, 1, 6);
      if (typeof p.k === 'number') state.k = clamp(p.k, 0, barGroup(state.n).props.order);
      if (typeof p.op === 'number') state.op = clamp(p.op, 0, OPS.length - 1);
      if (typeof p.step === 'number') state.step = clamp(p.step, 0, stepsOf(state.mode).length - 1);
      // 动画一律直接落到终点，保证每一帧都是确定的、不含时间噪声
      pose.bar = powGen(state.n, state.k);
      if (state.mode === 'td') {
        const st = stepsOf('td')[state.step];
        const wantsOp = !state.demo || (st && st.set && st.set.op !== undefined && st.set.op !== null);
        pose.td = wantsOp ? OPS[state.op].M : ID3;
      } else {
        pose.td = ID3;
      }
      rebuild(false);
      appear.settle();
    },
  });
}
