include(FindPackageHandleStandardArgs)

if(TARGET mbedtls AND TARGET mbedcrypto AND TARGET mbedx509)
    if(NOT TARGET MbedTLS::MbedTLS)
        add_library(MbedTLS::MbedTLS INTERFACE IMPORTED)
        target_link_libraries(MbedTLS::MbedTLS INTERFACE mbedtls)
    endif()

    if(NOT TARGET MbedTLS::MbedCrypto)
        add_library(MbedTLS::MbedCrypto INTERFACE IMPORTED)
        target_link_libraries(MbedTLS::MbedCrypto INTERFACE mbedcrypto)
    endif()

    if(NOT TARGET MbedTLS::MbedX509)
        add_library(MbedTLS::MbedX509 INTERFACE IMPORTED)
        target_link_libraries(MbedTLS::MbedX509 INTERFACE mbedx509)
    endif()

    get_target_property(_mbedtls_include_dirs mbedtls INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT _mbedtls_include_dirs)
        get_target_property(_mbedtls_include_dirs mbedtls INCLUDE_DIRECTORIES)
    endif()

    if(NOT _mbedtls_include_dirs)
        set(_mbedtls_include_dirs "")
    endif()

    set(MbedTLS_VERSION "3.6.2")
    set(MbedTLS_FOUND TRUE)
    set(MbedTLS_INCLUDE_DIR "${_mbedtls_include_dirs}")
    set(MBEDTLS_INCLUDE_DIRS "${_mbedtls_include_dirs}")
    set(MbedTLS_LIBRARY mbedtls)
    set(MbedCrypto_LIBRARY mbedcrypto)
    set(MbedX509_LIBRARY mbedx509)
    set(MbedTLS_LIBRARIES mbedtls mbedcrypto mbedx509)
    set(MBEDTLS_LIBRARIES mbedtls mbedcrypto mbedx509)

    find_package_handle_standard_args(
        MbedTLS
        REQUIRED_VARS MbedTLS_INCLUDE_DIR MbedTLS_LIBRARY MbedCrypto_LIBRARY MbedX509_LIBRARY
        VERSION_VAR MbedTLS_VERSION)

    mark_as_advanced(MbedTLS_INCLUDE_DIR MbedTLS_LIBRARY MbedCrypto_LIBRARY MbedX509_LIBRARY)
    return()
endif()

message(FATAL_ERROR "MbedTLS targets are unavailable. Ensure the superbuild fetched mbedtls before calling find_package(MbedTLS).")
