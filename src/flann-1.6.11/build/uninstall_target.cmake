if(NOT EXISTS "/home/shubham/ACG_Localizer_v_1_2_2/src/flann-1.6.11/build/install_manifest.txt")
    message(FATAL_ERROR "Cannot find install manifest: \"/home/shubham/ACG_Localizer_v_1_2_2/src/flann-1.6.11/build/install_manifest.txt\"")
endif(NOT EXISTS "/home/shubham/ACG_Localizer_v_1_2_2/src/flann-1.6.11/build/install_manifest.txt")

file(READ "/home/shubham/ACG_Localizer_v_1_2_2/src/flann-1.6.11/build/install_manifest.txt" files)
string(REGEX REPLACE "\n" ";" files "${files}")
foreach(file ${files})
    message(STATUS "Uninstalling \"$ENV{DESTDIR}${file}\"")
    if(EXISTS "$ENV{DESTDIR}${file}")
        exec_program("/usr/bin/cmake" ARGS "-E remove \"$ENV{DESTDIR}${file}\""
            OUTPUT_VARIABLE rm_out RETURN_VALUE rm_retval)
        if(NOT "${rm_retval}" STREQUAL 0)
            message(FATAL_ERROR "Problem when removing \"$ENV{DESTDIR}${file}\"")
        endif(NOT "${rm_retval}" STREQUAL 0)
    else(EXISTS "$ENV{DESTDIR}${file}")
        message(STATUS "File \"$ENV{DESTDIR}${file}\" does not exist.")
    endif(EXISTS "$ENV{DESTDIR}${file}")
endforeach(file)

