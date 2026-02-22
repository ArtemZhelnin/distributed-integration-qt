#include "integrator.h"

#include <QtGlobal>

#include <cmath>
#include <stdexcept>

namespace netproj {

double Integrator::f(double x) {
    return 1.0 / std::log(x);
}

bool Integrator::intervalContainsSingularity(double a, double b) {
    const double lo = std::min(a, b);
    const double hi = std::max(a, b);
    return lo <= 1.0 && 1.0 <= hi;
}

double Integrator::integrate(double a, double b, double h, MethodType method) {
    if (!(h > 0.0)) {
        throw std::invalid_argument("Step h must be > 0");
    }
    if (a == b) {
        return 0.0;
    }
    if (intervalContainsSingularity(a, b)) {
        throw std::invalid_argument("Integration interval contains x=1 singularity");
    }

    switch (method) {
    case MethodType::MidpointRectangles:
        return integrateMidpoint(a, b, h);
    case MethodType::Trapezoids:
        return integrateTrapezoids(a, b, h);
    case MethodType::Simpson:
        return integrateSimpson(a, b, h);
    default:
        throw std::invalid_argument("Unknown method type");
    }
}

/**
 * @brief Compute number of full steps of length h on [a,b] (floor).
 */
static inline quint64 stepsCount(double a, double b, double h) {
    const double len = std::abs(b - a);
    return static_cast<quint64>(std::floor(len / h));
}

double Integrator::integrateMidpoint(double a, double b, double h) {
    const double dir = (b > a) ? 1.0 : -1.0;
    const quint64 n = stepsCount(a, b, h);
    const double step = dir * h;

    double sum = 0.0;
    double x = a;
    for (quint64 i = 0; i < n; ++i) {
        const double mid = x + step * 0.5;
        sum += f(mid);
        x += step;
    }
    return sum * step;
}

double Integrator::integrateTrapezoids(double a, double b, double h) {
    const double dir = (b > a) ? 1.0 : -1.0;
    const quint64 n = stepsCount(a, b, h);
    const double step = dir * h;

    if (n == 0) {
        return 0.0;
    }

    double sum = 0.0;
    double x0 = a;
    for (quint64 i = 0; i < n; ++i) {
        const double x1 = x0 + step;
        sum += 0.5 * (f(x0) + f(x1));
        x0 = x1;
    }
    return sum * step;
}

double Integrator::integrateSimpson(double a, double b, double h) {
    const double dir = (b > a) ? 1.0 : -1.0;
    quint64 n = stepsCount(a, b, h);

    if (n < 2) {
        return integrateTrapezoids(a, b, h);
    }

    if (n % 2 == 1) {
        --n;
    }

    const double step = dir * h;

    double s0 = f(a);
    double s1 = 0.0;
    double s2 = 0.0;

    for (quint64 i = 1; i < n; ++i) {
        const double x = a + static_cast<double>(i) * step;
        if (i % 2 == 1) {
            s1 += f(x);
        } else {
            s2 += f(x);
        }
    }

    const double xn = a + static_cast<double>(n) * step;
    const double sn = f(xn);

    return (step / 3.0) * (s0 + 4.0 * s1 + 2.0 * s2 + sn);
}

} // namespace netproj
