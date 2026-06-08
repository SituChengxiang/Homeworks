clear;
clc;

// Single refracting spherical surface.
// Coordinate convention:
//   vertex at z = 0, center of curvature at z = R,
//   object on the left, image on the right, light travels left -> right.

function zsurf = spherical_surface_z(y, R)
    zsurf = R - sqrt(R * R - y * y);
endfunction

function [zint, ok] = ray_axis_intercept(u, R, n1, n2, y)
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

// Classic example: air -> glass
R = 100.0;
n1 = 1.0;
n2 = 1.5;

// Aplanatic point in this sign convention
u_ap = R * (n2 - n1) / n1;
v_ap = R * (n2 - n1) / n2;

ys = [1 5 10 15 20 25 30 35 40];

mprintf("Single refracting spherical surface verification\n");
mprintf("R = %.6f mm, n1 = %.6f, n2 = %.6f\n", R, n1, n2);
mprintf("Aplanatic point (theory): u = %.6f mm, v = %.6f mm\n\n", u_ap, v_ap);

u_list = [0.9 * u_ap, u_ap, 1.1 * u_ap];

for i = 1:length(u_list)
    u = u_list(i);
    [spread, mean_z, zvals] = aberration_spread(u, R, n1, n2, ys);

    mprintf("u = %.6f mm\n", u);
    mprintf("  axial intercepts: ");
    for k = 1:length(zvals)
        mprintf("%.12f ", zvals(k));
    end
    mprintf("\n");
    mprintf("  spread = %.12e mm, mean = %.12f mm\n\n", spread, mean_z);
end

// Scan around the aplanatic point to confirm the minimum numerically.
u_scan = linspace(0.8 * u_ap, 1.2 * u_ap, 41);
spread_scan = zeros(1, length(u_scan));

for i = 1:length(u_scan)
    [spread_scan(i), mean_z, zvals] = aberration_spread(u_scan(i), R, n1, n2, ys);
end

[min_spread, idx] = min(spread_scan);
mprintf("Scan minimum:\n");
mprintf("  u = %.6f mm\n", u_scan(idx));
mprintf("  spread = %.12e mm\n", min_spread);
