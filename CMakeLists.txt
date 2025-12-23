cmake_minimum_required(VERSION 3.28...3.30)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

if (NOT WIN32)
	return()
endif()

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)
option(SPOUT_USE_SHARED "Link against shared Spout libraries instead of static" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

include(cpm)

set(FETCHCONTENT_QUIET FALSE CACHE BOOL "Disable quiet mode for FetchContent" FORCE)

CPMAddPackage(
	URI "gh:leadedge/Spout2#2.007.017"
	OPTIONS "SPOUT_BUILD_LIBRARY ON" "SPOUT_BUILD_SPOUTDX ON"
	GIT_SHALLOW ON
	GIT_PROGRESS TRUE
)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

file(GLOB_RECURSE PLUGIN_SOURCES
	src/*.c
	src/*.cc
	src/*.cpp
)
# Exclude test executable from plugin sources
list(FILTER PLUGIN_SOURCES EXCLUDE REGEX "test-.*\\.cpp$")
list(FILTER PLUGIN_SOURCES EXCLUDE REGEX "test-.*\\.cc$")
list(FILTER PLUGIN_SOURCES EXCLUDE REGEX "test-.*\\.c$")

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${PLUGIN_SOURCES})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
	resources.qrc
)

set(INSTALLED_PLUGIN_BIN_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_PROJECT_NAME}/${CMAKE_INSTALL_BINDIR}/64bit")

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Spout2 includes from CPM package
target_include_directories(
	${CMAKE_PROJECT_NAME}
	PRIVATE
		"${Spout2_SOURCE_DIR}/SPOUTSDK/SpoutLibrary"
		"${Spout2_SOURCE_DIR}/SPOUTSDK/SpoutDirectX/SpoutDX"
)

# Link against Spout2 libraries from CPM package
if(SPOUT_USE_SHARED)
	target_link_libraries(
		${CMAKE_PROJECT_NAME}
		PRIVATE
			Spout
			SpoutLibrary
			SpoutDX
			OBS::w32-pthreads
	)

	# Copy Spout DLLs to plugin bin dir after build
	add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_FILE:Spout>
			"${INSTALLED_PLUGIN_BIN_DIR}/$<TARGET_FILE_NAME:Spout>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_FILE:SpoutDX>
			"${INSTALLED_PLUGIN_BIN_DIR}/$<TARGET_FILE_NAME:SpoutDX>"
	)

	# Copy Spout PDBs to plugin bin dir after build (Debug/RelWithDebInfo only)
	add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND}
			"-DSRC_FILE=$<TARGET_PDB_FILE:Spout>"
			"-DDST_DIR=${INSTALLED_PLUGIN_BIN_DIR}"
			"-DCONFIG=$<CONFIG>"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_pdb.cmake"
		COMMAND ${CMAKE_COMMAND}
			"-DSRC_FILE=$<TARGET_PDB_FILE:SpoutDX>"
			"-DDST_DIR=${INSTALLED_PLUGIN_BIN_DIR}"
			"-DCONFIG=$<CONFIG>"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_pdb.cmake"
	)

	# Install Spout DLLs alongside the plugin
	install(
		FILES
			$<TARGET_FILE:Spout>
			$<TARGET_FILE:SpoutDX>
		DESTINATION "${INSTALLED_PLUGIN_BIN_DIR}"
	)

	# Install Spout PDBs for Debug/RelWithDebInfo builds
	install(
		FILES
			$<TARGET_PDB_FILE:Spout>
			$<TARGET_PDB_FILE:SpoutDX>
		DESTINATION "${INSTALLED_PLUGIN_BIN_DIR}"
		CONFIGURATIONS Debug RelWithDebInfo
		OPTIONAL
	)
else()
	target_link_libraries(
		${CMAKE_PROJECT_NAME}
		PRIVATE
			Spout_static
			SpoutLibrary_static
			SpoutDX_static
			OBS::w32-pthreads
	)
endif()

# ============================================================================
# Deploy to OBS plugins directory (based on OBS_PLUGINS_DIR env var)
# ============================================================================
if(DEFINED ENV{OBS_PLUGINS_DIR})
	set(OBS_PLUGINS_DIR $ENV{OBS_PLUGINS_DIR})
	message(STATUS "OBS_PLUGINS_DIR: ${OBS_PLUGINS_DIR}")

	add_custom_command(
		TARGET ${CMAKE_PROJECT_NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${OBS_PLUGINS_DIR}"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>" "${OBS_PLUGINS_DIR}"
		COMMENT "Deploying plugin to OBS plugins directory: ${OBS_PLUGINS_DIR}"
	)

	# Copy plugin PDB if it exists
	add_custom_command(
		TARGET ${CMAKE_PROJECT_NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>" "${OBS_PLUGINS_DIR}"
		COMMENT "Deploying plugin PDB to OBS plugins directory"
	)
else()
	message(STATUS "OBS_PLUGINS_DIR not set - skipping deployment to OBS")
endif()