add_library(axiom_jit STATIC)

target_sources(axiom_jit PRIVATE 
    jit.cpp
)

target_include_directories(axiom_jit PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(axiom_jit PUBLIC 
    cxx_std_23
)

set_project_warnings(axiom_jit)