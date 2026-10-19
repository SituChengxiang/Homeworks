// 自动校验：{110} 晶面族的「12 个面 / 6 对平行面 / d = a/√2 / 菱形十二面体」，
// 以及面间距、二维网胞、面密度、晶带——页面上写给学生看的每个数字都来自这里验过的函数。
import fs from 'node:fs';
import {
  cubicRotations, cubicOperations, family, angleBetween, angleHistogram, formOf, planePatch, polyAngles,
  STRUCTURES, vPrim, atomicSpacing, planeMesh, pointsInPatch, zonesOf, neighboursOf,
  planeLabel, dirLabel, dotV, crossV,
} from '../src/family.js';
import { LAYERS, groupsOf, stepDelta, eqnOf, interceptsOf } from '../src/steps.js';

let fail = 0;
const check = (label, got, want, tol = 1e-9) => {
  const ok = typeof want === 'number' ? Math.abs(got - want) < tol : got === want;
  if (!ok) fail++;
  const show = (v) => (typeof v === 'number' ? v.toFixed(5) : String(v));
  console.log(`${ok ? '  ok ' : 'FAIL '} ${label.padEnd(50)} got=${show(got)}  want=${show(want)}`);
};

const SQ2 = Math.SQRT2;
const SQ3 = Math.sqrt(3);
const sub3 = (a, b) => a.map((x, i) => x - b[i]);
const len3 = (p) => Math.hypot(...p);
/** 平面多边形面积 = ½|Σ pᵢ × pᵢ₊₁|（三维矢量面积，与平面离原点多远无关） */
function polyArea3(poly) {
  const s = [0, 0, 0];
  for (let i = 0; i < poly.length; i++) {
    const a = poly[i]; const b = poly[(i + 1) % poly.length];
    const c = crossV(a, b);
    for (let k = 0; k < 3; k++) s[k] += c[k];
  }
  return len3(s) / 2;
}

console.log('\n=== 立方群 O 的 24 个真旋转 ===');
const ROT = cubicRotations();
check('真旋转个数 = 24', ROT.length, 24);
check('完整点群 m3m 有 48 个操作', cubicOperations().length, 48);
check('全是 det = +1（不含反映、不含反演）', ROT.every((M) => Math.abs(detOf(M) - 1) < 1e-12), true);
check('每个都是带号置换阵', ROT.every((M) => M.every((r) => Math.abs(r[0]) + Math.abs(r[1]) + Math.abs(r[2]) === 1)
  && M.map((r) => r.findIndex((x) => x !== 0)).every((x, i, all) => all.indexOf(x) === i)), true);
check('封闭：24 个互乘还在这 24 个里', closureOk(ROT), true);

function detOf(M) {
  return M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1])
    - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0])
    + M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);
}
function closureOk(list) {
  const keys = new Set(list.map((M) => M.map((r) => r.map(Math.round).join(',')).join(';')));
  for (const A of list) for (const B of list) {
    const C = [0, 1, 2].map((i) => [0, 1, 2].map((j) => A[i][0] * B[0][j] + A[i][1] * B[1][j] + A[i][2] * B[2][j]));
    if (!keys.has(C.map((r) => r.map(Math.round).join(',')).join(';'))) return false;
  }
  return true;
}

console.log('\n=== {110} 晶面族：12 个面、6 组 ===');
const F110 = family([1, 1, 0]);
check('成员个数 = 12（老师要的 12 个面）', F110.count, 12);
check('平行面对数 = 6（正好「2 个面一组、开六组」）', F110.pairs, 6);
check('每组恰好 2 个面', F110.groups.every((g) => g.members.length === 2), true);
check('12 个指标写法互不相同', new Set(F110.members.map((m) => m.label)).size, 12);
check('每组的两个面法线严格反向（互相平行）',
  F110.groups.every((g) => g.members[0].hkl.every((x, i) => x + g.members[1].hkl[i] === 0)), true);
check('每组的两个面夹角都是 180°',
  F110.groups.every((g) => angleBetween(g.members[0], g.members[1]) === 180), true);
check('负指标写成带上划线', planeLabel([-1, 1, 0]), '(1\u030410)');
check('晶向写法同理', dirLabel([1, -1, 0]), '[11\u03040]');
check('六组代表面按教材顺序排', F110.groups.map((g) => g.label).join(' '),
  '(110) (101) (011) (11\u03040) (101\u0304) (011\u0304)');
check('12 个面的指标都是 {1,1,0} 的带号排列',
  F110.members.every((m) => m.hkl.map((x) => Math.abs(x)).sort().join() === '0,1,1'), true);
check('每个面的 |n| = √2', F110.members.every((m) => Math.abs(m.L - SQ2) < 1e-12), true);
check('面间距 d₁₁₀ = a/√2 = 0.70711a', F110.members[0].d, 1 / SQ2, 1e-12);
check('12 个面的 d 全相等（同族等价）', new Set(F110.members.map((m) => m.d.toFixed(12))).size, 1);
check('法线 [hkl] 垂直于面内两个格矢（密勒指数就是这么定义的）',
  F110.members.every((m) => {
    const me = planeMesh('bcc', m.hkl);
    return Math.abs(dotV(m.hkl, me.u)) < 1e-9 && Math.abs(dotV(m.hkl, me.v)) < 1e-9;
  }), true);
check('面方程：(110) → x + y = 1', eqnOf(F110.groups[0].members[0]), 'x + y = 1');
check('面方程：(1̄1̄0) → −x − y = 1（等价于 x + y = −1）', eqnOf(F110.groups[0].members[1]), '−x − y = 1');
check('每组的两个面片关于原点对着（中心互为反点）',
  F110.groups.every((g) => {
    const c = (poly) => {
      const s = poly.reduce((a, p) => [a[0] + p[0], a[1] + p[1], a[2] + p[2]], [0, 0, 0]);
      return s.map((x) => x / poly.length);
    };
    const a = c(planePatch(g.members[0].hkl, 1, 1));
    const b = c(planePatch(g.members[1].hkl, 1, 1));
    return a.every((x, i) => Math.abs(x + b[i]) < 1e-9) && len3(a) > 0.5;
  }), true);
check('六组 12 张面片两两不重合（12 个不同的截面）',
  new Set(F110.members.map((m) => planePatch(m.hkl, 1, 1).map((p) => p.map((x) => x.toFixed(4)).join(',')).join(';'))).size, 12);
check('截距：(110) 与 z 轴平行 → (a, a, ∞)', interceptsOf(F110.groups[0].members[0]), '(a, a, ∞)');
check('截距：(11̄0) 给出负截距 → (a, −a, ∞)', interceptsOf(F110.groups[3].members[0]), '(a, −a, ∞)');

console.log('\n=== 换一族也照样算：{100} {111} {112} {123} ===');
const F100 = family([1, 0, 0]);
const F111 = family([1, 1, 1]);
check('{100}：6 个面 / 3 对 / d = 1.0000a', `${F100.count}/${F100.pairs}/${F100.members[0].d.toFixed(4)}`, '6/3/1.0000');
check('{111}：8 个面 / 4 对', `${F111.count}/${F111.pairs}`, '8/4');
check('{111} 的 d = a/√3', F111.members[0].d, 1 / SQ3, 1e-12);
check('{112}：24 个面 / 12 对', `${family([1, 1, 2]).count}/${family([1, 1, 2]).pairs}`, '24/12');
check('一般位置 {123}：48 个面 / 24 对', `${family([1, 2, 3]).count}/${family([1, 2, 3]).pairs}`, '48/24');

console.log('\n=== 组合：12 个面围成菱形十二面体 ===');
const FORM = formOf(F110);
check('顶点数 = 14（8 个三重 + 6 个四重）', FORM.vertCount, 14);
check('面数 = 12（一张不少）', FORM.faceCount, 12);
check('棱数 = 24', FORM.edgeCount, 24);
check('棱的两端都是有效顶点下标且长度非零（渲染层就靠这两个下标取坐标）',
  FORM.edges.every(([a, b]) => FORM.verts[a] && FORM.verts[b] && len3(sub3(FORM.verts[a], FORM.verts[b])) > 0.1), true);
check('每条棱恰好被两张面共用（欧拉公式 V−E+F=2）',
  FORM.vertCount - FORM.edgeCount + FORM.faceCount, 2);
check('每个面都是四边形（菱形）', FORM.faces.every((f) => f.poly.length === 4), true);
check('每个面面积 = a²/√2 = 0.70711', FORM.faces[0].area, 1 / SQ2, 1e-9);
check('12 个面面积全相等', new Set(FORM.faces.map((f) => f.area.toFixed(9))).size, 1);
const RH = FORM.faces[0].poly;
const IA = polyAngles(RH).sort((a, b) => a - b);
check('菱形内角 = 70.53° / 109.47°', `${IA[0]}/${IA[3]}`, '70.5288/109.4712');
check('内角和 = 360°', polyAngles(RH).reduce((a, b) => a + b, 0), 360, 1e-6);
check('两条对角线之比 = √2', len3(sub3(RH[0], RH[2])) / len3(sub3(RH[1], RH[3])), SQ2, 1e-9);
check('12 张晶形面正好对应族的 12 个成员',
  FORM.faces.map((f) => f.member.label).sort().join(' '), [...F110.members.map((m) => m.label)].sort().join(' '));
check('每个面有 4 个邻面', FORM.faces.every((f, i) => neighboursOf(FORM, i).length === 4), true);
check('邻面法线夹角 = 60°（二面角 120°）',
  neighboursOf(FORM, 0).every((g) => Math.abs(angleBetween(FORM.faces[0].member, g.member) - 60) < 1e-6), true);
check('14 个顶点坐标只取 0、±½、±1',
  FORM.verts.every((p) => p.every((x) => [0, 0.5, 1].some((v) => Math.abs(Math.abs(x) - v) < 1e-9))), true);
check('6 个四重顶点在距离 a 处（⟨100⟩ 格点）', FORM.verts.filter((p) => Math.abs(len3(p) - 1) < 1e-9).length, 6);
check('8 个三重顶点在 √3/2·a 处（立方体的角）', FORM.verts.filter((p) => Math.abs(len3(p) - SQ3 / 2) < 1e-9).length, 8);
check('原点到每张晶形面的距离 = d₁₁₀',
  FORM.faces.every((f) => Math.abs(dotV(f.member.n, f.center) - 1 / SQ2) < 1e-9), true);
check('{100} 的晶形 = 立方体 8/6/12',
  (() => { const f = formOf(F100); return `${f.vertCount}/${f.faceCount}/${f.edgeCount}`; })(), '8/6/12');
check('{111} 的晶形 = 八面体 6/8/12',
  (() => { const f = formOf(F111); return `${f.vertCount}/${f.faceCount}/${f.edgeCount}`; })(), '6/8/12');
check('{111} 每个面是三角形', formOf(F111).faces.every((f) => f.poly.length === 3), true);

console.log('\n=== 一个面的邻居分布 ===');
const HIST = Object.fromEntries(angleHistogram(F110));
check('与 (110) 成 60° 的：4 个（共棱的邻面）', HIST[60], 4);
check('与 (110) 成 90° 的：2 个', HIST[90], 2);
check('与 (110) 成 120° 的：4 个', HIST[120], 4);
check('与 (110) 成 180° 的：1 个（它的平行伙伴）', HIST[180], 1);
check('四类加起来 = 其余 11 个成员', [60, 90, 120, 180].reduce((a, k) => a + (HIST[k] || 0), 0), 11);
check('六组里第 4 组与第 1 组垂直', angleBetween(F110.groups[0].members[0], F110.groups[3].members[0]), 90);
check('六组里第 2 组与第 1 组成 60°', angleBetween(F110.groups[0].members[0], F110.groups[1].members[0]), 60);

console.log('\n=== 面片：切 2a×2a×2a 晶体得到的截面 ===');
const P1 = planePatch([1, 1, 0], 1, 1);
const P0 = planePatch([1, 1, 0], 0, 1);
check('x+y=1 切出矩形（4 个顶点）', P1.length, 4);
check('矩形面积 = √2·a × 2a = 2.8284a²', polyArea3(P1), 2 * SQ2, 1e-9);
check('过原点的 (110) 面面积 = 2√2·2a = 5.6569a²', polyArea3(P0), 4 * SQ2, 1e-9);
check('面片上每一点到原点的距离都是 d₁₁₀', P1.every((p) => Math.abs(dotV(p, F110.members[0].n) - 1 / SQ2) < 1e-9), true);
check('(110) 面含晶胞的一条竖直棱（x=y=½）',
  [[0.5, 0.5, 0.5], [0.5, 0.5, -0.5]].every((p) => Math.abs(dotV(p, [1, 1, 0]) - 1) < 1e-9), true);
check('12 张面在晶体里都切得出东西（非空）',
  F110.members.filter((m) => planePatch(m.hkl, m.t, 1).length >= 3).length, 12);
check('{100} 的面片是 2a×2a 的正方形', polyArea3(planePatch([1, 0, 0], 1, 1)), 4, 1e-9);
check('{111} 的面片正好过 cube 三个角 → 三角形', planePatch([1, 1, 1], 1, 1).length, 3);
check('{111} 的三角形面积 = 2√3 a²', polyArea3(planePatch([1, 1, 1], 1, 1)), 2 * SQ3, 1e-9);

console.log('\n=== 面间距与面密度：三种立方结构 ===');
const WANT_D = {
  'sc|100': 1, 'sc|110': 1 / SQ2, 'sc|111': 1 / SQ3,
  'bcc|100': 0.5, 'bcc|110': 1 / SQ2, 'bcc|111': 0.5 / SQ3,
  'fcc|100': 0.5, 'fcc|110': 0.5 / SQ2, 'fcc|111': 1 / SQ3,
};
for (const key of Object.keys(STRUCTURES)) {
  for (const hkl of [[1, 0, 0], [1, 1, 0], [1, 1, 1]]) {
    const tag = `${key}|${hkl.join('')}`;
    check(`${STRUCTURES[key].short} {${hkl.join('')}} 相邻含格点面间距`,
      atomicSpacing(key, hkl), WANT_D[tag], 1e-9);
    check(`${STRUCTURES[key].short} {${hkl.join('')}} 网胞面积 × 面间距 = 原胞体积`,
      planeMesh(key, hkl).area * atomicSpacing(key, hkl), vPrim(key), 1e-9);
  }
}
check('BCC {110} 面密度 = √2/a² = 1.4142', planeMesh('bcc', [1, 1, 0]).density, SQ2, 1e-6);
check('换成原子半径：BCC(110) = 0.265/R²（教材值）',
  planeMesh('bcc', [1, 1, 0]).density * STRUCTURES.bcc.radius ** 2, 3 * SQ2 / 16, 1e-6);
check('FCC {110} 面密度 = √2/a²', planeMesh('fcc', [1, 1, 0]).density, SQ2, 1e-6);
check('FCC {111} 面密度 = 4/(√3 a²)', planeMesh('fcc', [1, 1, 1]).density, 4 / SQ3, 1e-6);
check('SC {100} 面密度 = 1/a²', planeMesh('sc', [1, 0, 0]).density, 1, 1e-6);
check('BCC 的 {110} 比 {100}、{111} 都密', [planeMesh('bcc', [1, 0, 0]).density, planeMesh('bcc', [1, 1, 1]).density]
  .every((x) => x < planeMesh('bcc', [1, 1, 0]).density), true);
const densest = (key) => [[[1, 0, 0], '{100}'], [[1, 1, 0], '{110}'], [[1, 1, 1], '{111}']]
  .map(([h, l]) => [l, planeMesh(key, h).density]).sort((a, b) => b[1] - a[1])[0][0];
check('最密面：简单立方 = {100}', densest('sc'), '{100}');
check('最密面：体心立方 = {110}（本页主角）', densest('bcc'), '{110}');
check('最密面：面心立方 = {111}', densest('fcc'), '{111}');

console.log('\n=== 面里的格点 ===');
const FACE110 = FORM.faces.find((f) => f.member.label === '(110)');
check('BCC：菱形面 4 个顶点全是格点（这一张面就是一个二维原胞）',
  pointsInPatch('bcc', [1, 1, 0], 1, FACE110.poly).length, 4);
check('SC：同一张菱形面里只有 2 个格点',
  pointsInPatch('sc', [1, 1, 0], 1, FACE110.poly).length, 2);
check('FCC：同一张菱形面里 3 个格点（多一个面心）',
  pointsInPatch('fcc', [1, 1, 0], 1, FACE110.poly).length, 3);
check('BCC 的 (110) 面上有 (1,0,0) 与 (½,½,½)',
  [[1, 0, 0], [0.5, 0.5, 0.5]].every((p) => pointsInPatch('bcc', [1, 1, 0], 1, FACE110.poly)
    .some((q) => len3(sub3(q, p)) < 1e-9)), true);

console.log('\n=== 晶带：共用一个晶向的那些面 ===');
const Z110 = zonesOf(F110);
check('晶带轴个数（≥3 面共带）= 7', Z110.length, 7);
check('4 条 ⟨111⟩ 带轴各带 6 个面', Z110.filter((z) => z.members.length === 6).length, 4);
check('3 条 ⟨001⟩ 带轴各带 4 个面', Z110.filter((z) => z.members.length === 4).length, 3);
check('晶带判据 hu+kv+lw = 0 对带内每个面成立',
  Z110.every((z) => z.members.every((m) => Math.abs(dotV(z.dir, m.hkl)) < 1e-12)), true);
check('带外的面一律不满足判据',
  Z110.every((z) => F110.members.filter((m) => Math.abs(dotV(z.dir, m.hkl)) < 1e-12).length === z.members.length), true);
const Z001 = Z110.find((z) => z.dir.join(',') === '0,0,1');
check('[001] 带里是 4 个 l = 0 的面（绕 z 一圈）', Z001.members.length, 4);
check('[001] 带里正好两对平行面', new Set(Z001.members.map((m) => m.group)).size, 2);
check('[111] 带里 6 个面 = 3 对平行面',
  new Set(Z110.find((z) => z.dir.join(',') === '1,1,1').members.map((m) => m.group)).size, 3);

console.log('\n=== 分步脚本：六组 + 一个组合 ===');
const GS = groupsOf(F110, FORM);
check('演示组数 = 6 组平行对 + 1 个组合', GS.length, 7);
check('前六组与六对平行面一一对应', GS.slice(0, 6).map((s) => s.group.label).join(' '),
  F110.groups.map((g) => g.label).join(' '));
check('每组点亮恰好 2 个面', GS.slice(0, 6).every((s) => s.faces.length === 2), true);
check('组合视图点亮全部 12 个面', GS[6].faces.length, 12);
check('组合视图按平行对配色（12 张面 6 种颜色）',
  new Set(GS[6].faces.map((m) => m.group)).size, 6);
const bad = [];
GS.forEach((s, i) => {
  for (const k of Object.keys(s.layers)) if (!(k in LAYERS)) bad.push(`#${i} → ${k}`);
  if (!Object.values(s.layers).some(Boolean)) bad.push(`#${i} → 空步骤`);
  if (!s.title || !s.text) bad.push(`#${i} → 文案不完整`);
  if (i && stepDelta(GS[i - 1], s) === 0) bad.push(`#${i} → 与上一组画面完全一样`);
});
check('7 组图层键全部有效、无空步、相邻组画面不重复', bad.length ? bad.join(', ') : true, true);
check('第 1 组带上截距/过原点面/二维网格三样', ['intercept', 'through', 'net']
  .every((k) => GS[0].layers[k]), true);
check('第 2…6 组画法一致（同级对照，不搞特殊）',
  GS.slice(1, 6).every((s) => JSON.stringify(s.layers) === JSON.stringify(GS[1].layers)), true);

const mainSrc = fs.readFileSync('lattice-planes/src/main.js', 'utf8');
// 图层要么在 MAKERS 里有一张画法（值是个函数名），要么由 pairLayer 用 LAYER_ON 消费
const handled = (k) => new RegExp(`\\b${k}:\\s*[A-Za-z$_]`).test(mainSrc) || mainSrc.includes(`LAYER_ON('${k}')`);
const unrendered = Object.keys(LAYERS).filter((k) => !handled(k));
check('rebuild() 覆盖全部图层', unrendered.length ? unrendered.join(', ') : true, true);
const raw = [...mainSrc.matchAll(/new THREE\.(Sphere|Box|Circle|Edges|Plane)Geometry|new THREE\.(MeshStandard|MeshBasic|LineBasic|Sprite)Material/g)].map((m) => m[0]);
check('main.js 不直接造几何/材质（走 src/view.js 缓存）', raw.length ? [...new Set(raw)].join(', ') : true, true);

const pageIds = new Set([...fs.readFileSync('lattice-planes/index.html', 'utf8').matchAll(/id="([^"]+)"/g)].map((m) => m[1]));
const refIds = new Set([...mainSrc.matchAll(/(?:getElementById|\bel)\(['"]([^'"]+)['"]\)/g)].map((m) => m[1]));
const missing = [...refIds].filter((x) => !pageIds.has(x));
check(`index.html 提供脚本用到的 ${refIds.size} 个 id`, missing.length ? missing.join(', ') : true, true);

console.log(`\n${fail === 0 ? '全部通过 ✅' : `有 ${fail} 项失败 ❌`}`);
process.exit(fail === 0 ? 0 : 1);
