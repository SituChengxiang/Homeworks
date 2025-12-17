function [t, z] = fourLRK(odeFunc, tspan, z0, options)
% 四阶经典 Runge-Kutta 方法（RK4），参数：常微分方程、时段、初值、选项

if nargin >= 4 && isstruct(options) && isfield(options, 'MaxStep') && ~isempty(options.MaxStep)
    h = options.MaxStep;
    stepCount = ceil((tspan(2) - tspan(1)) / h);
else
    stepCount = 120000; % 一个比较多的默认分段数
    h = (tspan(2) - tspan(1)) / stepCount;
end

t = linspace(tspan(1), tspan(2), stepCount + 1);
numStates = numel(z0);
z = zeros(numStates, numel(t));
z(:, 1) = z0(:);

for k = 1:stepCount
    dt = h;
    k1 = odeFunc(t(k), z(:, k));
    k2 = odeFunc(t(k) + 0.5 * dt, z(:, k) + 0.5 * dt * k1);
    k3 = odeFunc(t(k) + 0.5 * dt, z(:, k) + 0.5 * dt * k2);
    k4 = odeFunc(t(k + 1), z(:, k) + dt * k3);
    z(:, k + 1) = z(:, k) + dt * (k1 + 2 * k2 + 2 * k3 + k4) / 6;
end

t = t(:);
z = z.';
end
