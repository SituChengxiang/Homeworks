# 激光原理与应用 —— 作业

《激光原理与应用》课程作业正文，只放自己的解答与计算，模板不在这里。

## 目录结构

- `shortSemester1.typ` —— 调研 1：激光器相干长度的测量方法（3 页 A4，`@local/zjut-phy-hwk` 排版）
- `resonator_design/` —— 工程任务主线（Task 01 起，`@local/simple-zjut-report` 排版 + Python 核算）：
  **代码平铺在顶层**（互相 import，省掉包路径），**交付物按 task 分目录**。

```
resonator_design/
  cavity_core.py      共享计算核心：g 参数、ABCD、本征 q、光斑半径、截断损耗、Fresnel 往返复核
                      （Task 02/03 直接 import 它，别再复制 eigen_q 这类 helper）
  cavity.py           Task 01 入口：算一遍 → task01/results.json + task01/numbers.typ → 调出图
  report_figs.py      Task 01 出图 → task01/figures/
  task2_foxli.py      Task 02 入口：柱对称 Fox-Li + 二维 FFT 校验 + 口径扫描 → task02/
  task3_gauss.py      Task 03 入口：等价共焦腔 + 高斯包络 + 晶体 + 泵浦匹配 → task03/
  task01/  task02/  task03/     交付：report.typ + numbers.typ + results.json + figures/
  refs/               题目 PDF 与已交报告 PDF（任务1.pdf、设计1报告.pdf）
  archive/            report_long.typ —— 15 页长版，正文仍停在旧设计 800/1200，未同步
```

三个 task 目录内部文件名完全一致（`report.typ` / `numbers.typ` / `results.json` / `figures/`），靠目录区分，
所以新增一个 task 只要照抄这套结构、把脚本输出指到新的 `taskNN/` 即可。`numbers.typ` 与 `figures/` 由脚本生成，
**报告里的数字一律 `#import "numbers.typ"`，不手抄**。

## 当前设计与各任务结论

腔型：**对称近共焦** 双凹腔，L = 550 mm、R₁ = R₂ = 600 mm（g₁g₂ = 0.0069，距共焦简并点 50 mm）。
改设计只动 `cavity_core.py` 的 `DESIGN`（对照腔在 `COMPARATORS`）。

- Task 01（几何设计与稳定裕度，λ = 632.8 nm）→ `task01/report.typ`（5 页，已交）。
  注意两个腔长裕度别混：`margin_L`（到真正失稳区）与 `margin_crit`（到 g₁g₂ = 0/1 的临界简并点）。
- Task 02（自再现模与衍射损耗，λ = 1064 nm）→ `task02/report.typ`（9 页）。
  近高斯基模，镜面 $w$ = 434.0 μm；1% 损耗要求 $D_{\min}$ = 1.355 mm，工程取 Ø1.6 mm
  （往返损耗 0.17%，次低阶模损耗 12.7%）；几何保留，只把镀膜改到 1064 nm（V2.0）。
- Task 03（腔内高斯光束、晶体与泵浦，λ = 1064 nm）→ `task03/report.typ`（15 页）。
  等价共焦腔 $f=z_R$ = 298.96 mm、$L_\text{eq}=R_\text{eq}$ = 597.91 mm，两实际镜面在等效坐标 ∓275 mm
  （比等效镜深入 23.96 mm）；束腰在腔心，$w_0$ = 318.20 μm，镜面光斑 432.35 μm；
  与 Task 02 的模斑二阶矩差 0.39%、往返相位差 1.4×10⁻³ rad；
  晶体 Nd:YAG 0.5 at.% Ø3×10 mm 居腔心（机内光斑变化 0.000%，$a/w$ = 2.51）；
  泵浦束腰 254.6 μm（= 0.8 $w_0$）在晶体输入面，重叠因子 0.9518；
  热透镜硬失稳在 $f_\text{th}$ = 138 mm，并在 1786 mm 处与共焦简并点相切（V3.0）。
- `task3_gauss.py` 会 `import task2_foxli` 并读 `task02/results.json`，保证第 4 项对照与 Task 02 同源。

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
uv run cavity.py                              # Task 01（PEP 723 自动装依赖；system python3 缺 scipy）
uv run task2_foxli.py                         # Task 02：含二维校验，约 40 s
uv run task3_gauss.py                         # Task 03：含 Fox-Li 对照，约 1 min

typst compile --root . task01/report.typ      # 5 页简版（交这份）
typst compile --root . task02/report.typ      # 9 页
typst compile --root . task03/report.typ      # 15 页
# typst compile --root . archive/report_long.typ   # 长版正文未同步，先改文字再编
```

`--root .` 是为了让 `report.typ` 里的相对图片路径在任意工作目录下都能解析。

常用参数：

- Task 02 改腔型/口径：`uv run task2_foxli.py --R1 600 --R2 600 --L 550 --lam 1064 --D 1.6 --sweep 1.2,1.4,1.6,2.0`
- Task 03 换晶体/泵浦：`--dop`（at.%）、`--Lc`（mm）、`--rod`（mm）、`--ratio`（泵浦束腰/腔模束腰）、
  `--Pabs`（吸收泵浦功率，用于热透镜换算）、`--T`（OC 透过率 %）、`--quick`（跳过衍射对照）

仓库根 `.gitignore` 已忽略 `*.pdf` 与 `*.png`：报告 PDF 和各 `figures/` 里的图默认不进版本库，
要留档得 `git add -f <文件>`。脚本与 `numbers.typ`/`results.json` 是入库的。
