#include "../src/common/integrator.h"

#include <gtest/gtest.h>

TEST(Integrator, RejectsSingularity) {
    EXPECT_THROW(netproj::Integrator::integrate(0.5, 2.0, 0.1, netproj::MethodType::Trapezoids), std::invalid_argument);
}

TEST(Integrator, RejectsZeroStep) {
    EXPECT_THROW(netproj::Integrator::integrate(2.0, 10.0, 0.0, netproj::MethodType::Simpson), std::invalid_argument);
}

TEST(Integrator, ReferenceIntegralSimpson) {
    const double v = netproj::Integrator::integrate(2.0, 10.0, 1e-4, netproj::MethodType::Simpson);
    EXPECT_NEAR(v, 5.120435, 2e-3);
}
