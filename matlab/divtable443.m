% Newton interpolation with GIVEN function values (5-digit approx)
clear; clc;

% Given data (use provided approximations, NOT sqrt)
x = [4.0, 5.0, 6.0, 7.0, 8.0];
y = [2.00000, 2.23607, 2.44949, 2.64575, 2.82843];  % <-- YOUR values

n = length(x);  % =5

% ---------- 1. Compute divided difference table ----------
% D(i, j) = f[x_{i-1}, x_i, ..., x_{i+j-2}]  (MATLAB 1-indexed)
% We store in lower-triangular fashion: j-th column = (j-1)-th order diff
D = zeros(n, n);
D(:,1) = y.';  % 0th order

for j = 2:n          % j = column = order+1
    for i = 1:n-j+1  % i = starting index (1-based)
        D(i,j) = (D(i+1, j-1) - D(i, j-1)) / (x(i+j-1) - x(i));
    end
end

% Display table in standard textbook format:
fprintf('Standard Divided Difference Table:\n');
fprintf('k   x_k      f[·]       f[·,·]     f[·,·,·]   f[·,·,·,·]  f[·,…,·]\n');
fprintf('---------------------------------------------------------------------\n');
for i = 1:n
    fprintf('%d   %.1f   ', i-1, x(i));
    for j = 1:n
        if j <= n - i + 1
            fprintf('%10.6f ', D(i,j));
        else
            fprintf('           ');
        end
    end
    fprintf('\n');
end

% Coefficients for Newton polynomial about x0:
c = zeros(1, n);
for k = 0:n-1
    c(k+1) = D(1, k+1);  % f[x0, x1, ..., xk]
end
fprintf('\nNewton coefficients (about x0 = %.1f):\n', x(1));
for k = 0:n-1
    fprintf('c_%d = f[x0..x%d] = %.8f\n', k, k, c(k+1));
end

% ---------- Helper: Newton evaluation (nested form) ----------
function val = eval_newton(z, x_nodes, coeffs)
    % coeffs = [c0, c1, ..., cm] where m = length(coeffs)-1
    % x_nodes = [x0, x1, ..., x_{m-1}] (length = m)
    m = length(coeffs);
    val = coeffs(m) * ones(size(z));
    for j = m-1:-1:1
        val = val .* (z - x_nodes(j)) + coeffs(j);
    end
end

% ---------- 2 & 3. Build p1~p4 and evaluate at 4.5, 7.5 ----------
x_test = [4.5, 7.5];
f_true_exact = sqrt(x_test);               % true value (for error only)
f_true_given  = [sqrt(4.5), sqrt(7.5)];    % just for reference (high precision)

fprintf('\nEvaluations (using given 5-digit table only!):\n');
fprintf('x     f_true      ');
for k = 1:4
    fprintf('p_%d(x)      ', k);
end
fprintf('\n');
fprintf('-------------------------------------------------------------\n');

results = zeros(length(x_test), 4);
for ti = 1:length(x_test)
    z = x_test(ti);
    row = [];
    for k = 1:4   % pk uses x0..xk (k+1 points), degree k
        coeffs_k = c(1:k+1);
        x_base    = x(1:k);  % [x0, ..., x_{k-1}]
        pk_z = eval_newton(z, x_base, coeffs_k);
        results(ti, k) = pk_z;
        row = [row, pk_z];
    end
    fprintf('%.1f  %.6f  ', z, sqrt(z));
    fprintf('%.6f  ', row);
    fprintf('\n');
end

% ---------- 4. Errors ----------
fprintf('\nAbsolute Errors |p_k(x) - sqrt(x)|:\n');
fprintf('x     ');
for k = 1:4
    fprintf('|p_%d-f|    ', k);
end
fprintf('\n');
fprintf('-------------------------------------------\n');
for ti = 1:length(x_test)
    err = abs(results(ti,:) - sqrt(x_test(ti)));
    fprintf('%.1f  ', x_test(ti));
    fprintf('%.2e  ', err);
    fprintf('\n');
end