import { A, computeWS, wsStats, atomsFor, atomsPerCell, touchRadius, nearestDistance, faceNormal } from '../src/geometry.js';

const exp = {
  SC:  { F: 6,  V: 8,  E: 12, n: 1, CN: 6,  pack: Math.PI / 6 },
  BCC: { F: 14, V: 24, E: 36, n: 2, CN: 8,  pack: Math.sqrt(3) * Math.PI / 8 },
  FCC: { F: 12, V: 14, E: 24, n: 4, CN: 12, pack: Math.sqrt(2) * Math.PI / 6 },
};

let fail = 0;
const check = (label, got, want, tol = 1e-6) => {
  const ok = typeof want === 'number' ? Math.abs(got - want) < tol : got === want;
  if (!ok) fail++;
  console.log(`${ok ? '  ok ' : 'FAIL '} ${label.padEnd(42)} got=${typeof got === 'number' ? got.toFixed(5) : got}  want=${typeof want === 'number' ? want.toFixed(5) : want}`);
};

// 多面体体积（散度定理）
function volume(faces) {
  let vol = 0;
  for (const f of faces) {
    const a = f[0];
    for (let i = 1; i < f.length - 1; i++) {
      const b = f[i], c = f[i + 1];
      vol += a.dot(b.clone().cross(c)) / 6;
    }
  }
  return Math.abs(vol);
}

for (const type of ['SC', 'BCC', 'FCC']) {
  console.log(`\n=== ${type} ===`);
  const { faces, neighbors, dmin } = computeWS(type);
  const s = wsStats(faces);
  const e = exp[type];

  check('WS 面数 F', s.F, e.F);
  check('WS 顶点数 V', s.V, e.V);
  check('WS 棱数 E', s.E, e.E);
  check('欧拉公式 V-E+F', s.V - s.E + s.F, 2);
  check('最近邻距离 dmin/a', dmin / A, type === 'SC' ? 1 : type === 'BCC' ? Math.sqrt(3) / 2 : Math.sqrt(2) / 2);
  check('每晶胞原子数', atomsPerCell(type), e.n);

  const r = touchRadius(type);
  const pack = atomsPerCell(type) * (4 / 3) * Math.PI * r ** 3 / A ** 3;
  check('堆积率', pack, e.pack, 1e-9);

  // WS 体积 = a^3 / n
  check('WS 体积 / (a^3/n)', volume(faces), A ** 3 / e.n, 1e-6);

  // 每个面都是平面且为凸多边形
  let planar = true, convex = true, outward = true;
  for (const f of faces) {
    const n = faceNormal(f);
    const d0 = n.dot(f[0]);
    if (f.some((p) => Math.abs(n.dot(p) - d0) > 1e-6)) planar = false;
    // 凸性：相邻边叉积与外法向同向
    for (let i = 0; i < f.length; i++) {
      const p = f[i], q = f[(i + 1) % f.length], r2 = f[(i + 2) % f.length];
      const cr = q.clone().sub(p).cross(r2.clone().sub(q));
      if (cr.dot(n) < -1e-9) convex = false;
    }
    // 外法向：面上点相对原点应在平面外侧（n·x ≈ d ≥ 0）
    if (n.dot(f[0]) <= 0) outward = false;
  }
  check('各面共面', planar, true);
  check('各面凸', convex, true);
  check('各面外法向', outward, true);

  // 所有 WS 顶点都不被任何近邻中垂面切开（n·x ≤ |n|²/2）
  let inside = true, worst = 0;
  const verts = [];
  for (const f of faces) for (const p of f) {
    if (!verts.some((q) => q.distanceToSquared(p) < 1e-9)) verts.push(p);
  }
  for (const p of verts)
    for (const r2 of neighbors) {
      const over = r2.dot(p) - r2.lengthSq() / 2;
      worst = Math.max(worst, over);
      if (over > 1e-6) inside = false;
    }
  check('全部顶点在近邻中垂面内侧', inside, true);
  check('最大越界量', worst, 0, 1e-7);

  // 面形状统计（边数分布）
  const shape = {};
  for (const f of faces) shape[f.length] = (shape[f.length] || 0) + 1;
  console.log(`     面形状: ${Object.entries(shape).map(([k, v]) => `${k}边形×${v}`).join(', ')}`);

  // 近邻键数量
  const atoms = atomsFor(type);
  let bonds = 0;
  for (let i = 0; i < atoms.length; i++)
    for (let j = i + 1; j < atoms.length; j++)
      if (Math.abs(atoms[i].pos.distanceTo(atoms[j].pos) - dmin) < 1e-4) bonds++;
  console.log(`     晶胞内最近邻键数: ${bonds}`);
}

console.log(`\n${fail === 0 ? '全部通过 ✅' : `有 ${fail} 项失败 ❌`}`);
process.exit(fail === 0 ? 0 : 1);
