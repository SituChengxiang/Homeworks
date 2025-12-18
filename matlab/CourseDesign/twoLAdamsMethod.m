function [t, z] = twoLAdamsMethod(odeFunc, tspan, z0, options)
% 二阶 Adams-Bashforth 显式多步法

if isfield(options, 'MaxStep'), h = options.MaxStep; else, h = (tspan(2)-tspan(1))/120000; end
stepCount = ceil((tspan(2) - tspan(1)) / h);

t = linspace(tspan(1), tspan(2), stepCount + 1);
numStates = numel(z0);
z = zeros(numStates, stepCount + 1);
z(:, 1) = z0(:);

fVals = zeros(numStates, stepCount + 1);
fVals(:, 1) = odeFunc(t(1), z(:, 1));

% 使用单步 RK4 引导首个步长。
z(:, 2) = rk4Step(t(1), z(:, 1), h, odeFunc);
fVals(:, 2) = odeFunc(t(2), z(:, 2));

for n = 2:stepCount
    z(:, n + 1) = z(:, n) + (h / 2) * (3 * fVals(:, n) - fVals(:, n - 1));
    fVals(:, n + 1) = odeFunc(t(n + 1), z(:, n + 1));
end

t = t(:);
z = z.';
end

function zNext = rk4Step(t0, z0, h, odeFunc)
k1 = odeFunc(t0, z0);
k2 = odeFunc(t0 + 0.5 * h, z0 + 0.5 * h * k1);
k3 = odeFunc(t0 + 0.5 * h, z0 + 0.5 * h * k2);
k4 = odeFunc(t0 + h, z0 + h * k3);
zNext = z0 + (h / 6) * (k1 + 2 * k2 + 2 * k3 + k4);
end
