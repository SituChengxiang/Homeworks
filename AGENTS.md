# AGENTS.md

个人大学作业仓库（ZJUT 应用物理）。**不是 monorepo**：每个顶层目录是一门课、一套自己的工具链，互相独立。
根目录没有 README 之外的总入口，没有统一的 build / test / lint / CI，也没有根 `package.json` / `pyproject.toml`
——永远先 `cd` 进对应子目录，用该目录自带的脚本。文档、注释、提交信息都用中文，新写的内容保持一致。

## 目录

| 路径 | 内容 |
|---|---|
| `Condense_Physics/` | 固体物理：three.js 晶胞可视化（`src/` → 打包成单文件 HTML） |
| `Computational_Physics/alpha_scattering/` | C89 + RK4 α 散射轨道计算（模块化 + 生成的单文件版） |
| `Computational_Physics/random_mean_stats/` | C89 随机数统计（采样 / 分箱 / Lagrange / 高斯拟合）+ Typst 报告 + `test/run.sh` 验证；`make_single_source.py` 生成单文件版 |
| `Computational_Physics/heat_diffusion/` | C89 一维热传导（FTCS 主算 + Crank–Nicolson 对照）+ Typst 报告 + 七层 `test/run.sh` 验证 |
| `Laser_Principle_with_Application/` | Typst 作业：相干长度调研 + 两镜谐振腔设计报告 |
| `Forum/` | Go/Gin 论坛后端，需要 MySQL/MariaDB |
| `matlab/`、`python/`、`C-exp/`、`Mathematical_Physical_Methods/` | 零散作业、笔记、实验图，无构建系统 |
| `stdserve-Back_end/Back-end` | 空 gitlink，忽略 |

## 全仓库注意

- 仓库只放**源码与可复算的数据**：**能重出的**图、PDF、视频、压缩包一律不入库，**也不要 `git add -f`**。
  根 `.gitignore` 的 `*.png` / `*.pdf` / `*.zip` 就是这条政策（注释原文是「二进制文件一律不进库，
  仓库只留源码（已跟踪的旧文件不动）」）。交付物在本地现编现用，交完可删；要长期留档放仓库外。
  历史里那些已跟踪的 PNG/PDF 是政策之前留下的，正在逐步取消跟踪——**别把它们当成「二进制可以入库」的先例**。
- **例外：重不出来的图必须保留跟踪**，删了就是丢数据。目前已知两类——
  `Mathematical_Physical_Methods/` 的 4 张（原题已找不到，留给学弟学妹看）和
  `matlab/CourseDesign/assets/` 的 13 张（FTLE、Poincaré 截面等，生成脚本不在本仓库）。
  判断「能不能重出」**别用「同目录有没有脚本」这种捷径**——计算物理三份作业的绘图脚本在项目根
  （`plot.py` / `plot_final.py`），不在 `data/` 里，按同目录去找会把这 10 张可重出的图误判成不可重出。
  要确认就真跑一遍该目录自带的出图命令，看文件是否重新出现。
- 提交前跑隐私核查：`python3 scripts/check_privacy.py --staged`（hook 在 `scripts/hooks/pre-commit`，
  启用一次 `git config core.hooksPath scripts/hooks`，这是本机设置、不入库）。模式清单在仓库之外
  `~/.config/homeworks/privacy-patterns`（权限 600），**绝不把清单本身提交进任何仓库**，否则检查工具
  自己就成了新的泄露点。核查输出只报「第 N 号模式 + 掩码上下文」，真名不回显。
  计算物理报告封面的姓名/学号是交付物需要的；激光原理的小组名单在源码里写成 `成员A…成员E` 占位，
  真实名单只留在已提交的那份交付件里。
- 没有 CI、没有根级任务配置，别臆造根目录命令。唯一的钩子是可选的隐私核查
  （`scripts/hooks/pre-commit`，要 `git config core.hooksPath scripts/hooks` 才生效，不配置模式清单时只提示不拦）。
- Typst 报告封面按课程分模式（`simple-zjut-report:0.3.0` 的 `mode`）：
  **计算物理 = `mode: "individual"`**（课程 / 专业班级 / 姓名 / 学号 / 报告日期，课程名传 `course:`），
  **激光原理 = `mode: "group"`**（课程 / 专业班级 / 成员 / 小组 / 报告日期，用 `course:` 与 `group:`）。
  0.3.0 起**取消字段借用**：空字段整行不印，`class` 不再顶替 `course`、`student-id` 不再顶替 `group`，
  所以别再把课程名塞进 `class`、把学号塞进 `student-id`。
- Typst 源码里**一个中文段落写一行，别硬折行**：段落内单个换行会被折成一个空格（只有空行断段，
  见官方 syntax 页「Paragraph break ← Blank line」），西文看不出代价、中文会多出约一个字宽的缝。
  本机 typst 0.15.1 实测 `你好↵世界` 渲染出 3.0pt 缝隙（12pt 字号下一个空格），与 `text.lang` 无关；
  上游 typst/typst#792 提的这条由 PR #7350（2026-09-18 合并）修，尚未进发布版。
  `check_reports.py` 会列出「中文续行」提示（只提示、不判失败，已交出去的报告不回头追改）。
- Typst 的注释是 `//` 和 `/* */`，**`%` 不是注释符**（那是 LaTeX 的习惯）；`%` 在 markup 和数学里
  都直接印出百分号，写 `\%` 反而在数学字符串 `$"50\%"$` 里会露出反斜杠。
- 改完模板或任何一份报告的封面参数，跑 `python3 scripts/check_reports.py`：
  它把 8 份报告全部编译到 `/tmp`（不覆盖交付 PDF），核对封面字段的顺序、取值、有没有串栏或溢出到第 2 页。
  模板仓库改了版本号要重跑 `scripts/typst-local-packages.sh`，否则老引用会报 manifest 版本不匹配。

## Condense_Physics

- `./build.sh` 用 `npx --yes esbuild@0.23.1` 打包 `src/*.js` 并内联 three.js（首次需 Node + 联网）。
  产出的 `crystal-cells.html`、`diamond-zincblende.html`、`hcp-close-packed.html` **是生成物，不要手改**；
  改 `src/` 再重建。开发版源码页是 `index.html`+`app.js`、`diamond.html`+`diamond-app.js`、`hcp.html`+`hcp-app.js`。
- 校验：`./test/run.sh`（用 esbuild 打包 `test/verify.js` 再跑，检查 WS 几何、堆积率、分步差异、DOM id 对应）。
  性能基准：`./test/bench.sh`。
- `test/verify.js` 会因两种写法报错：某一步只变动 <1 或 >4 个图层；以及 `src/` 里直接 `new THREE.SphereGeometry(...)`
  ——几何体 / 材质要用 `view.js` 的缓存。
- 出视频：`node tools/make-video.js [名称] [--gif] [--smoke N]`，依赖 Chrome（`CHROME=` 覆盖路径）+ ffmpeg。

## Computational_Physics/alpha_scattering

- 严格 ISO C89：`-std=c89 -pedantic-errors -Wall -Wextra`。即禁用 `//` 注释、禁止在 `for` 里声明变量、声明须置于块首。
- `CMakeLists.txt` 故意写显式 `-std=c89` 而不用 `set(CMAKE_C_STANDARD 89)`（CMake 对 GCC 16 会误报不支持）——**别去“修”它**。
- 一键：`sh run.sh`（编译 → 积分 → `plot.py` 出图）；只编译：`sh build.sh`。程序把数据写到相对于 CWD 的 `data/`。
- `alpha_scattering_single.c` 由 `python3 make_single_source.py` 从 `src/` **生成**；改完 `src/` 必须重跑，否则两份产物不一致。
- `plot.py` 需要 numpy/matplotlib；面向老师的数据 `data/*.dat` 和 `trajectories.png` 已提交。

## Computational_Physics/heat_diffusion

- 铁棍方波初值，Dirichlet / 周期两种边界。严格 C89 且 **`-Werror`**（`sh build.sh`）；
  C89 **没有 `isfinite` / `NAN`**（那是 C99），判有限用 `util.h` 的 `util_is_finite`
  （`v != v` 与 `v - v != 0`），核对里的哨兵值 likewise 自己造（`verify.c` 的 `V_NA`）。
- 分工是硬的：C 只算并导出 `data/*.dat`（`%.17g`，参数行写成 `key=value`，核对按 key 取不靠行序）；
  `plot.py` / `plot_final.py` **只读文件、不做任何计算**。报告里的数字全部来自程序输出，**不要手改**。
- **改验证逻辑必须走 `./build/heat_diffusion --verify-only data`**。直接跑会先重算并覆盖 `data/`，
  代入式核对就变成验自己刚写出的数据、篡改完全测不出来（这条是踩出来的：篡改跑成 0 项失败才加的入口）。
- `heat_diffusion_single.c` 由 `python3 make_single_source.py` 从 `include/`+`src/` **生成**，改完 `src/` 要重跑。
  它同时是**静态作用域检查器**：两份同名 `static` 函数分开编各看各的，并成一个翻译单元才重定义
  （`value_is_finite` 就是这样被逼出来抽成 `util.c` 的）。
- 回归：`sh test/run.sh`，七层 301 项 + 三套负向对照，约 20 s。任何一层「跑通了」都不算证据，
  判据要能失败；`test/run.sh` 的第 7 步就是专门去把它弄坏来确认会报 FAIL。
- 几何容差有**两个相反方向**的要求，别只记一边：`HOT_LO`/`HOT_HI` 与节点比大小**必须带** `GEOM_TOL`
  （实测差 `7e-18`，不带就漏掉半个初值）；但判「这个节点是不是半格」**不能用精确相等**
  （Δx=0.002 那个满格节点实测 `99.999999999999744`，`==` 会把求和尾巴读成真实半格）。
- 收敛判据只能用「离**物理终态**还差多少」（Dirichlet 是 0 ℃、周期是 10 ℃，由边界条件推出，不是取 0 了事）；
  「逐 step 变化量小于阈值」在慢衰减问题上松 `τ/Δt = 4405` 倍，会提前一半时间停手。
- 出交付 PDF：`typst compile --root . doc/report.typ doc/report.pdf`。图片路径是 `../data/*.png`，
  **漏掉 `--root .` 会报 `would escape the project root`**。
- 图与 PDF **都不入库**（仓库政策：只放源码与 `data/*.dat`）。8 张图由 `sh test/run.sh` 或
  `python3 plot_final.py data` 从 `.dat` 重出，报告由 `typst compile --root . doc/report.typ doc/report.pdf` 重出。
  **别再写 `git add -f`**；交付用的 PDF 留本地，交完可删。`data/fig_*.pdf` 是矢量副本、报告不引用。

## Laser_Principle_with_Application

- 文件清单、命令和排版说明以其 `README.md` 为准。
- 模板**不在本仓库**，是外部 `@local` Typst 包。先注册：`bash ../scripts/typst-local-packages.sh`
  （把 `$HOME/Git/zjut-phy-hwk`、`$HOME/Git/ZJUT-Report-Typst` 软链到 `~/.local/share/typst/packages/local`；
  `--check` 只看状态）。导入带版本号：`zjut-phy-hwk:0.2.0`、`simple-zjut-report:0.3.0`。
- 编译：`typst compile shortSemester1.typ`；工程任务的报告统一是
  `cd resonator_design && typst compile --root . taskNN/report.typ`（Task 01 交 `task01/report.typ`）。
- `resonator_design/` 是**代码平铺 + 交付按 task 分目录**：顶层放 `cavity_core.py`（纯计算 +
  `DESIGN`/`COMPARATORS`，**改设计只动这里**）、`cavity.py` + `report_figs.py`（Task 01）、
  `task2_foxli.py`（Task 02）、`task3_gauss.py`（Task 03）；每个任务的交付在 `taskNN/` 里，
  文件名固定为 `report.typ` / `numbers.typ` / `results.json` / `figures/`。新增任务照抄这套结构，
  脚本继续放顶层（放进子目录就得处理跨目录 import）。`uv run <脚本>` 在 `resonator_design/` 里执行，
  重算并覆盖对应 `taskNN/` 的 `results.json`、`numbers.typ`、`figures/`；报告数字全部
  `#import "numbers.typ"`，**不要手改报告里的数字**。这台机器 system `python3` 缺 scipy，只能走 `uv run`。
- `task2_foxli.py` / `task3_gauss.py` 已改为复用 `cavity_core.py` 的 `eigen_q` / `w_from_q` / `rt_matrix`，
  **不要再各抄一份 helper**；`task3_gauss.py` 另外 import `task2_foxli` 并读 `task02/results.json`，
  保证与 Task 02 同源。
- 题目与已交 PDF 在 `refs/`；未同步的旧长版在 `archive/report_long.typ`。
- 当前设计是**对称近共焦**：L = 550 mm、R₁ = R₂ = 600 mm（g₁g₂ = 0.0069，距共焦简并点 50 mm、
  距失稳区 550 mm）。`archive/report_long.typ`（15 页长版）正文还停留在旧设计 800/1200，**直接重编译会图文矛盾**，
  文件头有警示注释；日常交 `task01/report.typ`。
- 腔长裕度有两个量别混：`margin_L`（到真正失稳区）与 `margin_crit`（到 g1g2=0/1 的临界简并点）。
  对称腔的 L=R 只是 g1g2 触 0 弹回的相切点，**两侧都稳定**，老版本脚本曾把它当失稳边界，已修。

## Forum（Go / Gin）

- 模块名是 `Forum`；必须在 `Forum/` 目录里运行，因为 `config` 包在 import 时就读 CWD 下的 `./config.yaml`（读不到直接 fatal）。
  服务监听 `:9090`，路由分 `/api/user`、`/api/student`、`/api/admin`，日志写 `app.log`。
- `database.Init()` 连接 `127.0.0.1:3306` 的 MySQL/MariaDB（库名、账号见 `config.yaml`）并自动建表；**没有匹配的数据库则起不来**。
- 测试在 `internal/controllers/authControllersTest.go`（如 `go test ./internal/controllers/`）。
  注意测试进程的 CWD 是包目录，同样受上面的 `config.yaml` 加载逻辑影响。
