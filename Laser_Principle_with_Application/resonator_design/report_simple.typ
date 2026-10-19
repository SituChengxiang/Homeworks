#import "@local/simple-zjut-report:0.1.1": experiment-report
#import "summary.typ": R, COMPARISON

// 数字全部来自 cavity.py 生成的 summary.typ，不手抄。
#show figure.where(kind: image): set figure(supplement: [图])
#show figure.where(kind: table): set figure(supplement: [表])

#show: doc => experiment-report(
  title: "Report1 两镜光学谐振腔设计",
  semester: "2026-2027学年（一）学期",
  class: "应用物理基地班2401",
  name: "situChengxiang 华晨翔 陈露云 石晨曦 邓翱鹏",
  student-id: "302024343017",
  date: "2026年10月16日",
  doc
)

= 两镜光学谐振腔设计

== 设计任务与指标

设计一个总腔长不超过 1 m 的两镜光学谐振腔，要求为稳定腔，并具有基本的工程可实现性、不依赖临界状态。三条要求可以写成可核算的判据：

#figure(
  table(
    columns: (auto, 1fr, auto),
    inset: 6pt,
    stroke: 0.5pt + rgb("#c9ced3"),
    table.header[*要求*][*判据*][*本设计*],
    [总腔长 ≤ 1 m], [光学腔长 $L$], [#R.L_mm mm],
    [稳定腔], [$0 < g_1 g_2 < 1$],
    [$g_1 g_2 = #R.prod$；$abs((A+D)/2) = #R.half_trace$],
    [工程可实现、不靠临界], [腔长裕度、失调容差、孔径损耗],
    [#R.margin_L_mm mm；#R.mis_tolerance_urad μrad；约 $2.2 times 10^(-7)$],
  ),
  caption: [任务要求与量化判据],
)

== 腔型与参数

采用共轴双凹线性腔：M1 为凹面高反镜，M2 为凹面输出耦合镜，腔内放一支 300 mm 长的 He-Ne 放电管作为工作介质示例。主要参数如下。

#figure(
  table(
    columns: (auto, 1fr, auto, 1fr),
    inset: 6pt,
    stroke: 0.5pt + rgb("#c9ced3"),
    align: (left, right,left, right),
    table.header[*参数*][*数值*][*参数*][*数值*],
    [腔长 $L$], [#R.L_mm mm],
    [M1 曲率半径 $R_1$], [#R.R1_mm mm（凹）],
    [M2 曲率半径 $R_2$], [#R.R2_mm mm（凹，OC）],
    [镜口径], [Ø25.4 mm（通光 Ø12.7 mm）],
    
  ),
  caption: [主要设计参数（由 `cavity.py` 算出）],
)

#figure(
  image("figures/envelope.png", width: 92%),
  caption: [腔结构与基模包络。两面镜子均为凹面，束腰落在放电管内。],
)

=== 为什么选这一组参数

- *双凹*：平面镜对应 $g=1$，平面-平面腔 $g_1 g_2 = 1$ 严格在稳定边界上；平面-凹面半球腔的一个镜子 $g=1$，工作点也在边界附近。双凹腔可以把工作点放到稳定区内部。
- *$R_1=800$ mm、$R_2=1200$ mm、$L=550$ mm*：$g_1=#R.g1$、$g_2=#R.g2$，两者都不接近 0 或 1，$g_1 g_2=#R.prod$。1 英寸镜片的口径工程上可行。
- *不选近共焦/近同心*：近共焦（$R=560$ mm）和近同心（$R=280$ mm）在同样 550 mm 腔长下都只剩约 10 mm 的腔长裕度，工作点贴近边界。
- *工程规格*：Ø25.4 mm 球面镜、$R$ 按 ±1% 定制，M1 高反（总损耗低于 0.1%），M2 输出耦合 $T=1.5%$；这些都是常规光学件。

== 稳定性

=== $g$ 参数

$ g_i = 1 - frac(L, R_i) , quad
  g_1 = #R.g1 , quad g_2 = #R.g2 , quad g_1 g_2 = #R.prod . $

因为 $0 < g_1 g_2 < 1$，且 $g_1$、$g_2$ 同号，所以该腔是稳定腔。

=== 往返矩阵

以 M1 反射后为参考面，往返矩阵为

$ M = mat(1, 0; -2/R_1, 1) mat(1, L; 0, 1) mat(1, 0; -2/R_2, 1) mat(1, L; 0, 1) , $

其半迹

$ frac(A+D, 2) = 2 g_1 g_2 - 1 = #R.half_trace . $

半迹绝对值小于 1，本征值是一对单位模的共轭复数（$abs(lambda)=#R.rt_eig_abs$），任意傍轴光线往返多次都不发散。作为数值旁证，把 $q$ 映射迭代 300 次，模斑半径在 #R.qmap_w_lo_um–#R.qmap_w_hi_um μm 之间有界振荡。

#figure(
  image("figures/g_diagram.png", width: 68%),
  caption: [$g$-$g$ 稳定图。本设计点在稳定区内部，离两条边界都有距离。],
)

== 工程可实现性

=== 腔长裕度

固定两面镜子时，$g_1 g_2 (L) = (1-L/R_1)(1-L/R_2)$。本设计的稳定区间是 $L in (0, #R.L_stable_hi_mm)$ mm，以及 $L=R_2$ 到 $R_1+R_2$ 的另一段；取 $L=#R.L_mm$ mm，距最近的边界 $L=#R.L_stable_hi_mm$ mm 还有 #R.margin_L_mm mm，相对裕度 #R.margin_L_rel_pct%。装调时腔长放缩 ±10 mm，M1 光斑只从 #R.w1_at_Lm10mm_um μm 变到 #R.w1_at_Lp10mm_um μm（约 ±1.3%）。
// 铝基座热膨胀 23 μm/(m·K)，10 K 温漂引起 $Delta L=#R.dL_thermal_mm$ mm，$g_1 g_2$ 从 #R.prod 变到 #R.prod_at_thermal_drift，仍远离边界。

=== 曲率半径公差

两面镜子的 $R$ 各取 ±1%，$g_1 g_2$ 落在 #R.prod_Rtol_lo–#R.prod_Rtol_hi 之间，仍在 $(0,1)$ 内，模式参数变化在百分之一量级。

// *失调灵敏度。* M2 倾斜 100 μrad 时，谐振轴在 M1 面偏移 #R.mis100u_offset_M1_um μm、在 M2 面偏移 #R.mis100u_offset_M2_um μm。按“轴偏移不超过较小光斑的 1/4”的工程判据，允许倾角为 #R.mis_tolerance_urad μrad（约 14″）。这个量级需要用细分/差动调节的精密镜架，锁紧后应复测。

// === 孔径与衍射损耗

// 1 英寸镜的通光半径 6.35 mm，对最大光斑 $w_1=#R.w1_um$ μm 的比值是 #R.a_over_w_max；放电管毛细 Ø2 mm 是腔内最紧的光阑，管内最大束宽 #R.bore_w_max_um μm，$a/w approx 2.8$。按高斯截断式估算，往返衍射损耗只有约 $2.2 times 10^(-7)$，远小于 1%，不构成限制。

=== 与临界腔型的对比

#figure(
  table(
    columns: (1fr, auto, auto, auto, auto),
    inset: 5pt,
    stroke: 0.5pt + rgb("#c9ced3"),
    table.header[*腔型*][*$g_1 g_2$*][*稳定*][*腔长裕度*][*失调容差*],
    ..COMPARISON.map(r => (r.name, r.prod, r.stable, r.margin, r.mis)).flatten(),
  ),
  caption: [本设计与几种临界/近临界腔型的对比（同一波长、同一腔长 550 mm）],
)

平行平面、近共焦、近同心都贴在稳定边界上，或者腔长裕度只有 10 mm；近平面腔的腔长裕度虽然更大，但失调容差只有 26.8 μrad。本设计在腔长裕度（#R.margin_L_mm mm）和失调容差（#R.mis_tolerance_urad μrad）之间取了折中，不靠边界附近的巧合。

== 结论

#figure(
  table(
    columns: (auto, 1fr, auto),
    inset: 6pt,
    stroke: 0.5pt + rgb("#c9ced3"),
    table.header[*要求*][*本设计*][*判定*],
    [总腔长 ≤ 1 m], [#R.L_mm mm（含镜架约 620 mm）], [满足],
    [稳定腔], [$g_1 g_2=#R.prod$，$abs((A+D)/2)=#R.half_trace$], [满足],
    [工程可实现、不靠临界], [腔长裕度 #R.margin_L_mm mm；\
      损耗约 $2.2 times 10^(-7)$], [满足],
  ),
  caption: [任务要求逐条对照],
)

本设计为共轴双凹稳定腔：$L=#R.L_mm$ mm，$R_1=#R.R1_mm$ mm（HR），$R_2=#R.R2_mm$ mm（OC，$T=1.5%$）。腔长、曲率半径公差、孔径都在常规工程范围内，且工作点远离稳定边界。

