// 设计系统 · JS 侧的同一份 token
// CSS 变量给 DOM 用，这里给 three.js 的材质色和 Canvas2D 用 —— 两边必须同源，
// 否则「画面里的红」和「图例里的红」会在某次改色之后悄悄对不上。

export const COLORS = {
  bg: 0x060a14,
  stage: 0x0a1120,
  accent: 0x22d3ee,
  proper: 0x7dd3fc,
  improper: 0xfb7185,
  mirror: 0x4ade80,
  invert: 0xfb7185,
  ghost: 0x64748b,
  image: 0xc7d2fe,          /* 同一群作用出来的其余等效点（真旋转得到的） */
  imageImp: 0xfca5a5,       /* 非真操作得到的等效点 */
  line: 0x7e96c8,
  x: 0xf87171,
  y: 0x4ade80,
  z: 0x60a5fa,
  tetraFaces: [0x22d3ee, 0xfbbf24, 0xa78bfa, 0xf472b6],
  tetraVerts: [0x6ea8fe, 0xfdb454, 0x4ade80, 0xf472b6],
  axis4: 0xfbbf24,
  axis3: 0x60a5fa,
};

/** Canvas2D 里用字符串色，和上面一一对应 */
export const CSS = {
  accent: '#22d3ee',
  proper: '#7dd3fc',
  improper: '#fb7185',
  mirror: '#4ade80',
  invert: '#fb7185',
  ghost: '#64748b',
  txt: '#eaf0fd',
  muted: '#94a3c4',
  dim: 'rgba(126, 150, 200, 0.32)',
  x: '#f87171',
  y: '#4ade80',
  z: '#60a5fa',
};

/** 负指标与反轴的上横线：画布贴图上只能靠组合字符 */
export const over = (x) => `${x}\u0304`;

/** 舞台基准单位：短边的 1/100，所有尺寸从它推 —— 写死像素在投影下一定翻车 */
export const unitOf = (w, h) => Math.min(w, h) / 100;
