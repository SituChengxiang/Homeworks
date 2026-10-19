#import "@local/simple-zjut-report:0.1.1": experiment-report
#import "summary.typ": R, COMPARISON

// 报告里的每一个数字都取自 summary.typ —— 它由 cavity.py 每次运行时重新生成，
// 手改无效。这样"报告说的"和"脚本算的"不可能对不上。

#show figure.where(kind: image): set figure(supplement: [图])
#show figure.where(kind: table): set figure(supplement: [表])
#set math.equation(numbering: "(1)")

#show: doc => experiment-report(
  title: "Report1两镜光学谐振腔设计",
  semester: "2026-2027学年（一）学期",
  class: "应用物理基地班2401",
  name: "situChengxiang 华晨翔 陈露云 石晨曦 邓翱鹏",
  student-id: "302024343017",
  date: "2026年10月16日",
  doc
)

= 两镜光学谐振腔设计

== 设计任务与指标分解

任务要求设计一个总腔长不超过 1 m 的两镜谐振腔，腔型为稳定腔，并且设计本身要有
工程可实现性，不能靠各种临界状态凑出来。把这三句话翻译成可以算、可以测的判据：

#figure(
  table(
    columns: (auto, 1fr, 1fr),
    inset: 6pt,
    stroke: 0.5pt + rgb("#c9ced3"),
    table.header[*指标*][*量化判据*][*本设计取值*],
    [总腔长 ≤ 1 m], [腔长 $L$ 本身，不含镜座], [$L =$ #R.L_mm mm（占上限 55%）],
    [稳定腔], [$0 < g_1 g_2 < 1$，且 $abs("半迹") < 1$], [$g_1 g_2 =$ #R.prod],
    [不依赖临界状态], [到最近稳定边界的腔长裕度；\
      失调容差；横模/纵模间隔比不接近简单分数], [腔长裕度 #R.margin_L_mm mm（#R.margin_L_rel_pct%）；\
      失调容差 #R.mis_tolerance_urad μrad；间隔比 #R.tm_ratio],
    [工程可实现], [标准口径与标准曲率半径；损耗可忽略；\
      普通两维镜架可调到共轴], [1 英寸球面介质膜镜，$R$ = #R.R1_mm / #R.R2_mm mm；\
      往返衍射损耗 #R.loss_total_rt],
  ),
  caption: [任务要求到可计算判据的翻译],
) <tab:criteria>

其中"不依赖临界状态"这一条最容易做虚，所以 下文"对照"一节专门用几个踩在临界状态上
的对照设计（近平面、近共焦、近同心、平行平面）做横向比较。

== 谐振腔结构与主要设计参数

=== 结构

采用共轴双凹（concave–concave）线性腔，两面镜子正入射工作，因此没有斜入射像散。
腔内放置一支 He-Ne 放电管作为工作介质示例（毛细孔径 Ø2 mm，管长 300 mm），
本设计的稳定性判据与介质无关。

#figure(
  image("figures/envelope.png", width: 100%),
  caption: [腔结构与基模高斯光束包络。镜面在图上画成竖线：$R = 800$ mm 的球面在
    通光半径 6.35 mm 内的矢高只有 28 μm，按 mm 尺度画是看不出的。],
) <fig:envelope>

#figure(
  table(
    columns: 4,
    inset: 6pt,
    stroke: 0.5pt + rgb("#c9ced3"),
    align: (left, right, left, right),
    table.header[*符号*][*数值*][*符号*][*数值*],
    [工作波长 $lambda$], [#R.lam_nm nm], [束腰半径 $w_0$], [#R.w0_um μm],
    [腔长 $L$], [#R.L_mm mm], [束腰位置（距 M1）], [#R.waist_from_M1_mm mm],
    [M1 曲率半径 $R_1$（凹）], [#R.R1_mm mm], [瑞利长度 $z_R$], [#R.zR_mm mm],
    [M2 曲率半径 $R_2$（凹）], [#R.R2_mm mm], [M1 面光斑 $w_1$], [#R.w1_um μm],
    [M1 镀膜], [HR，$T approx 0.2%$], [M2 面光斑 $w_2$], [#R.w2_um μm],
    [M2 镀膜], [OC，$T = 1.5%$], [远场全角（$1/e^2$）], [#R.full_angle_mrad mrad],
    [镜口径], [Ø25.4 mm（通光 Ø12.7 mm）], [纵模间隔 FSR], [#R.fsr_MHz MHz],
  ),
  caption: [主要设计参数（全部由本目录 cavity.py 算出）],
) <tab:params>

=== 镜面型式与规格的选择依据

*为什么用球面镜而不是抛物面镜。* 抛物面只对平行光严格无球差，而谐振腔里的本征模
是束腰在腔内的高斯光束。在光斑半径 $r = w_1$ = #R.w1_um μm 处，球面与抛物面的矢高差
约为 $r^4 / (8 R^3)$ = 6.3 × $10^-15$ m，比波长（6.3 × $10^-7$ m）小八个数量级，
对模式没有任何可测影响。球面镜研磨与检测成熟、价格低，是这里唯一合理的选择。

*为什么两面都用凹面。* 平面镜对应 $g = 1$。两面都平就是 $g_1 g_2 = 1$ 的平行平面腔，
严格落在稳定区边界上，模式由孔径而非腔型决定，失调灵敏度趋于无穷，属于任务明确
要求避免的临界状态。一面平面一面凹面（半球腔）虽然形式上稳定，但工作点紧贴
$g_1 = 1$ 这条边，腔内光斑对腔长的相对变化很敏感。

*为什么选 $R_1$ = 800 mm、$R_2$ = 1200 mm 这一组。* 三个约束同时满足：
（1）$g_1$、$g_2$ 同号且都不接近 0 或 1，工作点落在稳定区内部；
（2）束腰落在放电管内部（#R.waist_from_M1_mm mm 处，管占 100–400 mm），
增益区模斑小（#R.w0_um μm），有利于低阈值；
（3）横模/纵模间隔比 #R.tm_ratio 不接近任何简单分数，避免共焦型简并。
800 mm 与 1200 mm 都是厂商常备的标准曲率半径，不需要定制。

*口径。* 标准 1 英寸（Ø25.4 mm）镜的通光半径 6.35 mm，是腔内最大光斑的
#R.a_over_w_max 倍，对应菲涅耳数 #R.fresnel_N，衍射损耗完全可以忽略（见"孔径与衍射损耗"一节）。

== 稳定性的判断依据

本节用三种互相独立的语言给出同一结论：$g$ 参数（几何光学的紧凑判据）、往返光线
矩阵的本征值（光线是否发散）、复 $q$ 参数本征方程（模式是否存在）。

=== $g$ 参数判据

$ g_i = 1 - L / R_i , quad 0 < g_1 g_2 < 1 $

$ g_1 = 1 - frac(L, R_1) = #R.g1 , quad g_2 = 1 - frac(L, R_2) = #R.g2 , quad g_1 g_2 = #R.prod $ <eq:g>


代入 $L$ = #R.L_mm mm、$R_1$ = #R.R1_mm mm、$R_2$ = #R.R2_mm mm 得 @eq:g。
两条临界曲线分别是共焦 $g_1 g_2 = 0$ 与平行平面/同心 $g_1 g_2 = 1$；本设计到两条
边界的距离都是 #R.margin_prod，位于稳定区内部。

=== 往返光线矩阵判据

以"M1 反射后离镜"为参考面，傍轴光线矢量 $(y, h)^T$ 的往返矩阵为

$ M = mat(1, 0; -2/R_1, 1) mat(1, L; 0, 1) mat(1, 0; -2/R_2, 1) mat(1, L; 0, 1)
    = mat(#R.matA, #R.matB; #R.matC, #R.matD) , $

其半迹

$ frac(A + D, 2) = #R.half_trace = 2 g_1 g_2 - 1 , $

绝对值小于 1，说明 $M$ 的本征值是一对单位模的共轭复数 $e^(± i mu)$（数值上
$abs(lambda) =$ #R.rt_eig_abs），任意傍轴光线往返任意多次都不发散——这是"稳定腔"
在几何光学里的定义。同时 $mu = arccos(#R.half_trace) = #R.gouy_round$ rad，与下面
用高斯模算出的往返 Gouy 相移一致，两条路径互为校核。

=== 复 $q$ 参数与本征模

稳定性的第三种表述：自再现条件 $q = (A q + B) / (C q + D)$ 展开成

$ C q^2 + (D - A) q - B = 0 , $

其判别式 $(D - A)^2 + 4 B C = (A + D)^2 - 4$。当 $abs((A + D) / 2) < 1$ 时判别式为负，
方程在上半平面内给出共轭复根，即存在物理的高斯本征模；否则根落在实轴上，
只对应非稳腔的"掠射"解。脚本里就是按这个分支来判定稳定与否的。

=== 迭代证据：稳定与失稳的实际差别

#figure(
  image("figures/stability_demo.png", width: 84%),
  caption: [把 $q$ 映射 $q -> (A q + B) / (C q + D)$ 迭代 40 次。稳定腔里模斑半径在
    #R.qmap_w_lo_um–#R.qmap_w_hi_um μm 之间有界振荡（保形旋转，既不收缩也不发散）；
    同一副镜把腔长拉到 860 mm 使 $g_1 g_2 < 0$ 后，模斑在第 40 次已达
    #R.qmap_w_iter_max_unstable_mm mm，光束从腔侧壁逸出。],
) <fig:stabilitydemo>

#figure(
  image("figures/g_diagram.png", width: 78%),
  caption: [$g$-$g$ 稳定图（浅色为稳定区 $0 < g_1 g_2 < 1$）。本设计点在稳定区内部，
    离两条边界都远；对照设计分别贴着共焦边（$g_1 g_2 approx 0$）、平行平面角
    （$g_1 g_2 -> 1$）和同心边（$g_1 g_2 -> 1$，$g < 0$ 分支）。],
) <fig:gdiagram>

== 模式参数计算

=== 束腰、光斑与发散角

由 M1 面的本征 $q_1$ 得瑞利长度 $z_R$ = #R.zR_mm mm，进而

$ w_0 = sqrt(frac(lambda z_R, pi)) = #R.w0_um " μm" , quad
  w_i = w_0 sqrt(1 + (z_i / z_R)^2) , quad
  theta = frac(lambda, pi w_0) = #R.theta_mrad " mrad" , $

即 M1 面 #R.w1_um μm、M2 面 #R.w2_um μm，远场全角（$1/e^2$ 强度）#R.full_angle_mrad mrad。
束腰在 M1 之后 #R.waist_from_M1_mm mm 处，正好落在放电管毛细段内。

=== 与闭式公式互验

一般两镜腔的镜面光斑闭式解为

$ w_1^2 = frac(lambda L, pi) sqrt(frac(g_2, g_1 (1 - g_1 g_2))) , quad
  w_2^2 = frac(lambda L, pi) sqrt(frac(g_1, g_2 (1 - g_1 g_2))) , $

算得 $w_1$ = #R.w1_closed_um μm、$w_2$ = #R.w2_closed_um μm，与 $q$ 矩阵法结果的相对
偏差 #R.w1_closed_rel_err_ppm ppm。另外，本征模在两镜面的波前曲率半径与镜面曲率半径
的相对偏差为 #R.Rmatch_err_M1_rel 与 #R.Rmatch_err_M2_rel，即已到双精度浮点的机器
精度——模式与腔镜严格自洽。

=== 纵模与横模间隔

$ Delta nu_"FSR" = c / (2 L) = #R.fsr_MHz " MHz" , quad
  Delta nu_"TEM" = frac(Delta nu_"FSR", pi) arccos sqrt(g_1 g_2) = #R.tm_spacing_MHz " MHz" , $

横模间隔与纵模间隔之比 #R.tm_ratio。这个比值在共焦腔上恰为 1/2（横纵模简并，腔对
腔长抖动极敏感），在平行平面与同心腔上趋于 0。本设计与分母不超过 8 的最接近简单
分数 3/8 相差 0.010，对应约 2.7 MHz 的失谐，因此不存在低阶简并。

=== 波动光学复核

上面全部是傍轴矩阵语言的结果。为了确认它确实是波动光学的解，脚本另外做了一次独立
计算：把 $q$ 法给出的 M1 面场分布当作入射场，用柱对称 Fresnel 衍射积分（核里用
$J_0$ 解析掉方位角积分）传播到 M2，再按"反射即横向相位共轭"返回 M1，走完整一个
往返。结果：

- M2 面光斑半径与解析值相对偏差 #R.rt_w2_dev，波前曲率相对偏差 #R.rt_roc2_dev；
- 往返自再现度（归一化重叠积分）#R.rt_overlap_bare；
- 无孔阑时往返功率保持率的偏离 #R.rt_noise_floor 完全由网格离散化决定，
  它就是本套数值方法的本底。

=== 模式体积与增益区匹配

束腰落在管内，毛细段（100–400 mm）内束宽最大处为 #R.bore_w_max_um μm（出现在距 M1
#R.bore_z_worst_mm mm 处），基模在该段占据的直径不到 0.7 mm，对 Ø2 mm 的毛细不会形成
明显截断，同时模体积也足够小以利单模运转。

== 公差与鲁棒性

=== 腔长公差

#figure(
  image("figures/length_tolerance.png", width: 100%),
  caption: [左：$g_1 g_2$ 随腔长的变化，浅色为稳定区，本设计点在 $L$ = #R.L_mm mm，
    到共焦边界 $L = R_1$ = #R.L_stable_hi_mm mm 还有一大段距离。右：基模束腰与两镜
    光斑随腔长的变化，靠近边界时光斑急剧变大。],
) <fig:ltol>

固定 $R_1$、$R_2$ 时，$g_1 g_2 (L) = (1 - L/R_1)(1 - L/R_2)$ 是一条开口向上的抛物线，
零点在 $L = R_1$、$R_2$，等于 1 在 $L = 0$ 与 $L = R_1 + R_2$。因此这对镜子的稳定
工作区间是 $L in$ (#R.L_stable_lo_mm, #R.L_stable_hi_mm) mm 与 (1200, 2000) mm 两段；
本设计取 #R.L_mm mm，距最近边界 #R.margin_L_mm mm，相对裕度 #R.margin_L_rel_pct%。
几个具体的工程量：

- 装调时把腔长放缩 ±10 mm，M1 面光斑只从 #R.w1_at_Lm10mm_um μm 变到
  #R.w1_at_Lp10mm_um μm（±1.3%），不会突然变大；
- 铝间隔件热膨胀 23 μm/(m·K)，环境变化 10 K 引起 $Delta L$ = #R.dL_thermal_mm mm，
  $g_1 g_2$ 从 #R.prod 变到 #R.prod_at_thermal_drift；
- 若把 $g_1 g_2$ 的工程允许范围收紧到 [0.05, 0.95]，腔长仍可在
  #R.L_for_prod_0p95_mm–#R.L_for_prod_0p05_mm mm 内取值。

=== 曲率半径公差

球面镜曲率半径的出厂公差一般给 ±1%（干涉法检测）。把两镜 $R$ 各取 ±1% 组合，
$g_1 g_2$ 落在 #R.prod_Rtol_lo–#R.prod_Rtol_hi 之间，仍远离 0 与 1，模式参数变化在
百分之一量级。

=== 失调灵敏度

镜面绕自身顶点倾斜 $alpha$ 时，稳定腔的谐振轴会整体平移。用带常数项的仿射光线
映射求平衡光轴：$v^* = (I - M)^(-1) b$，其中 $b$ 由倾斜引入的 $2 alpha$ 角偏转累加
而成。本设计 M2 倾斜 100 μrad 时，谐振轴在 M1 面偏移 #R.mis100u_offset_M1_um μm、在
M2 面偏移 #R.mis100u_offset_M2_um μm；按"偏移不超过较小光斑 1/4"的工程判据，允许的
倾角为 #R.mis_tolerance_urad μrad（约 14″）。普通两维微调镜架（螺距 0.5 mm、200 格）
单格就是几 μrad，因此这个容差是可以稳定复现的。

#figure(
  image("figures/misalignment.png", width: 88%),
  caption: [失调灵敏度对比：同样引起 1/4 光斑的轴偏移，本设计需要
    #R.mis_tolerance_urad μrad，近平面腔只需 26.8 μrad，近同心腔只需 7.0 μrad。
    工作点越靠近 $g$-$g$ 图边界，曲线越靠左下，即越"怕碰"。],
) <fig:mis>

=== 对照：踩在临界状态上的设计长什么样

#figure(
  table(
    columns: (1fr, auto, auto, auto, auto, auto, auto),
    inset: 5pt,
    stroke: 0.5pt + rgb("#c9ced3"),
    table.header[*腔型*][*$g_1 g_2$*][*离边界*][*稳定*][*腔长裕度*][*束腰 $w_0$*][*失调容差*],
    ..COMPARISON.map(r => (r.name, r.prod, r.edge, r.stable, r.margin, r.w0, r.mis)).flatten(),
  ),
  caption: [对照设计与本设计的横向比较（同一波长、同一腔长 550 mm）],
) <tab:compare>

- *平行平面腔*：$g_1 g_2 = 1$，严格在边界上，$I - M$ 奇异，上面的轴偏移公式直接
  无解——这就是"依赖临界状态"的反面教材。
- *近共焦腔*（$R$ = 560 mm，只差 10 mm 就共焦）：腔长漂 10 mm 就出稳定区，而且
  横纵模间隔比趋于 1/2，出现共焦简并，对模式竞争极其敏感。
- *近同心腔*（$R$ = 280 mm）：束腰只有 86 μm，瑞利长度极短，任何一点倾斜或口径
  不洁净都会造成大损耗，失调容差只有 7 μrad。
- *近平面腔*（$R$ = 10 m）：模斑大、腔长裕度看似充裕，但失调容差最差之一
  （26.8 μrad），且对镜面口径要求高。

本设计在"腔长裕度"和"失调容差"两项上都取得可接受的值，且没有任何一项指标是靠
边界附近的巧合得到的。

== 孔径与衍射损耗

=== 高斯截断解析式

半径 $a$ 的圆光阑截断束宽 $w$ 的高斯光束，单次功率损耗为

$ delta_"clip" = "exp"(-2 a^2 / w^2) , $

这是对高斯强度分布严格积分的结果（不是近似），工程上常用 $a >= 3 w$ 作为"可忽略"
判据。本设计中：

- 两镜通光半径 6.35 mm，对最大光斑 #R.w2_um μm 的比值 #R.a_over_w_max，
  往返合计 #R.loss_mirror_rt，实际上为零；
- 放电管毛细 Ø2 mm 是腔内最紧的光阑。取管内束宽最大处 #R.bore_w_max_um μm，
  $a / w$ = 2.83，往返经过两次，合计 #R.loss_total_rt（约 0.00002%）。

#figure(
  image("figures/aperture_loss.png", width: 88%),
  caption: [往返截断损耗随光阑半径的变化。设计工作点（两镜 6.35 mm、毛细 1.0 mm）
    都在曲线左端的可忽略区；若误用 Ø1 mm 的管，损耗立刻升到 1% 量级。],
) <fig:aper>

=== 用衍射积分复核

把高斯截断式与完整的 Fresnel 往返传播放在一起比较：

- Ø2 mm 孔阑：数值往返损耗 #R.rt_loss_bore_num，与无孔阑时的数值本底 #R.rt_noise_floor
  同量级，说明真实损耗低于本套数值方法可分辨的下限，工程上取解析值
  #R.loss_total_rt 即可；
- 刻意收紧到 Ø1 mm 孔阑做定量校核：数值 #R.rt_loss_tight_num_pct%，解析式
  #R.rt_loss_tight_ana_pct%，同一量级、相差不到一倍。两者的差异正是高斯截断式
  忽略的衍射振铃与模式再整形，说明 $a / w$ 接近 1 时只能用数值方法，而本设计
  工作在 $a / w$ = 2.8 以上，解析式足够可靠。

=== 损耗预算与输出耦合

往返损耗 = 输出耦合 $T$ + 衍射 #R.loss_total_rt + 镀膜吸收散射（优质介质膜单面
< 0.1%，两镜合计约 0.2%）≈ 1.7%。He-Ne 在 632.8 nm 的小信号增益文献典型值为
1.5–4 %/m，本设计 300 mm 管对应往返增益 0.9–2.4%，与 1.5% 的输出耦合在同一量级，
因此 $T$ = 1.5% 是合理起点（*这一项是量级校核，不是稳定性判据*）。若实测增益偏低，
把输出镜换成 $T$ = 1% 即可：换镜只改镀膜，不改曲率半径，因此上面所有稳定性与模式
结论保持不变——这正是把"稳定性"与"增益匹配"分开设计的好处。

== 工程实现与验收

=== 元件与结构

#figure(
  table(
    columns: (auto, 1fr, auto, 1fr),
    inset: 6pt,
    stroke: 0.5pt + rgb("#c9ced3"),
    table.header[*项目*][*规格*][*数量*][*说明*],
    [M1 高反镜], [Ø25.4 mm，凹球面 $R$ = 800 mm ± 1%，632.8 nm HR ≥ 99.8%], [1], [介质膜，膜面朝向腔内],
    [M2 输出镜], [Ø25.4 mm，凹球面 $R$ = 1200 mm ± 1%，$T$ = 1.5% ± 0.3%], [1], [与 M1 同批镀膜],
    [镜架], [两维微调（俯仰/偏摆）+ 锁紧], [2], [细牙螺距 0.5 mm 级，单格约几 μrad],
    [腔长调节], [M1 装在一维平移台上], [1], [行程 ≥ 20 mm，用于调腔长与扫描],
    [基座], [铝型材或双杆殷钢支架，两镜共轴基准], [1], [550 mm 跨距，等高],
    [放电管], [管长 300 mm，毛细 Ø2 mm，布儒斯特窗], [1], [稳定性判据与介质无关],
    [隔振], [光学平台 + 气浮], [—], [腔长噪声主要来源，见风险一节],
  ),
  caption: [元件清单（尺寸与公差都在市售标准件范围内）],
) <tab:bom>

结构上两个要点：其一，两面镜子都装在同一个刚性基座上，腔长由基座跨距而非镜架
螺帽决定，镜架只做角度调整，这样角度与长度两个自由度解耦；其二，M1 用一维平移台
支撑，既能在 400–600 mm 内找腔长，也便于后面做腔长扫描测量。

=== 装调步骤

1. 按 @tab:params 把两镜大致放在 550 mm 跨距上，用直尺与准直激光粗定共轴；
2. 以一支独立准直 He-Ne 激光沿腔轴入射，用"剪刀法"（自准直）调节 M1、M2 的俯仰与
   偏摆，使两面镜子的背反射光点都回到激光器出射孔附近——此时腔轴与镜轴重合，
   正入射，没有像散；
3. 接通放电管，把 M1 平移台从 400 mm 缓慢拉长，输出光首次出现亮斑；继续微调 M2
   角度，远场环数减少，直到只剩高斯近场分布（$"TEM"_00$ 模）；
4. 锁紧所有镜架，复测远场，记录输出功率与光斑尺寸。

第 3 步"从 400 mm 拉长找光"本身就是稳定性判据的现场演示：本设计的稳定区间是
$L in (0, #R.L_stable_hi_mm)$ mm，从任意短腔长拉过来都会遇到输出，不存在"要精确
跳到某一段才起振"的情况。

=== 验收测量（把设计值变成可核对的数）

#figure(
  table(
    columns: (1fr, auto, 1fr),
    inset: 6pt,
    stroke: 0.5pt + rgb("#c9ced3"),
    table.header[*可测量*][*设计预期*][*测量方法*],
    [纵模间隔 FSR], [#R.fsr_MHz MHz], [扫描 F-P 干涉仪（自由光谱程 1.5 GHz）测相邻纵模间距，反算 $L = c / (2 Delta nu)$],
    [束腰半径 $w_0$], [#R.w0_um μm], [刀口法或 CCD 光束分析仪沿轴多点取 $w (z)$，按 $w^2 (z) = w_0^2 (1 + ((z - z_0) / z_R)^2)$ 拟合],
    [远场全角], [#R.full_angle_mrad mrad], [距输出 1.0 m 处测 $1/e^2$ 光斑直径（约 1.42 mm）],
    [瑞利长度 $z_R$], [#R.zR_mm mm], [同上拟合的直接输出，可与 $w_0$ 交叉校核],
    [稳定区边界], [$L$ = #R.L_stable_hi_mm mm 处熄灭], [把 M1 拉到 800 mm 附近，观察输出急剧衰减直至消失，即共焦边界],
    [失调容差], [#R.mis_tolerance_urad μrad], [有意倾斜 M2 至输出功率降到一半，由镜架格数反推角度],
  ),
  caption: [验收测量方案：每一项都对应本报告里的一个计算结果],
) <tab:accept>

前四项之间只有一个自由参数（腔长），所以只要 @tab:accept 里的 FSR 与 $w_0$ 拟合值
同时符合，就等价于独立复核了整套模式计算。

=== 风险与对策

- *热*：铝基座 1 K 温漂约 0.013 mm，对应 $g_1 g_2$ 变化 $10^-5$ 量级，远小于裕度；
  若需长期稳功率，可把基座换成殷钢（2 μm/(m·K)）。
- *机械*：腔长裕度 250 mm 意味着镜架蠕变、锁紧回弹都不会把腔推出稳定区，这是
  "不依赖临界状态"带来的直接好处。
- *污染*：毛细内的溅射沉积会形成附加光阑。按 @fig:aper，只要等效孔径不缩到
  Ø1 mm 以下，衍射损耗仍低于 1%；限制放电电流与定期检漏可控制。
- *镜半径偏差*：±1% 的 $R$ 公差已在上面的公差分析里覆盖。

== 结论

设计的谐振腔为共轴双凹稳定腔：$lambda$ = #R.lam_nm nm，$L$ = #R.L_mm mm（不超过 1 m
上限的 55%），$R_1$ = #R.R1_mm mm（HR），$R_2$ = #R.R2_mm mm（OC，$T$ = 1.5%）；
基模束腰 #R.w0_um μm 位于 M1 后 #R.waist_from_M1_mm mm 处，两镜光斑
#R.w1_um / #R.w2_um μm，远场全角 #R.full_angle_mrad mrad，FSR = #R.fsr_MHz MHz，
横模间隔 #R.tm_spacing_MHz MHz。

对照任务要求逐条回答：

1. *总长不超过 1 m*：$L$ = #R.L_mm mm，加上两个镜架后总跨距约 620 mm。
2. *稳定腔*：$g_1 g_2$ = #R.prod ∈ (0, 1)，且 $abs((A + D) / 2)$ = #R.half_trace < 1，
   往返矩阵本征值单位模（#R.rt_eig_abs）；$q$ 映射迭代 40 次模斑有界
   （#R.qmap_w_lo_um–#R.qmap_w_hi_um μm）；波动光学往返传播自再现度 #R.rt_overlap_bare。
3. *工程可实现、不依赖临界状态*：全部元件为市售标准件；腔长裕度 #R.margin_L_mm mm
   （#R.margin_L_rel_pct%）、失调容差 #R.mis_tolerance_urad μrad、往返衍射损耗
   #R.loss_total_rt、无低阶横模简并（间隔比 #R.tm_ratio）。几个踩边界的对照设计
   说明这些余度不是天然就有的，而是选择 $R_1$、$R_2$ 的结果。

== 复算指引

报告里的每个数字都可以由本目录的脚本重算：

```bash
cd resonator_design
uv run cavity.py                   # 算模式、画 figures/、生成 summary.typ
typst compile --root . report.typ  # 排版本报告
```

`cavity.py` 用 PEP 723 内联依赖声明，`uv` 会自动准备 numpy / matplotlib / scipy；
没有 `uv` 时 `python3 cavity.py` 亦可（需自行安装这三个包）。报告排版使用
`@local/simple-zjut-report` 模板包，模板不在本目录复制，挂载方式见
`../scripts/typst-local-packages.sh`。

#figure(
  table(
    columns: (auto, 1fr),
    inset: 6pt,
    stroke: 0.5pt + rgb("#c9ced3"),
    table.header[*文件*][*作用*],
    [cavity.py], [全部计算：ABCD 矩阵、本征 $q$、$g$ 判据、公差、失调、Fresnel 往返],
    [summary.typ], [脚本生成的结果字典，report.typ 直接引用，避免手抄错数],
    [results.json], [同一批结果的机器可读版本，便于逐项核对],
    [figures/], [六张图（@fig:envelope 至 @fig:mis），由脚本重绘],
    [report.typ], [本报告源文件],
  ),
  caption: [文件清单],
)

不想跑脚本时，也可以手算三条独立校核：（1）用 $g_i = 1 - L / R_i$ 手算 $g_1 g_2$，
确认落在 (0, 1) 内部；（2）用 $2 g_1 g_2 - 1$ 核对往返矩阵半迹 #R.half_trace；
（3）用 @tab:params 的 $w_0$、$z_R$ 与 $w_i = w_0 sqrt(1 + (z_i / z_R)^2)$ 核对两镜
光斑，其中 $z_1$ = #R.waist_from_M1_mm mm、$z_2 = L - z_1$。三条都对上，说明腔型与
模式参数自洽。

核心代码摘录（完整实现见 `cavity.py`）：

```python
def rt_matrix(L, R1, R2):
    P = np.array([[1.0, L], [0.0, 1.0]])
    M1 = np.array([[1.0, 0.0], [-2.0 / R1, 1.0]])
    M2 = np.array([[1.0, 0.0], [-2.0 / R2, 1.0]])
    return M1 @ P @ M2 @ P          # 参考面：M1 反射后离镜


def eigen_q(M, lam):
    """q = (A q + B)/(C q + D)  ->  C q^2 + (D-A) q - B = 0，取 Im(q) > 0 的根。"""
    A, B, C, D = M[0, 0], M[0, 1], M[1, 0], M[1, 1]
    disc = (D - A) ** 2 + 4.0 * B * C
    if disc >= 0.0:
        return None                  # 非稳腔：不存在复的本征 q
    sq = math.sqrt(-disc)
    return next(q for q in (((A - D) + 1j * sq) / (2 * C),
                            ((A - D) - 1j * sq) / (2 * C)) if q.imag > 0)


def axis_offset(L, R1, R2, alpha1=0.0, alpha2=0.0):
    """镜面倾斜 -> 谐振轴平移：解 v* = (I - M)^(-1) b（b 累加 2*alpha 角偏转）。"""
    P = (np.array([[1.0, L], [0.0, 1.0]]), np.zeros(2))
    Ref1 = (np.array([[1.0, 0.0], [-2.0 / R1, 1.0]]), np.array([0.0, 2.0 * alpha1]))
    Ref2 = (np.array([[1.0, 0.0], [-2.0 / R2, 1.0]]), np.array([0.0, 2.0 * alpha2]))
    M, b = compose(Ref1, compose(P, compose(Ref2, P)))
    v = np.linalg.solve(np.eye(2) - M, b)
    return v[0], v[0] + v[1] * L     # 平衡光轴在 M1 / M2 面的横向位置
```
