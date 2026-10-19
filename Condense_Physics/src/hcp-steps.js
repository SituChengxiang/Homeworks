// 六方密堆积（HCP）的分步演示脚本：怎么一层层堆出来、堆积率怎么算。
// 每步只是一组图层开关 + 讲解文案，渲染在 src/hcp.js 的 rebuild() 里。
import { IDEAL_CA, hcpTetra } from './geometry.js';

export const HCP_LAYERS = {
  layerA: false, layerB: false, layerA2: false,   // 三层原子
  hollows: false,                                  // 底面三角形空隙
  prism: false,                                    // 六方棱柱（晶胞）棱框
  share: false,                                    // 份额裁剪（1/6 · 1/2 · 1）
  labels: false,                                   // 份额标签 / 层名标签
  bonds: false,                                    // 最近邻键
  nn: false,                                       // 中心原子的高亮近邻
  tetra: false,                                    // 正四面体 + c/a 推导线
  cloud: false,                                    // 扩展点阵
};

/**
 * 这一步实际会画出什么。share 只是把同一批原子换成裁剪后的样子，
 * 所以它和 layerA/layerB/layerA2 归一成同一组原子 + 一个 cut 标记：
 * 这样「按了下一步但画面其实没变」的死步骤会被 hcpStepDelta 抓出来。
 */
export function hcpRenderedOf(layers) {
  const set = new Set();
  for (const key of ['hollows', 'prism', 'bonds', 'nn', 'tetra', 'cloud']) if (layers[key]) set.add(key);
  if (layers.share || layers.layerA) set.add('atoms:A');
  if (layers.share || layers.layerB) set.add('atoms:B');
  if (layers.share || layers.layerA2) set.add('atoms:A2');
  if (layers.share) set.add('cut');
  if (layers.labels || layers.share) set.add('lab');
  return set;
}

/** 相邻两步之间真正变化的项数 */
export function hcpStepDelta(a, b) {
  const x = hcpRenderedOf(a.layers);
  const y = hcpRenderedOf(b.layers);
  let n = 0;
  for (const t of x) if (!y.has(t)) n++;
  for (const t of y) if (!x.has(t)) n++;
  return n;
}

let cache = null;

export function hcpSteps() {
  if (cache) return cache;
  const tet = hcpTetra();

  cache = [
    {
      title: '第一层：密置层 A',
      text: '把刚球紧挨着摆在桌面上，每个球周围正好贴 6 个——这就是密置层。同层最近邻距离 a 等于两个半径，所以 a = 2R。',
      layers: { layerA: true, labels: true },
    },
    {
      title: '第二层只能填一半的空隙',
      text: '第一层上有两种三角形空隙（尖朝上、尖朝下），数量各占一半。第二层的球只能坐在其中一种上面——两种都坐是做不到的，位置互相冲突。',
      layers: { layerA: true, hollows: true },
    },
    {
      title: '放上第二层 B',
      text: '被选中的那批空隙上落下第二层的球，本胞里正好落下 3 个（在棱柱内部，谁也不共享）。把它们和底下连起来看，就是一个个正四面体。',
      layers: { layerA: true, layerB: true, bonds: true },
    },
    {
      title: '第三层回到 A：ABAB…',
      text: '第三层如果落回与第一层完全对齐的位置，堆叠序列就是 ABABAB……——这就是六方密堆积。若第三层落在剩下的那批空隙（C），就得到 ABCABC……即面心立方 FCC；两者堆积率一样，只是层序不同。',
      layers: { layerA: true, layerB: true, layerA2: true, labels: true },
    },
    {
      title: '围出六方晶胞',
      text: '底面是边长 a 的正六边形（6 个顶角 + 1 个中心），高为 c。这个棱柱是 3 个初基胞拼起来的，所以含 6 个原子、而不是 1 个。',
      layers: { layerA: true, layerB: true, layerA2: true, prism: true },
    },
    {
      title: '数原子：12×1/6 + 2×1/2 + 3 = 6',
      text: '顶角同时被同层 3 个棱柱、上下 2 层共用 → 每家只有 1/6；上下底面中心被 2 个棱柱共用 → 1/2；中间层 3 个整颗在本胞内。12×(1/6) + 2×(1/2) + 3×1 = 6 个原子。',
      layers: { layerA: true, layerB: true, layerA2: true, prism: true, share: true },
    },
    {
      title: `轴比 c/a = √(8/3) = ${IDEAL_CA.toFixed(4)}`,
      text: `同层 3 个互相接触的球心 + 层间 1 个球心构成正四面体，棱长 a。底面三角形中心到顶点是 a/√3，所以四面体高（= 层间距）= √(2/3)·a ≈ 0.816a（画面里 a = 2 时高 ${tet.height.toFixed(2)}）。一个周期 c 装着两层间距：c = 2√(2/3)a → c/a = √(8/3) ≈ 1.633。`,
      layers: { layerA: true, layerB: true, layerA2: true, prism: true, tetra: true },
    },
    {
      title: '配位数 12：数一下谁在碰它',
      text: '挑一个球：同层 6 个、上面 3 个、下面 3 个，共 12 个最近邻——和 FCC 一样多。密置层的密度只由“每层密置 + 层间坐空隙”决定，与堆叠顺序无关。',
      layers: { layerA: true, layerB: true, layerA2: true, cloud: true, nn: true },
    },
    {
      title: '把相切的键连起来',
      text: '同层 6 条键围成正六边形，层间 3 + 3 条斜键夹着上下两层——这就是四面体骨架。键长都等于 a。',
      layers: { layerA: true, layerB: true, layerA2: true, cloud: true, bonds: true },
    },
    {
      title: '堆积率 = 6 个球体积 / 棱柱体积',
      text: '棱柱体积 = 底面积 × 高 = (3√3/2)a²·c。代入 a = 2R、c = √(8/3)a，六颗球的体积加起来正好是 πa³，于是 APF = πa³ / (3√2·a³) = π/(3√2) = √2π/6 ≈ 74.05%——与 FCC 是同一个数。右侧面板逐行代入。',
      layers: { layerA: true, layerB: true, layerA2: true, prism: true },
    },
    {
      title: '真实金属为什么不到 74%',
      text: '74.05% 是理想轴比的上限。c/a 偏离时层内与层间不能同时相切，刚球半径由更近的那组决定 R = min(a, √(a²/3+c²/4))/2：偏长（Zn 1.856、Cd 1.883）层间留缝，偏扁（Be 1.568、Ti 1.588）层间先接触，堆积率都掉到 74% 以下。拖 c/a 滑杆或换材料即可看到。',
      layers: { layerA: true, layerB: true, layerA2: true, prism: true, share: true },
    },
  ];
  return cache;
}
