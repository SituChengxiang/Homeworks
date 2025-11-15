% lagrange_cubic.m
% 用拉格朗日插值逼近 f(x) = x^3

clear; clc; close all;

% 真值函数
f = @(x) x.^3;

% 定义绘图区间
x_plot = linspace(-1.5, 2.5, 400);
y_true = f(x_plot);

% 创建图形窗口
figure('Position', [100, 100, 800, 600]);
plot(x_plot, y_true, 'k-', 'LineWidth', 2, 'DisplayName', 'f(x) = x^3');
hold on;
grid on;
xlabel('x');
ylabel('y');
title('拉格朗日插值逼近 f(x) = x^3');
colors = lines(5);

%% 1. P1(x): 节点 x0=-1, x1=0 → 线性插值
x1 = [-1, 0];
y1 = f(x1);
% 拉格朗日基函数：L0 = (x - x1)/(x0 - x1), L1 = (x - x0)/(x1 - x0)
% => L0 = (x - 0)/(-1 - 0) = -x,  L1 = (x + 1)/(0 + 1) = x + 1
% P1(x) = y0*L0 + y1*L1 = (-1)*(-x) + 0*(x+1) = x
% —— 果然，P1(x) = x
P1_coeff = polyfit(x1, y1, 1);  % 等价于拉格朗日，更简洁
y_P1 = polyval(P1_coeff, x_plot);
plot(x_plot, y_P1, '--', 'Color', colors(1,:), 'DisplayName', 'P_1(x): nodes [-1,0]');

%% 2. P2(x): 节点 x0=-1, x1=0, x2=1 → 二次插值
x2 = [-1, 0, 1];


y2 = f(x2);
P2_coeff = polyfit(x2, y2, 2);
y_P2 = polyval(P2_coeff, x_plot);
plot(x_plot, y_P2, ':', 'Color', colors(2,:), 'DisplayName', 'P_2(x): nodes [-1,0,1]');

%% 3. P3(x): 节点 x0=-1, x1=0, x2=1, x3=2 → 三次插值（应精确等于 x^3！）
x3 = [-1, 0, 1, 2];
y3 = f(x3);  % [-1, 0, 1, 8]
P3_coeff = polyfit(x3, y3, 3);
y_P3 = polyval(P3_coeff, x_plot);
plot(x_plot, y_P3, '-', 'Color', colors(3,:), 'LineWidth', 1.5, ...
    'DisplayName', 'P_3(x): nodes [-1,0,1,2] (exact!)');

%% 4. P4(x): 节点 x0=1, x1=2 → 线性插值（右侧局部逼近）
x4 = [1, 2];
y4 = f(x4);  % [1, 8]
P4_coeff = polyfit(x4, y4, 1);
y_P4 = polyval(P4_coeff, x_plot);
plot(x_plot, y_P4, '--', 'Color', colors(4,:), 'DisplayName', 'P_4(x): nodes [1,2]');

%% 5. P5(x): 节点 x0=0, x1=1, x2=2 → 二次插值（右侧）
x5 = [0, 1, 2];
y5 = f(x5);  % [0, 1, 8]
P5_coeff = polyfit(x5, y5, 2);
y_P5 = polyval(P5_coeff, x_plot);
plot(x_plot, y_P5, ':', 'Color', colors(5,:), 'DisplayName', 'P_5(x): nodes [0,1,2]');

% 标出所有节点位置
all_nodes = [x1, x2, x3, x4, x5];
plot(all_nodes, f(all_nodes), 'ko', 'MarkerFaceColor', 'k', 'MarkerSize', 5);

% 图例
legend('Location', 'northwest');

% 输出多项式表达式（符号形式）
syms x;
fprintf('=== 插值多项式表达式 ===\n');
fprintf('P1(x) = %s\n', char(vpa(poly2sym(P1_coeff, x), 4)));
fprintf('P2(x) = %s\n', char(vpa(poly2sym(P2_coeff, x), 4)));
fprintf('P3(x) = %s  (等于 x^3)\n', char(vpa(poly2sym(P3_coeff, x), 4)));
fprintf('P4(x) = %s\n', char(vpa(poly2sym(P4_coeff, x), 4)));
fprintf('P5(x) = %s\n', char(vpa(poly2sym(P5_coeff, x), 4)));

% 验证 P3(x) 是否等于 x^3
if isequal(round(P3_coeff, 10), round([1, 0, 0, 0], 10))
    fprintf('\n✅ P3(x) 精确等于 x^3！（因 f 是三次多项式，4点唯一确定）\n');
end