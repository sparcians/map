project(SYSTEMC_EXAMPLE)

include (../ExamplesMacros.cmake)

if(SYSTEMC_SUPPORT)

  set (TLM_EXAMPLES_DIR $ENV{SYSTEMC_HOME}/examples/tlm)
  if (NOT EXISTS ${TLM_EXAMPLES_DIR})
    message (FATAL_ERROR "TLM examples directory NOT found.  Will not build SysC example: sparta/example/SystemC")
  else()
    message (STATUS "TLM examples directory: ${TLM_EXAMPLES_DIR}")

    add_executable(sparta_sysc_model_2_phase
      ../common/SpartaSystemCSimulator.cpp
      ../common/systemc_example_top.cpp

     ../common/sparta-target/SpartaMemory.cpp
      sparta-target/SpartaTLMTargetGasket.cpp

      # From the TLM examples
      ${TLM_EXAMPLES_DIR}/at_2_phase/src/initiator_top.cpp
      ${TLM_EXAMPLES_DIR}/common/src/select_initiator.cpp
      ${TLM_EXAMPLES_DIR}/common/src/report.cpp
      ${TLM_EXAMPLES_DIR}/common/src/traffic_generator.cpp
      ${TLM_EXAMPLES_DIR}/common/src/memory.cpp

      ../common/sc_main.cpp
      )

    sparta_application(sparta_sysc_model_2_phase)
    target_link_libraries (sparta_sysc_model_2_phase SystemC::systemc)

    include_directories(
      # For reporting, traffic gen
      ${TLM_EXAMPLES_DIR}/common/include

      # For SimpleBus
      ${TLM_EXAMPLES_DIR}/common/include/models

      # For initiator top, et. al. used in this example
      ${TLM_EXAMPLES_DIR}/at_2_phase/include/

      ../common/
      ../common/sparta-target/
      )

    sparta_named_test(sparta_sys_phase2_tlm sparta_sysc_model_2_phase)

  endif()

endif()
