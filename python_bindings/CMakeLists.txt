pybind11_add_module(quantlab_cpp
    src/order_book_bindings.cpp
)

target_link_libraries(quantlab_cpp PRIVATE quantlab_order_book quantlab_risk_engine)
target_link_libraries(quantlab_cpp PRIVATE quantlab_backtester)
target_include_directories(quantlab_cpp PRIVATE ${CMAKE_SOURCE_DIR}/cpp/include)
target_compile_features(quantlab_cpp PRIVATE cxx_std_20)

# Place extension inside Python package folder for direct import.
set_target_properties(quantlab_cpp PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/python/quantlab_cpp
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/python/quantlab_cpp
)

if(MSVC)
    set_target_properties(quantlab_cpp PROPERTIES SUFFIX ".pyd")
endif()
