% Fourier series of f(x) = x + x^2 on (-pi, pi)
clear; clc; close all;

N1 = 10;
N2 = 50;
x = linspace(-pi, pi, 1000);          % 1×1000 row vector
f_exact = x + x.^2;

% Fourier partial sum (vectorized safely)
fourier_partial = @(x, N) pi^2/3 + ...
    4 * sum( (-1).^(1:N).' .* cos( (1:N).' * x ) ./ ((1:N).^2).' , 1) + ...
    2 * sum( (-1).^((1:N)+1).' .* sin( (1:N).' * x ) ./ (1:N).' , 1);

% But to avoid dimension confusion, better reshape x as column temporarily
% Alternative: use meshgrid-style computation
fourier_series = @(x, N) pi^2/3 + ...
    4 * sum( (-1).^(1:N) .* cos( x(:) * (1:N) ) ./ (1:N).^2 , 2).' + ...
    2 * sum( (-1).^((1:N)+1) .* sin( x(:) * (1:N) ) ./ (1:N) , 2).';

% Explanation:
% x(:) is M×1, (1:N) is 1×N → x(:)*(1:N) = M×N matrix: [x_i * n]
% cos(x(:)*(1:N)) is M×N
% Divide by (1:N).^2 (1×N) → automatic expansion
% sum(...,2) sums over n (columns), result M×1
% Then transpose to 1×M to match x

f_N1 = fourier_series(x, N1);
f_N2 = fourier_series(x, N2);

figure;
plot(x, f_exact, 'k', 'LineWidth', 2); hold on;
plot(x, f_N1, 'b--', 'LineWidth', 1.2);
plot(x, f_N2, 'r-.', 'LineWidth', 1.5);
legend('f(x) = x + x^2', ['N = ', num2str(N1)], ['N = ', num2str(N2)], ...
       'Location', 'northwest');
xlabel('x'); ylabel('f(x)');
title('Fourier Series Approximation of f(x) = x + x^2');
grid on;

% Verify value at x = pi
x_pi = pi;
f_limit = pi^2;  % average of left and right limits
approx_pi = fourier_series(x_pi, 100);  % use N=100 for better accuracy
fprintf('At x = pi:\n');
fprintf('Theoretical convergence value: %.8f\n', f_limit);
fprintf('Fourier series (N=100):          %.8f\n', approx_pi);
fprintf('Absolute error:                 %.2e\n', abs(f_limit - approx_pi));

% Verify Basel problem: sum 1/n^2 = pi^2/6
S = sum(1./(1:100000).^2);
fprintf('\nSum_{n=1}^{100000} 1/n^2 = %.8f\n', S);
fprintf('pi^2 / 6 =                 %.8f\n', pi^2/6);
fprintf('Difference:                %.2e\n', abs(S - pi^2/6));