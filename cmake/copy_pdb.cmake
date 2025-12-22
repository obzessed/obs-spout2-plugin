# copy_pdb.cmake - Conditionally copy PDB files for Debug/RelWithDebInfo builds
# Usage: cmake -DSRC_FILE=<pdb_path> -DDST_DIR=<dest_dir> -DCONFIG=<config> -P copy_pdb.cmake

if(CONFIG STREQUAL "Debug" OR CONFIG STREQUAL "RelWithDebInfo")
	if(EXISTS "${SRC_FILE}")
		get_filename_component(FILENAME "${SRC_FILE}" NAME)
		file(COPY "${SRC_FILE}" DESTINATION "${DST_DIR}")
		message(STATUS "Copied ${FILENAME} to ${DST_DIR}")
	endif()
endif()
