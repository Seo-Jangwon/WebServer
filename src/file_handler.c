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
#include <server.h>
#include <time.h>

#include "error_handle.h"

#ifdef _WIN32
#include <stdlib.h>
#define PATH_SEPARATOR '\\'
#else
#include <stdlib.h>
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

// 유니코드 코드포인트를 UTF-8로 변환
static int unicode_to_utf8(unsigned int code, char *dst) {
    if (code <= 0x7F) {
        dst[0] = (char) code;
        return 1;
    } else if (code <= 0x7FF) {
        dst[0] = (char) (0xC0 | (code >> 6));
        dst[1] = (char) (0x80 | (code & 0x3F));
        return 2;
    } else if (code <= 0xFFFF) {
        dst[0] = (char) (0xE0 | (code >> 12));
        dst[1] = (char) (0x80 | ((code >> 6) & 0x3F));
        dst[2] = (char) (0x80 | (code & 0x3F));
        return 3;
    } else if (code <= 0x10FFFF) {
        dst[0] = (char) (0xF0 | (code >> 18));
        dst[1] = (char) (0x80 | ((code >> 12) & 0x3F));
        dst[2] = (char) (0x80 | ((code >> 6) & 0x3F));
        dst[3] = (char) (0x80 | (code & 0x3F));
        return 4;
    } else {
        // 유효하지 않음
        return -1;
    }
}

// URL 디코딩
static void url_decode(char *dst, const char *src) {
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            char a = src[1];
            if ((a == 'u' || a == 'U') && src[2] && src[3] && src[4] && src[5]) {
                // 유니코드 인코딩 처리 (%uXXXX)
                if (isxdigit(src[2]) && isxdigit(src[3]) &&
                    isxdigit(src[4]) && isxdigit(src[5])) {
                    char code_str[5] = {src[2], src[3], src[4], src[5], '\0'};
                    unsigned int code;
                    sscanf(code_str, "%x", &code);
                    // 유니코드 코드포인트 -> UTF-8로
                    int len = unicode_to_utf8(code, dst);
                    if (len > 0) {
                        dst += len;
                        src += 6; // '%uXXXX' 처리 후 이동
                        continue;
                    }
                }
            } else if (isxdigit(a) && isxdigit(src[2])) {
                // 표준 URL 인코딩 처리 (%XX)
                char b = src[2];
                a = tolower(a);
                b = tolower(b);
                a = (a >= 'a') ? a - 'a' + 10 : a - '0';
                b = (b >= 'a') ? b - 'a' + 10 : b - '0';
                *dst++ = (char) (a * 16 + b);
                src += 3;
                continue;
            }
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

// 재귀적 디코딩 호출
void recursive_url_decode(char *dst, const char *src) {
    char temp[PATH_MAX];
    strcpy(temp, src);
    char decoded[PATH_MAX];
    while (1) {
        url_decode(decoded, temp);
        if (strcmp(decoded, temp) == 0) break;
        strcpy(temp, decoded);
    }
    strcpy(dst, decoded);
}

// 보안 검사
int is_path_safe(const char *path) {
    if (!path || !*path) {
        LOG_ERROR("Empty path rejected", "Path is null or empty");
        return 0;
    }

    // URL 인코딩된 패턴, 기본 패턴 검사
    if (strstr(path, "..") || strstr(path, "%2e%2e") || strstr(path, "%2E%2E") ||
        strstr(path, "%u2e%u2e") || strstr(path, "%c0%2e") ||
        strstr(path, "%u2215") || strstr(path, "%c0%af") ||
        strstr(path, "%u002e") || strstr(path, "%u002E") ||
        strstr(path, "%00") || strstr(path, "\\0") ||
        strstr(path, "%5c") || strstr(path, "%2f") ||
        strstr(path, "....")) {
        LOG_ERROR("Path traversal pattern detected", path);
        return 0;
    }

    // document_root 절대 경로
    char doc_root_path[PATH_MAX];
#ifdef _WIN32
    if (!_fullpath(doc_root_path, g_server->config.document_root, PATH_MAX)) {
#else
    if (!realpath(g_server->config.document_root, doc_root_path)) {
#endif
        LOG_ERROR("Document root normalization failed", g_server->config.document_root);
        return 0;
    }

    // URL 디코딩, 정규화
    char decoded_path[PATH_MAX];
    recursive_url_decode(decoded_path, path);
    printf("Decoded Path: %s\n", decoded_path);

    // 경로 컴포넌트별 체크
    char path_copy[PATH_MAX];
    strncpy(path_copy, decoded_path, PATH_MAX - 1);
    char *saveptr;
    char *token = strtok_r(path_copy, "/\\", &saveptr);
    int depth = 0;

    while (token) {
        // ".." 관련 변형 검사
        if (strstr(token, "..") || strstr(token, "....")) {
            LOG_ERROR("Path traversal component detected", token);
            return 0;
        }

        // 의심스러운 문자열 패턴 검사
        size_t suspicious_count = 0;
        size_t token_len = strlen(token);
        for (size_t i = 0; i < token_len; i++) {
            if (!isalnum(token[i]) && token[i] != '.' && token[i] != '-' && token[i] != '_') {
                suspicious_count++;
            }
        }
        if (suspicious_count > token_len / 2) {
            LOG_ERROR("Suspicious token pattern detected", token);
            return 0;
        }

        // 디렉토리 깊이 체크
        if (strcmp(token, "..") == 0) {
            depth--;
            if (depth < 0) {
                LOG_ERROR("Directory traversal attempt", decoded_path);
                return 0;
            }
        } else if (strcmp(token, ".") != 0) {
            depth++;
        }

        token = strtok_r(NULL, "/\\", &saveptr);
    }

    // 슬래시 정규화, 연속된 슬래시 제거
    char *src = decoded_path;
    char *dst = decoded_path;
    while (*src) {
        if ((*src == '/' || *src == '\\') && (*(src + 1) == '/' || *(src + 1) == '\\')) {
            src++;
            continue;
        }
        if (*src == '/' || *src == '\\') {
            *dst++ = PATH_SEPARATOR;
        } else {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';

    // 경로 결합, 절대 경로 변환
    char combined_path[PATH_MAX];
    snprintf(combined_path, sizeof(combined_path), "%s%c%s",
             doc_root_path,
             PATH_SEPARATOR,
             decoded_path[0] == '/' || decoded_path[0] == '\\' ? decoded_path + 1 : decoded_path);

    printf("Combined Path: %s\n", combined_path);

    char absolute_path[PATH_MAX];
#ifdef _WIN32
    if (!_fullpath(absolute_path, combined_path, PATH_MAX)) {
#else
    if (!realpath(combined_path, absolute_path)) {
#endif
        LOG_ERROR("Path normalization failed", combined_path);
        return 0;
    }

    printf("Absolute Path: %s\n", absolute_path);

    // document_root 외부 접근 시도 체크
    size_t root_len = strlen(doc_root_path);
    if (strncmp(absolute_path, doc_root_path, root_len) != 0 ||
        (absolute_path[root_len] != PATH_SEPARATOR && absolute_path[root_len] != '\0')) {
        LOG_ERROR("Path outside document root", absolute_path);
        return 0;
    }

    // 숨김 파일 검사
    const char *filename = strrchr(absolute_path, PATH_SEPARATOR);
    if (filename) {
        filename++; // 구분자 다음으로
    } else {
        filename = absolute_path;
    }
    if (filename[0] == '.') {
        LOG_ERROR("Hidden file access attempted", filename);
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
    printf("\n=== MIME Type Detection ===\n");
    printf("File path: %s\n", file_path);

    const char *extension = get_file_extension(file_path);
    printf("Extension: %s\n", extension);

    // 확장자가 없는 경우
    if (!extension || !*extension) {
        printf("No extension found\n");
        return "application/octet-stream; charset=utf-8";
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
            // 텍스트 기반 파일의 경우 UTF-8 인코딩
            if (strstr(mime->mime_type, "text/") == mime->mime_type ||
                strstr(mime->mime_type, "application/json") == mime->mime_type ||
                strstr(mime->mime_type, "application/javascript") == mime->mime_type) {
                static char mime_with_charset[256];
                snprintf(mime_with_charset,
                         sizeof(mime_with_charset),
                         "%s; charset=utf-8",
                         mime->mime_type);
                printf("Found MIME type: %s\n", mime_with_charset);
                return mime_with_charset;
            }
            printf("Found MIME type: %s\n", mime->mime_type);
            return mime->mime_type;
        }
    }

    printf("No matching MIME type found, using default\n");
    return "application/octet-stream; charset=utf-8";
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
    file_result result = {NULL, 0, NULL, 404, NULL};

    printf("\n=== File Read Operation ===\n");
    printf("Base path: %s\n", base_path);
    printf("Request path: %s\n", request_path);

    // 경로 안전성 체크
    if (!is_path_safe(request_path)) {
        result.status_code = 403;
        return result;
    }

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
    snprintf(full_path, sizeof(full_path), "%s%c%s", base_path, PATH_SEPARATOR, cleaned_path);
    printf("Full path: %s\n", full_path);

    // 경로 정규화
    char normalized_path[1024];
    strncpy(normalized_path, full_path, sizeof(normalized_path) - 1);
    normalize_path(normalized_path);

    printf("Normalized path: %s\n", normalized_path);

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
        result.status_code = 404;
        return result;
    }
    printf("File found. Size: %lld bytes\n", (long long) file_stat.st_size);

    // 파일 열기
    FILE *file = fopen(normalized_path, "rb");
    if (!file) {
        printf("Failed to open file: %s (errno: %d)\n", normalized_path, errno);
        // 접근 거부인지 파일 없음인지 확인
        if (errno == EACCES) {
            result.status_code = 403;
        } else if (errno == ENOENT) {
            result.status_code = 404;
        } else {
            result.status_code = 500;
        }
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
        result.error_detail = "Could not allocate buffer for file transfer";
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
