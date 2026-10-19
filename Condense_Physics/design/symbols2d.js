// 设计系统 · 沿轴投影的 Canvas2D 符号层（教材 / 国际表画法）
//
// 这一层只吃「投影后的二维数据」，不认识 three.js、也不认识群论：
//   点 = { u, v, z, handed, tag, note }，线 = { a, b, kind }
// 它存在的意义是把「3̄ = 3 + i」和「6̄ = 3/m」画成两种**一眼能分辨的图案**：
//   · 关于 i 成对  → 两个点分居两侧，虚线穿过投影中心（穿心）；
//   · 关于 m⊥ 成对 → 两个点投到同一个位置，叠成一个上下各半填的圆（叠影）。
// 符号约定：z > 0 填下半圆、z < 0 填上半圆，并且**每个点都直接把 z 数值标在旁边** ——
// 半填的朝向各家教材画法不同，数字不会骗人。
import { CSS, unitOf } from './tokens.js';

const TAU = Math.PI * 2;

export function createSymbolPanel(canvas, { onPick } = {}) {
  const ctx = canvas.getContext('2d');
  let model = null;
  let inset = false;
  let hits = [];                     // 命中测试用的屏幕坐标
  let u = 8;                         // 基准单位（短边的 1/100）

  function fit() {
    const box = canvas.getBoundingClientRect();
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    canvas.width = Math.max(1, Math.round(box.width * dpr));
    canvas.height = Math.max(1, Math.round(box.height * dpr));
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    // 基准单位取短边的 1.9%：符号层里所有线宽、半径、字号都从它推，
    // 面板被压扁时整幅画一起缩，不会出现「图还在、字糊了」的情况。
    u = unitOf(box.width, box.height) * 1.9;
    return { w: box.width, h: box.height };
  }

  new ResizeObserver(() => { fit(); draw(); }).observe(canvas);

  /* ---------- 主绘制 ---------- */
  function render(next) {
    model = next;
    draw();
  }

  /** 右上角有角标时，标题区只能占左半边 */
  function setInset(on) { inset = on; }

  function draw() {
    const { w, h } = fit();
    ctx.clearRect(0, 0, w, h);
    if (!model || !model.points || !model.points.length) return;

    const cx = w / 2;
    const cy = h / 2 + u * 3;
    const maxR = Math.max(0.6, ...model.points.map((p) => Math.hypot(p.u, p.v)));
    const scale = Math.min(w * 0.34, h * 0.30) / maxR;
    const X = (p) => cx + p.u * scale;
    const Y = (p) => cy - p.v * scale;          // 屏幕 y 向下，数学 v 向上
    const r = u * 1.9;

    frame(cx, cy, maxR * scale);
    if (model.spokes?.length) spokes(cx, cy, scale, model.spokes);
    if (model.ring) ring(cx, cy, maxR * scale, model.ring);
    pairs(model, X, Y, r);
    for (const g of group(model.points)) dot(g, X, Y, r);
    centerMark(cx, cy, model.center);
    if (model.legend !== false) legend(w, h);
    heading(w, model);

    hits = model.points.map((p) => ({ id: p.id, x: X(p), y: Y(p) }));
  }

  /** 投影面的参考系：一层淡淡的圆 + 十字，给出「这是沿轴看下去」的方位感 */
  function frame(cx, cy, R) {
    ctx.save();
    ctx.strokeStyle = CSS.dim;
    ctx.lineWidth = u * 0.16;
    ctx.setLineDash([u * 0.9, u * 1.4]);
    ctx.beginPath();
    ctx.arc(cx, cy, R * 1.22, 0, TAU);
    ctx.stroke();
    ctx.setLineDash([]);
    ctx.beginPath();
    ctx.moveTo(cx - R * 1.22, cy); ctx.lineTo(cx + R * 1.22, cy);
    ctx.moveTo(cx, cy - R * 1.22); ctx.lineTo(cx, cy + R * 1.22);
    ctx.globalAlpha = 0.5;
    ctx.stroke();
    ctx.restore();
  }

  /** 水平镜面在俯视图里就是投影面本身：一圈实线椭圆 + 标签 */
  function ring(cx, cy, R, label) {
    ctx.save();
    ctx.strokeStyle = CSS.mirror;
    ctx.globalAlpha = 0.55;
    ctx.lineWidth = u * 0.3;
    ctx.beginPath();
    ctx.ellipse(cx, cy, R * 1.32, R * 1.32 * 0.32, 0, 0, TAU);
    ctx.stroke();
    if (label) {
      ctx.globalAlpha = 1;
      ctx.fillStyle = CSS.mirror;
      ctx.font = `600 ${u * 2.4}px sans-serif`;
      ctx.textAlign = 'left';
      ctx.fillText(label, cx + R * 1.32 * 0.72, cy + R * 0.32 * 1.0 + u * 0.6);
    }
    ctx.restore();
  }

  /** 上圈各点的径向线：下圈的点落在线上 = 上下同方位角 = 有水平镜面 */
  function spokes(cx, cy, scale, angles) {
    ctx.save();
    ctx.strokeStyle = CSS.dim;
    ctx.lineWidth = u * 0.2;
    for (const a of angles) {
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(cx + Math.cos(a) * scale * 1.45, cy - Math.sin(a) * scale * 1.45);
      ctx.stroke();
    }
    ctx.restore();
  }

  function centerMark(cx, cy, kind) {
    if (!kind) return;
    ctx.save();
    ctx.translate(cx, cy);
    ctx.lineWidth = u * 0.3;
    if (kind === 'i') {
      ctx.strokeStyle = CSS.invert;
      ctx.beginPath(); ctx.arc(0, 0, u * 1.5, 0, TAU); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(-u * 1.05, 0); ctx.lineTo(u * 1.05, 0);
      ctx.moveTo(0, -u * 1.05); ctx.lineTo(0, u * 1.05); ctx.stroke();
      ctx.fillStyle = CSS.invert;
      ctx.font = `600 ${u * 2.1}px sans-serif`;
      ctx.textAlign = 'left';
      ctx.fillText('i', u * 2.6, -u * 1.8);
    } else {
      ctx.strokeStyle = CSS.mirror;
      ctx.beginPath(); ctx.arc(0, 0, u * 1.5, 0, TAU); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(-u * 1.05, 0); ctx.lineTo(u * 1.05, 0); ctx.stroke();
      ctx.fillStyle = CSS.mirror;
      ctx.font = `600 ${u * 2.1}px sans-serif`;
      ctx.textAlign = 'left';
      ctx.fillText('m⊥', u * 2.6, u * 0.8);
    }
    ctx.restore();
  }

  /** 配对线：i 穿心（虚线、暖色），m 叠影（短竖线 + 双箭头，绿色） */
  function pairs(model, X, Y, r) {
    for (const pr of model.pairs || []) {
      const a = model.points.find((p) => p.id === pr.a);
      const b = model.points.find((p) => p.id === pr.b);
      if (!a || !b) continue;
      const stacked = Math.hypot(a.u - b.u, a.v - b.v) < 1e-6;
      ctx.save();
      if (stacked) {
        const x = X(a);
        const y = Y(a);
        ctx.strokeStyle = CSS.mirror;
        ctx.lineWidth = u * 0.34;
        arrow(x, y - r * 1.15, x, y - r * 2.6);
        arrow(x, y + r * 1.15, x, y + r * 2.6);
        label(x + r * 1.4, y - r * 2.3, '上下互为镜像', CSS.mirror, u * 1.7, 'left');
      } else {
        ctx.strokeStyle = pr.kind === 'i' ? CSS.invert : CSS.ghost;
        ctx.lineWidth = u * 0.26;
        ctx.setLineDash([u * 0.8, u * 0.9]);
        ctx.beginPath();
        ctx.moveTo(X(a), Y(a));
        ctx.lineTo(X(b), Y(b));
        ctx.stroke();
        ctx.setLineDash([]);
      }
      ctx.restore();
    }
  }

  function arrow(x0, y0, x1, y1) {
    const a = Math.atan2(y1 - y0, x1 - x0);
    ctx.beginPath(); ctx.moveTo(x0, y0); ctx.lineTo(x1, y1); ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x1 - u * 0.9 * Math.cos(a - 0.5), y1 - u * 0.9 * Math.sin(a - 0.5));
    ctx.lineTo(x1 - u * 0.9 * Math.cos(a + 0.5), y1 - u * 0.9 * Math.sin(a + 0.5));
    ctx.closePath();
    ctx.fillStyle = ctx.strokeStyle;
    ctx.fill();
  }

  function label(x, y, text, color, size, align = 'center', bold = false) {
    ctx.save();
    ctx.fillStyle = color;
    ctx.font = `${bold ? 700 : 500} ${size}px sans-serif`;
    ctx.textAlign = align;
    ctx.textBaseline = 'middle';
    ctx.fillText(text, x, y);
    ctx.restore();
  }

  /**
   * 投到同一个 (u,v) 的点合并成一个符号：上下各半填 —— 这就是「叠影」。
   * 分开的点则按各自 z 的符号填一半。
   */
  function group(points) {
    const out = [];
    for (const p of points) {
      const hit = out.find((g) => Math.hypot(g[0].u - p.u, g[0].v - p.v) < 1e-6);
      if (hit) hit.push(p); else out.push([p]);
    }
    return out;
  }

  function dot(group, X, Y, r) {
    const [p] = group;
    const x = X(p);
    const y = Y(p);
    const stacked = group.length > 1;
    const sel = group.some((q) => q.selected);
    ctx.save();

    if (sel) {
      ctx.strokeStyle = CSS.accent;
      ctx.lineWidth = u * 0.42;
      ctx.beginPath(); ctx.arc(x, y, r * 1.75, 0, TAU); ctx.stroke();
    }

    const disc = (color) => {
      ctx.beginPath(); ctx.arc(x, y, r, 0, TAU);
      ctx.fillStyle = color; ctx.fill();
    };
    // 半圆：sign > 0 填下半、sign < 0 填上半（屏幕 y 向下，所以下半是 0..π）
    const halfDisc = (sign, color) => {
      ctx.beginPath();
      ctx.moveTo(x, y);
      ctx.arc(x, y, r, sign > 0 ? 0 : Math.PI, sign > 0 ? Math.PI : TAU);
      ctx.closePath();
      ctx.fillStyle = color; ctx.fill();
    };

    if (model.flat) {
      // 展开图：高度已经变成半径，整颗点只表手性
      disc(p.handed > 0 ? CSS.proper : CSS.improper);
    } else {
      // 教材俯视图：半填表 z 的正负；重合的一对上下各填一半，就是「叠影」
      const upper = stacked ? +1 : (p.z >= 0 ? +1 : -1);
      halfDisc(upper, upper > 0 ? CSS.proper : CSS.improper);
      if (stacked) halfDisc(-upper, CSS.proper);
      ctx.beginPath();
      ctx.moveTo(x - r, y); ctx.lineTo(x + r, y);
      ctx.strokeStyle = 'rgba(220, 232, 255, .55)';
      ctx.lineWidth = u * 0.16;
      ctx.stroke();
    }

    ctx.strokeStyle = sel ? CSS.accent : '#dbe6ff';
    ctx.lineWidth = u * (sel ? 0.34 : 0.24);
    ctx.beginPath(); ctx.arc(x, y, r, 0, TAU); ctx.stroke();

    // 手性指针：沿该点三面角 x 轴的投影方向伸出一根小针，右手暖色、左手冷色
    for (const q of group) {
      if (q.dir === undefined) continue;
      const col = q.handed > 0 ? CSS.x : CSS.z;
      ctx.strokeStyle = col;
      ctx.lineWidth = u * 0.3;
      ctx.beginPath();
      ctx.moveTo(x, y);
      ctx.lineTo(x + Math.cos(q.dir) * r * 2.1, y - Math.sin(q.dir) * r * 2.1);
      ctx.stroke();
    }

    const tags = group.map((q) => q.tag).filter((t) => t !== undefined && t !== '');
    if (tags.length) label(x, y - r * 2.4, tags.join('／'), '#eaf0fd', u * 2.0, 'center', true);
    const note = stacked ? group.map((q) => signed(q.z)).join(' ∥ ') : signed(p.z);
    label(x + r * 1.45, y + r * 1.4, note, stacked ? CSS.mirror : CSS.muted, u * 1.8, 'left');
    ctx.restore();
  }

  function signed(z) {
    const v = Math.round(z * 100) / 100;
    return v >= 0 ? `+${v.toFixed(2)}` : v.toFixed(2);
  }

  // 标题左对齐、副标题限宽换行：右上角留给教材俯视图那个角标，
  // 居中或不限宽都会把字压在角标上 —— 上一版就是这么糊掉的。
  function heading(w, model) {
    const right = inset ? w * 0.56 : w - u * 3;
    if (model.heading) label(u * 3, u * 4.5, model.heading, '#eaf0fd', u * 3.0, 'left', true);
    if (model.sub) wrapped(u * 3, u * 8.6, model.sub, right - u * 3, CSS.accent, u * 2.2);
  }

  function wrapped(x, y, text, maxWidth, color, size) {
    ctx.save();
    ctx.font = `500 ${size}px sans-serif`;
    ctx.textAlign = 'left';
    ctx.textBaseline = 'middle';
    ctx.fillStyle = color;
    const chars = [...text];
    let line = '';
    let yy = y;
    for (const ch of chars) {
      if (ctx.measureText(line + ch).width > maxWidth && line) {
        ctx.fillText(line, x, yy);
        yy += size * 1.35;
        line = ch;
      } else line += ch;
    }
    if (line) ctx.fillText(line, x, yy);
    ctx.restore();
  }

  function legend(w, h) {
    const items = model.flat
      ? [['整颗：真旋转得 / 非真得', CSS.proper],
         ['指针：手性', CSS.x],
         ['虚线穿过中心：关于 i', CSS.invert],
         ['细径向线：上圈的方位角', CSS.muted]]
      : [['半填：z 的正负', CSS.proper],
         ['指针：手性', CSS.x],
         ['穿心虚线：关于 i', CSS.invert],
         ['叠影：关于 m⊥', CSS.mirror]];
    let x = u * 3;
    const y = h - u * 3.2;
    ctx.save();
    ctx.font = `500 ${u * 1.8}px sans-serif`;
    ctx.textBaseline = 'middle';
    ctx.textAlign = 'left';
    for (const [text, color] of items) {
      ctx.fillStyle = color;
      ctx.beginPath(); ctx.arc(x, y, u * 0.75, 0, TAU); ctx.fill();
      ctx.fillStyle = CSS.muted;
      ctx.fillText(text, x + u * 1.4, y);
      x += u * 1.4 + ctx.measureText(text).width + u * 2.4;
    }
    ctx.restore();
  }

  /* ---------- 命中测试：点谁高亮谁的入口 ---------- */
  canvas.addEventListener('click', (e) => {
    if (!onPick) return;
    const box = canvas.getBoundingClientRect();
    const mx = e.clientX - box.left;
    const my = e.clientY - box.top;
    let best = null;
    let d = u * 3.4;
    for (const hh of hits) {
      const dd = Math.hypot(hh.x - mx, hh.y - my);
      if (dd < d) { d = dd; best = hh; }
    }
    if (best) onPick(best.id);
  });

  draw();
  return { render, setInset, get model() { return model; }, redraw: draw };
}
