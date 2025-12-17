% main.m 使用 ode45 求解复合双摆 0-60s 运动并可视化作为参考值。
clear; clc;

% 载入初始条件（角度以度为单位，主要是几何参数）
run('initalCondition.m');
params = struct('l1', l1, 'l2', l2, 'm1', m1, 'm2', m2, 'g', g);
radTheta1 = deg2rad(theta10);
radTheta2 = deg2rad(theta20);
z0 = [radTheta1; radTheta2; 0; 0];

% 积分设置：时间区域、各求解器自己还需要的参数
tspan = [0, 60];
options = odeset('RelTol', 1e-10, 'AbsTol', 1e-12, 'MaxStep', 0.0005);

% 调用 ode113 获得高精度参考轨迹。
[t, z] = ode113(@(tt, zz) doublePendulumODE(tt, zz, params), tspan, z0, options);

% 使用自编数值方法求解以对比结果。
% [t, z] = eularMethod(@(tt, zz) doublePendulumODE(tt, zz, params), tspan, z0, options);
% [t, z] = heunMethod(@(tt, zz) doublePendulumODE(tt, zz, params), tspan, z0, options);
% [t, z] = threeLRK(@(tt, zz) doublePendulumODE(tt, zz, params), tspan, z0, options);
% [t, z] = fourLRK(@(tt, zz) doublePendulumODE(tt, zz, params), tspan, z0, options);
% [t, z] = twoLAdamsMethod(@(tt, zz) doublePendulumODE(tt, zz, params), tspan, z0, options);
% [t, z] = fourLAdamsMethod(@(tt, zz) doublePendulumODE(tt, zz, params), tspan, z0, options);
% [t, z] = fourLHiddenAdamsMethod(@(tt, zz) doublePendulumODE(tt, zz, params), tspan, z0, options);

% 最核心部分的代码：复合双摆的 ODE 定义。
function dzdt = doublePendulumODE(~, z, params)
% 复合双摆一阶 ODE 表达。

l1 = params.l1; l2 = params.l2;
m1 = params.m1; m2 = params.m2; g = params.g;

theta1 = z(1); theta2 = z(2);
omega1 = z(3); omega2 = z(4);

massMatrix = [ (m1 + m2) * l1, m2 * l2 * cos(theta1 - theta2);
               l1 * cos(theta1 - theta2), l2 ];
forcing = [ -m2 * l2 * sin(theta1 - theta2) * omega2^2 - (m1 + m2) * g * sin(theta1);
            l1 * sin(theta1 - theta2) * omega1^2 - g * sin(theta2) ];

alpha = massMatrix \ forcing;

dzdt = [omega1; omega2; alpha(1); alpha(2)];
end

% 计算两端点在平面中的轨迹坐标。
x1 = l1 * sin(z(:, 1));
y1 = -l1 * cos(z(:, 1));
x2 = x1 + l2 * sin(z(:, 2));
y2 = y1 - l2 * cos(z(:, 2));

% 可视化角度响应（转为度便于解读）。
% figure('Name', 'Double Pendulum Trajectory', 'Color', 'w');
% plot(t, rad2deg(z(:, 1)), 'LineWidth', 1.5, 'DisplayName', '\theta_1');
% hold on;
% plot(t, rad2deg(z(:, 2)), 'LineWidth', 1.5, 'DisplayName', '\theta_2');
% hold off;
% legend('Location', 'best');
% xlabel('时间 / s');
% ylabel('角度 / °');
% title('复合双摆在 60 s 内的角度响应');
% grid on;

% 相图展示。
% figure('Name', 'Phase Portraits', 'Color', 'w');
% subplot(1, 2, 1);
% plot(rad2deg(z(:, 1)), rad2deg(z(:, 3)), 'LineWidth', 1.2);
% xlabel('\theta_1 / °');
% ylabel('\omega_1 / (°/s)');
% title('摆杆 1 相图');
% grid on;

% subplot(1, 2, 2);
% plot(rad2deg(z(:, 2)), rad2deg(z(:, 4)), 'LineWidth', 1.2);
% xlabel('\theta_2 / °');
% ylabel('\omega_2 / (°/s)');
% title('摆杆 2 相图');
% grid on;

% 平面轨迹图。
% figure('Name', 'Planar Motion', 'Color', 'w');
% plot(0, 0, 'ko', 'MarkerFaceColor', 'k', 'DisplayName', '铰点');
% hold on;
% plot(x1, y1, 'LineWidth', 1.2, 'DisplayName', '小球 1 轨迹');
% plot(x2, y2, 'LineWidth', 1.2, 'DisplayName', '小球 2 轨迹');
% scatter(x1(1), y1(1), 40, 'filled', 'MarkerFaceColor', [0 0.45 0.74], 'DisplayName', '起点 1');
% scatter(x2(1), y2(1), 40, 'filled', 'MarkerFaceColor', [0.85 0.33 0.1], 'DisplayName', '起点 2');
% axis equal;
% xlabel('x / m');
% ylabel('y / m');
% title('0-60 s 内端点轨迹');
% legend('Location', 'bestoutside');
% grid on;
% hold off;

% 动画演示：展示 0-60 s 内摆杆与小球的实时运动
% 这部分的代码由QwenCoder编写，实在是没空仔细查代码了
figure('Name', 'Pendulum Animation', 'Color', 'w');
axis equal;
hold on;
plot(0, 0, 'ko', 'MarkerFaceColor', 'k');
axisPadding = 1.1 * max([l1 + l2; abs(x1(:)); abs(y1(:))]);
axis([-axisPadding, axisPadding, -axisPadding, axisPadding]);
xlabel('x / m');
ylabel('y / m');
title('复合双摆 0-60 s 动画');
grid on;

rod1 = plot([0, x1(1)], [0, y1(1)], 'LineWidth', 2, 'Color', [0 0.45 0.74]);
rod2 = plot([x1(1), x2(1)], [y1(1), y2(1)], 'LineWidth', 2, 'Color', [0.85 0.33 0.1]);
mass1 = plot(x1(1), y1(1), 'o', 'MarkerSize', 10, 'MarkerFaceColor', [0 0.45 0.74], 'MarkerEdgeColor', 'none');
mass2 = plot(x2(1), y2(1), 'o', 'MarkerSize', 10, 'MarkerFaceColor', [0.85 0.33 0.1], 'MarkerEdgeColor', 'none');

frameSkip = max(1, round(numel(t) / 1800)); % 控制动画帧数，避免刷新过慢
for idx = 1:frameSkip:numel(t)
    set(rod1, 'XData', [0, x1(idx)], 'YData', [0, y1(idx)]);
    set(rod2, 'XData', [x1(idx), x2(idx)], 'YData', [y1(idx), y2(idx)]);
    set(mass1, 'XData', x1(idx), 'YData', y1(idx));
    set(mass2, 'XData', x2(idx), 'YData', y2(idx));
    drawnow;
end
