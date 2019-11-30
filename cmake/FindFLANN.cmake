FIND_PATH(FLANN_INCLUDE_DIR flann/flann.hpp 
	  PATHS "/home/shubham/ACG_Localizer_v_1_2_2/src/flann-1.6.11/src/cpp"
	  NO_DEFAULT_PATH
			)                
                
set( LIBDIR lib64 )
set( FLANN_FOUND TRUE )
set( FLANN_LIBRARY_DIR "/home/shubham/ACG_Localizer_v_1_2_2/src/flann-1.6.11/build/lib" )
set( FLANN_LIBRARY optimized flann_cpp debug flann_cpp )


