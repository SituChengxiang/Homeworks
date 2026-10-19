// {110} 晶面族的可视化：12 个面按「2 个面一组、开六组」+「12 个面合成一个晶形」来讲。
// 晶面族展开、面片裁剪、面间距、二维网胞、晶带全部由 src/family.js 现算，
// 这里只负责把算出来的东西摆进三维空间，并把数字写到面板上。
import * as THREE from 'three';
import { planeBasis } from '../../lib/matrix.js';
import {
  family, formOf, planePatch, structOf, latticePoints, pointsInPatch, planeMesh,
  atomicSpacing, zonesOf, angleBetween, dirLabel, STRUCTURES,
} from './family.js';
import { LAYERS, groupsOf } from './steps.js';
import {
  makeStage, startLoop, clearGroup, cachedObject, makeAppear, installFilmAPI,
  unitSphere, stdMat, labelSprite, tubeEdges,
} from '../../src/view.js';

const el = (id) => document.getElementById(id);
const clamp = (x, lo, hi) => Math.min(hi, Math.max(lo, x));
const v3 = (p) => new THREE.Vector3(p[0], p[1], p[2]);
const add = (a, b) => a.map((x, i) => x + b[i]);
const scaled = (p, s) => p.map((x) => x * s);
const unit = (p) => scaled(p, 1 / (Math.hypot(...p) || 1));
const centroid = (poly) => poly.reduce(add, [0, 0, 0]).map((x) => x / poly.length);
const f4 = (x) => x.toFixed(4);
const f1 = (x) => x.toFixed(1);
const same3 = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);

/** 六组平行对各给一色；同一组的正反两面同色——它们本来就是同一个朝向 */
const GROUP_COL = [0x22d3ee, 0xfbbf24, 0xa78bfa, 0x4ade80, 0xf472b6, 0x60a5fa];
const COL = {
  cell: 0xcbd5e1, crystal: 0x64748b, mid: 0x3f4d6b, point: 0xaab8d4, atom: 0x7fa8d9,
  through: 0x7c8ba8, measure: 0xfb7185, net: 0xfde68a, netPoint: 0xfbbf24,
  zone: 0x34d399, sphere: 0x93c5fd, edge: 0xe2e8f0,
  axis: [0xf87171, 0x4ade80, 0x60a5fa],
};
const colOf = (m) => GROUP_COL[m.group % GROUP_COL.length];

const CRYSTAL_HALF = 1;          // 画出的晶体 = 2a × 2a × 2a，面片是切它得到的截面
const AXIS_LEN = 1.62;
const ARROW_LEN = 1.3;
const STEP_MS = 9000;
const FAMS = [[1, 0, 0], [1, 1, 0], [1, 1, 1], [1, 1, 2]];

const LAYER_NAMES = {
  cell: '立方晶胞与晶轴',
  crystal: '2a×2a×2a 晶体块',
  points: '格点',
  atoms: '真实半径原子',
  pair: '点亮的晶面',
  through: '过原点的同向面',
  normal: '法线箭头与 [hkl]',
  labels: '面上的 (hkl) 标号',
  intercept: '截距 → 密勒指数',
  stack: '一叠平行面与 d',
  net: '面内二维网格与面密度',
  form: '晶形轮廓（棱与顶点）',
  star: '12 张面平移到原点',
  sphere: '法线端点与截角八面体',
  zone: '晶带高亮',
};

/* =========================================================
   状态
   ========================================================= */
const state = {
  hkl: [1, 1, 0],
  struct: 'bcc',
  group: 0,                      // 0…pairs−1 = 平行对；pairs = 组合视图
  demo: false,
  playing: false,
  zone: 0,
  freeGroup: null,               // 两组图层默认值在 buildCtx() 之后填
  freeForm: null,
};

const params = new URLSearchParams(location.search);
const P = (x) => params.get(x);
const parseFam = (s) => (s.includes(',')
  ? s.split(',').map(Number)
  : [...s].map((c) => (c === 'x' || c === 'X' ? 0 : Number(c)))).filter((x) => Number.isFinite(x));
if (P('fam')) { const h = parseFam(P('fam')); if (h.length === 3) state.hkl = h; }
if (P('struct') && STRUCTURES[P('struct')]) state.struct = P('struct');
if (P('group')) state.group = clamp(parseInt(P('group'), 10) || 0, 0, 99);
if (params.has('demo')) state.demo = P('demo') !== '0';
if (P('zone')) state.zone = clamp(parseInt(P('zone'), 10) || 0, 0, 99);

/** 每次换族/换结构/换组都要重算的一坨，全部来自 family.js */
let ctx = null;
function buildCtx() {
  const fam = family(state.hkl);
  const form = formOf(fam);
  const steps = groupsOf(fam, form);
  const group = clamp(state.group, 0, fam.pairs);
  const step = steps[group];
  state.group = group;
  ctx = {
    fam, form, steps, step, group,
    zones: zonesOf(fam),
    isForm: step.kind === 'form',
    struct: structOf(state.struct),
    points: latticePoints(state.struct, CRYSTAL_HALF + 1e-6),
    mesh: planeMesh(state.struct, fam.hkl),
    dAtomic: atomicSpacing(state.struct, fam.hkl),
    faceOf: new Map(form.faces.map((f) => [f.member.label, f])),
  };
  if (!state.freeGroup) state.freeGroup = { ...step.layers };
  if (!state.freeForm) state.freeForm = { ...steps[fam.pairs].layers };
  return ctx;
}

const litFaces = () => (ctx.isForm ? ctx.fam.members : ctx.step.faces);
const litGroups = () => (ctx.isForm ? ctx.fam.groups : [ctx.step.group]);

/* =========================================================
   画图小件
   ========================================================= */
function sphereAt(p, color, key, r = 0.052, opacity = 1) {
  const m = new THREE.Mesh(unitSphere(18, 14), stdMat(`sph|${key}`, {
    color, roughness: 0.42, metalness: 0.12,
    emissive: color, emissiveIntensity: 0.18,
    transparent: opacity < 1, opacity,
  }));
  m.position.copy(v3(p));
  m.scale.setScalar(r);
  m.userData.grow = true;
  return m;
}

/** 画布文字的总缩放：所有 spriteIn 都乘这个数，调一处就够了 */
const LABEL_SCALE = 0.62;

function spriteIn(node, text, color, w, h, scale, pos) {
  const s = labelSprite(text, color, w, h, scale * LABEL_SCALE);
  s.position.copy(v3(pos));
  node.add(s);
  return s;
}

/** 面内方向：用来把标注从面中心挪开，免得所有文字都叠在一点上 */
const inPlane = (n, ang) => {
  const b = planeBasis(n);
  return add(scaled(b.u, Math.cos(ang)), scaled(b.v, Math.sin(ang)));
};

/** 凸多边形面片：三角扇 + 一圈细棱，双面材质翻过去也看得见 */
function facePatch(key, poly, color, opacity = 0.32) {
  const g = new THREE.Group();
  if (!poly || poly.length < 3) return g;
  const geo = new THREE.BufferGeometry();
  const fan = [poly[0], ...poly.slice(1), ...poly.slice(1, -1).reverse()];
  geo.setAttribute('position', new THREE.BufferAttribute(new Float32Array(fan.flatMap((p) => p)), 3));
  geo.computeVertexNormals();
  const mesh = new THREE.Mesh(geo, stdMat(`fill|${key}`, {
    color, roughness: 0.85, metalness: 0.02,
    transparent: true, opacity, side: THREE.DoubleSide, depthWrite: false,
  }));
  mesh.userData.grow = true;
  g.add(mesh);
  g.add(tubeEdges(`rim|${key}`, poly.map((p, i) => [v3(p), v3(poly[(i + 1) % poly.length])]),
    0.012, color, Math.min(1, opacity + 0.5)));
  return g;
}

function rod(key, from, to, color, radius = 0.019, opacity = 0.95) {
  const g = new THREE.Group();
  g.add(tubeEdges(`rod|${key}`, [[v3(from), v3(to)]], radius, color, opacity));
  g.add(sphereAt(to, color, `tip|${key}`, radius * 2.2, opacity));
  return g;
}

/** 这一组/这一族该画的面片：组视图切晶体块，组合视图用晶形自己围出来的那张面 */
const patchOf = (m) => (ctx.isForm
  ? (ctx.faceOf.get(m.label) || {}).poly
  : planePatch(m.hkl, m.t, CRYSTAL_HALF));

/* =========================================================
   图层
   ========================================================= */
function cellLayer() {
  const g = new THREE.Group();
  const h = 0.5;
  const seg = [];
  for (const a of [-h, h]) for (const b of [-h, h]) {
    seg.push([v3([-h, a, b]), v3([h, a, b])]);
    seg.push([v3([a, -h, b]), v3([a, h, b])]);
    seg.push([v3([a, b, -h]), v3([a, b, h])]);
  }
  g.add(tubeEdges('cell', seg, 0.014, COL.cell, 0.5));
  [0, 1, 2].forEach((i) => {
    const e = [0, 0, 0];
    e[i] = 1;
    g.add(rod(`ax${i}`, scaled(e, -AXIS_LEN), scaled(e, AXIS_LEN), COL.axis[i], 0.016, 0.7));
    spriteIn(g, ['x', 'y', 'z'][i], '#fecaca', 64, 64, 0.26, scaled(e, AXIS_LEN + 0.17));
  });
  spriteIn(g, 'a', '#cbd5e1', 64, 64, 0.24, [0.25, -0.62, 0.6]);
  return g;
}

/** 2a×2a×2a 的晶体块：外框 + 三张中位剖面，一眼看出是 8 个晶胞拼的 */
function crystalLayer() {
  const g = new THREE.Group();
  const h = CRYSTAL_HALF;
  const outer = [];
  const mid = [];
  for (const a of [-h, h]) for (const b of [-h, h]) {
    outer.push([v3([-h, a, b]), v3([h, a, b])]);
    outer.push([v3([a, -h, b]), v3([a, h, b])]);
    outer.push([v3([a, b, -h]), v3([a, b, h])]);
  }
  // 三张中位剖面（x=0 / y=0 / z=0 上的正方形轮廓），让人一眼看出这是 8 个晶胞拼的
  for (const i of [0, 1, 2]) {
    const others = [0, 1, 2].filter((k) => k !== i);
    for (const v of others) for (const s of [-h, h]) {
      const a = [0, 0, 0];
      const b = [0, 0, 0];
      a[v] = -h; b[v] = h;
      a[others.find((k) => k !== v)] = s;
      b[others.find((k) => k !== v)] = s;
      mid.push([v3(a), v3(b)]);
    }
  }
  g.add(tubeEdges('cry', outer, 0.011, COL.crystal, 0.32));
  g.add(tubeEdges('cry-mid', mid, 0.006, COL.mid, 0.24));
  return g;
}

function pointsLayer() {
  const g = new THREE.Group();
  ctx.points.forEach((p, i) => g.add(sphereAt(p, COL.point, `pt${i}`, 0.05, 0.92)));
  return g;
}

function atomsLayer() {
  const g = new THREE.Group();
  ctx.points.forEach((p, i) => g.add(sphereAt(p, COL.atom, `at${i}`, ctx.struct.radius, 0.5)));
  return g;
}

function pairLayer() {
  const g = new THREE.Group();
  for (const m of litFaces()) {
    const poly = patchOf(m);
    if (!poly || poly.length < 3) continue;
    const sub = facePatch(`face|${ctx.fam.label}|${m.label}`, poly, colOf(m), ctx.isForm ? 0.42 : 0.34);
    if (LAYER_ON('labels')) spriteIn(sub, m.label, '#eef4ff', 210, 80, 0.36,
      add(centroid(poly), scaled(m.n, 0.16)));
    g.add(sub);
  }
  return g;
}

function throughLayer() {
  const g = new THREE.Group();
  for (const grp of litGroups()) {
    const m = grp.members[0];
    g.add(facePatch(`th|${ctx.fam.label}|${m.label}`, planePatch(m.hkl, 0, CRYSTAL_HALF), COL.through, 0.08));
  }
  return g;
}

function normalLayer() {
  const g = new THREE.Group();
  for (const m of litFaces()) {
    const tip = scaled(m.n, ARROW_LEN);
    g.add(rod(`nm|${m.label}`, [0, 0, 0], tip, colOf(m), 0.02));
    spriteIn(g, dirLabel(m.hkl), '#f1f5ff', 200, 78, 0.32, scaled(m.n, ARROW_LEN + 0.21));
    const foot = scaled(m.n, m.d);                 // 垂足：原点到这张面的距离正好是 d
    g.add(sphereAt(foot, COL.measure, `ft|${m.label}`, 0.032, 0.95));
  }
  return g;
}

function stackLayer() {
  const g = new THREE.Group();
  const m = litFaces()[0];
  if (ctx.isForm) {
    // 组合视图里逐族讲没有意义，改成标出原点到每张面的 d
    for (const x of ctx.fam.members) {
      g.add(tubeEdges(`dm|${x.label}`, [[v3([0, 0, 0]), v3(scaled(x.n, x.d))]], 0.008, COL.measure, 0.45));
    }
    spriteIn(g, `每张面到原点都是 d = ${f4(m.d)}a`, '#ffd7dd', 560, 80, 0.38, [0, 0, -1.78]);
    return g;
  }
  // 用本组代表面的指标把一叠等距平行面画出来；亮着的那两张（t = ±1）不重复画
  for (let t = -3; t <= 3; t++) {
    if (Math.abs(t) === 1 || t === 0 && LAYER_ON('through')) continue;
    g.add(facePatch(`st|${ctx.fam.label}|${m.label}|${t}`, planePatch(m.hkl, t, CRYSTAL_HALF), COL.through, 0.06));
  }
  g.add(tubeEdges(`dmA`, [[v3([0, 0, 0]), v3(scaled(m.n, m.d))]], 0.015, COL.measure, 0.95));
  g.add(tubeEdges(`dmB`, [[v3(scaled(m.n, -m.d)), v3(scaled(m.n, m.d))]], 0.008, COL.measure, 0.4));
  const side = inPlane(m.n, 0.6);
  spriteIn(g, `d = ${f4(m.d)}a`, '#ffd7dd', 300, 78, 0.32,
    add(scaled(m.n, m.d / 2), scaled(side, 0.3)));
  return g;
}

function netLayer() {
  const g = new THREE.Group();
  const mesh = ctx.mesh;
  for (const m of (ctx.isForm ? [litFaces()[0]] : ctx.step.faces)) {
    const poly = patchOf(m);
    if (!poly || poly.length < 3) continue;
    const pts = pointsInPatch(state.struct, m.hkl, m.t, poly);
    pts.forEach((p, i) => g.add(sphereAt(p, COL.netPoint, `np|${m.label}|${i}`, 0.052, 1)));
    if (mesh && pts.length) {
      // 把二维原胞摆到面中心附近：挑离中心最近的格点，再挑让平行四边形最居中的那组符号
      const c = centroid(poly);
      const d2 = (p) => p.reduce((a, x) => a + x * x, 0);
      const o = pts.reduce((b, p) => (d2(sub3(p, c)) < d2(sub3(b, c)) ? p : b), pts[0]);
      let q = null;
      let best = Infinity;
      for (const su of [1, -1]) for (const sv of [1, -1]) {
        const cc = add(o, add(scaled(mesh.u, su / 2), scaled(mesh.v, sv / 2)));
        if (d2(sub3(cc, c)) >= best) continue;
        best = d2(sub3(cc, c));
        q = [o, add(o, scaled(mesh.u, su)), add(add(o, scaled(mesh.u, su)), scaled(mesh.v, sv)), add(o, scaled(mesh.v, sv))];
      }
      g.add(tubeEdges(`mesh|${m.label}`, q.map((p, i) => [v3(p), v3(q[(i + 1) % 4])]), 0.013, COL.net, 0.92));
    }
  }
  return g;
}
const sub3 = (a, b) => a.map((x, i) => x - b[i]);

function formLayer() {
  const g = new THREE.Group();
  g.add(tubeEdges('form-ed', ctx.form.edges.map(([a, b]) => [v3(ctx.form.verts[a]), v3(ctx.form.verts[b])]),
    0.016, COL.edge, 0.85));
  ctx.form.verts.forEach((p, i) => g.add(sphereAt(p, COL.point, `fv${i}`, 0.056, 0.95)));
  // 汉字比拉丁字母宽得多：画布按 60 px/字给，否则两端被裁
  const line = `${ctx.form.faceCount} 面 · ${ctx.form.vertCount} 顶点 · ${ctx.form.edgeCount} 棱`;
  spriteIn(g, line, '#e2e8f0', 60 * line.length, 84, 0.34, [0, -0.4, 1.62]);
  return g;
}

function starLayer() {
  const g = new THREE.Group();
  for (const m of ctx.fam.members) {
    g.add(facePatch(`star|${ctx.fam.label}|${m.label}`, planePatch(m.hkl, 0, CRYSTAL_HALF), colOf(m), 0.16));
    spriteIn(g, m.label, '#dbeafe', 200, 78, 0.28, scaled(m.n, 1.34));
  }
  return g;
}

function sphereLayer() {
  const g = new THREE.Group();
  const R = 1.04;
  const ball = new THREE.Mesh(unitSphere(30, 20), stdMat('ball', {
    color: COL.sphere, roughness: 1, metalness: 0,
    transparent: true, opacity: 0.05, side: THREE.DoubleSide, depthWrite: false,
  }));
  ball.scale.setScalar(R);
  ball.userData.grow = true;
  g.add(ball);
  const ends = ctx.fam.members.map((m) => scaled(m.n, R));
  const seg = [];
  ctx.fam.members.forEach((a, i) => ctx.fam.members.forEach((b, j) => {
    if (j <= i || Math.abs(angleBetween(a, b) - 60) > 1e-6) return;
    seg.push([v3(ends[i]), v3(ends[j])]);
  }));
  g.add(tubeEdges('cbt', seg, 0.008, COL.sphere, 0.42));
  ctx.fam.members.forEach((m, i) => {
    g.add(sphereAt(ends[i], colOf(m), `se|${m.label}`, 0.055, 0.95));
    g.add(tubeEdges(`sr|${m.label}`, [[v3([0, 0, 0]), v3(ends[i])]], 0.007, colOf(m), 0.33));
  });
  return g;
}

function zoneLayer() {
  const g = new THREE.Group();
  const z = ctx.zones[clamp(state.zone, 0, ctx.zones.length - 1)];
  if (!z) return g;
  const d = scaled(unit(z.dir), 1.95);
  g.add(rod('zone', scaled(d, -1), d, COL.zone, 0.024, 0.95));
  spriteIn(g, `晶带轴 ${z.label}`, '#a7f3d0', 330, 80, 0.36, scaled(unit(z.dir), 2.2));
  for (const m of z.members) {
    const poly = patchOf(m);
    if (!poly || poly.length < 3) continue;
    g.add(tubeEdges(`zn|${m.label}`, poly.map((p, i) => [v3(p), v3(poly[(i + 1) % poly.length])]),
      0.026, COL.zone, 0.95));
    spriteIn(g, m.label, '#d1fae5', 210, 80, 0.3, add(centroid(poly), scaled(m.n, 0.4)));
  }
  return g;
}

/** 截距层：面与三根晶轴的交点 + 「截距 → 倒数 → 密勒指数」这条来路 */
function interceptLayer() {
  const g = new THREE.Group();
  if (ctx.isForm) return g;
  for (const m of ctx.step.faces) {
    const cuts = [];
    for (let i = 0; i < 3; i++) {
      const c = m.hkl[i];
      if (c === 0) { cuts.push({ txt: '∞', p: null }); continue; }
      const p = [0, 0, 0];
      p[i] = m.t / c;
      cuts.push({ txt: `${fmtIdx(p[i])}a`, p: Math.abs(p[i]) <= CRYSTAL_HALF + 1e-9 ? p : null });
    }
    cuts.forEach((c, i) => { if (c.p) g.add(sphereAt(c.p, COL.measure, `ic|${m.label}|${i}`, 0.055, 1)); });
  }
  return g;
}
const fmtIdx = (x) => (Math.abs(x - Math.round(x)) < 1e-9 ? String(Math.round(x)) : x.toFixed(3));

/** labels 不在这里：面上的 (hkl) 由 pairLayer 按 LAYER_ON('labels') 决定画不画 */
const MAKERS = {
  cell: cellLayer, crystal: crystalLayer, points: pointsLayer, atoms: atomsLayer,
  pair: pairLayer, through: throughLayer, normal: normalLayer,
  intercept: interceptLayer, stack: stackLayer, net: netLayer,
  form: formLayer, star: starLayer, sphere: sphereLayer, zone: zoneLayer,
};

let activeLayers = { ...LAYERS };
const LAYER_ON = (k) => !!activeLayers[k];

/* =========================================================
   舞台
   ========================================================= */
const { renderer, scene, camera, controls, root, dirty } = makeStage({ canvasId: 'scene', fogDensity: 0.008 });
const appear = makeAppear();
let shownObjects = new Set();
camera.up.set(0, 0, 1);                    // 晶学惯例：c 轴朝上
function setView() {
  camera.position.set(2.9, -3.25, 2.15);
  controls.target.set(0, 0, 0);
  controls.update();
}

function currentLayers() {
  if (state.demo) return { ...LAYERS, ...ctx.step.layers };
  return { ...LAYERS, ...(ctx.isForm ? state.freeForm : state.freeGroup) };
}

function rebuild(animate = false) {
  activeLayers = currentLayers();
  clearGroup(root);
  const added = [];
  for (const name of Object.keys(LAYERS)) {
    if (!activeLayers[name] || !MAKERS[name]) continue;
    const o = cachedObject(`${ctx.fam.label}|${state.struct}|${state.group}|${state.zone}|${name}`,
      () => MAKERS[name]());
    o.name = name;
    added.push(o);
    root.add(o);
  }
  const fresh = added.filter((o) => !shownObjects.has(o));
  shownObjects = new Set(added);
  if (animate) appear.begin(root, fresh); else appear.settle();
  dirty.set();
  refreshPanels();
}

/* =========================================================
   右侧面板
   ========================================================= */
function rowsHtml(rows) {
  return rows.map(([k, val]) => `<div class="row"><span class="k">${k}</span><span class="val">${val}</span></div>`).join('');
}
const eqnTxt = (m) => {
  const term = (c, v) => (c === 0 ? '' : ` ${c < 0 ? '−' : '+'} ${Math.abs(c) === 1 ? v : Math.abs(c) + v}`);
  const lhs = (term(m.hkl[0], 'x') + term(m.hkl[1], 'y') + term(m.hkl[2], 'z'))
    .replace(/^ \+ /, '').replace(/^ − /, '−');
  return `${lhs} = ${m.t > 0 ? '1' : '−1'}`;
};
function faceAnglesTxt(poly) {
  const a = poly.map((p, i) => {
    const u = poly[(i + poly.length - 1) % poly.length].map((x, j) => x - p[j]);
    const v = poly[(i + 1) % poly.length].map((x, j) => x - p[j]);
    return Math.acos(Math.max(-1, Math.min(1, (u[0] * v[0] + u[1] * v[1] + u[2] * v[2])
      / (Math.hypot(...u) * Math.hypot(...v))))) * 180 / Math.PI;
  }).sort((x, y) => x - y);
  return `${a[0].toFixed(2)}° / ${a[a.length - 1].toFixed(2)}°`;
}
const diagRatio = (poly) => {
  if (poly.length !== 4) return null;
  const d = [[0, 2], [1, 3]].map(([i, j]) => Math.hypot(...poly[i].map((x, k) => x - poly[j][k])));
  return Math.max(...d) / Math.min(...d);
};
const formName = (f) => (f.faceCount === 12 ? '菱形十二面体'
  : f.faceCount === 6 ? '立方体' : f.faceCount === 8 ? '八面体' : `${f.faceCount} 面体`);
const densestLabel = () => [[[1, 0, 0], '{100}'], [[1, 1, 0], '{110}'], [[1, 1, 1], '{111}']]
  .map(([h, l]) => [l, planeMesh(state.struct, h).density]).sort((a, b) => b[1] - a[1])[0][0];
const neighCount = () => {
  const a = ctx.form.faces[0];
  return ctx.form.faces.filter((g, j) => j !== 0 && g.edges.some((e) => a.edges.includes(e))).length;
};

function membersTable(anchor) {
  const rows = ctx.fam.members.map((m) => {
    const on = ctx.step.faces.includes(m);
    return `<tr${on ? ' class="hl"' : ''}><td>${m.label}</td><td>${dirLabel(m.hkl)}</td>`
      + `<td>${m.group + 1}</td><td>${m.sign > 0 ? '正面' : '反面'}</td>`
      + `<td>${ctx.isForm ? f1(m.d) : f1(angleBetween(anchor, m))}°</td></tr>`;
  }).join('');
  return `<table class="tbl"><tr><th>晶面</th><th>法线</th><th>组</th><th>朝向</th><th>${ctx.isForm ? 'd/a' : '与它成角'}</th></tr>${rows}</table>`;
}

function densityTable() {
  const rows = [[1, 0, 0], [1, 1, 0], [1, 1, 1]].map((h) => {
    const me = planeMesh(state.struct, h);
    const here = same3(h, ctx.fam.hkl);
    return `<tr${here ? ' class="hl"' : ''}><td>{${h.join('')}}</td><td>${f4(1 / Math.hypot(...h))}</td>`
      + `<td>${f4(atomicSpacing(state.struct, h))}</td><td>${f4(me.area)}</td><td>${f4(me.density)}</td></tr>`;
  }).join('');
  return `<div class="section-title" style="margin-top:13px">本结构三种面族对照（${ctx.struct.name}）</div>`
    + `<table class="tbl"><tr><th>面族</th><th>教科书 d</th><th>相邻原子面</th><th>二维原胞</th><th>面密度</th></tr>${rows}</table>`
    + `<p class="sub" style="margin-top:6px">最密的是 <b>${densestLabel()}</b>。「相邻原子面」比教科书 d 小，`
    + `是复式格子在中间多插了一层（消光那件事的几何版本）。</p>`;
}

function groupPanel() {
  const g = ctx.step.group;
  const [pos, neg] = g.members;
  const mesh = planeMesh(state.struct, g.hkl);
  const face = ctx.faceOf.get(pos.label);
  const perp = ctx.fam.groups.filter((o) => o !== g && Math.abs(angleBetween(o.members[0], pos) - 90) < 1e-6);
  const nb = ctx.fam.groups.filter((o) => o !== g && Math.abs(angleBetween(o.members[0], pos) - 60) < 1e-6);
  const pts = pointsInPatch(state.struct, pos.hkl, pos.t, patchOf(pos) || []);
  el('infoTitle').innerHTML = `第 ${state.group + 1} 组　${g.label} ∥ ${neg.label}`;
  el('infoRows').innerHTML = rowsHtml([
    ['这一组', `两张互相平行的面，法线 ${dirLabel(pos.hkl)} 与 ${dirLabel(neg.hkl)}`],
    ['面方程', `${eqnTxt(pos)}　/　${eqnTxt(neg)}`],
    ['截距', `(${[0, 1, 2].map((i) => (pos.hkl[i] === 0 ? '∞' : `${fmtIdx(pos.t / pos.hkl[i])}a`)).join(', ')})`],
    ['面间距 d', `<b>${f4(pos.d)}a</b> = a/√(h²+k²+l²) = a/√${Math.round(pos.L * pos.L)}`],
    ['相邻含格点面', `${f4(ctx.dAtomic)}a${Math.abs(ctx.dAtomic - pos.d) < 1e-9 ? '（没有附加面）' : '（复式格子多插一层）'}`],
    ['这张面上的格点', `${pts.length} 个（${ctx.struct.short}）`],
    ['面内二维原胞', `${f4(mesh.area)}a² → 面密度 <b>${f4(mesh.density)}/a²</b>`],
    ['晶形上的这张面', face ? `${face.poly.length} 边形 ${f4(face.area)}a²，内角 ${faceAnglesTxt(face.poly)}` : '—'],
    ['与它垂直的一组', perp.map((o) => o.label).join('、') || '—'],
    ['与它成 60° 的面', nb.map((o) => o.label).join('、') || '—'],
  ]);
  el('infoTable').innerHTML = membersTable(pos) + densityTable();
  el('infoNote').innerHTML = `${ctx.fam.label} 一共 <b>${ctx.fam.count}</b> 个面：指标绝对值是 {1,1,0} 的排列就算同族，`
    + `零放在哪个位置 3 种、另两个指标带不带负号 4 种，3 × 4 = <b>12</b>。`
    + `而 (hkl) 与 (h̄k̄l̄) 是同一张面的两个朝向，所以真正不同的朝向只有 <b>${ctx.fam.pairs}</b> 个 —— 就是这里的 ${ctx.fam.pairs} 组。`;
  el('howto').innerHTML = `<b>怎么看</b>：按 1…${ctx.fam.pairs} 换组，每组亮起的就是那一对平行面；最后一个按钮把 ${ctx.fam.count} 张面一起摆成晶形。`
    + `开 <b>一叠平行面与 d</b> 看面间距，开 <b>面内二维网格</b> 看这张面上原子怎么排、面密度多少，`
    + `<b>沿法线俯视</b> 会把两张平行面压成两条平行线。键：<b>←/→</b> 换组、<b>1…${ctx.fam.pairs + 1}</b> 直接跳、`
    + `<b>空格</b> 播放、<b>[ / ]</b> 换面族。`;
}

function formPanel() {
  const f = ctx.form;
  const rh = f.faces[0];
  const dr = diagRatio(rh.poly);
  el('infoTitle').innerHTML = `组合　${f.faceCount} 个 ${ctx.fam.label} 面围成的${formName(f)}`;
  el('infoRows').innerHTML = rowsHtml([
    ['晶形', `<b>${f.faceCount} 面 · ${f.vertCount} 顶点 · ${f.edgeCount} 棱</b>`],
    ['每个面的形状', `${rh.poly.length} 边形，面积 ${f4(rh.area)}a²`],
    ['面的内角', faceAnglesTxt(rh.poly)],
    ['对角线之比', dr === null ? '（这张面不是四边形）' : dr.toFixed(3)],
    ['原点到每张面', `d = <b>${f4(ctx.fam.members[0].d)}a</b>`],
    ['每个面的邻面', `${neighCount()} 个，法线夹角 60°（二面角 120°）`],
    ['晶带', ctx.zones.map((z) => `${z.label}·${z.members.length}面`).join('　')],
    ['本结构最密面', `<b>${densestLabel()}</b>`],
  ]);
  el('infoTable').innerHTML = membersTable(ctx.fam.members[0]) + densityTable();
  el('infoNote').innerHTML = `把 ${f.faceCount} 张面各自沿自己的法线推到离原点 d 的位置，就围成封闭的${formName(f)}。`
    + `{110} 在体心立方里是最密面，所以 BCC 的晶形和第一布里渊区都是菱形十二面体；`
    + `面心立方的最密面是 {111}（八面体），简单立方是 {100}（立方体）——同一套算法换个族照样算。`;
  el('howto').innerHTML = `<b>怎么看</b>：开 <b>晶形轮廓</b> 看 12 张面立在参考立方体的 12 条棱上；`
    + `开 <b>12 张面平移到原点</b> 看「晶面族交于一点」的原始含义；开 <b>法线端点</b> 把面指标换成方向看；`
    + `开 <b>晶带高亮</b> 再挑一根带轴，看哪几个面共用它。键：<b>←/→</b> 也能在六组与组合之间走。`;
}

/* =========================================================
   控件
   ========================================================= */
const btnHtml = (attrs, main, sub) => `<button ${attrs}>${main}${sub ? `<small>${sub}</small>` : ''}</button>`;

function buildControls() {
  el('famSel').innerHTML = FAMS.map((h) => btnHtml(`data-hkl="${h.join(',')}"`, `{${h.join('')}}`)).join('');
  el('stSel').innerHTML = Object.values(STRUCTURES).map((s) => btnHtml(`data-st="${s.key}"`, s.short, s.name)).join('');
  el('layBox').innerHTML = Object.keys(LAYERS).map((k) => `<label class="switch">${LAYER_NAMES[k] || k}`
    + `<input type="checkbox" data-lay="${k}"><span class="track"></span></label>`).join('');
  el('grpSel').innerHTML = ctx.steps.map((s, i) => (i < ctx.fam.pairs
    ? btnHtml(`data-g="${i}"`, `第 ${i + 1} 组`, s.group.label)
    : btnHtml(`data-g="${i}"`, '组合', `${ctx.fam.count} 个面`))).join('');
  el('zoneSel').innerHTML = ctx.zones.map((z, i) => btnHtml(`data-z="${i}"`, z.label, `${z.members.length} 面`)).join('');
  const hex = (n) => `#${n.toString(16).padStart(6, '0')}`;
  el('legend').innerHTML = ctx.fam.groups.map((g, i) => `<div><span class="dot" style="background:${hex(GROUP_COL[i % GROUP_COL.length])}"></span>`
    + `第 ${i + 1} 组 ${g.label}</div>`).join('')
    + '<div><span class="dot" style="background:#fb7185"></span>面间距 d 的测量线</div>'
    + '<div><span class="dot" style="background:#fde68a"></span>面内二维网胞</div>';
}

function syncToggles() {
  const L = currentLayers();
  document.querySelectorAll('#layBox input').forEach((b) => { b.checked = !!L[b.dataset.lay]; });
  document.querySelectorAll('#grpSel button').forEach((b) => b.classList.toggle('active', Number(b.dataset.g) === state.group));
  document.querySelectorAll('#famSel button').forEach((b) => b.classList.toggle('active', same3(b.dataset.hkl.split(',').map(Number), state.hkl)));
  document.querySelectorAll('#stSel button').forEach((b) => b.classList.toggle('active', b.dataset.st === state.struct));
  document.querySelectorAll('#zoneSel button').forEach((b) => b.classList.toggle('active', Number(b.dataset.z) === state.zone));
}

function refreshPanels() {
  el('stageBadge').textContent = state.demo
    ? `第 ${state.group + 1}/${ctx.steps.length} 组 · ${ctx.step.title}`
    : (ctx.isForm ? `${ctx.fam.label} 的 ${ctx.fam.count} 个面：合成一个晶形` : `${ctx.step.title}`);
  el('stepPanel').style.display = state.demo ? 'block' : 'none';
  el('hint').style.display = state.demo ? 'none' : 'block';
  el('hint').textContent = '面片、面间距、二维网胞、晶形顶点全部由密勒指数现算，不是手画的';
  el('stepTitle').textContent = `${state.group + 1} / ${ctx.steps.length}　${ctx.step.title}`;
  el('stepText').textContent = ctx.step.text;
  el('stepDots').innerHTML = ctx.steps.map((s, i) => `<button class="dot-btn${i === state.group ? ' on' : ''}" data-g="${i}" title="${s.title}">${i + 1}</button>`).join('');
  el('stepPrev').disabled = state.group === 0;
  el('stepNext').disabled = state.group === ctx.steps.length - 1;
  el('stepPlay').textContent = state.playing ? '❚❚ 暂停' : '▶ 自动播放';
  el('stepPlay').classList.toggle('on', state.playing);
  el('tDemo').checked = state.demo;
  el('equiv').innerHTML = `<b>${ctx.fam.label}</b>：${ctx.fam.count} 个晶面 · ${ctx.fam.pairs} 组平行对 · d = ${f4(ctx.fam.members[0].d)}a`;
  el('zoneCtl').style.display = ctx.zones.length && !state.demo ? 'block' : 'none';
  document.querySelectorAll('.manual-only').forEach((nd) => nd.classList.toggle('dimmed', state.demo));
  buildControls();
  syncToggles();
  if (ctx.isForm) formPanel(); else groupPanel();
}

/* =========================================================
   交互
   ========================================================= */
function goto({ group = state.group, fam = state.hkl, struct = state.struct, zone = state.zone } = {}) {
  state.hkl = fam;
  state.struct = struct;
  state.group = group;
  state.zone = zone;
  buildCtx();
  rebuild(true);
}

buildCtx();
buildControls();
rebuild(false);
setView();

let stepShownAt = performance.now();
startLoop({
  renderer, scene, camera, controls, dirty,
  onFrame: (now) => {
    if (state.demo && state.playing && now - stepShownAt > STEP_MS) {
      stepShownAt = now;
      if (state.group >= ctx.steps.length - 1) { state.playing = false; refreshPanels(); }
      else goto({ group: state.group + 1 });
    }
    return appear.tick(now);
  },
});

function clickOn(id, handler) {
  el(id).addEventListener('click', (e) => {
    const b = e.target.closest('button');
    if (b) handler(b);
  });
}
clickOn('famSel', (b) => goto({ fam: b.dataset.hkl.split(',').map(Number), group: 0, zone: 0 }));
clickOn('stSel', (b) => goto({ struct: b.dataset.st }));
clickOn('zoneSel', (b) => { state.zone = Number(b.dataset.z); rebuild(true); refreshPanels(); });
clickOn('grpSel', (b) => { state.playing = false; stepShownAt = performance.now(); goto({ group: Number(b.dataset.g) }); });
clickOn('stepDots', (b) => { state.playing = false; stepShownAt = performance.now(); goto({ group: Number(b.dataset.g) }); });
el('layBox').addEventListener('change', (e) => {
  const box = e.target.closest('input');
  if (!box) return;
  const store = ctx.isForm ? state.freeForm : state.freeGroup;
  store[box.dataset.lay] = box.checked;
  rebuild(true);
});
el('tDemo').addEventListener('change', () => {
  state.demo = el('tDemo').checked;
  if (!state.demo) state.playing = false;
  rebuild(true);
});
el('stepPrev').addEventListener('click', () => { stepShownAt = performance.now(); goto({ group: state.group - 1 }); });
el('stepNext').addEventListener('click', () => { stepShownAt = performance.now(); goto({ group: state.group + 1 }); });
el('stepPlay').addEventListener('click', () => {
  state.demo = true;
  state.playing = !state.playing;
  stepShownAt = performance.now();
  rebuild(true);
});
el('resetView').addEventListener('click', setView);
el('alongView').addEventListener('click', () => {
  const n = unit(ctx.isForm ? ctx.fam.members[0].n : ctx.step.group.n);
  camera.position.copy(v3(scaled(unit(add(n, [0.2, -0.2, 0.32])), 4.5)));
  controls.target.set(0, 0, 0);
  controls.update();
});
el('tRotate').addEventListener('change', (e) => { controls.autoRotate = e.target.checked; });

addEventListener('keydown', (e) => {
  if (e.key === 'ArrowRight' || e.key === 'ArrowLeft') {
    stepShownAt = performance.now();
    goto({ group: state.group + (e.key === 'ArrowRight' ? 1 : -1) });
    e.preventDefault();
  } else if (/^[1-9]$/.test(e.key)) {
    goto({ group: Number(e.key) - 1 });
  } else if (e.key === ' ') {
    state.demo = true;
    state.playing = !state.playing;
    stepShownAt = performance.now();
    rebuild(true);
    e.preventDefault();
  } else if (e.key === ']' || e.key === '[') {
    const i = FAMS.findIndex((f) => same3(f, state.hkl));
    goto({ fam: FAMS[(i + (e.key === ']' ? 1 : FAMS.length - 1)) % FAMS.length], group: 0 });
  }
});

/* 摄制模式：?film=1 暴露 window.__film.set({fam, group, struct, zone, layers, demo, cam}) */
if (params.has('film')) {
  installFilmAPI({
    camera, controls, renderer, scene, root,
    meta: {
      families: FAMS.map((f) => f.join('')), groups: ctx.steps.length, structs: Object.keys(STRUCTURES),
      layers: Object.keys(LAYERS), zones: ctx.zones.map((z) => z.label),
      normals: ctx.fam.groups.map((g) => g.n), labels: ctx.steps.map((s) => s.title),
    },
    apply: (p) => {
      const fam = p.fam ? (String(p.fam).includes(',') ? String(p.fam).split(',').map(Number) : [...String(p.fam)].map(Number)) : state.hkl;
      goto({
        fam: fam.length === 3 ? fam : state.hkl,
        struct: p.struct && STRUCTURES[p.struct] ? p.struct : state.struct,
        group: typeof p.group === 'number' ? p.group : state.group,
        zone: typeof p.zone === 'number' ? p.zone : state.zone,
      });
      state.demo = p.demo === undefined ? true : !!p.demo;
      if (p.layers) Object.assign(ctx.isForm ? state.freeForm : state.freeGroup, p.layers);
      if (p.all) Object.keys(LAYERS).forEach((k) => { state.freeGroup[k] = true; state.freeForm[k] = true; });
      rebuild(false);
      appear.settle();
    },
  });
}
