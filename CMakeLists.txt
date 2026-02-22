cmake_minimum_required(VERSION 3.19)
project(NetProj LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Concurrent)

qt_standard_project_setup()

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

qt_add_executable(net_server
    src/common/framed_socket.cpp
    src/common/integrator.cpp
    src/server/server_main.cpp
)

target_link_libraries(net_server
    PRIVATE
        Qt::Core
        Qt::Network
        Qt::Concurrent
)

qt_add_executable(net_client
    src/common/framed_socket.cpp
    src/common/integrator.cpp
    src/client/client_main.cpp
)

target_link_libraries(net_client
    PRIVATE
        Qt::Core
        Qt::Network
        Qt::Concurrent
)

include(GNUInstallDirs)

install(TARGETS net_server net_client
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

option(NETPROJ_BUILD_TESTS "Build NetProj unit tests" ON)

if (NETPROJ_BUILD_TESTS)
    include(CTest)
    enable_testing()

    find_package(GTest QUIET)
    if (GTest_FOUND)
        add_executable(netproj_tests
            tests/integrator_tests.cpp
            src/common/integrator.cpp
        )
        target_include_directories(netproj_tests PRIVATE src/common)
        target_link_libraries(netproj_tests PRIVATE GTest::gtest GTest::gtest_main Qt::Core)
        add_test(NAME netproj_tests COMMAND netproj_tests)
    endif()
endif()
