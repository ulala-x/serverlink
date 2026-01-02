# Windows WSAStartup 디버깅 작업 인수인계

---

## 최종 목표: 6개 플랫폼 CI/CD 완성

### 대상 플랫폼
| Platform | Architecture | Runner | I/O Backend | 빌드 | 테스트 |
|----------|-------------|--------|-------------|------|--------|
| Linux | x64 | ubuntu-22.04 | epoll | ✅ 통과 | ✅ 통과 |
| Linux | ARM64 | ubuntu-22.04-arm | epoll | ✅ 통과 | (크로스컴파일) |
| Windows | x64 | windows-2022 | wepoll | ✅ 통과 | ❌ **실패** |
| Windows | ARM64 | windows-2022 | wepoll | ✅ 통과 | (크로스컴파일) |
| macOS | x64 (Intel) | macos-15 | kqueue | ✅ 통과 | ✅ 통과 |
| macOS | ARM64 | macos-15 | kqueue | ✅ 통과 | ✅ 통과 |

### CI/CD 파이프라인 구성
```
.github/workflows/
├── build.yml        # 메인 빌드 (6개 플랫폼 빌드 + 테스트 + 릴리즈)
├── pr-check.yml     # PR 검증
└── benchmark.yml    # 성능 벤치마크

build-scripts/
├── linux/build.sh
├── windows/build.ps1
├── macos/build.sh
└── common/verify.sh

scripts/
├── run_tests.sh      # Linux/macOS
├── run_tests.ps1     # Windows
├── run_benchmarks.sh
├── run_benchmarks.ps1
└── format_benchmark.py
```

### 완료된 작업
- [x] VERSION 파일 생성
- [x] 빌드 스크립트 (Linux, Windows, macOS)
- [x] 테스트 스크립트
- [x] GitHub Actions workflows
- [x] Linux x64/ARM64 빌드 및 테스트
- [x] macOS x64/ARM64 빌드 및 테스트
- [x] Windows x64/ARM64 빌드
- [x] iphlpapi 라이브러리 링크 수정
- [x] unistd.h Windows 호환성 수정
- [x] test_glob_pattern 링커 에러 수정
- [x] test_format_helpers slk_assert_fmt 에러 수정
- [x] macOS ARM64 test_introspection 타임아웃 수정
- [x] test_span_api 링커 에러 수정
- [x] Windows CTest -C Release 플래그 수정

### 남은 작업
- [ ] **Windows x64 테스트 통과** ← 현재 작업 중
- [ ] 전체 6개 플랫폼 CI 통과 확인
- [ ] 릴리즈 자동화 검증

### 릴리즈 산출물 (목표)
```
serverlink-v0.1.0-linux-x64.zip
serverlink-v0.1.0-linux-arm64.zip
serverlink-v0.1.0-windows-x64.zip
serverlink-v0.1.0-windows-arm64.zip
serverlink-v0.1.0-macos-x64.zip
serverlink-v0.1.0-macos-arm64.zip
checksums.txt
```

---

## 현재 상황

### 문제
Windows x64 빌드에서 모든 소켓 관련 테스트가 실패:
```
Assertion failed: Successful WSASTARTUP not yet performed [10093]
```

### 원인 분석
- `WSAStartup()`이 소켓 함수 호출 전에 실행되지 않음
- `ctx_t` 생성자에서 `_term_mailbox` 멤버가 초기화될 때 소켓 필요
- 멤버 초기화는 생성자 본문보다 먼저 실행됨

### 시도한 해결책들

1. **Static initializer struct** - 실패
   - 정적 초기화 순서 문제로 DLL 로드 시 동작 안함

2. **C++11 function-local static** - 실패
   ```cpp
   bool initialize_network() {
       static bool initialized = do_initialize_network();
       return initialized;
   }
   ```
   - 이론상 동작해야 하지만 실패

3. **DllMain + function-local static** - 현재 테스트 중
   ```cpp
   #if defined _WIN32 && defined SL_BUILDING_DLL
   extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID) {
       if (fdwReason == DLL_PROCESS_ATTACH) {
           slk::initialize_network();
       }
       return TRUE;
   }
   #endif
   ```

## 테스트 방법

### 1. 빌드
```powershell
cd C:\path\to\serverlink
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DBUILD_SHARED_LIBS=ON -DBUILD_TESTS=ON
cmake --build build --config Release
```

### 2. 테스트 실행
```powershell
cd build
ctest -C Release --output-on-failure
```

### 3. 단일 테스트 실행 (디버깅용)
```powershell
.\build\tests\Release\test_ctx.exe
.\build\tests\Release\test_wsastartup_order.exe
```

## 핵심 파일

| 파일 | 설명 |
|------|------|
| `src/io/ip.cpp` | WSAStartup 초기화 코드, DllMain |
| `src/io/ip.hpp` | `initialize_network()` 선언 |
| `tests/windows/test_wsastartup_order.cpp` | WSAStartup 테스트 |
| `tests/CMakeLists.txt` | 테스트 빌드 설정 |

## 디버깅 포인트

### ip.cpp의 주요 함수들
```cpp
// Line 34-41: WSAStartup 호출
static bool do_initialize_network() {
    const WORD version_requested = MAKEWORD(2, 2);
    WSADATA wsa_data;
    const int rc = WSAStartup(version_requested, &wsa_data);
    return rc == 0 && LOBYTE(wsa_data.wVersion) == 2
                   && HIBYTE(wsa_data.wVersion) == 2;
}

// Line 44-54: 초기화 함수
bool initialize_network() {
    static bool initialized = do_initialize_network();
    return initialized;
}

// Line 56-90: 소켓 생성 (initialize_network 호출)
fd_t open_socket(int domain_, int type_, int protocol_) {
    initialize_network();  // Line 60
    // ...
    const fd_t s = WSASocket(...);  // Line 70-71
}
```

### 에러 발생 위치
```cpp
// Line 153-160: make_fdpair_tcpip
static int make_fdpair_tcpip(fd_t *r_, fd_t *w_) {
    SOCKET listener = open_socket(AF_INET, SOCK_STREAM, 0);
    wsa_assert(listener != INVALID_SOCKET);  // <-- 여기서 실패
}
```

## 확인해야 할 사항

1. **DllMain이 실제로 호출되는가?**
   - `printf` 또는 `OutputDebugString` 추가해서 확인

2. **SL_BUILDING_DLL 매크로가 정의되는가?**
   - CMake에서 shared lib 빌드 시 정의됨
   - `CMakeLists.txt` 확인:
     ```cmake
     if(BUILD_SHARED_LIBS)
         target_compile_definitions(serverlink
             PRIVATE SL_BUILDING_DLL
             ...
         )
     endif()
     ```

3. **initialize_network()의 반환값**
   - WSAStartup 성공 여부 확인

## 디버깅 코드 추가 예시

```cpp
// ip.cpp에 추가
#ifdef _WIN32
#include <cstdio>

static bool do_initialize_network() {
    fprintf(stderr, "[DEBUG] do_initialize_network called\n");
    const WORD version_requested = MAKEWORD(2, 2);
    WSADATA wsa_data;
    const int rc = WSAStartup(version_requested, &wsa_data);
    fprintf(stderr, "[DEBUG] WSAStartup returned %d\n", rc);
    bool success = rc == 0 && LOBYTE(wsa_data.wVersion) == 2
                           && HIBYTE(wsa_data.wVersion) == 2;
    fprintf(stderr, "[DEBUG] initialization %s\n", success ? "SUCCESS" : "FAILED");
    return success;
}
#endif
```

## Git 상태

현재 브랜치: `main`
최신 커밋: `fix(windows): Restore DllMain for WSAStartup on DLL load`

```bash
git pull origin main  # Windows에서 최신 코드 가져오기
```

## 성공 기준

- `test_ctx` 통과
- `test_wsastartup_order` 통과
- 모든 47개 테스트 통과

## 참고 문서

- `WINDOWS_IOCP_SUPPORT.md` - Windows I/O 백엔드 정보
- `CLAUDE.md` - 프로젝트 전체 상태
