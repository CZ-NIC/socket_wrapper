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

/* Define to 1 if you want to enable ZLIB */
#cmakedefine WITH_ZLIB 1

/* Define to 1 if you want to enable SFTP */
#cmakedefine WITH_SFTP 1

/* Define to 1 if you want to enable SSH1 */
#cmakedefine WITH_SSH1 1

/* Define to 1 if you want to enable server support */
#cmakedefine WITH_SERVER 1

/* Define to 1 if you want to enable debug output for crypto functions */
#cmakedefine DEBUG_CRYPTO 1

/* Define to 1 if you want to enable pcap output support (experimental) */
#cmakedefine WITH_PCAP 1

/* Define to 1 if you want to enable calltrace debug output */
#cmakedefine DEBUG_CALLTRACE 1

#cmakedefine HAVE_IPV6 1

/*************************** ENDIAN *****************************/

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#cmakedefine WORDS_BIGENDIAN 1
