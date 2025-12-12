project(SYSTEMC_EXAMPLE)

include (../ExamplesMacros.cmake)

if(SYSTEMC_SUPPORT AND TLM_EXAMPLES_DIR)

  if (NOT EXISTS ${TLM_EXAMPLES_DIR})
    message (FATAL_ERROR "TLM examples directory NOT found.  Please set -DTLM_EXAMPLES_DIR=/path/to/examples/tlm")
  else()
    message (STATUS "TLM example 4_phase enabled")

    add_executable(sparta_sysc_model_4_phase
      ../common/SpartaSystemCSimulator.cpp
      ../common/systemc_example_top.cpp

     ../common/sparta-target/SpartaMemory.cpp
      sparta-target/SpartaTLMTargetGasket.cpp

      # From the TLM examples
      ${TLM_EXAMPLES_DIR}/at_4_phase/src/initiator_top.cpp
      ${TLM_EXAMPLES_DIR}/common/src/select_initiator.cpp
      ${TLM_EXAMPLES_DIR}/common/src/report.cpp
      ${TLM_EXAMPLES_DIR}/common/src/traffic_generator.cpp
      ${TLM_EXAMPLES_DIR}/common/src/memory.cpp

      ../common/sc_main.cpp
      )

    sparta_application(sparta_sysc_model_4_phase)
    target_link_libraries (sparta_sysc_model_4_phase SystemC::systemc)

    include_directories(
      # For reporting, traffic gen
      ${TLM_EXAMPLES_DIR}/common/include

      # For SimpleBus
      ${TLM_EXAMPLES_DIR}/common/include/models

      # For initiator top, et. al. used in this example
      ${TLM_EXAMPLES_DIR}/at_4_phase/include/

      ../common/
      ../common/sparta-target/

      )

    sparta_named_test(sparta_sys_phase4_tlm sparta_sysc_model_4_phase)

  endif()

else()
  message (STATUS "TLM 4_phase example disabled (TLM_EXAMPLES_DIR not set)")
endif()
