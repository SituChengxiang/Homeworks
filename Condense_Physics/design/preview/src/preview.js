// 设计系统的可视化验收页：同一批等效点系，用两种画法各画一遍。
// 上排 = 教材俯视图（半填圆点表 z 的正负；上下成对的两点会重合）
// 下排 = 上下圈展开图（高度换成半径：点数不被合并，「对齐」与「穿心」直接可见）
import '../../tokens.css';
import { barOrbit } from '../../../lib/rotoinversion.js';
import { orbitModel, selectPoint } from '../../../lib/projection.js';
import { over } from '../../../lib/matrix.js';
import { createSymbolPanel } from '../../symbols2d.js';

const PM = [1.15, 0.45, 0.78];          // 一般位置的取样点
const NS = [1, 2, 3, 4, 6];

/** 往某一排放一个面板，并接上「点谁高亮谁」 */
function addPanel(parent, model) {
  const cell = document.createElement('div');
  cell.className = 'cell';
  const canvas = document.createElement('canvas');
  cell.appendChild(canvas);
  parent.appendChild(cell);

  const panel = createSymbolPanel(canvas, {
    onPick: (id) => {
      const already = model.points.some((p) => p.selected && p.id === id);
      selectPoint(model, already ? null : id);
      panel.render(model);
    },
  });
  panel.render(model);
  return panel;
}

const verdict = document.getElementById('verdict');

for (const n of NS) {
  const orbit = barOrbit(n, PM);
  const top = orbitModel(orbit, `${over(n)} 俯视 · 教材画法`);
  addPanel(document.getElementById('row-top'), top);
  addPanel(document.getElementById('row-fan'), orbitModel(orbit, `${over(n)} 上下圈展开`, { view: 'fan' }));

  const s = top.stats;
  const word = s.signature === 'stack' ? '上下同方位角 → m⊥'
    : s.signature === 'through' ? '成对穿过中心 → i' : '两者都没有';
  const v = document.createElement('div');
  v.innerHTML = `<b>${over(n)}</b>　${s.points} 点 / 俯视落 ${s.spots} 处<br>${word}`;
  verdict.appendChild(v);
}
