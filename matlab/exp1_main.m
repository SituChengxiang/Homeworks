clc;
format long
sqrt_newton(5)
fprintf('precise value: %.10f\n',sqrt(5))
fprintf('diff: %.10f\n',abs(ans-sqrt(5)))

%{
Iteration 0: 5.0000000000
Iteration 1: 3.0000000000
Iteration 2: 2.3333333333
Iteration 3: 2.2380952381
Iteration 4: 2.2360688956
Iteration 5: 2.2360679775
Iteration 6: 2.2360679775

ans =

   2.236067977499790

precise value: 2.2360679775
diff: 0.0000000000
%}