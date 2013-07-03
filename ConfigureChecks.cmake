include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckTypeSize)
include(CheckStructHasMember)
include(CheckPrototypeDefinition)
include(TestBigEndian)

set(PACKAGE ${APPLICATION_NAME})
set(VERSION ${APPLICATION_VERSION})
set(DATADIR ${DATA_INSTALL_DIR})
set(LIBDIR ${LIB_INSTALL_DIR})
set(PLUGINDIR "${PLUGIN_INSTALL_DIR}-${LIBRARY_SOVERSION}")
set(SYSCONFDIR ${SYSCONF_INSTALL_DIR})

set(BINARYDIR ${CMAKE_BINARY_DIR})
set(SOURCEDIR ${CMAKE_SOURCE_DIR})

function(COMPILER_DUMPVERSION _OUTPUT_VERSION)
    # Remove whitespaces from the argument.
    # This is needed for CC="ccache gcc" cmake ..
    string(REPLACE " " "" _C_COMPILER_ARG "${CMAKE_C_COMPILER_ARG1}")

    execute_process(
        COMMAND
            ${CMAKE_C_COMPILER} ${_C_COMPILER_ARG} -dumpversion
        OUTPUT_VARIABLE _COMPILER_VERSION
    )

    string(REGEX REPLACE "([0-9])\\.([0-9])(\\.[0-9])?" "\\1\\2"
           _COMPILER_VERSION "${_COMPILER_VERSION}")

    set(${_OUTPUT_VERSION} ${_COMPILER_VERSION} PARENT_SCOPE)
endfunction()

if(CMAKE_COMPILER_IS_GNUCC AND NOT MINGW AND NOT OS2)
    compiler_dumpversion(GNUCC_VERSION)
    if (NOT GNUCC_VERSION EQUAL 34)
        set(CMAKE_REQUIRED_FLAGS "-fvisibility=hidden")
        check_c_source_compiles(
"void __attribute__((visibility(\"default\"))) test() {}
int main(void){ return 0; }
" WITH_VISIBILITY_HIDDEN)
        set(CMAKE_REQUIRED_FLAGS "")
    endif (NOT GNUCC_VERSION EQUAL 34)
endif(CMAKE_COMPILER_IS_GNUCC AND NOT MINGW AND NOT OS2)

# HEADERS
check_include_file(sys/filio.h HAVE_SYS_FILIO_H)

# FUNCTIONS
check_function_exists(strncpy HAVE_STRNCPY)
check_function_exists(vsnprintf HAVE_VSNPRINTF)
check_function_exists(snprintf HAVE_SNPRINTF)

if (WIN32)
    check_function_exists(_vsnprintf_s HAVE__VSNPRINTF_S)
    check_function_exists(_vsnprintf HAVE__VSNPRINTF)
    check_function_exists(_snprintf HAVE__SNPRINTF)
    check_function_exists(_snprintf_s HAVE__SNPRINTF_S)
endif (WIN32)

if (UNIX)
    if (NOT LINUX)
        # libsocket (Solaris)
        check_library_exists(socket getaddrinfo "" HAVE_LIBSOCKET)
        if (HAVE_LIBSOCKET)
          set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} socket)
        endif (HAVE_LIBSOCKET)

        # libnsl/inet_pton (Solaris)
        check_library_exists(nsl inet_pton "" HAVE_LIBNSL)
        if (HAVE_LIBNSL)
            set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} nsl)
        endif (HAVE_LIBNSL)
    endif (NOT LINUX)

    check_function_exists(getaddrinfo HAVE_GETADDRINFO)
endif (UNIX)

set(LIBSSH_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} CACHE INTERNAL "libssh required system libraries")

# STRUCT MEMBERS
check_struct_has_member("struct sockaddr" sa_len "sys/socket.h netinet/in.h" HAVE_STRUCT_SOCKADDR_SA_LEN)

# PROTOTYPES
check_prototype_definition(gettimeofday
    "int gettimeofday(struct timeval *tv, struct timezone *tz)"
    "-1"
    "sys/time.h"
    HAVE_GETTIMEOFDAY_TZ)

# IPV6
check_c_source_compiles("
    #include <stdlib.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <net/if.h>

int main(void) {
    struct sockaddr_storage sa_store;
    struct addrinfo *ai = NULL;
    struct in6_addr in6addr;
    int idx = if_nametoindex(\"iface1\");
    int s = socket(AF_INET6, SOCK_STREAM, 0);
    int ret = getaddrinfo(NULL, NULL, NULL, &ai);
    if (ret != 0) {
        const char *es = gai_strerror(ret);
    }

    freeaddrinfo(ai);
    {
        int val = 1;
#ifdef HAVE_LINUX_IPV6_V6ONLY_26
#define IPV6_V6ONLY 26
#endif
        ret = setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY,
                         (const void *)&val, sizeof(val));
    }

    return 0;
}" HAVE_IPV6)

check_c_source_compiles("
#include <sys/socket.h>

int main(void) {
    struct sockaddr_storage s;

    return 0;
}" HAVE_SOCKADDR_STORAGE)

check_c_source_compiles("
void test_destructor_attribute(void) __attribute__ ((destructor));

void test_destructor_attribute(void)
{
    return;
}

int main(void) {
    return 0;
}" HAVE_DESTRUCTOR_ATTRIBUTE)

check_c_source_compiles("
__thread int tls;

int main(void) {
    return 0;
}" HAVE_GCC_THREAD_LOCAL_STORAGE)

check_library_exists(dl dlopen "" HAVE_LIBDL)
if (HAVE_LIBDL)
    find_library(DLFCN_LIBRARY dl)
    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${DLFCN_LIBRARY})
endif (HAVE_LIBDL)

# ENDIAN
if (NOT WIN32)
    test_big_endian(WORDS_BIGENDIAN)
endif (NOT WIN32)

set(SWRAP_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} CACHE INTERNAL "swrap required system libraries")
