// 纯几何/物理计算（不依赖 DOM，可在 Node 中单独测试）
import * as THREE from 'three';

export const A = 2;        // 显示用晶格常数
export const HALF = A / 2;

export const v = (x, y, z) => new THREE.Vector3(x, y, z);

/** 三种结构的初级格矢 */
export function primitiveVectors(type) {
  if (type === 'SC')  return [v(A, 0, 0), v(0, A, 0), v(0, 0, A)];
  if (type === 'BCC') return [v(-A / 2, A / 2, A / 2), v(A / 2, -A / 2, A / 2), v(A / 2, A / 2, -A / 2)];
  return [v(0, A / 2, A / 2), v(A / 2, 0, A / 2), v(A / 2, A / 2, 0)]; // FCC
}

/** 以原点为参考格点，列出 maxDist 内的所有格点（去重） */
export function latticePoints(type, maxDist) {
  const [p1, p2, p3] = primitiveVectors(type);
  const raw = [];
  for (let i = -4; i <= 4; i++)
    for (let j = -4; j <= 4; j++)
      for (let k = -4; k <= 4; k++) {
        if (i === 0 && j === 0 && k === 0) continue;
        const r = v(0, 0, 0)
          .addScaledVector(p1, i)
          .addScaledVector(p2, j)
          .addScaledVector(p3, k);
        if (r.length() <= maxDist) raw.push(r);
      }
  const uniq = [];
  for (const r of raw)
    if (!uniq.some((q) => q.distanceToSquared(r) < 1e-9)) uniq.push(r);
  return uniq;
}

export function nearestDistance(type) {
  return Math.min(...latticePoints(type, 3 * A).map((p) => p.length()));
}

/* ---------- 由半空间交求凸多面体 ---------- */
export function faceNormal(poly) {
  const n = v(0, 0, 0);
  for (let i = 0; i < poly.length; i++) {
    const a = poly[i], b = poly[(i + 1) % poly.length];
    n.x += (a.y - b.y) * (a.z + b.z);
    n.y += (a.z - b.z) * (a.x + b.x);
    n.z += (a.x - b.x) * (a.y + b.y);
  }
  return n.normalize();
}

/** 三平面交点（克拉默法则）；平行/退化时返回 null */
function intersect3(p1, p2, p3) {
  const a = p1.n, b = p2.n, c = p3.n;
  const det = a.dot(new THREE.Vector3().crossVectors(b, c));
  if (Math.abs(det) < 1e-9) return null;
  const d1 = p1.d, d2 = p2.d, d3 = p3.d;
  const bc = new THREE.Vector3().crossVectors(b, c);
  const ca = new THREE.Vector3().crossVectors(c, a);
  const ab = new THREE.Vector3().crossVectors(a, b);
  return bc.multiplyScalar(d1).add(ca.multiplyScalar(d2)).add(ab.multiplyScalar(d3)).divideScalar(det);
}

/** 把共面点按绕质心的角度排序，得到逆时针面多边形 */
function orderFace(points, normal) {
  const c = v(0, 0, 0);
  points.forEach((p) => c.add(p));
  c.multiplyScalar(1 / points.length);

  const helper = Math.abs(normal.x) > 0.9 ? v(0, 1, 0) : v(1, 0, 0);
  const u = new THREE.Vector3().crossVectors(normal, helper).normalize();
  const w = new THREE.Vector3().crossVectors(normal, u).normalize();

  const sorted = [...points].sort((p, q) => {
    const pa = Math.atan2(w.dot(p.clone().sub(c)), u.dot(p.clone().sub(c)));
    const qa = Math.atan2(w.dot(q.clone().sub(c)), u.dot(q.clone().sub(c)));
    return pa - qa;
  });
  if (faceNormal(sorted).dot(normal) < 0) sorted.reverse();
  return sorted;
}

/**
 * 计算 WS 原胞：对最近邻 + 次近邻作中垂面，
 * 求所有“三面交点”中满足全部半空间约束的点（= 多面体顶点），
 * 再按平面回收各个面。
 */
export function computeWS(type) {
  const dmin = nearestDistance(type);
  const neighbors = latticePoints(type, 1.35 * dmin);
  const planes = neighbors.map((r) => ({ n: r.clone().normalize(), d: r.length() / 2 }));
  const EPS = 1e-6;

  // 顶点 = 三平面交点 ∩ 全部半空间
  const verts = [];
  for (let i = 0; i < planes.length; i++)
    for (let j = i + 1; j < planes.length; j++)
      for (let k = j + 1; k < planes.length; k++) {
        const p = intersect3(planes[i], planes[j], planes[k]);
        if (!p) continue;
        if (!planes.every((q) => q.n.dot(p) - q.d <= EPS)) continue;
        if (!verts.some((q) => q.distanceToSquared(p) < 1e-9)) verts.push(p);
      }

  // 面 = 落在该平面上的所有顶点
  const faces = [];
  const faceNeighbors = []; // 真正贡献了“面”的近邻（= 与中心胞共面的格点）
  planes.forEach((q, idx) => {
    const on = verts.filter((p) => Math.abs(q.n.dot(p) - q.d) < EPS);
    if (on.length < 3) return;
    faces.push(orderFace(on, q.n));
    faceNeighbors.push(neighbors[idx]);
  });

  return { faces, neighbors, faceNeighbors, dmin };
}

/** 去重后的棱（每条棱只保留一次） */
export function uniqueEdges(faces) {
  const map = new Map();
  const key = (p) => `${p.x.toFixed(4)},${p.y.toFixed(4)},${p.z.toFixed(4)}`;
  for (const f of faces)
    for (let i = 0; i < f.length; i++) {
      const a = f[i], b = f[(i + 1) % f.length];
      const ka = key(a), kb = key(b);
      map.set(ka < kb ? ka + '|' + kb : kb + '|' + ka, [a, b]);
    }
  return [...map.values()];
}

export function wsStats(faces) {
  const vset = new Map();
  const key = (p) => `${p.x.toFixed(4)},${p.y.toFixed(4)},${p.z.toFixed(4)}`;
  const eset = new Set();
  for (const f of faces)
    for (let i = 0; i < f.length; i++) {
      const a = f[i], b = f[(i + 1) % f.length];
      vset.set(key(a), a);
      const ka = key(a), kb = key(b);
      eset.add(ka < kb ? ka + '|' + kb : kb + '|' + ka);
    }
  return { V: vset.size, E: eset.size, F: faces.length };
}

/* ---------- 惯用晶胞中的原子 ---------- */
export function atomsFor(type) {
  const atoms = [];
  for (const x of [-HALF, HALF])
    for (const y of [-HALF, HALF])
      for (const z of [-HALF, HALF])
        atoms.push({ pos: v(x, y, z), kind: 'corner' });

  if (type === 'BCC') atoms.push({ pos: v(0, 0, 0), kind: 'body' });
  if (type === 'FCC') {
    [v(HALF, 0, 0), v(-HALF, 0, 0), v(0, HALF, 0), v(0, -HALF, 0), v(0, 0, HALF), v(0, 0, -HALF)]
      .forEach((p) => atoms.push({ pos: p, kind: 'face' }));
  }
  return atoms;
}

/** 刚球接触时的原子半径 */
export function touchRadius(type) {
  if (type === 'SC') return A / 2;
  if (type === 'BCC') return (Math.sqrt(3) / 4) * A;
  return (Math.sqrt(2) / 4) * A;
}

/** 统计惯用晶胞内原子数（按分数贡献） */
export function atomsPerCell(type) {
  let n = 0;
  for (const a of atomsFor(type)) {
    if (a.kind === 'corner') n += 1 / 8;
    else if (a.kind === 'face') n += 1 / 2;
    else n += 1;
  }
  return n;
}
