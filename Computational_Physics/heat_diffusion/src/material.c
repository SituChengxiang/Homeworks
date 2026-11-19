#include "config.h"
#include "material.h"

double material_alpha(void)
{
    return IRON_ALPHA;
}

double material_alpha_from_krc(void)
{
    return IRON_K / (IRON_RHO * IRON_CP);
}

double material_alpha_rel_dev(void)
{
    double ref;

    ref = material_alpha_from_krc();
    return (material_alpha() - ref) / ref;
}

double material_fourier_number(double alpha, double dx, double dt)
{
    return alpha * dt / (dx * dx);
}

double material_dt_max_ftcs(double alpha, double dx)
{
    return dx * dx / (2.0 * alpha);
}

double material_tau_dirichlet(double alpha, double length)
{
    return length * length / (HD_PI * HD_PI * alpha);
}

double material_tau_periodic(double alpha, double length)
{
    return length * length / (4.0 * HD_PI * HD_PI * alpha);
}