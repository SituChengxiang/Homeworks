// 作业 A 的 3D 图层。只画三维里才看得见的东西：轴、镜面片、等效点、手性三面角、配对线。
// 文字说明交给字幕条和 2D 图，3D 里只留必要的记号 —— 上一版把结论写在画面里，
// 结果哪儿都是字，谁也不显眼。
import * as THREE from 'three';
import { ID3, applyM, same, over } from '../../lib/matrix.js';
import { barOrbit, barGroup, powGen } from '../../lib/rotoinversion.js';
import { COLORS as C } from '../../design/tokens.js';
import { unitSphere, unitCircle, stdMat, labelSprite, tubeEdges } from '../../src/view.js';

const PM = [1.15, 0.45, 0.78];        // 一般位置的取样点：三个坐标互不相等也不为零
const TRIAD = 0.42;
const AXIS = 2.3;
const v3 = (p) => new THREE.Vector3(p[0], p[1], p[2]);

export { PM };

function sphereAt(p, color, key, r = 0.13, opacity = 1, pickId = null) {
  const m = new THREE.Mesh(unitSphere(20, 16), stdMat(`sph|${key}`, {
    color, roughness: 0.4, metalness: 0.12,
    emissive: color, emissiveIntensity: 0.14,
    transparent: opacity < 1, opacity,
  }));
  m.position.copy(v3(p));
  m.scale.setScalar(r);
  m.userData.grow = true;
  if (pickId !== null) m.userData.pickId = pickId;
  return m;
}

/** 三面角线段：按颜色分三组，每组一个 InstancedMesh */
function sticks(mats, len, base = PM, radius = 0.032, opacity = 0.95, tag = '') {
  const dirs = [[1, 0, 0], [0, 1, 0], [0, 0, 1]];
  const cols = [C.x, C.y, C.z];
  const g = new THREE.Group();
  dirs.forEach((e, i) => {
    const segs = [];
    for (const M of mats) {
      const p = applyM(M, base);
      const d = applyM(M, e);
      segs.push([v3(p), v3(p.map((x, j) => x + d[j] * len))]);
    }
    if (segs.length) g.add(tubeEdges(`st|${tag}|${i}`, segs, radius, cols[i], opacity));
  });
  return g;
}

const orbitOf = (n) => barOrbit(n, PM);
const images = (n) => orbitOf(n).pts.filter((x) => !same(x.M, ID3));

/* ---------- 各图层 ---------- */
export function layerAxis(n) {
  const g = new THREE.Group();
  const { props } = barGroup(n);
  const d = v3([0, 0, 1]);
  g.add(tubeEdges(`ax|${n}`, [[d.clone().multiplyScalar(-AXIS), d.clone().multiplyScalar(AXIS)]], 0.022, C.accent, 0.95));
  const tip = new THREE.Mesh(unitCircle(3), stdMat(`ax-tip|${n}`, { color: C.accent, roughness: 0.5, side: THREE.DoubleSide }));
  tip.position.copy(d.clone().multiplyScalar(AXIS));
  tip.lookAt(d.clone().multiplyScalar(2 * AXIS));
  tip.scale.setScalar(0.13);
  tip.userData.grow = true;
  g.add(tip);
  const lab = labelSprite(over(n), '#a5f3fc', 128, 96, 0.66);
  lab.position.set(0, 0, AXIS + 0.34);
  g.add(lab);
  g.add(sphereAt([0, 0, 0], props.hasI ? C.invert : 0x475569, `org|${n}|${props.hasI}`, 0.09, props.hasI ? 1 : 0.5));
  return g;
}

/** 定义那一步：转过的弧 + 穿过原点的反演线 */
export function layerTrace(n) {
  const g = new THREE.Group();
  const segs = [];
  const N = 26;
  const deg = 360 / n;
  const r = Math.hypot(PM[0], PM[1]);
  const a0 = Math.atan2(PM[1], PM[0]);
  for (let i = 0; i < N; i++) {
    const t0 = a0 + (deg * Math.PI / 180) * (i / N);
    const t1 = a0 + (deg * Math.PI / 180) * ((i + 1) / N);
    segs.push([new THREE.Vector3(r * Math.cos(t0), r * Math.sin(t0), PM[2]),
      new THREE.Vector3(r * Math.cos(t1), r * Math.sin(t1), PM[2])]);
  }
  g.add(tubeEdges('trace-arc', segs, 0.02, C.proper, 0.85));
  g.add(tubeEdges('trace-inv', [[v3(PM), v3(PM.map((x) => -x))]], 0.016, C.invert, 0.7));
  const l1 = labelSprite('转 ' + Math.round(deg) + '°', '#bfdbfe', 260, 64, 0.4);
  l1.position.set(PM[0] * 0.4, PM[1] * 2.2, PM[2] + 0.25);
  g.add(l1);
  const l2 = labelSprite('反演：穿过原点', '#fda4af', 300, 64, 0.4);
  l2.position.set(PM[0] * -0.5, PM[1] * -0.5, PM[2] * -0.5 - 0.4);
  g.add(l2);
  return g;
}

/** 原始基元 P 就是「恒等」那一个像，pickId 用恒等在 orbit.pts 里的下标，三处才对得上 */
export function layerMotif(n) {
  const g = new THREE.Group();
  g.add(sphereAt(PM, C.accent, `mo|${n}`, 0.15, 1, barGroup(n).props.order - 1));
  g.add(sticks([ID3], TRIAD, PM, 0.036, 0.98, `mo${n}`));
  const lab = labelSprite('P', '#a5f3fc', 96, 96, 0.5);
  lab.position.copy(v3(PM).add(v3([0, 0, 0.34])));
  g.add(lab);
  return g;
}

export function layerOrbit(n) {
  const g = new THREE.Group();
  const imgs = images(n);
  g.add(sticks(imgs.map((x) => x.M), TRIAD, PM, 0.03, 0.85, `ob${n}`));
  imgs.forEach((x) => {
    g.add(sphereAt(x.p, x.det > 0 ? C.image : C.imageImp, `ob|${n}|${x.k}`, 0.12, 0.95, x.k - 1));
  });
  return g;
}

export function layerNumbers(n) {
  const g = new THREE.Group();
  for (const x of images(n)) {
    const s = labelSprite(String(x.k), '#e2e8f0', 84, 84, 0.42);
    s.position.copy(v3(x.p).add(v3([0, 0, 0.3])));
    g.add(s);
  }
  return g;
}

/** 配对线：穿心（穿过原点）与上下相对（同 (x,y)、z 反号） */
export function layerPairs(n) {
  const o = orbitOf(n);
  const g = new THREE.Group();
  const segsI = [];
  const segsM = [];
  for (const [a, b, kind] of o.pairs) {
    (kind === 'i' ? segsI : segsM).push([v3(o.pts[a].p), v3(o.pts[b].p)]);
  }
  if (segsI.length) g.add(tubeEdges(`prI|${n}`, segsI, 0.018, C.invert, 0.85));
  if (segsM.length) {
    g.add(tubeEdges(`prM|${n}`, segsM, 0.018, C.mirror, 0.85));
    for (const [a, b] of o.pairs.filter((p) => p[2] === 'm')) {
      const p = o.pts[a].p;
      g.add(sphereAt([p[0], p[1], 0], C.mirror, `prMd|${n}|${a}`, 0.055, 0.9));
    }
  }
  return g;
}

export function layerMirror(n) {
  const g = new THREE.Group();
  const disc = new THREE.Mesh(unitCircle(44), stdMat(`disc|${n}`, {
    color: C.mirror, roughness: 0.9, transparent: true, opacity: 0.15,
    side: THREE.DoubleSide, depthWrite: false,
  }));
  disc.scale.setScalar(1.95);
  disc.userData.grow = true;
  g.add(disc);
  const segs = [];
  const N = 44;
  for (let i = 0; i < N; i++) {
    const t0 = (i / N) * Math.PI * 2;
    const t1 = ((i + 1) / N) * Math.PI * 2;
    segs.push([new THREE.Vector3(1.95 * Math.cos(t0), 1.95 * Math.sin(t0), 0),
      new THREE.Vector3(1.95 * Math.cos(t1), 1.95 * Math.sin(t1), 0)]);
  }
  g.add(tubeEdges(`rim|${n}`, segs, 0.011, C.mirror, 0.6));
  const lab = labelSprite('m⊥', '#a5f3c4', 128, 80, 0.5);
  lab.position.set(1.62, 0.5, 0.12);
  g.add(lab);
  return g;
}

export function layerCenter(n) {
  const g = new THREE.Group();
  const halo = new THREE.Mesh(unitSphere(16, 12), stdMat(`halo|${n}`, {
    color: C.invert, emissive: C.invert, emissiveIntensity: 0.5,
    transparent: true, opacity: 0.35, roughness: 0.6,
  }));
  halo.scale.setScalar(0.2);
  halo.userData.grow = true;
  g.add(halo);
  const lab = labelSprite('i', '#fda4af', 96, 96, 0.5);
  lab.position.set(0.42, 0.42, -0.36);
  g.add(lab);
  return g;
}

/** 一步一步施加：当前姿态由 main.js 写进 matrix */
export function layerLive(n) {
  const g = new THREE.Group();
  g.add(sphereAt(PM, C.accent, `live|${n}`, 0.15, 1));
  g.add(sticks([ID3], TRIAD, PM, 0.036, 0.98, `live${n}`));
  return g;
}

export function layerGhost(n, k) {
  const g = new THREE.Group();
  for (let j = 1; j < k; j++) {
    const M = powGen(n, j);
    g.add(sphereAt(applyM(M, PM), C.ghost, `gh|${n}|${j}`, 0.1, 0.42));
    g.add(sticks([M], TRIAD * 0.8, PM, 0.022, 0.3, `gh${n}|${j}`));
  }
  return g;
}

/** 图层键 → 构造器。main.js 只认这张表。 */
export const BUILDERS = {
  axis: (n) => layerAxis(n),
  trace: (n) => layerTrace(n),
  motif: (n) => layerMotif(n),
  orbit: (n) => layerOrbit(n),
  numbers: (n) => layerNumbers(n),
  pairs: (n) => layerPairs(n),
  mirror: (n) => (barGroup(n).props.hasMirrorPerp ? layerMirror(n) : new THREE.Group()),
  center: (n) => (barGroup(n).props.hasI ? layerCenter(n) : new THREE.Group()),
  live: (n) => layerLive(n),
  ghost: (n, k) => layerGhost(n, k),
};
