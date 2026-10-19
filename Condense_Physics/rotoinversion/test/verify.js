// 作业 A 的校验：画面上出现的每一个数字、每一条结论，都要能被算出来。
// 分三层：① 反轴的群论事实；② 展开图数出来的统计量（这页的论证全靠它）；
// ③ 分步脚本与页面 id、单文件产物没有外部引用。
import fs from 'node:fs';
import { ID3, INV, mul, applyM, det as detM, same, mkey, closure, rotZ, rotAxis, mirrorPlane, over } from '../../lib/matrix.js';
import { barGroup, barOrbit, rotoInversion, powGen, genOf, ALLOWED_BAR } from '../../lib/rotoinversion.js';
import { orbitModel } from '../../lib/projection.js';
import { STEPS, LAYERS } from '../src/steps.js';

let fail = 0;
const check = (label, got, want, tol = 1e-9) => {
  const ok = typeof want === 'number' ? Math.abs(got - want) < tol : got === want;
  if (!ok) fail++;
  const show = (v) => (typeof v === 'number' ? v.toFixed(5) : String(v));
  console.log(`${ok ? '  ok ' : 'FAIL '} ${label.padEnd(48)} got=${show(got)} want=${show(want)}`);
};
const isM = (A, B) => same(A, B, 1e-9);
const setOf = (ms) => [...new Set(ms.map(mkey))].sort().join('|');
const PM = [1.15, 0.45, 0.78];

console.log('\n=== 反轴：群与等价记号 ===');
const WANT = {
  1: { order: 2, i: true, m: false, proper: 1 },
  2: { order: 2, i: false, m: true, proper: 1 },
  3: { order: 6, i: true, m: false, proper: 3 },
  4: { order: 4, i: false, m: false, proper: 2 },
  6: { order: 6, i: false, m: true, proper: 3 },
};
for (const n of ALLOWED_BAR) {
  const g = barGroup(n);
  const w = WANT[n];
  check(`${over(n)} 的阶`, g.props.order, w.order);
  check(`${over(n)} 含对称中心 i`, g.props.hasI, w.i);
  check(`${over(n)} 含水平镜面 m⊥`, g.props.hasMirrorPerp, w.m);
  check(`${over(n)} 真旋转最高次数`, g.props.maxProper, w.proper);
  check(`${over(n)} 生成元 = 转 ${Math.round(360 / n)}° + 反演`, isM(g.gen, mul(INV, rotZ(360 / n))), true);
  check(`${over(n)} 做 |G| 次回到原位`, isM(powGen(n, g.props.order), ID3), true);
  check(`${over(n)} 做 |G|−1 次还没回去`, isM(powGen(n, g.props.order - 1), ID3), false);
}
check(`${over('2')} 的矩阵逐元素等于水平镜面`, isM(rotoInversion(2), mirrorPlane([0, 0, 1])), true);
check(`${over('3')} 的群 = ⟨3 次轴, i⟩`, setOf(barGroup(3).elems) === setOf(closure([rotZ(120), INV])), true);
check(`${over('6')} 的群 = ⟨3 次轴, m⊥⟩ = 3/m`, setOf(barGroup(6).elems) === setOf(closure([rotZ(120), mirrorPlane([0, 0, 1])])), true);
check(`${over('6')} 的群里没有 6 次真旋转`, barGroup(6).elems.filter((M) => detM(M) > 0 && isM(M, rotZ(60))).length, 0);
check(`${over('4')} 的群 = S4`, setOf(barGroup(4).elems) === setOf(closure([mul(mirrorPlane([0, 0, 1]), rotZ(90))])), true);
check(`${over('4')} 独立：既不能由 i 也不能由 m 表达`,
  barGroup(4).props.hasI === false && barGroup(4).props.hasMirrorPerp === false, true);

console.log('\n=== 展开图数出来的量（这页的论证全靠它）===');
const FAN = {
  1: { points: 2, up: 1, down: 1, aligned: 0, spots: 2 },
  2: { points: 2, up: 1, down: 1, aligned: 1, spots: 1 },
  3: { points: 6, up: 3, down: 3, aligned: 0, spots: 6 },
  4: { points: 4, up: 2, down: 2, aligned: 0, spots: 4 },
  6: { points: 6, up: 3, down: 3, aligned: 3, spots: 3 },
};
for (const n of ALLOWED_BAR) {
  const orbit = barOrbit(n, PM);
  const fan = orbitModel(orbit, '', { view: 'fan' }).stats;
  const top = orbitModel(orbit, '').stats;
  const w = FAN[n];
  check(`${over(n)} 展开图保住全部点数`, fan.points, w.points);
  check(`${over(n)} 展开图上圈/下圈`, `${fan.up}/${fan.down}`, `${w.up}/${w.down}`);
  check(`${over(n)} 下圈落在上圈径向上的个数`, fan.aligned, w.aligned);
  check(`${over(n)} 俯视落点数（教材画法）`, top.spots, w.spots);
  // 展开图不许把任何两点合并：否则「点数」这个最朴素的事实就被画错了
  check(`${over(n)} 展开图里点与点不重合`, fan.spots, fan.points);
}
check('3̄ 与 6̄ 的俯视图都各有 6 个点，区别只在配对方式',
  barOrbit(3, PM).pts.length === 6 && barOrbit(6, PM).pts.length === 6, true);
check('3̄ 是穿心（i 对 3、m 对 0）', (() => {
  const o = barOrbit(3, PM);
  return `${o.pairs.filter((p) => p[2] === 'i').length}/${o.pairs.filter((p) => p[2] === 'm').length}`;
})(), '3/0');
check('6̄ 是叠影（i 对 0、m 对 3）', (() => {
  const o = barOrbit(6, PM);
  return `${o.pairs.filter((p) => p[2] === 'i').length}/${o.pairs.filter((p) => p[2] === 'm').length}`;
})(), '0/3');
check('6̄ 的第 3 次幂就是水平镜面：(x,y,z)→(x,y,−z)',
  isM(powGen(6, 3), mirrorPlane([0, 0, 1])), true);
check('3̄ 的第 3 次幂就是反演', isM(powGen(3, 3), INV), true);
check('展开图自检：判成 stack 却数不齐就抛错', (() => {
  try {
    orbitModel({ pts: barOrbit(6, PM).pts, pairs: [], props: { hasI: false, hasMirrorPerp: true, order: 6 } }, '坏样本', { view: 'fan' });
    return false;
  } catch { return true; }
})(), true);

console.log('\n=== 分步脚本 ===');
const bad = [];
STEPS.forEach((s, i) => {
  for (const k of Object.keys(s.layers)) if (!(k in LAYERS)) bad.push(`${s.key} → 未知图层 ${k}`);
  if (!Object.values(s.layers).some(Boolean)) bad.push(`${s.key} → 空步骤`);
  if (!s.title || !s.note || !s.punch || !s.key) bad.push(`${s.key} → 文案不全`);
  if (!s.set || typeof s.set.n !== 'number') bad.push(`${s.key} → 没说要画哪根轴`);
  if (i && JSON.stringify(s.layers) === JSON.stringify(STEPS[i - 1].layers) && s.set.n === STEPS[i - 1].set.n) {
    bad.push(`${s.key} → 与上一步画面完全一样`);
  }
});
check(`8 步全部有效（${STEPS.length} 步）`, bad.length ? bad.join(', ') : true, true);
check('五种反轴都各有一步在讲', ALLOWED_BAR.every((n) => STEPS.some((s) => s.set.n === n && s.key.includes(over(n)))), true);
check('答案那一步落在 6̄', STEPS.find((s) => s.title.startsWith('6̄')).punch, '= 3/m，里面没有 6');

console.log('\n=== 页面与产物 ===');
const page = fs.readFileSync('rotoinversion/index.html', 'utf8');
const mainSrc = fs.readFileSync('rotoinversion/src/main.js', 'utf8');
const pageIds = new Set([...page.matchAll(/id="([^"]+)"/g)].map((m) => m[1]));
const refIds = new Set([
  ...[...mainSrc.matchAll(/(?:getElementById|\bel)\(['"]([^'"]+)['"]\)/g)].map((m) => m[1]),
  // 图层开关的 id 是运行时拼出来的（ly-<图层名>），得从模板串里把前缀认出来
  ...Object.keys(LAYERS).map((k) => `ly-${k}`),
]);
const missing = [...refIds].filter((x) => !pageIds.has(x) && !x.startsWith('ly-'));
check(`index.html 提供脚本用到的 ${refIds.size} 个 id`, missing.length ? missing.join(', ') : true, true);
check('每个图层都有名字（抽屉里的开关不会漏项）',
  Object.keys(LAYERS).every((k) => new RegExp(`${k}: '`).test(mainSrc)), true);
check('每个图层都有构造器', Object.keys(LAYERS).every((k) => new RegExp(`\\b${k}:`).test(fs.readFileSync('rotoinversion/src/render3d.js', 'utf8'))), true);
check('main.js 不自己 new 几何/材质',
  [...mainSrc.matchAll(/new THREE\.(Sphere|Box|Circle|Edges)Geometry|new THREE\.(MeshStandard|LineBasic|Sprite)Material/g)].length, 0);

const built = 'rotoinversion/rotoinversion.html';
if (fs.existsSync(built)) {
  const html = fs.readFileSync(built, 'utf8');
  check('单文件成品没有外部脚本/样式', /<script[^>]+src=|<link[^>]+href=/.test(html), false);
  check('单文件成品内联了 three', /WebGLRenderer|MeshStandardMaterial/.test(html), true);
  check('单文件成品体积合理（< 900 KB）', html.length < 900 * 1024, true);
} else {
  check('先跑 ./rotoinversion/build.sh 再验产物', false, true);
}

console.log(`\n${fail === 0 ? '全部通过 ✅' : `有 ${fail} 项失败 ❌`}`);
process.exit(fail === 0 ? 0 : 1);
