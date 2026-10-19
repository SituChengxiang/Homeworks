// 三个页面共用的显示层：舞台搭建、资源缓存、按需渲染、摄制接口。
// 资源缓存的目的很单纯——重建（rebuild）只该改场景图，不该重新分配 GPU 缓冲区。
import * as THREE from 'three';
import { OrbitControls } from '../vendor/OrbitControls.js';

/** 自动旋转速度：OrbitControls 的约定是「60 / speed 秒转一圈」，1.0 ≈ 一分钟一圈 */
export const AUTO_ROTATE_SPEED = 1.0;

const geos = new Map();
const mats = new Map();
const texs = new Map();
const planes = new Map();

function cache(map, key, make) {
  let hit = map.get(key);
  if (!hit) { hit = make(); map.set(key, hit); }
  return hit;
}

/* ---------- 几何：都用单位尺寸 + 缩放，半径变化不必重建缓冲区 ---------- */
export const unitSphere = (widthSeg = 32, heightSeg = 24) =>
  cache(geos, `sphere|${widthSeg}|${heightSeg}`, () => new THREE.SphereGeometry(1, widthSeg, heightSeg));

export const unitCircle = (seg = 28) =>
  cache(geos, `circle|${seg}`, () => new THREE.CircleGeometry(1, seg));

export const unitCylinder = () =>
  cache(geos, 'cylinder', () => new THREE.CylinderGeometry(1, 1, 1, 12));

/**
 * 有厚度的线：WebGL 里 LineSegments 的 linewidth 基本无效（恒为 1 像素），
 * 在视频里棱框会被球挡住几乎看不见，所以框线一律用细圆柱实例画。
 */
const UP = new THREE.Vector3(0, 1, 0);

export function updateTubes(inst, segments, radius) {
  const dir = new THREE.Vector3();
  const pos = new THREE.Vector3();
  const quat = new THREE.Quaternion();
  const scale = new THREE.Vector3();
  const mat4 = new THREE.Matrix4();
  const n = Math.min(segments.length, inst.count ?? segments.length);
  for (let i = 0; i < n; i++) {
    const [a, b] = segments[i];
    dir.subVectors(b, a);
    const len = Math.max(dir.length(), 1e-4);
    quat.setFromUnitVectors(UP, pos.copy(dir).divideScalar(len));
    pos.copy(a).addScaledVector(dir, 0.5);
    scale.set(radius, len, radius);
    inst.setMatrixAt(i, mat4.compose(pos, quat, scale));
  }
  inst.count = n;
  inst.instanceMatrix.needsUpdate = true;
  return inst;
}

export function tubeEdges(key, segments, radius, color, opacity = 1) {
  const inst = new THREE.InstancedMesh(
    unitCylinder(),
    stdMat(`tube|${key}`, {
      color, roughness: 0.45, metalness: 0.1,
      transparent: opacity < 1, opacity,
    }),
    Math.max(segments.length, 1)
  );
  return updateTubes(inst, segments, radius);
}

export const boxEdges = (size) =>
  cache(geos, `box|${size}`, () => new THREE.EdgesGeometry(new THREE.BoxGeometry(size, size, size)));

/* ---------- 材质 ---------- */
// 材质会被淡入动画改 opacity，所以把初始值记在 userData 上（缓存复用后不能靠当场读取）
const rememberOpacity = (m, params) => {
  m.userData.baseOpacity = params.opacity === undefined ? 1 : params.opacity;
  return m;
};

export const stdMat = (key, params) =>
  cache(mats, `std|${key}`, () => rememberOpacity(new THREE.MeshStandardMaterial(params), params));

export const lineMat = (key, params) =>
  cache(mats, `line|${key}`, () => rememberOpacity(new THREE.LineBasicMaterial(params), params));

/* ---------- 文字标签（canvas 贴图最贵，必须缓存） ---------- */
function labelTexture(text, color, w, h) {
  return cache(texs, `${text}|${color}|${w}x${h}`, () => {
    const c = document.createElement('canvas');
    c.width = w; c.height = h;
    const ctx = c.getContext('2d');
    ctx.font = `bold ${Math.round(h * 0.6)}px sans-serif`;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillStyle = color;
    ctx.fillText(text, w / 2, h / 2 + 2);
    const tex = new THREE.CanvasTexture(c);
    tex.colorSpace = THREE.SRGBColorSpace;
    return tex;
  });
}

export function labelSprite(text, color, w = 64, h = 64, scale = 0.55) {
  const mat = cache(mats, `sprite|${text}|${color}|${w}x${h}`, () => rememberOpacity(new THREE.SpriteMaterial({
    map: labelTexture(text, color, w, h), transparent: true, depthWrite: false,
  }), { opacity: 1 }));
  const s = new THREE.Sprite(mat);
  s.scale.set(scale * (w / h), scale, scale);
  return s;
}

/* ---------- 裁剪面：把原子切成本胞内的份额（顶点 1/8、面心 1/2） ---------- */
export function sharePlanes(pos, half) {
  const sign = [0, 1, 2].map((i) => Math.sign(pos.getComponent(i))).join(',');
  return cache(planes, `${half}|${sign}`, () => {
    const out = [];
    for (let i = 0; i < 3; i++) {
      const s = Math.sign(pos.getComponent(i));
      if (s === 0) continue;
      const n = new THREE.Vector3();
      n.setComponent(i, -s);
      out.push(new THREE.Plane(n, half)); // 保留 n·p + half ≥ 0 的一侧
    }
    return out;
  });
}

/* ---------- 场景 ---------- */
export function makeStage({ canvasId, bg = 0x0b1020, fogDensity = 0.017 }) {
  const canvas = document.getElementById(canvasId);
  const renderer = new THREE.WebGLRenderer({
    canvas, antialias: true, powerPreference: 'high-performance',
  });
  renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
  renderer.localClippingEnabled = true;

  const scene = new THREE.Scene();
  scene.background = new THREE.Color(bg);
  scene.fog = new THREE.FogExp2(bg, fogDensity);

  const camera = new THREE.PerspectiveCamera(45, 1, 0.1, 200);
  camera.position.set(5.4, 4.0, 6.6);

  const controls = new OrbitControls(camera, canvas);
  controls.enableDamping = true;
  controls.dampingFactor = 0.08;
  controls.minDistance = 3;
  controls.maxDistance = 30;
  controls.autoRotateSpeed = AUTO_ROTATE_SPEED;

  scene.add(new THREE.AmbientLight(0xffffff, 0.5));
  scene.add(new THREE.HemisphereLight(0xbfd4ff, 0x141a2e, 0.9));
  const key = new THREE.DirectionalLight(0xffffff, 1.6);
  key.position.set(7, 10, 8);
  scene.add(key);
  const fill = new THREE.DirectionalLight(0x88aaff, 0.75);
  fill.position.set(-8, -5, -7);
  scene.add(fill);

  const root = new THREE.Group();
  scene.add(root);

  const dirty = makeDirty();
  const fit = () => {
    const w = canvas.clientWidth, h = canvas.clientHeight;
    if (!w || !h) return;
    renderer.setSize(w, h, false);
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
    dirty.set();
  };
  fit();                        // 首帧就要有正确的画布尺寸
  new ResizeObserver(fit).observe(canvas);

  return { canvas, renderer, scene, camera, controls, root, dirty };
}

/** 极简脏标记：只有场景真的变了才提交一帧 */
export function makeDirty() {
  let flag = true;
  return {
    set: () => { flag = true; },
    take: () => { const v = flag; flag = false; return v; },
  };
}

/**
 * 按需渲染循环：空闲时不出图（省电、也不占 GPU）。
 * onFrame 返回 true 表示这一帧改了东西（生长动画等），必须出图。
 */
export function startLoop({ renderer, scene, camera, controls, dirty, onFrame }) {
  controls.addEventListener('change', dirty.set);
  let last = performance.now();
  const frame = (now) => {
    requestAnimationFrame(frame);
    // 必须把 deltaTime 传给 update()：不传的话 OrbitControls 按「每帧固定角度」自转，
    // 120/144 Hz 屏上就会比预期快 2～2.4 倍。上限 50 ms 是防止切回后台再回来跳一大步。
    const dt = Math.min(Math.max((now - last) / 1000, 0), 0.05);
    last = now;
    const touched = onFrame ? onFrame(now) : false;
    controls.update(dt);             // 阻尼/自动旋转会在这里触发 change
    if (dirty.take() || touched) renderer.render(scene, camera);
  };
  requestAnimationFrame(frame);
}

/** 场景图重建：资源都在缓存里，这里只需要摘掉旧节点 */
const objs = new Map();

/**
 * 摄制模式（?film=1）：把画面变成参数的纯函数，供无头 Chrome 逐帧截图。
 * 关掉阻尼和自动旋转，set() 里同步渲染一帧，这样每张截图都是确定的、不含时间噪声。
 */
export function installFilmAPI({ camera, controls, renderer, scene, root, apply, meta }) {
  // 只留 3D 画面：侧栏和表格在视频里读不清，步骤说明用画布上的浮层就够
  const style = document.createElement('style');
  style.textContent = `
    body.film #app { grid-template-columns: 1fr !important; }
    body.film #side, body.film #info { display: none !important; }
    body.film #stepPanel, body.film #caption { max-width: min(78%, 900px); }
    body.film #stepText, body.film #capText { font-size: 15px; }
    body.film .badge { font-size: 14px; padding: 8px 14px; }
  `;
  document.head.appendChild(style);
  document.body.classList.add('film');

  controls.enableDamping = false;
  controls.autoRotate = false;

  const box = new THREE.Box3();
  const center = new THREE.Vector3();
  const size = new THREE.Vector3();

  /** 自动取景：焦点放在当前可见图层的包围盒中心，距离按视野角算，
      否则只有底面七个球的步骤会整团跑到画面角落去。 */
  function frame(cam) {
    const az = cam.az ?? 0.9, po = cam.po ?? 1.12;
    const dir = new THREE.Vector3(Math.sin(po) * Math.cos(az), Math.cos(po), Math.sin(po) * Math.sin(az));
    let target = center.set(0, 0, 0);
    let dist = cam.dist;
    // 取景只看主体：扩展点云之类的背景（userData.noFrame）会把包围盒撑得巨大，
    // 一进去晶胞就缩成画面中间的一个小点。
    box.makeEmpty();
    for (const child of root.children) if (!child.userData.noFrame) box.expandByObject(child);
    if (!box.isEmpty()) {
      box.getCenter(target);
      box.getSize(size);
      const r = Math.max(size.length() / 2, 0.6);
      const halfV = THREE.MathUtils.degToRad(camera.fov / 2);
      const halfH = Math.atan(Math.tan(halfV) * (camera.aspect || 16 / 9));
      dist = (r * 1.35) / Math.min(Math.sin(halfV), Math.sin(halfH));
    }
    if (cam.dist) dist *= cam.dist / 9.6;          // 分镜仍可微调远近
    if (!Number.isFinite(dist) || dist <= 0) dist = cam.dist || 9.6;
    camera.position.copy(target).addScaledVector(dir, dist);
    controls.target.copy(target);
    controls.update();
  }

  window.__film = {
    meta,
    set(params = {}) {
      apply(params);                               // 先更新场景，再按新内容取景
      frame(params.cam || {});
      renderer.render(scene, camera);
    },
  };
}

/**
 * 图层「逐件生长/淡入」动画。
 * 因为材质是共享缓存的，淡入时不能当场读 material.opacity（可能已经被前一个对象设成 0），
 * 必须用创建时记下的 baseOpacity；每帧只处理首次遇到的对象，共享同一材质的后续对象自动同步。
 */
export function makeAppear({ stagger = 42, dur = 430 } = {}) {
  let items = [];
  let t0 = 0;

  function begin(root, targets) {
    settle();                       // 上一段动画若没跑完，先归位
    items = [];
    const faded = new Set();
    let i = 0;
    // targets：只有这一步新出现的图层参与生长/淡入，已经在那儿的图层保持不动，
    // 否则每按一次“下一步”整幅画都重新长一遍，看上去就是在重播而不是在推进。
    const roots = targets ? targets : [root];
    for (const node of roots) {
      node.traverse((o) => {
        if (o.isInstancedMesh) {
          const full = o.userData.fullCount ?? o.count;
          o.userData.fullCount = full;
          o.count = 0;
          items.push({ o, kind: 'inst', base: full, i: i++ });
        } else if (o.userData.grow) {
          const full = o.scale.x > 1e-3 ? o.scale.x : (o.userData.growBase ?? 1);
          o.userData.growBase = full;
          o.scale.setScalar(1e-4);
          items.push({ o, kind: 'grow', base: full, i: i++ });
        } else if (o.material && o.material.transparent && !faded.has(o.material)) {
          faded.add(o.material);
          const base = o.material.userData.baseOpacity ?? 1;
          o.material.opacity = 0;
          items.push({ o, kind: 'fade', base, i: i++ });
        }
      });
    }
    t0 = performance.now();
  }

  function apply(it, k) {
    const e = 1 - Math.pow(1 - k, 3);
    if (it.kind === 'grow') it.o.scale.setScalar(Math.max(1e-4, it.base * e));
    else if (it.kind === 'fade') it.o.material.opacity = it.base * e;
    else it.o.count = Math.round(it.base * e);
  }

  /** 中途取消：把所有资源恢复到最终外观（缓存对象不能停在半透明/半大小） */
  function settle() {
    for (const it of items) apply(it, 1);
    items = [];
  }

  /** 返回 true 表示这一帧动过外观，必须出图 */
  function tick(now) {
    if (!items.length) return false;
    let allDone = true;
    for (const it of items) {
      const k = Math.min(1, Math.max(0, (now - t0 - it.i * stagger) / dur));
      apply(it, k);
      if (k < 1) allDone = false;
    }
    if (allDone) items = [];
    return true;
  }

  return { begin, settle, tick, get running() { return items.length > 0; } };
}

/**
 * 图层对象缓存：同一个 key 只搭一次，之后 rebuild 只是把现成的 Group 挂回去。
 * 这些对象只会被挂在 root 上，所以复用是安全的。
 */
export function cachedObject(key, make) {
  let o = objs.get(key);
  if (!o) { o = make(); objs.set(key, o); }
  return o;
}

export function clearGroup(g) {
  g.clear();
}
