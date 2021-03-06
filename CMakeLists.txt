cmake_minimum_required(VERSION 2.6)

project(aseba)
include(ParseVersion.cmake)

add_definitions(-Wall)

# Dashel
# TODO: make Dashel.cmake to perform this

find_path(DASHEL_INCLUDE_DIR dashel.h
	/usr/include/dashel
	/usr/local/include/dashel
)

find_library(DASHEL_LIBRARY dashel
	/usr/lib
	/usr/local/lib
)

include_directories(${DASHEL_INCLUDE_DIR})
set(EXTRA_LIBS ${DASHEL_LIBRARY})

# libudev
find_path(UDEV_INCLUDE_DIR libudev.h)
find_library(UDEV_LIBS udev)

if (UDEV_INCLUDE_DIR AND UDEV_LIBS)
	message(STATUS "libudev found")
	set(UDEV_FOUND 1)
endif (UDEV_INCLUDE_DIR AND UDEV_LIBS)

if (WIN32)
	set(EXTRA_LIBS ${EXTRA_LIBS} winspool ws2_32 setupapi)
else (WIN32)
	if (APPLE)
		find_library(COREFOUNDATION_LIBRARY CoreFoundation)
		find_library(IOKIT_LIBRARY IOKit)
		mark_as_advanced(COREFOUNDATION_LIBRARY)
		mark_as_advanced(IOKIT_LIBRARY)
		set(EXTRA_LIBS ${EXTRA_LIBS} ${COREFOUNDATION_LIBRARY} ${IOKIT_LIBRARY} ${EXTRA_LIBS})
	else (APPLE)
		# Note that because of static initializers, this cannot be linked as a library,
		# hence all aseba tools must link to this file directly
		set (SOCKETCAN_PLUGIN ../transport/dashel_plugin/socketcan.cpp)
		if (UDEV_FOUND)
			include_directories(${UDEV_INCLUDE_DIR})
			set(EXTRA_LIBS ${EXTRA_LIBS} ${UDEV_LIBS} ${EXTRA_LIBS})
		else (UDEV_FOUND)
			find_package(PkgConfig)
			pkg_check_modules(HAL hal)
			if (${HAL_FOUND})
				include_directories(${HAL_INCLUDE_DIRS})
				set(EXTRA_LIBS ${EXTRA_LIBS} ${HAL_LIBRARIES} ${EXTRA_LIBS})
			endif (${HAL_FOUND})
		endif (UDEV_FOUND)
	endif (APPLE)
endif (WIN32)

# Enki

find_path(ENKI_INCLUDE_DIR enki/PhysicalEngine.h
	/usr/include/enki
	/usr/local/include/enki
)

find_library(ENKI_LIBRARY enki
	/usr/lib
	/usr/local/lib
)

find_library(ENKI_VIEWER_LIBRARY enkiviewer
	/usr/lib
	/usr/local/lib
)


if (ENKI_INCLUDE_DIR AND ENKI_LIBRARY AND ENKI_VIEWER_LIBRARY)
	set(ENKI_FOUND 1)
endif (ENKI_INCLUDE_DIR AND ENKI_LIBRARY AND ENKI_VIEWER_LIBRARY)

# Aseba itself

add_definitions(-DASEBA_ASSERT)

set (ASEBACORE_SRC
	utils/FormatableString.cpp
	utils/utils.cpp
	utils/HexFile.cpp
	msg/msg.cpp
	msg/descriptions-manager.cpp
)

set (ASEBACOMPILER_SRC
	compiler/compiler.cpp
	compiler/errors.cpp
	compiler/identifier-lookup.cpp
	compiler/lexer.cpp
	compiler/parser.cpp
	compiler/analysis.cpp
	compiler/tree-build.cpp
	compiler/tree-expand.cpp
	compiler/tree-dump.cpp
	compiler/tree-typecheck.cpp
	compiler/tree-optimize.cpp
	compiler/tree-emit.cpp
)

set (ASEBAVM_SRC
	vm/vm.c
	vm/natives.c
)

set (ASEBAVMBUFFER_SRC
	transport/buffer/vm-buffer.c
)

if (SHARED_LIBS)
	add_library(asebacore SHARED ${ASEBACORE_SRC})
	add_library(asebacompiler SHARED ${ASEBACOMPILER_SRC})
	add_library(asebavm SHARED ${ASEBAVM_SRC})
	add_library(asebavmbuffer SHARED ${ASEBAVMBUFFER_SRC})

	install(TARGETS asebacore LIBRARY DESTINATION lib)
	install(TARGETS asebacompiler LIBRARY DESTINATION lib)
	install(TARGETS asebavm LIBRARY DESTINATION lib)
	install(TARGETS asebavmbuffer LIBRARY DESTINATION lib)
else(SHARED_LIBS)
	add_library(asebacore ${ASEBACORE_SRC})
	add_library(asebacompiler ${ASEBACOMPILER_SRC})
	add_library(asebavm ${ASEBAVM_SRC})
	add_library(asebavmbuffer ${ASEBAVMBUFFER_SRC})
	
	install(TARGETS asebacore ARCHIVE DESTINATION lib)
	install(TARGETS asebacompiler ARCHIVE DESTINATION lib)
	install(TARGETS asebavm ARCHIVE DESTINATION lib)
	install(TARGETS asebavmbuffer ARCHIVE DESTINATION lib)
endif(SHARED_LIBS)

set (ASEBACORE_HDR_UTILS 
	utils/utils.h
	utils/FormatableString.h
)
set (ASEBACORE_HDR_MSG
	msg/msg.h
	msg/descriptions-manager.h
)
set (ASEBACORE_HDR_COMMON
	common/consts.h
	common/types.h
)
set (ASEBACORE_HDR_COMPILER
	compiler/compiler.h
	compiler/errors_code.h
)
	
install(FILES ${ASEBACORE_HDR_UTILS}
	DESTINATION include/aseba/utils
)
install(FILES ${ASEBACORE_HDR_MSG}
	DESTINATION include/aseba/msg
)
install(FILES ${ASEBACORE_HDR_COMMON}
	DESTINATION include/aseba/common
)
install(FILES ${ASEBACORE_HDR_COMPILER}
	DESTINATION include/aseba/compiler
)

add_subdirectory(switch)

add_subdirectory(medulla)

add_subdirectory(cmd)

add_subdirectory(dump)

add_subdirectory(replay)

add_subdirectory(exec)

add_subdirectory(targets/dummy)

enable_testing()
add_subdirectory(tests)

if(NOT CMD_LINE)
	add_subdirectory(studio)

	add_subdirectory(targets/enki-marxbot)

	add_subdirectory(targets/challenge)

	add_subdirectory(targets/playground)

	add_subdirectory(eventlogger)
endif(NOT CMD_LINE)

