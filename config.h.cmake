/* Name of package */
#cmakedefine PACKAGE "${APPLICATION_NAME}"

/* Version number of package */
#cmakedefine VERSION "${APPLICATION_VERSION}"

#cmakedefine LOCALEDIR "${LOCALE_INSTALL_DIR}"
#cmakedefine DATADIR "${DATADIR}"
#cmakedefine LIBDIR "${LIBDIR}"
#cmakedefine PLUGINDIR "${PLUGINDIR}"
#cmakedefine SYSCONFDIR "${SYSCONFDIR}"
#cmakedefine BINARYDIR "${BINARYDIR}"
#cmakedefine SOURCEDIR "${SOURCEDIR}"

/************************** HEADER FILES *************************/

#cmakedefine HAVE_SYS_FILIO_H 1
#cmakedefine HAVE_SYS_SIGNALFD_H 1
#cmakedefine HAVE_SYS_EVENTFD_H 1
#cmakedefine HAVE_SYS_TIMERFD_H 1
#cmakedefine HAVE_GNU_LIB_NAMES_H 1

/**************************** STRUCTS ****************************/

#cmakedefine HAVE_STRUCT_IN6_PKTINFO 1

/************************ STRUCT MEMBERS *************************/

#cmakedefine HAVE_STRUCT_SOCKADDR_SA_LEN 1
#cmakedefine HAVE_STRUCT_MSGHDR_MSG_CONTROL 1

/*************************** FUNCTIONS ***************************/

/* Define to 1 if you have the `getaddrinfo' function. */
#cmakedefine HAVE_GETADDRINFO 1
#cmakedefine HAVE_SIGNALFD 1
#cmakedefine HAVE_EVENTFD 1
#cmakedefine HAVE_TIMERFD_CREATE 1

#cmakedefine HAVE_ACCEPT_PSOCKLEN_T 1
#cmakedefine HAVE_IOCTL_INT 1

/*************************** LIBRARIES ***************************/

#cmakedefine HAVE_GETTIMEOFDAY_TZ 1
#cmakedefine HAVE_GETTIMEOFDAY_TZ_VOID 1

/*************************** DATA TYPES***************************/

#cmakedefine SIZEOF_PID_T @SIZEOF_PID_T@

/**************************** OPTIONS ****************************/

#cmakedefine HAVE_GCC_THREAD_LOCAL_STORAGE 1
#cmakedefine HAVE_DESTRUCTOR_ATTRIBUTE 1
#cmakedefine HAVE_SOCKADDR_STORAGE 1
#cmakedefine HAVE_IPV6 1
#cmakedefine HAVE_FUNCTION_ATTRIBUTE_FORMAT 1

#cmakedefine HAVE_APPLE 1
#cmakedefine HAVE_LIBSOCKET 1

/*************************** ENDIAN *****************************/

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#cmakedefine WORDS_BIGENDIAN 1
