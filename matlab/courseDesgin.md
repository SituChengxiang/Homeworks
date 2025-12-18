# couseDesign

课程设计

## 废话

### 研究内容

多步法 Adams方法  
单步法 休恩方法、RK方法……

#### **1. 研究背景**

双摆系统是经典力学中**最简单的混沌系统**之一，由两个单摆通过铰链串联构成。其历史可追溯至18世纪欧拉对刚体动力学的研究，但直到20世纪80年代混沌理论兴起后，双摆的非线性动力学特性才被系统研究。  

- **理论地位**：双摆是**三自由度哈密顿系统**（平面运动下为二自由度）的典型代表，其运动方程无法解析求解，需依赖数值方法。  
- **实验实现**：从早期机械式双摆（如Foucault摆的变体）到现代气浮导轨双摆，其实验装置已成为非线性动力学教学的标准教具。  
- **计算需求**：双摆的长期行为预测需高精度数值积分（如Runge-Kutta-Fehlberg方法），是检验数值算法稳定性的经典案例。

#### **2. 研究意义**

| **层面**        | **具体意义**                                                                 |
|-----------------|-----------------------------------------------------------------------------|
| **理论意义**    | - 揭示确定性系统中混沌现象的产生机制（对初值敏感性、Lyapunov指数正定）<br>- 验证KAM定理在弱非线性系统中的适用边界<br>- 为高维耦合振子系统（如分子振动、电网同步）提供简化模型 |
| **应用价值**    | - 机器人双臂运动规划中的碰撞规避算法设计<br>- 地震工程中高层建筑减振器的参数优化<br>- 航天器姿态控制中柔性附件的振动抑制 |
| **教学价值**    | - 直观展示拉格朗日力学与哈密顿力学的建模差异<br>- 训练数值计算能力（ODE求解、相空间重构）<br>- 培养对非线性现象的物理直觉（分岔、吸引子） |

> **注**：本课程设计聚焦**平面复合双摆**（竖直向下为y轴正方向），其运动方程如前述推导，为后续数值模拟提供理论基础。

---

### 研究内容（课程设计任务分解）

1. 理论建模：建立双摆系统的拉格朗日量（含广义坐标 $\theta_1, \theta_2$ 定义）
，推导二阶非线性微分方程组（运动方程）， (可选)验证小角度近似下的线性化解析解（简正模频率与振型）

2. 数值模拟：将运动方程转化为一阶ODE系统，选一个初始条件，采用并使用 Euler 方法，Heun 方法，3阶和4阶Runge-Kutta 方法，2阶和4阶显式、4阶隐格式 Adams 方法对微分方程进行数值求解

3. 采用 `ode45` 求解器进行数值积分（相对误差容限 $10^{-6}$）得到较精确解求解，对于给定参数的初始条件，比较不同算法的误差随着步长h，时间t的变化规律。

4. 选择一种高精度算法，分析双摆轨迹的变化规律（并绘制分岔图、吸引子图、Poincaré截面图。）

- **参数设置**：  

  ```matlab
  % 系统参数（典型值）
  l1 = 1.0; l2 = 0.8;    % 杆长 (m)
  m1 = 1.5; m2 = 1.0;    % 质量 (kg)
  g = 9.8;               % 重力加速度 (m/s²)
  ```

- **初值敏感性测试**：  
  - 对比初值 $(\theta_1,\theta_2) = (0.1, 0.1)$ 与 $(0.1001, 0.1)$ 的轨迹差异  
  - 计算Lyapunov指数（通过相邻轨迹分离速率）

#### **3. 可视化分析**

| **可视化类型**       | **实现要点**                                  | **物理意义**                     |
|----------------------|---------------------------------------------|--------------------------------|
| **时域响应**         | 绘制 $\theta_1(t), \theta_2(t)$ 曲线          | 观察准周期/混沌振荡特征         |
| **相空间轨迹**       | 绘制 $(\theta_1, \dot{\theta}_1)$, $(\theta_2, \dot{\theta}_2)$ 相图 | 识别周期轨道与奇怪吸引子       |
| **Poincaré截面**     | 取 $\theta_1 = 0$ 且 $\dot{\theta}_1 > 0$ 的截面点集 | 分析相空间结构分形维数         |
| **能量守恒验证**     | 绘制 $E(t) = T(t) + V(t)$ 曲线              | 检验数值算法保辛性（理想应为常数） |

#### **4. 参数影响研究**

- **质量比影响**：固定 $l_1=l_2=1$，扫描 $m_2/m_1 \in [0.1, 5]$，统计混沌阈值（最大Lyapunov指数转正点）  
- **杆长比影响**：固定 $m_1=m_2=1$，扫描 $l_2/l_1 \in [0.2, 2]$，绘制分岔图（$\theta_1$ 极大值 vs $l_2/l_1$）  
- **能量标度律**：研究总能量 $E$ 与混沌区域占比的关系（$E_c$ 为混沌-规则运动临界能量）

#### **5. 扩展任务（可选）**

- **控制设计**：在 $m_2$ 处添加PID控制器，实现混沌抑制（目标：稳定至 $\theta_1=\theta_2=0$）  
- **随机扰动**：在运动方程中加入白噪声项 $\xi(t)$，研究随机共振现象  

> **交付要求**：  
>
> 1. MATLAB代码（含详细注释，函数封装）  
> 2. 报告（含理论推导、参数设置、结果图表、误差分析）  
> 3. **重点强调**：所有图表需符合期刊格式（Arial字体、线宽≥1.5pt、坐标轴标签含单位）  

#### **6. 数值算法实现与对比**

- **算法实现**（严格按课程要求）：

  | **方法**               | **阶数** | **实现要求**                              |
  |------------------------|----------|-----------------------------------------|
  | Euler方法             | 1阶      | 显式单步法，代码实现 $\mathbf{y}_{n+1} = \mathbf{y}_n + h \mathbf{f}(t_n, \mathbf{y}_n)$ |
  | Heun方法              | 2阶      | 改进Euler法，含预测-校正步骤              |
  | Runge-Kutta 3阶       | 3阶      | 经典Kutta公式（$c_2=1/2, c_3=1$）        |
  | Runge-Kutta 4阶       | 4阶      | 标准RK4公式（$k_1,k_2,k_3,k_4$）         |
  | Adams-Bashforth 2阶   | 2阶      | 两步显式法：$\mathbf{y}_{n+1} = \mathbf{y}_n + \frac{h}{2} [3\mathbf{f}_n - \mathbf{f}_{n-1}]$ |
  | Adams-Bashforth 4阶   | 4阶      | 四步显式法（系数 $[55,-59,37,-9]/24$）  |
  | Adams-Moulton 4阶     | 4阶      | 隐式法，需Newton迭代求解（初值用AB4预测）|

- **误差分析方案**：
  1. **参考解生成**：用 `ode113`（可变阶Adams法，容差 $10^{-12}$）生成高精度参考解 $\mathbf{y}^*(t)$
  2. **误差度量**：
     - 局部误差：$e_{\text{local}}(h) = \| \mathbf{y}_h(t_0 + h) - \mathbf{y}^*(t_0 + h) \|_2$
     - 全局误差：$e_{\text{global}}(t; h) = \| \mathbf{y}_h(t) - \mathbf{y}^*(t) \|_2$
  3. **收敛性验证**：
     - 固定 $t=5$ s，扫描步长 $h \in [0.001, 0.1]$，绘制 $\log(e_{\text{global}})$ vs $\log(h)$ 曲线，斜率应趋近理论阶数
     - 固定 $h=0.01$，绘制 $e_{\text{global}}(t)$ vs $t$ 曲线，观察误差累积特性

#### **4. 参数敏感性分析**

- **初始条件扫描**：
  - $\theta_{10} \in [0, \pi/2]$, $\theta_{20} = \theta_{10} + \delta$ ($\delta \in [-\pi/4, \pi/4]$)
  - 绘制Lyapunov指数热力图（横轴 $\theta_{10}$，纵轴 $\delta$）
- **系统参数影响**：

  | **参数** | **扫描范围**       | **分析指标**               |
  |----------|--------------------|--------------------------|
  | $l_2/l_1$ | $[0.2, 2.0]$      | 混沌阈值能量 $E_c$        |
  | $m_2/m_1$ | $[0.1, 5.0]$      | 最大Lyapunov指数 $\lambda_{\max}$ |
  | $g$      | $[1.6, 24.8]$ (地月到木星) | 周期轨道占比             |

---

### 研究方法与算法理论推导

#### **1. 双摆运动方程的ODE形式**

将二阶方程组转化为一阶系统：
$$
\mathbf{z} = \begin{bmatrix} \theta_1 \\ \theta_2 \\ \omega_1 \\ \omega_2 \end{bmatrix}, \quad
\frac{d\mathbf{z}}{dt} = \mathbf{f}(t, \mathbf{z}) = \begin{bmatrix}
\omega_1 \\
\omega_2 \\
f_1(\theta_1,\theta_2,\omega_1,\omega_2) \\
f_2(\theta_1,\theta_2,\omega_1,\omega_2)
\end{bmatrix}
$$
其中 $f_1, f_2$ 由运动方程解出：
$$
\begin{bmatrix} f_1 \\ f_2 \end{bmatrix} = \mathbf{M}^{-1}(\theta_1,\theta_2) \mathbf{F}(\theta_1,\theta_2,\omega_1,\omega_2)
$$
$\mathbf{M}$ 为质量矩阵，$\mathbf{F}$ 为广义力向量（见前述推导）。

#### **2. 数值方法理论推导**

##### **(1) Euler方法（1阶）**

- **公式**：
  $$
  \mathbf{z}_{n+1} = \mathbf{z}_n + h \mathbf{f}(t_n, \mathbf{z}_n)
  $$
- **截断误差**：$O(h^2)$（局部），$O(h)$（全局）
- **稳定性**：绝对稳定区域为 $|1 + h\lambda| \leq 1$（$\lambda$ 为Jacobian特征值）

##### **(2) Heun方法（2阶）**

- **预测-校正**：
  $$
  \begin{aligned}
  \mathbf{z}^*_{n+1} &= \mathbf{z}_n + h \mathbf{f}(t_n, \mathbf{z}_n) \quad &\text{(Euler预测)} \\
  \mathbf{z}_{n+1} &= \mathbf{z}_n + \frac{h}{2} \left[ \mathbf{f}(t_n, \mathbf{z}_n) + \mathbf{f}(t_{n+1}, \mathbf{z}^*_{n+1}) \right] \quad &\text{(梯形校正)}
  \end{aligned}
  $$
- **截断误差**：$O(h^3)$（局部），$O(h^2)$（全局）

##### **(3) Runge-Kutta 4阶（标准RK4）**

- **公式**：
  $$
  \begin{aligned}
  \mathbf{k}_1 &= \mathbf{f}(t_n, \mathbf{z}_n) \\
  \mathbf{k}_2 &= \mathbf{f}(t_n + \frac{h}{2}, \mathbf{z}_n + \frac{h}{2} \mathbf{k}_1) \\
  \mathbf{k}_3 &= \mathbf{f}(t_n + \frac{h}{2}, \mathbf{z}_n + \frac{h}{2} \mathbf{k}_2) \\
  \mathbf{k}_4 &= \mathbf{f}(t_n + h, \mathbf{z}_n + h \mathbf{k}_3) \\
  \mathbf{z}_{n+1} &= \mathbf{z}_n + \frac{h}{6} (\mathbf{k}_1 + 2\mathbf{k}_2 + 2\mathbf{k}_3 + \mathbf{k}_4)
  \end{aligned}
  $$
- **截断误差**：$O(h^5)$（局部），$O(h^4)$（全局）

##### **(4) Adams-Bashforth 2阶和4阶（显式）**

- **公式**（需前4步值）：
  $$
  \mathbf{z}_{n+1} = \mathbf{z}_n + \frac{h}{24} \left( 55\mathbf{f}_n - 59\mathbf{f}_{n-1} + 37\mathbf{f}_{n-2} - 9\mathbf{f}_{n-3} \right)
  $$
- **启动问题**：前3步需用RK4生成

##### **(5) Adams-Moulton 4阶（隐式）**

- **公式**：
  $$
  \mathbf{z}_{n+1} = \mathbf{z}_n + \frac{h}{720} \left( 251\mathbf{f}_{n+1} + 646\mathbf{f}_n - 264\mathbf{f}_{n-1} + 106\mathbf{f}_{n-2} - 19\mathbf{f}_{n-3} \right)
  $$
- **求解策略**：
  1. 用AB4预测 $\mathbf{z}^*_{n+1}$
  2. Newton迭代：$\mathbf{z}^{(k+1)}_{n+1} = \mathbf{z}^{(k)}_{n+1} - [\mathbf{I} - \frac{h}{720} \cdot 251 \mathbf{J}]^{-1} \mathbf{R}$
     - $\mathbf{J} = \partial\mathbf{f}/\partial\mathbf{z}$ 为Jacobian矩阵
     - $\mathbf{R}$ 为残差向量

#### **3. 误差分析理论框架**

- **局部截断误差（LTE）**：
  $$
  \tau_n = \frac{1}{h} \left[ \mathbf{z}(t_{n+1}) - \Phi(t_n, \mathbf{z}(t_n), h) \right]
  $$
  其中 $\Phi$ 为数值方法的增量函数。
- **全局误差传播**：
  $$
  e_n = \mathbf{z}(t_n) - \mathbf{z}_n = e_{n-1} + h[\mathbf{f}(t_{n-1},\mathbf{z}(t_{n-1})) - \mathbf{f}(t_{n-1},\mathbf{z}_{n-1})] + h\tau_{n-1}
  $$
  由Gronwall不等式得 $\|e_n\| \leq C e^{Lt} \max \|\tau_k\|$（$L$ 为Lipschitz常数）。

#### **4. 混沌量化指标**

- **Lyapunov指数**：
  $$
  \lambda = \lim_{t \to \infty} \frac{1}{t} \ln \frac{\|\delta \mathbf{z}(t)\|}{\|\delta \mathbf{z}(0)\|}
  $$
  其中 $\delta \mathbf{z}$ 为相邻轨迹偏差，通过求解变分方程 $\dot{\delta\mathbf{z}} = \mathbf{J} \delta\mathbf{z}$ 计算。
- **分岔图**：固定 $l_1=l_2=1, m_1=m_2=1$，扫描总能量 $E$，记录 $\theta_1$ 的局部极大值。

---

### MATLAB实现要点（课程设计核心代码框架）

```matlab
%% 1. ODE函数定义（双摆运动方程）
function dzdt = double_pendulum(t, z, params)
    l1 = params.l1; l2 = params.l2;
    m1 = params.m1; m2 = params.m2; g = params.g;
    
    theta1 = z(1); theta2 = z(2);
    omega1 = z(3); omega2 = z(4);
    dtheta = theta1 - theta2;
    
    % 质量矩阵 M * [alpha1; alpha2] = F
    M = [ (m1+m2)*l1, m2*l2*cos(dtheta);
          l1*cos(dtheta), l2 ];
    F = [ -m2*l2*sin(dtheta)*omega2^2 - (m1+m2)*g*sin(theta1);
          l1*sin(dtheta)*omega1^2 - g*sin(theta2) ];
    
    alpha = M \ F;  % 解线性方程组
    dzdt = [omega1; omega2; alpha(1); alpha(2)];
end

%% 2. RK4方法实现
function [t, z] = rk4_solver(ode_func, tspan, z0, h, params)
    t = tspan(1):h:tspan(2);
    z = zeros(length(z0), length(t));
    z(:,1) = z0;
    
    for n = 1:length(t)-1
        k1 = ode_func(t(n), z(:,n), params);
        k2 = ode_func(t(n)+h/2, z(:,n)+h/2*k1, params);
        k3 = ode_func(t(n)+h/2, z(:,n)+h/2*k2, params);
        k4 = ode_func(t(n)+h, z(:,n)+h*k3, params);
        z(:,n+1) = z(:,n) + h/6*(k1 + 2*k2 + 2*k3 + k4);
    end
end

%% 3. 误差分析主程序
params = struct('l1',1,'l2',0.8,'m1',1.5,'m2',1,'g',9.8);
z0 = [0.1; 0.1; 0; 0];  % 初值 [theta1; theta2; omega1; omega2]
tspan = [0, 5];

% 生成高精度参考解
options = odeset('RelTol',1e-12,'AbsTol',1e-14);
[~, z_ref] = ode113(@(t,z) double_pendulum(t,z,params), tspan, z0, options);

% 测试不同步长下的RK4误差
h_list = logspace(-3, -1, 20);
err_global = zeros(size(h_list));

for i = 1:length(h_list)
    h = h_list(i);
    [~, z_num] = rk4_solver(@double_pendulum, tspan, z0, h, params);
    err_global(i) = norm(z_num(:,end) - z_ref(end,:)','inf');
end

% 绘制收敛阶
loglog(h_list, err_global, 'o-', 'LineWidth', 1.5);
hold on; loglog(h_list, h_list.^4, '--', 'Color', 'r'); % 理论4阶线
xlabel('Step size h'); ylabel('Global error at t=5s');
legend('RK4 error', 'O(h^4)', 'Location', 'southwest');
```

> **课程设计交付物**：  
>
> 1. 完整MATLAB代码（含6种数值方法实现）  
> 2. 误差分析报告（含收敛阶验证图、误差-时间演化图）  
> 3. 参数敏感性分析图集（Lyapunov指数热力图、分岔图）  
> 4. **理论推导附录**（含所有数值方法的公式推导与误差分析）  
>
> **注**：隐式Adams-Moulton方法需额外实现Jacobian矩阵计算与Newton迭代，建议作为高阶任务。

## 建模推导

### 复合双摆运动方程推导（竖向量形式与完整耦合项展示）

#### **1. 位置与速度的竖向量表示**

- **小球1位置**：
  $$
  \begin{bmatrix} x_1 \\ y_1 \end{bmatrix} = \begin{bmatrix} l_1 \sin\theta_1 \\ l_1 \cos\theta_1 \end{bmatrix}
  $$
- **小球2位置**：
  $$
  \begin{bmatrix} x_2 \\ y_2 \end{bmatrix} = \begin{bmatrix} l_1 \sin\theta_1 + l_2 \sin\theta_2 \\ l_1 \cos\theta_1 + l_2 \cos\theta_2 \end{bmatrix}
  $$
- **小球1速度**：
  $$
  \begin{bmatrix} \dot{x_1} \\ \dot{y_1} \end{bmatrix} = \begin{bmatrix} l_1 \cos\theta_1 \cdot \dot{\theta_1} \\ -l_1 \sin\theta_1 \cdot \dot{\theta_1} \end{bmatrix}
  $$
- **小球2速度**：
  $$
  \begin{bmatrix} \dot{x_2} \\ \dot{y_2} \end{bmatrix} = \begin{bmatrix} l_1 \cos\theta_1 \cdot \dot{\theta_1} + l_2 \cos\theta_2 \cdot \dot{\theta_2} \\ -l_1 \sin\theta_1 \cdot \dot{\theta_1} - l_2 \sin\theta_2 \cdot \dot{\theta_2} \end{bmatrix}
  $$
- **小球1动能**：
  $$
  T_1 = \frac{1}{2} m_1 (\dot{x_1}^2 + \dot{y_1}^2) = \frac{1}{2} m_1 l_1^2 \dot{\theta_1}^2
  $$
- **小球2动能**：
  $$
  \begin{aligned}
  T_2 &= \frac{1}{2} m_2 (\dot{x_2}^2 + \dot{y_2}^2) \\
  &= \frac{1}{2} m_2 \left[ l_1^2 \dot{\theta_1}^2 + l_2^2 \dot{\theta_2}^2 + 2 l_1 l_2 \cos(\theta_1 - \theta_2) \dot{\theta_1} \dot{\theta_2} \right]
  \end{aligned}
  $$
- **总动能**：
  $$
  T = T_1 + T_2 = \frac{1}{2} (m_1 + m_2) l_1^2 \dot{\theta_1}^2 + \frac{1}{2} m_2 l_2^2 \dot{\theta_2}^2 + m_2 l_1 l_2 \cos(\theta_1 - \theta_2) \dot{\theta_1} \dot{\theta_2}
  $$

- **小球1势能**（重力势能 $V = -mgy$）（注意：$y$ 轴向下为正）：
  $$
  V_1 = -m_1 g y_1 = -m_1 g l_1 \cos\theta_1
  $$
- **小球2势能**：
  $$
  V_2 = -m_2 g y_2 = -m_2 g (l_1 \cos\theta_1 + l_2 \cos\theta_2)
  $$
- **总势能**：
  $$
  V = V_1 + V_2 = -(m_1 + m_2) g l_1 \cos(\theta_1) - m_2 g l_2 \cos(\theta_2)
  $$

---

#### **2. 拉格朗日量（含完整耦合项）**

$$
L = \underbrace{\frac{1}{2} (m_1 + m_2) l_1^2 \dot{\theta_1}^2}_{T_1} + \underbrace{\frac{1}{2} m_2 l_2^2 \dot{\theta_2}^2}_{T_2} + \underbrace{m_2 l_1 l_2 \cos(\theta_1 - \theta_2) \dot{\theta_1} \dot{\theta_2}}_{T_{\text{couple}}} + \underbrace{(m_1 + m_2) g l_1 \cos\theta_1 + m_2 g l_2 \cos\theta_2}_{-V}
$$
**关键项**：$T_{\text{couple}} = m_2 l_1 l_2 \cos(\theta_1 - \theta_2) \dot{\theta_1} \dot{\theta_2}$ 是唯一的速度耦合项。

$$
\begin{aligned}
L &= T - V \\
&= \frac{1}{2} (m_1 + m_2) l_1^2 \dot{\theta_1}^2 + \frac{1}{2} m_2 l_2^2 \dot{\theta_2}^2 + m_2 l_1 l_2 \cos(\theta_1 - \theta_2) \dot{\theta_1} \dot{\theta_2} \\
&\quad + (m_1 + m_2) g l_1 \cos\theta_1 + m_2 g l_2 \cos\theta_2
\end{aligned}
$$

---

#### **3. 广义动量的时间导数（完整耦合项保留）**

##### **(a) 对 $\theta_1$ 的广义动量**

$$
p_{\theta_1} = \frac{\partial L}{\partial \dot{\theta_1}} = (m_1 + m_2) l_1^2 \dot{\theta_1} + \underbrace{m_2 l_1 l_2 \cos(\theta_1 - \theta_2) \dot{\theta_2}}_{\text{耦合项}}
$$
**时间全导数**：
$$
\begin{aligned}
\frac{d}{dt} p_{\theta_1} &= (m_1 + m_2) l_1^2 \ddot{\theta_1} \\
&+ m_2 l_1 l_2 \left[ \frac{d}{dt} \cos(\theta_1 - \theta_2) \right] \dot{\theta_2} + m_2 l_1 l_2 \cos(\theta_1 - \theta_2) \ddot{\theta_2} \\
&= (m_1 + m_2) l_1^2 \ddot{\theta_1} \\
&+ m_2 l_1 l_2 \left[ -\sin(\theta_1 - \theta_2) \cdot (\dot{\theta_1} - \dot{\theta_2}) \right] \dot{\theta_2} + m_2 l_1 l_2 \cos(\theta_1 - \theta_2) \ddot{\theta_2} \\
&= (m_1 + m_2) l_1^2 \ddot{\theta_1} + m_2 l_1 l_2 \cos(\theta_1 - \theta_2) \ddot{\theta_2} \\
&\quad \underbrace{- m_2 l_1 l_2 \sin(\theta_1 - \theta_2) \dot{\theta_1} \dot{\theta_2}}_{\text{交叉耦合项}} \underbrace{+ m_2 l_1 l_2 \sin(\theta_1 - \theta_2) \dot{\theta_2}^2}_{\text{离心项}}
\end{aligned}
$$

- **完整方程**：
  $$
  \begin{aligned}
  &\left[ (m_1 + m_2) l_1^2 \ddot{\theta_1} + m_2 l_1 l_2 \cos(\theta_1 - \theta_2) \ddot{\theta_2} - m_2 l_1 l_2 \sin(\theta_1 - \theta_2) \dot{\theta_1} \dot{\theta_2} + m_2 l_1 l_2 \sin(\theta_1 - \theta_2) \dot{\theta_2}^2 \right] \\
  &\quad - \left[ -m_2 l_1 l_2 \sin(\theta_1 - \theta_2) \dot{\theta_1} \dot{\theta_2} - (m_1 + m_2) g l_1 \sin\theta_1 \right] = 0
  \end{aligned}
  $$
- **耦合项抵消**：
  $$
  \underbrace{- m_2 l_1 l_2 \sin(\theta_1 - \theta_2) \dot{\theta_1} \dot{\theta_2}}_{\text{来自 } d p_{\theta_1}/dt} \quad \text{与} \quad \underbrace{+ m_2 l_1 l_2 \sin(\theta_1 - \theta_2) \dot{\theta_1} \dot{\theta_2}}_{\text{来自 } -\partial L/\partial \theta_1} \quad \text{完全抵消！}
  $$
- **最终简化**：
  $$
  \boxed{(m_1 + m_2) l_1 \ddot{\theta_1} + m_2 l_2 \cos(\theta_1 - \theta_2) \ddot{\theta_2} + m_2 l_2 \sin(\theta_1 - \theta_2) \dot{\theta_2}^2 + (m_1 + m_2) g \sin\theta_1 = 0}
  $$

##### **(b) 对 $\theta_2$ 的广义动量**

$$
p_{\theta_2} = \frac{\partial L}{\partial \dot{\theta_2}} = m_2 l_2^2 \dot{\theta_2} + \underbrace{m_2 l_1 l_2 \cos(\theta_1 - \theta_2) \dot{\theta_1}}_{\text{耦合项}}
$$
**时间全导数**：
$$
\begin{aligned}
\frac{d}{dt} p_{\theta_2} &= m_2 l_2^2 \ddot{\theta_2} \\
&+ m_2 l_1 l_2 \left[ \frac{d}{dt} \cos(\theta_1 - \theta_2) \right] \dot{\theta_1} + m_2 l_1 l_2 \cos(\theta_1 - \theta_2) \ddot{\theta_1} \\
&= m_2 l_2^2 \ddot{\theta_2} \\
&+ m_2 l_1 l_2 \left[ -\sin(\theta_1 - \theta_2) \cdot (\dot{\theta_1} - \dot{\theta_2}) \right] \dot{\theta_1} + m_2 l_1 l_2 \cos(\theta_1 - \theta_2) \ddot{\theta_1} \\
&= m_2 l_2^2 \ddot{\theta_2} + m_2 l_1 l_2 \cos(\theta_1 - \theta_2) \ddot{\theta_1} \\
&\quad \underbrace{- m_2 l_1 l_2 \sin(\theta_1 - \theta_2) \dot{\theta_1}^2}_{\text{离心项}} \underbrace{+ m_2 l_1 l_2 \sin(\theta_1 - \theta_2) \dot{\theta_1} \dot{\theta_2}}_{\text{交叉耦合项}}
\end{aligned}
$$

- **完整方程**：
  $$
  \begin{aligned}
  &\left[ m_2 l_2^2 \ddot{\theta_2} + m_2 l_1 l_2 \cos(\theta_1 - \theta_2) \ddot{\theta_1} - m_2 l_1 l_2 \sin(\theta_1 - \theta_2) \dot{\theta_1}^2 + m_2 l_1 l_2 \sin(\theta_1 - \theta_2) \dot{\theta_1} \dot{\theta_2} \right] \\
  &\quad - \left[ m_2 l_1 l_2 \sin(\theta_1 - \theta_2) \dot{\theta_1} \dot{\theta_2} - m_2 g l_2 \sin\theta_2 \right] = 0
  \end{aligned}
  $$
- **耦合项抵消**：
  $$
  \underbrace{+ m_2 l_1 l_2 \sin(\theta_1 - \theta_2) \dot{\theta_1} \dot{\theta_2}}_{\text{来自 } d p_{\theta_2}/dt} \quad \text{与} \quad \underbrace{- m_2 l_1 l_2 \sin(\theta_1 - \theta_2) \dot{\theta_1} \dot{\theta_2}}_{\text{来自 } -\partial L/\partial \theta_2} \quad \text{完全抵消！}
  $$
- **最终简化**：
  $$
  \boxed{l_2 \ddot{\theta_2} + l_1 \cos(\theta_1 - \theta_2) \ddot{\theta_1} - l_1 \sin(\theta_1 - \theta_2) \dot{\theta_1}^2 + g \sin\theta_2 = 0}
  $$

---

#### **4. 运动方程矩阵形式（竖向量标准写法）**

将方程组写为 $\mathbf{M} \ddot{\boldsymbol{\theta}} = \mathbf{F}$ 形式：
$$
\begin{bmatrix}
(m_1 + m_2) l_1 & m_2 l_2 \cos(\theta_1 - \theta_2) \\
l_1 \cos(\theta_1 - \theta_2) & l_2
\end{bmatrix}
\begin{bmatrix} \ddot{\theta_1} \\ \ddot{\theta_2} \end{bmatrix}
=
\begin{bmatrix}
- m_2 l_2 \sin(\theta_1 - \theta_2) \dot{\theta_2}^2 - (m_1 + m_2) g \sin\theta_1 \\
l_1 \sin(\theta_1 - \theta_2) \dot{\theta_1}^2 - g \sin\theta_2
\end{bmatrix}
$$
其中：

- **质量矩阵 $\mathbf{M}$**：描述惯性耦合
- **力向量 $\mathbf{F}$**：包含离心力（$\dot{\theta}_i^2$ 项）和重力项

---

#### **5. 关键物理机制总结**

| 项类型 | 数学形式 | 物理意义 |
|--------|----------|----------|
| **耦合项** | $\cos(\theta_1 - \theta_2) \ddot{\theta}_j$ | 两摆的加速度通过连杆耦合 |
| **离心力** | $\sin(\theta_1 - \theta_2) \dot{\theta}_i^2$ | 高速旋转时产生的惯性力 |
| **重力恢复力** | $g \sin\theta_i$ | 重力试图将摆拉回竖直位置 |
| **交叉速度项** | $\sin(\theta_1 - \theta_2) \dot{\theta_1} \dot{\theta_2}$ | **在完整方程中精确抵消**，不显式出现 |

> **推导验证**：  
>
> 1. 耦合项 $\dot{\theta_1} \dot{\theta_2}$ 在 $\frac{d}{dt}(\partial L/\partial \dot{q})$ 和 $\partial L/\partial q$ 中符号相反，**必然抵消**  
> 2. 最终方程仅含 $\dot{\theta_1}^2$ 和 $\dot{\theta_2}^2$ 项（离心力），符合物理直觉  
> 3. 当 $\theta_1 = \theta_2 = 0$（竖直向下），$\sin(0)=0$，方程退化为线性系统  
>
> **数值求解提示**：实际编程时需将二阶方程组转为一阶系统：
> $$
> \begin{bmatrix} \dot{\theta_1} \\ \dot{\theta_2} \\ \ddot{\theta_1} \\ \ddot{\theta_2} \end{bmatrix} =
> \begin{bmatrix}
> \omega_1 \\
> \omega_2 \\
> \text{solve from matrix eq} \\
> \text{solve from matrix eq}
> \end{bmatrix}
> $$

#### 6. 验证与讨论

- **单摆极限**（$m_2 = 0$）：
  第一方程退化为 $(m_1 l_1) \ddot{\theta_1} + m_1 g \sin\theta_1 = 0$，即 $\ddot{\theta_1} + \frac{g}{l_1} \sin\theta_1 = 0$，符合单摆方程。
  
- **小角度近似**（$\sin\theta \approx \theta$, $\cos\theta \approx 1$）：
  $$
  \begin{cases}
  (m_1 + m_2) l_1 \ddot{\theta_1} + m_2 l_2 \ddot{\theta_2} + (m_1 + m_2) g \theta_1 = 0 \\
  l_2 \ddot{\theta_2} + l_1 \ddot{\theta_1} + g \theta_2 = 0
  \end{cases}
  $$
  此线性系统可解耦为简正模，对应同相/反相振动模式。

- **物理意义**：
  - $\sin(\theta_1 - \theta_2)$ 项体现**非线性耦合**
  - $\dot{\theta}_i^2$ 项对应**离心力效应**
  - 两方程相互耦合，需数值求解（如 Runge-Kutta 法）
  - 系统表现出**混沌行为**（对初值敏感）

> **注**：由于 $y$ 轴向下为正，势能项为 $-mgy$。若改用 $y$ 轴向上为正，则所有 $\cos\theta_i$ 项符号反转，但最终运动方程形式不变。此推导严格遵循题目坐标系要求。

## MATLAB算法推导

- 对于混沌系统，不那么精确的欧拉方法必须步长极小才能勉强维持60s的收敛

```matlab
Warning: Matrix is singular, close to singular or badly scaled. Results may be inaccurate. RCOND =
NaN. 
> In main>doublePendulumODE (line 42)
In main>@(tt,zz)doublePendulumODE(tt,zz,params) (line 19)
In eularMethod (line 18)
In main (line 19)
```

---

### **单步法-通用问题设定**

考虑一阶常微分方程初值问题：
$$
\dot{y} = f(t, y), \quad y(t_0) = y_0
$$
目标是求解 $y(t_{n+1}) = y(t_n + h)$，其中 $h$ 为步长。

---

### **1. 显式Euler方法 (Explicit Euler)**

#### **Eular推导**

1. **泰勒展开**：  
    $$
    y(t_{n+1}) = y(t_n) + h \dot{y}(t_n) + \frac{h^2}{2} \ddot{y}(t_n) + \mathcal{O}(h^3)
    $$
2. **代入方程**：$\dot{y}(t_n) = f(t_n, y_n)$  
    忽略二阶及以上导数项（截断误差 $\mathcal{O}(h^2)$）：
    $$
    y(t_{n+1}) \approx y(t_n) + h f(t_n, y_n)
    $$
3. **迭代格式**：
    $$
    y_{n+1} = y_n + h f(t_n, y_n)
    $$

#### **Eular关键特性**

- **局部截断误差 (LTE)**：$\mathcal{O}(h^2)$
- **全局截断误差 (GTE)**：$\mathcal{O}(h)$
- **单步、显式、一阶精度**

---

### **2. Heun方法 (Heun's Method / Improved Euler)**

#### **Heun推导**

1. **梯形法则思想**：  
    积分 $\int_{t_n}^{t_{n+1}} \dot{y}  dt = \int_{t_n}^{t_{n+1}} f(t, y)  dt$，用梯形公式近似：
    $$
    y_{n+1} = y_n + \frac{h}{2} \left[ f(t_n, y_n) + f(t_{n+1}, y_{n+1}) \right]
    $$
    但 $y_{n+1}$ 未知，导致隐式。
2. **显式化**：用Euler法预测 $y_{n+1}^{(0)} = y_n + h f(t_n, y_n)$，代入：
    $$
    y_{n+1} = y_n + \frac{h}{2} \left[ f(t_n, y_n) + f(t_{n+1}, y_n + h f(t_n, y_n)) \right]
    $$
3. **标准格式**：
    $$
    \begin{aligned}
    k_1 &= f(t_n, y_n) \\
    k_2 &= f(t_n + h, y_n + h k_1) \\
    y_{n+1} &= y_n + \frac{h}{2} (k_1 + k_2)
    \end{aligned}
    $$

#### **Heun关键特性**

- **LTE**：$\mathcal{O}(h^3)$ （因梯形法对线性函数精确）
- **GTE**：$\mathcal{O}(h^2)$
- **预测-校正结构，二阶精度**

---

### **3. 三阶Runge-Kutta方法 (RK3)**

#### **RK3推导**

目标是构造一个三阶段方法，使其泰勒展开匹配到 $\mathcal{O}(h^3)$。

1. **通用RK3形式**：
    $$
    \begin{aligned}
    k_1 &= f(t_n, y_n) \\
    k_2 &= f(t_n + \frac{h}{2}, y_n + \frac{h}{2} k_1) \\
    k_3 &= f(t_n + h, y_n - h k_1 + 2h k_2) \\
    y_{n+1} &= y_n + \frac{h}{6} (k_1 + 4k_2 + k_3)
    \end{aligned}
    $$
    *（采用Kutta's 3rd-order scheme）*

2. **验证RK3精度**：  
    将 $k_2, k_3$ 用泰勒展开至 $h^2$ 项：
    $$
    \begin{aligned}
    k_2 &= f + \frac{h}{2} (f_t + f_y f) + \mathcal{O}(h^2) \\
    k_3 &= f + h (-f_t - f_y f + 2f_t + 2f_y f) + \mathcal{O}(h^2) = f + h (f_t + f_y f) + \mathcal{O}(h^2)
    \end{aligned}
    $$
    其中 $f = f(t_n, y_n)$, $f_t = \partial f/\partial t$, $f_y = \partial f/\partial y$。  
    代入 $y_{n+1}$：
    $$
    \begin{aligned}
    y_{n+1} &= y_n + \frac{h}{6} \left[ f + 4\left(f + \frac{h}{2}(f_t + f_y f)\right) + \left(f + h(f_t + f_y f)\right) \right] + \mathcal{O}(h^3) \\
    &= y_n + h f + \frac{h^2}{2} (f_t + f_y f) + \mathcal{O}(h^3)
    \end{aligned}
    $$
    与 $y(t_{n+1})$ 的二阶泰勒展开一致（因 $\ddot{y} = f_t + f_y f$）。

#### **RK3关键特性**

- **LTE**：$\mathcal{O}(h^4)$
- **GTE**：$\mathcal{O}(h^3)$
- **三阶精度，计算量适中**

---

### **4. 四阶Runge-Kutta方法 (RK4)**

#### **RK4推导**

经典RK4是匹配泰勒展开到 $\mathcal{O}(h^4)$ 的最简方案。

1. **RK4算法公式**：
    $$
    \begin{aligned}
    k_1 &= f(t_n, y_n) \\
    k_2 &= f(t_n + \frac{h}{2}, y_n + \frac{h}{2} k_1) \\
    k_3 &= f(t_n + \frac{h}{2}, y_n + \frac{h}{2} k_2) \\
    k_4 &= f(t_n + h, y_n + h k_3) \\
    y_{n+1} &= y_n + \frac{h}{6} (k_1 + 2k_2 + 2k_3 + k_4)
    \end{aligned}
    $$

2. **RK4精度验证**（概要）：  
    - $k_1 = f$  
    - $k_2 = f + \frac{h}{2}(f_t + f_y f) + \frac{h^2}{8}(f_{tt} + 2f_{ty}f + f_{yy}f^2 + f_y f_t + f_y^2 f) + \mathcal{O}(h^3)$  
    - $k_3$ 展开类似 $k_2$  
    - $k_4 = f + h(f_t + f_y f) + \frac{h^2}{2}(f_{tt} + 2f_{ty}f + f_{yy}f^2 + f_y f_t + f_y^2 f) + \mathcal{O}(h^3)$  
    代入 $y_{n+1}$ 并整理，可得：
    $$
    y_{n+1} = y_n + h f + \frac{h^2}{2} \dot{f} + \frac{h^3}{6} \ddot{f} + \frac{h^4}{24} f^{(3)} + \mathcal{O}(h^5)
    $$
    其中 $\dot{f} = f_t + f_y f$, $\ddot{f} = f_{tt} + 2f_{ty}f + f_{yy}f^2 + f_y(f_t + f_y f)$, $f^{(3)}$ 为三阶导数。  
    这与 $y(t_{n+1})$ 的四阶泰勒展开完全一致。

#### **RK4关键特性**

- **LTE**：$\mathcal{O}(h^5)$
- **GTE**：$\mathcal{O}(h^4)$
- **最广泛使用的高精度单步方法，四阶精度**

---

### **单步骤法-精度与效率对比总结**

| 方法          | 阶数 | 每步函数计算次数 | 全局误差     | 适用场景                     |
|---------------|------|------------------|--------------|------------------------------|
| **Euler**     | 1    | 1                | $\mathcal{O}(h)$ | 教学演示，极低精度需求       |
| **Heun**      | 2    | 2                | $\mathcal{O}(h^2)$ | 平衡精度与计算量            |
| **RK3**       | 3    | 3                | $\mathcal{O}(h^3)$ | 对精度要求中等              |
| **RK4**       | 4    | 4                | $\mathcal{O}(h^4)$ | **高精度首选**，工程标准    |

---

### **在双摆问题中的实施要点**

1. **状态向量**：所有方法作用于 $\mathbf{z} = [\theta_1, \theta_2, \omega_1, \omega_2]^T \in \mathbb{R}^4$  
2. **步长选择**：  
   - Euler: $h \leq 0.01$ (否则发散)  
   - RK4: $h = 0.05 \sim 0.1$ 可获高精度  
3. **误差控制**：用`ode113`解作为"真解"，计算其他方法的 $\|\mathbf{z}_{\text{num}} - \mathbf{z}_{\text{ref}}\|_2$

---

### **多步法-通用设定回顾**

考虑初值问题：
$$
\dot{y} = f(t, y), \quad y(t_n) = y_n
$$

在区间 $[t_n, t_{n+1}]$ 上对微分方程积分：
$$
y(t_{n+1}) = y(t_n) + \int_{t_n}^{t_{n+1}} f(t, y(t)) \, dt
$$

**核心思想**：用插值多项式 $p_k(t)$ 近似 $f(t, y(t))$，然后对 $p_k(t)$ 积分。  

- 若 $p_k$ 基于 $t_n, t_{n-1}, \dots$ → **显式法（Adams-Bashforth）**  
- 若 $p_k$ 包含 $t_{n+1}$ → **隐式法（Adams-Moulton）**

设等距节点：$t_j = t_0 + jh$，步长 $h$ 恒定。

---

### **1. Adams-Bashforth 2阶方法（AB2）**

#### **1. AB2插值构造**

用 $t_n, t_{n-1}$ 处的函数值构造线性插值 $p_1(t)$：
$$
p_1(t) = f_n + \frac{f_n - f_{n-1}}{h} (t - t_n)
$$
其中 $f_j = f(t_j, y_j)$。

#### **2. AB2积分近似**

$$
\begin{aligned}
\int_{t_n}^{t_{n+1}} f(t)\,dt
&\approx \int_{t_n}^{t_{n+1}} p_1(t)\,dt
= \int_{t_n}^{t_{n+1}} \left[ f_n + \frac{f_n - f_{n-1}}{h} (t - t_n) \right] dt \\
&= \left[ f_n (t - t_n) + \frac{f_n - f_{n-1}}{2h} (t - t_n)^2 \right]_{t_n}^{t_{n+1}} \\
&= f_n h + \frac{f_n - f_{n-1}}{2h} h^2
= h \left( f_n + \frac{f_n - f_{n-1}}{2} \right) \\
&= \frac{h}{2} (3f_n - f_{n-1})
\end{aligned}
$$

#### **3. 迭代格式**

$$
\boxed{y_{n+1} = y_n + \frac{h}{2} \left( 3f_n - f_{n-1} \right)}
$$

#### **4. 局部截断误差分析**

插值余项：
$$
f(t) - p_1(t) = \frac{f''(\xi)}{2} (t - t_n)(t - t_{n-1}), \quad \xi \in [t_{n-1}, t_{n+1}]
$$
积分余项：
$$
\begin{aligned}
\text{LTE} &= \int_{t_n}^{t_{n+1}} \frac{f''(\xi(t))}{2} (t - t_n)(t - t_{n-1}) \, dt \\
&\approx \frac{f''(\xi)}{2} \int_0^h \tau (\tau - h) \, d\tau
= \frac{f''(\xi)}{2} \left[ \frac{\tau^3}{3} - \frac{h\tau^2}{2} \right]_0^h \\
&= \frac{f''(\xi)}{2} \left( \frac{h^3}{3} - \frac{h^3}{2} \right)
= -\frac{h^3}{12} f''(\xi)
\end{aligned}
$$
故 **LTE = $\mathcal{O}(h^3)$** ⇒ **二阶方法**。

> **注意**：需 $y_0, y_1$ 作为起始值（可用RK2或RK4生成）。

---

### **2. Adams-Bashforth 4阶方法（AB4）**

#### **1. AB4插值构造**

用 $t_n, t_{n-1}, t_{n-2}, t_{n-3}$ 处的 $f$ 构造三次插值 $p_3(t)$。  
更高效：采用**牛顿前向差分插值**。令 $s = \frac{t - t_n}{h}$，则：
$$
p_3(t) = f_n + s \nabla f_n + \frac{s(s+1)}{2!} \nabla^2 f_n + \frac{s(s+1)(s+2)}{3!} \nabla^3 f_n
$$
其中 $\nabla f_n = f_n - f_{n-1}$, $\nabla^2 f_n = \nabla f_n - \nabla f_{n-1}$, etc.

### **2. AB4积分变换**

积分变量替换：$t = t_n + sh$, $dt = h\,ds$, $s \in [0,1]$：
$$
\int_{t_n}^{t_{n+1}} f(t)\,dt = h \int_0^1 p_3(t_n + sh)\, ds
$$

计算积分：
$$
\begin{aligned}
\int_0^1 p_3\, ds &= \int_0^1 \left[ f_n + s \nabla f_n + \frac{s(s+1)}{2} \nabla^2 f_n + \frac{s(s+1)(s+2)}{6} \nabla^3 f_n \right] ds \\
&= f_n \int_0^1 ds + \nabla f_n \int_0^1 s\,ds + \frac{\nabla^2 f_n}{2} \int_0^1 (s^2 + s)\,ds + \frac{\nabla^3 f_n}{6} \int_0^1 (s^3 + 3s^2 + 2s)\,ds \\
&= f_n (1) + \nabla f_n \left(\frac{1}{2}\right) + \frac{\nabla^2 f_n}{2} \left(\frac{1}{3} + \frac{1}{2}\right) + \frac{\nabla^3 f_n}{6} \left(\frac{1}{4} + 1 + 1\right) \\
&= f_n + \frac{1}{2} \nabla f_n + \frac{5}{12} \nabla^2 f_n + \frac{3}{8} \nabla^3 f_n
\end{aligned}
$$

展开差分算子（还原为 $f_j$）：
$$
\begin{aligned}
\nabla f_n &= f_n - f_{n-1} \\
\nabla^2 f_n &= f_n - 2f_{n-1} + f_{n-2} \\
\nabla^3 f_n &= f_n - 3f_{n-1} + 3f_{n-2} - f_{n-3}
\end{aligned}
$$

代入并整理：
$$
\begin{aligned}
\int_0^1 p_3\, ds &=
f_n \left(1 + \frac{1}{2} + \frac{5}{12} + \frac{3}{8}\right)
+ f_{n-1} \left(-\frac{1}{2} - \frac{10}{12} - \frac{9}{8}\right) \\
&\quad + f_{n-2} \left(\frac{5}{12} + \frac{9}{8}\right)
+ f_{n-3} \left(-\frac{3}{8}\right) \\
&= \frac{55}{24}f_n - \frac{59}{24}f_{n-1} + \frac{37}{24}f_{n-2} - \frac{9}{24}f_{n-3}
\end{aligned}
$$

#### **3. AB4迭代格式**

$$
\boxed{y_{n+1} = y_n + \frac{h}{24} \left( 55f_n - 59f_{n-1} + 37f_{n-2} - 9f_{n-3} \right)}
$$

#### **4. AB4精度**

- 插值 $p_3(t)$ 匹配 $f$ 至三阶导数 ⇒ 积分余项含 $f^{(4)}$  
- **LTE = $\mathcal{O}(h^5)$** ⇒ **四阶方法**  
- 需 $y_{n-3}, y_{n-2}, y_{n-1}, y_n$ 作为起始值（建议用RK4生成前4点）

---

### **3. Adams-Moulton 4阶方法（AM4，隐式）**

#### **1. AM4插值构造**

包含终点 $t_{n+1}$，用 $t_{n+1}, t_n, t_{n-1}, t_{n-2}$ 构造三次插值 $q_3(t)$。  
牛顿后向差分插值（令 $s = \frac{t - t_{n+1}}{h}$）：
$$
q_3(t) = f_{n+1} + s \nabla f_{n+1} + \frac{s(s+1)}{2} \nabla^2 f_{n+1} + \frac{s(s+1)(s+2)}{6} \nabla^3 f_{n+1}
$$

#### **2. AM4积分计算**

$\int_{t_n}^{t_{n+1}} f(t)\,dt = h \int_{-1}^0 q_3(t_{n+1} + sh)\, ds$（因 $t_n = t_{n+1} - h$ ⇒ $s=-1$）

$$
\begin{aligned}
\int_{-1}^0 q_3\, ds &= \int_{-1}^0 \left[ f_{n+1} + s \nabla f_{n+1} + \frac{s(s+1)}{2} \nabla^2 f_{n+1} + \frac{s(s+1)(s+2)}{6} \nabla^3 f_{n+1} \right] ds \\
&= f_{n+1} (1) + \nabla f_{n+1} \left(-\frac{1}{2}\right) + \frac{\nabla^2 f_{n+1}}{2} \left(-\frac{1}{6}\right) + \frac{\nabla^3 f_{n+1}}{6} \left(\frac{1}{4}\right) \\
&= f_{n+1} - \frac{1}{2} \nabla f_{n+1} - \frac{1}{12} \nabla^2 f_{n+1} + \frac{1}{24} \nabla^3 f_{n+1}
\end{aligned}
$$

展开差分（注意 $\nabla f_{n+1} = f_{n+1} - f_n$, $\nabla^2 f_{n+1} = f_{n+1} - 2f_n + f_{n-1}$, etc.）：
$$
\begin{aligned}
\int_{-1}^0 q_3\, ds &=
f_{n+1} \left(1 - \frac{1}{2} - \frac{1}{12} + \frac{1}{24}\right)
+ f_n \left(\frac{1}{2} + \frac{2}{12} - \frac{3}{24}\right) \\
&\quad + f_{n-1} \left(-\frac{1}{12} + \frac{3}{24}\right)
+ f_{n-2} \left(-\frac{1}{24}\right) \\
&= \frac{9}{24}f_{n+1} + \frac{19}{24}f_n - \frac{5}{24}f_{n-1} + \frac{1}{24}f_{n-2}
\end{aligned}
$$

### **3. AM4迭代格式（隐式）**

$$
\boxed{y_{n+1} = y_n + \frac{h}{24} \left( 9f_{n+1} + 19f_n - 5f_{n-1} + f_{n-2} \right)}
$$
其中 $f_{n+1} = f(t_{n+1}, y_{n+1})$ 依赖于待求的 $y_{n+1}$。

### **4. AM4求解策略**

- **固定点迭代**（简单但可能不收敛）：
  $$
  y_{n+1}^{(k+1)} = y_n + \frac{h}{24} \left( 9f(t_{n+1}, y_{n+1}^{(k)}) + 19f_n - 5f_{n-1} + f_{n-2} \right)
  $$
- **牛顿法**（推荐）：  
  记 $F(y) = y - y_n - \frac{h}{24} \left( 9f(t_{n+1}, y) + C \right)$，求 $F(y)=0$。  
  迭代：$y^{(k+1)} = y^{(k)} - \left[ I - \frac{9h}{24} f_y(t_{n+1}, y^{(k)}) \right]^{-1} F(y^{(k)})$

### **5. AM4精度与优势**

- **LTE = $\mathcal{O}(h^5)$**（比AB4高一阶，因插值含终点）  
- **A-稳定区域更大**：比AB4更稳定（尤其对刚性问题）  
- 常与AB4组成**预测-校正对**（PECE模式）：
  1. **Predict**: 用AB4预测 $\tilde{y}_{n+1}$
  2. **Evaluate**: 计算 $\tilde{f}_{n+1} = f(t_{n+1}, \tilde{y}_{n+1})$
  3. **Correct**: 用AM4校正 $y_{n+1} = y_n + \frac{h}{24}(9\tilde{f}_{n+1} + 19f_n - 5f_{n-1} + f_{n-2})$
  4. **Evaluate**: 更新 $f_{n+1} = f(t_{n+1}, y_{n+1})$（用于下一步）

---

## **方法对比与使用建议**

| 方法       | 类型   | 公式                                                                 | 阶数 | 启动要求         | 推荐用途                     |
|------------|--------|----------------------------------------------------------------------|------|------------------|------------------------------|
| **AB2**    | 显式   | $y_{n+1} = y_n + \frac{h}{2}(3f_n - f_{n-1})$                      | 2    | 2点（$y_0,y_1$） | 快速原型，非刚性问题         |
| **AB4**    | 显式   | $y_{n+1} = y_n + \frac{h}{24}(55f_n - 59f_{n-1} + 37f_{n-2} - 9f_{n-3})$ | 4    | 4点             | 高效高精度（非刚性）         |
| **AM4**    | 隐式   | $y_{n+1} = y_n + \frac{h}{24}(9f_{n+1} + 19f_n - 5f_{n-1} + f_{n-2})$   | 4    | 3点             | 高精度+稳定性（或与AB4配对） |

> **关键警告**：  
>
> - AB系列**绝对不稳定**（A-稳定区域为零），对刚性系统必发散。  
> - 你的双摆问题**非刚性**（特征时间尺度相近），AB4完全适用。  
> - 若做AM4，**必须用PECE模式**（避免昂贵的隐式求解器）。

---

## **在双摆问题中的实施建议**

1. **AB4实现**：

   ```matlab
   % 假设已有 y_prev = [y_n; y_{n-1}; y_{n-2}; y_{n-3}] (4x4矩阵，每列一个状态)
   f_n = f(t(n),   y_prev(:,1));
   f_n1 = f(t(n-1), y_prev(:,2));
   f_n2 = f(t(n-2), y_prev(:,3));
   f_n3 = f(t(n-3), y_prev(:,4));
   
   y_new = y_prev(:,1) + h/24 * (55*f_n - 59*f_n1 + 37*f_n2 - 9*f_n3);
   ```

2. **AB4+AM4预测-校正**：

   ```matlab
   % Predict with AB4
   y_pred = y_n + h/24 * (55*f_n - 59*f_n1 + 37*f_n2 - 9*f_n3);
   f_pred = f(t_{n+1}, y_pred);
   
   % Correct with AM4
   y_{n+1} = y_n + h/24 * (9*f_pred + 19*f_n - 5*f_n1 + f_n2);
   ```

3. **启动方案**：  
   用RK4生成前4个点（$y_0, y_1, y_2, y_3$），存入历史缓存。

推导完毕。这些公式是数值分析的基石，理解其来源可避免“调包式编程”。下一步可直接编码——若需AB4/AM4的MATLAB模板，我可立即提供。

## MATLAB程序实现

一个小报错：

```matlab
Error: The variable 'm2' is perhaps intended as a reduction variable, but is actually an
uninitialized temporary. For more information, see Parallel for Loops in MATLAB, "Temporary
Variables Intended as Reduction Variables".

Error in main (line 35)
analyzeSensitivity(@doublePendulumODE, tspan, params, options);
```

## 不同算法的比较

- Why 数据点都不一样多
  - ODE45为自适应步长求解，等内置求解器是自适应步长，其内部机制不允许精确控制步长；
  - 自实现算法多为固定步长，二者工作原理本质不同；
  - 混沌系统的敏感性使直接比较长期轨迹失去意义
- 如何在数据点不一样多的情况下进行比较（为什么选择这个方案进行比较）
  - 统一时间网格插值法：能精确比较任意时间点的误差
  - 误差指标计算方法：关键指标：L2Error、maxError、能量漂移，将大量数据压缩为几个关键指标，便于算法比较
  - 事件点比较法：关注特定时间点的精度，大幅减少计算量，结果直观
  - 自适应比较策略：针对系统动态特性进行智能比较，非线性系统如双摆，其动态特性在不同区域差异大
  - 最终选择：采用统一时间网格插值法比较L2Error、maxError、能量漂移这3个关键指标的误差
  - 差值方法选择：样条法
- 比较结果:fourLAdamsMoultonMethod最好

## 参数敏感性探究

对于初值敏感的混沌系统比较位置没有太大意义，所以探究一些特定的物理指标：

相空间分离 (Phase Space Separation)
庞加莱截面 (Poincaré Section)
最大李雅普诺夫指数 (MLE) 的参数扫描

- l1/l2 → 比值
- m1,m2 → 比值
- theta1, theta2 单独探究

## 附录

- 绳和杆的讨论
- 混沌系统的物理描述

## 参考文献

常用数值方法及其MATLAB实现
数值计算
Introduction to Numerical Analysis
MATLABl理论力学