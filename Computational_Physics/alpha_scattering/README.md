# α 粒子散射轨道计算（C89 + RK4）

## 1. 题目

原子核固定在 (0, 0)，带电量 97e；α 粒子相对分子质量 4，带电量 2e。
使用约化单位：电荷单位 e，质量单位相对分子质量，长度单位 A，库仑常数 k = 1。

计算以下六条运动轨迹并画图：

| 组 | 初始位置 | 初始速度 |
|---|---|---|
| (1) | (-20, 20) | (1, 0)、(1.5, 0)、(3, 0) |
| (2) | (-20, -20) | (1, 0)、(1.5, 0)、(3, 0) |

画图要求：横纵坐标限制在 -22.5 ~ 22.5；不同轨迹在颜色或虚实上有区别；每条曲线有文字标识。

## 2. 物理建模

核固定，只需求 α 粒子在核的库仑斥力场中的运动。约化单位下 k = 1，故

```
        k q₁ q₂       97 × 2         r⃗          48.5 · r⃗
F⃗ = ────────── r̂ = ────────── r̂ = 194 ─────  ⇒  a⃗ = ────────────
          r²             r²            r³          r³
```

其中 `r⃗ = (x, y)` 是 α 粒子相对核的位置矢量，`r = |r⃗| = √(x² + y²)`。
质量 m = 4 已并入加速度系数（194 / 4 = 48.5）。

写成 RK4 需要的一阶显式系统，状态向量 `y = {x, y, vₓ, v_y}`：

```
dx/dt  = vₓ
dy/dt  = v_y
dvₓ/dt = 48.5 · x / (x² + y²)^(3/2)
dv_y/dt = 48.5 · y / (x² + y²)^(3/2)
```

两个守恒量用来验收：机械能 `E = ½m(vₓ² + v_y²) + 194/r`，角动量 `L = m(x·v_y − y·vₓ)`。
中心力场下二者都应严格不变。

## 3. 模块划分

| # | 模块 | 文件 | 职责 |
|---|---|---|---|
| 0 | 算例定义 | `cases.h / cases.c` | 六组初值与图上文字标签，集中一处 |
| 1 | 单位与常量 | `units.h` | 约化单位约定、`k q₁q₂`、绘图框 ±22.5、奇点保护 |
| 2 | 库仑场 | `coulomb.h / coulomb.c` | 力、势能、位径。只到「力」为止，与入射粒子无关 |
| 3 | 动力学 | `dynamics.h / dynamics.c` | `a = F/m`、动能。质量只出现在这一层 |
| 4 | 手动积分 | `stepper.h / stepper.c` | 位置用新旧速度平均的显式格式，1 次力调用/步 |
| 5 | 四阶 Runge-Kutta | `rk4.h / rk4.c` | 经典四级格式，4 次力调用/步。两种格式接口一致，可切换 |
| 6 | 轨道推进 | `trajectory.h / trajectory.c` | 串联上面各层：定步长推进、终止判据、采样、守恒量诊断 |
| 7 | 解析对照 | `rutherford.h / rutherford.c` | 卢瑟福解析解：渐近撞击参数、偏心率、偏转角、最近距离 |
| 8 | 数据导出 | `export.h / export.c` | 写 `traj_*.dat`（5 列 + `#` 注释头）与 `summary.md/.txt` |
| 9 | 主程序 | `main.c` | 调度六条轨迹，打印两张表 |
| 10 | 可视化 | `plot.py` | 读 `.dat` 作图，满足题目的坐标范围、颜色虚实、文字标识要求 |

解耦要点：`rk4.c` 与 `stepper.c` 都不知道这是库仑力，`coulomb.c` 不知道什么是步长，
`dynamics.c` 是唯一知道粒子质量的地方。换用力律只动 `coulomb.c`，
换积分方法只动 `stepper.c` / `rk4.c`，`trajectory.c` 里换个函数指针即可。

## 4. 构建与运行

### CLion（走 CMake）

1. `File → Open`，直接选中本目录（有 `CMakeLists.txt` 的那一层），CLion 会识别成
   CMake 工程并自动 Reload。
2. 首次打开如果提示选 toolchain，选默认的 `GCC` 即可（Linux 上就是系统 gcc，
   Windows 上选 Dev-C++ 附带的 MinGW 也行）。
3. 右上角运行配置选 `alpha_scattering`，点 `Edit Configurations…` →
   `Working directory` 填成本目录（**很重要**：程序往 `data/` 写文件，
   工作目录不对就会提示"目录不存在或不可写，改写到当前目录"）。
4. 想在程序里换步长或时长，在同一个对话框的 `Parameters` 里填，例如
   `-t 14 -dt 0.002` 或 `--manual -dt 0.02`。
5. `Build → Build 'alpha_scattering'` 后运行，终端会打印两张表，
   `data/` 下生成 6 个 `.dat` 与 `summary.md/.txt`。

`CMakeLists.txt` 里锁 C89 的核心就是这一行：

```cmake
add_compile_options(-std=c89 -pedantic-errors -Wall -Wextra -O2)
```

`-pedantic-errors` 会拒掉一切非 ISO C89 写法，实测确实生效：

```
$ gcc -std=c89 -pedantic-errors -c c99_demo.c
error: C++ 风格的注释在 ISO C90 中不被允许
error: 'for' loop initial declarations are only allowed in C99 or C11 mode
```

**一个会踩到的 CMake 坑：** 直觉上该写 `set(CMAKE_C_STANDARD 89)`，
但在 CMake 4.4 + GCC 16 这套组合下会直接 configure 失败：

```
Target "alpha_scattering" requires the language dialect "C89".
But the current compiler "GNU" does not support this, or CMake does not know the flags to enable it.
```

这是 CMake 的编译器特性表没有 GCC 16 这个新版本号导致的误报——
`gcc -std=c89` 本身完全可用。**解决办法就是绕开那张表，显式写 `-std=c89`**，
`-std=c89`（而非 `-std=gnu89`）同时保证了 `CMAKE_C_EXTENSIONS OFF` 的等价效果。
`CMakeLists.txt` 里用 `check_c_compiler_flag` 做了一次探测，注释也写明了原因。

### 单文件版（给不在 IDE 里建工程的人）

正式交付是 `src/ + include/` 的分模块版本（题目要求分模块）。
但要在 Dev-C++ 里一次加 9 个源文件对非开发环境不太友好，
所以额外提供 `alpha_scattering_single.c` —— 一个文件、按 F11 就能跑：

```bat
gcc -std=c89 -pedantic-errors -Wall -Wextra -O2 alpha_scattering_single.c -o alpha.exe -lm
```

它由 `python3 make_single_source.py` 从模块化源码合并生成
（按依赖顺序先放所有声明、再放各模块实现，剥掉 include guard，
标准库头只留一份）。两份产物输出**逐字节相同**，比对方法：

```sh
./build/alpha_scattering /tmp/mod    --ascii
./alpha_single           /tmp/single --ascii
diff -r /tmp/mod /tmp/single          # 只有路径名一行不同
```

改了 `src/` 里任何代码之后，记得重跑一次 `make_single_source.py` 再打包。

### Dev-C++ / Windows MinGW

代码是纯 ISO C89，只用标准库（`stdio.h stdlib.h string.h math.h`），
没有 POSIX 依赖，Dev-C++ 能直接编。两种做法：

**A. 建工程（推荐，能一次编完所有模块）**

1. `File → New → Project…` → 选 `Project` → `Empty Project`，
   存到本目录，类型选 `C project`。
2. 把 `src/` 下 9 个 `.c` 全部 `Add → Existing Source` 进工程
   （只加 `main.c` 会链接失败，其余模块都在别的 `.c` 里）。
3. **控制台中文乱码？** Dev-C++ 的控制台默认是 GBK 代码页，UTF-8 中文会花屏。
   程序在 `_WIN32` 下已自动切成英文文案，也可以手动控制：
   `--ascii` 强制英文、`--utf8` 强制中文，或者事先 `chcp 65001` 换代码页。
   数据文件与 `summary.*` 本来就是纯 ASCII，不受影响。
4. `Project Options → Parameters` 里：
   - `C Compiler` 追加 `-std=c89 -pedantic-errors -Wall -Wextra`（想严格验证才加，
     不加也能编，代码本身就符合 C89）；
   - `Files` 里确认链接了 `libm`（MinGW 通常自动带上，若报 `undefined reference to sqrt`
     就在 `Linker` 里加 `-lm`）。
5. `Project Options → Application → Working directory` 设为本目录，
   或者干脆不管——工作目录不对时程序会自动把数据写到当前目录并打印提示。
6. `Execute → Compile & Run`，控制台会打印两张表。

**B. 命令行一行搞定**

```bat
gcc -std=c89 -pedantic-errors -Wall -Wextra -O2 -Iinclude src\*.c -o alpha.exe
alpha.exe out
alpha.exe out --ascii        :: 控制台用英文
```

命令行参数一览：`[输出目录] [-t 时长] [-dt 步长] [--manual] [--ascii|--utf8]`。

`plot.py` 只是出图用，老师那边不跑 Python 也没关系——
`data/trajectories.png` 已经是成品图，`.dat` 里是原始数据。

### 命令行（Linux）

```sh
sh run.sh          # 编译 -> 积分 -> 作图，三步一次完成
```

或分步：

```sh
sh build.sh                            # 只编译，验证 C89 零警告
./build/alpha_scattering data          # 积分并写数据（默认 t_max=14, dt=0.005）
./build/alpha_scattering data -dt 0.002 -t 14     # 更小步长
./build/alpha_scattering data --manual -dt 0.02   # 换手动积分格式
python3 plot.py data                   # 作图
```

## 5. 结果与验证

### 六条轨迹

| case | 初始位置 | v₀ | E | L | r_min 数值 | r_min 解析 | 偏转角（框内） | 卢瑟福渐近偏转角 |
|---|---|---|---|---|---|---|---|---|
| 1 | (-20, +20) | 1.0 | 8.8589 | −80 | 25.44745 | 25.44745 | 58.94° | 98.09° |
| 2 | (-20, +20) | 1.5 | 11.3589 | −120 | 23.75101 | 23.75101 | 39.58° | 68.30° |
| 3 | (-20, +20) | 3.0 | 24.8589 | −240 | 21.36226 | 21.36226 | 17.96° | 25.83° |
| 4 | (-20, -20) | 1.0 | 8.8589 | +80 | 25.44745 | 25.44745 | 58.94° | 98.09° |
| 5 | (-20, -20) | 1.5 | 11.3589 | +120 | 23.75101 | 23.75101 | 39.58° | 68.30° |
| 6 | (-20, -20) | 3.0 | 24.8589 | +240 | 21.36226 | 21.36226 | 17.96° | 25.83° |

### 四重验证

1. **能量与角动量守恒**：六条轨迹全程取最大偏差，`max|ΔE|/E = 5.6e-15`、
   `max|ΔL|/|L| = 8.5e-15`（都出现在 v₀ = 3.0 那组），已经是双精度舍入水平，
   说明 dt = 0.005 足够。逐条数值见各数据文件头部的 `# invariant` 行。
2. **与解析解对照**：最近距离数值 vs 卢瑟福解析值吻合到 6 位以上
   （残差 6e-8 ~ 1.1e-6，来自采样点不正好落在精确拱点上，不是积分误差）。
3. **步长减半收敛性**：`-dt` 依次取 h、h/2、h/4，比较同一时刻的轨道坐标，
   RK4 的误差比稳定在 16 附近（实测 16.2、16.1，即四阶）；
   作为对照，`--manual` 的平均速度格式误差比是 2.00（一阶），
   等代价（同样力调用次数）下精度差 4~7 个数量级。
4. **镜像对称**：case 1/4、2/5、3/6 的 E、|L|、r_min、出框点 y 符号相反、
   数值完全相同——这是初值设计白送的正确性检验。

### 两个容易踩的物理坑

- **几何撞击参数 b = |y₀| = 20 不是卢瑟福公式里的 b。**
  粒子起始位径 √(−20)² + 20² = 28.28，已经在相互作用区内部，
  真正的渐近撞击参数是 `b = L/(m·v_∞)`，算出来是 9.50 / 12.59 / 17.02。
  若直接把 20 代进卢瑟福公式，偏转角会算错。
- **上下两组轨迹关于 x 轴严格镜像。** 因为初速度沿 x、初始位置只差 y 的符号，
  而力是中心力。这本身就是程序正确性的一个免费检验：
  程序里 case 1 与 case 4 的 r_min、E、|L| 完全相同。

## 6. 输出文件

```
data/
  traj_1.dat ... traj_6.dat    每条轨迹 5 列：t x y vx vy
                               头部 # 注释存初值、单位、积分设置、守恒量、解析对照
  summary.md / summary.txt     数值解 vs 卢瑟福解析解汇总表
  trajectories.png             提交用图（200 dpi，1560×1560）
  trajectories.pdf             同一张图的矢量版
```
