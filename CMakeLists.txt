cmake_minimum_required(VERSION 3.5)

PROJECT(iftp)

add_definitions(-std=c++11)

include_directories("${PROJECT_SOURCE_DIR}/include")

aux_source_directory(. DIRSRCS)

add_subdirectory(conf)
add_subdirectory(global)
add_subdirectory(inet_util)
add_subdirectory(lock_map)
add_subdirectory(log)
add_subdirectory(strop)
add_subdirectory(worker)

link_directories("${PROJECT_SOURCE_DIR}/lib")

add_executable(iftp ${DIRSRCS})
target_link_libraries(iftp global worker inet_util lock_map conf log strop RIO pthread crypt)
