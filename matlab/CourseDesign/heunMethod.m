function [t, z] = heunMethod(odeFunc, tspan, z0, options)
% Heun 方法（改进欧拉）处理一阶常微分方程组。

if nargin >= 4 && isstruct(options) && isfield(options, 'MaxStep') && ~isempty(options.MaxStep)
    h = options.MaxStep;
    stepCount = ceil((tspan(2) - tspan(1)) / h);
else
    stepCount = 60000;
    h = (tspan(2) - tspan(1)) / stepCount;
end

t = linspace(tspan(1), tspan(2), stepCount + 1);
numStates = numel(z0);
z = zeros(numStates, numel(t));
z(:, 1) = z0(:);

for k = 1:stepCount
    dt = h;
    k1 = odeFunc(t(k), z(:, k));
    predictor = z(:, k) + dt * k1;
    k2 = odeFunc(t(k + 1), predictor);
    z(:, k + 1) = z(:, k) + dt * 0.5 * (k1 + k2);
end

t = t(:);
z = z.';
end
