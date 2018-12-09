/* The MIT License (MIT)
 *
 * Copyright (c) 2018 Jean Gressmann <jean@0x42.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <cstdint>
#include <atomic>
#include <thread>
#include <cassert>

namespace conq
{


// fixed size single producer multiple consumer lock-free queue
template<typename T, std::uint16_t N>
class mpscfsq
{
public:
    static constexpr std::uint16_t capacity = N;

public:
    ~mpscfsq()
    {
        T t;
        while (dequeue(t)) {

        }
    }

    mpscfsq() noexcept
        : m_State(0)
    {
        for (auto i = 0; i < N; ++i) {
            m_Markers[i].store(0, std::memory_order_relaxed);
        }
    }

    mpscfsq(const mpscfsq&) = delete;
    mpscfsq& operator=(const mpscfsq&) = delete;

    std::int32_t enqueue(T&& t)
    {
        std::uint64_t o, n;
        std::int32_t position;
        do
        {
            n = m_State.load(std::memory_order_relaxed);
            o = n;
            auto counter = static_cast<std::uint32_t>(n >> 32);
            auto mux = static_cast<std::uint32_t>(n);
            auto start = mux >> 16;
            std::uint32_t size = mux & 0xffff;
            assert(size <= N);
            if (size == N)
            {
                return -1;
            }

            position = (start + size) % N;
            ++size;
            n = ++counter;
            n <<= 16;
            n |= start;
            n <<= 16;
            n |= size;
        }
        while (!m_State.compare_exchange_weak(o, n, std::memory_order_release, std::memory_order_relaxed));

        T* ts = reinterpret_cast<T*>(m_Raw);
        new (ts + position) T(std::move(t));

        m_Markers[position].store(1, std::memory_order_release);

        return position;
    }

    bool cancel(std::int32_t ticket, bool* cancelled) noexcept
    {
        if (ticket < 0 || static_cast<std::uint32_t>(ticket) >= N)
        {
            return false;
        }

        bool dummy;
        if (!cancelled) {
            cancelled = &dummy;
        }

        int8_t expectedMarker = 1;
        do
        {
            auto marker = m_Markers[ticket].load(std::memory_order_relaxed);
            switch (marker)
            {
            case -1:
            case 0:
                *cancelled = false;
                return true;
            default:
                break;
            }
        }
        while (!m_Markers[ticket].compare_exchange_weak(expectedMarker, -1, std::memory_order_release, std::memory_order_relaxed));

        *cancelled = true;
        return true;
    }

    bool dequeue(T& t)
    {
        uint64_t o, n;
        uint32_t position;
        int8_t expectedMarker = 1;

        while (true)
        {
            n = m_State.load(std::memory_order_relaxed);
            auto mux = static_cast<std::uint32_t>(n);
            position = mux >> 16;
            auto size = mux & 0xffff;
            assert(size <= N);
            if (!size)
            {
                return false;
            }

            bool hasValue;

            do
            {
                hasValue = true;
                auto marker = m_Markers[position].load(std::memory_order_relaxed);
                switch (marker) {
                case -1:
                    hasValue = false;
                    break;
                case 0: // need to wait, value not yet written
                    return false;
                default:
                    break;
                }
            }
            while (hasValue && !m_Markers[position].compare_exchange_weak(expectedMarker, 0, std::memory_order_release, std::memory_order_relaxed));

            if (hasValue) {
                T* ts = reinterpret_cast<T*>(m_Raw);
                t = ts[position];
                ts[position].~T();
            }

            do
            {
                n = m_State.load(std::memory_order_relaxed);
                o = n;
                auto counter = static_cast<uint32_t>(n >> 32);
                auto mux = static_cast<std::uint32_t>(n);
                auto start = mux >> 16;
                assert(start == position);
                auto size = mux & 0xffff;
                assert(size);
                assert(size <= N);

                --size;
                start = (start + 1) % N;
                n = ++counter;
                n <<= 16;
                n |= start;
                n <<= 16;
                n |= size;
            }
            while (!m_State.compare_exchange_weak(o, n, std::memory_order_release, std::memory_order_relaxed));

            if (hasValue) {
                break;
            }
        }

        return true;
    }



private:
    std::atomic<std::uint64_t> m_State;
    std::atomic<std::int8_t> m_Markers[N];
    unsigned char m_Raw[sizeof(T) * N];
};

} // conq
