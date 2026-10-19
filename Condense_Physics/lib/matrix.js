// 3×3 正交矩阵与对称操作的通用内核：旋转、反映、反演、幂与闭包、轴角分解、动画路径。
// 与 three.js 无关，页面、Canvas2D 符号层、node 校验用的是同一份定义。
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


/**
 * 给定一个法线方向，返回垂直于它的两个单位向量（沿轴投影时的屏幕 u/v 轴）。
 * 选轴刻意避开「与法线平行的坐标轴」，保证同一个方向每次投影出来的图朝向一致。
 */
export function planeBasis(normal) {
  const n = norm3(normal);
  const helper = Math.abs(n[0]) < 0.9 ? [1, 0, 0] : [0, 1, 0];
  const u = norm3(cross(helper, n));
  const v = norm3(cross(n, u));
  return { u, v, n };
}

/** 三维点在某组基下的二维投影坐标 (u, v) 与沿法线的高度 z */
export function project(p, basis) {
  const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  return { u: dot(p, basis.u), v: dot(p, basis.v), z: dot(p, basis.n) };
}
