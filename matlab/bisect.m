function [c,err,yc]=bisect(f,a,b,delta)
    ya=feval(f,a); yb=feval(f,b);
    if ya*yb>0
        reutrn;
    end
    max1=1+round((log(b-a)-log(delta))/log2;
    for k=1:max1
        c=(a+b)/2; %中点
        yc=feval(f,c);
    if yc==0
        a=c; b=c;
    elseif yb*yc>0
        b=c; yb=yc;
    else
        a=c;ya=yc;
    end
    if b-a<delta
    break;
    end
    c=(a+b)/2;
    err=abs(b-a)/2;
    yc=feval(f,c);
end
% 暴力迭代。输入参数为函数的，放入的应当是句柄，带@