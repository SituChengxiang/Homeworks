// 对称操作的纯数学：反轴（旋转 + 反演）与正四面体的 24 个对称操作。
//
// 这里刻意不 import three.js：页面渲染、分步文案、test/verify.js 用的都是同一份定义，
// 「6̄ 到底等价于什么」这种话必须由算出来的矩阵说，不能由文案说。
// 矩阵一律用 [[a,b,c],[d,e,f],[g,h,i]] 的普通数组，元素都是 0、±1、±1/2、±√3/2 这一类。

export const ID3 = [[1, 0, 0], [0, 1, 0], [0, 0, 1]];
/** 反演 i = −E：左乘它就是把点变成关于原点对称的那个点 */
export const INV = [[-1, 0, 0], [0, -1, 0], [0, 0, -1]];

/**
 * 只给显示和 keying 用的取整：算术中一律保留双精度。
 * （早先 mul() 里也四舍五入到 1e-6，结果 0.866025² + 0.5² ≠ 1，
 *  连乘几次后矩阵已经不是正交阵，幂永远回不到单位阵——群的阶就算错了。）
 */
export const R6 = (x) => Math.round(x * 1e6) / 1e6;
const DEG = 180 / Math.PI;
const rad = (d) => d / DEG;
const clamp = (x, lo, hi) => Math.min(hi, Math.max(lo, x));

/* =========================================================
   3×3 矩阵 basics
   ========================================================= */
export function mul(A, B) {
  const C = [[0, 0, 0], [0, 0, 0], [0, 0, 0]];
  for (let i = 0; i < 3; i++) {
    for (let j = 0; j < 3; j++) {
      C[i][j] = A[i][0] * B[0][j] + A[i][1] * B[1][j] + A[i][2] * B[2][j];
    }
  }
  return C;
}

export const applyM = (A, p) => A.map((row) => row[0] * p[0] + row[1] * p[1] + row[2] * p[2]);

export const det = (A) => (
  A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1])
  - A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0])
  + A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0])
);

export const tr = (A) => A[0][0] + A[1][1] + A[2][2];

export const transpose = (A) => A.map((row, i) => row.map((_, j) => A[j][i]));

export function same(A, B, tol = 1e-9) {
  for (let i = 0; i < 3; i++) for (let j = 0; j < 3; j++) {
    if (Math.abs(A[i][j] - B[i][j]) > tol) return false;
  }
  return true;
}

/** 矩阵的唯一字符串键（去重、判封闭都靠它）；6 位小数足够区分 1/2 与 √3/2 */
export const mkey = (M) => M.map((r) => r.map((x) => R6(x).toFixed(6)).join(',')).join(';');

export const diag = (a, b, c) => [[a, 0, 0], [0, b, 0], [0, 0, c]];
export const scaling = (s) => diag(s, s, s);

/* =========================================================
   造基本操作：绕任意轴转 θ（Rodrigues）、对过原点的平面反映
   ========================================================= */
export function norm3(a) {
  const n = Math.hypot(a[0], a[1], a[2]);
  if (n < 1e-12) throw new Error('零向量不能当轴');
  return [a[0] / n, a[1] / n, a[2] / n];
}

/** 叉乘矩阵 [a]×：[a]×·v = a × v */
function crossMat(a) {
  return [[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]];
}

/** Rodrigues 公式：绕单位轴 a 转 deg 度（a 不必预先归一化） */
export function rotAxis(axis, deg) {
  const a = norm3(axis);
  const t = rad(deg);
  const K = crossMat(a);
  const K2 = mul(K, K);
  const out = [[0, 0, 0], [0, 0, 0], [0, 0, 0]];
  for (let i = 0; i < 3; i++) {
    for (let j = 0; j < 3; j++) {
      out[i][j] = (i === j ? 1 : 0) + Math.sin(t) * K[i][j] + (1 - Math.cos(t)) * K2[i][j];
    }
  }
  return out;
}

export const rotZ = (deg) => rotAxis([0, 0, 1], deg);

/** 对「过原点、法线为 n」的平面反映：H = E − 2 n̂n̂ᵀ，det = −1 */
export function mirrorPlane(normal) {
  const n = norm3(normal);
  const out = [[0, 0, 0], [0, 0, 0], [0, 0, 0]];
  for (let i = 0; i < 3; i++) {
    for (let j = 0; j < 3; j++) {
      out[i][j] = (i === j ? 1 : 0) - 2 * n[i] * n[j];
    }
  }
  return out;
}

/* =========================================================
   幂、循环群、群性质
   ========================================================= */
/** M 的 1..order 次幂（最后一项一定是单位阵） */
export function powers(M, max = 24) {
  let cur = ID3;
  const out = [];
  for (let k = 1; k <= max; k++) {
    cur = mul(M, cur);
    out.push({ k, M: cur });
    if (same(cur, ID3)) return out;
  }
  throw new Error('矩阵的幂没在有限步内回到单位阵（不是正交整数阶矩阵？）');
}

export const orderOf = (M) => powers(M).length;

/** 由若干生成元闭包出有限群（BFS，靠 mkey 去重） */
export function closure(gens) {
  const seen = new Map();
  const queue = [ID3];
  seen.set(mkey(ID3), ID3);
  while (queue.length) {
    const a = queue.shift();
    for (const g of gens) {
      const x = mul(g, a);
      const k = mkey(x);
      if (!seen.has(k)) { seen.set(k, x); queue.push(x); }
    }
  }
  return [...seen.values()];
}

/** 把矩阵翻译成看得懂的一句话 */
export function describeOp(M) {
  if (same(M, ID3)) return 'E：什么都不动';
  if (same(M, INV)) return 'i：关于原点的反演';
  const p = parts(M);
  if (p.det > 0) return `C${p.order}：绕 ${dirName(p.axis)} 转 ${fmtAngle(p.angleDeg)}`;
  if (Math.abs(p.angleDeg) < 1e-6) {
    return `m⊥：对垂直于 ${dirName(p.axis)} 的平面反映`;
  }
  return `转 ${fmtAngle(p.angleDeg)}（轴 ${dirName(p.axis)}）+ 对垂直平面的反映 = S${p.order}`;
}

const fmtAngle = (a) => `${R6(a)}°`;

/** [x y z] 方向的 crystallographic 写法：负指标加上横线 */
export function dirName(axis) {
  const a = norm3(axis).map((x) => Math.round(x * 100) / 100);
  // 归到最小整数
  const ints = a.map((x) => Math.round(x));
  const s = ints.map((x) => (x < 0 ? `${over(Math.abs(x))}` : String(x))).join('');
  return `[${s}]`;
}

/** 负指标用组合上划线：1 → 1̄（面板里另可用 .ov 类） */
export const over = (x) => `${x}\u0304`;

/**
 * 分解正交矩阵：
 *  · det = +1 → 绕某轴转 θ 的真旋转；
 *  · det = −1 → 唯一写成「对平面 σ 反映 + 绕同一法轴转 θ」的形式（σ 的法线就是 −1 本征向量），
 *    这正是黄昆书里「转动反映轴」的样子，也是反轴 n̄ = 转 + 反演 的同一族操作。
 * 返回的 axis/angleDeg 满足 mirrorPlane(axis) · rotAxis(axis, angleDeg) === M。
 */
export function parts(M) {
  const d = det(M);
  // (M − Mᵀ)/2 = sinθ · [â]×，两种情形都能用它定 sinθ
  const skew = [M[2][1] - M[1][2], M[0][2] - M[2][0], M[1][0] - M[0][1]];

  if (d > 0) {
    const c = clamp((tr(M) - 1) / 2, -1, 1);
    const s = Math.hypot(skew[0], skew[1], skew[2]) / 2;
    const angle = Math.atan2(s, c) * DEG;                  // 0..180
    if (angle < 1e-6) return { det: 1, kind: 'identity', axis: [0, 0, 1], angleDeg: 0, order: 1 };
    // θ = 180° 时 sinθ = 0，改从 M + E = 2ââᵀ 的列里拿轴
    const axis = s < 1e-9
      ? longestColumn(addIdentity(M))
      : norm3(skew);
    return { det: 1, kind: 'rotation', axis, angleDeg: R6(angle), order: orderOf(M) };
  }

  if (same(M, INV)) {                    // 只有反演本身：三个方向都是 −1 本征值，轴随便取
    return { det: -1, kind: 'inversion', axis: [0, 0, 1], angleDeg: 180, order: 2 };
  }
  const c = clamp((tr(M) + 1) / 2, -1, 1);                // tr = 2cosθ − 1
  const axis = minusOneEigenvector(M);                     // σ 的法线 / 转动反映轴
  const s = (skew[0] * axis[0] + skew[1] * axis[1] + skew[2] * axis[2]) / 2;
  const angle = Math.atan2(s, c) * DEG;                    // 相对这根轴可正可负
  return {
    det: -1,
    kind: Math.abs(angle) < 1e-6 ? 'mirror' : 'roto-reflection',
    axis, angleDeg: R6(angle), order: orderOf(M),
  };
}

const addIdentity = (M) => [
  [M[0][0] + 1, M[0][1], M[0][2]],
  [M[1][0], M[1][1] + 1, M[1][2]],
  [M[2][0], M[2][1], M[2][2] + 1],
];

function longestColumn(A) {
  let best = 0;
  let len = -1;
  for (let j = 0; j < 3; j++) {
    const l = Math.hypot(A[0][j], A[1][j], A[2][j]);
    if (l > len) { len = l; best = j; }
  }
  return norm3([A[0][best], A[1][best], A[2][best]]);
}

/** −1 本征方向：对纯镜面就是「镜面的法线」。用 (E−M) 的两列叉乘不稳定，直接解线性方程。 */
function minusOneEigenvector(M) {
  // 解 (M + E)v = 0：三行里挑两行线性无关的做叉乘
  const A = [[M[0][0] + 1, M[0][1], M[0][2]], [M[1][0], M[1][1] + 1, M[1][2]], [M[2][0], M[2][1], M[2][2] + 1]];
  const cand = [
    cross(A[0], A[1]), cross(A[0], A[2]), cross(A[1], A[2]),
    // 行太像时退回列叉乘
    cross([A[0][0], A[1][0], A[2][0]], [A[0][1], A[1][1], A[2][1]]),
    cross([A[0][0], A[1][0], A[2][0]], [A[0][2], A[1][2], A[2][2]]),
    cross([A[0][1], A[1][1], A[2][1]], [A[0][2], A[1][2], A[2][2]]),
  ];
  for (const c of cand) {
    const l = Math.hypot(c[0], c[1], c[2]);
    if (l > 1e-6) {
      const v = [c[0] / l, c[1] / l, c[2] / l];
      // 校验：M v 必须等于 −v
      if (Math.hypot(...applyM(M, v).map((x, i) => x + v[i])) < 1e-6) return v;
    }
  }
  throw new Error('找不到 −1 本征方向');
}

const cross = (a, b) => [
  a[1] * b[2] - a[2] * b[1],
  a[2] * b[0] - a[0] * b[2],
  a[0] * b[1] - a[1] * b[0],
];

/* =========================================================
   动画路径：把「一步操作」拆成看得见的连续过程
   ========================================================= */
/**
 * 从当前姿态 P 出发施加 M 的动画矩阵，t = 0 → P，t = 1 → M·P。
 *  · 真旋转：按轴角插值；
 *  · 非真操作（det = −1）：先转 tθ，再把沿轴的分量从 +1 连续压到 −1
 *    ——就是「被压扁到镜面上、再从另一面出来」这个连续过程，t=1 时 E − 2ââᵀ 正是那个镜面。
 */
export function animPath(M, t, P = ID3) {
  const k = clamp(t, 0, 1);
  if (same(M, ID3)) return P;
  const p = parts(M);
  if (p.det > 0) return mul(rotAxis(p.axis, p.angleDeg * k), P);

  const a = p.axis;
  const S = [[0, 0, 0], [0, 0, 0], [0, 0, 0]];
  for (let i = 0; i < 3; i++) {
    for (let j = 0; j < 3; j++) S[i][j] = (i === j ? 1 : 0) - 2 * k * a[i] * a[j];
  }
  return mul(mul(S, rotAxis(p.axis, p.angleDeg * k)), P);
}

/**
 * 反轴 n̄ 的动画：定义是「绕轴转 360/n 再接反演」，那就照着定义演——
 * 转 (360/n)·t，同时整体按 (1 − 2t) 缩放，t = 1 时正好等于 i·Rz(360/n)。
 * P 是已经施加过的姿态（P = M^(k−1)），用来一步一步把等效点走出来。
 */
export function invertStep(n, t, P = ID3) {
  const k = clamp(t, 0, 1);
  return mul(mul(scaling(1 - 2 * k), rotZ((360 / n) * k)), P);
}

/* =========================================================
   反轴 bar n：绕 z 转 360/n 再接反演
   ========================================================= */
export const rotoInversion = (n) => mul(INV, rotZ(360 / n));

/**
 * 算出 n̄ 生成的群、它的性质、以及「它其实等价于什么」。
 * 结论全部从矩阵推出来：
 *   1̄ → {E, i}；2̄ → 阶 2 且含水平镜面 = m；3̄ → 阶 6、含 i、真轴 3 次；
 *   4̄ → 阶 4、既无 i 也无镜面、真轴只剩 2 次；6̄ → 阶 6、含水平镜面但不含 i、真轴只有 3 次 → 3/m
 */
export function barGroup(n) {
  const gen = rotoInversion(n);
  const elems = closure([gen]);
  const proper = elems.filter((M) => det(M) > 0);
  const rotOrders = proper.filter((M) => !same(M, ID3)).map((M) => orderOf(M));

  const props = {
    order: elems.length,
    properCount: proper.length,
    hasI: elems.some((M) => same(M, INV)),
    hasMirrorPerp: elems.some((M) => same(M, mirrorPlane([0, 0, 1]))),
    maxProper: rotOrders.length ? Math.max(...rotOrders) : 1,
  };
  props.equiv = equivalentLabel(n, props);
  return { n, gen, elems, props, cycle: powers(gen) };
}

function equivalentLabel(n, p) {
  const bar = over(String(n));
  if (p.order === 2 && p.hasI) return `${bar} = i（就是对称中心）`;
  if (p.order === 2 && p.hasMirrorPerp) return `${bar} = m（对垂直于轴的镜面，一点不新）`;
  if (p.order === 6 && p.hasI) return `${bar} = ${p.maxProper} + i（真 3 次轴 ⊗ 对称中心，即 S6）`;
  if (p.order === 6 && p.hasMirrorPerp && !p.hasI) {
    return `${bar} = ${p.maxProper}/m（真 3 次轴 + 垂直镜面，即 C3h）——里面根本没有 6 次操作`;
  }
  if (p.order === 4) return `${bar} 独立：等价 S4，既无 i 也无镜面，真旋转只剩 2 次`;
  return `${bar}：阶 ${p.order}`;
}

/** 反轴晶系学上允许的 n：晶体周期只允许 1,2,3,4,6 */
export const ALLOWED_BAR = [1, 2, 3, 4, 6];

/**
 * 一个一般位置的点 P 在 n̄ 作用下的轨道（等效点系），
 * 并指出这些点两两之间的关系：3̄ 给出 3 对「关于原点对称」，6̄ 给出 3 对「关于水平镜面对称」。
 */
export function barOrbit(n, P) {
  const g = barGroup(n);
  const pts = g.cycle.map(({ k, M }) => ({ k, M, p: applyM(M, P), det: det(M) }));
  const pairs = [];
  const used = new Set();
  const link = (test, tag) => {
    for (let i = 0; i < pts.length; i++) {
      if (used.has(i)) continue;
      for (let j = i + 1; j < pts.length; j++) {
        if (used.has(j)) continue;
        if (test(pts[i].p, pts[j].p)) { pairs.push([i, j, tag]); used.add(i); used.add(j); break; }
      }
    }
  };
  if (g.props.hasI) link((a, b) => Math.hypot(...a.map((x, i) => x + b[i])) < 1e-6, 'i');
  else if (g.props.hasMirrorPerp) {
    link((a, b) => Math.abs(a[0] - b[0]) < 1e-6 && Math.abs(a[1] - b[1]) < 1e-6 && Math.abs(a[2] + b[2]) < 1e-6, 'm');
  }
  return { ...g, pts, pairs, handed: pts.map((x) => x.det) };
}

/* =========================================================
   正四面体的对称操作 Td（4̄3m）：24 个
   ========================================================= */
/**
 * 把四面体摆进立方体：4 个顶点取立方体 8 个角里互不相邻的那 4 个（坐标乘积 = +1）。
 * 这样三条立方体棱向 ⟨100⟩ 就是 4̄ 轴，4 条体对角线 ⟨111⟩ 就是 3 次轴，
 * 6 个 {110} 对角面就是镜面——国际记号 4̄3m 一眼就能指出来。
 * 金刚石 / 闪锌矿里每个原子的 4 个最近邻正是这样一套隔角顶点。
 */
export const TETRA_VERTS = [[1, 1, 1], [1, -1, -1], [-1, 1, -1], [-1, -1, 1]];
export const TETRA_FACES = [[1, 2, 3], [0, 2, 3], [0, 1, 3], [0, 1, 2]];  // 第 i 个面 = 去掉第 i 个顶点

const AXES_100 = [[1, 0, 0], [0, 1, 0], [0, 0, 1]];
/** 4 条体对角线：和 4 个顶点方向重合（顶点在 [111]，对面面心在 [1̄1̄1̄]） */
const AXES_111 = TETRA_VERTS;
/** 6 个 σd 镜面的法线：{110} 型 */
export const MIRROR_110 = [
  [1, 1, 0], [1, -1, 0], [1, 0, 1], [1, 0, -1], [0, 1, 1], [0, 1, -1],
];

/** 把 48 个带符号置换矩阵（Oh 群）逐个列出来 */
export function ohGroup() {
  const perms = [[0, 1, 2], [0, 2, 1], [1, 0, 2], [1, 2, 0], [2, 0, 1], [2, 1, 0]];
  const out = [];
  for (const p of perms) {
    for (let s = 0; s < 8; s++) {
      const signs = [(s & 1) ? -1 : 1, (s & 2) ? -1 : 1, (s & 4) ? -1 : 1];
      const M = [[0, 0, 0], [0, 0, 0], [0, 0, 0]];
      for (let i = 0; i < 3; i++) for (let j = 0; j < 3; j++) M[i][j] = 0;
      for (let i = 0; i < 3; i++) M[i][p[i]] = signs[i];
      out.push(M);
    }
  }
  return out;
}

const keyOfPoint = (p) => p.map((x) => Math.round(x)).join(',');

/** Oh 的 48 个里，能把这套顶点映回自己的正好 24 个 = Td；另外 24 个把它换成互补的第二套四面体 */
export function tdGroupFromOh() {
  const set = new Set(TETRA_VERTS.map(keyOfPoint));
  return ohGroup().filter((M) => TETRA_VERTS.every((v) => set.has(keyOfPoint(applyM(M, v)))));
}

/** 4 个顶点的一个置换（用来证明 Td ≅ S4） */
export function vertexPerm(M) {
  const idx = new Map(TETRA_VERTS.map((v, i) => [keyOfPoint(v), i]));
  return TETRA_VERTS.map((v) => idx.get(keyOfPoint(applyM(M, v))));
}

/** 置换的奇偶：偶置换 = 真旋转（A4），奇置换 = 带反映/反演 */
export function permParity(perm) {
  let inv = 0;
  for (let i = 0; i < perm.length; i++) {
    for (let j = i + 1; j < perm.length; j++) if (perm[i] > perm[j]) inv++;
  }
  return inv % 2 ? -1 : 1;
}

const permText = (perm) => perm.map((x) => x + 1).join('');

/**
 * 按「对称元素」逐个列出 24 个操作，标签是国际记号而不是猜出来的：
 *   E ×1
 *   4 条 3 次轴 × {转 120°, 转 240°} → 8C3
 *   3 条 ⟨100⟩ 上的 4̄：4̄² = C2 → 3C2
 *   3 条 4̄ 轴上的 4̄¹ 与 4̄³（就是 S4 的两个方向）→ 6S4
 *   6 个 σd 镜面 → 6σd
 * 合计 1 + 8 + 3 + 6 + 6 = 24。
 */
let tdOpsCache = null;

export function tdOps() {
  if (tdOpsCache) return tdOpsCache;
  const out = [];
  out.push(op({ label: 'E', cls: 'E', clsCount: 1, M: ID3, axis: null, note: '什么都不动' }));

  for (const a of AXES_111) {
    for (const deg of [120, 240]) {
      out.push(op({
        label: deg === 120 ? 'C3' : 'C3²',
        alt: `绕 ${dirName(a)} 转 ${deg}°`,
        cls: 'C3', clsCount: 8, M: rotAxis(a, deg), axis: a, deg,
        note: '真旋转：沿体对角线',
      }));
    }
  }
  for (const a of AXES_100) {
    out.push(op({
      label: 'C2', alt: `= ${over('4')}²`, cls: 'C2', clsCount: 3,
      M: rotAxis(a, 180), axis: a, deg: 180,
      note: '三条 ⟨100⟩ 上的 2 次轴 = 4̄ 的平方',
    }));
  }
  for (const a of AXES_100) {
    // 4̄ = i·R(90) = R(270)·σ⊥，4̄³ = i·R(270) = R(90)·σ⊥ —— 和 S4 是同一条轴上的同一批操作
    for (const [deg, tag] of [[270, over('4')], [90, `${over('4')}³`]]) {
      out.push(op({
        label: tag, alt: `S4${deg === 90 ? '' : '³'}`,
        cls: 'S4', clsCount: 6,
        M: mul(INV, rotAxis(a, deg === 270 ? 90 : 270)), axis: a, deg,
        note: '绕 ⟨100⟩ 转 90° 再反演：非真操作',
      }));
    }
  }
  for (const n of MIRROR_110) {
    out.push(op({
      label: 'σd', cls: 'σd', clsCount: 6, M: mirrorPlane(n), axis: n, mirrorNormal: n, deg: 0,
      note: '{110} 型镜面：含着四面体的一条棱，把对棱那两个顶点互换',
    }));
  }
  tdOpsCache = out;
  return out;
}

/** 给每个操作补上 det、顶点置换、奇偶、矩阵键，以及动画用的分解（axis + 转角） */
function op(base) {
  const perm = vertexPerm(base.M);
  const info = parts(base.M);
  return {
    ...base,
    det: det(base.M),
    perm,
    permText: permText(perm),
    parity: permParity(perm),
    key: mkey(base.M),
    animAxis: info.axis,
    animDeg: info.angleDeg,
    improper: info.det < 0,
  };
}

/** 24 个操作按类别分组，供左边的按钮网格用 */
export function tdClasses() {
  const ops = tdOps();                     // 同一批对象：页面用 indexOf 反查下标
  const order = ['E', 'C3', 'C2', 'S4', 'σd'];
  const names = {
    E: 'E · 恒等', C3: '8C3 · 沿体对角线', C2: '3C2 · 4̄ 的平方',
    S4: '6S4 = 4̄,4̄³', 'σd': '6σd · {110} 镜面',
  };
  return order.map((cls) => ({
    cls,
    name: names[cls],
    items: ops.filter((o) => o.cls === cls),
  }));
}

/** 四面体的 4 个面也各是 4 个顶点去掉 1 个；看面怎么被置换，比看顶点更能说明 24 = |S4| */
export function facePerm(M) {
  const map = new Map(TETRA_VERTS.map((v, i) => [keyOfPoint(v), i]));
  return TETRA_FACES.map((f) => {
    const img = f.map((vi) => map.get(keyOfPoint(applyM(M, TETRA_VERTS[vi]))));
    return [0, 1, 2, 3].find((x) => !img.includes(x));
  });
}

/** 顶点 1 在哪个顶点方向上（用来把操作和「4 个顶点」对上号） */
export const vertexDirName = (i) => dirName(TETRA_VERTS[i]);
