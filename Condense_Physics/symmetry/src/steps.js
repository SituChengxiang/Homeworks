// 分步演示脚本：两块内容各一套。
//   bar —— 反轴 n̄（旋转 + 反演），一路算到老师留的那个问号：6̄ = ?
//   td  —— 正四面体的 24 个对称操作，数出国际记号 4̄3m
// 每步只是「图层开关 + 讲解文案 + 要把状态调到哪」，真正画什么是 src/main.js 的 rebuild()。

import { over, barGroup, tdClasses } from './group.js';

const BAR = over('1');

export const BAR_LAYERS = {
  axis: false,      // 反轴本身（竖直轴 + n̄ 记号）
  motif: false,     // 原始基元：一个右手系的彩色三面角 + 顶点 P
  orbit: false,     // 整个群作用出来的等效点系（含编号）
  current: false,   // 只画「施加了 k 次」的那一个，配滑杆一步一步走
  ghost: false,     // 已经走过的历史位置（半透明）
  pairs: false,     // 等效点之间的两两关系：3̄ 给出关于 i 的对，6̄ 给出关于 m 的对
  mirror: false,     // 水平镜面 m⊥（只在群里真的有它时画）
  center: false,    // 对称中心 i（同上）
  numbers: false,   // 编号 1…k
  caption: false,   // 画面上的结论文字
};

export const TD_LAYERS = {
  cube: false,      // 参考立方框：4 个顶点是它的隔角顶点
  faces: false,     // 四个面分别着色
  edges: false,     // 六条棱
  verts: false,     // 四个顶点球
  labels: false,    // 顶点号 1…4
  triad: false,     // 体心的 RGB 三面角（手性指示器）
  ghost: false,     // 变形前的原位置（线框）
  axis4: false,     // 3 条 4̄ 轴（⟨100⟩）
  axis3: false,     // 4 条 3 次轴（体对角线）
  mirrors: false,   // 6 个 σd 镜面（{110}）
  caption: false,
};

/** 这一步实际会画出什么：用来抓「按了下一步但画面没变」的死步骤 */
export function renderedOf(layers) {
  const set = new Set();
  for (const k of Object.keys(layers)) if (layers[k]) set.add(k);
  return set;
}

export function stepDelta(a, b) {
  const x = renderedOf(a.layers);
  const y = renderedOf(b.layers);
  let n = 0;
  for (const t of x) if (!y.has(t)) n++;
  for (const t of y) if (!x.has(t)) n++;
  return n;
}

const L = (keys) => Object.fromEntries(keys.map((k) => [k, true]));

let barCache = null;

/** 反轴那一段：从定义走到 6̄ = 3/m */
export function barSteps() {
  if (barCache) return barCache;
  const g3 = barGroup(3);
  const g4 = barGroup(4);
  const g6 = barGroup(6);
  barCache = [
    {
      title: '反轴 n̄ 的定义：转 360/n，再反演',
      text: '在轴外放一个一般位置的点 P，再给它配一个红→绿→蓝按右手系排列的小三面角。之所以要带三面角，是因为反轴是「旋转 + 反演」这种非真操作，图形做完整齐以后会换成左手系——只有带手性的标架才看得出来它被反演过。反演 = 把每个坐标取负，等于关于原点把整个图形翻个面。',
      layers: L(['axis', 'motif']),
      set: { n: 4, k: 0 },
    },
    {
      title: `${over('1')} = i：只有对称中心`,
      text: '转 360° 等于没转，所以 1̄ 就只剩下反演：P 和 (−x, −y, −z) 成一对，两点连线的中点就是对称中心。群 {E, i} 只有 2 个操作。',
      layers: L(['axis', 'motif', 'orbit', 'center', 'numbers', 'pairs']),
      set: { n: 1, k: 0 },
    },
    {
      title: `${over('2')} = m：反轴里的 2 次就是镜面`,
      text: '绕 z 转 180° 把 (x, y, z) 变成 (−x, −y, z)，再反演一次就变回 (x, y, −z)——坐标只剩 z 反号，这就是对垂直于轴的平面的反映。所以 2̄ 不是新东西，它等价于镜面 m；算出来的矩阵和反映矩阵完全相同。',
      layers: L(['axis', 'motif', 'orbit', 'mirror', 'numbers', 'pairs']),
      set: { n: 2, k: 0 },
    },
    {
      title: `${over('3')} = 3 + i：老师说的那个 3̄`,
      text: `转 120° 再反演，连着做一共得到 ${g3.props.order} 个等效点。把这 ${g3.props.order} 个点两两连线：每对都关于原点对称（画面上是穿过中心的那些虚线），说明对称中心 i 本来就在这个群里；而群里的真旋转只有 E 和两个 120°，即一根 3 次轴。所以 3̄ 这个群 = 3 次轴 ⊗ 对称中心，写作 3̄ = 3 + i，也等于 6 次的转动反映轴 S6。`,
      layers: L(['axis', 'motif', 'orbit', 'center', 'pairs', 'numbers']),
      set: { n: 3, k: 0 },
    },
    {
      title: `${over('4')}：反轴里唯一真正新的一个`,
      text: `4̄ 是转 90° 再反演，一共只给 ${g4.props.order} 个等效点：奇数次幂是反演型的，偶数次幂回到真旋转，而这里的真旋转只有 180°（一根 2 次轴）。群里没有 i，也没有任何镜面——所以 4̄ 不能拆成「已经用过的元素的组合」，它是独立的对称元素。金刚石、闪锌矿的对称性里它就是那个 4̄。`,
      layers: L(['axis', 'motif', 'orbit', 'center', 'mirror', 'numbers']),
      set: { n: 4, k: 0 },
    },
    {
      title: `${over('6')} = ?：答案是没有 6`,
      text: `转 60° 再反演，得到 ${g6.props.order} 个等效点。检查这个群：真旋转只有 E、120°、240°，即一根 3 次轴；6 个点分成 3 对，每对关于水平面 (x, y, −z) 镜像对称，说明含一根垂直于轴的镜面 m；而没有任何一对关于原点对称，即不含 i。结论：6̄ = 3/m（记作 C3h），它里面根本没有 6 次操作——这是最容易画错、也最常考的一条。`,
      layers: L(['axis', 'motif', 'orbit', 'mirror', 'pairs', 'numbers', 'caption']),
      set: { n: 6, k: 0 },
    },
    {
      title: '一步一步施加：看它什么时候回到原位',
      text: '把等效点系换成动画：拖下面「施加次数 k」的滑杆或用 ←/→，每加一次就是再做一遍 6̄。做 6 次正好回到 P——群的阶是 6。同一个滑杆换到 4̄ 只要 4 步就回去，2̄ 和 1̄ 都只要 2 步。',
      layers: L(['axis', 'motif', 'current', 'ghost', 'mirror', 'numbers']),
      set: { n: 6, k: 1 },
    },
    {
      title: '晶体学里为什么只有 1̄ 2̄ 3̄ 4̄ 6̄',
      text: '反轴的前提是那根旋转轴能用，而周期格子只允许 1、2、3、4、6 次轴（5 次和 7 次以上填不满空间），所以反轴也只有这五种。其中 2̄ = m、6̄ = 3/m 都能由已经认识的元素表达，真正独立的对称元素是 1̄(=i)、m、2、3、4、6、3̄、4̄、6̄，把它们按相容的方式搭配就得到 32 个晶体学点群。右侧对照表是算出来的，不是背出来的。',
      layers: L(['axis', 'motif', 'orbit', 'mirror', 'center', 'numbers']),
      set: { n: 6, k: 0 },
    },
  ];
  return barCache;
}

let tdCache = null;

/** Td 那一段：四面体的 24 个操作，数出 4̄3m */
export function tdSteps() {
  if (tdCache) return tdCache;
  const cls = tdClasses();
  const size = Object.fromEntries(cls.map((c) => [c.cls, c.items.length]));
  tdCache = [
    {
      title: '先把正四面体放进立方体里',
      text: '取立方体 8 个角上互不相邻的 4 个（坐标乘积都等于 +1）当顶点，棱长是立方体一个面的对角线。这不是为了好看：这样摆，立方体的 3 条棱向、4 条体对角线、6 个对角面就直接是四面体的对称元素，金刚石与闪锌矿里每个原子的 4 个最近邻也正是这套隔角顶点。',
      layers: L(['cube', 'faces', 'edges', 'verts', 'labels']),
      set: { op: null },
    },
    {
      title: '真旋转只有 12 个：E + 8C3 + 3C2',
      text: `4 条体对角线，每条能转 120° 和 240° → ${size.C3} 个 C3；3 条棱向 ⟨100⟩，每条转 180° → ${size.C2} 个 C2；加上恒等 E。合计 1 + 8 + 3 = 12，这就是纯旋转子群 T（≅ A4）。挑左边任意一个操作按下去，四面体转完还是原来那个。`,
      layers: L(['cube', 'faces', 'edges', 'verts', 'labels', 'axis3', 'triad']),
      set: { op: 'C3' },
    },
    {
      title: '6 个 σd 镜面：{110} 对角面',
      text: `每个镜面含着四面体的一条棱，并把对棱的两个端点互换，这样的面有 ${size['σd']} 个（立方体的 6 个对角面）。它们都是非真操作：做完整齐以后体心那个右手三面角变成左手三面角。`,
      layers: L(['cube', 'faces', 'edges', 'verts', 'labels', 'mirrors', 'ghost', 'triad']),
      set: { op: 'σd' },
    },
    {
      title: `6 个 S4 = 3 条 ${BAR} 轴上的操作`,
      text: `沿立方体三条棱向各转 90° 再反演，四面体照样回到自己身上：${size.S4} 个这样的操作（转 ±90° 两种）加上它们的平方（就是那 3 个 C2），一共 9 个操作来自 3 条 ${BAR} 轴。注意单独转 90° 是不行的——必须有反演陪着，所以这 6 个操作既不是真旋转也不是镜面。`,
      layers: L(['cube', 'faces', 'edges', 'verts', 'ghost', 'axis4', 'triad']),
      set: { op: 'S4' },
    },
    {
      title: '1 + 8 + 3 + 6 + 6 = 24 → 国际记号 4̄3m',
      text: '把三样东西并起来看：3 条 4̄ 轴沿 ⟨100⟩、4 根 3 次轴沿体对角线、6 个镜面是 {110}，操作个数 1 + 8 + 3 + 6 + 6 = 24。按「棱向 / 体对角线 / 对角面」的顺序把对称元素写进三个位置，就是 4̄3m —— 正四面体（也是金刚石、闪锌矿结构）的点群 Td。',
      layers: L(['cube', 'faces', 'edges', 'verts', 'axis4', 'axis3', 'mirrors', 'caption']),
      set: { op: null },
    },
    {
      title: '每个操作 = 4 个顶点的一个置换：Td ≅ S4',
      text: '给 4 个顶点编号 1、2、3、4，24 个操作做完，正好给出 4 个元素的全部 4! = 24 个置换，一个不多一个不少。所以 Td 同构于对称群 S4；其中 12 个真旋转是偶置换，构成 A4 = T 群。左侧点任何一个操作，右边面板会显示它的置换和矩阵。',
      layers: L(['cube', 'faces', 'edges', 'verts', 'labels', 'ghost']),
      set: { op: 0 },
    },
    {
      title: '手性：为什么是 24 而不是 12',
      text: '能保住这套四面体的操作里，一半是真旋转（det = +1），一半带反映或反演（det = −1）。看体心那根 RGB 三面角：做一次非真操作它就从右手系翻成左手系。如果给四个面各贴一个同方向的旋涡（让四面体本身带上手性），12 个非真操作立刻全部作废，只剩 T 群那 12 个。金刚石里 4 个最近邻是同种原子、面上没有花样，所以 24 个全都成立。',
      layers: L(['cube', 'faces', 'edges', 'verts', 'labels', 'triad', 'ghost']),
      set: { op: 'σd' },
    },
    {
      title: '顺带看清：48 个立方体操作里只有 24 个保四面体',
      text: '立方体自己（Oh 群）有 48 个对称操作。把 48 个逐个作用到这套隔角顶点上：正好 24 个把它映回自己（就是 Td），另外 24 个把它换成互补的第二套四面体（坐标乘积 = −1 的那 4 个角）。金刚石结构的两套 FCC、沿体对角线错开 1/4，正是这一对互补四面体——和 diamond-zincblende 那一页是同一件事。',
      layers: L(['cube', 'faces', 'edges', 'verts', 'labels', 'ghost', 'axis4', 'axis3', 'mirrors']),
      set: { op: null },
    },
  ];
  return tdCache;
}

export function stepsOf(mode) {
  return mode === 'td' ? tdSteps() : barSteps();
}

export const LAYERS_OF = { bar: BAR_LAYERS, td: TD_LAYERS };
