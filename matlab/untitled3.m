k=0;x0=0.5;
x1=x0-(x0*exp(x0)-1)/((1+x0)*exp(x0))
while abs(x1-x0)>0.00001 & k<10
    k=k+1, x0=x1;
    x1=x0-(x0*exp(x0)-1)/((1+x0)*exp(x0))
end