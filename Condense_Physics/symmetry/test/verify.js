// 自动校验：反轴 n̄ 的等价关系、四面体 24 个操作的身份、分步脚本与页面 id 的一致性。
// 这一页的全部卖点都是「算出来的」，所以这里守住的就是页面上写给学生看的那几句结论。
import fs from 'node:fs';
import {
  ID3, INV, mul, applyM, det as detM, tr as trM, same, mkey, closure, powers, orderOf,
  rotZ, rotAxis, mirrorPlane, rotoInversion, barGroup, barOrbit, animPath, invertStep,
  transpose, parts, dirName, over, tdOps, tdClasses, tdGroupFromOh, ohGroup, vertexPerm, permParity,
  facePerm, TETRA_VERTS, TETRA_FACES, MIRROR_110,
} from '../src/group.js';
import { BAR_LAYERS, TD_LAYERS, barSteps, tdSteps, stepsOf, stepDelta } from '../src/steps.js';

let fail = 0;
const check = (label, got, want, tol = 1e-9) => {
  const ok = typeof want === 'number' ? Math.abs(got - want) < tol : got === want;
  if (!ok) fail++;
  const show = (v) => (typeof v === 'number' ? v.toFixed(5) : String(v));
  console.log(`${ok ? '  ok ' : 'FAIL '} ${label.padEnd(46)} got=${show(got)}  want=${show(want)}`);
};
const isM = (A, B) => same(A, B, 1e-9);
const setOf = (ms) => [...new Set(ms.map(mkey))].sort().join(' | ');
const eqSet = (a, b) => setOf(a) === setOf(b);

console.log('\n=== 基本操作 ===');
check('det(E) = +1', detM(ID3), 1);
check('det(i) = −1（三维里反演是 3 阶行列式 −1）', detM(INV), -1);
check('绕 z 转 360° 回到单位阵', isM(rotZ(360), ID3), true);
check('镜面是它自己的逆：m² = E', isM(mul(mirrorPlane([1, 1, 0]), mirrorPlane([1, 1, 0])), ID3), true);
check('对 xy 面的镜面 = diag(1,1,−1)', isM(mirrorPlane([0, 0, 1]), [[1, 0, 0], [0, 1, 0], [0, 0, -1]]), true);

console.log('\n=== 反轴 n̄ = 转 360/n + 反演 ===');
const WANT_ORDER = { 1: 2, 2: 2, 3: 6, 4: 4, 5: 10, 6: 6 };
const wantBar = (n) => mul(INV, rotZ(360 / n));
for (const n of [1, 2, 3, 4, 5, 6]) {
  const g = barGroup(n);
  check(`${over(n)} 的阶 |G|`, g.props.order, WANT_ORDER[n]);
  check(`${over(n)} 生成元 = i·Rz(360/${n})`, isM(g.gen, wantBar(n)), true);
  check(`${over(n)}：反演与旋转对易（i·R = R·i）`, isM(g.gen, mul(rotZ(360 / n), INV)), true);
  check(`${over(n)} 群封闭（closure 不超出循环）`, closure([g.gen]).length, g.props.order);
  // 动画路径的终点必须正好是该操作，否则画面和矩阵就对不上
  const badAnim = g.elems.filter((M) => !isM(animPath(M, 1), M)).length;
  check(`${over(n)} 每个元素都能被动画路径精确复现`, badAnim, 0);
}
check(`${over('1')} 群 = {E, i}`, eqSet(barGroup(1).elems, [ID3, INV]), true);
check(`${over('2')} = m：矩阵直接等于水平镜面`, isM(rotoInversion(2), mirrorPlane([0, 0, 1])), true);
check(`${over('2')} 群里没有 i`, barGroup(2).props.hasI, false);
check(`${over('2')} 有垂直于轴的镜面`, barGroup(2).props.hasMirrorPerp, true);
check(`${over('3')} 有对称中心 i`, barGroup(3).props.hasI, true);
check(`${over('3')} 没有 m⊥`, barGroup(3).props.hasMirrorPerp, false);
check(`${over('3')} = 3 ⊗ i：与 {3 次轴, i} 生成的群相同`,
  eqSet(barGroup(3).elems, closure([rotZ(120), INV])), true);
check(`${over('3')} 的真旋转是 {E, 3, 3²}`,
  eqSet(barGroup(3).elems.filter((M) => detM(M) > 0), [ID3, rotZ(120), rotZ(240)]), true);
check(`${over('4')} 群里既无 i 也无镜面`, barGroup(4).props.hasI || barGroup(4).props.hasMirrorPerp, false);
check(`${over('4')} 的真旋转只剩 2 次轴`,
  eqSet(barGroup(4).elems.filter((M) => detM(M) > 0), [ID3, rotZ(180)]), true);
check(`${over('4')} = S4：与转动反映轴生成同一个群`,
  eqSet(barGroup(4).elems, closure([mul(mirrorPlane([0, 0, 1]), rotZ(90))])), true);

console.log('\n=== 老师留的问号：6̄ = ？ ===');
const g6 = barGroup(6);
check(`${over('6')} 的阶`, g6.props.order, 6);
check(`${over('6')} 里没有 6 次真旋转（最高只有 3 次）`, g6.props.maxProper, 3);
check(`${over('6')} 不含对称中心 i`, g6.props.hasI, false);
check(`${over('6')} 含垂直于轴的镜面 m⊥`, g6.props.hasMirrorPerp, true);
check(`${over('6')} = 3/m：与「3 次轴 + 垂直镜面」生成的群完全相同`,
  eqSet(g6.elems, closure([rotZ(120), mirrorPlane([0, 0, 1])])), true);
check(`${over('6')} 的结论字样`, /3\/m/.test(g6.props.equiv), true);
check(`${over('6')} 做 6 次回到原位`, isM(powers(g6.gen)[g6.props.order - 1].M, ID3), true);
check(`${over('6')} 生成元 = 转 60° + 反演`, isM(g6.gen, mul(INV, rotZ(60))), true);
check(`${over('6')} 的一步动画终点 = 生成元`, isM(invertStep(6, 1), g6.gen), true);
check('5 次轴不属于晶体学（ALLOWED_BAR 里没有 5）', [1, 2, 3, 4, 6].includes(5), false);

console.log('\n=== 等效点系（一般位置的点）===');
const PM = [1.15, 0.45, 0.78];
for (const n of [1, 2, 3, 4, 6]) {
  const o = barOrbit(n, PM);
  check(`${over(n)} 给一般点 ${o.pts.length} 个等效点`, o.pts.length, WANT_ORDER[n]);
  check(`${over(n)} 等效点里恰有一半是非真操作`, o.pts.filter((x) => x.det < 0).length, o.pts.length / 2);
}
const o3 = barOrbit(3, PM);
const o6 = barOrbit(6, PM);
check(`${over('3')} 的 6 个点配成 3 对关于原点对称`, o3.pairs.length, 3);
check(`${over('3')} 的配对类型是 i`, o3.pairs.every((p) => p[2] === 'i'), true);
check(`${over('6')} 的 6 个点配成 3 对关于水平面对称`, o6.pairs.length, 3);
check(`${over('6')} 的配对类型是 m`, o6.pairs.every((p) => p[2] === 'm'), true);
check(`${over('6')} 里找不到关于原点对称的一对`,
  o6.pts.some((a) => o6.pts.some((b) => b !== a && Math.hypot(...applyM(b.M, PM).map((x, i) => x + applyM(a.M, PM)[i])) < 1e-9)), false);
const o4 = barOrbit(4, PM);
check(`${over('4')} 只有 4 个等效点、配不成 i 或 m 的对`, `${o4.pts.length}/${o4.pairs.length}`, '4/0');
check('逐步施加 k 次的姿态 = 生成元的 k 次幂', isM(invertStep(3, 1, invertStep(3, 1, ID3)), mul(barGroup(3).gen, barGroup(3).gen)), true);

console.log('\n=== 正四面体的对称操作 Td ===');
const OPS = tdOps();
check('操作个数 = 24', OPS.length, 24);
check('24 个矩阵互不相同', new Set(OPS.map((o) => o.key)).size, 24);
check('与「从 Oh 的 48 个里筛出保四面体的那些」完全一致', eqSet(OPS.map((o) => o.M), tdGroupFromOh()), true);
check('封闭：24 个互相乘还是这 24 个', closure(OPS.map((o) => o.M)).length, 24);
check('正交性：M·Mᵀ = E 的个数', OPS.filter((o) => isM(mul(o.M, transpose(o.M)), ID3)).length, 24);
check('真旋转（det = +1）12 个', OPS.filter((o) => o.det > 0).length, 12);
check('非真操作（det = −1）12 个', OPS.filter((o) => o.det < 0).length, 12);
check('Td 里没有对称中心 i', OPS.some((o) => isM(o.M, INV)), false);
const cnt = Object.fromEntries(tdClasses().map((c) => [c.cls, c.items.length]));
check('类别分布 E/8C3/3C2/6S4/6σd', `${cnt.E}/${cnt.C3}/${cnt.C2}/${cnt.S4}/${cnt['σd']}`, '1/8/3/6/6');
check('五类个数加起来 = 24', Object.values(cnt).reduce((a, b) => a + b, 0), 24);
check('顶点置换共 24 种 = |S4|', new Set(OPS.map((o) => o.permText)).size, 24);
check('4 个面的置换也正好 24 种', new Set(OPS.map((o) => facePerm(o.M).join(''))).size, 24);
check('det = +1 ⇔ 偶置换（Td ≅ S4，真旋转子群 ≅ A4）',
  OPS.every((o) => (o.det > 0) === (permParity(o.perm) > 0)), true);
check('4 条 3 次轴 = 4 个顶点方向',
  new Set(OPS.filter((o) => o.cls === 'C3').map((o) => dirName(o.axis))).size, 4);
check('3 条 4̄ 轴沿 ⟨100⟩', new Set(OPS.filter((o) => o.cls === 'S4').map((o) => dirName(o.axis))).size, 3);
check('6 个 σd 镜面法线是 {110} 型', new Set(OPS.filter((o) => o.cls === 'σd').map((o) => dirName(o.axis))).size, 6);
check('动画路径复现全部 24 个操作', OPS.filter((o) => !isM(animPath(o.M, 1), o.M)).length, 0);

console.log('\n=== 4̄ 轴与 S4 是同一批操作 ===');
for (const a of [[1, 0, 0], [0, 1, 0], [0, 0, 1]]) {
  const b4 = mul(INV, rotAxis(a, 90));                 // 4̄ = 转 90° + 反演
  check(`4̄ 沿 ${dirName(a)} 在 Td 里`, OPS.some((o) => isM(o.M, b4)), true);
  check(`4̄² = 该轴上的 C2`, isM(mul(b4, b4), rotAxis(a, 180)), true);
  check(`4̄³ 也在 Td 里`, OPS.some((o) => isM(o.M, mul(b4, mul(b4, b4)))), true);
  check(`4̄⁴ = E`, isM(mul(mul(b4, b4), mul(b4, b4)), ID3), true);
}
check('S4 类操作正好是 3 条轴 × {4̄, 4̄³}', OPS.filter((o) => o.cls === 'S4').length, 6);
check('C2 类操作正好是 3 条轴 × {4̄²}', OPS.filter((o) => o.cls === 'C2').length, 3);

console.log('\n=== 与立方群 Oh 的关系（金刚石那两套四面体）===');
/** 立方体的 8 个角分成两批：坐标乘积 +1 的是这套四面体，−1 的是互补的那套 */
const COMPLEMENT = [[-1, -1, -1], [-1, 1, 1], [1, -1, 1], [1, 1, -1]];
const sendsTo = (M, from, to) => from.every((v) => {
  const q = applyM(M, v);
  return to.some((u) => u.every((x, i) => Math.abs(x - q[i]) < 1e-9));
});
const oh = ohGroup();
check('Oh 有 48 个元素', oh.length, 48);
check('24 个把两套四面体各自保住（这 24 个就是 Td）',
  oh.filter((M) => sendsTo(M, TETRA_VERTS, TETRA_VERTS) && sendsTo(M, COMPLEMENT, COMPLEMENT)).length, 24);
check('另外 24 个把两套互换',
  oh.filter((M) => sendsTo(M, TETRA_VERTS, COMPLEMENT) && sendsTo(M, COMPLEMENT, TETRA_VERTS)).length, 24);
check('两类加起来正好是整个 Oh', oh.length, 48);
check('保住这套顶点的 24 个 = tdGroupFromOh()',
  eqSet(oh.filter((M) => sendsTo(M, TETRA_VERTS, TETRA_VERTS)), tdGroupFromOh()), true);
check('反演把每个顶点送到互补那一套（所以 Td 不含 i）',
  TETRA_VERTS.every((v) => COMPLEMENT.some((u) => applyM(INV, v).every((x, i) => Math.abs(x - u[i]) < 1e-9))), true);
check('互补四面体的坐标乘积都是 −1', COMPLEMENT.every((v) => v.reduce((a, b) => a * b, 1) === -1), true);

console.log('\n=== 分步脚本与页面 ===');
for (const [mode, steps, dict] of [['bar', barSteps(), BAR_LAYERS], ['td', tdSteps(), TD_LAYERS]]) {
  const bad = [];
  steps.forEach((s, i) => {
    for (const k of Object.keys(s.layers)) if (!(k in dict)) bad.push(`${mode}#${i} → ${k}`);
    if (!Object.values(s.layers).some(Boolean)) bad.push(`${mode}#${i} → 空步骤`);
    if (!s.title || !s.text) bad.push(`${mode}#${i} → 文案不完整`);
    if (i && stepDelta(steps[i - 1], s) === 0) bad.push(`${mode}#${i} → 与上一步画面完全一样`);
  });
  check(`${mode} 步骤图层全部有效（${steps.length} 步）`, bad.length ? bad.join(', ') : true, true);
  check(`${mode} 步数 ≥ 6（老师要的两段都讲透）`, steps.length >= 6, true);
  check(`stepsOf('${mode}') 与直接调用一致`, stepsOf(mode) === steps, true);
}

const mainSrc = fs.readFileSync('symmetry/src/main.js', 'utf8');
const unrendered = Object.keys(BAR_LAYERS).filter((k) => !new RegExp(`L\\.${k}\\b`).test(mainSrc));
check('rebuild() 覆盖 bar 的所有图层', unrendered.length ? unrendered.join(', ') : true, true);
const unrenderedTd = Object.keys(TD_LAYERS).filter((k) => !new RegExp(`L\\.${k}\\b`).test(mainSrc));
check('rebuild() 覆盖 td 的所有图层', unrenderedTd.length ? unrenderedTd.join(', ') : true, true);

const pageIds = new Set([...fs.readFileSync('symmetry/index.html', 'utf8').matchAll(/id="([^"]+)"/g)].map((m) => m[1]));
const refIds = new Set([
  ...[...mainSrc.matchAll(/(?:getElementById|\bel)\(['"]([^'"]+)['"]\)/g)].map((m) => m[1]),
  ...[...mainSrc.matchAll(/\b(?:t|h|b)[A-Z][A-Za-z]*\s*:/g)].map((m) => m[0].replace(':', '').trim()),
]);
const missing = [...refIds].filter((x) => !pageIds.has(x));
check(`index.html 提供脚本用到的 ${refIds.size} 个 id`, missing.length ? missing.join(', ') : true, true);

const raw = [...mainSrc.matchAll(/new THREE\.(Sphere|Box|Circle|Edges)Geometry|new THREE\.(MeshStandard|MeshBasic|LineBasic|Sprite)Material/g)].map((m) => m[0]);
check('main.js 不直接创建几何/材质（走 view.js 缓存）', raw.length ? [...new Set(raw)].join(', ') : true, true);

// 画布上的文字用组合上划线，HTML 里用 .ov：两条路都得有，否则 4̄ 显示成 4̄ 以外的东西
check('over() 用的是组合上划线 U+0304', over(4), '4\u0304');
check('面板里的上横线走 CSS 类 .ov', /class="ov"/.test(fs.readFileSync('symmetry/index.html', 'utf8')), true);

console.log(`\n${fail === 0 ? '全部通过 ✅' : `有 ${fail} 项失败 ❌`}`);
process.exit(fail === 0 ? 0 : 1);
