// 分步脚本：{110} 的 12 个面按「2 个面一组、开六组」排，第七组把 12 个面合起来看成一个晶形。
// 每步只是「点亮哪几个面 + 图层开关 + 讲解文案」，真正画什么是 src/main.js 的 rebuild()。
// 文案里的每个数字都从 src/family.js 现算，不手写。
import { formOf, planeMesh, zonesOf, angleBetween, dirLabel } from './family.js';

/** 图层字典：页面开关、分步配置、校验脚本共用同一套键名 */
export const LAYERS = {
  cell: false,       // 立方晶胞框 + 三根晶轴
  crystal: false,    // 2a×2a×2a 的晶体块（面片就是切它切出来的截面）
  points: false,     // 格点
  atoms: false,      // 按真实半径摆出来的原子
  pair: false,       // 当前点亮的面：六组里是 2 张，组合视图里是 12 张
  through: false,    // 过原点的那张同向面（(hkl) 与 (h̄k̄l̄) 过原点时是同一张面）
  normal: false,     // 两根法线箭头 [hkl] / [h̄k̄l̄]
  labels: false,     // 贴在面上的 (hkl)
  intercept: false,  // 截距与倒数 → 密勒指数怎么来的
  stack: false,      // 一叠平行面 + 面间距 d 的标注
  net: false,        // 面内二维网胞、格点与面密度
  form: false,       // 晶形的棱与顶点（菱形十二面体）
  star: false,       // 把 12 张面都平移到原点：晶面族交于一点
  sphere: false,     // 参考球 + 12 个法线端点（截角八面体）
  zone: false,       // 晶带：共用同一晶向的那些面
};

const L = (keys) => Object.fromEntries(keys.map((k) => [k, true]));

/** 六组平行对共同的基础图层：六组是同级的，画法一律相同，只是点亮的面不一样 */
const BASE = L(['cell', 'crystal', 'points', 'pair', 'normal', 'labels', 'stack']);
/** 第一组多带上「怎么读密勒指数」的三样：截距、过原点的面、面内网格 */
const FIRST = L([...Object.keys(BASE), 'intercept', 'through', 'net']);
/** 组合视图：12 张面按晶形围起来，另加晶形轮廓 */
const FORM = L(['cell', 'points', 'pair', 'labels', 'form']);

/**
 * 生成分步内容：fam.groups 的前 6 组各点亮一对平行面，第 7 组点亮全部 12 个面。
 * @param fam  family([1,1,0]) 的结果
 * @param form formOf(fam) 的结果（组合视图要用它算邻面与内角）
 */
export function groupsOf(fam, form = formOf(fam)) {
  const zones = zonesOf(fam);
  const dTxt = `${fam.members[0].d.toFixed(4)}a`;
  const steps = fam.groups.map((g, i) => {
    const [pos, neg] = g.members;
    const perp = fam.groups.filter((o) => o !== g && Math.abs(angleBetween(o.members[0], pos) - 90) < 1e-6);
    const nb60 = fam.groups.filter((o) => o !== g && Math.abs(angleBetween(o.members[0], pos) - 60) < 1e-6);
    return {
      kind: 'group',
      group: g,
      faces: g.members,
      title: `第 ${i + 1} 组　${g.label} ∥ ${neg.label}`,
      text: `${pos.label} 与 ${neg.label} 是同一张面的两个朝向：法线 ${dirLabel(pos.hkl)} 与 ${dirLabel(neg.hkl)} 严格反向，`
        + `所以两面互相平行，到原点都正好是 d = ${dTxt}。${pos.label} 的面方程是 ${eqnOf(pos)}（坐标以 a 为单位），`
        + `截距 ${interceptsOf(pos)}，取倒数并化整就得到密勒指数 ${pos.label}；${neg.label} 自己的方程 ${eqnOf(neg)} `
        + `等价于把右端换成 −1，所以它在原点另一侧，与 ${pos.label} 隔着原点面对面平行。`
        + `这一组与${perp.map((o) => o.label).join('、') || '—'}垂直，与${nb60.map((o) => o.label).join('、')}成 60°。`
        + (i === 0 ? '第一组额外把截距、过原点的那张面、面内二维网格都打开，后面五组只留基础画法，照着这张图换方向看就行。' : ''),
      layers: i === 0 ? FIRST : BASE,
      set: { group: i },
    };
  });

  const rhomb = form.faces[0];
  const dr = diagRatio(rhomb.poly);
  const shape = dr === null
    ? `每个面是 ${rhomb.poly.length} 边形（内角 ${angTxt(rhomb.poly)}）`
    : `每个面是一张对角线比 ${dr.toFixed(3)} 的菱形（内角 ${angTxt(rhomb.poly)}）`;
  const nb = neighbourTxt(form);
  const is110 = fam.label === '(110)';
  const here = planeMesh('bcc', fam.hkl).density;
  steps.push({
    kind: 'form',
    group: null,
    faces: fam.members,
    title: `组合　${fam.count} 个面合成一个晶形`,
    text: `把 ${fam.count} 个面各自沿自己的法线推到离原点 d = ${dTxt} 的位置（就是每组那对平行面的外侧两张），`
      + `它们正好围成一个封闭的${faceName(form)}：${form.faceCount} 个面、${form.vertCount} 个顶点、${form.edgeCount} 条棱，${shape}。`
      + `${is110
        ? `看点有三个：① 12 张面正好立在参考立方体的 12 条棱上，等于在立方体的 6 个面上各扣一个四方棱锥；`
          + `② 12 个法线端点是截角八面体的 12 个顶点，同一组的两个面在球面上是关于原点对着的一对；③ `
        : `看点：每张面的法线就是同方向的晶向 ⟨${fam.hkl.join('')}⟩，${fam.count} 个端点摊成一个球带；`}
        ${nb}，${zones.length} 条晶带轴（≥3 面共带：${zones.slice(0, 3).map((z) => `${z.label} 带 ${z.members.length} 面`).join('、')}${zones.length > 3 ? '…' : ''}）。`
      + `体心立方里 ${fam.label} 的面密度 ${here.toFixed(4)}/a²，`
      + `${is110 ? `是 BCC 三种低指数面里最密的一个（{100} ${planeMesh('bcc', [1, 0, 0]).density.toFixed(4)}、{111} ${planeMesh('bcc', [1, 1, 1]).density.toFixed(4)}），所以 BCC 的晶形和第一布里渊区都是这个菱形十二面体。`
        : `对照 {100} ${planeMesh('bcc', [1, 0, 0]).density.toFixed(4)}、{110} ${planeMesh('bcc', [1, 1, 0]).density.toFixed(4)}、{111} ${planeMesh('bcc', [1, 1, 1]).density.toFixed(4)}——BCC 最密的是 {110}。`}`,
    layers: FORM,
    set: { group: fam.pairs },
  });
  return steps;
}

/** 邻面关系：从晶形的共棱关系现算，{110} 得到 60°/120°，{111} 得到 70.53°/109.47° */
function neighbourTxt(form) {
  const a = form.faces[0];
  const nb = form.faces.filter((g, j) => j !== 0 && g.edges.some((e) => a.edges.includes(e)));
  if (!nb.length) return '这个晶形里没有共棱的邻面';
  const ang = angleBetween(a.member, nb[0].member);
  return `每个面有 ${nb.length} 个邻面，相邻两面法线夹角 ${ang.toFixed(2)}°（二面角 ${(180 - ang).toFixed(2)}°）`;
}

function faceName(form) {
  if (form.faceCount === 12 && form.faces.every((f) => f.poly.length === 4)) return '菱形十二面体';
  if (form.faceCount === 6) return '立方体';
  if (form.faceCount === 8) return '八面体';
  return `${form.faceCount} 面体`;
}

/** 面方程左边：h x + k y + l z = ±1，负系数写成减号，首项不再带前导加号 */
export function eqnOf(m) {
  const term = (c, v) => (c === 0 ? '' : ` ${c < 0 ? '−' : '+'} ${Math.abs(c) === 1 ? v : Math.abs(c) + v}`);
  const lhs = (term(m.hkl[0], 'x') + term(m.hkl[1], 'y') + term(m.hkl[2], 'z'))
    .replace(/^ \+ /, '').replace(/^ − /, '−');
  return `${lhs || '0'} = ${m.t > 0 ? '1' : '−1'}`;
}

/** 截距：面 hx+ky+lz=1 与三根晶轴的交点；与轴平行的那个写 ∞，负截距直接带负号 */
export function interceptsOf(m) {
  const one = (c) => (c === 0 ? '∞' : c === 1 ? 'a' : c === -1 ? '−a' : `${(1 / c).toFixed(3)}a`);
  return `(${m.hkl.map(one).join(', ')})`;
}

function diagRatio(poly) {
  if (poly.length !== 4) return null;
  const d = (a, b) => Math.hypot(...a.map((x, i) => x - b[i]));
  return Math.max(d(poly[0], poly[2]), d(poly[1], poly[3])) / Math.min(d(poly[0], poly[2]), d(poly[1], poly[3]));
}

function anglesOf(poly) {
  const L3 = (a) => Math.hypot(...a);
  return poly.map((p, i) => {
    const a = poly[(i + poly.length - 1) % poly.length].map((x, j) => x - p[j]);
    const b = poly[(i + 1) % poly.length].map((x, j) => x - p[j]);
    const c = (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) / (L3(a) * L3(b));
    return Math.acos(Math.max(-1, Math.min(1, c))) * 180 / Math.PI;
  });
}
const angTxt = (poly) => {
  const a = anglesOf(poly).sort((x, y) => x - y);
  return `${a[0].toFixed(2)}°/${a[a.length - 1].toFixed(2)}°`;
};

/** 这一组实际画了什么：用来抓「换了组但画面没变」的死步骤 */
export function renderedOf(step) {
  const set = new Set();
  for (const k of Object.keys(step.layers)) if (step.layers[k]) set.add(k);
  set.add(`faces:${step.faces.map((m) => m.label).join(',')}`);
  return set;
}

export function stepDelta(a, b) {
  const x = renderedOf(a);
  const y = renderedOf(b);
  let n = 0;
  for (const t of x) if (!y.has(t)) n++;
  for (const t of y) if (!x.has(t)) n++;
  return n;
}
