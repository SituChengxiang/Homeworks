#include "rutherford.h"
#include "units.h"
#include <math.h>

double rutherford_geometric_b(double y0)
{
	return fabs(y0);
}

double rutherford_v_infinity(double e_total)
{
	/* U(r -> inf) = 0，故 E 全部是动能 */
	return sqrt(2.0 * e_total / MASS_ALPHA);
}

double rutherford_impact_parameter(double e_total, double l)
{
	return fabs(l) / (MASS_ALPHA * rutherford_v_infinity(e_total));
}

double rutherford_eccentricity(double e_total, double l)
{
	double c2 = COUPLING * COUPLING;

	return sqrt(1.0 + 2.0 * e_total * l * l / (MASS_ALPHA * c2));
}

double rutherford_deflection_deg(double e_total, double l)
{
	double eps = rutherford_eccentricity(e_total, l);
	double s   = 1.0 / eps;

	if (s > 1.0)  s = 1.0;
	if (s < -1.0) s = -1.0;

	return 2.0 * asin(s) * 180.0 / PI_CONST;
}

double rutherford_closest_approach(double e_total, double l)
{
	/* E = A u^2 + COUPLING * u - E = 0,  u = 1/r, A = L^2/(2m)
	 * 取正根 u = (-C + sqrt(C^2 + 4 A E)) / (2 A)                   */
	double a = l * l / (2.0 * MASS_ALPHA);
	double disc;
	double u;

	if (a <= 0.0) {
		/* 对心碰撞（L = 0）：E = C/r_min */
		return COUPLING / e_total;
	}

	disc = COUPLING * COUPLING + 4.0 * a * e_total;
	u    = (-COUPLING + sqrt(disc)) / (2.0 * a);

	return 1.0 / u;
}
