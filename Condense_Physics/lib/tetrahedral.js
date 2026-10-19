// 正四面体的 24 个对称操作 Td（国际记号 4̄3m）：按对称元素逐个构造，再给每个操作补上
// 顶点置换、奇偶、det 与动画分解。金刚石 / 闪锌矿的四面体配位用的就是这一组。
import { ID3, INV, mul, applyM, det, mkey, rotAxis, mirrorPlane, dirName, over, parts } from './matrix.js';

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
