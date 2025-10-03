function result = homework_1(x)
    % 使用泰勒展开计算 e^x
    % 精度要求: 与 MATLAB 内置 exp() 函数的差值小于 1e-6

    tolerance = 1e-6;  
    maxIter = 1000;   % 最大迭代次数，防止无限循环
    accuracy = exp(x);
    
    term = 1;          % 当前第k项的值，从1开始
    result = term;     % 累加结果
    k = 1;             % 第k项
    
    while k <= maxIter
        % 计算下一项: term = term * x / k
        term = term * x / k;
        result = result + term;
        
        % 检查收敛条件
        if abs(result - accuracy) < tolerance
            break;
        end
        k = k + 1;
    end

    finalError = result - exp(x); % 与内置函数的实际差值

    % 高精度输出
    fprintf('Final result: %.15e\n', result);
    fprintf('Final error (result - exp(x)): %.15e\n', finalError);

    if k > maxIter
        warning('Maximum iterations reached. Result may not meet precision requirement.');
    end
end