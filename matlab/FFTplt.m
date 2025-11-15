% ==== 参数设置 ====
load logo_curve.mat x y;
Nt = 500;
Ncirc = 50;
% ================
close all;
x = x - mean(x);
y = y - mean(y);
N = numel(x);
z = x + 1i*y;
x0 = 1; dx = 1;
[C, w] = FS(z, x0, dx); % 傅里叶级数
C = C(:); w = w(:);

t = linspace(1,N,Nt);
z1 = sum(C.*exp(1i*w.*t), 1);
figure; plot(x, y); hold on;
plot(real(z1),imag(z1));

th = linspace(0, 2*pi, 200);
costh = cos(th); sinth = sin(th);
N1 = round(N/2-1);
[~, order] = sort(abs(C), 'descend');
C1 = C(order);
w1 = w(order);
z1 = sum(C1.*exp(1i*w1.*t), 1);
plot(real(z1),imag(z1));

for it = 1:Nt
    z1 = [0; cumsum(C1.*exp(1i*w1*t(it)), 1)];
    clf;
    plot(x, y); hold on; axis equal;
    for i = 1:Ncirc
        A = abs(z1(i+1)-z1(i)); x0 = real(z1(i)); y0 = imag(z1(i));
        plot(x0+A*costh, y0+A*sinth, 'g');
    end
    plot(real(z1), imag(z1), '.-');
    axis([-0.7, 0.7, -0.4, 0.7]);
    saveas(gcf, [num2str(it), '.png']);
end
