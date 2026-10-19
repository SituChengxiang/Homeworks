import * as THREE from 'three';
import {
  A, HALF, v,
  atomsFor, touchRadius, latticePoints, nearestDistance,
  computeWS, wsStats, uniqueEdges,
} from './geometry.js';
import { BASE_LAYERS, stepsFor } from './steps.js';
import {
  makeStage, startLoop, clearGroup, cachedObject, makeAppear, installFilmAPI,
  unitSphere, unitCircle, boxEdges, stdMat, lineMat, labelSprite, sharePlanes, tubeEdges,
} from './view.js';

/* =========================================================
   配色
   ========================================================= */
const COLORS = {
  bg:       0x0b1020,
  corner:   0x6ea8fe,
  face:     0xffb454,
  body:     0x4ade80,
  bond:     0x7c8aa5,
  cellEdge: 0xcbd5e1,
  wsFill:   0x22d3ee,
  wsEdge:   0x67e8f9,
  nn1:      0xfb7185,
  nn2:      0xc084fc,
  nn3:      0x94a3b8,
  extended: 0x9fb0cf,
  shareGhost: 0x5b6b8c,
  latticeDot: 0xffffff,
  axisX:    0xff6b6b,
  axisY:    0x6bff8f,
  axisZ:    0x6bb5ff,
};

/* =========================================================
   场景 / 相机 / 灯光（在 view.js 里统一搭建，按需渲染）
   ========================================================= */
const { renderer, scene, camera, controls, root, dirty } = makeStage({ canvasId: 'scene' });

/* =========================================================
   网格构建：几何与材质全部走缓存，半径靠缩放实现
   ========================================================= */
function makeCellEdges() {
  const e = [];
  for (const a of [-HALF, HALF]) for (const b of [-HALF, HALF]) {
    e.push([v(a, b, -HALF), v(a, b, HALF)]);      // 平行 c
    e.push([v(a, -HALF, b), v(a, HALF, b)]);      // 平行 b
    e.push([v(-HALF, a, b), v(HALF, a, b)]);      // 平行 a
  }
  const g = new THREE.Group();
  g.name = 'cell';
  g.add(tubeEdges('cell', e, 0.022, COLORS.cellEdge, 0.95));
  return g;
}

function makeAtoms(atoms, type, scale) {
  const g = new THREE.Group();
  g.name = 'atoms';
  const geo = unitSphere();
  const r = touchRadius(type) * scale;
  const mats = {
    corner: stdMat('atom-corner', { color: COLORS.corner, roughness: 0.42, metalness: 0.15 }),
    face:   stdMat('atom-face',   { color: COLORS.face,   roughness: 0.42, metalness: 0.15 }),
    body:   stdMat('atom-body',   { color: COLORS.body,   roughness: 0.42, metalness: 0.15 }),
  };
  for (const a of atoms) {
    const m = new THREE.Mesh(geo, mats[a.kind]);
    m.position.copy(a.pos);
    m.scale.setScalar(r);
    m.userData.grow = true;
    g.add(m);
  }
  return g;
}

/** 格点：抽象的几何点，用白色小球表示 */
function makeLatticeDots(positions, radius, color, opacity) {
  const g = new THREE.Group();
  const geo = unitSphere(20, 14);
  const mat = stdMat(`dot|${color}|${opacity}`, {
    color, roughness: 0.35, metalness: 0.05,
    emissive: color, emissiveIntensity: 0.35,
    transparent: true, opacity,
  });
  for (const p of positions) {
    const m = new THREE.Mesh(geo, mat);
    m.position.copy(p);
    m.scale.setScalar(radius);
    m.userData.grow = true;
    g.add(m);
  }
  return g;
}

/**
 * “份额原子”：把每个原子用中垂面（过该原子球心、平行于晶胞面的平面）裁开，
 * 只留下落在本晶胞里的那一份，其余部分画成半透明虚影。
 * 顶角 → 1/8，面心 → 1/2，体心 → 1。这样“8×1/8 + 6×1/2 = 4”是能直接看见的。
 */
function makeShareAtoms(atoms, type, scale) {
  const g = new THREE.Group();
  g.name = 'share';
  const r = touchRadius(type) * scale;
  const geo = unitSphere();
  const colors = { corner: COLORS.corner, face: COLORS.face, body: COLORS.body };
  const ghostMat = stdMat('share-ghost', {
    color: COLORS.shareGhost, roughness: 0.9, metalness: 0,
    transparent: true, opacity: 0.14, depthWrite: false,
  });

  for (const a of atoms) {
    const sign = [0, 1, 2].map((i) => Math.sign(a.pos.getComponent(i))).join(',');
    const clip = sharePlanes(a.pos, HALF);
    const solid = new THREE.Mesh(geo, stdMat(`share|${a.kind}|${sign}`, {
      color: colors[a.kind], roughness: 0.4, metalness: 0.15,
      side: THREE.DoubleSide, clippingPlanes: clip,
    }));
    solid.position.copy(a.pos);
    solid.scale.setScalar(r);
    solid.userData.grow = true;
    g.add(solid);

    // 被别的晶胞占去的部分：虚影（体心原子整个在本胞内，无需虚影）
    if (clip.length) {
      const ghost = new THREE.Mesh(geo, ghostMat);
      ghost.position.copy(a.pos);
      ghost.scale.setScalar(r);
      g.add(ghost);
    }
  }
  return g;
}

/** 晶胞份额标签（1/8、1/2、1） */
function makeShareLabels(atoms, type) {
  const g = new THREE.Group();
  g.name = 'labels';
  const txt = { corner: '1/8', face: '1/2', body: '1' };
  const css = { corner: '#9ec3ff', face: '#ffcf8f', body: '#a5f3c4' };
  for (const a of atoms) {
    const s = labelSprite(txt[a.kind], css[a.kind], 96, 56);
    const dir = a.kind === 'body' ? v(0.7, 0.7, 0) : a.pos.clone().normalize();
    s.position.copy(a.pos).addScaledVector(dir, touchRadius(type) * state.radiusScale * 1.5 + 0.15);
    g.add(s);
  }
  return g;
}

function makeBonds(atoms) {
  let dmin = Infinity;
  for (let i = 0; i < atoms.length; i++)
    for (let j = i + 1; j < atoms.length; j++)
      dmin = Math.min(dmin, atoms[i].pos.distanceTo(atoms[j].pos));

  const pairs = [];
  for (let i = 0; i < atoms.length; i++)
    for (let j = i + 1; j < atoms.length; j++)
      if (Math.abs(atoms[i].pos.distanceTo(atoms[j].pos) - dmin) < 1e-4)
        pairs.push([atoms[i].pos, atoms[j].pos]);
  const g = new THREE.Group();
  g.name = 'bonds';
  g.add(tubeEdges('bond', pairs, 0.02, COLORS.bond, 0.92));
  return g;
}

function makeWSMesh(faces) {
  const g = new THREE.Group();
  g.name = 'ws';

  const pos = [];
  for (const f of faces)
    for (let i = 1; i < f.length - 1; i++)
      pos.push(
        f[0].x, f[0].y, f[0].z,
        f[i].x, f[i].y, f[i].z,
        f[i + 1].x, f[i + 1].y, f[i + 1].z
      );

  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  geo.computeVertexNormals();

  const mesh = new THREE.Mesh(geo, stdMat('ws-fill', {
    color: COLORS.wsFill, transparent: true, opacity: 0.17, side: THREE.DoubleSide,
    depthWrite: false, roughness: 0.3, metalness: 0.0,
    emissive: new THREE.Color(COLORS.wsFill), emissiveIntensity: 0.12,
  }));
  mesh.renderOrder = 2;
  g.add(mesh);

  const edges = tubeEdges('ws-edge', uniqueEdges(faces), 0.016, COLORS.wsEdge, 0.95);
  edges.renderOrder = 3;
  g.add(edges);

  return g;
}

/**
 * 中垂面：对每个近邻格点 r，取过 r/2、法向沿 r 的平面，
 * 画一小块半透明面片，直观显示「中垂面把空间一块块切出来」。
 */
function makeBisectPlanes(type) {
  const g = new THREE.Group();
  g.name = 'planes';
  const dmin = nearestDistance(type);
  const geo = unitCircle();
  const mat = stdMat('plane', {
    color: COLORS.wsFill, transparent: true, opacity: 0.2,
    side: THREE.DoubleSide, depthWrite: false, roughness: 0.4,
    emissive: COLORS.wsFill, emissiveIntensity: 0.3,
  });

  for (const r of latticePoints(type, 1.35 * dmin)) {
    const mesh = new THREE.Mesh(geo, mat);
    mesh.position.copy(r).multiplyScalar(0.5);
    mesh.quaternion.setFromUnitVectors(new THREE.Vector3(0, 0, 1), r.clone().normalize());
    mesh.scale.setScalar(dmin * 0.36);
    g.add(mesh);
  }
  return g;
}

/** 各近邻壳层（用于“构造”演示与图例） */
function shellsOf(type) {
  const dmin = nearestDistance(type);
  const shells = [];
  for (const p of latticePoints(type, 1.35 * dmin)) {
    const d = p.length();
    if (!shells.some((s) => Math.abs(s.d - d) < 1e-4)) shells.push({ d, pts: [] });
    shells.find((s) => Math.abs(s.d - d) < 1e-4).pts.push(p);
  }
  shells.sort((a, b) => a.d - b.d);
  return shells;
}

function makeNeighbors(type) {
  const g = new THREE.Group();
  g.name = 'neighbors';
  const shells = shellsOf(type);
  const colors = [COLORS.nn1, COLORS.nn2, COLORS.nn3];
  const geo = unitSphere(16, 12);

  shells.forEach((s, i) => {
    const color = colors[i] || COLORS.nn3;
    const lmat = lineMat(`nn-line|${color}`, { color, transparent: true, opacity: 0.95 });
    const pmat = stdMat(`nn-dot|${color}`, {
      color, roughness: 0.4, metalness: 0.1, emissive: color, emissiveIntensity: 0.25,
    });
    const lpos = [];
    for (const p of s.pts) {
      lpos.push(0, 0, 0, p.x, p.y, p.z);
      const m = new THREE.Mesh(geo, pmat);
      m.position.copy(p);
      m.scale.setScalar(0.06);
      m.userData.grow = true;
      g.add(m);
    }
    const lgeo = new THREE.BufferGeometry();
    lgeo.setAttribute('position', new THREE.Float32BufferAttribute(lpos, 3));
    g.add(new THREE.LineSegments(lgeo, lmat));
  });

  const center = new THREE.Mesh(geo, stdMat('nn-center', {
    color: 0xffffff, emissive: 0xffffff, emissiveIntensity: 0.5,
  }));
  center.scale.setScalar(0.08);
  g.add(center);
  return g;
}

/** 扩展晶格点云：把"无限晶格"的感觉画出来 */
function makeExtendedLattice(type, range) {
  const g = new THREE.Group();
  g.name = 'extended';
  const pts = [v(0, 0, 0), ...latticePoints(type, range * A)];

  const inst = new THREE.InstancedMesh(
    unitSphere(12, 10),
    stdMat('extended', {
      color: COLORS.extended, roughness: 0.6, metalness: 0.1,
      transparent: true, opacity: 0.5,
    }),
    pts.length
  );
  const dummy = new THREE.Object3D();
  pts.forEach((p, i) => {
    dummy.position.copy(p);
    dummy.scale.setScalar(0.075);
    dummy.updateMatrix();
    inst.setMatrixAt(i, dummy.matrix);
  });
  inst.instanceMatrix.needsUpdate = true;
  g.add(inst);
  g.userData.noFrame = true;          // 背景点云不参与自动取景
  return g;
}

/**
 * WS 原胞平铺：只在「与中心胞共面的格点」上再放一个 WS 胞，
 * 于是能直接看到面与面互相贴合、无缝填满空间。
 * faceNeighbors 正是那些贡献了面的近邻。
 */
function makeWSTiling(faces, faceNeighbors) {
  const g = new THREE.Group();
  g.name = 'tiling';
  const pts = [v(0, 0, 0), ...faceNeighbors];
  const edges = uniqueEdges(faces);

  const epos = [];
  for (const p of pts)
    for (const [a, b] of edges)
      epos.push(a.x + p.x, a.y + p.y, a.z + p.z, b.x + p.x, b.y + p.y, b.z + p.z);

  const egeo = new THREE.BufferGeometry();
  egeo.setAttribute('position', new THREE.Float32BufferAttribute(epos, 3));
  g.add(new THREE.LineSegments(egeo, lineMat('tiling', {
    color: COLORS.wsEdge, transparent: true, opacity: 0.4,
  })));
  return g;
}

/** 晶轴箭头 + a/b/c 标签 */
function makeAxes() {
  const g = new THREE.Group();
  g.name = 'axes';
  const len = 1.5 * A;
  const defs = [
    [v(1, 0, 0), COLORS.axisX, '#ff9a9a', 'a'],
    [v(0, 1, 0), COLORS.axisY, '#9affb8', 'b'],
    [v(0, 0, 1), COLORS.axisZ, '#9ad0ff', 'c'],
  ];
  for (const [dir, color, css, label] of defs) {
    g.add(new THREE.ArrowHelper(dir, v(0, 0, 0), len, color, 0.2, 0.1));
    const lab = labelSprite(label, css);
    lab.scale.set(0.62, 0.62, 0.62);
    lab.position.copy(dir).multiplyScalar(len + 0.3);
    g.add(lab);
  }
  return g;
}

/* =========================================================
   结构说明数据
   ========================================================= */
const STRUCT_INFO = {
  SC: {
    title: '简立方 (Simple Cubic)',
    rows: [
      ['布拉维格子', '简单立方 (P)'],
      ['每晶胞原子数', '8 × 1/8 = <b>1</b>'],
      ['配位数 CN', '<b>6</b>（最近邻沿 &lt;100&gt;）'],
      ['最近邻距离', 'a'],
      ['原子半径（接触）', 'a/2'],
      ['堆积率', 'π/6 ≈ <b>52.36%</b>'],
      ['WS 原胞', '立方体，6 个正方形面'],
      ['典型材料', 'α-Po（钋）'],
    ],
    note: 'SC 的 WS 原胞就是晶胞本身：6 个中垂面恰好把空间切成边长 a 的立方体。',
  },
  BCC: {
    title: '体心立方 (Body-Centered Cubic)',
    rows: [
      ['布拉维格子', '体心立方 (I)'],
      ['每晶胞原子数', '8 × 1/8 + 1 = <b>2</b>'],
      ['配位数 CN', '<b>8</b>（最近邻在体对角线方向）'],
      ['最近邻距离', '(√3/2)·a ≈ 0.866a'],
      ['原子半径（接触）', '(√3/4)·a'],
      ['堆积率', '√3π/8 ≈ <b>68.02%</b>'],
      ['WS 原胞', '截角八面体，14 面 = 6 正方形 + 8 正六边形'],
      ['典型材料', 'Fe(α)、Cr、W、Mo、Na、K'],
    ],
    note: '最近邻 8 个（体对角线方向）切出 8 个六边形面；次近邻 6 个（沿 <100>）把 8 个顶点截掉，切出 6 个正方形面。',
  },
  FCC: {
    title: '面心立方 (Face-Centered Cubic)',
    rows: [
      ['布拉维格子', '面心立方 (F)'],
      ['每晶胞原子数', '8 × 1/8 + 6 × 1/2 = <b>4</b>'],
      ['配位数 CN', '<b>12</b>（最近邻在面对角线方向）'],
      ['最近邻距离', '(√2/2)·a ≈ 0.707a'],
      ['原子半径（接触）', '(√2/4)·a'],
      ['堆积率', '√2π/6 ≈ <b>74.05%</b>'],
      ['WS 原胞', '菱形十二面体，12 个全等菱形面'],
      ['典型材料', 'Cu、Al、Au、Ag、Ni、γ-Fe'],
    ],
    note: '12 个最近邻的中垂面围成菱形十二面体，这是最密堆积结构之一。',
  },
};

/* =========================================================
   状态 / UI
   ========================================================= */
const state = {
  type: 'SC',
  atoms: true,
  cell: true,
  bonds: true,
  ws: true,
  neighbors: false,
  planes: false,    // WS 中垂面
  extended: true,   // 扩展晶格点云
  axes: false,      // 坐标轴
  tiling: false,    // WS 原胞平铺
  range: 3,         // 扩展范围（单位 a）
  radiusScale: 0.55,
  autoRotate: false,
  demo: false,      // 分步演示模式
  step: 0,
  playing: false,
};

// 支持用 URL 参数直接打开某个视图，例如 ?s=BCC&nn=1
const params = new URLSearchParams(location.search);
if (['SC', 'BCC', 'FCC'].includes((params.get('s') || '').toUpperCase()))
  state.type = params.get('s').toUpperCase();
for (const [q, key] of [
  ['atoms', 'atoms'], ['cell', 'cell'], ['bonds', 'bonds'], ['ws', 'ws'], ['nn', 'neighbors'],
  ['ext', 'extended'], ['axes', 'axes'], ['tile', 'tiling'], ['demo', 'demo'], ['planes', 'planes'],
]) if (params.has(q)) state[key] = params.get(q) !== '0';
if (params.has('r')) state.radiusScale = Math.min(1, Math.max(0.25, parseFloat(params.get('r'))));
if (params.has('range')) state.range = Math.min(4, Math.max(1.5, parseFloat(params.get('range'))));
if (params.has('rot')) state.autoRotate = params.get('rot') !== '0';
if (params.has('step')) state.step = Math.max(0, parseInt(params.get('step'), 10) - 1 || 0);

let wsResult = null;

/** 当前该画哪些图层：自由模式读开关，演示模式读当前步骤 */
function currentLayers() {
  if (!state.demo) {
    return {
      ...BASE_LAYERS,
      axes: state.axes, cell: state.cell, atoms: state.atoms ? 'all' : false,
      bonds: state.bonds, nn: state.neighbors, planes: state.planes, ws: state.ws,
      tiling: state.tiling, ext: state.extended, radius: state.radiusScale,
    };
  }
  const steps = stepsFor(state.type);
  const s = steps[Math.min(state.step, steps.length - 1)];
  return { ...BASE_LAYERS, radius: state.radiusScale, ...s.layers };
}

const cornersOfCell = () => atomsFor('SC').map((a) => a.pos);
const q2 = (x) => x.toFixed(2);

/**
 * 重建 = 决定「哪些图层可见」，图层对象本身按参数缓存，
 * 拖动滑杆或切换步骤时只是把现成的 Group 挂回场景。
 */
let shownRadius = null;          // 上一次真正显示的半径比例，用来做补间
let shownObjects = new Set();    // 上一步已经挂着的图层对象
let radiusTween = null;

/**
 * 重建 = 决定「哪些图层可见」，图层对象本身按参数缓存，
 * 拖动滑杆或切换步骤时只是把现成的 Group 挂回场景。
 * 两步之间只有半径不同时用补间过渡，并且只让「新出现」的图层做生长动画，
 * 这样按下一步是往前推进，而不是把整幅画重新播一遍。
 */
function rebuild(withAppear = false) {
  clearGroup(root);
  const type = state.type;
  const L = currentLayers();
  const atoms = atomsFor(type);
  const pick = (which) => atoms.filter((a) => (which === 'all' ? true : a.kind === which));

  wsResult = computeWS(type);

  const added = [];
  const add = (key, on, make) => {
    if (!on) return;
    const o = cachedObject(key, make);
    added.push(o);
    root.add(o);
  };

  add(`ext|${type}|${state.range}`, L.ext, () => makeExtendedLattice(type, state.range));
  add(`tile|${type}`, L.tiling, () => makeWSTiling(wsResult.faces, wsResult.faceNeighbors));
  add('cell', L.cell, makeCellEdges);
  add('corners', L.corners, () => makeLatticeDots(cornersOfCell(), 0.085, COLORS.latticeDot, 0.9));
  add(`bonds|${type}`, L.bonds, () => makeBonds(atoms));
  // 原子/份额图层的缓存键里不含半径：半径由 applyRadius 统一归位，
  // 否则「只缩放不重建」的快速路径会把缓存对象改成别的尺寸。
  add(`atoms|${type}|${L.atoms}`, L.atoms, () => makeAtoms(pick(L.atoms), type, L.radius));
  add(`share|${type}|${L.share}`, L.share, () => makeShareAtoms(pick(L.share), type, L.radius));
  add(`labels|${type}|${L.share}|${q2(L.radius)}`, L.share && L.labels, () => makeShareLabels(pick(L.share), type));
  add(`ws|${type}`, L.ws, () => makeWSMesh(wsResult.faces));
  add(`planes|${type}`, L.planes, () => makeBisectPlanes(type));
  add(`nn|${type}`, L.nn, () => makeNeighbors(type));
  add('center', L.center, () => makeLatticeDots([v(0, 0, 0)], 0.11, COLORS.latticeDot, 1));
  add('axes', L.axes, makeAxes);

  const rChanged = shownRadius !== null && Math.abs(shownRadius - L.radius) > 0.005;
  applyRadius(L.radius);
  radiusTween = withAppear && rChanged ? { from: shownRadius, to: L.radius, t0: performance.now() } : null;
  shownRadius = L.radius;

  let fresh = added.filter((o) => !shownObjects.has(o));
  shownObjects = new Set(added);
  if (radiusTween) fresh = fresh.filter((o) => o.name !== 'atoms' && o.name !== 'share');
  if (withAppear) appear.begin(root, fresh); else appear.settle();

  dirty.set();
  refreshInfo();
  updateStepUI();
}

/* ---------- 图层的生长/淡入动画（实现在 view.js，可被 Node 单元测试） ---------- */
const appear = makeAppear();

/** 只改原子大小的快速路径：缩放现成的网格，不重建场景图 */
function scaleAtomsTo(radiusScale) {
  const s = touchRadius(state.type) * radiusScale;
  for (const g of root.children) {
    if (g.name !== 'atoms' && g.name !== 'share') continue;
    g.traverse((o) => { if (o.isMesh) o.scale.setScalar(s); });
  }
}

function applyRadius(r) {
  appear.settle();
  scaleAtomsTo(r);
  dirty.set();
}

/** 步骤之间半径不同就平滑过渡（刚球接触这一步尤其明显） */
function tickRadius(now) {
  if (!radiusTween) return false;
  const k = Math.min(1, (now - radiusTween.t0) / 420);
  const e = 1 - Math.pow(1 - k, 3);
  scaleAtomsTo(radiusTween.from + (radiusTween.to - radiusTween.from) * e);
  if (k === 1) radiusTween = null;
  return true;
}

/* 面板刷新各自带脏检查：拖滑杆时类型/步骤没变，就不碰 DOM */
let infoKey = '';
let stepKey = '';

function refreshInfo() {
  const type = state.type;
  const L = currentLayers();
  const key = `${type}|${L.nn ? 1 : 0}`;
  if (key === infoKey) return;
  infoKey = key;

  const info = STRUCT_INFO[type];
  el('infoTitle').textContent = info.title;

  const stats = wsStats(wsResult.faces);
  const rows = [...info.rows, ['WS 原胞几何', `V = ${stats.V}，E = ${stats.E}，F = ${stats.F}`]];

  el('infoRows').innerHTML = rows
    .map(([k, val]) => `<div class="row"><span class="k">${k}</span><span class="val">${val}</span></div>`)
    .join('');
  el('infoNote').textContent = info.note;

  document.querySelectorAll('#structSel button').forEach((b) =>
    b.classList.toggle('active', b.dataset.s === type)
  );

  const legend = el('nnLegend');
  legend.style.display = L.nn ? 'block' : 'none';
  if (L.nn) {
    const names = ['最近邻', '次近邻', '第三近邻'];
    const dots = ['#fb7185', '#c084fc', '#94a3b8'];
    legend.innerHTML =
      '<div class="legend-title">近邻方向（WS 构造）</div>' +
      shellsOf(type)
        .map((s, i) =>
          `<div class="lg"><i style="background:${dots[i] || dots[2]}"></i>${
            names[i] || '近邻'
          }：d = ${s.d.toFixed(3)} = ${(s.d / A).toFixed(3)}a（${s.pts.length} 个）</div>`
        )
        .join('');
  }
}

/* ---------- 事件绑定 ---------- */
const el = (id) => document.getElementById(id);
let stepShownAt = 0;

const stepCount = () => stepsFor(state.type).length;

function setStep(i) {
  state.step = Math.max(0, Math.min(stepCount() - 1, i));
  stepShownAt = performance.now();
  rebuild(true);
}

function updateStepUI(force = false) {
  const n = stepCount();
  const i = Math.min(state.step, n - 1);
  const s = stepsFor(state.type)[i];
  const demo = state.demo;

  const key = `${state.type}|${i}|${demo}|${state.playing}`;
  if (!force && key === stepKey) return;
  stepKey = key;

  el('stageBadge').textContent = demo
    ? `${state.type} · 第 ${i + 1}/${n} 步 · ${s.title}`
    : '拖拽旋转 · 滚轮缩放 · 右键平移';
  el('stepPanel').style.display = demo ? 'block' : 'none';
  el('hint').style.display = demo ? 'none' : 'block';
  el('stepTitle').textContent = `${i + 1} / ${n}　${s.title}`;
  el('stepText').textContent = s.text;
  el('stepDots').innerHTML = stepsFor(state.type)
    .map((st, k) => `<button class="dot-btn${k === i ? ' on' : ''}" data-i="${k}" title="${k + 1}. ${st.title}">${k + 1}</button>`)
    .join('');
  el('stepPrev').disabled = !demo || i === 0;
  el('stepNext').disabled = !demo || i === n - 1;
  el('stepPlay').textContent = state.playing ? '❚❚ 暂停' : '▶ 自动播放';
  el('stepPlay').classList.toggle('on', state.playing);
  el('tDemo').checked = demo;
  document.querySelectorAll('.manual-only').forEach((nd) => nd.classList.toggle('dimmed', demo));
}

document.querySelectorAll('#structSel button').forEach((b) => {
  b.addEventListener('click', () => {
    state.type = b.dataset.s;
    state.step = Math.min(state.step, stepCount() - 1);
    rebuild(state.demo);
  });
});

el('tDemo').addEventListener('change', () => {
  state.demo = el('tDemo').checked;
  if (!state.demo) state.playing = false;
  if (state.demo) setStep(0);
  else rebuild();
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
  updateStepUI();
});

addEventListener('keydown', (e) => {
  if (!state.demo) return;
  if (e.key === 'ArrowRight') { setStep(state.step + 1); e.preventDefault(); }
  else if (e.key === 'ArrowLeft') { setStep(state.step - 1); e.preventDefault(); }
  else if (e.key === ' ') {
    state.playing = !state.playing;
    stepShownAt = performance.now();
    updateStepUI();
    e.preventDefault();
  }
});

const STEP_MS = 5200;
function tickPlay(now) {
  if (!state.demo || !state.playing) return;
  if (now - stepShownAt < STEP_MS) return;
  if (state.step >= stepCount() - 1) {
    state.playing = false;
    updateStepUI();
    return;
  }
  setStep(state.step + 1);
}

const TOGGLE_IDS = {
  tAtoms: 'atoms', tCell: 'cell', tBonds: 'bonds', tWS: 'ws',
  tNeighbors: 'neighbors', tPlanes: 'planes', tExtended: 'extended',
  tAxes: 'axes', tTiling: 'tiling',
};
for (const [id, key] of Object.entries(TOGGLE_IDS)) {
  const box = el(id);
  box.checked = state[key];
  box.addEventListener('change', () => {
    state[key] = box.checked;
    rebuild();
  });
}

const rangeSlider = el('range');
rangeSlider.value = state.range;
rangeSlider.addEventListener('input', () => {
  state.range = parseFloat(rangeSlider.value);
  el('rangeVal').textContent = state.range.toFixed(1) + 'a';
  rebuild();
});
el('rangeVal').textContent = state.range.toFixed(1) + 'a';

const rSlider = el('rScale');
rSlider.value = state.radiusScale;
rSlider.addEventListener('input', () => {
  state.radiusScale = parseFloat(rSlider.value);
  el('rVal').textContent = state.radiusScale.toFixed(2);
  const L = currentLayers();
  // 只有原子大小在变 → 直接缩放现成的网格，不重建
  if (!L.labels && (L.atoms || L.share)) applyRadius(touchRadius(state.type) * state.radiusScale);
  else rebuild();
});
el('rVal').textContent = state.radiusScale.toFixed(2);

const tRot = el('tRotate');
tRot.checked = state.autoRotate;
tRot.addEventListener('change', () => {
  state.autoRotate = tRot.checked;
  controls.autoRotate = state.autoRotate;
});

el('resetView').addEventListener('click', () => {
  camera.position.set(5.4, 4.0, 6.6);
  controls.target.set(0, 0, 0);
  controls.update();
});

/* =========================================================
   渲染循环：按需出图（空闲帧不做任何 GPU 工作）
   ========================================================= */
rebuild();
startLoop({
  renderer, scene, camera, controls, dirty,
  onFrame: (now) => {
    tickPlay(now);
    return appear.tick(now) || tickRadius(now);
  },
});

/* 摄制模式：?film=1 时暴露 window.__film.set()，一帧一个确定画面，供无头 Chrome 截图 */
if (params.has('film')) {
  installFilmAPI({
    camera, controls, renderer, scene, root,
    meta: {
      steps: { SC: stepsFor('SC').length, BCC: stepsFor('BCC').length, FCC: stepsFor('FCC').length },
      titles: { SC: stepsFor('SC').map((s) => s.title), BCC: stepsFor('BCC').map((s) => s.title), FCC: stepsFor('FCC').map((s) => s.title) },
      // 每一步的半径比例，摄制时可以逐帧插值，刚球那一步就是连续涨大而不是硬切
      radius: Object.fromEntries(['SC', 'BCC', 'FCC'].map((t) => [t,
        stepsFor(t).map((s) => s.layers.radius ?? 0.55)])),
      defaultRadius: 0.55,
    },
    apply: (p) => {
      if (p.type) state.type = p.type;
      state.demo = p.demo === undefined ? true : !!p.demo;
      if (typeof p.step === 'number') state.step = p.step;
      if (typeof p.range === 'number') state.range = p.range;
      rebuild(false);
      appear.settle();
      if (p.radiusTweenTo !== undefined) {        // 让补间在截帧之间也能定格在任意时刻
        radiusTween = null;
        scaleAtomsTo(touchRadius(state.type) * p.radiusTweenTo);
      }
    },
  });
}
