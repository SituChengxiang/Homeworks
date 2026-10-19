// 反轴 n̄（绕轴转 360/n 再接反演）：算出它生成的群、群里的元素、以及「它等价于什么」。
// 6̄ = 3/m 这条结论不是文案，是从闭包出来的集合里数出来的。
import { ID3, INV, mul, applyM, det, same, rotZ, mirrorPlane, powers, orderOf, closure, over } from './matrix.js';

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


const genCache = new Map();
/** n̄ 的生成元（带缓存，页面每一步都在问它） */
export function genOf(n) {
  if (!genCache.has(n)) genCache.set(n, rotoInversion(n));
  return genCache.get(n);
}

/** n̄ 连做 k 次；k = 群的阶时正好回到单位阵，这就是「群的阶」的可操作定义 */
export function powGen(n, k) {
  let M = ID3;
  for (let i = 0; i < k; i++) M = mul(genOf(n), M);
  return M;
}
