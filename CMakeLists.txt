cmake_minimum_required(VERSION 3.10)
project(WebServer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 헤더 파일 디렉토리 추가
include_directories(${PROJECT_SOURCE_DIR}/include)

# 소스 파일 목록
file(GLOB_RECURSE SOURCES "src/*.c")

# 실행 파일 생성
add_executable(${PROJECT_NAME} ${SOURCES}
        include/file_handler.h
        src/file_handler.c)

# Windows에서 필요한 라이브러리 추가
if (WIN32)
    target_link_libraries(${PROJECT_NAME} wsock32 ws2_32)
endif ()

# 디버그 모드 설정
set(CMAKE_BUILD_TYPE Debug)
add_compile_options(-Wall -Wextra)

add_custom_target(copy_static_files ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/static
        ${CMAKE_BINARY_DIR}/static
)

add_dependencies(${PROJECT_NAME} copy_static_files)