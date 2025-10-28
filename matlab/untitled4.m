%=============================================================
%   LU分解示例：矩阵 A = [-5 2 -1; 1 0 3; 3 1 6]
%   包含：
%       1) MATLAB 自带的 lu（带列主元置换）
%       2) 手动实现的 Doolittle（不带主元置换）
%   运行方法：直接在 MATLAB 中保存为 .m 文件后运行
%=============================================================
clear; clc;

% ---------- 1. 原始矩阵 ----------
A = [1, 0, 3;
     3, 1, 6;
     -5,2,-1];

fprintf('原始矩阵 A =\n');
disp(A);

% ---------- 2. MATLAB 自带的 lu（带列主元） ----------
%  lu 会返回一个置换矩阵 P，使得 P*A = L*U
fprintf('--- 使用 MATLAB 自带 lu（带列主元置换） ---\n');
[P, L1, U1] = lu(A);

fprintf('置换矩阵 P =\n');
disp(P);
fprintf('下三角矩阵 L (单位对角线) =\n');
disp(L1);
fprintf('上三角矩阵 U =\n');
disp(U1);

% 验证 P*A = L*U
res1 = P*A - L1*U1;
fprintf('验证误差 norm(P*A - L*U) = %e\n', norm(res1));

% ---------- 3. 手动 Doolittle 分解（不带主元） ----------
%  这里实现的是经典的 Doolittle 算法：L 的对角线全为 1
fprintf('\n--- 手动实现 Doolittle（不带主元） ---\n');
n = size(A,1);
L = eye(n);
U = zeros(n);

for k = 1:n
    % 计算第 k 行的 U
    for j = k:n
        U(k,j) = A(k,j) - L(k,1:k-1)*U(1:k-1,j);
    end
    
    % 计算第 k 列的 L（对角线保持为 1）
    for i = k+1:n
        L(i,k) = (A(i,k) - L(i,1:k-1)*U(1:k-1,k)) / U(k,k);
    end
end

fprintf('下三角矩阵 L =\n');
disp(L);
fprintf('上三角矩阵 U =\n');
disp(U);

% 验证误差
res2 = A - L*U;
fprintf('验证误差 norm(A - L*U) = %e\n', norm(res2));

% ---------- 4. 小结 ----------
fprintf('\n小结：\n');
fprintf('1) 使用 lu 时，得到的 P、L、U 满足 P*A = L*U（带列主元）。\n');
fprintf('2) 手动 Doolittle 的结果满足 A = L*U（前提是 A 本身无零主元，否则会出现除 0 错误）。\n');
fprintf('   对本例来说，A(1,1)=-5≠0，整个分解过程是安全的。\n');
