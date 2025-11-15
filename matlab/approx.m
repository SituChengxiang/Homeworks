function [C, D] = approx(X, Y)
%NEWPOLY 使用牛顿插值法构造差商表并返回系数
%   C = diagonal entries of the divided-difference table (Newton coefficients)
%   D = full divided-difference table

    X = X(:); % Ensure column vectors
    Y = Y(:);

    n = length(X);
    if length(Y) ~= n
        error('newpoly:InputSizeMismatch', 'X and Y must have the same length.');
    end

    D = zeros(n, n);
    D(:, 1) = Y; % 第一列为函数值

    % 差商计算（按列递推）
    for j = 2:n % 列索引（阶数）
        for k = j:n % 行索引，对应第 k 个节点
            D(k, j) = (D(k, j-1) - D(k-1, j-1)) / (X(k) - X(k-j+1));
        end
    end

    % 牛顿插值系数位于对角线
    C = diag(D);
end