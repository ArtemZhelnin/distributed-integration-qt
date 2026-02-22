#pragma once

#include "protocol.h"

#include <QtGlobal>

namespace netproj {

/**
 * @brief Numerical integrator for f(x)=1/ln(x).
 */
class Integrator {
public:
    /**
     * @brief Integrate f(x)=1/ln(x) on [a,b] with step h using selected method.
     *
     * @param a Lower bound.
     * @param b Upper bound.
     * @param h Integration step (must be > 0).
     * @param method Integration method.
     *
     * @throws std::invalid_argument If h <= 0 or if interval contains x=1.
     */
    static double integrate(double a, double b, double h, MethodType method);

private:
    /**
     * @brief Function value f(x)=1/ln(x).
     */
    static double f(double x);

    /**
     * @brief Check if [a,b] contains singularity at x=1.
     */
    static bool intervalContainsSingularity(double a, double b);

    static double integrateMidpoint(double a, double b, double h);
    static double integrateTrapezoids(double a, double b, double h);
    static double integrateSimpson(double a, double b, double h);
};

} // namespace netproj
