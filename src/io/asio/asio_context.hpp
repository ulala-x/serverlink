/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_ASIO_CONTEXT_HPP_INCLUDED
#define SERVERLINK_ASIO_CONTEXT_HPP_INCLUDED

#include "../../util/config.hpp"

#ifdef SL_USE_ASIO
#include <asio.hpp>
#include <memory>
#include <thread>

namespace slk
{
    // Asio io_context 관리 클래스
    // 싱글톤 패턴으로 전역 io_context 제공
    class asio_context_t
    {
    public:
        // 싱글톤 인스턴스 반환
        static inline asio_context_t& instance()
        {
            static asio_context_t instance;
            return instance;
        }

        // io_context 반환
        inline asio::io_context& get_context()
        {
            return _io_context;
        }

        // io_context 실행 시작 (별도 스레드)
        inline void start()
        {
            if (_running) {
                return;
            }

            _work_guard = std::make_unique<asio::io_context::work>(_io_context);
            _thread = std::make_unique<std::thread>([this]() {
                _io_context.run();
            });
            _running = true;
        }

        // io_context 중지
        inline void stop()
        {
            if (!_running) {
                return;
            }

            _work_guard.reset();
            _io_context.stop();

            if (_thread && _thread->joinable()) {
                _thread->join();
            }

            _thread.reset();
            _running = false;
        }

        // 실행 중인지 확인
        inline bool is_running() const
        {
            return _running;
        }

    private:
        inline asio_context_t() : _running(false)
        {
        }

        inline ~asio_context_t()
        {
            stop();
        }

        // 복사 방지
        asio_context_t(const asio_context_t&) = delete;
        asio_context_t& operator=(const asio_context_t&) = delete;

        asio::io_context _io_context;
        std::unique_ptr<asio::io_context::work> _work_guard;
        std::unique_ptr<std::thread> _thread;
        bool _running;
    };
}

#endif // SL_USE_ASIO
#endif
