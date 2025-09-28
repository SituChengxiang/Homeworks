fx='exp(-x.^2)';
I=quad(fx,0,1,1e-8);
fprintf("%d",I)