FIND_PATH(FLANN_INCLUDE_DIR flann/flann.hpp 
	  PATHS "/home/shubham/ACG_Localizer/src/flann-1.6.11/src/cpp"
	  NO_DEFAULT_PATH
			)                
                
set( LIBDIR lib64 )
set( FLANN_FOUND TRUE )
set( FLANN_LIBRARY_DIR "/home/shubham/ACG_Localizer/src/flann-1.6.11/build/lib" )

#SET(CMAKE_FIND_LIBRARY_PREFIXES "lib")
#SET(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".a")

find_library(FLANN_LIBRARIES
    NAMES
    flann
    flann_s
    flann_cpp
    HINTS
    ${FLANN_LIBRARY_DIR}
    )

set( FLANN_LIBRARY optimized flann_cpp debug flann_cpp )


