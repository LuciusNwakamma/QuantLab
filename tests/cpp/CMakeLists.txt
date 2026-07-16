cmake_minimum_required(VERSION 3.20)
project(QuantLabTests)

include(GoogleTest)

# Order book tests
add_executable(test_order_book
    test_order_book.cpp
)
target_link_libraries(test_order_book PRIVATE quantlab_order_book GTest::gtest_main)
gtest_discover_tests(test_order_book)

# Risk engine tests
add_executable(test_risk_engine
    test_risk_engine.cpp
)
target_link_libraries(test_risk_engine PRIVATE quantlab_risk_engine GTest::gtest_main)
gtest_discover_tests(test_risk_engine)

# Backtester tests
add_executable(test_backtester
    test_backtester.cpp
)
target_link_libraries(test_backtester PRIVATE quantlab_backtester GTest::gtest_main)
gtest_discover_tests(test_backtester)

# Portfolio optimizer tests
add_executable(test_portfolio_optimizer
    test_portfolio_optimizer.cpp
)
target_link_libraries(test_portfolio_optimizer PRIVATE quantlab_portfolio_optimizer GTest::gtest_main)
gtest_discover_tests(test_portfolio_optimizer)

# Monte Carlo tests
add_executable(test_monte_carlo
    test_monte_carlo.cpp
)
target_link_libraries(test_monte_carlo PRIVATE quantlab_monte_carlo GTest::gtest_main)
gtest_discover_tests(test_monte_carlo)
