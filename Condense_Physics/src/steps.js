// 分步演示的脚本数据：把「画晶胞的过程」拆成若干步，
// 每步只是一组图层开关 + 讲解文案，具体渲染在 main.js 的 rebuild() 里。
// 这里是纯数据（不碰 DOM），test/verify.js 会检查步与步之间的画面差异。
import { atomsFor, atomsPerCell } from './geometry.js';

/** 所有可用图层；rebuild() 逐个判断 */
export const BASE_LAYERS = {
  center: false, axes: false, corners: false, cell: false,
  atoms: false, share: false, bonds: false, nn: false, planes: false,
  ws: false, tiling: false, ext: false, labels: false,
};

/** 图层名 + 可选的数值参数（radius 覆盖原子半径比例） */
export const LAYER_KEYS = Object.keys({ ...BASE_LAYERS, radius: 0.55 });

const CN_OF = { SC: 6, BCC: 8, FCC: 12 };

/**
 * 这一步「实际上会画出什么」，用于比较相邻两步的画面差异：
 * atoms 与 share 都归一成同一组原子（只是切不切），SC 的 corner/all 因此视为同一画面，
 * 避免出现「按了下一步却什么都没变」的死步骤。
 */
export function renderedOf(type, layers) {
  const kinds = new Set(atomsFor(type).map((a) => a.kind));
  const expand = (which) => (which === 'all' ? [...kinds] : kinds.has(which) ? [which] : []);
  const set = new Set();
  for (const key of ['center', 'axes', 'corners', 'cell', 'bonds', 'nn', 'planes', 'ws', 'tiling', 'ext', 'labels'])
    if (layers[key]) set.add(key);
  const atoms = new Set([...expand(layers.atoms), ...expand(layers.share)]);
  for (const k of atoms) set.add(`atom:${k}`);
  if (layers.share) set.add('cut');
  if (layers.radius !== undefined) set.add(`r:${layers.radius}`);
  return set;
}

/** 两步之间画面变化的项数 */
export function stepDelta(type, a, b) {
  const x = renderedOf(type, a.layers);
  const y = renderedOf(type, b.layers);
  let n = 0;
  for (const t of x) if (!y.has(t)) n++;
  for (const t of y) if (!x.has(t)) n++;
  return n;
}

/** 三种结构的逐步脚本 */
export function stepsFor(type) {
  const meta = {
    SC:  { inner: null,   nd: 'a',        rr: 'a/2',        pack: '52.36%', wsName: '立方体（6 个正方形面）' },
    BCC: { inner: 'body', nd: '(√3/2)·a', rr: '(√3/4)·a',   pack: '68.02%', wsName: '截角八面体（6 正方形 + 8 正六边形）' },
    FCC: { inner: 'face', nd: '(√2/2)·a', rr: '(√2/4)·a',   pack: '74.05%', wsName: '菱形十二面体（12 个全等菱形面）' },
  }[type];
  const n = atomsPerCell(type);
  const innerName = meta.inner === 'body' ? '体心' : '面心';

  const steps = [
    {
      title: '任选一个格点',
      text: '晶体的本质是原子的周期性重复排列。把原子抽象成一堆完全等价的几何点，就得到空间点阵（布拉维格子）。背景里的淡色点是它周围的同类格点，演示先只保留原点这一个。',
      layers: { center: true, ext: true },
    },
    {
      title: '取出三条晶轴 a / b / c',
      text: '从这个格点出发，沿三个不共面的最小平移周期各取一个格矢，就是晶轴。立方晶系的三条轴两两垂直、长度都等于晶格常数 a。',
      layers: { center: true, axes: true, ext: true },
    },
    {
      title: '沿晶轴平移，划出晶胞',
      text: '把格点沿 a、b、c 各平移一个周期，得到 8 个顶角格点；它们围出的平行六面体（这里是立方体）就是晶胞——晶体中最小的重复单元。',
      layers: { center: true, axes: true, corners: true, cell: true, ext: true },
    },
    {
      title: '格点 + 基元 = 晶体结构',
      text: '晶格只是抽象的点，要在每个格点上放上真实的原子（基元）才得到结构。' +
        (type === 'SC'
          ? 'SC 的基元只有 1 个原子，所以每个顶角格点上就是一个原子。'
          : `先只摆出顶角上的原子，${innerName}那个下一 step 再补。`),
      layers: { axes: true, corners: true, cell: true, atoms: 'corner', ext: true },
    },
    {
      title: '顶角原子怎么数：每个只算 1/8',
      text: '一个顶角同时被 8 个相邻晶胞共用，落在本胞内的只有它的 1/8。亮色实心是被裁出的份额，半透明虚影是分给邻胞的部分。8 × 1/8 = 1 个原子。',
      layers: { cell: true, share: 'corner', labels: true, ext: true },
    },
  ];

  if (meta.inner) {
    steps.push({
      title: `补上${innerName}原子，数出每胞原子数 n`,
      text: meta.inner === 'body'
        ? '体心位置还有一个原子，它整个落在晶胞内部、不跟任何邻胞分摊，贡献记满 1：顶角 8 × 1/8 = 1，体心 1 × 1 = 1，所以 BCC 每胞 n = 2 个原子。'
        : '6 个面的中心各有一个原子，每个面被相邻 2 个晶胞共用 → 6 × 1/2 = 3；加上顶角的 1 个，FCC 每胞 n = 4 个原子。',
      layers: { cell: true, share: 'all', labels: true },
    });
    steps.push({
      title: '把原子放回完整的样子',
      text: '去掉裁剪，看这一套原子在晶胞里完整的样子——接下来要量的距离、角度都和份额无关，先把形状认全。',
      layers: { cell: true, atoms: 'all' },
    });
  } else {
    steps.push({
      title: 'SC 只有一个原子：n = 1',
      text: 'SC 只有顶角贡献，n = 8 × 1/8 = 1 个原子。也就是说这个晶胞里其实只放着"一个格点 + 一个基元"，它是三种立方结构里最松垮的一种。',
      layers: { cell: true, atoms: 'corner' },
    });
  }

  steps.push({
    title: `最近邻与配位数 CN = ${CN_OF[type]}`,
    text: `把最近邻连起来：SC 沿 <100> 有 6 个，BCC 沿体对角线 <111> 有 8 个，FCC 沿面对角线 <110> 有 12 个。当前 ${type} 的最近邻距离为 ${meta.nd}，配位数 CN = ${CN_OF[type]}。`,
    layers: { cell: true, atoms: 'all', bonds: true },
  });
  steps.push({
    title: '刚球接触，看堆积率',
    text: `把原子当作互相接触的刚性球（半径连续涨到刚球尺寸）：${type} 的半径 r = ${meta.rr}，晶胞体积 a³，堆积率 = n · (4/3)πr³ / a³ ≈ ${meta.pack}。`,
    layers: { cell: true, atoms: 'all', radius: 1 },
  });
  steps.push({
    title: '把原子收小，退回格点的眼睛',
    text: '接下来要换一种取原胞的方法，它只关心格点、不关心原子大小。先把球缩回成小颗粒，背景的点阵重新露出来。',
    layers: { cell: true, center: true, ext: true, atoms: 'all', radius: 0.3 },
  });
  steps.push({
    title: '换一种取法：Wigner–Seitz（先连近邻）',
    text: '惯用晶胞不是唯一选择。以原点格点为中心，向所有最近邻、次近邻格点连线（红=最近邻，紫=次近邻），下一步作这些连线的中垂面。棱框留着当参照。',
    layers: { cell: true, center: true, ext: true, nn: true },
  });
  steps.push({
      title: '画出垂直平分面',
      text: '对每条连线取中点，作与之垂直的平分面（半透明圆片）。每个面只保留靠近中心的一侧——这就是 Wigner–Seitz 构造的关键一步。',
      layers: { cell: true, center: true, nn: true, planes: true },
    });
  steps.push({
    title: '中垂面围出 WS 原胞',
    text: '所有这些半空间的交集，就是离中心格点最近的那块区域：WS 原胞。图中它是实时算出来的——先求三平面交点得到顶点，再按平面回收出各个面（圆片超出胞体的部分说明平面本是无限大的）。',
    layers: { cell: true, center: true, nn: true, planes: true, ws: true },
  });
  steps.push({
    title: 'WS 原胞无缝填满空间',
    text: `在每个与中心胞共面的近邻格点上再放一个 WS 胞，它们面贴面地堆起来、不留空隙。${type} 的 WS 原胞是${meta.wsName}，只含 1 个格点，体积 a³/n = a³/${n}。`,
    layers: { center: true, planes: true, ws: true, tiling: true, ext: true },
  });
  steps.push({
    title: '把惯用晶胞盖回去对比',
    text: `同一套格点、两种取法：把立方棱框盖在 WS 胞外面。惯用晶胞含 n = ${n} 个原子，WS 原胞只含 1 个格点，两者体积之比正好是 ${n}∶1——这就是"a³/n"的来历。`,
    layers: { cell: true, planes: true, ws: true, tiling: true, ext: true },
  });
  steps.push({
    title: '两种取法摆在一起',
    text: '棱框、三条晶轴、完整原子，外面套着刚算出来的 WS 胞和平铺——两套描述叠在同一张图上看。',
    layers: { cell: true, axes: true, atoms: 'all', ws: true, tiling: true, ext: true },
  });
  steps.push({
    title: '收尾：只留两张图',
    text: '关掉平铺和点云，只剩惯用晶胞（含 n 个原子）与 WS 原胞（只含 1 个格点、体积最小）。想单独看哪一个，随手关掉对应图层。',
    layers: { cell: true, axes: true, atoms: 'all', ws: true },
  });
  return steps;
}
