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
#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifdef _WIN32
#include <stdlib.h>  // for _fullpath
#endif

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
  const char* error_detail; // 에러 상세 내용
} file_result;

typedef struct {
  char *data; // 파일 데이터
  size_t size; // 파일 크기
  char *content_type; // MIME 타입
  time_t last_modified; // 파일 마지막 수정 시간
  time_t cached_time; // 캐시된 시간
  size_t ref_count; // 참조 카운터
} cache_entry;

typedef struct {
  cache_entry **entries; // 캐시 엔트리 배열
  char **paths; // 캐시된 파일 경로 배열
  size_t size; // 현재 캐시된 파일 수
  size_t capacity; // 최대 캐시 크기
} file_cache;

// 파일 읽기
file_result read_file(const char *base_path, const char *request_path);

// 리소스 정리
void free_file_result(file_result *result);

// MIME 타입 감지
const char *get_mime_type(const char *file_path);

// 보안용 경로 검증
int is_path_safe(const char *path);

// 캐시 관련 함수들
void cache_init(size_t capacity);
void cache_cleanup(void);
cache_entry *cache_get(const char *path);
void cache_put(const char *path, const file_result *result);
void cache_remove(const char *path);

#endif // FILE_HANDLER_H
