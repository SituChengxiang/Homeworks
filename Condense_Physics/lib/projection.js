// 数学 → 画法的适配层：把三维等效点系换算成 Canvas2D 符号层要的那份数据。
// 单独放一个文件有两个理由：页面和预览页共用同一套换算；这些统计量（落成几处、
// 几对穿心、几对叠影）本身就是要被 test 守住的教学结论。
import { planeBasis, project, applyM } from './matrix.js';

/**
 * @param orbit  barOrbit() 的结果：{ pts:[{M,p,det,k}], pairs:[[i,j,kind]], props }
 * @param label  画在图上的标题（如「6̄ 沿轴投影」）
 * @param along  投影方向（沿哪根轴看下去），默认 z
 */
/**
 * view = 'top' 教材俯视图：(u,v) 就是垂直于轴的那个平面里的坐标，
 *        高度用半填圆点表示 —— 和书上的等效点系图一致，代价是上下成对的两点会重合。
 *      'fan'  上下圈展开图：方位角原样保留，高度换成半径（z 越大越靠外圈）。
 *        一张图里 6 个点就是 6 个点，「上下同方位角」变成「在同一条径向线上」，
 *        「穿过中心成对」变成「内外圈上正对着的一条直线」，不需要先学会半填约定。
 */
export function orbitModel(orbit, label, { along = [0, 0, 1], view = 'top' } = {}) {
  const basis = planeBasis(along);
  const maxAbsZ = Math.max(1e-9, ...orbit.pts.map((x) => Math.abs(project(x.p, basis).z)));
  const points = orbit.pts.map((x, i) => {
    const q = project(x.p, basis);
    const d = project(applyM(x.M, [1, 0, 0]), basis);   // 该点三面角 x 轴的投影方向
    let u = q.u;
    let v = q.v;
    if (view === 'fan') {
      const az = Math.atan2(q.v, q.u);
      const r = 0.72 + 0.42 * (q.z / maxAbsZ);          // 高度 → 半径：上圈在外、下圈在内
      u = r * Math.cos(az);
      v = r * Math.sin(az);
    }
    return {
      id: i, u, v, z: q.z,
      handed: x.det, tag: x.k, dir: Math.atan2(d.v, d.u), selected: false,
    };
  });

  const spots = new Set(points.map((p) => `${p.u.toFixed(6)},${p.v.toFixed(6)}`)).size;
  const up = points.filter((p) => p.z >= 0);
  const dn = points.filter((p) => p.z < 0);
  const spokes = view === 'fan' ? up.map((p) => Math.atan2(p.v, p.u)) : [];
  // 「下圈落在上圈的径向线上」= 上下同方位角 = 存在垂直于轴的水平镜面。
  // 这件事必须是一个数出来的量，而不是「看起来挺对齐」。
  const az = (p) => Math.atan2(p.v, p.u);
  const sameAz = (a, b) => Math.abs(Math.atan2(Math.sin(a - b), Math.cos(a - b))) < 1e-6;
  const aligned = dn.filter((q) => up.some((r) => sameAz(az(q), az(r)))).length;
  const iPairs = orbit.pairs.filter((p) => p[2] === 'i').length;
  const mPairs = orbit.pairs.filter((p) => p[2] === 'm').length;
  const sig = mPairs ? 'stack' : iPairs ? 'through' : 'none';
  if (view === 'fan' && dn.length) {
    const expect = sig === 'stack' ? dn.length : 0;
    if (aligned !== expect) {
      throw new Error(`${label}：展开图数出 ${aligned} 个下圈点对齐，按 6̄=${sig} 应为 ${expect} 个`);
    }
  }

  const tail = sig === 'stack'
    ? `每处上下叠一对 → 群里有 m⊥`
    : sig === 'through'
      ? `${iPairs} 对连线穿过中心 → 群里有 i`
      : `既不叠成影、也不穿心 → 既没有 m⊥ 也没有 i`;

  return {
    points,
    pairs: orbit.pairs.map(([a, b, kind]) => ({ a, b, kind })),
    center: orbit.props.hasI ? 'i' : orbit.props.hasMirrorPerp ? 'm' : null,
    ring: orbit.props.hasMirrorPerp ? 'm⊥' : null,
    view, flat: view === 'fan', spokes,
    heading: label,
    sub: view === 'fan'
      ? `上圈 ${up.length} 个、下圈 ${dn.length} 个：${
          aligned ? `下圈 ${aligned} 个全落在上圈的径向线上 → 上下互为镜像`
                  : `下圈没有一个落在上圈的径向线上 → 没有水平镜面`}`
      : `${points.length} 个点 → 落成 ${spots} 处：${tail}`,
    stats: {
      points: points.length, spots, iPairs, mPairs, signature: sig,
      up: up.length, down: dn.length, aligned,
    },
  };
}

/** 把选中的点与它的配对点一起点亮 */
export function selectPoint(model, id) {
  const clear = (m) => { m.points.forEach((p) => { p.selected = false; }); return m; };
  if (id === null || id === undefined) return clear(model);
  clear(model);
  const pair = (model.pairs || []).find((pr) => pr.a === id || pr.b === id);
  const want = new Set([id]);
  if (pair) { want.add(pair.a); want.add(pair.b); }
  model.points.forEach((p) => { if (want.has(p.id)) p.selected = true; });
  return model;
}
