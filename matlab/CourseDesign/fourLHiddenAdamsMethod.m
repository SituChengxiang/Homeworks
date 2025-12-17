function [t, z] = fourLHiddenAdamsMethod(odeFunc, tspan, z0, options)
% 四阶隐式 Adams-Moulton 方法（Newton 迭代求解）。

if nargin >= 4 && isstruct(options) && isfield(options, 'MaxStep') && ~isempty(options.MaxStep)
    h = options.MaxStep;
    stepCount = ceil((tspan(2) - tspan(1)) / h);
else
    stepCount = 120000; % 一个比较多的默认分段数
    h = (tspan(2) - tspan(1)) / stepCount;
end

t = linspace(tspan(1), tspan(2), stepCount + 1);
numStates = numel(z0);
z = zeros(numStates, stepCount + 1);
z(:, 1) = z0(:);

fVals = zeros(numStates, stepCount + 1);
fVals(:, 1) = odeFunc(t(1), z(:, 1));

% 先用 RK4 产生足够的历史信息供多步法使用。
for bootstrap = 1:3
    z(:, bootstrap + 1) = rk4Step(t(bootstrap), z(:, bootstrap), h, odeFunc);
    fVals(:, bootstrap + 1) = odeFunc(t(bootstrap + 1), z(:, bootstrap + 1));
end

tol = 1e-9;
maxIter = 8;

for n = 4:stepCount
    tNew = t(n + 1);
    predictor = z(:, n) + (h / 24) * (55 * fVals(:, n) - 59 * fVals(:, n - 1) + 37 * fVals(:, n - 2) - 9 * fVals(:, n - 3));
    zGuess = predictor;
    converged = false;

    for iter = 1:maxIter
        fGuess = odeFunc(tNew, zGuess);
        residual = zGuess - z(:, n) - (h / 720) * (251 * fGuess + 646 * fVals(:, n) - 264 * fVals(:, n - 1) + 106 * fVals(:, n - 2) - 19 * fVals(:, n - 3));

        if norm(residual, inf) < tol
            converged = true;
            break;
        end

        jac = eye(numStates) - (h / 720) * 251 * numericalJacobian(tNew, zGuess, odeFunc, fGuess);
        delta = jac \ residual;
        zGuess = zGuess - delta;

        if norm(delta, inf) < tol
            converged = true;
            break;
        end
    end

    if ~converged
        % warning('Newton 迭代未在限定次数内收敛');
    end

    z(:, n + 1) = zGuess;
    fVals(:, n + 1) = odeFunc(tNew, z(:, n + 1));
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

function J = numericalJacobian(tPoint, zPoint, odeFunc, fAtPoint)
if nargin < 4
    fAtPoint = odeFunc(tPoint, zPoint);
end

numStates = numel(zPoint);
J = zeros(numStates);
perturbBase = sqrt(eps);

for j = 1:numStates
    step = perturbBase * max(1, abs(zPoint(j)));
    zShifted = zPoint;
    zShifted(j) = zShifted(j) + step;
    fShifted = odeFunc(tPoint, zShifted);
    J(:, j) = (fShifted - fAtPoint) / step;
end

end
