// 纯几何/物理计算（不依赖 DOM，可在 Node 中单独测试）
import * as THREE from 'three';

export const A = 2;        // 显示用晶格常数
export const HALF = A / 2;

export const v = (x, y, z) => new THREE.Vector3(x, y, z);

/** 三种结构的初级格矢 */
export function primitiveVectors(type) {
  if (type === 'SC')  return [v(A, 0, 0), v(0, A, 0), v(0, 0, A)];
  if (type === 'BCC') return [v(-A / 2, A / 2, A / 2), v(A / 2, -A / 2, A / 2), v(A / 2, A / 2, -A / 2)];
  return [v(0, A / 2, A / 2), v(A / 2, 0, A / 2), v(A / 2, A / 2, 0)]; // FCC
}

/* 这些都是纯函数，结果只跟参数有关；缓存起来，交互重建时不必再算一遍。
   返回的 Vector3 请一律视为只读。 */
const pointCache = new Map();
const dminCache = new Map();
const wsCache = new Map();
const atomCache = new Map();
const edgeCache = new WeakMap();
const statsCache = new WeakMap();

/** 以原点为参考格点，列出 maxDist 内的所有格点（去重）。返回值请视为只读。 */
export function latticePoints(type, maxDist) {
  const key = `${type}|${maxDist.toFixed(6)}`;
  let hit = pointCache.get(key);
  if (!hit) { hit = buildLatticePoints(type, maxDist); pointCache.set(key, hit); }
  return hit;
}

function buildLatticePoints(type, maxDist) {
  const [p1, p2, p3] = primitiveVectors(type);
  const raw = [];
  for (let i = -4; i <= 4; i++)
    for (let j = -4; j <= 4; j++)
      for (let k = -4; k <= 4; k++) {
        if (i === 0 && j === 0 && k === 0) continue;
        const r = v(0, 0, 0)
          .addScaledVector(p1, i)
          .addScaledVector(p2, j)
          .addScaledVector(p3, k);
        if (r.length() <= maxDist) raw.push(r);
      }
  const uniq = [];
  for (const r of raw)
    if (!uniq.some((q) => q.distanceToSquared(r) < 1e-9)) uniq.push(r);
  return uniq;
}

export function nearestDistance(type) {
  let hit = dminCache.get(type);
  if (hit === undefined) {
    hit = Math.min(...latticePoints(type, 3 * A).map((p) => p.length()));
    dminCache.set(type, hit);
  }
  return hit;
}

/* ---------- 由半空间交求凸多面体 ---------- */
export function faceNormal(poly) {
  const n = v(0, 0, 0);
  for (let i = 0; i < poly.length; i++) {
    const a = poly[i], b = poly[(i + 1) % poly.length];
    n.x += (a.y - b.y) * (a.z + b.z);
    n.y += (a.z - b.z) * (a.x + b.x);
    n.z += (a.x - b.x) * (a.y + b.y);
  }
  return n.normalize();
}

/** 三平面交点（克拉默法则）；平行/退化时返回 null */
function intersect3(p1, p2, p3) {
  const a = p1.n, b = p2.n, c = p3.n;
  const det = a.dot(new THREE.Vector3().crossVectors(b, c));
  if (Math.abs(det) < 1e-9) return null;
  const d1 = p1.d, d2 = p2.d, d3 = p3.d;
  const bc = new THREE.Vector3().crossVectors(b, c);
  const ca = new THREE.Vector3().crossVectors(c, a);
  const ab = new THREE.Vector3().crossVectors(a, b);
  return bc.multiplyScalar(d1).add(ca.multiplyScalar(d2)).add(ab.multiplyScalar(d3)).divideScalar(det);
}

/** 把共面点按绕质心的角度排序，得到逆时针面多边形 */
function orderFace(points, normal) {
  const c = v(0, 0, 0);
  points.forEach((p) => c.add(p));
  c.multiplyScalar(1 / points.length);

  const helper = Math.abs(normal.x) > 0.9 ? v(0, 1, 0) : v(1, 0, 0);
  const u = new THREE.Vector3().crossVectors(normal, helper).normalize();
  const w = new THREE.Vector3().crossVectors(normal, u).normalize();

  const sorted = [...points].sort((p, q) => {
    const pa = Math.atan2(w.dot(p.clone().sub(c)), u.dot(p.clone().sub(c)));
    const qa = Math.atan2(w.dot(q.clone().sub(c)), u.dot(q.clone().sub(c)));
    return pa - qa;
  });
  if (faceNormal(sorted).dot(normal) < 0) sorted.reverse();
  return sorted;
}

export function computeWS(type) {
  let hit = wsCache.get(type);
  if (!hit) { hit = buildWS(type); wsCache.set(type, hit); }
  return hit;
}

/**
 * 计算 WS 原胞：对最近邻 + 次近邻作中垂面，
 * 求所有“三面交点”中满足全部半空间约束的点（= 多面体顶点），
 * 再按平面回收各个面。
 */
function buildWS(type) {
  const dmin = nearestDistance(type);
  const neighbors = latticePoints(type, 1.35 * dmin);
  const planes = neighbors.map((r) => ({ n: r.clone().normalize(), d: r.length() / 2 }));
  const EPS = 1e-6;

  // 顶点 = 三平面交点 ∩ 全部半空间
  const verts = [];
  for (let i = 0; i < planes.length; i++)
    for (let j = i + 1; j < planes.length; j++)
      for (let k = j + 1; k < planes.length; k++) {
        const p = intersect3(planes[i], planes[j], planes[k]);
        if (!p) continue;
        if (!planes.every((q) => q.n.dot(p) - q.d <= EPS)) continue;
        if (!verts.some((q) => q.distanceToSquared(p) < 1e-9)) verts.push(p);
      }

  // 面 = 落在该平面上的所有顶点
  const faces = [];
  const faceNeighbors = []; // 真正贡献了“面”的近邻（= 与中心胞共面的格点）
  planes.forEach((q, idx) => {
    const on = verts.filter((p) => Math.abs(q.n.dot(p) - q.d) < EPS);
    if (on.length < 3) return;
    faces.push(orderFace(on, q.n));
    faceNeighbors.push(neighbors[idx]);
  });

  return { faces, neighbors, faceNeighbors, dmin };
}

/** 去重后的棱（每条棱只保留一次） */
export function uniqueEdges(faces) {
  let hit = edgeCache.get(faces);
  if (hit) return hit;
  const map = new Map();
  const key = (p) => `${p.x.toFixed(4)},${p.y.toFixed(4)},${p.z.toFixed(4)}`;
  for (const f of faces)
    for (let i = 0; i < f.length; i++) {
      const a = f[i], b = f[(i + 1) % f.length];
      const ka = key(a), kb = key(b);
      map.set(ka < kb ? ka + '|' + kb : kb + '|' + ka, [a, b]);
    }
  hit = [...map.values()];
  edgeCache.set(faces, hit);
  return hit;
}

export function wsStats(faces) {
  let hit = statsCache.get(faces);
  if (hit) return hit;
  const vset = new Map();
  const key = (p) => `${p.x.toFixed(4)},${p.y.toFixed(4)},${p.z.toFixed(4)}`;
  const eset = new Set();
  for (const f of faces)
    for (let i = 0; i < f.length; i++) {
      const a = f[i], b = f[(i + 1) % f.length];
      vset.set(key(a), a);
      const ka = key(a), kb = key(b);
      eset.add(ka < kb ? ka + '|' + kb : kb + '|' + ka);
    }
  hit = { V: vset.size, E: eset.size, F: faces.length };
  statsCache.set(faces, hit);
  return hit;
}

/* ---------- 惯用晶胞中的原子 ---------- */
export function atomsFor(type) {
  let hit = atomCache.get(type);
  if (!hit) { hit = buildAtoms(type); atomCache.set(type, hit); }
  return hit;
}

function buildAtoms(type) {
  const atoms = [];
  for (const x of [-HALF, HALF])
    for (const y of [-HALF, HALF])
      for (const z of [-HALF, HALF])
        atoms.push({ pos: v(x, y, z), kind: 'corner' });

  if (type === 'BCC') atoms.push({ pos: v(0, 0, 0), kind: 'body' });
  if (type === 'FCC') {
    [v(HALF, 0, 0), v(-HALF, 0, 0), v(0, HALF, 0), v(0, -HALF, 0), v(0, 0, HALF), v(0, 0, -HALF)]
      .forEach((p) => atoms.push({ pos: p, kind: 'face' }));
  }
  return atoms;
}

/** 刚球接触时的原子半径 */
export function touchRadius(type) {
  if (type === 'SC') return A / 2;
  if (type === 'BCC') return (Math.sqrt(3) / 4) * A;
  return (Math.sqrt(2) / 4) * A;
}

/* ---------- 金刚石 / 闪锌矿：两套 FCC 沿体对角线错开 ---------- */

/** FCC 的 4 个基元位置（一个晶胞里的 4 个格点） */
export function fccBasis(s = A) {
  return [v(0, 0, 0), v(0, s / 2, s / 2), v(s / 2, 0, s / 2), v(s / 2, s / 2, 0)];
}

/**
 * 第二套 FCC：整支亚点阵沿体对角线 [111] 平移 t·a（t = 1/4 即金刚石 / 闪锌矿）。
 * 平移向量长度 = √3·t·a；t = 1/4 时恰好等于最近邻（键长）√3a/4。
 */
export function shiftedBasis(t, s = A) {
  const d = v(t, t, t).multiplyScalar(s);
  return fccBasis(s).map((p) => p.clone().add(d));
}

/** 键长（最近邻 A–B 距离），t = 1/4 时为 √3a/4 */
export function basisDistance(t, s = A) {
  return Math.sqrt(3) * t * s;
}

/** 连接 A、B 两套亚点阵里距离不超过 maxLen 的原子对 */
export function pairsWithin(listA, listB, maxLen) {
  const out = [];
  let dmin = Infinity;
  for (const a of listA)
    for (const b of listB) {
      const d = a.distanceTo(b);
      if (d < dmin) dmin = d;
    }
  const cut = Math.min(maxLen, dmin * 1.02);
  for (const a of listA)
    for (const b of listB)
      if (a.distanceTo(b) <= cut + 1e-9) out.push([a, b]);
  return { bonds: out, dmin, cut };
}

/** 分数坐标（单位 a），便于和教材上的原子位置对照 */
export function fractionalOf(p, s = A) {
  return [p.x, p.y, p.z].map((c) => c / s);
}

/** 球体积（Å³） */
export const sphereVolume = (r) => (4 / 3) * Math.PI * r ** 3;

/* =========================================================
   六方密堆积 HCP
   约定：底面边长 a（= 相邻原子间距 = 2R），棱柱高 c，晶胞中心在原点。
   ========================================================= */

/** 理想轴比 c/a = √(8/3) = 1.63299…，由正四面体推出（见 hcpTetra） */
export const IDEAL_CA = Math.sqrt(8 / 3);

/** 六方棱柱体积 = 底面积 × 高 = (3√3/2)·a²·c */
export function hexPrismVolume(a = A, cOverA = IDEAL_CA) {
  return ((3 * Math.sqrt(3)) / 2) * a * a * (a * cOverA);
}

/**
 * 六方密堆积晶胞里的原子（棱柱：底面 6 个顶角 + 上下底面中心 + 中间层 3 个）：
 * 12 个顶角（z = 0 与 z = c 两个面各 6 个）+ 2 个底面中心 + 3 个中间层原子。
 * kind 决定份额：corner 1/6、base 1/2、mid 1。坐标约定与 hcpLattice 一致（A 层在 z = 0）。
 */
export function hcpAtoms(a = A, cOverA = IDEAL_CA) {
  const c = a * cOverA;
  const out = [];
  for (const z of [0, c]) {
    out.push({ pos: v(0, 0, z), kind: 'base' });
    for (let k = 0; k < 6; k++) {
      const th = (k * Math.PI) / 3;
      out.push({ pos: v(a * Math.cos(th), a * Math.sin(th), z), kind: 'corner' });
    }
  }
  // 中间层（B 层）：落在底面三角形空隙正上方，半径 a/√3、互差 120°
  const rHollow = a / Math.sqrt(3);
  for (let k = 0; k < 3; k++) {
    const th = Math.PI / 6 + (k * 2 * Math.PI) / 3;
    out.push({ pos: v(rHollow * Math.cos(th), rHollow * Math.sin(th), c / 2), kind: 'mid' });
  }
  return out;
}

/**
 * 完整 HCP 点阵（用于数配位数、铺点云）：
 * A 层 = 二维六方格点 i·a1 + j·a2 + k·a3；B 层 = A 层平移 (a1 + a2)/3 + c/2。
 */
export function hcpLattice(a = A, cOverA = IDEAL_CA, maxR = 2 * A) {
  const c = a * cOverA;
  const a1 = v(a, 0, 0);
  const a2 = v(a / 2, (Math.sqrt(3) / 2) * a, 0);
  const a3 = v(0, 0, c);
  const bShift = a1.clone().add(a2).multiplyScalar(1 / 3).addScaledVector(a3, 0.5);
  const N = Math.ceil(maxR / Math.min(a, c)) + 1;
  const out = [];
  for (let i = -N; i <= N; i++)
    for (let j = -N; j <= N; j++)
      for (let k = -N; k <= N; k++) {
        const p = v(0, 0, 0).addScaledVector(a1, i).addScaledVector(a2, j).addScaledVector(a3, k);
        for (const q of [p, p.clone().add(bShift)]) {
          if (q.length() <= maxR && !out.some((r) => r.distanceToSquared(q) < 1e-9)) out.push(q);
        }
      }
  return out;
}

/** 某个原子的最近邻个数（在完整点阵里数，本胞边界效应不影响原点这个原子） */
export function hcpCoordination(a = A, cOverA = IDEAL_CA) {
  const pts = hcpLattice(a, cOverA, 2.2 * a);
  const origin = v(0, 0, 0);
  let dmin = Infinity;
  for (const p of pts) if (p.length() > 1e-9) dmin = Math.min(dmin, p.length());
  return pts.filter((p) => p.length() > 1e-9 && Math.abs(p.length() - dmin) < 1e-6).length;
}

/**
 * 份额裁剪面：返回 { n, d }，保留区域为 n·x + d ≥ 0（与 three.js 的裁剪约定一致）。
 *   顶角 1/6 → 两条棱柱侧面（法向在顶角方位角 ±30°，到轴心距离 = 六边形内切距 √3a/2）+ 一条底面
 *   底面中心 1/2 → 一条底面
 *   中间层 1 → 不裁
 */
export function hcpSharePlanes(item, a = A, cOverA = IDEAL_CA) {
  const c = a * cOverA;
  const apothem = (Math.sqrt(3) / 2) * a;
  const out = [];
  if (item.kind === 'corner') {
    const th = Math.atan2(item.pos.y, item.pos.x);
    for (const off of [-Math.PI / 6, Math.PI / 6]) {
      const n = th + off;
      out.push({ n: [-Math.cos(n), -Math.sin(n), 0], d: apothem });
    }
    out.push(item.pos.z < c / 2 ? { n: [0, 0, 1], d: 0 } : { n: [0, 0, -1], d: c });
  } else if (item.kind === 'base') {
    out.push(item.pos.z < c / 2 ? { n: [0, 0, 1], d: 0 } : { n: [0, 0, -1], d: c });
  }
  return out;
}

/** 每个位置的份额：顶角被 6 个棱柱共用、底面中心被 2 个共用、中间层整个在本胞内 */
export const HCP_SHARE = { corner: 1 / 6, base: 1 / 2, mid: 1 };

/** 本胞内原子数 = Σ份额 = 12×(1/6) + 2×(1/2) + 3×1 = 6 */
export function hcpAtomsPerCell(a = A, cOverA = IDEAL_CA) {
  return hcpAtoms(a, cOverA).reduce((s, x) => s + HCP_SHARE[x.kind], 0);
}

/** 同层最近邻距离（= a）与层间距（= √(a²/3 + c²/4)，即球心到球的斜边） */
export function hcpDistances(a = A, cOverA = IDEAL_CA) {
  const c = a * cOverA;
  return { inPlane: a, betweenLayers: Math.sqrt((a * a) / 3 + (c * c) / 4), layerGap: c / 2 };
}

/**
 * 刚球半径：轴比理想时层内与层间接触都是 a；
 * c/a 偏离后取「最先碰到的那一组」，所以 R = min(a, 层间距离)/2。
 */
export function hcpRadius(a = A, cOverA = IDEAL_CA) {
  const d = hcpDistances(a, cOverA);
  return Math.min(d.inPlane, d.betweenLayers) / 2;
}

/** 堆积率 = 本胞内 6 个球的体积 / 棱柱体积；理想轴比时正好等于 √2π/6 = 74.05% */
export function hcpPackingFraction(a = A, cOverA = IDEAL_CA) {
  const R = hcpRadius(a, cOverA);
  return (hcpAtomsPerCell(a, cOverA) * sphereVolume(R)) / hexPrismVolume(a, cOverA);
}

/**
 * c/a 从哪来：底面 3 个互相接触的原子（中心 + 相邻两顶角）与中间层 1 个原子
 * 构成正四面体，棱长 a。底面三角形的外接圆半径 a/√3、四面体高 √(2/3)·a，
 * 而层间距就是这个高，c = 2 × 层间距 → c/a = √(8/3)。
 */
export function hcpTetra(a = A) {
  const inPlaneR = a / Math.sqrt(3);          // 三角形中心到三个顶点
  const height = Math.sqrt(a * a - inPlaneR * inPlaneR); // = √(2/3)·a
  return { edge: a, inPlaneR, height, cOverA: (2 * height) / a };
}

/** 同一组原子内部的所有最近邻对（先找最短距离，再按 2% 容差收） */
export function nearestNeighborPairs(list, maxLen = Infinity, tol = 1.02) {
  let dmin = Infinity;
  for (let i = 0; i < list.length; i++)
    for (let j = i + 1; j < list.length; j++) dmin = Math.min(dmin, list[i].distanceTo(list[j]));
  const out = [];
  for (let i = 0; i < list.length; i++)
    for (let j = i + 1; j < list.length; j++) {
      const d = list[i].distanceTo(list[j]);
      if (d <= Math.min(maxLen, dmin * tol)) out.push([list[i], list[j]]);
    }
  return { pairs: out, dmin };
}

/** 真实金属的六方晶格常数（Å），用于对比理想与非理想轴比 */
export const HCP_METALS = [
  { key: 'ideal', name: '理想 HCP', a: null, c: null, note: 'c/a = √(8/3) = 1.633，层内层间同时相切，堆积率正好 74.05%。理想模型没有具体元素，右侧 Å 数值按 a = 3.209 Å（Mg）作标尺显示。' },
  { key: 'Mg', name: '镁 Mg', a: 3.209, c: 5.211, note: '最接近理想轴比的金属之一' },
  { key: 'Zn', name: '锌 Zn', a: 2.665, c: 4.947, note: 'c/a = 1.856，沿 c 轴被拉长，堆积率明显掉下来' },
  { key: 'Cd', name: '镉 Cd', a: 2.979, c: 5.610, note: 'c/a = 1.883，偏离更大' },
  { key: 'Ti', name: '钛 Ti', a: 2.951, c: 4.686, note: 'c/a = 1.588，比理想值略扁' },
  { key: 'Be', name: '铍 Be', a: 2.286, c: 3.584, note: 'c/a = 1.568，最扁的一个' },
];

/**
 * 以本胞中心为原点枚举 FCC 的原子位置：坐标和为奇数的 (a/2) 整数倍点。
 * 这样枚举出来的集合与 atomsFor('FCC') 的胞内位置完全一致，可直接铺成点云。
 */
export function fccAtomSet(maxR, unit = A / 2) {
  const R = Math.ceil(maxR / unit);
  const out = [];
  for (let i = -R; i <= R; i++)
    for (let j = -R; j <= R; j++)
      for (let k = -R; k <= R; k++) {
        if (((i + j + k) & 1) === 0) continue;
        const p = v(i * unit, j * unit, k * unit);
        if (p.length() <= maxR) out.push(p);
      }
  return out;
}

/** 统计惯用晶胞内原子数（按分数贡献） */
export function atomsPerCell(type) {
  let n = 0;
  for (const a of atomsFor(type)) {
    if (a.kind === 'corner') n += 1 / 8;
    else if (a.kind === 'face') n += 1 / 2;
    else n += 1;
  }
  return n;
}
