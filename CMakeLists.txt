add_library(Dryad "${CMAKE_CURRENT_SOURCE_DIR}/dyad/src/dyad.c")
target_include_directories(Dryad PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/dyad/src"
	"${CMAKE_CURRENT_SOURCE_DIR}"
)
if(WIN32)
	target_link_libraries(Dryad INTERFACE ws2_32)
endif()
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
	find_package(Threads)
	target_link_libraries(Dryad INTERFACE ${CMAKE_THREAD_LIBS_INIT})
endif()
