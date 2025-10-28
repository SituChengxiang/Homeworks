function [root] = sqrt_newton(x)

    if x == 0
        root = 0;
        printIteration(0, root);
        return;
    end

    % 默认配置
    x0 = x;            % 初始猜测值：设为 x 本身
    tol = 5e-11;       % 收敛容差：确保小数点后10位准确
    max_iter = 500;     % 最大迭代次数

    % prealloc 迭代记录
    iter_history = complex(zeros(max_iter + 1, 1));
    iter_history(1) = x0;

    % ========== 牛顿迭代主循环 ==========
    % 求解 f(t) = t^2 - x = 0 => t_{n+1} = (t_n + x / t_n) / 2
    xn = x0;
    printIteration(0, xn);
    for k = 1:max_iter
        xn_new = (xn + x / xn) / 2;
        iter_history(k + 1) = xn_new;
        printIteration(k, xn_new);

        % 收敛条件判定
        if abs(xn_new - xn) < tol
            root = xn_new;
            return;
        end
        xn = xn_new;
    end

    % 若达到最大迭代次数仍未收敛，返回当前值并警告
    warning('sqrt_newton:MaxIter', ...
        'The maximum number of iterations (%d) has been reached without convergence.', max_iter);
    root = xn;

end

function printIteration(k, value)
    if isreal(value)
        fprintf('Iteration %d: %.10f\n', k, real(value));
    else
        fprintf('Iteration %d: %.10f%+.10fi\n', k, real(value), imag(value));
    end
end