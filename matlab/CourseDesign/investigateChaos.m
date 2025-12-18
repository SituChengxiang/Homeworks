function investigateChaos()
    clc; close all;
    
    % 1. 基础参数设置
    % 载入默认参数 (仅为了获取 g, l, m 等物理常数，角度后面会覆盖)
    run('initalCondition.m'); 
    params = struct('l1', l1, 'l2', l2, 'm1', m1, 'm2', m2, 'g', g, 'theta10', theta10, 'theta20', theta20);
    
    % 求解器设置 (高精度)
    % 混沌对误差极其敏感，需要较小的步长和较高的容差
    options = odeset('RelTol', 1e-10, 'AbsTol', 1e-12, 'MaxStep', 0.001); 
    
    %% 任务 1: 相空间分离 (Phase Space Separation) - 蝴蝶效应可视化
    fprintf('Running Task 1: Phase Space Separation...\n');
    tspan = [0, 60]; % 运行时间
    getPhaseSpaceSeparation(tspan, params, options);
    

    %% 任务 2: 庞加莱截面 (Poincaré Section)
    fprintf('Running Task 2: Poincaré Section...\n');
    tspan_poincare = [0, 600];
    getPoincareSection(tspan_poincare, params, options);

    %% 任务 3: 最大李雅普诺夫指数 (MLE) 参数扫描
    fprintf('Running Task 3: MLE Parameter Scan (Heatmap)...\n');
    getMLEHeatmap(params, options);

end

%% 相空间分离图
function getPhaseSpaceSeparation(tspan, params, options)
    % 准备两个初始条件：Base 和 Perturbed
    z0Base = [deg2rad(params.theta10); deg2rad(params.theta20); 0; 0];
    
    delta = 1e-3; % 微扰大小
    z0Pert = z0Base;
    z0Pert(1) = z0Pert(1) + delta; % 仅在 theta1 上施加微扰
    
    % 封装 ODE 函数
    odeFuncWrapped = @(t, z) doublePendulumODE(t, z, params);
    
    % 运行仿真
    fprintf('  Simulating Base Trajectory...\n');
    [tBase, zBase] = fourLAdamsMoultonMethod(odeFuncWrapped, tspan, z0Base, options);
    
    fprintf('  Simulating Perturbed Trajectory...\n');
    [tPert, zPert] = fourLAdamsMoultonMethod(odeFuncWrapped, tspan, z0Pert, options);
    
    % 由于定步长求解器返回的时间点可能完全一致（如果是固定步长实现），
    % 但为了鲁棒性，我们这里假设时间点是对齐的。
    % 如果求解器是变步长的，这里需要插值对齐。
    % 检查长度，取最小长度进行截断对比
    len = min(length(tBase), length(tPert));
    t = tBase(1:len);
    zBase = zBase(1:len, :);
    zPert = zPert(1:len, :);
    
    % 计算相空间距离 (欧几里得距离)
    % distance(t) = || zBase(t) - zPert(t) ||
    diff = zBase - zPert;
    dist = sqrt(sum(diff.^2, 2));
    
    % 绘图
    figure('Name', 'Phase Space Separation', 'Color', 'w', 'Position', [100, 100, 1000, 600]);
    
    % 子图 1: 距离随时间的演化 (对数坐标)
    subplot(2, 1, 1);
    semilogy(t, dist, 'Color','#0076a8', 'LineWidth', 1.2);
    grid on;
    xlabel('Time (s)');
    ylabel('Separation Distance ||\delta z||');
    title(sprintf('Phase Space Separation (Log Scale)\nInitial: \\theta_1=%d^\\circ, \\theta_2=%d^\\circ, \\delta=10^{-3}', params.theta10, params.theta20));
    
    % 添加参考线：如果斜率为正，说明指数分离
    
    % 子图 2: 两个轨迹的 theta1 对比，直观展示分离时刻
    subplot(2, 1, 2);
    plot(t, rad2deg(zBase(:,1)), 'Color','#0070bb', 'LineWidth', 1.0, 'DisplayName', 'Base');
    hold on;
    plot(t, rad2deg(zPert(:,1)), 'Color','#cf2c1d', 'LineWidth', 1.0, 'DisplayName', 'Perturbed');
    grid on;
    xlabel('Time (s)');
    ylabel('\theta_1 (deg)');
    legend('Location', 'best');
    title('Trajectory Divergence in Time Domain');
    fprintf('  Task 1 Completed.\n');
end

%% 庞加莱截面图
function getPoincareSection(tspan_poincare, params, options)
    % 使用与上面相同的初始条件
    z0 = [deg2rad(params.theta10); deg2rad(params.theta20); 0; 0];
    
    fprintf('  Simulating for Poincaré Section (Duration: %ds)...\n', tspan_poincare(2));
    
    % 封装 ODE 函数
    odeFuncWrapped = @(t, z) doublePendulumODE(t, z, params);
    
    [t, z] = fourLAdamsMoultonMethod(odeFuncWrapped, tspan_poincare, z0, options);
    
    % 寻找截面穿越点: theta2 = 0 (mod 2pi) 且 theta2_dot > 0；即，监测 sin(theta2) 从负变正的时刻 (对应 theta2 = 2k*pi)
    
    poincare_points = [];
    
    theta2 = z(:, 2);
    % theta2_dot = z(:, 4);
    
    monitor_var = sin(theta2); 
    
    for i = 1:length(t)-1
        % 检测 monitor_var 从负变正 (Zero Crossing with Positive Slope)
        if monitor_var(i) < 0 && monitor_var(i+1) >= 0
            % 线性插值寻找精确时刻
            dt = t(i+1) - t(i);
            dy = monitor_var(i+1) - monitor_var(i);
            frac = (0 - monitor_var(i)) / dy;
            
            % 插值状态
            z_cross = z(i, :) + frac * (z(i+1, :) - z(i, :));
            
            % 检查 theta2_dot > 0 (确保是同向穿越)
            if z_cross(4) > 0
                % 保存 (theta1, theta1_dot)
                % 将 theta1 归一化到 [-pi, pi]
                th1 = mod(z_cross(1) + pi, 2*pi) - pi;
                th1_dot = z_cross(3);
                poincare_points = [poincare_points; th1, th1_dot];
            end
        end
    end
    
    % 绘图
    figure('Name', 'Poincaré Section', 'Color', 'w');
    plot(poincare_points(:,1), poincare_points(:,2), '.', 'Color', '#0076a8', 'MarkerSize', 8);
    xlabel('\theta_1 (rad)');
    ylabel('$\dot{\theta}_1$ (rad/s)', 'Interpreter', 'latex');
    title(sprintf('Poincaré Section (\\theta_2=0, \\dot{\theta}_2>0)\nTime=0-%ds', tspan_poincare(2)));
    grid on;
    xlim([-pi, pi]);
    
    fprintf('  Task 2 Completed. Found %d points.\n', size(poincare_points, 1));
end

%% MLE 热力图
function getMLEHeatmap(params, options)
    % 扫描范围
    theta1_range = linspace(-180, 180, 50); % 50x50 网格
    theta2_range = linspace(-180, 180, 50);
    
    n1 = length(theta1_range);
    n2 = length(theta2_range);
    
    mle_matrix = zeros(n2, n1); % 注意行列对应: 行是 y (theta2), 列是 x (theta1)
    
    % 仿真时间 (不需要太长，FTLE 是有限时间估计)
    tspan_mle = [0, 20];
    delta = 1e-3;
    
    fprintf('  Starting Grid Scan (%dx%d). This may take a while...\n', n1, n2);
    
    % 并行计算
    parfor i = 1:n1
        % 为了减少通信开销，按列计算
        col_mle = zeros(n2, 1);
        t1_val = theta1_range(i);
        
        for j = 1:n2
            t2_val = theta2_range(j);
            
            % 构造两个初始条件
            z0_base = [deg2rad(t1_val); deg2rad(t2_val); 0; 0];
            z0_pert = z0_base;
            z0_pert(1) = z0_pert(1) + delta;
            
            % 封装 ODE
            % 注意：parfor 中不能直接用外部的 odeFuncWrapped，最好在循环内构造或传递
            % 这里我们直接调用 doublePendulumODE
            
            % 运行两次仿真
            [~, z_base] = fourLAdamsMoultonMethod(@(t,z) doublePendulumODE(t,z,params), tspan_mle, z0_base, options);
            [~, z_pert] = fourLAdamsMoultonMethod(@(t,z) doublePendulumODE(t,z,params), tspan_mle, z0_pert, options);
            
            % 计算末态距离
            z_end_base = z_base(end, :);
            z_end_pert = z_pert(end, :);
            
            dist_final = norm(z_end_base - z_end_pert);
            dist_initial = delta;
            
            % 计算 FTLE: lambda = (1/T) * ln(d(T)/d(0))
            T = tspan_mle(2);
            lambda = (1/T) * log(dist_final / dist_initial);
            
            % 过滤掉可能的负值或异常值 (仅保留正值用于展示混沌)
            if lambda < 0, lambda = 0; end
            
            col_mle(j) = lambda;
        end
        mle_matrix(:, i) = col_mle;
        fprintf('.'); % 进度条
        if mod(i, 10) == 0, fprintf('\n'); end
    end
    fprintf('\n  Scan Completed.\n');
    
    % 绘图
    figure('Name', 'MLE Parameter Scan', 'Color', 'w');
    imagesc(theta1_range, theta2_range, mle_matrix);
    set(gca, 'YDir', 'normal'); % 确保 Y 轴方向正确
    colorbar;
    colormap('jet');
    xlabel('\theta_{10} (deg)');
    ylabel('\theta_{20} (deg)');
    title('Finite Time Lyapunov Exponent (FTLE) Heatmap');
    axis equal;
    xlim([-180, 180]);
    ylim([-180, 180]);
    
    fprintf('  Task 3 Completed.\n');
end