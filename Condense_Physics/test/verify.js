// 自动校验：几何/物理量 + 演示脚本 + 页面与脚本的 id 对应关系
import fs from 'node:fs';
import * as THREE from 'three';
import { makeAppear, sharePlanes } from '../src/view.js';
import {
  A, HALF, v,
  computeWS, wsStats, atomsFor, atomsPerCell, touchRadius, nearestDistance, faceNormal,
  fccBasis, shiftedBasis, basisDistance, pairsWithin, sphereVolume, fccAtomSet,
  IDEAL_CA, hcpAtoms, hcpAtomsPerCell, hcpLattice, hcpCoordination, hcpDistances,
  hcpRadius, hcpPackingFraction, hcpTetra, hexPrismVolume, HCP_SHARE, HCP_METALS, hcpSharePlanes,
} from '../src/geometry.js';
import { BASE_LAYERS, LAYER_KEYS, stepsFor, stepDelta } from '../src/steps.js';
import { HCP_LAYERS, hcpSteps, hcpStepDelta } from '../src/hcp-steps.js';

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

  // 页面上的“份额原子”用中垂面裁剪：每个非零坐标方向切掉一半，
  // 所以落在本胞内的份额 = (1/2)^(非零坐标个数)。各原子份额之和必须等于 n。
  const shareOf = (p) => 0.5 ** [p.x, p.y, p.z].filter((c) => Math.abs(c) > 1e-9).length;
  const shareSum = atomsFor(type).reduce((s, a) => s + shareOf(a.pos), 0);
  check('份额裁剪求和 = 每胞原子数', shareSum, e.n, 1e-12);

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

/* ---------- 金刚石 / 闪锌矿：两套 FCC 沿 [111] 错开 ¼ ---------- */
console.log('\n=== 金刚石 / 闪锌矿 ===');
{
  const cellA = atomsFor('FCC').map((x) => x.pos);
  const bOf = (t) => shiftedBasis(t, A).map((p) => p.sub(v(HALF, HALF, HALF)));
  const dBond = (Math.sqrt(3) / 4) * A;

  check('A 亚点阵每胞 4 个', atomsPerCell('FCC'), 4);
  check('B 亚点阵基元 4 个', fccBasis().length, 4);
  check('B 的 4 个原子全在胞内', bOf(0.25).every((p) => [p.x, p.y, p.z].every((c) => Math.abs(c) < HALF - 1e-9)), true);

  const bd = basisDistance(0.25);
  check('¼ 体对角线 = 键长 √3a/4', bd, dBond, 1e-12);

  const { bonds, dmin } = pairsWithin(cellA, bOf(0.25), 0.55 * A);
  check('A–B 最短距离就是键长', dmin, dBond, 1e-9);
  check('每个 B 原子 4 个近邻（四面体）', bonds.length / 4, 4);

  // 键角：取一个 B 原子的 4 条键，验证夹角都是 109.47°
  const center = bOf(0.25)[0];
  const nbrs = cellA.filter((p) => Math.abs(p.distanceTo(center) - dBond) < 1e-9);
  let worstAngle = 0;
  for (let i = 0; i < nbrs.length; i++)
    for (let j = i + 1; j < nbrs.length; j++) {
      const a = nbrs[i].clone().sub(center).normalize();
      const b = nbrs[j].clone().sub(center).normalize();
      worstAngle = Math.max(worstAngle, Math.abs(Math.acos(a.dot(b)) * 180 / Math.PI - 109.4712));
    }
  check('四面体键角 109.47°', worstAngle, 0, 0.01);

  // t = 0 两套重合；t = ½ 时两套 FCC 合成一个简单立方（间距 a/2），配位变成 6
  const at0 = bOf(0);
  check('t=0 时与 A 完全重合', at0.every((p) => cellA.some((q) => q.distanceTo(p) < 1e-9)), true);
  {
    const RA = fccAtomSet(2.2);                                  // 原点附近整套 A 亚点阵
    const halfB = RA.map((p) => p.clone().add(v(1, 1, 1)));      // t=½：平移 (½,½,½)a = (1,1,1)
    const nbAt = (p, list, d) => list.filter((q) => Math.abs(q.distanceTo(p) - d) < 1e-9).length;
    check('t=½ 时 A–B 最短距离 = a/2', pairsWithin(RA, halfB, 2 * A).dmin, A / 2, 1e-12);
    check('t=½ 时每个原子 6 个异种近邻', nbAt(v(0, 0, 0), RA, A / 2), 6);
    check('t=½ 时同种最近邻更远（a/√2）', A / Math.SQRT2 > A / 2, true);
    // 两套 FCC（奇/偶坐标和）互为 (1,1,1) 平移，并集应覆盖简单立方的所有格点
    const triples = (R, pred) => {
      const s = new Set();
      for (let i = -R; i <= R; i++)
        for (let j = -R; j <= R; j++)
          for (let k = -R; k <= R; k++) if (pred(i, j, k)) s.add(`${i},${j},${k}`);
      return s;
    };
    const oddBig = triples(3, (i, j, k) => ((i + j + k) & 1) !== 0);
    const shifted = new Set([...oddBig].map((s) => {
      const [i, j, k] = s.split(',').map(Number);
      return `${i + 1},${j + 1},${k + 1}`;
    }));
    const inner = triples(2, () => true);
    check('t=½ 时两套 FCC 覆盖简单立方全部格点', [...inner].every((p) => oddBig.has(p) || shifted.has(p)), true);
  }

  // 真实数据：共价半径 ≈ 键长一半（经验半径表允许几个百分点偏差）
  const REAL = { C: { a: 3.567, r: 0.77 }, Si: { a: 5.431, r: 1.17 } };
  for (const [k, m] of Object.entries(REAL)) {
    const bond = (Math.sqrt(3) / 4) * m.a;
    check(`${k} 键长 √3a/4 ≈ 2r（3% 内）`, Math.abs(bond - 2 * m.r) / bond, 0, 0.03);
    const pack = (8 * sphereVolume(m.r)) / m.a ** 3;
    check(`${k} 堆积率 ≈ 34%`, pack, 0.34, 0.01);
  }
  const zn = { a: 5.406, rS: 1.70, rZn: 0.74 };
  const packZn = (4 * (sphereVolume(zn.rS) + sphereVolume(zn.rZn))) / zn.a ** 3;
  check('ZnS 堆积率（真实离子半径）', packZn, 0.564, 0.01);

  // 点云枚举规则（坐标和为奇数的 a/2 整数倍）必须覆盖晶胞里那 14 个原子位置
  const set = fccAtomSet(3 * A);
  const dup = set.length !== new Set(set.map((p) => `${p.x},${p.y},${p.z}`)).size;
  check('点云无重复', dup, false);
  check('点云覆盖胞内 14 个位置', cellA.every((p) => set.some((q) => q.distanceTo(p) < 1e-9)), true);
  check('点云全部是格点', set.every((p) => {
    const h = [p.x, p.y, p.z].map((c) => c / (A / 2));
    return h.every((c) => Number.isInteger(Math.round(c)) && Math.abs(c - Math.round(c)) < 1e-9);
  }), true);
}

// 节奏检查：相邻两步之间必须「真的画出新东西」，也不能一次换掉大半张图。
// renderedOf 把 atoms/share 归一成同一批原子（只是切不切），所以 SC 的
// share:'corner' 与 share:'all' 会被视为同一画面 —— 正是曾经出现过的死步骤。
console.log('\n=== 分步节奏（相邻步画面差异） ===');
for (const type of ['SC', 'BCC', 'FCC']) {
  const st = stepsFor(type);
  const deltas = [];
  for (let i = 0; i < st.length - 1; i++) deltas.push(stepDelta(type, st[i], st[i + 1]));
  const dead = deltas.map((d, i) => (d === 0 ? `${i + 1}→${i + 2}` : null)).filter(Boolean);
  const big = deltas.map((d, i) => (d > 4 ? `${i + 1}→${i + 2}(Δ${d})` : null)).filter(Boolean);
  check(`${type} 没有原地不动的一步`, dead.length ? dead.join(',') : true, true);
  check(`${type} 没有一步换掉大半张图（Δ ≤ 4）`, big.length ? big.join(',') : true, true);
  console.log(`     ${type} 共 ${st.length} 步，差异 ${deltas.join(' ')}（最大 ${Math.max(...deltas)}）`);
}
{
  const st = hcpSteps();
  const deltas = [];
  for (let i = 0; i < st.length - 1; i++) deltas.push(hcpStepDelta(st[i], st[i + 1]));
  const dead = deltas.map((d, i) => (d === 0 ? `${i + 1}→${i + 2}` : null)).filter(Boolean);
  const big = deltas.map((d, i) => (d > 4 ? `${i + 1}→${i + 2}(Δ${d})` : null)).filter(Boolean);
  check('HCP 没有原地不动的一步', dead.length ? dead.join(',') : true, true);
  check('HCP 没有一步换掉大半张图（Δ ≤ 4）', big.length ? big.join(',') : true, true);
  console.log(`     HCP 共 ${st.length} 步，差异 ${deltas.join(' ')}（最大 ${Math.max(...deltas)}）`);
}

/* ---------- 分步演示脚本：图层名拼错会静默不渲染，这里挡住 ---------- */
console.log('\n=== 分步演示脚本 ===');
const ALLOWED = new Set(LAYER_KEYS);
for (const type of ['SC', 'BCC', 'FCC']) {
  const steps = stepsFor(type);
  const bad = [];
  for (const s of steps) {
    for (const [k, val] of Object.entries(s.layers)) {
      if (!ALLOWED.has(k)) bad.push(`${s.title} → ${k}`);
      // atoms / share 只能取原子种类名或 'all'（rebuild() 里按 kind 过滤）
      if ((k === 'atoms' || k === 'share') && !['corner', 'face', 'body', 'all'].includes(val))
        bad.push(`${s.title} → ${k}=${val}`);
      if (k === 'labels' && !s.layers.share) bad.push(`${s.title} → labels 但没有 share`);
    }
    if (!Object.values(s.layers).some((x) => x !== false && x !== 0))
      bad.push(`${s.title} → 空步骤`);
    if (!s.title || !s.text) bad.push(`${s.title || '(缺标题)'} → 文案不完整`);
  }
  check(`${type} 步骤图层全部有效（${steps.length} 步）`, bad.length ? bad.join(', ') : true, true);
}

// 每个图层都得被 rebuild() 认出来，否则脚本里写了也不会画
const mainSrc = fs.readFileSync('src/main.js', 'utf8');
const notRendered = Object.keys(BASE_LAYERS).filter((k) => !new RegExp(`L\\.${k}\\b`).test(mainSrc));
check('rebuild() 覆盖所有图层', notRendered.length ? notRendered.join(', ') : true, true);

// HCP 的分步脚本
{
  const hcpSrc = fs.readFileSync('src/hcp.js', 'utf8');
  const unrendered = Object.keys(HCP_LAYERS).filter((k) => !new RegExp(`L\\.${k}\\b`).test(hcpSrc));
  check('HCP rebuild() 覆盖所有图层', unrendered.length ? unrendered.join(', ') : true, true);
  const bad = [];
  for (const s of hcpSteps()) {
    for (const k of Object.keys(s.layers)) if (!(k in HCP_LAYERS)) bad.push(`${s.title} → ${k}`);
    if (!Object.values(s.layers).some(Boolean)) bad.push(`${s.title} → 空步骤`);
    if (!s.title || !s.text) bad.push(`${s.title || '(缺标题)'} → 文案不完整`);
  }
  check(`HCP 步骤图层全部有效（${hcpSteps().length} 步）`, bad.length ? bad.join(', ') : true, true);
}

// 共享模块的函数必须在 import 列表里：esbuild 不报错，页面却会直接跑不起来
{
  const read = (f) => fs.readFileSync(f, 'utf8');
  const exportsOf = (f) => [...read(f).matchAll(/export\s+(?:function|const|class)\s+([A-Za-z_$][\w$]*)/g)].map((m) => m[1]);
  const known = new Set(['src/view.js', 'src/geometry.js', 'src/steps.js', 'src/hcp-steps.js'].flatMap(exportsOf));
  for (const page of ['src/main.js', 'src/diamond.js', 'src/hcp.js']) {
    const src = read(page);
    const imported = new Set([...src.matchAll(/import\s*\{([^}]+)\}/g)]
      .flatMap((m) => m[1].split(',').map((x) => x.trim().split(/\s+as\s+/).pop()).filter(Boolean)));
    const used = new Set([...src.matchAll(/(^|[^.\w$])([A-Za-z_$][\w$]*)\s*\(/g)].map((m) => m[2]));
    const missing = [...used].filter((u) => known.has(u) && !imported.has(u));
    check(`${page} 用到的共享函数都已 import`, missing.length ? missing.join(', ') : true, true);
  }
}

// 页面里的 id 必须和脚本引用对得上（改名会静默失效）
for (const [script, page] of [['src/main.js', 'index.html'], ['src/diamond.js', 'diamond.html'], ['src/hcp.js', 'hcp.html']]) {
  const js = fs.readFileSync(script, 'utf8');
  const pageIds = new Set([...fs.readFileSync(page, 'utf8').matchAll(/id="([^"]+)"/g)].map((m) => m[1]));
  const refIds = new Set([
    ...[...js.matchAll(/(?:getElementById|\bel)\(['"]([^'"]+)['"]\)/g)].map((m) => m[1]),
    // 图层开关是以 { hA: 'layerA', … } 这种对象表驱动的，id 只出现在 key 位置
    ...[...js.matchAll(/\b(t[A-Z][A-Za-z]*|s[A-Z][A-Za-z]*|h[A-Z][A-Za-z]*)\s*:/g)].map((m) => m[1]),
  ]);
  const missing = [...refIds].filter((x) => !pageIds.has(x));
  check(`${page} 提供脚本用到的 ${refIds.size} 个 id`, missing.length ? missing.join(', ') : true, true);

  // 性能约定：几何/材质一律走 view.js 的缓存，页面脚本不许自己 new 这些资源
  const raw = [...js.matchAll(/new THREE\.(Sphere|Box|Circle|Edges)Geometry|new THREE\.(MeshStandard|LineBasic|Sprite)Material/g)].map((m) => m[0]);
  check(`${script} 不直接创建几何/材质`, raw.length ? [...new Set(raw)].join(', ') : true, true);

  // 会被「只改缩放」快速路径改写的图层，缓存键里不能出现半径，
  // 否则拖回旧半径会拿到一个尺寸被改过的缓存对象。
  // 两种写法都要覆盖：cachedObject(`atoms|…`) 与 add(`atoms|…`, …)
  const mutableKeys = [...js.matchAll(/(?:cachedObject|\badd)\(\s*`(atoms|share)[^`]*`/g)].map((m) => m[0]);
  // main.js 才有「原子/份额图层 + 半径快速路径」这套组合，另外两页没有可变图层键
  if (script === 'src/main.js') check(`${script} 找到可变图层键`, mutableKeys.length >= 2, true);
  check(`${script} 可变图层的缓存键与半径无关`,
    mutableKeys.every((k) => !k.includes('radius') && !/\$\{q2\(/.test(k)), true);
}

/* ---------- 淡入动画：材质是共享缓存的，动画不能把资源留在中间态 ---------- */
console.log('\n=== 淡入动画（共享材质） ===');
{
  const geo = new THREE.SphereGeometry(1, 8, 6);
  const shared = new THREE.MeshStandardMaterial({ transparent: true, opacity: 0.5 });
  shared.userData.baseOpacity = 0.5;
  const a = new THREE.Mesh(geo, shared);
  const b = new THREE.Mesh(geo, shared);           // 与 a 共用同一个材质
  const grow = new THREE.Mesh(geo, new THREE.MeshStandardMaterial());
  grow.scale.setScalar(0.4);
  grow.userData.grow = true;
  const inst = new THREE.InstancedMesh(geo, new THREE.MeshStandardMaterial({ transparent: true, opacity: 0.4 }), 10);
  inst.userData.fullCount = 10;
  const root = new THREE.Group();
  root.add(a, b, grow, inst);

  const ap = makeAppear();
  ap.begin(root);
  check('淡入起点为 0', shared.opacity, 0, 1e-9);
  check('生长起点接近 0', grow.scale.x < 1e-3, true);
  check('实例数起点为 0', inst.count, 0);
  ap.tick(performance.now() + 5000);
  check('动画结束回到基准不透明', shared.opacity, 0.5, 1e-9);
  check('动画结束回到原尺寸', grow.scale.x, 0.4, 1e-6);
  check('动画结束实例数恢复', inst.count, 10);

  ap.begin(root);
  ap.settle();                                     // 中途取消（例如切图层）
  check('settle 立即恢复共享材质', shared.opacity, 0.5, 1e-9);
  check('settle 不丢共享材质的第二个对象', b.material.opacity, 0.5, 1e-9);
  check('settle 后不再有动画项', ap.tick(performance.now()), false);
}

/* ---------- 六方密堆积 HCP：堆积率是怎么算出来的 ---------- */
console.log('\n=== 六方密堆积 HCP ===');
{
  const a = A;
  check('理想轴比 c/a = √(8/3)', IDEAL_CA, Math.sqrt(8 / 3), 1e-12);
  check('理想轴比 ≈ 1.633', IDEAL_CA, 1.6329932, 1e-6);

  // 正四面体推导：三层几何关系自己长出 c/a
  const tet = hcpTetra(a);
  check('三角形中心到顶点 = a/√3', tet.inPlaneR, a / Math.sqrt(3), 1e-12);
  check('四面体高 = √(2/3)·a', tet.height, Math.sqrt(2 / 3) * a, 1e-12);
  check('由四面体推出 c/a', tet.cOverA, IDEAL_CA, 1e-12);

  // 晶胞里的原子数：12×1/6 + 2×1/2 + 3×1 = 6
  const atoms = hcpAtoms(a);
  check('晶胞内画出的原子位置数（12+2+3）', atoms.length, 17);
  check('按份额求和 = 6 个原子', hcpAtomsPerCell(a), 6, 1e-12);
  const kinds = {};
  for (const x of atoms) kinds[x.kind] = (kinds[x.kind] || 0) + 1;
  check('顶角 12 / 底面中心 2 / 中间层 3', `${kinds.corner}/${kinds.base}/${kinds.mid}`, '12/2/3');

  // 晶胞里的每个原子都必须落在完整点阵上（两套坐标约定必须自洽）
  const lat = hcpLattice(a, IDEAL_CA, 3 * a);
  const stray = atoms.filter((x) => !lat.some((p) => p.distanceTo(x.pos) < 1e-9));
  check('晶胞原子与完整点阵一致', stray.length, 0);

  // 距离与接触
  const d = hcpDistances(a, IDEAL_CA);
  check('理想时层间距离 = a（也是最近邻）', d.betweenLayers, a, 1e-12);
  check('理想时刚球半径 = a/2', hcpRadius(a, IDEAL_CA), a / 2, 1e-12);
  check('理想时配位数 = 12', hcpCoordination(a, IDEAL_CA), 12);
  // 层内 6 + 上层 3 + 下层 3
  const center = v(0, 0, 0);
  const near = lat.filter((p) => p.distanceTo(center) < a * 1.02 && p.length() > 1e-9);
  const inPlane = near.filter((p) => Math.abs(p.z) < 1e-6).length;
  const above = near.filter((p) => p.z > 1e-6).length;
  check('同层最近邻 6 个', inPlane, 6);
  check('上一层 3 个（下一层对称也是 3）', above, 3);

  // 棱柱体积与堆积率
  const idealC = a * IDEAL_CA;
  check('棱柱体积 = (3√3/2)a²c', hexPrismVolume(a, IDEAL_CA), ((3 * Math.sqrt(3)) / 2) * a * a * idealC, 1e-9);
  check('棱柱体积 = 3√2·a³', hexPrismVolume(a, IDEAL_CA), 3 * Math.SQRT2 * a ** 3, 1e-9);
  const apf = hcpPackingFraction(a, IDEAL_CA);
  check('HCP 堆积率 = π/(3√2)', apf, Math.PI / (3 * Math.SQRT2), 1e-12);
  check('HCP 堆积率 = 74.05%（和 FCC 完全相同）', apf, Math.sqrt(2) * Math.PI / 6, 1e-12);
  check('π/(3√2) 与 √2π/6 是同一个数', Math.PI / (3 * Math.SQRT2), (Math.SQRT2 * Math.PI) / 6, 1e-12);

  // 真实金属：c/a 偏离理想值 → 堆积率掉下来（这就是“为什么只有 74% 是上限”）
  for (const m of HCP_METALS.filter((x) => x.a)) {
    const ca = m.c / m.a;
    const f = hcpPackingFraction(a, ca);
    const ok = f <= apf + 1e-9;
    check(`${m.name} c/a=${ca.toFixed(3)} 堆积率 ${(f * 100).toFixed(1)}% ≤ 74.05%`, ok, true);
  }
  const mg = hcpPackingFraction(a, HCP_METALS.find((m) => m.key === 'Mg').c / HCP_METALS.find((m) => m.key === 'Mg').a);
  const zn = hcpPackingFraction(a, HCP_METALS.find((m) => m.key === 'Zn').c / HCP_METALS.find((m) => m.key === 'Zn').a);
  check('Mg 接近理想（> 73%）', mg > 0.73, true);
  check('Zn 明显偏离（< 70%）', zn < 0.70, true);
  check('Zn 是 c/a 偏大、层间不接触', hcpDistances(a, 4.947 / 2.665).betweenLayers > a, true);
  check('Be 是 c/a 偏扁、层间先接触（半径由层间决定）',
    hcpRadius(a, 3.584 / 2.286) < a / 2 - 1e-6, true);
}

/* ---------- 份额裁剪真的切出了 1/8、1/2、1/6 吗（蒙特卡洛数立体角） ---------- */
console.log('\n=== 份额裁剪的立体角验证 ===');
{
  // mulberry32：固定种子、可复现，且没有朴素 LCG 低位相关的偏差
  let seed = 20261009;
  const rnd = () => {
    seed = (seed + 0x6d2b79f5) | 0;
    let t = seed;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };

  const N = 20000;
  const TOL = 0.012;                       // 20000 点的统计涨落约 0.004，留一倍余量
  // 两种写法都支持：THREE.Plane（立方页用）与 { n, d } 规格（geometry.js 给 HCP 用）
  const asND = (list) => list.map((pl) => (pl.normal
    ? [[pl.normal.x, pl.normal.y, pl.normal.z], pl.constant]
    : [pl.n, pl.d]));

  /** 从球心向四面八方均匀撒方向，数“被裁剪面留住”的比例 = 该原子落在本胞内的份额 */
  const fraction = (origin, planes, r = 0.8) => {
    const nd = asND(planes);
    let inside = 0;
    for (let i = 0; i < N; i++) {
      const u = rnd() * 2 - 1;
      const phi = rnd() * Math.PI * 2;
      const s = Math.sqrt(1 - u * u);
      const p = v(origin.x + r * s * Math.cos(phi), origin.y + r * s * Math.sin(phi), origin.z + r * u);
      if (nd.every(([n, d]) => n[0] * p.x + n[1] * p.y + n[2] * p.z + d >= 0)) inside++;
    }
    return inside / N;
  };

  // 立方晶胞：顶点 1/8、面心 1/2、体心 1
  const cubic = atomsFor('FCC');
  const cornerC = cubic.find((x) => x.kind === 'corner');
  const faceC = cubic.find((x) => x.kind === 'face');
  const bodyB = atomsFor('BCC').find((x) => x.kind === 'body');
  check('立方·顶角裁出 1/8', fraction(cornerC.pos, sharePlanes(cornerC.pos, HALF)), 1 / 8, TOL);
  check('立方·面心裁出 1/2', fraction(faceC.pos, sharePlanes(faceC.pos, HALF)), 1 / 2, TOL);
  check('立方·体心不裁', fraction(bodyB.pos, sharePlanes(bodyB.pos, HALF)), 1, 1e-9);

  // 六方棱柱：顶角 1/6、底面中心 1/2、中间层 1
  const hc = hcpAtoms(A, IDEAL_CA);
  const corner = hc.find((x) => x.kind === 'corner');
  const base = hc.find((x) => x.kind === 'base');
  const mid = hc.find((x) => x.kind === 'mid');
  check('HCP·顶角裁出 1/6', fraction(corner.pos, hcpSharePlanes(corner, A, IDEAL_CA)), 1 / 6, TOL);
  check('HCP·底面中心裁出 1/2', fraction(base.pos, hcpSharePlanes(base, A, IDEAL_CA)), 1 / 2, TOL);
  check('HCP·中间层不裁', fraction(mid.pos, hcpSharePlanes(mid, A, IDEAL_CA)), 1, 1e-9);

  // 侧面必须正好过该顶角（内切距 √3a/2）；写成 a/2 就把 1/6 切歪了
  const sides = hcpSharePlanes(corner, A, IDEAL_CA).filter((s) => s.n[2] === 0);
  check('HCP·顶角有两条侧面', sides.length, 2);
  for (const s of sides)
    check('HCP·侧面过该顶角', Math.abs(s.n[0] * corner.pos.x + s.n[1] * corner.pos.y + s.d), 0, 1e-9);
  check('HCP·侧面距离 = 内切距 √3a/2', sides[0].d, (Math.sqrt(3) / 2) * A, 1e-12);
}

console.log(`\n${fail === 0 ? '全部通过 ✅' : `有 ${fail} 项失败 ❌`}`);
process.exit(fail === 0 ? 0 : 1);
