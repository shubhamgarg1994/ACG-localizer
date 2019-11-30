# - Try to find LAPACK
# Once done this will define
#  
# LAPACK_FOUND           - system has LAPACK
# LAPACK_INCLUDE_DIR - theLAPACK include directory
# LAPACK_LIBRARY         - Link these to use LAPACK
# LAPACK_LIBRARY_DIR  - Library DIR of LAPACK
#   

IF (LAPACK_LIBRARIES)
  # Already in cache, be silent
  SET(LAPACK_FIND_QUIETLY TRUE) 
  message("not found")
ENDIF (LAPACK_LIBRARIES)


  FIND_LIBRARY(LAPACK_LIB
    NAMES lapack 
    PATHS /usr/lib /usr/local/lib)
  
  FIND_LIBRARY(BLAS_LIB 
    NAMES blas 
    PATHS /usr/lib /usr/local/lib)
  
  FIND_LIBRARY(F2C_LIB
    NAMES f2c 
    PATHS /usr/lib /usr/local/lib)

 GET_FILENAME_COMPONENT( LAPACK_LIBRARY_PATH ${LAPACK_LIB} PATH )

  if( LAPACK_LIB AND BLAS_LIB AND F2C_LIB )

    SET( LAPACK_FOUND TRUE )
      
    SET( LAPACK_LIBRARIES
      ${LAPACK_LIB}
      ${BLAS_LIB}
      ${F2C_LIB_opt} )
        
  else()
    
    SET(LAPACK_FOUND FALSE )
    MESSAGE(STATUS "Lapack not found")
    
  endif()





