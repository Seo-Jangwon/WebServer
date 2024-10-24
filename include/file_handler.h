#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <stddef.h>

// MIME 타입
typedef struct {
  const char *extension;
  const char *mime_type;
} mime_mapping;

// 파일 읽기 결과
typedef struct {
  char *data; // 파일 데이터
  size_t size; // 파일 크기
  char *content_type; // MIME 타입
  int status_code; // HTTP 상태 코드
} file_result;

file_result read_file(const char *base_path, const char *request_path);
void free_file_result(file_result *result);
const char *get_mime_type(const char *file_path);

#endif // FILE_HANDLER_H
