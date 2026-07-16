add_library(quantlab_order_book
    order_book/order_book.cpp
    matching_engine/matching_engine.cpp
)
target_include_directories(quantlab_order_book PUBLIC ${CMAKE_SOURCE_DIR}/cpp/include)
target_link_libraries(quantlab_order_book PUBLIC Threads::Threads)

add_library(quantlab_risk_engine
    risk_engine/risk_engine.cpp
)
target_include_directories(quantlab_risk_engine PUBLIC ${CMAKE_SOURCE_DIR}/cpp/include)

add_library(quantlab_backtester
    backtester/backtester.cpp
)
target_include_directories(quantlab_backtester PUBLIC ${CMAKE_SOURCE_DIR}/cpp/include)
target_link_libraries(quantlab_backtester PUBLIC quantlab_risk_engine)
set_target_properties(quantlab_backtester PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(quantlab_portfolio_optimizer
    portfolio_optimizer/portfolio_optimizer.cpp
)
target_include_directories(quantlab_portfolio_optimizer PUBLIC ${CMAKE_SOURCE_DIR}/cpp/include)

add_library(quantlab_monte_carlo
    monte_carlo/monte_carlo.cpp
)
target_include_directories(quantlab_monte_carlo PUBLIC ${CMAKE_SOURCE_DIR}/cpp/include)

add_library(quantlab_execution_simulator
    execution_simulator/execution_simulator.cpp
)
target_include_directories(quantlab_execution_simulator PUBLIC ${CMAKE_SOURCE_DIR}/cpp/include)
target_link_libraries(quantlab_execution_simulator PUBLIC quantlab_risk_engine Threads::Threads)

add_library(quantlab_market_data
    market_data/data_feed.cpp
)
target_include_directories(quantlab_market_data PUBLIC ${CMAKE_SOURCE_DIR}/cpp/include)
target_link_libraries(quantlab_market_data PUBLIC Threads::Threads nlohmann_json::nlohmann_json spdlog::spdlog)

# Aggregate interface target
add_library(quantlab INTERFACE)
target_link_libraries(quantlab INTERFACE
    quantlab_order_book
    quantlab_risk_engine
    quantlab_backtester
    quantlab_portfolio_optimizer
    quantlab_monte_carlo
    quantlab_execution_simulator
    quantlab_market_data
)
