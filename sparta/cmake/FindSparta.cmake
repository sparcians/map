include(FindPackageHandleStandardArgs)

if(NOT SPARTA_FOUND)
    find_package(Boost REQUIRED COMPONENTS timer filesystem serialization program_options)
    find_package(HDF5 REQUIRED COMPONENTS CXX)
    find_package(SQLite3 REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(yaml-cpp REQUIRED)
    find_package(RapidJSON REQUIRED)
    find_package(Threads REQUIRED)

    # On Linux we need to link against rt as well
    if (NOT APPLE)
      find_library(LIBRT rt)
    endif()

    find_path(SPARTA_INCLUDE_DIRS sparta/sparta.hpp
        HINTS ${SPARTA_INCLUDE_DIR} ${SPARTA_SEARCH_DIR}
        HINTS ENV CPATH
        HINTS ENV SPARTA_INSTALL_HOME
        PATH_SUFFIXES include)

    find_path(SIMDB_INCLUDE_DIRS simdb/ObjectManager.hpp
        HINTS ${SPARTA_INCLUDE_DIR} ${SPARTA_SEARCH_DIR}
        HINTS ENV CPATH
        HINTS ENV SPARTA_INSTALL_HOME
        PATH_SUFFIXES include)
    list(APPEND SPARTA_INCLUDE_DIRS ${SIMDB_INCLUDE_DIRS})

    set(SPARTA_SEARCH_COMPOMPONENTS simdb sparta)
    foreach(_comp ${SPARTA_SEARCH_COMPOMPONENTS})
        # Search for the libraries
        find_library(SPARTA_${_comp}_LIBRARY ${_comp}
            HINTS ${SPARTA_LIBRARY} ${SPARTA_SEARCH_DIR}
            HINTS ENV LIBRARY_PATH
            HINTS ENV SPARTA_INSTALL_HOME
            PATH_SUFFIXES lib)

        if(SPARTA_${_comp}_LIBRARY)
            list(APPEND SPARTA_LIBRARIES "${SPARTA_${_comp}_LIBRARY}")
        endif()

        if(SPARTA_${_comp}_LIBRARY AND EXISTS "${SPARTA_${_comp}_LIBRARY}")
            set(SPARTA_${_comp}_FOUND TRUE)
        else()
            set(SPARTA_${_comp}_FOUND FALSE)
        endif()

        # Mark internal variables as advanced
        mark_as_advanced(SPARTA_${_comp}_LIBRARY)
    endforeach()

    find_package_handle_standard_args(Sparta
        REQUIRED_VARS SPARTA_INCLUDE_DIRS SPARTA_LIBRARIES
        HANDLE_COMPONENTS
        VERSION_VAR SPARTA_VERSION)

    ##################################
    # Create targets
    ##################################

    if(NOT CMAKE_VERSION VERSION_LESS 3.0 AND SPARTA_FOUND)
        add_library(SPARTA::libsparta STATIC IMPORTED)
        set_property(TARGET SPARTA::libsparta PROPERTY IMPORTED_LOCATION "${SPARTA_sparta_LIBRARY}")
        add_library(SPARTA::libsimdb STATIC IMPORTED)
        set_property(TARGET SPARTA::libsimdb PROPERTY IMPORTED_LOCATION "${SPARTA_simdb_LIBRARY}")

        add_library(SPARTA::sparta INTERFACE IMPORTED)
        # Workaround as per https://github.com/jbeder/yaml-cpp/issues/774#issuecomment-927357017
        if (yaml-cpp_VERSION_MINOR EQUAL 7)
          get_target_property(YAML_CPP_INCLUDE_DIR yaml-cpp INTERFACE_INCLUDE_DIRECTORIES)
        else ()
          # To be used with yaml-cpp 0.8 or (assumed) higer
          get_target_property(YAML_CPP_INCLUDE_DIR yaml-cpp::yaml-cpp INTERFACE_INCLUDE_DIRECTORIES)
        endif ()
        set_property(TARGET SPARTA::sparta
          PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${SPARTA_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${SQLite3_INCLUDE_DIRS} ${HDF5_CXX_INCLUDE_DIRS} ${RAPIDJSON_INCLUDE_DIR} ${RapidJSON_INCLUDE_DIR} ${YAML_CPP_INCLUDE_DIR})
        set_property(TARGET SPARTA::sparta
          PROPERTY INTERFACE_LINK_LIBRARIES SPARTA::libsparta SPARTA::libsimdb HDF5::HDF5 SQLite::SQLite3
          Boost::filesystem Boost::serialization Boost::timer Boost::program_options
          ZLIB::ZLIB yaml-cpp Threads::Threads)

        if(LIBRT)
          set_property(TARGET SPARTA::sparta APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${LIBRT})
        endif ()

        set_property(TARGET SPARTA::sparta
          PROPERTY INTERFACE_COMPILE_FEATURES cxx_std_17)
        include(${CMAKE_CURRENT_LIST_DIR}/SpartaTestingMacros.cmake)
        include(${CMAKE_CURRENT_LIST_DIR}/SimdbTestingMacros.cmake)
        set(SPARTA_FOUND TRUE)
    endif()

    mark_as_advanced(SPARTA_INCLUDE_DIRS SPARTA_LIBRARIES)

endif()
