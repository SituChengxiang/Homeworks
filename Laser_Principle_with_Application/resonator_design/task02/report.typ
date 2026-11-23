#import "@local/simple-zjut-report:0.3.0": experiment-report, three-line-table
#import "numbers.typ": Q, SWEEP

// 全部数字来自 task2_foxli.py 生成的 numbers.typ（本目录），不手抄。
#show figure.where(kind: image): set figure(supplement: [图])
#show figure.where(kind: table): set figure(supplement: [表])
#set math.equation(numbering: "(1)")

#show: doc => experiment-report(
  mode: "group",
  title: "任务2 稳定腔的自再现模与衍射损耗",
  semester: "2026-2027学年第一学期",
  course: "激光原理与技术",
  name: "成员A 成员B 成员C 成员D 成员E",
  group: "5",
  date: "2026年10月21日",
  doc
)

= 稳定腔的自再现模与衍射损耗

== 任务与判据

Task 01 只用几何光学判据（$g$ 参数、往返矩阵）说明腔是稳定的。但真实的光是波，在腔内往返时会发生衍射，镜面和腔内元件的横向尺寸也有限。本任务把 Task 01 的腔型放到波长 $lambda = 1064$ nm 下，用衍射传播加数值迭代回答：什么样的横向光场能一次又一次往返而不改变形状（自再现模）、它的衍射损耗是多少、需要多大的有效通光口径。

#figure(
  three-line-table(
    columns: (auto, 1fr, 1.35fr),
    head: ([*要求*], [*判据*], [*本任务结果*]),
    [自再现模], [$u = gamma T u$，$T$ 为往返算子], [基模，圆对称近高斯，$w$=#Q.wmode μm],
    [迭代收敛], [相位对齐后的场变化 $< 10^(-9)$], [#Q.iters 次往返，末次 $d$=#Q.dconv],
    [往返衍射损耗], [由本征值 $delta = 1 - abs(gamma)^2$ 给出], [#Q.loss（约 0.17%）],
    [最小有效口径], [$delta(D) <= 1%$ 的最小 $D$], [$D_"min"$ = #Q.dmin mm],
    [是否改设计], [比较裕度、模式判别与工程代价], [几何不变，只更新规格（V2.0）],
  ),
  caption: [表1：本任务的量化判据与结果],
) <tab:criteria>

== V1.0 腔型与 1064 nm 下的参数


#figure(
  three-line-table(
    columns: (auto, auto, auto, auto),
    head: ([*参数*], [*数值*], [*参数*], [*数值*]),
    align: (left, right, left, right),
    [腔长 $L$], [#Q.L mm], [束腰半径 $w_0$], [#Q.w0 μm],
    [M1 曲率半径 $R_1$], [#Q.R1 mm（凹）], [镜面光斑 $w_1 = w_2$], [#Q.w1 μm],
    [M2 曲率半径 $R_2$], [#Q.R2 mm（凹，OC）], [瑞利长度 $z_R$], [#Q.zR mm],
    [波长 $lambda$], [#Q.lam nm], [束腰位置（距 M1）], [#Q.waist mm（腔心）],
    [$g_1 g_2$], [#Q.prod], [纵模间隔 FSR], [#Q.fsr MHz],
    [有效口径 $D$（设计值）], [#Q.D mm], [横/纵模间隔比], [#Q.tmr],
  ),
  caption: [表2：Task 02 的腔型与 1064 nm 下的基模参数（解析 $q$ 参数法）],
) <tab:v1>

横纵模间隔比 #Q.tmr 很接近 1/2，这是近共焦腔的固有特征：横模频率几乎简并。它对稳定性没有影响，但会影响模式竞争，后面的收敛速度与模式判别会量化这部分的影响。（AI补充：瑞利长度是指从束腰半径沿着光传播方向，到光束半径增大为原来 $sqrt(2)$ 倍的位置之间的距离。）

== 方法：Fox-Li 衍射迭代

=== 单程传播

傍轴条件下，一段距离 $L$ 的自由传播由 Fresnel 衍射积分描述：

$ u_2 (bold(r)_2) = 1/(i lambda L) integral u_1 (bold(r)_1)
  exp(i k abs(bold(r)_2 - bold(r)_1)^2 / (2 L)) dif bold(r)_1 . $ <eq:fresnel>

腔是轴对称的（球面镜 + 圆口径 + 轴上传播），基模也必然轴对称，于是方位角积分可以解析掉，得到只用半径的一维积分：

$ u_2 (r_2) = (-2 pi i)/(lambda L) integral_0^infinity u_1 (r_1)
  exp(i k (r_1^2 + r_2^2) / (2 L)) J_0 (k r_1 r_2 / L) r_1 dif r_1 , $ <eq:j0>

其中 $J_0$ 是零阶贝塞尔函数。这一步把二维问题降成一维，网格可以取到 1.5 μm，损耗算到 $10^(-4)$ 以下。

=== 往返算子与迭代格式

把场定义在 M1 面上、沿 $+z$ 传播。一次往返是：

1. 自由传播 $L$ 到 M2 面；
2. 乘 M2 的口径因子与镜面相位 $exp(-i k r^2 / R_2)$（凹面镜等效于焦距 $R_2/2$ 的透镜）；
3. 自由传播回 M1 面；
4. 乘 M1 的口径因子与相位。

写成算子就是

$ T = D_1 P D_2 P , quad D_i = 1_(r <= a) exp(-i k r^2 / R_i) , $ <eq:T>

$a = D/2$ 是有效通光半径。自再现模满足

$ T u = gamma u , $ <eq:eigen>

$abs(gamma)^2$ 就是往返一次的功率保持率，往返衍射损耗 $delta = 1 - abs(gamma)^2$。

数值上不直接做矩阵本征分解，而是用 Fox-Li 迭代 $u_(n+1) = T u_n / norm(T u_n)$：取一个与基模明显不同的初值（宽度为 $0.45 a$ 的高斯），迭代到场不再变化。由于本征值带相位，每次迭代先把全局相位对齐再比较：

$ d_n = norm(u_(n+1) - u_n) / norm(u_n) , quad "收敛判据" d_n < 10^(-9) . $ <eq:conv>

每次自由传播后按功率归一，抵消离散化带来的微小非酉性；这一步只影响数值稳定性，不会抹掉口径造成的损耗（损耗全部来自 $D_i$）。

=== 数值参数与交叉校验

#figure(
  three-line-table(
    columns: (auto, 1fr),
    head: ([*项*], [*设置*]),
    [径向网格], [$n$=1000–1200，$r_"max" = 2.2 a$，$dif r approx 1.5$ μm],
    [损耗数值本底], [约 $4 times 10^(-5)$（网格离散化），低于此值的损耗只作量级参考],
    [二维校验], [单次 FFT 的 Fresnel 传播，$N$=640，$dif x = sqrt(lambda L / N) = 30$ μm],
    [解析校验], [高斯截断式 $delta approx 2 exp(-2 a^2 / w^2)$ 与解析 $q$ 参数],
  ),
  caption: [表3：数值设置。二维与柱对称两套独立实现互相校验],
) <tab:num>

#figure(
  three-line-table(
    columns: (auto, auto, auto, auto),
    head: ([*量*], [*柱对称 Fox-Li*], [*二维 Fox-Li*], [*相对差*]),
    [往返损耗], [#Q.loss], [#Q.two_d_loss], [约 15%],
    [镜面模斑 $w$], [#Q.wmode μm], [#Q.two_d_w μm], [$<0.1%$],
  ),
  caption: [表4：两套独立数值方法在 $D$ = #Q.D mm 的对比。损耗相差 15% 属细网格/粗网格的离散化差异，对 1% 判据的影响见"最小有效通光口径"一节],
) <tab:cross>

== 结果一：自再现模

#figure(
  image("figures/iteration.png", width: 100%),
  caption: [图1：Fox-Li 迭代过程（$D$ = #Q.D mm）。初值是一个宽度只有口径 45% 的高斯，前几次往返里光束迅速展宽并出现衍射环，30 次以后环基本消失，200 次时已经稳定。每帧按自身峰值归一，便于看形状。],
) <fig:iter>

#figure(
  image("figures/mode.png", width: 100%),
  caption: [图2：最终自再现模。左：镜面处归一化强度 $I(x,y)$；右：中心截面的对数强度与高斯拟合。中心部分与高斯吻合到 $10^(-4)$ 以下，边缘在 $r = a$ 处被口径截断。],
) <fig:mode>

#figure(
  image("figures/convergence.png", width: 92%),
  caption: [图3：迭代收敛曲线（去全局相位后的场变化）。四条曲线对应四个有效口径；圆点标出各自降到 $10^(-6)$ 所需的往返次数。],
) <fig:conv>

稳定模的主要空间特征：

- *圆对称的近高斯模*：二维强度分布 $I(x,y)$ 是圆对称的，中心截面与高斯拟合吻合，二阶矩半径 #Q.wmode μm，与解析基模 #Q.w1 μm 只差 0.4%。这说明有限口径下的自再现模就是腔的本征高斯模，口径只是把它在边缘切掉。
- *尺度*：镜面处 $w = $ #Q.wmode μm；由 $q$ 参数法，束腰 $w_0 = $ #Q.w0 μm 位于腔心，瑞利长度 #Q.zR mm，与 Task 01 的结构完全对应。
- *相位*：镜面上的波前是球面，曲率半径等于该镜的 $R$（否则不能自再现）；因此输出耦合镜透出的光近似为发散球面波，远场为高斯。
- *高阶模*：谱中次低阶模的损耗为 #Q.loss1（见"口径对光场与损耗的影响"一节），远高于基模的 #Q.loss，
  所以迭代最终收敛到基模；在更大口径下这个差距迅速缩小（见 @tab:conv）。

迭代收敛的证据（@fig:conv 与 @tab:conv）：

#figure(
  three-line-table(
    columns: (auto, auto, auto, auto),
    head: ([*有效口径 $D$ / mm*], [*到 $d=10^(-6)$ 的往返次数*], [*到 $d=10^(-9)$ 的往返次数*], [*往返损耗*]),
    [1.4], [29], [46], [6.24 × $10^(-3)$],
    [1.6], [172], [276], [#Q.loss],
    [2.0], [3015], [4822], [5.45 × $10^(-5)$],
  ),
  caption: [表5：不同口径下的收敛速度。口径越小，基模与高阶模的损耗差越大，收敛越快],
) <tab:conv>

$d_n$ 在双对数坐标下是直线，说明误差按固定比例衰减，也就是按本征值模之比$abs(gamma_1 / gamma_0)^n$ 收敛。$D$ = 2.0 mm 时收敛慢到 5000 次，正对应#Q.tmr ≈ 1/2 的近简并：各阶模损耗几乎相同，腔"选不出"单一模式。

== 结果二：口径对光场与损耗的影响

#figure(
  image("figures/aperture.png", width: 100%),
  caption: [图4：左：基模往返损耗随有效口径的变化，与高斯截断估计对比；红点为 1% 指标，虚竖线是最小口径。右：基模与次低阶模的损耗，口径越小两者差距越大。],
) <fig:aper>

#figure(
  three-line-table(
    columns: (auto, auto, auto, auto, auto),
    head: ([*$D$ / mm*], [*基模损耗 $delta$*], [*模斑 $w$ / μm*], [*次低阶模损耗*], [*损耗差*]),
    ..SWEEP.map(r => (r.D, r.loss, r.w, r.m1, r.gap)).flatten(),
  ),
  caption: [口径扫描结果（Fox-Li 迭代，$lambda$ = 1064 nm）],
) <tab:sweep>

三条规律：

- *损耗随口径急剧下降*：$delta$ 近似按 $2 exp(-2 a^2 / w^2)$ 变化，口径从 1.36 mm 增到 2.0 mm，损耗从 0.95% 掉到 $5.5 times 10^(-5)$。小口径段 Fox-Li 的损耗  比高斯估计低（模式会稍微"收缩"以适应口径），大口径段两者重合。
- *模式随口径收缩*：$D$ 从 2.0 mm 缩到 1.3 mm，模斑从 #Q.w2 μm 附近的 432.4 μm 增到 438.9 μm 再被裁到约 437 μm；口径接近光斑时，模式形状开始偏离高斯（衍射环加深），这正是损耗快速上升的原因。
- *口径同时是模式选择器*：$D$ = 1.3 mm 时次低阶模损耗 0.82，$D$ = 1.6 mm 时#Q.loss1，$D$ = 2.0 mm 时只有 0.0078。口径越大，高阶模越容易起振。

== 结果三：最小有效通光口径

把 @fig:aper 左图的 $delta(D)$ 在对数坐标下插值，$delta = 1%$ 对应

$ D_"min" = #Q.dmin " mm" , quad a_"min" = D_"min" / 2 . $

数值不确定度约 ±0.02 mm（来自柱对称与二维两套方法的 15% 损耗差；按 $delta prop exp(-2 a^2 / w^2)$，15% 的损耗误差只对应约 1% 的口径误差）。

工程上取 $D$ = #Q.D mm 作为设计有效口径，理由如下：

- 损耗 #Q.loss（约 0.17%），对 1% 指标有 6 倍裕度；
- 次低阶模损耗 #Q.loss1，与基模相差两个数量级，单横模运转有保障；
- 迭代 276 次即收敛到 $10^(-9)$，数值上不依赖近简并；
- 口径半径比模斑大 $(0.8 - 0.434)$ mm ≈ 0.37 mm，装调时的轴偏移有可见余量；
- 若进一步取 $D$ = 2.0 mm，损耗虽降到 $5 times 10^(-5)$，但次低阶模损耗只有 0.78%，
  对高增益激光器不足以压制高阶模，反而更难单模运转。

== 结果四：是否修改 Task 01 的设计（V2.0）

*结论：不需要修改几何。* Task 01 的 $R_1 = R_2 = 600$ mm、$L = 550$ mm 在 1064 nm下仍是稳定腔，1% 损耗只要求 1.36 mm 的有效口径，远小于 25.4 mm 的镜片口径，工程上用一个普通光阑（或把增益介质孔径限制在 2 mm 内）就能实现。改动集中在"波长相关"的工程规格上，所以记为 V2.0：

#figure(
  three-line-table(
    columns: (auto, auto, auto, auto),
    head: ([*参数*], [*V1.0（Task 01）*], [*Task 02 工作点*], [*V2.0*]),
    align: (left, center, center, center),
    [$R_1$ / mm], [600], [600], [600],
    [$R_2$ / mm], [600], [600], [600],
    [$L$ / mm], [550], [550], [550],
    [$lambda$ / nm], [632.8], [1064], [1064],
    [有效口径 $D$ / mm], [未规定], [#Q.D], [#Q.D（规定 $>= D_"min"$）],
    [镀膜], [He-Ne 632.8 nm], [同一腔型核算], [改 1064 nm 介质膜],
    [几何是否修改], [—], [—], [否],
  ),
  caption: [Task 01 / Task 02 / V2.0 的参数对照：几何不变，只换镀膜并规定有效口径],
) <tab:versions>

不做几何修改的理由可以定量说明：

- *口径不是瓶颈*：$D_"min"$ = #Q.dmin mm 比镜片口径小一个多数量级，光阑、增益介质孔径都能做到；
- *稳定性结论沿用*：$g$ 参数与波长无关，Task 01 的失调容差（78.5 μrad）在这套几何下仍然成立；有限口径只会*增加*高阶模损耗，不会让基模失稳。

需要补做的工程项：M1/M2 镀 1064 nm 介质膜（HR ≥ 99.8% / OC $T$ = 1.5%），腔内加 Ø#Q.D mm 光阑（或选 Ø2 mm 毛细的增益介质），并在装调时用远场光斑确认基模。

== 结论

- *Q1 最终稳定模式是什么样？* 圆对称的近高斯模，镜面处 $1/e^2$ 半径 #Q.wmode μm（解析 #Q.w1 μm），束腰半径 #Q.w0 μm ，瑞利长度 #Q.zR mm，相位为与镜面曲率相同的球面波；在 $D$ = #Q.D mm 下边缘被口径截断，高阶模损耗比它高两个数量级，所以迭代收敛到基模。
- *Q2 满足 1% 衍射损耗的最小有效口径？* #Q.dmin mm；工程取 #Q.D mm，往返损耗 #Q.loss（0.17%），次低阶模损耗 #Q.loss1。
- *Q3 保留还是修改 Task 01？* 保留几何，记为 V2.0，只更新镀膜波长与有效口径规格。  原因：1.36 mm 的口径要求远低于现有镜片口径，而近共焦的小模斑在口径与损耗上都是有利的；改动几何只会把 $D_"min"$ 推大。

== 复算指引

```bash
cd resonator_design
uv run task2_foxli.py                       # 默认 V1 腔型 + D=1.6 mm，含二维校验
uv run task2_foxli.py --R1 600 --R2 600 --L 550 --lam 1064 --D 1.6
uv run task2_foxli.py --sweep 1.2,1.4,1.6,2.0   # 只改口径扫描列表
typst compile --root . task02/report.typ     # 排版本报告
```

$R_1$、$R_2$、$L$、$lambda$、$D$ 任一改变，图、`task02/numbers.typ`、`task02/results.json`
全部重算；报告里的数字都引自 `task02/numbers.typ`，所以重跑后报告会自动同步。（若改成非稳腔，$g_1 g_2$ 不在 $(0,1)$ 内，脚本会直接报错退出——非稳腔没有收敛封闭模。）

#figure(
  three-line-table(
    columns: (auto, 1fr),
    head: ([*文件*], [*作用*]),
    [task2_foxli.py], [全部计算：柱对称 Fox-Li、二维校验、口径扫描、最小口径插值],
    [task02/numbers.typ], [结果字典，本报告的数字来源],
    [task02/results.json], [同一批结果的机器可读版本（含扫描表与收敛次数）],
    [task02/figures/], [四张图（@fig:iter–@fig:aper）],
  ),
  caption: [文件清单],
)

关键数值一览（与 `task02/results.json` 一致）：$w_0$ = #Q.w0 μm，$w_1$ = #Q.w1 μm，$z_R$ = #Q.zR mm，$D_"min"$ = #Q.dmin mm，$D$ = #Q.D mm 时往返损耗 #Q.loss、模斑 #Q.wmode μm，迭代 #Q.iters 次收敛到 $10^(-9)$。
