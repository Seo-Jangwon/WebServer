#include "file_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

// 경로 정규화
static void normalize_path(char* path) {
    char* src = path;
    char* dst = path;
    
    // 연속된 슬래시 제거
    while (*src) {
        if (*src == '/' || *src == '\\') {
            *dst++ = PATH_SEPARATOR;
            while (*src == '/' || *src == '\\') src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

// 파일 읽기
file_result read_file(const char* base_path, const char* request_path) {
    file_result result = {NULL, 0, NULL, 404}; // 기본값은 404
    
    // 전체 경로 생성
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s%c%s", 
             base_path, PATH_SEPARATOR, request_path);
    
    // 경로 정규화
    normalize_path(full_path);
    
    // 파일 정보 추출
    struct stat file_stat;
    if (stat(full_path, &file_stat) != 0) {
        return result; // 404 Not Found
    }
    
    // 파일 크기
    result.size = file_stat.st_size;
    
    // 파일 열기
    FILE* file = fopen(full_path, "rb");
    if (!file) {
        return result; // 404 Not Found
    }
    
    // 파일 데이터를 위한 메모리 할당
    result.data = (char*)malloc(result.size);
    if (!result.data) {
        fclose(file);
        result.status_code = 500;
        return result;
    }
    
    // 파일 읽기
    size_t bytes_read = fread(result.data, 1, result.size, file);
    fclose(file);
    
    if (bytes_read != result.size) {
        free(result.data);
        result.data = NULL;
        result.status_code = 500;
        return result;
    }
    
    result.status_code = 200;
    return result;
}

// 메모리 해제
void free_file_result(file_result* result) {
    if (result) {
        free(result->data);
        free(result->content_type);
        result->data = NULL;
        result->content_type = NULL;
        result->size = 0;
    }
}