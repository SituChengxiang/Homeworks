#import "@local/simple-zjut-report:0.1.1": experiment-report, three-line-table
#import "summary_task3.typ": Q, FLAT, POS, THERM, PUMP

// 全部数字来自 task3_gauss.py 生成的 summary_task3.typ，不手抄。
#show figure.where(kind: image): set figure(supplement: [图])
#show figure.where(kind: table): set figure(supplement: [表])
#set math.equation(numbering: "(1)")

#show: doc => experiment-report(
  title: "任务3 腔内高斯光束、增益晶体位置与泵浦光斑匹配",
  semester: "2026-2027学年（一）学期",
  class: "激光原理与技术",
  name: "成员A 成员B 成员C 成员D 成员E",
  student-id: "10",
  date: "2026.11.02",
  doc
)

= 腔内高斯光束、增益晶体与泵浦匹配

== 任务与判据

Task 01 用几何光学判据把腔定下来（$R_1 = R_2$ = #Q.R1 mm、$L$ = #Q.L mm），Task 02 用衍射迭代证明腔里能存住一个近高斯的自再现模（镜面光斑 #Q.foxw μm，往返损耗 #Q.lossfox）。但"腔里有模"还不等于"有激光器"：必须知道纵向光场到底怎么分布——光束在哪里最细、在不同位置多粗、晶体该占哪一段、泵浦要聚焦到什么尺寸，才能把增益介质放进腔里并用泵浦把能量注进去。本任务用高斯光束完整描述 Task 02 的腔，并据此给出晶体与泵浦的工程参数。工作波长仍取 #Q.lam nm，有效通光口径沿用 Task 02 的 #Q.D mm。

#figure(
  three-line-table(
    columns: (auto, 1.05fr, 1.35fr),
    head: ([*作业要求*], [*方法*], [*本任务结果*]),
    [1 等价共焦腔], [等相位面与镜面曲率匹配；闭式解与 $q$ 法互验；正反演闭环],
      [共焦参数 $f$ = #Q.fnum mm；$L_"eq" = R_"eq" = 2 f$ = #Q.Leq mm；两镜等效坐标 $z_1$ = #Q.z1 mm、$z_2$ = + #Q.d2 mm],
    [2 束腰位置与尺寸], [$q_1$ 的实部与虚部分别给出束腰位置与瑞利长度],
      [$w_0$ = #Q.w0 μm，距 M1 #Q.d1 mm（腔心）],
    [3 腔内包络与光束参数], [$w(z)$、$R(z)$、$z_R$、发散角、古伊相移、模体积],
      [两镜光斑 #Q.w1 μm；半发散角 #Q.th1 mrad；往返古伊 #Q.gouyrt rad],
    [4 验证 Task 02], [同一截面用三种"光斑半径"定义 + 剖面 + 相位 + 损耗对照],
      [二阶矩差 #Q.foxdev\%；往返相位差 #Q.phdiff rad（0.05\%）],
    [5 晶体位置与尺寸], [束腰平坦度、口径裕度、载入平板与热透镜复核],
      [Nd:YAG #Q.dop at.\%、Ø #Q.rod × #Q.Lc mm，中心与束腰重合（距 M1 #Q.zin~#Q.zout mm）],
    [6 泵浦匹配], [重叠因子 $xi$ 与吸收加权；聚焦光组],
      [$w_p$ = #Q.wp μm（= #Q.ratio $w_0$），泵浦束腰在晶体输入面，加权 $xi$ = #Q.xibar],
  ),
  caption: [表1：六项要求的做法与量化结果],
) <tab:criteria>

== 等价共焦腔（要求 1）

=== 定义、公式与对应关系

教材（周炳琨《激光原理》）的处理是：任一稳定球面腔都唯一对应一个*等价共焦腔*，它的行波场有两个等相位面与实际腔的两个镜面重合，二者腔模完全相同。符号约定取教材口径——高斯光束的共焦参数

$ f = pi w_0^2 / lambda = z_R $,

即数值上等于瑞利长度；对称共焦腔同时满足 $L_"eq" = R_"eq" = 2 f$（每镜焦距 $R_"eq" / 2 = f$，两镜焦点重合于腔中心）。

设束腰到 M1、M2 的距离为 $d_1$、$d_2$（$d_1 + d_2 = L$），坐标原点取束腰、$+z$ 由 M1 指向 M2，则光束的等相位面曲率半径为 $R(z) = z + f^2 / z$。镜面必须坐在等相位面上，故

$ R_i = d_i + f^2 / d_i , quad i = 1, 2 . $ <eq:selfrepro>

两式相等并代入 $d_1 + d_2 = L$ 解得

$ d_1 = L (R_2 - L) / (R_1 + R_2 - 2 L) , quad
  d_2 = L (R_1 - L) / (R_1 + R_2 - 2 L) , quad
  f^2 = d_1 (R_1 - d_1) = d_2 (R_2 - d_2) . $ <eq:closed>

@eq:inverse 是同一映射的反演，说明 $(R_1, R_2, L)$ 与 $(f, d_1, d_2)$ 一一对应：

$ R_i = d_i + f^2 / d_i , quad L = d_1 + d_2 . $ <eq:inverse>

于是"与原来的 $(R_1, R_2, L)$ 有什么对应关系"可以说得很干脆：*几何腔长 $L$ 并不映射为等效腔长*；映射为等效腔长的是波前曲率（$2f$），而 $L$ 只决定两镜在等效坐标里的位置 $z_1 = -d_1$、$z_2 = +d_2$。

=== 本设计的等价共焦腔参数

#figure(
  three-line-table(
    columns: (auto, auto, 1fr),
    head: ([*等价共焦腔量*], [*数值*], [*说明*]),
    [共焦参数 $f$（= 瑞利长度 $z_R$）], [#Q.fnum mm], [由 @eq:closed 求出，与本征 $q_1$ 的虚部一致],
    [等价腔长 $L_"eq" = 2 f$], [#Q.Leq mm], [比实际腔长 #Q.L mm 长出 #Q.dleq mm],
    [等价镜曲率半径 $R_"eq" = 2 f$], [#Q.Req mm], [对称共焦条件 $R_"eq" = L_"eq"$],
    [等价腔中心], [实际束腰所在截面], [本例为对称腔，即腔心，距两镜各 #Q.d1 mm],
    [实际 M1 的等效坐标], [$z_1$ = #Q.z1 mm], [在等价镜面（$-f$ = -#Q.fnum mm）以内 #Q.eqout mm],
    [实际 M2 的等效坐标], [$z_2$ = + #Q.d2 mm], [两镜各比等价镜深入 #Q.eqout mm],
    [最小等相位面曲率半径], [$R_"min" = 2 f$ = #Q.Rmin mm], [位于 $z = ± f$，正是等价腔镜面所在平面],
  ),
  caption: [表2：Task 02 腔型（$R_1 = R_2$ = #Q.R1 mm，$L$ = #Q.L mm，#Q.lam nm）的等价共焦腔参数],
) <tab:equiv>

三点值得强调：

- 实际两面镜坐在等价共焦腔*内部*，各比等价镜面深入 #Q.eqout mm（@fig:env 右图）。这不是巧合：$g_1 g_2$ = #Q.prod 远小于 1，说明腔靠近共焦简并点，因而 $f$ 比半腔 #Q.d2 mm 只大 #Q.fminusL2 mm。
- 等价共焦腔的镜面位置 $z = ± f$ 恰好就是瑞利面——等相位面曲率取极小值 $R_"min" = 2 f$ = #Q.Rmin mm 的地方。"等价腔镜 = 瑞利面"是共焦参数几何意义最直观的表述。
- $R_1 ≠ R_2$ 时束腰偏离腔心、$z_1 ≠ -z_2$，但 $d_1 + d_2 = L$ 与 @eq:closed/@eq:inverse 同样适用（脚本按通式计算，可换参数复算）。

自检：@eq:closed 与 @eq:inverse 的八项残差（$f^2$ 两式相等、$d_1 + d_2 = L$、束腰位置与镜面光斑对照 $q$ 法、正反映射闭环）最大 #Q.chkmax；单程古伊相移的 2 倍与往返公式 $2 "arccos" sqrt(g_1 g_2)$ 相差 #Q.gouydiff rad，都是机器精度量级。

== 束腰位置与尺寸（要求 2）

M1 反射后离镜处的本征 $q$ 参数为 $q_1 = -d_1 + i f$，即实部 #Q.d1 mm、虚部 #Q.fnum mm。两个最基本的回答随之确定：

- *束腰位置*：距 M1 #Q.d1 mm，正好是腔的几何中心（对称腔的必然结果）；@fig:env 左图在实际腔结构图上用红虚线标出。
- *束腰半径*：$w_0 = sqrt(lambda f / pi)$ = #Q.w0 μm。

瑞利长度 $z_R = f$ = #Q.zR mm 比半腔长还大 #Q.fminusL2 mm，所以腔内光斑只从束腰的 #Q.w0 μm 单调张开到镜面的 #Q.w1 μm，束腰两侧完全对称。

== 腔内高斯光束包络与主要参数（要求 3）

$ w(z) = w_0 sqrt(1 + ((z - z_"w") / z_R)^2) , quad
  R(z) = (z - z_"w") + z_R^2 / (z - z_"w") , $ <eq:envelope>

其中 $z_"w"$ = #Q.d1 mm 为束腰位置（自 M1 起算）。

#figure(
  three-line-table(
    columns: (auto, auto, auto, auto),
    head: ([*参数*], [*数值*], [*参数*], [*数值*]),
    align: (left, right, left, right),
    [束腰半径 $w_0$], [#Q.w0 μm], [瑞利长度 $z_R$], [#Q.zR mm],
    [束腰位置（自 M1）], [#Q.d1 mm（腔心）], [镜面光斑 $w_1 = w_2$], [#Q.w1 μm = #Q.rw1 $w_0$],
    [瑞利面光斑 $sqrt(2) w_0$], [#Q.wR μm], [远场半发散角 $theta_0$], [#Q.th1 mrad],
    [远场全发散角], [#Q.thf mrad], [单程古伊相移], [#Q.gouy1 rad],
    [往返古伊相移], [#Q.gouyrt rad], [横模/纵模间隔比], [#Q.tmr],
    [纵模间隔 FSR], [#Q.fsr MHz], [束腰两侧 ±$z_R$ 内模体积], [#Q.V2zR mm³],
  ),
  caption: [表3：腔内基模（TEM₀₀）的高斯光束参数（#Q.lam nm）],
) <tab:beam>

@fig:env 左图是整段腔的光束包络：按 @eq:envelope 画出 ±$w(z)$，标出束腰（红虚线）、两镜光斑、瑞利面（点线，也就是等价共焦腔镜面位置）与晶体所占区间；右图把"实际腔 / 等价共焦腔"两套坐标并排画出来。@fig:wf 左图给出 $R(z)$：它在两镜处正好等于镜曲率半径 #Q.R1 mm（@eq:selfrepro 的自再现条件——Task 01 里用几何焦距 $R/2$ 说的那件事，在这里换成等相位面语言），在束腰处发散为无穷（平面波前）；右图把区间向腔外延拓，标出瑞利面处 $w = sqrt(2) w_0$ = #Q.wR μm 与最小波前曲率 $2 f$ = #Q.Rmin mm。

#figure(
  image("figures_task3/envelope.png", width: 100%),
  caption: [图1：左——腔内基模高斯包络，束腰（距 M1 #Q.d1 mm 的腔心）、两镜光斑与增益晶体位置；右——等价共焦腔坐标系，实际两镜（$z$ = #Q.z1 / + #Q.d2 mm）坐在等价腔镜（$z = ± f$ = ±#Q.fnum mm）以内。镜形挠度按示意放大。],
) <fig:env>

#figure(
  image("figures_task3/wavefront.png", width: 96%),
  caption: [图2：左——等相位面曲率半径 $R(z)$ 在两镜处与镜曲率重合；右——腔外延拓，瑞利面 $z = ± f$ 处 $w = sqrt(2) w_0$、波前曲率取极小 $2 f$ = #Q.Rmin mm。],
) <fig:wf>

输出耦合镜 M2 对腔内模的作用由其曲率（#Q.R2 mm）决定。若透射面是平行平面基片，出射光束就是腔内高斯光束的延续——束腰仍在腔心、远场全发散角 #Q.thf mrad，理想基模 $M^2$ = 1；若曲率面直接对外，基片折射还会改写
出射波前，需要按基片的两个表面曲率与折射率另做核算；本报告给出的 #Q.thf mrad 是腔内模的远场发散角，作为出射光束的下限。

== 验证 Task 02（要求 4）

Task 02 用柱对称 Fox-Li 迭代求自再现模，本任务用高斯模型描述同一个腔。对照在*同一腔内截面*上做，并把"光斑大小"用三种定义各量一遍——口径截断之后这三种定义本来就不等价。

#figure(
  three-line-table(
    columns: (auto, auto, auto, auto),
    head: ([*比较量*], [*Task 02（衍射）*], [*Task 03（高斯模型）*], [*相对差*]),
    [镜面光斑：二阶矩 $sqrt(2 r^2)$], [#Q.foxw μm], [#Q.w1 μm], [#Q.foxdev\%],
    [镜面光斑：$1/e^2$ 强度穿越半径], [#Q.foxedge μm], [#Q.w1 μm], [#Q.foxedgedev\%],
    [镜面光斑：核心区（$r < 0.45 a$）自由斜率拟合], [#Q.foxfit μm], [#Q.w1 μm], [#Q.foxfitdev\%],
    [沿腔二阶矩最大偏差], [—], [—], [#Q.foxmax\%（在 $z$ = #Q.foxmaxz mm）],
    [束腰位置], [#Q.zwfox mm（Fox-Li 场逐段传播后定出）], [#Q.d1 mm], [#Q.zwdiff mm],
    [往返相位], [#Q.phasefox rad], [#Q.gouyrt rad（古伊，取负号）], [#Q.phdiff rad],
    [往返衍射损耗], [#Q.lossfox], [#Q.lossgauss（高斯截断式估计）], [高斯估计高 #Q.losscmp\%],
    [次低阶模往返损耗], [#Q.lossmode], [—（基模模型不含高阶模）], [—],
  ),
  caption: [表4：Task 02 的波动光学结果与 Task 03 高斯模型在同一截面上的对照（$D$ = #Q.D mm；Task 02 一列直接引自其生成物 `results_task2.json`）],
) <tab:cross>

#figure(
  image("figures_task3/verify_task2.png", width: 100%),
  caption: [图3：左——沿腔光斑尺寸，高斯模型与 Fox-Li 自再现模的二阶矩 / $1/e^2$ 半径 / 核心拟合值；中——二阶矩相对偏差沿腔的变化；右——镜面处横向强度分布与解析高斯的对比（对数纵轴）。],
) <fig:verify>

*结论：两种方法一致。* 用与 Task 02 相同的口径 #Q.D mm 重跑柱对称 Fox-Li（#Q.foxit 次往返收敛到 $10^(-9)$），模斑二阶矩 #Q.foxw μm 与高斯模型的 #Q.w1 μm 只差 #Q.foxdev\%，其二维 FFT 版本给出 #Q.w2dT2 μm，落在同一个值上。把收敛场用同一套 Fresnel 算子向腔内逐段传播，二阶矩沿腔与 $w(z)$ 的最大偏离只有 #Q.foxmax\%，用抛物线顶点反推的束腰位置比模型值偏 #Q.zwdiff mm。更关键的是*相位*也对上了：Task 02 本征值的辐角 #Q.phasefox rad 与高斯模型的往返古伊相移 -#Q.gouyrt rad 只差 #Q.phdiff rad（0.05\%）——这说明高斯模型不只是"宽度差不多"，而是把腔的纵向谐振结构也描述对了。

*残余差异的来源，按重要性排序：*

1. *硬口径截断使本征模不是严格高斯*。@fig:verify 右图里，自再现模归一化后，核心区（$r < 0.45 a$）逐点 $"ln" I$ 与解析高斯的最大偏离 #Q.coredev（平均 #Q.coresign > 0，即*核心略偏胖*），到口径边缘偏离增大到 #Q.apdev。同一个模用不同定义量半径自然给出不同结果：二阶矩 #Q.foxw μm 最接近高斯值（截断的长尾权重被削掉），$1/e^2$ 穿越 #Q.foxedge μm，核心斜率拟合 #Q.foxfit μm。
2. *损耗定义层面的差异*：高斯截断式 $delta approx 2 "exp"(-2 a^2 / w^2)$ 给 #Q.lossgauss，比迭代值 #Q.lossfox 大 25\%，正是上述"胖核心 + 陡边缘"的后果——模式为适应口径自行收缩，实际漏出的功率比纯高斯估计少。这个差别不改变 Task 02 的结论（1\% 损耗对应的最小口径 #Q.dminT2 mm 仍远小于镜片口径）。
3. *数值来源*：径向网格 $d r$ ≈ 1.5 μm、损耗数值本底 $4 × 10^(-5)$、反射按"振幅共轭"处理（认为往返相位取实）、收敛判据 $10^(-9)$，这些只影响 $10^(-3)$ 量级以下的数字。

一句话回答作业问题：*一致*；差异来自有限口径对高斯模的截断（模式变形 + "光斑半径"定义不唯一），而不是两套理论的物理假设不同。顺带说明：Task 02 报告里"中心部分与高斯吻合到 $10^(-4)$ 以下"是按二阶矩半径作图所作的目视比较；本报告改用自由斜率拟合与逐点 $"ln" I$ 度量，得到的是"高斯性极好、但存在 $1\%$ 量级的系统偏离"，两种说法不矛盾，后一种更适合解释损耗与选模行为。

== 增益晶体的位置与尺寸（要求 5）

腔内基模定了以后，晶体的位置和尺寸就是三个可量化的条件：光斑要小（阈值低、需要的反转少）；沿晶体长度光斑要*几乎不变*（增益区与模体积对齐，不产生增益导波）；口径裕度要够（不与 Task 02 的衍射损耗预算冲突）。

=== 位置：中心与束腰重合（腔心）

晶体占据距 M1 #Q.zin~#Q.zout mm 的一段，长 #Q.Lc mm。逐条定量回答：

- *晶体处腔模光斑多大*：$w$ = #Q.win μm（就是 $w_0$），光束直径 #Q.dmode mm。
- *晶体尺寸范围内光斑变化是否明显*：不明显。离开束腰 $h$ 处 $w / w_0 - 1 = sqrt(1 + (h / f)^2) - 1$，而 $f$ = #Q.fnum mm 远大于晶体长度：

  #figure(
    three-line-table(
      columns: (auto, auto, auto, auto),
      head: ([*晶体全长 / mm*], [*半长 $h$ / mm*], [*$w / w_0 - 1$ / \%*], [*端面光斑 / μm*]),
      ..FLAT.flatten(),
    ),
    caption: [表5：以束腰为中心的晶体，内部光斑尺寸的漂移（$z_R$ = #Q.zR mm）],
  ) <tab:flat>

  瑞利长度 #Q.zR mm 是晶体长度 #Q.Lc mm 的 #Q.zratio 倍，所以*只有束腰处 $d w / d z = 0$*：任何偏移都会让晶体两端光斑一增一减，泵浦沉积区与模体积不再对齐。这是把晶体放在束腰的核心定量理由。
- *光束能否安全通过晶体有效区域*：能。腔内光阑半径 #Q.airis μm（$a/w$ = #Q.mir，截断损耗 #Q.irisloss，与 Task 02 的损耗预算一致），晶体半径 #Q.arod μm（$a/w$ = #Q.mrod）：光束直径不到掺杂芯区直径的三分之一，既不擦到未掺杂区/端面键合层，也不受晶体通光孔径限制。
- *为什么不是别处*：@fig:crys 右图与 @tab:pos 把同样长度的晶体放到腔内其它位置对照。

  #figure(
    three-line-table(
      columns: (auto, auto, auto, auto, auto, auto),
      head: ([*位置*], [*中心 $z$ / mm*], [*中心光斑 / μm*], [*机内 $Δ w / w$ / \%*], [*$a/w$（光阑）*], [*截断损耗*]),
      ..POS.flatten(),
    ),
    caption: [表6：Ø #Q.rod × #Q.Lc mm 晶体放在腔内不同位置（光阑直径 $D$ = #Q.D mm）],
  ) <tab:pos>

  靠近镜面时（@tab:pos 末行）中心光斑涨到 #Q.posfarw μm（比束腰处大 #Q.posfarinc\%），晶体内部的光斑漂移升到 #Q.posfardw 个百分点，同时 $a/w$ 降到 #Q.posfarmar（比束腰处小 #Q.posnarmar 倍）、截断损耗升到 #Q.posfarloss——阈值、模式匹配、衍射损耗三项同时变差。此外*对称放置*保住了 Task 01/02 的失调容差：载入元件后两镜光斑仍然相等（@tab:therm 每行 $w_1 = w_2$），镜面处波前仍与镜面对合到 #Q.rmatch 的相对精度。

#figure(
  image("figures_task3/crystal.png", width: 100%),
  caption: [图4：左——离开束腰的光斑相对变化（$10^(-2)\%$ 量级，极为平坦）；右——同长晶体放在束腰处与近镜处，晶体内光斑由"常数"变成"明显倾斜"。],
) <fig:crys>

=== 尺寸：Nd:YAG #Q.dop at.\%、Ø #Q.rod × #Q.Lc mm

依据（材料常数见文末）：

- *吸收*：$alpha = sigma_"abs" N_"Nd"$ = #Q.alpha cm⁻¹，$1 / alpha$ = #Q.adep mm，#Q.Lc mm 吸收 #Q.abst\% 的泵浦。增益主要沉积在入射面起约三倍吸收长度内，与晶体长度匹配；掺杂再高会把吸收压进前 1~2 mm，使有效泵浦体积（#Q.Vpump mm³）明显小于晶体内模体积（#Q.Vmode_c mm³），并加重浓度淬灭与热负荷。
- *阈值核对*：往返损耗 = 输出耦合 1.5\% + 口径截断 #Q.irisloss ≈ #Q.rtloss\%，对应阈值增益 $g_"th"$ = #Q.gth cm⁻¹、阈值反转密度 $N_"th"$ = #Q.Nth cm⁻³，只是掺杂离子数的 #Q.finv（#Q.finvpct\%）。阈值极低，说明"低掺杂 + 中等长度"对 #Q.lam nm 的四能级运转有充足增益储备，不必用提高掺杂来妥协。
- *直径*：Ø #Q.rod mm（$a/w$ = #Q.mrod）。限模不靠晶体，仍由 Task 02 的 Ø #Q.D mm 腔内光阑负责（放在晶体输入面处，兼作高阶模抑制）；泵浦区直径 #Q.dwp mm 比基模直径 #Q.dmode mm 还细，因此增益区本身小于模体积，对高阶模还有一层增益导引式的抑制。
- *端面*：双面镀 808/1064 增透；平行晶面会形成弱标准具，工程上用小于 $lambda/10$ 的楔角或严格控温避免纵模被调制。
- *插入晶体的光学代价*（平行平板，$n$ = 1.82）：等效腔长由 #Q.L mm 折算为 #Q.insLeq mm，$f$ 变化 #Q.insdf\%、$w_0$ 变化 #Q.insdw0\%（#Q.insw0 μm）、镜面光斑变化 #Q.insdw1\%（#Q.insw1 μm）。折算坐标下束腰移动 #Q.insd1 mm，但物理上由于结构对称仍在腔心。也就是说"把晶体放进腔里"对模式的影响在 1\% 以内，Task 01/02 的稳定性与损耗结论全部沿用。

=== 热透镜容差：这项设计真正的约束

晶体吸收泵浦后等效于一个正透镜。把薄透镜放在晶体中心（= 束腰），用同一套 $q$ 法算腔的响应：

#figure(
  three-line-table(
    columns: (auto, auto, auto, auto, auto, auto, auto, auto),
    head: ([*$f_"th"$ / mm*], [*半迹 $(A+D)/2$*], [*$w_0$ / μm*], [*$z_R$ / mm*], [*束腰 / mm*], [*$w_1$ / μm*], [*$w_2$ / μm*], [*稳定性*]),
    align: (right, right, right, right, right, right, right, center),
    ..THERM.flatten(),
  ),
  caption: [表7：腔内热透镜强度对基模与稳定性的影响（薄透镜置于晶体中心；"束腰"列为折算坐标下的 $-q_1$ 实部）],
) <tab:therm>

- 硬性失稳只出现在共心一侧：$f_"th"$ = #Q.funst mm（半迹到 +1）。
- 正透镜让半迹先朝 $-1$ 靠拢，在 $f_"th"$ ≈ #Q.fnear mm 处*相切于共焦简并点*（细扫最小半迹 #Q.trmin，等价 $g_1 g_2$ = #Q.prodmin，#Q.crossconf）。也就是说对称腔 + 中心透镜不会因为热透镜而在共焦侧失稳，但经过这一档时横模完全简并、Task 02 的口径选模机制失效、失调容差最差——工程上应当避开。
- 换算成功率：端泵弱吸收（$alpha w_p$ ≈ 0.1 ≪ 1，面源近似）下 $1 / f_"th" approx P_"abs" (d n / d T + alpha_"th" (n - 1)) / (pi k_c w_p^2)$，本泵浦光斑给出系数 #Q.coefth m⁻¹W⁻¹（把全部吸收功率都当热功率，偏保守；1 W 吸收对应 $f_"th"$ ≈ #Q.fth1w mm）。若只计量子缺陷放热（#Q.heatfrac，即 #Q.lamp → #Q.lam nm），系数降为 #Q.coefthq m⁻¹W⁻¹。两个口径给出功率窗：经过共焦简并档对应吸收功率 #Q.pdeg~#Q.pdegq W，共心侧硬失稳对应 #Q.phard~#Q.phardq W。因此*吸收功率应避开 #Q.pdeg~#Q.pdegq W 这一档*：要么远小于它，要么足够大（$f_"th"$ 明显短于 #Q.fnear mm 但仍留 3 倍以上裕度到 #Q.funst mm）。表7 同时显示：透镜变强时等价 $g_1 g_2$ 回升、横模间隔比从 #Q.tmr 往下走（选模更有利），但腔模在晶体处也随之变小，泵浦光斑与 $xi$ 必须按最终热模型复核——这属于 Task 04（速率方程与热设计）的输入。

== 泵浦光与腔模的空间匹配（要求 6）

=== 判据与允许带

课件给的基础判据是"晶体输入面的泵浦光斑半径不大于基模束腰半径"，即 $w_p <= w_0$ = #Q.w0 μm，目的是让泵浦区完全落在腔模内、优先放大基模。这个判据只给上界，还需要一个连续的指标来决定该取多小。这里采用两个归一化高斯强度分布的重叠因子

$ xi = frac((integral I_p I_l dif A)^2, integral I_p^2 dif A integral I_l^2 dif A)
    = 4 w_p^2 w_l^2 / (w_p^2 + w_l^2)^2 , $ <eq:xi>

$xi$ = 1 当且仅当 $w_p = w_l$。它就是端泵固体激光器里"泵浦体积/模体积重叠效率"的标准写法（Koechner；亦见 Opt. Express 24, 26024 (2016) 中同型的 $S(w_p, w_c)$），其失配份额 $1 - xi$ 直接表示模体积内未被泵浦覆盖的比例。把两者结合，取设计指标 $xi >= 0.95$ 且 $w_p <= w_0$，得到允许带

$ #Q.b95lo <= w_p / w_0 <= 1.0 . $

（放宽到 $xi >= 0.90$ 则是 #Q.b90lo~#Q.b90hi；收紧到 $xi >= 0.98$ 则只有 #Q.b98lo~#Q.b98hi，见 @fig:pump 左图。）偏小比偏大好：泵浦略细于腔模时阈值强度更高，且对高阶模还有一层增益导引式的抑制；泵浦粗于腔模则直接浪费模体积、抬高阈值。

#figure(
  three-line-table(
    columns: (auto, auto, auto, auto),
    head: ([*$w_p / w_0$*], [*泵浦束腰 $w_p$ / μm*], [*重叠因子 $xi$*], [*失配 $1 - xi$ / \%*]),
    ..PUMP.flatten(),
  ),
  caption: [表8：泵浦光斑大小与腔模的空间匹配（晶体处腔模光斑 = $w_0$ = #Q.w0 μm）],
) <tab:pump>

=== 设计点：泵浦束腰尺寸与位置

#figure(
  image("figures_task3/pump.png", width: 100%),
  caption: [图5：左——重叠因子随 $w_p / w_0$ 的变化，标出 $xi$ = 0.90/0.95/0.98 允许带与课件判据 $w_p <= w_0$；右——晶体内泵浦与腔模光斑（两者都近似常数，阴影为间隙），虚线为剩余泵浦比例（#Q.abst\% 被吸收）。],
) <fig:pump>

- *泵浦束腰半径*：取 $w_p = #Q.ratio w_0$ = #Q.wp μm，对应 $xi$ = #Q.xiin（失配 #Q.xloss\%，见 @tab:pump），满足 $w_p <= w_0$。
- *泵浦束腰位置*：放在*晶体输入面*（距 M1 #Q.zin mm）。这个选择的定量依据全在 $z_"Rp"$ 这个数上：$z_"Rp" = pi w_p^2 / lambda_p$ = #Q.zRp mm 是晶体长度 #Q.Lc mm 的二十几倍，泵浦进晶体后几乎不张开（$w_p$ 由 #Q.wp μm 只变到 #Q.wpex μm），而腔模同样平坦，于是 $xi$ 沿晶体几乎不变：输入面 #Q.xiin → 出射面 #Q.xiex，按泵浦沉积加权平均 #Q.xibar，机内最小 #Q.ximin。*结论：在这套近共焦参数下，泵浦束腰放在晶体内哪个深度并不敏感，真正决定匹配质量的是光斑尺寸的比值。*
- 吸收系数 #Q.alpha cm⁻¹ 对应 $1/alpha$ = #Q.adep mm，泵浦在前 #Q.adep mm 内就衰减到 $1/e$；把束腰放在输入面，可以让最强的泵浦与最高的增益正好落在腔模最细、也是几何对称中心的位置，热负荷中心与模中心重合，减小偏心热透镜。
- 泵浦传到 M1 时半径长到 #Q.wpm1 μm（半角 #Q.thp mrad），仍远小于镜片口径与 2 $a$ = #Q.D mm 的两倍，注入本身不产生额外截断。

*聚焦光组*：泵浦源取 808 nm 光纤耦合二极管（芯径 Ø #Q.fiber μm、NA 0.22；输出近似平顶，等效 $1/e^2$ 半径 #Q.wfib μm）。所需成像倍率 $beta = w_p / w_"fib"$ = #Q.beta，用伽利略望远系统实现：$f_1$ = #Q.f1 mm、$f_2$ = #Q.f2n mm（间距 #Q.lensd mm，$beta$ = #Q.lensbeta），像方光斑 #Q.lenswp μm，束腰落在晶体输入面。M1 需同时是 1064 nm 高反与 808 nm 高透（HR-1064 / HT-808 复合膜），晶体输入面镀双增透。

*强度与热*：每瓦泵浦在输入面的峰值强度约 #Q.I1W W/cm²（$2P / (pi w_p^2)$），其中量子缺陷份额 #Q.heatfrac 转为热，即 @tab:therm 讨论的热负荷来源。

== 结论

1. *等价共焦腔*：#Q.R1 / #Q.R2 / #Q.L mm 双凹腔唯一对应 $f$ = #Q.fnum mm、$L_"eq" = R_"eq"$ = #Q.Leq mm 的对称共焦腔；等价腔中心在实际束腰处（距 M1 #Q.d1 mm），两实际镜面的等效坐标 $z_1$ = #Q.z1 mm、$z_2$ = + #Q.d2 mm，各比等价镜面深入 #Q.eqout mm；对应关系见 @eq:selfrepro/@eq:inverse，正反演残差 #Q.chkmax。共焦参数按教材约定 $f = pi w_0^2 / lambda = z_R$，等价腔镜平面即瑞利面（最小波前曲率 $2 f$ = #Q.Rmin mm）。
2. *束腰*：位置在腔心，距 M1 #Q.d1 mm；半径 $w_0$ = #Q.w0 μm；$z_R$ = #Q.zR mm。已在 @fig:env 的腔结构图上标出。
3. *腔内包络*：$w(z)$、$R(z)$ 见 @eq:envelope 与 @fig:env、@fig:wf；两镜光斑 $w_1 = w_2$ = #Q.w1 μm（= #Q.rw1 $w_0$），瑞利面光斑 #Q.wR μm，远场全发散角 #Q.thf mrad，往返古伊相移 #Q.gouyrt rad，束腰两侧 ±$z_R$ 内模体积 #Q.V2zR mm³（@tab:beam）。
4. *与 Task 02 是否一致*：一致。同一口径下自再现模二阶矩 #Q.foxw μm 对高斯 #Q.w1 μm（差 #Q.foxdev\%），沿腔最大 #Q.foxmax\%，束腰差 #Q.zwdiff mm，往返相位差 #Q.phdiff rad（0.05\%）。残差主要来自硬口径截断造成的模式变形（核心略胖、边缘更陡；$"ln" I$ 偏离核心 #Q.coredev、口径内 #Q.apdev）以及"光斑半径"定义不唯一，而不是两套理论的物理假设差异。
5. *晶体*：Nd:YAG #Q.dop at.\%、Ø #Q.rod × #Q.Lc mm，中心与束腰重合（距 M1 #Q.zin~#Q.zout mm）。晶体处光斑 #Q.win μm、机内变化 #Q.dwcry\%；$a/w$ = #Q.mir（光阑）/ #Q.mrod（晶体），光束完全在有效掺杂区内；吸收 #Q.abst\%，阈值反转仅为掺杂数的 #Q.finvpct\%；插入晶体只让 $w_0$、镜面光斑变化 #Q.insdw0\% / #Q.insdw1\%。热透镜容差：硬失稳 $f_"th"$ = #Q.funst mm，并应避开 #Q.pdeg~#Q.pdegq W 的共焦简并档。
6. *泵浦*：808 nm 端泵，泵浦束腰半径 $w_p$ = #Q.wp μm（= #Q.ratio $w_0$，满足"晶体输入面泵浦光斑不大于基模束腰"的判据），束腰位于晶体输入面；$xi$ = #Q.xiin（沉积加权 #Q.xibar），允许带 #Q.b95lo ≤ $w_p / w_0$ ≤ 1.0；用 $f_1$ = #Q.f1 mm / $f_2$ = #Q.f2n mm 的伽利略系统（$beta$ = #Q.lensbeta）聚焦到位，泵浦到 M1 处半径 #Q.wpm1 μm，M1 兼作 808 增透 / 1064 高反。

相对 Task 02 的 V2.0 规格，本任务记为 V3.0：几何（#Q.R1 / #Q.R2 / #Q.L mm）与有效口径（#Q.D mm）不变，新增腔内元件规格——Nd:YAG #Q.dop at.\% Ø #Q.rod × #Q.Lc mm 居腔心、腔内 Ø #Q.D mm 光阑在晶体输入面、M1 增透 808 / 高反 1064、泵浦 Ø #Q.fiber μm 光纤经 #Q.f1 mm/#Q.f2n mm 望远聚焦至 #Q.wp μm。

== 复算指引

```bash
cd resonator_design
uv run task3_gauss.py                       # 默认沿用 Task 02 腔型 + Nd:YAG 端泵参数
uv run task3_gauss.py --dop 1.0 --Lc 6 --ratio 0.9   # 换掺杂/晶体长度/泵浦比重算
uv run task3_gauss.py --R1 600 --R2 600 --L 550 --lam 1064 --D 1.6 --quick   # 跳过与 Task 02 的衍射对照
typst compile --root . report_task3.typ     # 排版本报告
```

#figure(
  three-line-table(
    columns: (auto, 1fr),
    head: ([*文件*], [*作用*]),
    [task3_gauss.py], [全部计算：等价共焦腔闭式解与 $q$ 法自检、腔内包络、Fox-Li 对照、晶体与热透镜、泵浦匹配],
    [summary_task3.typ], [结果字典 `Q` 与四张表 `FLAT`/`POS`/`THERM`/`PUMP`，本报告的数字来源],
    [results_task3.json], [同一批结果的机器可读版本（含中间量与自检残差）],
    [figures_task3/], [五张图（@fig:env ~ @fig:pump）],
    [task2_foxli.py], [被 import：Task 02 的柱对称 Fresnel 传播算子与 Fox-Li 迭代（对照用）],
    [results_task2.json], [被读取：Task 02 的模斑、损耗、本征相位（保证同源）],
  ),
  caption: [表9：文件清单],
)

材料常数（Nd:YAG，第 5、6 项使用，写在脚本头部，改一处即全套重算）：$sigma_"abs"$（808 nm）= 4.2 × $10^(-20)$ cm²，$sigma_"em"$（1064 nm）= 2.8 × $10^(-19)$ cm²，$tau_"f"$ = 230 μs，$n$ = 1.82，$k_c$ = 14 W·m⁻¹·K⁻¹，$d n / d T$ = 7.3 × $10^(-6)$ K⁻¹，$alpha_"th"$ = 7.5 × $10^(-6)$ K⁻¹；Y 格位密度 1.377 × $10^(22)$ cm⁻³（由 $rho$ = 4.55 g·cm⁻³、$M$ = 596.8 g·mol⁻¹、每式单位 3 个 Y 位算出）。

== 参考依据

- 周炳琨等，《激光原理》（第 7 版），第 2 章：高斯光束的 $q$ 参数与 $f$ 参数（共焦参数）定义、"任一稳定球面腔唯一等价于一个共焦腔"——本报告第 1 项的符号与定义按此约定。
- A. E. Siegman, *Lasers*, Science Books：稳定腔高斯模、$R(z) = z + z_R^2 / z$、古伊相移。
- W. Koechner, *Solid-State Laser Engineering*（第 6 版）：Nd:YAG 吸收/发射截面与寿命、端泵重叠效率、热透镜公式。
- Opt. Express 24, 26024 (2016)，端面泵浦凹-凸腔设计中的泵浦/腔模重叠效率表达式（与 @eq:xi 同型）。
- 本仓库 Task 01 / Task 02 报告：`report_simple.typ`（几何设计与稳定裕度）、`report_task2.typ`（自再现模、最小口径 #Q.dminT2 mm、往返损耗 #Q.lossfox）。
