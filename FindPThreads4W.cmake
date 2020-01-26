# - Find PThreads4W
# Find the native pthread for windows includes and library
#
#   PTHREADS4W_FOUND        - True if pthread for windows found.
#   PTHREADS4W_INCLUDE_DIRS - where to find pthread.h, etc.
#   PTHREADS4W_LIBRARIES    - List of libraries when using pthread for windows.
#

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_PTHREADS4W pthreads4w QUIET)
endif()

find_path(PTHREADS4W_INCLUDE_DIRS NAMES pthread.h
                                  PATHS ${PC_PTHREADS4W_INCLUDEDIR})
find_library(PTHREADS4W_LIBRARIES NAMES pthreads4w
                                  PATHS ${PC_PTHREADS4W_LIBDIR})

include("FindPackageHandleStandardArgs")
find_package_handle_standard_args(PThreads4W REQUIRED_VARS PTHREADS4W_INCLUDE_DIRS PTHREADS4W_LIBRARIES)

mark_as_advanced(PTHREADS4W_INCLUDE_DIRS PTHREADS4W_LIBRARIES)
