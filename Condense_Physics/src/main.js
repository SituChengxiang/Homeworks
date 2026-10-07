import * as THREE from 'three';
import { OrbitControls } from '../vendor/OrbitControls.js';
import {
  A, v,
  atomsFor, touchRadius, latticePoints, nearestDistance,
  computeWS, wsStats, uniqueEdges,
} from './geometry.js';

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
  axisX:    0xff6b6b,
  axisY:    0x6bff8f,
  axisZ:    0x6bb5ff,
};

/* =========================================================
   场景 / 相机 / 灯光
   ========================================================= */
const canvas = document.getElementById('scene');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));

const scene = new THREE.Scene();
scene.background = new THREE.Color(COLORS.bg);
scene.fog = new THREE.FogExp2(COLORS.bg, 0.017); // 轻微雾效，营造纵深

const camera = new THREE.PerspectiveCamera(45, 1, 0.1, 200);
camera.position.set(5.4, 4.0, 6.6);

const controls = new OrbitControls(camera, canvas);
controls.enableDamping = true;
controls.dampingFactor = 0.08;
controls.minDistance = 3;
controls.maxDistance = 30;
controls.autoRotateSpeed = 1.8;

scene.add(new THREE.AmbientLight(0xffffff, 0.5));
scene.add(new THREE.HemisphereLight(0xbfd4ff, 0x141a2e, 0.9));
const key = new THREE.DirectionalLight(0xffffff, 1.6);
key.position.set(7, 10, 8);
scene.add(key);
const fill = new THREE.DirectionalLight(0x88aaff, 0.75);
fill.position.set(-8, -5, -7);
scene.add(fill);

const root = new THREE.Group();
scene.add(root);

/* =========================================================
   网格构建
   ========================================================= */
function disposeGroup(g) {
  g.traverse((o) => {
    if (o.geometry) o.geometry.dispose();
    if (o.material) {
      if (Array.isArray(o.material)) o.material.forEach((m) => m.dispose());
      else o.material.dispose();
    }
  });
  g.clear();
}

function makeCellEdges() {
  const geo = new THREE.EdgesGeometry(new THREE.BoxGeometry(A, A, A));
  const mat = new THREE.LineBasicMaterial({ color: COLORS.cellEdge, transparent: true, opacity: 0.85 });
  const seg = new THREE.LineSegments(geo, mat);
  seg.name = 'cell';
  return seg;
}

function makeAtoms(atoms, type, scale) {
  const g = new THREE.Group();
  g.name = 'atoms';
  const r = touchRadius(type) * scale;
  const geo = new THREE.SphereGeometry(r, 32, 24);
  const mats = {
    corner: new THREE.MeshStandardMaterial({ color: COLORS.corner, roughness: 0.42, metalness: 0.15 }),
    face:   new THREE.MeshStandardMaterial({ color: COLORS.face,   roughness: 0.42, metalness: 0.15 }),
    body:   new THREE.MeshStandardMaterial({ color: COLORS.body,   roughness: 0.42, metalness: 0.15 }),
  };
  for (const a of atoms) {
    const m = new THREE.Mesh(geo, mats[a.kind]);
    m.position.copy(a.pos);
    g.add(m);
  }
  return g;
}

function makeBonds(atoms) {
  let dmin = Infinity;
  for (let i = 0; i < atoms.length; i++)
    for (let j = i + 1; j < atoms.length; j++)
      dmin = Math.min(dmin, atoms[i].pos.distanceTo(atoms[j].pos));

  const pos = [];
  for (let i = 0; i < atoms.length; i++)
    for (let j = i + 1; j < atoms.length; j++) {
      if (Math.abs(atoms[i].pos.distanceTo(atoms[j].pos) - dmin) < 1e-4) {
        pos.push(atoms[i].pos.x, atoms[i].pos.y, atoms[i].pos.z);
        pos.push(atoms[j].pos.x, atoms[j].pos.y, atoms[j].pos.z);
      }
    }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  const mat = new THREE.LineBasicMaterial({ color: COLORS.bond, transparent: true, opacity: 0.9 });
  const seg = new THREE.LineSegments(geo, mat);
  seg.name = 'bonds';
  return seg;
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

  const mat = new THREE.MeshStandardMaterial({
    color: COLORS.wsFill,
    transparent: true,
    opacity: 0.17,
    side: THREE.DoubleSide,
    depthWrite: false,
    roughness: 0.3,
    metalness: 0.0,
    emissive: new THREE.Color(COLORS.wsFill),
    emissiveIntensity: 0.12,
  });
  const mesh = new THREE.Mesh(geo, mat);
  mesh.renderOrder = 2;
  g.add(mesh);

  const epos = [];
  for (const [a, b] of uniqueEdges(faces)) epos.push(a.x, a.y, a.z, b.x, b.y, b.z);
  const egeo = new THREE.BufferGeometry();
  egeo.setAttribute('position', new THREE.Float32BufferAttribute(epos, 3));
  const emat = new THREE.LineBasicMaterial({ color: COLORS.wsEdge, transparent: true, opacity: 0.95 });
  const edges = new THREE.LineSegments(egeo, emat);
  edges.renderOrder = 3;
  g.add(edges);

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

  const sphereGeo = new THREE.SphereGeometry(0.06, 16, 12);
  shells.forEach((s, i) => {
    const color = colors[i] || COLORS.nn3;
    const lmat = new THREE.LineBasicMaterial({ color, transparent: true, opacity: 0.95 });
    const pmat = new THREE.MeshStandardMaterial({
      color, roughness: 0.4, metalness: 0.1,
      emissive: color, emissiveIntensity: 0.25,
    });
    const lpos = [];
    for (const p of s.pts) {
      lpos.push(0, 0, 0, p.x, p.y, p.z);
      const m = new THREE.Mesh(sphereGeo, pmat);
      m.position.copy(p);
      g.add(m);
    }
    const lgeo = new THREE.BufferGeometry();
    lgeo.setAttribute('position', new THREE.Float32BufferAttribute(lpos, 3));
    g.add(new THREE.LineSegments(lgeo, lmat));
  });

  const center = new THREE.Mesh(
    new THREE.SphereGeometry(0.08, 16, 12),
    new THREE.MeshStandardMaterial({ color: 0xffffff, emissive: 0xffffff, emissiveIntensity: 0.5 })
  );
  g.add(center);
  return g;
}

/** 扩展晶格点云：把"无限晶格"的感觉画出来（借鉴 Qwen 版） */
function makeExtendedLattice(type, range) {
  const g = new THREE.Group();
  g.name = 'extended';
  const pts = [v(0, 0, 0), ...latticePoints(type, range * A)];

  const geo = new THREE.SphereGeometry(0.075, 12, 10);
  const mat = new THREE.MeshStandardMaterial({
    color: COLORS.extended, roughness: 0.6, metalness: 0.1,
    transparent: true, opacity: 0.5,
  });
  const inst = new THREE.InstancedMesh(geo, mat, pts.length);
  const dummy = new THREE.Object3D();
  pts.forEach((p, i) => {
    dummy.position.copy(p);
    dummy.updateMatrix();
    inst.setMatrixAt(i, dummy.matrix);
  });
  inst.instanceMatrix.needsUpdate = true;
  g.add(inst);
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
  const emat = new THREE.LineBasicMaterial({ color: COLORS.wsEdge, transparent: true, opacity: 0.4 });
  g.add(new THREE.LineSegments(egeo, emat));
  return g;
}

/** 坐标轴箭头 + a/b/c 标签（借鉴 Qwen 版） */
function makeLabel(text, color) {
  const c = document.createElement('canvas');
  c.width = c.height = 64;
  const ctx = c.getContext('2d');
  ctx.font = 'bold 46px sans-serif';
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillStyle = color;
  ctx.fillText(text, 32, 34);
  const tex = new THREE.CanvasTexture(c);
  tex.colorSpace = THREE.SRGBColorSpace;
  const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: tex, transparent: true, depthWrite: false }));
  s.scale.set(0.55, 0.55, 0.55);
  return s;
}

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
    const lab = makeLabel(label, css);
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
  extended: true,   // 扩展晶格点云
  axes: false,      // 坐标轴
  tiling: false,    // WS 原胞平铺
  range: 3,         // 扩展范围（单位 a）
  radiusScale: 0.55,
  autoRotate: false,
};

// 支持用 URL 参数直接打开某个视图，例如 ?s=BCC&nn=1
const params = new URLSearchParams(location.search);
if (['SC', 'BCC', 'FCC'].includes((params.get('s') || '').toUpperCase()))
  state.type = params.get('s').toUpperCase();
for (const [q, key] of [
  ['atoms', 'atoms'], ['cell', 'cell'], ['bonds', 'bonds'], ['ws', 'ws'], ['nn', 'neighbors'],
  ['ext', 'extended'], ['axes', 'axes'], ['tile', 'tiling'],
]) if (params.has(q)) state[key] = params.get(q) !== '0';
if (params.has('r')) state.radiusScale = Math.min(1, Math.max(0.25, parseFloat(params.get('r'))));
if (params.has('range')) state.range = Math.min(4, Math.max(1.5, parseFloat(params.get('range'))));
if (params.has('rot')) state.autoRotate = params.get('rot') !== '0';

let wsResult = null;

function rebuild() {
  disposeGroup(root);
  const type = state.type;
  const atoms = atomsFor(type);

  wsResult = computeWS(type);

  if (state.extended) root.add(makeExtendedLattice(type, state.range));
  if (state.tiling) root.add(makeWSTiling(wsResult.faces, wsResult.faceNeighbors));
  if (state.cell) root.add(makeCellEdges());
  if (state.bonds) root.add(makeBonds(atoms));
  if (state.atoms) root.add(makeAtoms(atoms, type, state.radiusScale));
  if (state.ws) root.add(makeWSMesh(wsResult.faces));
  if (state.neighbors) root.add(makeNeighbors(type));
  if (state.axes) root.add(makeAxes());

  updateInfo();
}

function updateInfo() {
  const type = state.type;
  const info = STRUCT_INFO[type];
  document.getElementById('infoTitle').textContent = info.title;

  const stats = wsStats(wsResult.faces);
  const rows = [...info.rows, ['WS 原胞几何', `V = ${stats.V}，E = ${stats.E}，F = ${stats.F}`]];

  document.getElementById('infoRows').innerHTML = rows
    .map(([k, val]) => `<div class="row"><span class="k">${k}</span><span class="val">${val}</span></div>`)
    .join('');
  document.getElementById('infoNote').textContent = info.note;

  document.querySelectorAll('#structSel button').forEach((b) =>
    b.classList.toggle('active', b.dataset.s === type)
  );

  const legend = document.getElementById('nnLegend');
  legend.style.display = state.neighbors ? 'block' : 'none';
  if (state.neighbors) {
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
document.querySelectorAll('#structSel button').forEach((b) => {
  b.addEventListener('click', () => {
    state.type = b.dataset.s;
    rebuild();
  });
});

const bindToggle = (id, key) => {
  const el = document.getElementById(id);
  el.checked = state[key];
  el.addEventListener('change', () => {
    state[key] = el.checked;
    rebuild();
  });
};
bindToggle('tAtoms', 'atoms');
bindToggle('tCell', 'cell');
bindToggle('tBonds', 'bonds');
bindToggle('tWS', 'ws');
bindToggle('tNeighbors', 'neighbors');
bindToggle('tExtended', 'extended');
bindToggle('tAxes', 'axes');
bindToggle('tTiling', 'tiling');

const rangeSlider = document.getElementById('range');
rangeSlider.value = state.range;
rangeSlider.addEventListener('input', () => {
  state.range = parseFloat(rangeSlider.value);
  document.getElementById('rangeVal').textContent = state.range.toFixed(1) + 'a';
  rebuild();
});
document.getElementById('rangeVal').textContent = state.range.toFixed(1) + 'a';

const rSlider = document.getElementById('rScale');
rSlider.value = state.radiusScale;
rSlider.addEventListener('input', () => {
  state.radiusScale = parseFloat(rSlider.value);
  document.getElementById('rVal').textContent = state.radiusScale.toFixed(2);
  rebuild();
});
document.getElementById('rVal').textContent = state.radiusScale.toFixed(2);

const tRot = document.getElementById('tRotate');
tRot.checked = state.autoRotate;
tRot.addEventListener('change', () => {
  state.autoRotate = tRot.checked;
  controls.autoRotate = state.autoRotate;
});

document.getElementById('resetView').addEventListener('click', () => {
  camera.position.set(5.4, 4.0, 6.6);
  controls.target.set(0, 0, 0);
  controls.update();
});

/* =========================================================
   渲染循环 / 自适应
   ========================================================= */
function resize() {
  const w = canvas.clientWidth, h = canvas.clientHeight;
  if (!w || !h) return;
  renderer.setSize(w, h, false);
  camera.aspect = w / h;
  camera.updateProjectionMatrix();
}
new ResizeObserver(resize).observe(canvas);
resize();

function animate() {
  requestAnimationFrame(animate);
  controls.update();
  renderer.render(scene, camera);
}

rebuild();
animate();
