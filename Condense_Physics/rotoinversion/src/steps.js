// 作业 A 的分步脚本：每一步 = 一组图层 + 要把状态调到哪 + 字幕怎么说。
// 文案里凡是「几个点、几处、几对」的数字都不写死：那些由 lib/projection.js 现算，
// 显示在 2D 图的副标题上。这里只写解释，不写数据。
export const LAYERS = {
  axis: false,      // 反轴本身（竖直轴 + n̄ 记号）
  trace: false,     // 定义用的两条路径：转过的弧 + 穿过原点的反演线
  motif: false,     // 原始基元：一个点 + 挂在它上面的右手系三面角
  orbit: false,     // 整个群作用出来的等效点系
  numbers: false,   // 编号
  pairs: false,     // 等效点的两两关系（穿心 / 上下相对）
  mirror: false,    // 水平镜面 m⊥（只在群里真的有它时画）
  center: false,    // 对称中心 i（同上）
  live: false,      // 一步一步施加：当前那个基元由姿态矩阵带动
  ghost: false,     // 已经走过的虚影
};

const L = (keys) => Object.fromEntries(keys.map((k) => [k, true]));

export const STEPS = [
  {
    key: '定义',
    title: '反轴 n̄：绕轴转 360/n，再过原点反演',
    punch: '转一下，再穿原点翻面',
    note: '两步合起来做一次，图形和原来重合。反演 = 每个坐标取负，等于把整个图形穿过原点翻到对面。',
    layers: L(['axis', 'trace', 'motif']),
    set: { n: 3, k: 0 },
  },
  {
    key: '1̄',
    title: '1̄ = i：只剩一个对称中心',
    punch: '= i',
    note: '转 360° 是白转，所以 1̄ 就是反演本身。两点连线穿过中心 —— 这是「穿心」的最简样子。',
    layers: L(['axis', 'motif', 'orbit', 'numbers', 'pairs', 'center']),
    set: { n: 1, k: 0 },
  },
  {
    key: '2̄',
    title: '2̄ = m：反轴里的 2 次就是镜面',
    punch: '= m，不是新元素',
    note: '转 180° 把 x、y 变号，反演再把 z 变号，合起来只有 z 变号 —— 对垂直于轴的平面反映。所以晶体学表里从不写 2̄，写 m。',
    layers: L(['axis', 'motif', 'orbit', 'numbers', 'pairs', 'mirror']),
    set: { n: 2, k: 0 },
  },
  {
    key: '3̄',
    title: '3̄ = 3 + i：老师说的那个 3̄',
    punch: '= 3 + i（即 S₆）',
    note: '6 个等效点两两穿过中心成对，说明群里有 i；而真旋转只有 E 和两个 120°，即一根 3 次轴。合起来就是 3 ⊗ i，也等于 S₆。',
    layers: L(['axis', 'motif', 'orbit', 'numbers', 'pairs', 'center']),
    set: { n: 3, k: 0 },
  },
  {
    key: '4̄',
    title: '4̄：反轴里唯一真正新的一个',
    punch: '独立：既无 i 也无 m',
    note: '只有 4 个等效点，真旋转只剩 2 次，既凑不出 i 也凑不出镜面 —— 不能由已认识的元素表达，所以必须单独给个符号。金刚石、闪锌矿里就是它。',
    layers: L(['axis', 'motif', 'orbit', 'numbers', 'pairs', 'mirror', 'center']),
    set: { n: 4, k: 0 },
  },
  {
    key: '6̄ = ?',
    title: '6̄ = 3/m：答案是没有 6',
    punch: '= 3/m，里面没有 6',
    note: '看右边那张展开图：上圈 3 个、下圈 3 个，下圈每一个都落在上圈的径向线上 —— 上下互为镜像，所以有 m⊥；而没有一个点穿过中心和别人配对，所以没有 i。真旋转只有 3 次。',
    layers: L(['axis', 'motif', 'orbit', 'numbers', 'pairs', 'mirror']),
    set: { n: 6, k: 0 },
  },
  {
    key: '施加',
    title: '一步一步施加：做 6 次才回到原位',
    punch: '群的阶 = 6',
    note: '按 L 打开图层，拖「施加次数 k」：每加一次就是再做一遍 6̄，第 6 次正好回到 P。这个 6 就是群的阶。',
    layers: L(['axis', 'motif', 'live', 'ghost', 'numbers', 'mirror']),
    set: { n: 6, k: 1 },
  },
  {
    key: '对照',
    title: '为什么只有 1̄ 2̄ 3̄ 4̄ 6̄',
    punch: '五种，且只有五种',
    note: '周期格子只允许 1、2、3、4、6 次轴，反轴也就只有这五种；其中 2̄ = m、6̄ = 3/m 都能由老元素表达。按相容的方式搭配，得到 32 个晶体学点群 —— 数据抽屉里是这一页全部的对照表。',
    layers: L(['axis', 'motif', 'orbit', 'numbers', 'pairs', 'mirror', 'center']),
    set: { n: 6, k: 0 },
  },
];

/** 数据抽屉里那张总表：数字全部来自 barGroup，不手写 */
export const SUMMARY_ORDER = [1, 2, 3, 4, 6];
