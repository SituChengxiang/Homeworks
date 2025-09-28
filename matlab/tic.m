tic;
noprealloc;
toc;

function noprealloc
    N = 4000;
    A=zeros(N,N);
    % 不预分配版本
    for ii = 1:N
        for jj = 1:N
            A(ii,jj) = ii * jj;
        end
    end
end