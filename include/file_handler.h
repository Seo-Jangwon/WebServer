/*
 * 정적 파일 처리
 * 1. 정적 파일 읽기 및 전송
 * 2. MIME 타입 감지
 * 3. 파일 경로 정규화
 * 4. 기본 보안 검사
 */

#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <stddef.h>

// MIME 타입 매핑
typedef struct {
  const char *extension;
  const char *mime_type;
} mime_mapping;

// 파일 처리 결과
typedef struct {
  char *data; // 파일 데이터
  size_t size; // 파일 크기
  char *content_type; // MIME 타입
  int status_code; // HTTP 상태 코드
} file_result;

// 파일 읽기
file_result read_file(const char *base_path, const char *request_path);

// 리소스 정리
void free_file_result(file_result *result);

// MIME 타입 감지
const char *get_mime_type(const char *file_path);

// 보안용 경로 검증
int is_path_safe(const char *path);

#endif // FILE_HANDLER_H
