# AGENTS.md

个人大学作业仓库（ZJUT 应用物理）。**不是 monorepo**：每个顶层目录是一门课、一套自己的工具链，互相独立。
根目录没有 README 之外的总入口，没有统一的 build / test / lint / CI，也没有根 `package.json` / `pyproject.toml`
——永远先 `cd` 进对应子目录，用该目录自带的脚本。文档、注释、提交信息都用中文，新写的内容保持一致。

## 目录

| 路径 | 内容 |
|---|---|
| `Condense_Physics/` | 固体物理：three.js 晶胞可视化（`src/` → 打包成单文件 HTML） |
| `Computational_Physics/alpha_scattering/` | C89 + RK4 α 散射轨道计算（模块化 + 生成的单文件版） |
| `Laser_Principle_with_Application/` | Typst 作业：相干长度调研 + 两镜谐振腔设计报告 |
| `Forum/` | Go/Gin 论坛后端，需要 MySQL/MariaDB |
| `matlab/`、`python/`、`C-exp/`、`Mathematical_Physical_Methods/` | 零散作业、笔记、实验图，无构建系统 |
| `stdserve-Back_end/Back-end` | 空 gitlink，忽略 |

## 全仓库注意

- 根 `.gitignore` 末尾自定义了 `*.zip`、`*.png`、`*.pdf`。**新生成的图 / PDF / 视频不会出现在 `git status` 里**；
  要提交用 `git add -f <文件>`。大量 PNG/PDF 是加这条规则之前就已跟踪的。
- 当前 `Laser_Principle_with_Application/` 整个目录还未被 git 跟踪。
- 没有 CI、没有 pre-commit、没有根级任务配置，别臆造根目录命令。

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

## Laser_Principle_with_Application

- 文件清单、命令和排版说明以其 `README.md` 为准。
- 模板**不在本仓库**，是外部 `@local` Typst 包。先注册：`bash ../scripts/typst-local-packages.sh`
  （把 `$HOME/Git/zjut-phy-hwk`、`$HOME/Git/ZJUT-Report-Typst` 软链到 `~/.local/share/typst/packages/local`；
  `--check` 只看状态）。导入带版本号：`zjut-phy-hwk:0.2.0`、`simple-zjut-report:0.1.1`。
- 编译：`typst compile shortSemester1.typ`；报告需 `cd resonator_design && typst compile --root . report.typ`。
- `resonator_design/cavity.py` 是 PEP 723 脚本，用 `uv run cavity.py` 运行。它会重算并覆盖 `figures/`、`results.json`、
  `summary.typ`；`report.typ` 的数字全部 import 自 `summary.typ`，所以**不要手改报告里的数字**，重跑脚本即可。

## Forum（Go / Gin）

- 模块名是 `Forum`；必须在 `Forum/` 目录里运行，因为 `config` 包在 import 时就读 CWD 下的 `./config.yaml`（读不到直接 fatal）。
  服务监听 `:9090`，路由分 `/api/user`、`/api/student`、`/api/admin`，日志写 `app.log`。
- `database.Init()` 连接 `127.0.0.1:3306` 的 MySQL/MariaDB（库名、账号见 `config.yaml`）并自动建表；**没有匹配的数据库则起不来**。
- 测试在 `internal/controllers/authControllersTest.go`（如 `go test ./internal/controllers/`）。
  注意测试进程的 CWD 是包目录，同样受上面的 `config.yaml` 加载逻辑影响。
