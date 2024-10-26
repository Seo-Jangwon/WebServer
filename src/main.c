/*
 * 1. 서버 설정 초기화
 * 2. 서버 시작 및 실행
 * 3. 종료 처리
 */

#include "server.h"
#include "config.h"
#include "file_handler.h"
#include <stdio.h>

int main() {
  // 캐시 초기화
  cache_init(100);

  // 기본 설정 로드
  server_config config = load_default_config();

  // 설정 검증
  if (!validate_config(&config)) {
    fprintf(stderr, "Invalid server configuration\n");
    return 1;
  }

  // 설정 출력
  print_config(&config);

  // 서버 초기화
  http_server server = {0};
  server.config = config;

  if (server_init(&server) != 0) {
    fprintf(stderr, "Server initialization failed\n");
    return 1;
  }

  // 서버 시작
  int result = server_start(&server);

  // 서버 종료
  server_stop(&server);

  // 캐시 정리
  cache_cleanup();

  return result;
}
