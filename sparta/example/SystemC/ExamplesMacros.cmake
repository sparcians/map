
#
# Macros to build Sparta example applications
#

if(NOT DEFINED Sparta_LIBS OR "${Sparta_LIBS}" STREQUAL "")
  message (FATAL_ERROR "Sparta_LIBS is not defined. \n"
    "\tThis must be defined to link your application"
    "\tThis variable is found in sparta/cmake/sparta-config.cmake")
endif()

macro(sparta_application build_target)
  target_link_libraries(${build_target} ${Sparta_LIBS})
endmacro(sparta_application)
