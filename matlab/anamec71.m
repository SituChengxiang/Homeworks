% 参数设置
k = 1; m = 1; L = 1; % 弹簧常数、质量、角动量
r = linspace(0.1, 3, 1000); % 避免 r=0 奇点

%% (a) 有效势能可视化
V_eff = 0.5*k*r.^2 + L^2./(2*m*r.^2);
figure;
plot(r, V_eff, 'b-', 'LineWidth', 2);
hold on;
V_min = sqrt(k*L^2/(2*m));
r_min = (L^2/(m*k))^(1/4);
plot(r_min, V_min, 'ro', 'MarkerSize', 8, 'LineWidth', 2);
xlabel('$r$', 'Interpreter', 'latex');
ylabel('$V_{\text{eff}}(r)$', 'Interpreter', 'latex');
title('Effective Potential for Spring Potential');
legend AutoUpdate
grid on; axis tight;

%% (b) 椭圆轨道可视化
A = 1.5; B = 0.8; % 椭圆半轴
theta = linspace(0, 2*pi, 1000);
x = A * cos(theta);
y = B * sin(theta);
figure;
plot(x, y, 'm-', 'LineWidth', 2);
axis equal; grid on;
xlabel('$x$', 'Interpreter', 'latex');
ylabel('$y$', 'Interpreter', 'latex');
title('Elliptical Orbit in Spring Potential');