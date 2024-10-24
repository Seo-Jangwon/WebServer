#include "file_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

// MIME 타입 매핑
static const mime_mapping MIME_TYPES[] = {
  {".html", "text/html"},
  {".htm", "text/html"},
  {".css", "text/css"},
  {".js", "application/javascript"},
  {".json", "application/json"},
  {".txt", "text/plain"},
  {".jpg", "image/jpeg"},
  {".jpeg", "image/jpeg"},
  {".png", "image/png"},
  {".gif", "image/gif"},
  {".svg", "image/svg+xml"},
  {".ico", "image/x-icon"},
  {".pdf", "application/pdf"},
  {".xml", "application/xml"},
  {".zip", "application/zip"},
  {NULL, "application/octet-stream"} // Default
};

// 파일 확장자 추출
static const char *get_file_extension(const char *file_path) {
  const char *dot = strrchr(file_path, '.');
  if (!dot || dot == file_path) {
    return "";
  }
  return dot;
}

// MIME 타입 얻기
const char *get_mime_type(const char *file_path) {
  const char *extension = get_file_extension(file_path);
  printf("Extracted extension: %s\n", extension);

  // 확장자가 없는 경우
  if (!extension || !*extension) {
    printf("No extension found\n");
    return "application/octet-stream";
  }

  // 확장자를 소문자로
  char ext_lower[32];
  strncpy(ext_lower, extension, sizeof(ext_lower) - 1);
  ext_lower[sizeof(ext_lower) - 1] = '\0';

  for (char *p = ext_lower; *p; p++) {
    *p = tolower(*p);
  }
  printf("Normalized extension: %s\n", ext_lower);

  // MIME 타입 검색
  for (const mime_mapping *mime = MIME_TYPES; mime->extension != NULL; mime++) {
    printf("Comparing with: %s -> %s\n", mime->extension, mime->mime_type);
    if (strcmp(ext_lower, mime->extension) == 0) {
      printf("Found MIME type: %s\n", mime->mime_type);
      return mime->mime_type;
    }
  }

  printf("No matching MIME type found, using default\n");
  return "application/octet-stream";
}

// 경로 정규화
static void normalize_path(char *path) {
  char *src = path;
  char *dst = path;

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
file_result read_file(const char *base_path, const char *request_path) {
  file_result result = {NULL, 0, NULL, 404};

  printf("\n=== File Read Operation ===\n");
  printf("Base path: %s\n", base_path);
  printf("Request path: %s\n", request_path);

  // 경로 처리
  const char *cleaned_path = request_path;
  while (*cleaned_path == '/') {
    cleaned_path++;
  }

  // index.html 처리
  if (strlen(cleaned_path) == 0) {
    cleaned_path = "index.html";
  }

  // 전체 경로 생성
  char full_path[1024];
  snprintf(full_path,
           sizeof(full_path),
           "%s%c%s",
           base_path,
           PATH_SEPARATOR,
           cleaned_path);
  printf("Full path: %s\n", full_path);

  // 경로 정규화
  char normalized_path[1024];
  strncpy(normalized_path, full_path, sizeof(normalized_path) - 1);
  normalize_path(normalized_path);

  printf("Attempting to read: %s\n", normalized_path);

  // MIME 타입 설정
  const char *mime_type = get_mime_type(request_path);
  printf("MIME type: %s\n", mime_type);
  result.content_type = strdup(mime_type);

  // 파일 존재 확인
  struct stat file_stat;
  if (stat(normalized_path, &file_stat) != 0) {
    printf("File not found: %s (errno: %d)\n", normalized_path, errno);
    return result;
  }
  printf("File found. Size: %lld bytes\n", (long long) file_stat.st_size);

  // 파일 열기
  FILE *file = fopen(normalized_path, "rb");
  if (!file) {
    printf("Failed to open file: %s (errno: %d)\n", normalized_path, errno);
    return result;
  }

  // 파일 크기 설정
  result.size = file_stat.st_size;

  // 메모리 할당
  result.data = (char *) malloc(result.size);
  if (!result.data) {
    printf("Memory allocation failed for size: %zu\n", result.size);
    fclose(file);
    result.status_code = 500;
    return result;
  }

  // 파일 읽기
  size_t bytes_read = fread(result.data, 1, result.size, file);
  fclose(file);

  if (bytes_read != result.size) {
    printf("Read error. Expected: %zu, Got: %zu\n", result.size, bytes_read);
    free(result.data);
    result.data = NULL;
    result.status_code = 500;
    return result;
  }

  printf("File successfully read\n");
  result.status_code = 200;
  return result;
}

// 메모리 해제
void free_file_result(file_result *result) {
  if (result) {
    free(result->data);
    free(result->content_type);
    result->data = NULL;
    result->content_type = NULL;
    result->size = 0;
  }
}
