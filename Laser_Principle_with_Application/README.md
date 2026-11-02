# 激光原理与应用 —— 作业

《激光原理与应用》课程作业正文，只放自己的解答与计算，模板不在这里。

## 文件

- `shortSemester1.typ` —— 调研 1：激光器相干长度的测量方法（3 页 A4，`@local/zjut-phy-hwk` 排版）
- `resonator_design/` —— 工程任务：两镜稳定谐振腔设计（`@local/simple-zjut-report` 排版 + Python 核算）
  - 当前设计：**对称近共焦** 双凹腔，L = 550 mm、R₁ = R₂ = 600 mm（g₁g₂ = 0.0069，距共焦简并点 50 mm）
  - `cavity_core.py` —— 纯计算：稳定区与两类腔长裕度、ABCD 矩阵、本征 q、g 判据、公差、失调、Fresnel 往返复核；
    **改设计只动这里的 `DESIGN`（对照腔在 `COMPARATORS`）**
  - `report_figs.py` —— 全部出图 → `figures/`（`cavity.png` 新手腔图、`g_diagram.png` 等）
  - `cavity.py` —— 驱动入口：算一遍 → `results.json` / `summary.typ` → 调出图
  - `summary.typ` / `results.json` / `figures/` 全部由脚本生成，报告里的数字直接引自 `summary.typ`
  - `report_simple.typ` → 5 页简版报告（交这份）
  - `report.typ` → 15 页长版；**正文还停留在旧设计 800/1200，未同步，直接重编译会图文矛盾**
- Task 02（同一腔型的自再现模与衍射损耗，λ = 1064 nm）
  - `task2_foxli.py` —— 柱对称 Fox-Li 迭代 + 二维 FFT 校验 + 口径扫描；改 `--R1/--R2/--L/--lam/--D`
    即可重算全套图与结果，单独拿走也能跑（不依赖 `cavity_core.py`）
  - `report_task2.typ` → 10 页设计说明（交这份），数字引自 `summary_task2.typ`
  - 结论：近高斯基模，$w$ = 434.0 μm；1% 损耗要求 $D_{\min}$ = 1.355 mm，工程取 Ø1.6 mm
    （往返损耗 0.17%，次低阶模损耗 12.7%）；Task 01 的几何保留，只把镀膜改到 1064 nm（V2.0）
  - `summary_task2.typ` / `results_task2.json` / `figures_task2/` 同样由脚本生成
- Task 03（腔内高斯光束、增益晶体与泵浦匹配，λ = 1064 nm）
  - `task3_gauss.py` —— 等价共焦腔闭式解（正反演自检 + 与 $q$ 法互验）、束腰与腔内包络、
    载入平行平板/热透镜的 ABCD 复核、泵浦重叠因子；`import task2_foxli` 复用衍射算子做第 4 项对照，
    并直接读 `results_task2.json` 保证与 Task 02 同源
  - `report_task3.typ` → 15 页设计说明（交这份），数字引自 `summary_task3.typ`
  - 结论：等价共焦腔 $f=z_R$ = 298.96 mm、$L_\text{eq}=R_\text{eq}$ = 597.91 mm，两实际镜面在等效坐标
    ∓275 mm（比等效镜深入 23.96 mm）；束腰在腔心、$w_0$ = 318.20 μm、镜面光斑 432.35 μm；
    与 Task 02 模斑二阶矩差 0.39%、往返相位差 1.4×10⁻³ rad；
    晶体取 Nd:YAG 0.5 at.% Ø3×10 mm 居腔心（机内光斑变化 0.000%，$a/w$ = 2.51），
    泵浦束腰 254.6 μm（= 0.8 $w_0$）在晶体输入面，重叠因子 0.9518；
    热透镜硬失稳在 $f_\text{th}$ = 138 mm，另在 1786 mm 处与共焦简并点相切
  - `summary_task3.typ` / `results_task3.json` / `figures_task3/` 同样由脚本生成

## 模板

排版来自本地 Typst 包，不在本目录复制模板文件：

```typst
#import "@local/zjut-phy-hwk:0.2.0": *                    // 物理解答模板
#import "@local/simple-zjut-report:0.1.1": experiment-report  // 实验报告模板
```

两个包的源文件在 `~/Git/zjut-phy-hwk` 与 `~/Git/ZJUT-Report-Typst`，通过本地包注册表软链接挂载。换新机器或模板版本号变化后，重跑一次建链脚本即可：

```bash
bash ../scripts/typst-local-packages.sh          # 建立 / 刷新软链接
bash ../scripts/typst-local-packages.sh --check   # 只查看当前挂载状态
```

## 编译

```bash
typst compile shortSemester1.typ

cd resonator_design
uv run cavity.py                       # 一键：算 → results.json/summary.typ → 出图（PEP 723 自动装依赖）
typst compile --root . report_simple.typ   # 出简版报告 PDF
# typst compile --root . report.typ        # 长版：正文未同步旧设计，先改文字再编

uv run task2_foxli.py                  # Task 02：自再现模 + 口径扫描（含二维校验，约 40 s）
typst compile --root . report_task2.typ    # 出 Task 02 报告 PDF

uv run task3_gauss.py                  # Task 03：等价共焦腔 + 晶体 + 泵浦匹配（含 Fox-Li 对照）
typst compile --root . report_task3.typ    # 出 Task 03 报告 PDF
```

Task 03 的可选参数：`--dop`（at.% 掺杂）、`--Lc`（晶体长度 mm）、`--rod`（直径）、`--ratio`（泵浦束腰/腔模束腰）、
`--Pabs`（吸收泵浦功率，用于热透镜换算）、`--T`（OC 透过率 %）、`--quick`（跳过与 Task 02 的衍射对照）。

Task 02 的脚本支持直接改腔型或口径重算，例如
`uv run task2_foxli.py --R1 600 --R2 600 --L 550 --lam 1064 --D 1.6 --sweep 1.2,1.4,1.6,2.0`。

仓库根 `.gitignore` 已忽略 `*.pdf` 与 `*.png`：报告 PDF 和 `figures/` 里的图默认不进版本库，
要留档得 `git add -f <文件>`。
