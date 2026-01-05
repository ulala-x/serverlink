/* SPDX-License-Identifier: MPL-2.0 */

// Asio 통합 검증 테스트
// Asio 헤더가 올바르게 포함되고 기본 타입이 사용 가능한지 확인

#include "../../src/io/i_async_stream.hpp"
#include <cassert>
#include <cstdio>

#ifdef SL_USE_ASIO
#include <asio.hpp>
#endif

// i_async_stream 인터페이스의 간단한 구현
class test_stream : public slk::i_async_stream
{
public:
    void async_read(void* /*buf*/, size_t /*len*/, read_handler handler) override
    {
        // 테스트용 더미 구현
        handler(0, 0);
    }

    void async_write(const void* /*buf*/, size_t /*len*/, write_handler handler) override
    {
        // 테스트용 더미 구현
        handler(0, 0);
    }

    void close() override
    {
        // 테스트용 더미 구현
    }
};

int main()
{
    printf("=== Asio Integration Test ===\n");

    // 1. i_async_stream 인터페이스 테스트
    {
        test_stream stream;

        bool read_called = false;
        stream.async_read(nullptr, 0, [&](size_t bytes, int error) {
            read_called = true;
            assert(bytes == 0);
            assert(error == 0);
        });
        assert(read_called);
        printf("✓ async_read interface works\n");

        bool write_called = false;
        stream.async_write(nullptr, 0, [&](size_t bytes, int error) {
            write_called = true;
            assert(bytes == 0);
            assert(error == 0);
        });
        assert(write_called);
        printf("✓ async_write interface works\n");

        stream.close();
        printf("✓ close interface works\n");
    }

#ifdef SL_USE_ASIO
    // 2. Asio 헤더 포함 확인
    {
        asio::error_code ec;
        printf("✓ Asio headers included (error_code type available)\n");
    }
#else
    printf("⚠ Asio not enabled (SL_USE_ASIO not defined)\n");
#endif

    printf("\nAll Asio integration tests passed!\n");
    return 0;
}
