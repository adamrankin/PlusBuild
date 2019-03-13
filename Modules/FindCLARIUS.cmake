# This module can be loaded by
#   FIND_PACKAGE(CLARIUS)
# This module defines the following variables
# CLARIUS_DIR
# CLARIUS_INCLUDE_DIR
# CLARIUS_LIB_DIR
# CLARIUS_FOUND

SET(CLARIUS_PATH_HINTS
  "C:/Users/$ENV{USERNAME}/Documents/clarius_listen_plugin/clarius_listen_sdk"
  "C:/Program Files/clarius_listen_plugin/clarius_listen_sdk"
  )

FIND_PATH(CLARIUS_DIR include/listen/listen.h
  PATHS ${CLARIUS_PATH_HINTS}
  DOC "Clarius API dir")

IF(CLARIUS_DIR)
  SET(CLARIUS_INCLUDE_DIR ${CLARIUS_DIR}/include/listen)
  SET(CLARIUS_LIB_DIR ${CLARIUS_DIR}/lib)
  SET(CLARIUS_FOUND 1)
ENDIF()

IF(NOT CLARIUS_INCLUDE_DIR)
  MESSAGE(FATAL_ERROR "Clarius header files not found")
ENDIF()

IF(NOT CLARIUS_LIB_DIR)
  MESSAGE(FATAL_ERROR "Clarius lib files not found")
ENDIF()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
  CLARIUS
  FOUND_VAR CLARIUS_FOUND
  REQUIRED_VARS CLARIUS_INCLUDE_DIR CLARIUS_LIB_DIR CLARIUS_DIR
)

IF(CLARIUS_FOUND)
  MESSAGE("Clarius files found")
  ADD_LIBRARY(Clarius STATIC IMPORTED)
  SET_TARGET_PROPERTIES(Clarius PROPERTIES
  IMPORTED_IMPLIB ${CLARIUS_LIB_DIR}
  IMPORTED_LOCATION ${CLARIUS_LIB_DIR}/listen${CMAKE_SHARED_LIBRARY_SUFFIX}
  INTERFACE_INCLUDE_DIRECTORIES ${CLARIUS_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(CLARIUS_INCLUDE_DIR CLARIUS_LIB_DIR CLARIUS_DIR)