cmake_policy(SET CMP0015 NEW)
# this policy was introduced in cmake 3.0
# remove if, once 3.0 will be used on unix
if (${CMAKE_MAJOR_VERSION} GREATER 2)
	# old policy do not use MACOSX_RPATH
	cmake_policy(SET CMP0042 OLD)
endif()

set(CMAKE_AUTOMOC OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSTATICLIB")

aux_source_directory(. SRC_LIST)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(..)

set(EXECUTABLE devcore)

file(GLOB HEADERS "*.h")

if(ETH_STATIC)
	add_library(${EXECUTABLE} STATIC ${SRC_LIST} ${HEADERS})
else()
	add_library(${EXECUTABLE} SHARED ${SRC_LIST} ${HEADERS})
endif()

target_link_libraries(${EXECUTABLE} ${Boost_THREAD_LIBRARIES})
target_link_libraries(${EXECUTABLE} ${Boost_DATE_TIME_LIBRARIES})
target_link_libraries(${EXECUTABLE} ${Boost_SYSTEM_LIBRARIES})
target_link_libraries(${EXECUTABLE} ${Boost_CHRONO_LIBRARIES})

if (APPLE)
	find_package(Threads REQUIRED)
	target_link_libraries(${EXECUTABLE} ${CMAKE_THREAD_LIBS_INIT})
elseif (UNIX)
	find_package(Threads REQUIRED)
	target_link_libraries(${EXECUTABLE} ${CMAKE_THREAD_LIBS_INIT})
endif()

install( TARGETS ${EXECUTABLE} ARCHIVE DESTINATION lib LIBRARY DESTINATION lib )
install( FILES ${HEADERS} DESTINATION include/${EXECUTABLE} )

