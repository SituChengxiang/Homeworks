#import "@local/simple-zjut-report:0.1.1": experiment-report
#import "summary.typ": R, COMPARISON

// 数字全部来自 cavity.py 生成的 summary.typ，不手抄。
#show figure.where(kind: image): set figure(supplement: [图])
#show figure.where(kind: table): set figure(supplement: [表])

#show: doc => experiment-report(
  title: "任务1 第一个稳定光学谐振腔",
  semester: "2026-2027学年（一）学期",
  class: "激光原理与技术",
  name: "成员A 成员B 成员C 成员D 成员E",
  student-id: "5",
  date: "2026.10.19",
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
    [工程可实现、不靠临界], [失稳裕度、临界点裕度、失调容差],
    [#R.margin_L_mm mm、#R.margin_crit_mm mm],
  ),
  caption: [任务要求与量化判据],
)

== 腔型与参数

采用共轴双凹（对称）线性腔：两面镜子曲率半径相同，M1 为凹面高反镜，M2 为凹面输出耦合镜，腔内放一支 300 mm 长的 He-Ne 放电管作为工作介质示例。主要参数如下。

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
  caption: [主要设计参数],
)

#figure(
  image("figures/cavity.png", width: 92%),
  caption: [腔结构示意：腔长、两镜曲率与两镜焦点（镜面曲率已放大，真实矢高只有几十 μm）。\
    两焦点相距 #R.focus_sep_mm mm——对称共焦腔要求 $R_1=R_2=L$、两焦点重合在腔心，\
    本设计 $R$ 比 $L$ 大 #R.margin_crit_mm mm，是贴近但不踩线的稳定腔。],
)

=== 为什么选这一组参数

- *对称双凹 $R_1 = R_2 = 600$ mm、$L = 550$ mm*：$g_1 = g_2 = #R.g1$，$g_1 g_2 = #R.prod$。两面镜子规格相同，加工、装调、分析都对称；1 英寸镜片的口径工程上可行。
- *贴近课本的对称共焦腔，但留了余量*：对称共焦要求 $R = L = 550$ mm，那时 $g_1 g_2 = 0$ 正好落在稳定区边界上，接近临界腔，横纵模间隔比恰好是 1/2（模式简并）。本设计 $R$ 比 $L$ 大 #R.margin_crit_mm mm，错开了这个简并点。
- *不取同心与平面-平面*：同心（$R = L/2$）和平行平面（$R → ∞$）的 $g_1 g_2 = 1$，同样在临界点上；本设计腔长要挪到 0 或 #R.L_stable_hi_mm mm 才出稳定区，裕度 #R.margin_L_mm mm。
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

半迹绝对值小于 1，本征值是一对单位模的共轭复数（$abs(lambda)=#R.rt_eig_abs$），任意傍轴光线往返多次都不发散。
// 作为数值旁证，把 $q$ 映射迭代 300 次，模斑半径在 #R.qmap_w_lo_um–#R.qmap_w_hi_um μm 之间有界振荡。

#figure(
  image("figures/g_diagram.png", width: 80%),
  caption: [$g$-$g$ 稳定图（蓝色为稳定区）。大蓝点是本设计，在第一象限的稳定区内、\
    紧贴但没落在共焦简并点 (0,0) 上；右下插图是原点放大。],
)

== 工程可实现性

=== 腔长裕度

固定两面镜子时，$g_1 g_2 (L) = (1-L/R_1)(1-L/R_2)$，只在 $L = 0$、$R_1$、$R_2$、$R_1+R_2$ 处取 0 或 1。本设计的稳定区间是 $L in (0, #R.L_stable_hi_mm)$ mm，其中 $L = R_1 = #R.R1_mm$ mm 处只是 $g_1 g_2$ 触 0 又弹回的简并点、两侧都仍然稳定，真正失稳要到 $L → 0$ 或 $L = R_1 + R_2$。取 $L = #R.L_mm$ mm，到失稳区 #R.margin_L_mm mm，到最近的共焦简并点 #R.margin_crit_mm mm，两个数都在表里可查。装调时腔长放缩 ±10 mm，M1 光斑只从 #R.w1_at_Lm10mm_um μm 变到 #R.w1_at_Lp10mm_um μm（约 ±0.8%）。
// 铝基座热膨胀 23 μm/(m·K)，10 K 温漂引起 $Delta L=#R.dL_thermal_mm$ mm，$g_1 g_2$ 从 #R.prod 变到 #R.prod_at_thermal_drift，仍远离边界。

=== 曲率半径公差

两面镜子的 $R$ 各取 ±1%，$g_1 g_2$ 落在 #R.prod_Rtol_lo–#R.prod_Rtol_hi 之间，仍严格在 $(0,1)$ 内，模式尺寸的变化不到 1%。

// *失调灵敏度。* M2 倾斜 100 μrad 时，谐振轴在 M1 面偏移 #R.mis100u_offset_M1_um μm、在 M2 面偏移 #R.mis100u_offset_M2_um μm。按“轴偏移不超过较小光斑的 1/4”的工程判据，允许倾角为 #R.mis_tolerance_urad μrad（约 16″）。这个量级需要用细分/差动调节的精密镜架，锁紧后应复测。

// === 孔径与衍射损耗

// 1 英寸镜的通光半径 6.35 mm，对最大光斑 $w_1=#R.w1_um$ μm 的比值是 #R.a_over_w_max；放电管毛细 Ø2 mm 是腔内最紧的光阑，管内最大束宽 #R.bore_w_max_um μm，$a/w approx 3.5$。按高斯截断式估算，往返衍射损耗只有 $#R.loss_total_tex$，远小于 1%，不构成限制。

=== 与临界腔型的对比

（注：本部分大部分由AI辅助编写，经过人工核验）

#figure(
  table(
    columns: (1fr, auto, auto, auto, auto),
    inset: 5pt,
    stroke: 0.5pt + rgb("#c9ced3"),
    table.header[*腔型*][*$g_1 g_2$*][*稳定*][*到失稳区*][*到临界点*],
    ..COMPARISON.map(r => (r.name, r.prod, r.stable, r.margin, r.crit)).flatten(),
  ),
  caption: [本设计与几种临界/近临界腔型的对比（同一波长、同一腔长 550 mm；后两列都是腔长方向的距离）],
)

“到失稳区”与“到临界点”都是腔长方向的裕度：后者指腔长挪多远才碰到 $g_1 g_2 = 0$（共焦简并）或 $g_1 g_2 = 1$（平面-平面/同心）这类临界点。严格共焦、平面-平面自己就停在临界点上，稳定判据直接不过；近共焦、近同心只剩 10 mm。本设计的两个裕度是 #R.margin_L_mm mm 与 #R.margin_crit_mm mm，都写在表里，不靠边界附近的巧合。

== 结论

#figure(
  table(
    columns: (auto, 1fr, auto),
    inset: 6pt,
    stroke: 0.5pt + rgb("#c9ced3"),
    table.header[*要求*][*本设计*][*判定*],
    [总腔长 ≤ 1 m], [#R.L_mm mm（含镜架约 620 mm）], [满足],
    [稳定腔], [$g_1 g_2=#R.prod$，$abs((A+D)/2)=#R.half_trace$], [满足],
    [工程可实现、不靠临界], [失稳裕度 #R.margin_L_mm mm；到共焦简并点 #R.margin_crit_mm mm], [满足],
  ),
  caption: [任务要求逐条对照],
)

本设计为共轴对称双凹稳定腔：$L = #R.L_mm$ mm，$R_1 = R_2 = #R.R1_mm$ mm（M1 高反，M2 输出耦合 $T = 1.5%$），曲率比对称共焦条件 $R = L$ 大 #R.margin_crit_mm mm、两镜焦点相距 #R.focus_sep_mm mm，是贴近课本共焦腔但不踩临界线的稳定腔。曲率 ±1% 公差与失调容差都在常规镜架能力内，工作点在稳定区内部而非边界上。

