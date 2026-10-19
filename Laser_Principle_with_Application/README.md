# 激光原理与应用 —— 作业

《激光原理与应用》课程作业正文，只放自己的解答与计算，模板不在这里。

## 文件

- `shortSemester1.typ` —— 调研 1：激光器相干长度的测量方法（3 页 A4，`@local/zjut-phy-hwk` 排版）
- `resonator_design/` —— 工程任务：两镜稳定谐振腔设计（`@local/simple-zjut-report` 排版 + Python 核算）
  - `cavity.py` ABCD 矩阵、本征 q、g 判据、公差、失调灵敏度、Fresnel 往返复核
  - `summary.typ` / `results.json` / `figures/` 全部由脚本生成，报告里的数字直接引自 `summary.typ`
  - `report.typ` → 15 页设计报告

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
uv run cavity.py                    # 重算并生成 figures/ 与 summary.typ（PEP 723 自动装依赖）
typst compile --root . report.typ   # 出报告 PDF
```

仓库根 `.gitignore` 已忽略 `*.pdf`，产物只留在本地。
