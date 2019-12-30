#
# Makefile to list the source and include dependencies for the core
#
ifndef CORE_MK_PARSED
export CORE_MK_PARSED := 1

ifndef CORE
$(error "The core's path variable (CORE) is not defined.  Please define before including this file")
endif

endif
