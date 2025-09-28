d=pi/100;
t=0:d:2*pi;
x=sin(t);
dt=d;
x_eps=sin(t+dt);
dxdt_eps=(x_eps-x)/dt;
plot(t,x)
hold on
plot(t,dxdt_eps)