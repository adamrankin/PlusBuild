# Find the StealthLink2.0 Libraries
# This module defines
# STEALTHLINK_INCLUDE_DIRS - STEALTHLINK header files 
# STEALTHLINK_STEALTHLINK_STATIC_LIBRARY  - STEALTHLINK static library
# STEALTHLINK_STEALTHLINKD_STATIC_LIBRARY - STEALTHLINK static library with debug info
# STEALTHLINK_STEALTHLINK_SHARED_LIBRARY  - STEALTHLINK shared library
# STEALTHLINK_STEALTHLINKD_SHARED_LIBRARY - STEALTHLINK shared library with debug info
#

SET( STEALTHLINK_PATH_HINTS 
    ../StealthLink-2.0.1
    ../PLTools/StealthLink-2.0.1
    ../../PLTools/StealthLink-2.0.1
    ../trunk/PLTools/StealthLink-2.0.1
    ${CMAKE_CURRENT_BINARY_DIR}/StealthLink-2.0.1
    )
IF(NOT ${CMAKE_GENERATOR} MATCHES "Visual Studio 10" )
  MESSAGE(FATAL_ERROR "error: StealthLink can only be built using Visual Studio 2010")
ENDIF(NOT ${CMAKE_GENERATOR} MATCHES "Visual Studio 10" )

SET (PLATFORM_SUFFIX "Win32")
IF (CMAKE_HOST_WIN32 AND CMAKE_CL_64)
  SET( PLATFORM_SUFFIX "x64")
ENDIF (CMAKE_HOST_WIN32 AND CMAKE_CL_64)

find_path (STEALTHLINK_INCLUDE_DIRS
           NAMES "StealthLink/Stealthlink.h"
           PATHS ${STEALTHLINK_PATH_HINTS} 
           DOC "Include directory, i.e. parent directory of directory \"StealthLink\"")

FIND_LIBRARY (STEALTHLINK_STEALTHLINK_STATIC_LIBRARY
              NAMES StealthLink
              PATH_SUFFIXES /windows/${PLATFORM_SUFFIX}/Release 
              PATHS ${STEALTHLINK_PATH_HINTS}
              )
FIND_LIBRARY (STEALTHLINK_STEALTHLINKD_STATIC_LIBRARY
              NAMES StealthLink
              PATH_SUFFIXES /windows/${PLATFORM_SUFFIX}/Debug 
              PATHS ${STEALTHLINK_PATH_HINTS}
              )
FIND_FILE (STEALTHLINK_STEALTHLINK_SHARED_LIBRARY
           NAMES StealthLink${CMAKE_SHARED_LIBRARY_SUFFIX}
           PATH_SUFFIXES /windows/${PLATFORM_SUFFIX}/Release 
           PATHS ${STEALTHLINK_PATH_HINTS}
           )
FIND_FILE (STEALTHLINK_STEALTHLINKD_SHARED_LIBRARY
           NAMES StealthLink${CMAKE_SHARED_LIBRARY_SUFFIX}
           PATH_SUFFIXES /windows/${PLATFORM_SUFFIX}/Debug
           PATHS ${STEALTHLINK_PATH_HINTS}
           )                  

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(STEALTHLINK DEFAULT_MSG  
  STEALTHLINK_INCLUDE_DIRS  
  STEALTHLINK_STEALTHLINK_STATIC_LIBRARY
  STEALTHLINK_STEALTHLINK_SHARED_LIBRARY
  STEALTHLINK_STEALTHLINKD_STATIC_LIBRARY
  STEALTHLINK_STEALTHLINKD_SHARED_LIBRARY
  )
