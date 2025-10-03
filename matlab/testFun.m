t=0:2*pi/99:2*pi;
x=1.15*cos(t);y=3.25*sin(t);  
subplot(2,3,1),plot(x,y),axis normal,%2行3列子图
    grid on,title('Normal and Grid on')  
subplot(2,3,2),plot(x,y),axis equal,
    grid on,title('Equal')  
subplot(2,3,3),plot(x,y),axis square,
    grid on,title('Square')  
subplot(2,3,4),plot(x,y),axis image,
    box off,title('Image and Box off')  
subplot(2,3,5),plot(x,y),axis image fill,
    box off,title('Image and Fill')  
subplot(2,3,6),plot(x,y),axis tight,
    box off,title('Tight')  

