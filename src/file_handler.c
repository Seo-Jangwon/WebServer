/*
* 파일 처리기
 * 주요 구현사항:
 * 1. 파일 시스템 접근 및 읽기
 * 2. 보안을 위한 경로 정규화
 * 3. MIME 타입 매핑
 * 4. 메모리 관리
 */
#include "file_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

static file_cache *cache = NULL;
static const int CACHE_TTL = 300; // 5분 캐시 유효시간

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

// 보안 검사
int is_path_safe(const char *path) {
  // ".." 상위 디렉토리 접근 제한
  if (strstr(path, "..") != NULL) {
    return 0;
  }

  // 절대 경로 사용 제한
  if (path[0] == '/' || path[0] == '\\') {
    return 0;
  }

  // 기타 특수 문자 제한
  const char *invalid_chars = "<>:\"\\|?*";
  if (strpbrk(path, invalid_chars) != NULL) {
    return 0;
  }

  return 1;
}

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

  // 캐시 확인
  cache_entry *cached = cache_get(normalized_path);
  if (cached) {
    printf("Cache hit: %s\n", normalized_path);
    result.data = cached->data;
    result.size = cached->size;
    result.content_type = strdup(cached->content_type);
    result.status_code = 200;
    cached->ref_count++;
    return result;
  }

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

  // MIME 타입 설정
  result.content_type = strdup(get_mime_type(request_path));
  result.status_code = 200;

  // 캐시에 저장
  if (result.status_code == 200) {
    cache_put(normalized_path, &result);
  }

  return result;
}

// 데이터 포인터로 캐시 엔트리 찾기
static cache_entry *find_cache_entry(const void *data) {
  if (!cache) return NULL;

  for (size_t i = 0; i < cache->size; i++) {
    if (cache->entries[i] && cache->entries[i]->data == data) {
      return cache->entries[i];
    }
  }
  return NULL;
}

// 메모리 해제
void free_file_result(file_result *result) {
  if (!result) return;

  cache_entry *entry = find_cache_entry(result->data);
  if (entry) {
    entry->ref_count--; // 참조 카운트 감소
    if (entry->ref_count == 0) {
      // 실제 메모리 해제는 캐시에서 제거될 때만
      result->data = NULL;
    }
  } else {
    free(result->data); // 캐시되지 않은 경우 직접 해제
  }

  free(result->content_type);
  result->data = NULL;
  result->content_type = NULL;
  result->size = 0;
}


// 캐시 초기화
void cache_init(size_t capacity) {
  cache = (file_cache *) malloc(sizeof(file_cache));
  cache->entries = (cache_entry **) calloc(capacity, sizeof(cache_entry *));
  cache->paths = (char **) calloc(capacity, sizeof(char *));
  cache->size = 0;
  cache->capacity = capacity;
}

// 캐시 정리
void cache_cleanup(void) {
  if (!cache) return;

  for (size_t i = 0; i < cache->size; i++) {
    if (cache->entries[i]) {
      free(cache->entries[i]->data);
      free(cache->entries[i]->content_type);
      free(cache->entries[i]);
    }
    if (cache->paths[i]) {
      free(cache->paths[i]);
    }
  }

  free(cache->entries);
  free(cache->paths);
  free(cache);
  cache = NULL;
}

// 캐시에서 파일 찾기
cache_entry *cache_get(const char *path) {
  if (!cache) {
    printf("Cache not initialized!\n");
    return NULL;
  }

  time_t current_time = time(NULL);
  printf("\n=== Cache Lookup ===\n");
  printf("Looking for path: %s\n", path);
  printf("Current cache size: %zu/%zu\n", cache->size, cache->capacity);

  for (size_t i = 0; i < cache->size; i++) {
    printf("Comparing with cached path: %s\n", cache->paths[i]);
    if (strcmp(cache->paths[i], path) == 0) {
      cache_entry *entry = cache->entries[i];
      printf("Cache hit! Entry found.\n");
      printf("Entry size: %zu bytes\n", entry->size);
      printf("Time in cache: %lld seconds\n", current_time - entry->cached_time);

      // 캐시 유효성 검사 (TTL)
      if (current_time - entry->cached_time > CACHE_TTL) {
        printf("Cache entry expired (TTL: %d seconds)\n", CACHE_TTL);
        cache_remove(path);
        return NULL;
      }

      // 파일 변경 확인
      struct stat st;
      if (stat(path, &st) == 0) {
        if (st.st_mtime > entry->last_modified) {
          printf("File modified since cached\n");
          cache_remove(path);
          return NULL;
        }
      }
      printf("Cache entry valid and returned\n");
      return entry;
    }
  }

  printf("Cache miss!\n");
  return NULL;
}

// 캐시에 파일 추가 (LRU 방식)
void cache_put(const char *path, const file_result *result) {
  if (!cache || !result || !result->data) {
    printf("Invalid cache put attempt!\n");
    return;
  }

  printf("\n=== Cache Put Operation ===\n");
  printf("Adding file: %s\n", path);
  printf("File size: %zu bytes\n", result->size);
  printf("Current cache size: %zu/%zu\n", cache->size, cache->capacity);

  // 캐시가 꽉 찬 경우 가장 오래된 항목 제거
  if (cache->size == cache->capacity) {
    printf("Cache full, removing oldest entry: %s\n", cache->paths[0]);
    cache_remove(cache->paths[0]);
  }

  // 새 엔트리 생성
  cache_entry *entry = (cache_entry *) malloc(sizeof(cache_entry));
  if (!entry) {
    printf("Failed to allocate cache entry!\n");
    return;
  }

  entry->data = malloc(result->size);
  if (!entry->data) {
    printf("Failed to allocate cache data!\n");
    free(entry);
    return;
  }

  // 데이터 복사 시작
  printf("Copying %zu bytes to cache...\n", result->size);
  memcpy(entry->data, result->data, result->size);
  entry->size = result->size;
  entry->content_type = strdup(result->content_type);
  entry->cached_time = time(NULL);
  entry->ref_count = 1;

  struct stat st;
  if (stat(path, &st) == 0) {
    entry->last_modified = st.st_mtime;
  }

  // 캐시에 추가
  size_t idx = cache->size;
  cache->entries[idx] = entry;
  cache->paths[idx] = strdup(path);
  cache->size++;

  printf("File successfully cached\n");
  printf("New cache size: %zu/%zu\n", cache->size, cache->capacity);
}

// 캐시에서 파일 제거
void cache_remove(const char *path) {
  if (!cache) return;

  for (size_t i = 0; i < cache->size; i++) {
    if (strcmp(cache->paths[i], path) == 0) {
      // 메모리 해제
      free(cache->entries[i]->data);
      free(cache->entries[i]->content_type);
      free(cache->entries[i]);
      free(cache->paths[i]);

      // 배열 정리
      for (size_t j = i; j < cache->size - 1; j++) {
        cache->entries[j] = cache->entries[j + 1];
        cache->paths[j] = cache->paths[j + 1];
      }

      cache->size--;
      return;
    }
  }
}
