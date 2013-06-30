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


/*************************** FUNCTIONS ***************************/

/* Define to 1 if you have the `getaddrinfo' function. */
#cmakedefine HAVE_GETADDRINFO 1

/*************************** LIBRARIES ***************************/

#cmakedefine HAVE_GETTIMEOFDAY_TZ 1

/**************************** OPTIONS ****************************/

#cmakedefine HAVE_DESTRUCTOR_ATTRIBUTE 1
#cmakedefine HAVE_SOCKADDR_STORAGE 1
#cmakedefine HAVE_IPV6 1

/*************************** ENDIAN *****************************/

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#cmakedefine WORDS_BIGENDIAN 1
