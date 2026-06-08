// visualize_optical.sce
// 可视化单折射球面：轴向交点 vs 瞳高、像差 vs u、光线轨迹示意

clear;
clc;

function zsurf = spherical_surface_z(y, R)
    zsurf = R - sqrt(R * R - y * y);
endfunction

function [zint, ok, tx, tz] = ray_axis_intercept(u, R, n1, n2, y)
    zsurf = spherical_surface_z(y, R);

    // Object point O = (0, -u), surface point P = (y, zsurf)
    dx = y;
    dz = zsurf + u;
    L = sqrt(dx * dx + dz * dz);
    ix = dx / L;
    iz = dz / L;

    // Unit normal from medium 1 to medium 2
    nx = -y / R;
    nz = -(zsurf - R) / R;

    cosi = -(nx * ix + nz * iz);
    eta = n1 / n2;
    k = 1 - eta * eta * (1 - cosi * cosi);

    if k < 0 then
        zint = %nan;
        ok = %f;
        tx = 0; tz = 0;
        return;
    end

    tx = eta * ix + (eta * cosi - sqrt(k)) * nx;
    tz = eta * iz + (eta * cosi - sqrt(k)) * nz;

    if abs(tx) < 1d-12 then
        zint = %nan;
        ok = %f;
        return;
    end

    lambda = -y / tx;
    zint = zsurf + lambda * tz;
    ok = %t;
endfunction

function [spread, mean_z, zvals] = aberration_spread(u, R, n1, n2, ys)
    zvals = zeros(1, length(ys));
    for k = 1:length(ys)
        [zvals(k), ok] = ray_axis_intercept(u, R, n1, n2, ys(k));
        if ~ok then
            error("Ray tracing failed at one pupil height.");
        end
    end
    spread = max(zvals) - min(zvals);
    mean_z = mean(zvals);
endfunction

// 参数
R = 100.0;
n1 = 1.0;
n2 = 1.5;
u_ap = R * (n2 - n1) / n1;
v_ap = R * (n2 - n1) / n2;
ys = [1 5 10 15 20 25 30 35 40];

// 1) 轴向交点 vs 瞳高，比较三个 u
u_list = [0.9 * u_ap, u_ap, 1.1 * u_ap];
z_matrix = zeros(length(u_list), length(ys));
for i = 1:length(u_list)
    for k = 1:length(ys)
        [z_matrix(i,k), ok] = ray_axis_intercept(u_list(i), R, n1, n2, ys(k));
        if ~ok then z_matrix(i,k) = %nan; end
    end
end

scf();
clf();
a = gca();
a.auto_clear = "off";
markers = ['o' 's' 'd'];
for i = 1:size(z_matrix,1)
    plot(ys, z_matrix(i,:), '-x');
end
a.auto_clear = "on";
xtitle("Axial intercept z vs pupil height y","y (mm)","z (mm)");
legend(["0.9 u_ap","u_ap","1.1 u_ap"]);
savefile1 = "z_vs_y.png";
xs2png(gcf().figure_id, savefile1);
mprintf("Saved %s\n", savefile1);

// 2) 像差（spread）随 u 的变化
u_scan = linspace(0.8 * u_ap, 1.2 * u_ap, 81);
spread_scan = zeros(1, length(u_scan));
for i = 1:length(u_scan)
    [spread_scan(i), mean_z, zvals] = aberration_spread(u_scan(i), R, n1, n2, ys);
end

[min_spread, idx] = min(spread_scan);
scf();
clf();
a = gca();
a.auto_clear = "off";
plot(u_scan, spread_scan, '-b');
plot(u_scan(idx), spread_scan(idx), 'or');
a.auto_clear = "on";
xtitle("Aberration spread vs u","u (mm)","spread (mm)");
legend(["spread(u)","min spread"]);
savefile2 = "spread_vs_u.png";
xs2png(gcf().figure_id, savefile2);
mprintf("Saved %s (minimum at u=%.6f, spread=%.12e)\n", savefile2, u_scan(idx), min_spread);

// 3) 光线轨迹示意（yz 平面截面）
ys_ray = [-40 -30 -20 -10 0 10 20 30 40];
scf();
clf();
a = gca();
a.auto_clear = "off";

// 绘制球面轮廓（矢量化计算以避免匿名函数语法问题）
yy = linspace(-R, R, 400);
zz = R - sqrt(R * R - yy.^2);
plot(yy, zz, '-k');

// 对于每条光线，画入射线和折射线
u_example = u_ap; // 以 aplanatic 点为例
for i = 1:length(ys_ray)
    y = ys_ray(i);
    if abs(y) >= R then continue; end
    zsurf = spherical_surface_z(y, R);
    // 入射方向
    dx = y;
    dz = zsurf + u_example;
    L = sqrt(dx*dx + dz*dz);
    ix = dx / L; iz = dz / L;
    // 折射方向
    [zint, ok, tx, tz] = ray_axis_intercept(u_example, R, n1, n2, y);
    if ~ok then continue; end
    // 绘制从物点到球面
    O = [0, -u_example];
    P = [y, zsurf];
    plot([O(1), P(1)], [O(2), P(2)], '-r');
    // 绘制折射线段（延长到 zint 或再延伸一段）
    tlen = 1.2 * (zint - zsurf) / tz; // scale param so that z ~ zint*1.2
    Q = [P(1) + tx * tlen, P(2) + tz * tlen];
    plot([P(1), Q(1)], [P(2), Q(2)], '-b');
end

a.auto_clear = "on";
xtitle("Ray paths through spherical surface","y (mm)","z (mm)");
legend(["surface","incident","refracted"]);
savefile3 = "ray_paths.png";
xs2png(gcf().figure_id, savefile3);
mprintf("Saved %s\n", savefile3);

mprintf("Visualizations complete. Files: %s, %s, %s\n", savefile1, savefile2, savefile3);
