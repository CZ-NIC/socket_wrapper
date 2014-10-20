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
check_include_file(sys/signalfd.h HAVE_SYS_SIGNALFD_H)
check_include_file(sys/eventfd.h HAVE_SYS_EVENTFD_H)
check_include_file(sys/timerfd.h HAVE_SYS_TIMERFD_H)
check_include_file(gnu/lib-names.h HAVE_GNU_LIB_NAMES_H)
check_include_file(rpc/rpc.h HAVE_RPC_RPC_H)

# FUNCTIONS
check_function_exists(strncpy HAVE_STRNCPY)
check_function_exists(vsnprintf HAVE_VSNPRINTF)
check_function_exists(snprintf HAVE_SNPRINTF)
check_function_exists(signalfd HAVE_SIGNALFD)
check_function_exists(eventfd HAVE_EVENTFD)
check_function_exists(timerfd_create HAVE_TIMERFD_CREATE)
check_function_exists(bindresvport HAVE_BINDRESVPORT)


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

set(SWRAP_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} CACHE INTERNAL "socket_wrapper required system libraries")

# STRUCTS
check_struct_has_member("struct in_pktinfo" ipi_addr "sys/types.h;sys/socket.h;netinet/in.h" HAVE_STRUCT_IN_PKTINFO)
set(CMAKE_REQUIRED_FLAGS -D_GNU_SOURCE)
check_struct_has_member("struct in6_pktinfo" ipi6_addr "sys/types.h;sys/socket.h;netinet/in.h" HAVE_STRUCT_IN6_PKTINFO)
set(CMAKE_REQUIRED_FLAGS)

# STRUCT MEMBERS
check_struct_has_member("struct sockaddr" sa_len "sys/types.h;sys/socket.h;netinet/in.h" HAVE_STRUCT_SOCKADDR_SA_LEN)
check_struct_has_member("struct msghdr" msg_control "sys/types.h;sys/socket.h" HAVE_STRUCT_MSGHDR_MSG_CONTROL)

# PROTOTYPES
check_prototype_definition(gettimeofday
    "int gettimeofday(struct timeval *tv, struct timezone *tz)"
    "-1"
    "sys/time.h"
    HAVE_GETTIMEOFDAY_TZ)

check_prototype_definition(gettimeofday
    "int gettimeofday(struct timeval *tv, void *tzp)"
    "-1"
    "sys/time.h"
    HAVE_GETTIMEOFDAY_TZ_VOID)

check_prototype_definition(accept
    "int accept(int s, struct sockaddr *addr, Psocklen_t addrlen)"
    "-1"
    "sys/types.h;sys/socket.h"
    HAVE_ACCEPT_PSOCKLEN_T)

check_prototype_definition(ioctl
    "int ioctl(int s, int r, ...)"
    "-1"
    "unistd.h;sys/ioctl.h"
    HAVE_IOCTL_INT)

if (HAVE_EVENTFD)
    check_prototype_definition(eventfd
        "int eventfd(unsigned int count, int flags)"
        "-1"
        "sys/eventfd.h"
        HAVE_EVENTFD_UNSIGNED_INT)
endif (HAVE_EVENTFD)

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

check_c_source_compiles("
void log_fn(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

int main(void) {
    return 0;
}" HAVE_FUNCTION_ATTRIBUTE_FORMAT)

check_c_source_compiles("
void test_address_sanitizer_attribute(void) __attribute__((no_sanitize_address));

void test_address_sanitizer_attribute(void)
{
    return;
}

int main(void) {
    return 0;
}" HAVE_ADDRESS_SANITIZER_ATTRIBUTE)

check_library_exists(dl dlopen "" HAVE_LIBDL)
if (HAVE_LIBDL)
    find_library(DLFCN_LIBRARY dl)
    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${DLFCN_LIBRARY})
endif (HAVE_LIBDL)

if (OSX)
    set(HAVE_APPLE 1)
endif (OSX)

# ENDIAN
if (NOT WIN32)
    test_big_endian(WORDS_BIGENDIAN)
endif (NOT WIN32)

check_type_size(pid_t SIZEOF_PID_T)

set(SWRAP_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} CACHE INTERNAL "swrap required system libraries")
