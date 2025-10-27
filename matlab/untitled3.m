function [p1,p2,p3] = untitled3()
% untitled3 - 使用牛顿-拉夫森法迭代3次
%   f(x) = x^2 - x + 2
%   初始值 p0 = -1.5
%   返回 p1, p2, p3 并在命令行打印结果

    % 函数和导数
    f = @(x) x.^2 - x + 2;
    df = @(x) 2*x - 1;

    % 初始点
    p0 = -1.5;

    % 第1次迭代
    if df(p0) == 0
        error('导数在 p0 处为零，无法迭代。');
    end
    p1 = p0 - f(p0)/df(p0);

    % 第2次迭代
    if df(p1) == 0
        error('导数在 p1 处为零，无法迭代。');
    end
    p2 = p1 - f(p1)/df(p1);

    % 第3次迭代
    if df(p2) == 0
        error('导数在 p2 处为零，无法迭代。');
    end
    p3 = p2 - f(p2)/df(p2);

    % 打印结果
    fprintf('p0 = %.12f\n', p0);
    fprintf('p1 = %.12f\n', p1);
    fprintf('p2 = %.12f\n', p2);
    fprintf('p3 = %.12f\n', p3);
end