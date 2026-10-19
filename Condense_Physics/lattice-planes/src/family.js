// 立方晶系晶面族 {hkl} 的数学内核：族展开、平行面对、面间距、晶形多面体、
// 面内二维网胞、晶带。单位一律取晶格常数 a = 1。
//
// 与 lib/matrix.js 一样刻意不 import three.js：页面渲染、分步文案、test/verify.js
// 用的是同一份定义——「{110} 有 12 个面、正好配成 6 对平行面、d = a/√2、合成菱形十二面体」
// 这些话全部由这里算出来，不写在文案里。
import { over, norm3, det, planeBasis, project, applyM } from '../../lib/matrix.js';

const TOL = 1e-9;
const DEG = 180 / Math.PI;

/* ---------- 小工具：三维向量算术（这里不需要单位化，交给 norm3） ---------- */
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const cross = (a, b) => [
  a[1] * b[2] - a[2] * b[1],
  a[2] * b[0] - a[0] * b[2],
  a[0] * b[1] - a[1] * b[0],
];
const sub = (a, b) => a.map((x, i) => x - b[i]);
const add = (a, b) => a.map((x, i) => x + b[i]);
const mulS = (a, s) => a.map((x) => x * s);
const len = (a) => Math.hypot(a[0], a[1], a[2]);
const sameV = (a, b, tol = 1e-9) => a.every((x, i) => Math.abs(x - b[i]) < tol);
const gcd = (a, b) => (b === 0 ? Math.abs(a) : gcd(b, a % b));
const keyOf = (v) => v.map((x) => Math.round(x * 1e6) / 1e6).join(',');

/* =========================================================
   1. 立方族的展开：用完整点群 m3m 的 48 个操作作用在 (hkl) 上
   ========================================================= */
/** 置换的奇偶（带号置换阵的行列式 = 奇偶 × 三个符号之积） */
function permParity(p) {
  let n = 0;
  for (let i = 0; i < 3; i++) for (let j = i + 1; j < 3; j++) if (p[i] > p[j]) n++;
  return n % 2 ? -1 : 1;
}

const PERMS = [[0, 1, 2], [0, 2, 1], [1, 0, 2], [1, 2, 0], [2, 0, 1], [2, 1, 0]];
const signedPerm = (p, s) => {
  const R = [[0, 0, 0], [0, 0, 0], [0, 0, 0]];
  for (let i = 0; i < 3; i++) R[i][p[i]] = s[i];
  return R;
};
const ALL_SIGNS = [0, 1, 2, 3, 4, 5, 6, 7].map((m) => [0, 1, 2].map((i) => (m & (1 << i) ? -1 : 1)));

/** 带号置换矩阵里 det = +1 的那 24 个 = 立方群 O 的真旋转 */
let rotCache = null;
export function cubicRotations() {
  if (rotCache) return rotCache;
  rotCache = [];
  for (const p of PERMS) {
    const par = permParity(p);
    for (const s of ALL_SIGNS) {
      if (par * s[0] * s[1] * s[2] !== 1) continue;        // 只留真旋转
      rotCache.push(signedPerm(p, s));
    }
  }
  return rotCache;                                        // 24 个
}

/**
 * 完整点群 Oh = m3m 的 48 个操作。晶面族是按整个点群展开的：
 * 一般位置的 {123} 因此有 48 个成员；{100}{110}{111}{112} 这些高对称族两种取法结果一样。
 */
let opCache = null;
export function cubicOperations() {
  if (opCache) return opCache;
  opCache = [];
  for (const p of PERMS) for (const s of ALL_SIGNS) opCache.push(signedPerm(p, s));
  return opCache;                                         // 6 × 8 = 48 个
}

/** 负指标写成 (1̄10)：画布贴图和 node 里都用组合上划线，HTML 面板另有 .ov 类 */
export const planeLabel = (hkl) => `(${hkl.map((x) => (x < 0 ? over(Math.abs(x)) : String(x))).join('')})`;
export const dirLabel = (v) => `[${v.map((x) => (x < 0 ? over(Math.abs(x)) : String(x))).join('')}]`;

/** 同族的互相平行的面归一组：把 (h̄k̄l̄) 折回 (hkl) */
function canonical(v) {
  for (const x of v) {
    if (x > 0) return v;
    if (x < 0) return v.map((y) => -y);
  }
  return v;
}

/** 教材里习惯的读法：先零位靠后、再负号少、最后按指标 */
function sortKey(v) {
  const neg = v.filter((x) => x < 0).length;
  const zero = v.findIndex((x) => x === 0);
  return [neg, zero < 0 ? 3 : -zero, v[0], v[1], v[2]];
}
const cmpKey = (a, b) => {
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return a[i] - b[i];
  return 0;
};

/**
 * 晶面族：{110} → 12 个成员、6 对平行面；{100} → 6/3；{111} → 8/4；{112} → 24/12。
 * 成员上的 t = ±1 是面方程 h x + k y + l z = t 的右端，也就是晶形上那张面所在的位置。
 */
export function family(hkl) {
  const uniq = [];
  const seen = new Set();
  for (const R of cubicOperations()) {
    const v = applyM(R, hkl).map(Math.round);
    const k = v.join(',');
    if (seen.has(k)) continue;
    seen.add(k);
    uniq.push(v);
  }
  const gmap = new Map();
  for (const v of uniq) {
    const c = canonical(v);
    const k = c.join(',');
    if (!gmap.has(k)) gmap.set(k, { hkl: c, label: planeLabel(c), dir: dirLabel(c), members: [] });
  }
  const groups = [...gmap.values()];
  groups.sort((a, b) => cmpKey(sortKey(a.hkl), sortKey(b.hkl)));
  groups.forEach((g, i) => { g.i = i; });

  const members = [];
  for (const v of uniq) {
    const g = groups.find((x) => x.hkl.join(',') === canonical(v).join(','));
    const sign = sameV(v, g.hkl) ? 1 : -1;
    const L = len(v);
    const m = {
      hkl: v,
      label: planeLabel(v),
      dir: dirLabel(v),
      group: g.i,
      groupLabel: g.label,
      sign,
      // 每个成员自己的面方程一律写成 h x + k y + l z = 1：
      // (1̄1̄0) 就是 −x − y = 1，即 x + y = −1，天然落在原点的另一侧。
      t: 1,
      L,
      n: v.map((x) => x / L),        // 单位法线 = 晶向 [hkl]
      d: 1 / L,                      // 面间距 d = a / √(h²+k²+l²)
    };
    g.members.push(m);
    members.push(m);
  }
  members.sort((a, b) => a.group - b.group || b.sign - a.sign);
  groups.forEach((g) => {
    g.members.sort((a, b) => b.sign - a.sign);
    g.d = g.members[0].d;
    g.n = g.members[0].n;
  });
  return { hkl, label: planeLabel(hkl), members, groups, count: members.length, pairs: groups.length };
}

/** 两个面法线之间的夹角（度）；180° 就是那一对互相平行的面 */
export function angleBetween(m1, m2) {
  return Math.round(Math.acos(Math.max(-1, Math.min(1, dot(m1.n, m2.n)))) * DEG * 1e4) / 1e4;
}

/* =========================================================
   2. 晶形：12 个面各自 n·r ≤ 1 围出来的多面体
   ========================================================= */
function solve3(rows, rhs) {
  const D = det(rows);
  if (Math.abs(D) < 1e-9) return null;
  return [0, 1, 2].map((j) => {
    const M = rows.map((r, i) => r.map((x, k) => (k === j ? rhs[i] : x)));
    return det(M) / D;
  });
}

const polyArea = (poly, basis) => {
  const q = poly.map((p) => project(p, basis));
  let A = 0;
  for (let i = 0; i < q.length; i++) {
    const a = q[i]; const b = q[(i + 1) % q.length];
    A += a.u * b.v - b.u * a.v;
  }
  return Math.abs(A) / 2;
};

/** 多边形内角（度），用来验菱形的 70.53°/109.47° */
export function polyAngles(poly) {
  const out = [];
  const n = poly.length;
  for (let i = 0; i < n; i++) {
    const a = sub(poly[(i + n - 1) % n], poly[i]);
    const b = sub(poly[(i + 1) % n], poly[i]);
    const c = dot(a, b) / (len(a) * len(b));
    out.push(Math.round(Math.acos(Math.max(-1, Math.min(1, c))) * DEG * 1e4) / 1e4);
  }
  return out;
}

/**
 * 把整个族当作晶形围起来：{r : h x + k y + l z ≤ 1 对族里每个面都成立}。
 * {110} 得到菱形十二面体（14 顶点 / 12 个菱形面 / 24 条棱），
 * {100} 得到立方体，{111} 得到八面体——同一个算法，不写死任何一个形状。
 */
export function formOf(fam) {
  const cons = fam.members;
  const verts = [];
  const vidx = new Map();
  const push = (p) => {
    const k = keyOf(p);
    if (!vidx.has(k)) { vidx.set(k, verts.length); verts.push(p); }
    return vidx.get(k);
  };
  for (let a = 0; a < cons.length; a++) {
    for (let b = a + 1; b < cons.length; b++) {
      for (let c = b + 1; c < cons.length; c++) {
        const p = solve3([cons[a].hkl, cons[b].hkl, cons[c].hkl], [1, 1, 1]);
        if (!p) continue;
        if (!cons.every((k) => dot(k.hkl, p) <= 1 + 1e-9)) continue;
        push(p);
      }
    }
  }
  const faces = cons.map((m) => {
    const basis = planeBasis(m.hkl);
    const idx = verts.map((p, j) => ({ p, j })).filter(({ p }) => Math.abs(dot(m.hkl, p) - 1) < 1e-9);
    const cen = mulS(idx.map(({ p }) => p).reduce(add, [0, 0, 0]), 1 / idx.length);
    const withAng = idx.map(({ p, j }) => {
      const q = project(sub(p, cen), basis);
      return { j, p, ang: Math.atan2(q.v, q.u) };
    }).sort((x, y) => x.ang - y.ang);
    const poly = withAng.map((x) => x.p);
    return {
      member: m,
      group: m.group,
      vertices: withAng.map((x) => x.j),
      poly,
      center: cen,
      area: polyArea(poly, basis),
      // 棱用顶点下标存：两张面共用同一条棱时，[a,b] 的键自然相同（用坐标字符串就没法反查顶点）
      edges: withAng.map((x, i) => [x.j, withAng[(i + 1) % withAng.length].j].sort((p, q) => p - q).join('|')),
      basis,
    };
  });
  const edges = [...new Set(faces.flatMap((f) => f.edges))].map((k) => k.split('|').map(Number));
  return { verts, faces, edges, vertCount: verts.length, faceCount: faces.length, edgeCount: edges.length };
}

/* =========================================================
   3. 把一张面切成「晶体里真的存在的那个截面」：与 |x|,|y|,|z| ≤ half 的方块求交
   ========================================================= */
/**
 * 把一张面切成「晶体里真的存在的那个截面」：与 |x|,|y|,|z| ≤ half 的方块求交。
 * normal 用整数指标，t 是面方程 h x + k y + l z = t 的右端（与 pointsInPatch 同一套约定），
 * 所以基准点是 normal · t / |normal|²，到原点的距离正好是 t/|normal| = t·d。
 */
export function planePatch(normal, t, half, span = 6) {
  const base = mulS(normal, t / dot(normal, normal));
  const basis = planeBasis(normal);
  let poly = [[-span, -span], [span, -span], [span, span], [-span, span]]
    .map(([u, v]) => add(base, add(mulS(basis.u, u), mulS(basis.v, v))));
  // 六个半空间：s·x[axis] ≤ half，即 g(p) = s·p[axis] − half ≤ 0 的一侧留着
  const g = (p, axis, s) => s * p[axis] - half;
  const inter = (a, b, axis, s) => {
    const fa = g(a, axis, s);
    const fb = g(b, axis, s);
    return add(a, mulS(sub(b, a), fa / (fa - fb)));
  };
  for (const axis of [0, 1, 2]) {
    for (const s of [1, -1]) {
      const out = [];
      const N = poly.length;
      for (let i = 0; i < N; i++) {
        const a = poly[i];
        const b = poly[(i + 1) % N];
        const ain = g(a, axis, s) <= 1e-9;
        const bin = g(b, axis, s) <= 1e-9;
        if (ain) out.push(a);
        if (ain !== bin) out.push(inter(a, b, axis, s));
      }
      poly = out;
      if (!poly.length) return [];
    }
  }
  // 合并重合点、去掉共线点，多边形才干净（{111} 那种正好过 cube 三个角的退化情形也稳在这里处理）
  const dedup = [];
  for (const p of poly) {
    if (!dedup.length || len(sub(p, dedup[dedup.length - 1])) > 1e-9) dedup.push(p);
  }
  if (dedup.length > 1 && len(sub(dedup[0], dedup[dedup.length - 1])) < 1e-9) dedup.pop();
  const out2 = [];
  for (let i = 0; i < dedup.length; i++) {
    const a = dedup[(i + dedup.length - 1) % dedup.length];
    const b = dedup[i];
    const c = dedup[(i + 1) % dedup.length];
    if (len(cross(sub(b, a), sub(c, b))) < 1e-9) continue;
    out2.push(b);
  }
  return out2.length >= 3 ? out2 : [];
}

/* =========================================================
   4. 结构：格点、真实原子半径、面间距与二维网胞
   ========================================================= */
export const STRUCTURES = {
  sc: { key: 'sc', name: '简单立方', short: 'SC', basis: [[0, 0, 0]], points: 1, radius: 0.5 },
  bcc: { key: 'bcc', name: '体心立方', short: 'BCC', basis: [[0, 0, 0], [0.5, 0.5, 0.5]], points: 2, radius: Math.sqrt(3) / 4 },
  fcc: { key: 'fcc', name: '面心立方', short: 'FCC', basis: [[0, 0, 0], [0.5, 0.5, 0], [0.5, 0, 0.5], [0, 0.5, 0.5]], points: 4, radius: Math.sqrt(2) / 4 },
};
export const structOf = (key) => STRUCTURES[key] || STRUCTURES.bcc;
/** 原胞体积 = 惯用晶胞体积 / 每胞格点数（这里惯用胞 = 单位立方体） */
export const vPrim = (key) => 1 / structOf(key).points;

/** 原点附近的格点：整数平移 + 基内偏移，取 |r| ≤ lim */
export function latticePoints(key, lim = 1.6) {
  const s = structOf(key);
  const n = Math.ceil(lim + 0.5);
  const out = [];
  const seen = new Set();
  for (let x = -n; x <= n; x++) {
    for (let y = -n; y <= n; y++) {
      for (let z = -n; z <= n; z++) {
        for (const b of s.basis) {
          const p = [x + b[0], y + b[1], z + b[2]];
          if (len(p) > lim + 1e-9) continue;
          const k = keyOf(p);
          if (seen.has(k)) continue;
          seen.add(k);
          out.push(p);
        }
      }
    }
  }
  return out;
}

/**
 * 相邻「真的含格点的平面」的间距。
 * 复式格子会在教科书公式算出的面之间多插进一层原子面（BCC 的 {100} 就是这样），
 * 所以这里按格点把 h x + k y + l z 的所有取值求最大公度量，而不是直接用 1/|n|。
 */
export function atomicSpacing(key, hkl) {
  const ints = latticePoints(key, 3.4).map((p) => Math.abs(Math.round(dot(hkl, p) * 2)));
  let g = 0;
  for (const v of ints) g = gcd(g, v);
  if (g === 0) g = 2;
  return (g / 2) / len(hkl);
}

/** 落在某张面片里的格点：先在平面上，再在多边形内（顶点/棱上的按「在面内」算） */
export function pointsInPatch(key, hkl, t, poly) {
  if (!poly || poly.length < 3) return [];
  const basis = planeBasis(hkl);
  const q = poly.map((p) => project(p, basis));
  // 略微放大一点再判内外：面片的顶点本身就是格点，射线法在边界上不可靠
  const cu = q.reduce((a, b) => a + b.u, 0) / q.length;
  const cv = q.reduce((a, b) => a + b.v, 0) / q.length;
  const s = q.map((p) => ({ u: cu + (p.u - cu) * 1.000001, v: cv + (p.v - cv) * 1.000001 }));
  const inside2 = (u, v) => {
    let flip = false;
    for (let i = 0; i < s.length; i++) {
      const a = s[i]; const b = s[(i + 1) % s.length];
      if ((a.v > v) !== (b.v > v)) {
        const x = a.u + ((v - a.v) / (b.v - a.v)) * (b.u - a.u);
        if (u < x) flip = !flip;
      }
    }
    return flip;
  };
  const out = [];
  for (const p of latticePoints(key, 2.4)) {
    if (Math.abs(dot(hkl, p) - t) > 1e-9) continue;
    const pp = project(p, basis);
    if (inside2(pp.u, pp.v)) out.push(p);
  }
  return out;
}

/**
 * 面内的二维原胞：在平面方向上暴力搜最短的两个独立格矢。
 * 校验脚本用「面积 × 相邻面间距 = 原胞体积」这条恒等式对答案，两条独立路子必须给同一个数。
 */
export function planeMesh(key, hkl) {
  const n = norm3(hkl);
  const cand = latticePoints(key, 2.1)
    .filter((v) => len(v) > 1e-9 && Math.abs(dot(n, v)) < 1e-9)   // 零矢量不算格矢
    .sort((a, b) => len(a) - len(b));
  if (!cand.length) return null;
  const u = cand[0];
  let best = null;
  for (const v of cand) {
    const area = len(cross(u, v));
    if (area < 1e-9) continue;
    if (!best || area < best.area - 1e-9) best = { u, v, area };
  }
  if (!best) return null;
  return { ...best, density: 1 / best.area };
}

/* =========================================================
   5. 晶带：一族面里共用同一个晶向的那些面（zone law: hu + kv + lw = 0）
   ========================================================= */
export function zonesOf(fam, minCount = 3) {
  const map = new Map();
  for (let a = 0; a < fam.members.length; a++) {
    for (let b = a + 1; b < fam.members.length; b++) {
      const d = cross(fam.members[a].hkl, fam.members[b].hkl).map(Math.round);
      if (len(d) < 1e-9) continue;
      const g = d.reduce((acc, x) => gcd(acc, Math.abs(x)), 0) || 1;   // 约化成互质指标：[002] → [001]
      const c = canonical(d.map((x) => x / g));
      const k = c.join(',');
      if (!map.has(k)) map.set(k, c);
    }
  }
  const out = [];
  for (const dir of map.values()) {
    const list = fam.members.filter((m) => Math.abs(dot(dir, m.hkl)) < 1e-9);
    if (list.length >= minCount) out.push({ dir, label: dirLabel(dir), members: list, groups: new Set(list.map((m) => m.group)).size });
  }
  out.sort((a, b) => b.members.length - a.members.length || cmpKey(a.dir, b.dir));
  return out;
}

/* =========================================================
   6. 给面板用的一眼结论
   ========================================================= */
/** 族里每个面的邻居分布：几个 60°、几个 90°……{110} 是 4 个 60° + 2 个 90° + 1 个 180° */
export function angleHistogram(fam) {
  const hist = new Map();
  const m = fam.members[0];
  for (const other of fam.members) {
    if (other === m) continue;
    const a = angleBetween(m, other);
    const k = Math.round(a);
    hist.set(k, (hist.get(k) || 0) + 1);
  }
  return [...hist.entries()].sort((a, b) => a[0] - b[0]);
}

/** 与面 m 相邻（共用一条棱）的面：夹角最小的那些就是晶形上的邻面 */
export function neighboursOf(form, faceIndex) {
  const f = form.faces[faceIndex];
  return form.faces.filter((g, j) => j !== faceIndex && g.edges.some((e) => f.edges.includes(e)));
}

export const dotV = dot;
export const crossV = cross;
export const TOLV = TOL;
