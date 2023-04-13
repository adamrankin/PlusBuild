###############################################################################
# Find DAQ USB3-FRM13-B SDK
#
#     find_package(DAQUSB3FRM13BSDK)
#
# Variables defined by this module:
#
#  DAQVIDEOSOURCESDK_FOUND                 True if DAQ USB3-FRM13-B SDK found
#  DAQVIDEOSOURCE_SDK_VERSION              The version of DAQ USB3-FRM13-B SDK
#  DAQVIDEOSOURCE_SDK_INCLUDE_DIR          The location(s) of DAQ USB3-FRM13-B SDK headers
#  DAQVIDEOSOURCE_SDK_LIBRARY_DIR          Libraries needed to use DAQ USB3-FRM13-B SDK
#  DAQVIDEOSOURCE_SDK_BINARY_DIR           Binaries needed to use DAQ USB3-FRM13-B SDK

SET(DAQVIDEOSOURCE_SDK_PATH_HINTS
  "../ThirdParty/DAQVIDEOSOURCE"
  "../Plus-bin/DAQVIDEOSOURCE"
  
  )

find_path(DAQVIDEOSOURCE_SDK_DIR include/usb3_frm13_import.h
  PATHS ${DAQVIDEOSOURCE_SDK_PATH_HINTS}
  DOC "DAQVideoSource SDK directory")

if (DAQVIDEOSOURCE_SDK_DIR)
  # Include directories
  set(DAQVIDEOSOURCE_SDK_INCLUDE_DIR ${DAQVIDEOSOURCE_SDK_DIR}/include)
  mark_as_advanced(DAQVIDEOSOURCE_SDK_INCLUDE_DIR)

  # Libraries
  SET(PLATFORM_SUFFIX "")

  IF (CMAKE_HOST_WIN32 AND CMAKE_CL_64 )
    SET(PLATFORM_SUFFIX "x64")
    SET(SDK_FILENAME "USB3-FRM13_x64.dll")
	SET(LIB_FILENAME "USB3-FRM13_x64.lib")
  ENDIF (CMAKE_HOST_WIN32 AND CMAKE_CL_64 )

  IF (CMAKE_HOST_WIN32 AND NOT CMAKE_CL_64 )
    SET(PLATFORM_SUFFIX "x86")
	SET(SDK_FILENAME "USB3-FRM13.dll")
	SET(LIB_FILENAME "USB3-FRM13.lib")
  ENDIF (CMAKE_HOST_WIN32 AND NOT CMAKE_CL_64 )

  find_library(DAQVIDEOSOURCE_SDK_LIBRARY
            NAMES ${LIB_FILENAME}
            PATHS "${DAQVIDEOSOURCE_SDK_DIR}/bin/${PLATFORM_SUFFIX}/" NO_DEFAULT_PATH
            PATH_SUFFIXES ${PLATFORM_SUFFIX})

  find_path(DAQVIDEOSOURCE_SDK_BINARY
            NAMES ${SDK_FILENAME}
            PATHS "${DAQVIDEOSOURCE_SDK_DIR}/bin/${PLATFORM_SUFFIX}/" NO_DEFAULT_PATH
            PATH_SUFFIXES ${PLATFORM_SUFFIX})

 
  mark_as_advanced(DAQVIDEOSOURCE_SDK_LIBRARY)
  mark_as_advanced(DAQVIDEOSOURCE_SDK_BINARY)

  set(DAQVIDEOSOURCE_BIN_FILE ${DAQVIDEOSOURCE_SDK_BINARY} "/" ${SDK_FILENAME})

  set(DAQVIDEOSOURCE_SDK_VERSION "1.0")

endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DAQVIDEOSOURCESDK
  FOUND_VAR DAQVIDEOSOURCESDK_FOUND
  REQUIRED_VARS DAQVIDEOSOURCE_SDK_DIR DAQVIDEOSOURCE_SDK_BINARY DAQVIDEOSOURCE_SDK_LIBRARY DAQVIDEOSOURCE_SDK_INCLUDE_DIR
  VERSION_VAR DAQVIDEOSOURCE_SDK_VERSION
)
