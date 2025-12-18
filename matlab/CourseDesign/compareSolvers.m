function compareSolvers(odeFunc, tspan, z0, params, solvers, options)
% compareSolvers 比较不同数值求解器的精度和能量守恒特性
% 参数:常微分方程函数句柄、时间区间、初始状态、各算法需要的东西、算法

% 1. 计算高精度参考解 (ode45)
fprintf('Generating High Accuracy Reference (ode45)...\n');
[tRef, zRef] = ode45(@(t, z) odeFunc(t, z, params), tspan, z0, options);

% 2. 建立统一时间网格用于插值对比
numInterpPoints = 10000;
tUni = linspace(tspan(1), tspan(2), numInterpPoints)';
zRefUni = interp1(tRef, zRef, tUni, 'spline');
ERefUni = calEnergy(zRefUni, params);
E0 = ERefUni(1);

% 初始化绘图
figErr = figure('Name', 'Solver Error Comparison', 'Color', 'w', 'Position', [100, 100, 1200, 500]);
subplot(1, 2, 1); hold on; title('ln(位置误差) 关于时间变化图'); xlabel('Time (s)'); ylabel('Error Norm'); set(gca, 'YScale', 'log'); grid on;
subplot(1, 2, 2); hold on; title('ln(能量漂移) 关于时间变化图'); xlabel('Time (s)'); ylabel('Energy Drift (J)'); set(gca, 'YScale', 'log'); grid on;

colors = lines(size(solvers, 1));
stats = cell(size(solvers, 1), 5);

% 3. 循环测试每个求解器，步长设置和main程序一致
for i = 1:size(solvers, 1)
    solverFunc = solvers{i, 1};
    solverName = solvers{i, 2};
    
    fprintf('Testing: %s ...\n', solverName);
    
    % 运行求解器
    tic;
    [tSol, zSol] = solverFunc(@(t, z) odeFunc(t, z, params), tspan, z0, options);
     elapsedTime = toc;
        
    % 插值到统一网格，使用样条插值
    zSolUni = interp1(tSol, zSol, tUni, 'spline');

    % 1. 状态误差 (L2 Norm)
    diff_state = zSolUni - zRefUni;
    err_l2Trace = vecnorm(diff_state, 2, 2);
    max_err = max(err_l2Trace);
    mean_err = mean(err_l2Trace);
    
    % 2. 能量漂移
    ESolUni = calEnergy(zSolUni, params);
    driftTrace = abs(ESolUni - E0);
    maxDrift = max(driftTrace);
    
    % 记录统计数据
    stats{i, 1} = solverName;
    stats{i, 2} = max_err;
    stats{i, 3} = mean_err;
    stats{i, 4} = maxDrift;
    stats{i, 5} = elapsedTime;
    
    figure(figErr);
    subplot(1, 2, 1);
    semilogy(tUni, err_l2Trace, 'Color', colors(i,:), 'LineWidth', 1.5, 'DisplayName', solverName);
    
    subplot(1, 2, 2);
    semilogy(tUni, driftTrace, 'Color', colors(i,:), 'LineWidth', 1.5, 'DisplayName', solverName);
end

figure(figErr);
subplot(1, 2, 1); legend('Location', 'best');
subplot(1, 2, 2); legend('Location', 'best');

fprintf('\n%-15s | %-12s | %-12s | %-12s | %-10s\n', 'Solver', 'Max Error', 'Mean Error', 'Max E-Drift', 'Time(s)');
fprintf('%s\n', repmat('-', 1, 75));
for i = 1:size(stats, 1)
    fprintf('%-15s | %-12.4e | %-12.4e | %-12.4e | %-10.4f\n', ...
        stats{i,1}, stats{i,2}, stats{i,3}, stats{i,4}, stats{i,5});
end

end
