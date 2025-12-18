function [t, z] = eularMethod(odeFunc, tspan, z0, options)
% eularMethod 显式欧拉法

h = options.MaxStep;
stepCount = ceil((tspan(2) - tspan(1)) / h);
t = linspace(tspan(1), tspan(2), stepCount + 1);

numStates = numel(z0);
z = zeros(numStates, numel(t));
z(:, 1) = z0(:);

for k = 1:stepCount
	z(:, k + 1) = z(:, k) + h * odeFunc(t(k), z(:, k));
end

% 转置以匹配 ode45 的输出格式 (N x 1 和 N x numStates)，省得画图报错
t = t(:);
z = z.';

end