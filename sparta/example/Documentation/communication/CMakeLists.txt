cmake_minimum_required(VERSION 3.15)
project(DOCUMENTATION_MODEL)

# A helper macro for compiler the example doc build
# with different defines
macro (example_comm_build name build_defines)
    add_executable(${name} main.cpp)
    target_link_libraries(${name} sparta ${Sparta_LIBS} -L${PROJECT_BINARY_DIR}/../../.. -L${PROJECT_BINARY_DIR}/../../../simdb/)
    set_target_properties(${name} PROPERTIES COMPILE_DEFINITIONS "${build_defines}")
    sparta_test(${name})
endmacro(example_comm_build)

example_comm_build(Events_example_test "EVENTS_EXAMPLE=1")
example_comm_build(Events_dual_example_test "EVENTS_DUAL_EXAMPLE=1")
example_comm_build(Ports_example_test "PORTS_EXAMPLE=1")
