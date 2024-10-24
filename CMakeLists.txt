
cmake_minimum_required(VERSION 3.10)
project(WebServer C)

# C11 표준 설정
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 헤더 파일 디렉토리
include_directories(${PROJECT_SOURCE_DIR}/include)

# 소스 파일 목록
set(SOURCES
        src/main.c
        src/server.c
        src/config.c
        src/http_parser.c
        src/file_handler.c
        src/connection.c
)

# 실행 파일 생성
add_executable(${PROJECT_NAME} ${SOURCES})

# Windows 환경 설정
if (WIN32)
    target_link_libraries(${PROJECT_NAME} wsock32 ws2_32)
endif ()

# 컴파일 옵션
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
    )
endif ()

# 디버그 모드 설정
set(CMAKE_BUILD_TYPE Debug)

# static 폴더 복사 (빌드할 때마다 갱신)
add_custom_target(copy_static_files ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/static
        ${CMAKE_BINARY_DIR}/static
        COMMENT "Copying static files..."
)

# 빌드 순서 설정
add_dependencies(${PROJECT_NAME} copy_static_files)

# 출력 디렉토리 설정
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)