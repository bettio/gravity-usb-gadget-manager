# Finds libconnman-qt5 and its libraries
# Distributed under the BSD license. See COPYING-CMAKE-SCRIPTS for details.

#defining any of these disables systemd support
if (NOT LIBCONNMANQT5_FOUND)
    find_package(PkgConfig)
    if (PKG_CONFIG_FOUND)
        pkg_check_modules(LIBCONNMANQT5 QUIET "connman-qt5")
    endif (PKG_CONFIG_FOUND)

    if (LIBCONNMANQT5_FOUND)
        message(STATUS "Found libconnman-qt5")
    endif(LIBCONNMANQT5_FOUND)

    mark_as_advanced(LIBCONNMANQT5_FOUND)
    mark_as_advanced(LIBCONNMANQT5_INCLUDE_DIR)
    mark_as_advanced(LIBCONNMANQT5_LIBRARIES)
endif ()
