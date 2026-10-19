// 性能基准：一次交互（拖动滑杆 / 切步骤）在 JS 侧要算多少东西。
// 用法：./test/bench.sh
import {
  A, v, computeWS, latticePoints, nearestDistance, wsStats, atomsFor, uniqueEdges,
  fccAtomSet, shiftedBasis, pairsWithin,
  hcpAtoms, hcpLattice, hcpPackingFraction, nearestNeighborPairs,
} from '../src/geometry.js';

const ms = (fn, n) => {
  const t0 = performance.now();
  for (let i = 0; i < n; i++) fn();
  return (performance.now() - t0) / n;
};

const HALF = A / 2;

/** crystal-cells.html 一次 rebuild 的几何计算部分 */
function crystalRebuild(type) {
  const ws = computeWS(type);
  atomsFor(type);
  latticePoints(type, 3 * A);                                  // 扩展点云
  latticePoints(type, 1.35 * nearestDistance(type));           // 近邻壳层
  wsStats(ws.faces);
  uniqueEdges(ws.faces);
  return ws.faces.length;
}

/** diamond-zincblende.html 一次 applyT 的计算部分（点云 + 键） */
function diamondRebuild(t) {
  const pts = fccAtomSet(2 * A);
  const b = shiftedBasis(t, A).map((p) => p.sub(v(HALF, HALF, HALF)));
  pairsWithin(pts.slice(0, 14), b, 0.55 * A);
  return pts.length;
}

/** hcp-close-packed.html 换一个 c/a 的代价（点阵枚举 + 键搜索，之后按值缓存） */
function hcpRebuild(ca) {
  const lat = hcpLattice(A, ca, 2.6 * A);
  const atoms = hcpAtoms(A, ca);
  nearestNeighborPairs(atoms.map((x) => x.pos), A);
  hcpPackingFraction(A, ca);
  return lat.length + atoms.length;
}

console.log('— 冷启动：每种结构第一次算 —');
for (const type of ['SC', 'BCC', 'FCC']) {
  const t0 = performance.now();
  crystalRebuild(type);
  console.log(`  ${type} ${(performance.now() - t0).toFixed(2)} ms`);
}

console.log('\n— 缓存命中后：每次交互的成本 —');
let warmTotal = 0;
for (const type of ['SC', 'BCC', 'FCC']) {
  const warm = ms(() => crystalRebuild(type), 500);
  warmTotal += warm;
  console.log(`  ${type} ${warm.toFixed(4)} ms`);
}
const warmDiamond = ms(() => diamondRebuild(0.25), 500);
console.log(`  金刚石页 applyT ${warmDiamond.toFixed(4)} ms`);

const hcpCold = ms(() => hcpRebuild(1.6 + Math.random() * 0.3), 40);
console.log(`  HCP 页换一个 c/a ${hcpCold.toFixed(3)} ms（含点阵枚举，之后同值不再计算）`);
console.log(`  HCP 滑杆全程（100 个不同 c/a）≈ ${(hcpCold * 100).toFixed(0)} ms`);

console.log(`\n拖完一次滑杆 ≈ 45 帧：`);
console.log(`  crystal-cells  ${(warmTotal / 3 * 45).toFixed(3)} ms 纯计算`);
console.log(`  diamond        ${(warmDiamond * 45).toFixed(3)} ms 纯计算`);
