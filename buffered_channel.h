#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <exception>

namespace tcn {

    namespace detail {

        inline
        std::chrono::system_clock::time_point convert(
                std::chrono::system_clock::time_point const& timeout_time) noexcept {
            return timeout_time;
        }

        template< typename Clock, typename Duration >
        std::chrono::system_clock::time_point convert(
                std::chrono::time_point< Clock, Duration > const& timeout_time) {
            return std::chrono::system_clock::now() + ( timeout_time - Clock::now() );
        }

    }

    enum class channel_op_status {
        success = 0,
        empty,
        full,
        closed,
        timeout
    };

    template<typename T>
    class buffered_channel {
    public:
        using value_type = typename std::remove_reference<T>::type;

    private:
        using slot_type = value_type;

        mutable std::mutex mutex_{};
        std::condition_variable waiting_producers_{};
        std::condition_variable waiting_consumers_{};
        slot_type *slots_;
        std::size_t pidx_{0};
        std::size_t cidx_{0};
        std::size_t capacity_;
        bool closed_{false};

        bool is_full_() const noexcept {
            return cidx_ == ((pidx_ + 1) % capacity_);
        }

        bool is_empty_() const noexcept {
            return cidx_ == pidx_;
        }

        bool is_closed_() const noexcept {
            return closed_;
        }

    public:
        explicit buffered_channel(std::size_t capacity) :
                capacity_{capacity} {
            if (2 > capacity_ || 0 != (capacity_ & (capacity_ - 1))) {
                throw std::length_error{"buffer capacity is invalid"};
            }
            slots_ = new slot_type[capacity_];
        }

        ~buffered_channel() {
            close();
            delete[] slots_;
        }

        buffered_channel(buffered_channel const &) = delete;

        buffered_channel &operator=(buffered_channel const &) = delete;

        bool is_closed() const noexcept {
            std::scoped_lock<std::mutex> lk{mutex_};
            return is_closed_();
        }

        void close() noexcept {
            std::scoped_lock<std::mutex> lk{mutex_};
            if (!closed_) {
                closed_ = true;
                waiting_producers_.notify_all();
                waiting_consumers_.notify_all();
            }
        }

        channel_op_status try_push(value_type const &value) {
            std::scoped_lock<std::mutex> lk{mutex_};
            if (is_closed_()) {
                return channel_op_status::closed;
            }
            if (is_full_()) {
                return channel_op_status::full;
            }
            slots_[pidx_] = value;
            pidx_ = (pidx_ + 1) % capacity_;
            waiting_consumers_.notify_one();
            return channel_op_status::success;
        }

        channel_op_status try_push(value_type &&value) {
            std::scoped_lock<std::mutex> lk{mutex_};
            if (is_closed_()) {
                return channel_op_status::closed;
            }
            if (is_full_()) {
                return channel_op_status::full;
            }
            slots_[pidx_] = std::move(value);
            pidx_ = (pidx_ + 1) % capacity_;
            waiting_consumers_.notify_one();
            return channel_op_status::success;
        }

        channel_op_status push(value_type const &value) {
            std::unique_lock<std::mutex> lk{mutex_};
            if (is_closed_()) {
                return channel_op_status::closed;
            }

            if (is_full_()) {
                waiting_producers_.wait(lk, [&]() {
                    return !(is_full_() && !is_closed_());
                });
            }

            if (is_closed_()) {
                return channel_op_status::closed;
            }

            slots_[pidx_] = value;
            pidx_ = (pidx_ + 1) % capacity_;
            waiting_consumers_.notify_one();
            return channel_op_status::success;
        }

        channel_op_status push(value_type &&value) {
            std::unique_lock<std::mutex> lk{mutex_};

            if (is_closed_()) {
                return channel_op_status::closed;
            }

            if (is_full_()) {
                waiting_producers_.wait(lk, [&]() {
                    return !(is_full_() && !is_closed_());
                });
            }

            if (is_closed_()) {
                return channel_op_status::closed;
            }

            slots_[pidx_] = std::move(value);
            pidx_ = (pidx_ + 1) % capacity_;

            waiting_consumers_.notify_one();
            return channel_op_status::success;
        }

        template<typename Rep, typename Period>
        channel_op_status push_wait_for(value_type const &value,
                                        std::chrono::duration<Rep, Period> const &timeout_duration) {
            return push_wait_until(value,
                                   std::chrono::system_clock::now() + timeout_duration);
        }

        template<typename Rep, typename Period>
        channel_op_status push_wait_for(value_type &&value,
                                        std::chrono::duration<Rep, Period> const &timeout_duration) {
            return push_wait_until(std::forward<value_type>(value),
                                   std::chrono::system_clock::now() + timeout_duration);
        }

        template<typename Clock, typename Duration>
        channel_op_status push_wait_until(value_type const &value,
                                          std::chrono::time_point<Clock, Duration> const &timeout_time_) {
            std::chrono::system_clock::time_point timeout_time = detail::convert(timeout_time_);
            std::unique_lock<std::mutex> lk{mutex_};

            if (is_closed_()) {
                return channel_op_status::closed;
            }

            if (is_full_()) {
                auto status = waiting_producers_.wait_until(lk, timeout_time, [&]() {
                    return !(is_full_() && !is_closed_());
                });

                if (!status) {
                    return channel_op_status::timeout;
                }
            }

            if (is_closed_()) {
                return channel_op_status::closed;
            }

            slots_[pidx_] = value;
            pidx_ = (pidx_ + 1) % capacity_;
            waiting_consumers_.notify_one();
            return channel_op_status::success;
        }

        template<typename Clock, typename Duration>
        channel_op_status push_wait_until(value_type &&value,
                                          std::chrono::time_point<Clock, Duration> const &timeout_time_) {
            std::chrono::system_clock::time_point timeout_time = detail::convert(timeout_time_);
            std::unique_lock<std::mutex> lk{mutex_};

            if (is_closed_()) {
                return channel_op_status::closed;
            }

            if (is_full_()) {
                auto status = waiting_producers_.wait_until(lk, timeout_time, [&]() {
                    return !(is_full_() && !is_closed_());
                });

                if (!status) {
                    return channel_op_status::timeout;
                }
            }

            if (is_closed_()) {
                return channel_op_status::closed;
            }

            slots_[pidx_] = std::move(value);
            pidx_ = (pidx_ + 1) % capacity_;
            // notify one waiting consumer
            waiting_consumers_.notify_one();
            return channel_op_status::success;
        }

        channel_op_status try_pop(value_type &value) {
            std::scoped_lock<std::mutex> lk{mutex_};
            if (is_empty_()) {
                return is_closed_()
                       ? channel_op_status::closed
                       : channel_op_status::empty;
            }
            value = std::move(slots_[cidx_]);
            cidx_ = (cidx_ + 1) % capacity_;
            waiting_producers_.notify_one();
            return channel_op_status::success;
        }

        channel_op_status pop(value_type &value) {
            std::unique_lock<std::mutex> lk{mutex_};

            if (is_closed_()) {
                return channel_op_status::closed;
            }

            if (is_empty_()) {
                waiting_consumers_.wait(lk, [&]() {
                    return !(is_empty_() && !is_closed_());
                });
            }

            if (is_closed_()) {
                return channel_op_status::closed;
            }

            value = std::move(slots_[cidx_]);
            cidx_ = (cidx_ + 1) % capacity_;
            waiting_producers_.notify_one();
            return channel_op_status::success;

        }

        value_type value_pop() {
            std::unique_lock<std::mutex> lk{mutex_};

            if (is_closed_()) {
                return channel_op_status::closed;
            }

            if (is_empty_()) {
                auto status = waiting_consumers_.wait(lk, [&]() {
                    return !(is_empty_() && !is_closed_());
                });

                if (!status) {
                    return channel_op_status::timeout;
                }
            }

            if (is_closed_()) {
                return channel_op_status::closed;
            }

            value_type value = std::move(slots_[cidx_]);
            cidx_ = (cidx_ + 1) % capacity_;
            waiting_producers_.notify_one();
            return value;
        }

        template<typename Rep, typename Period>
        channel_op_status pop_wait_for(value_type &value,
                                       std::chrono::duration<Rep, Period> const &timeout_duration) {
            return pop_wait_until(value,
                                  std::chrono::system_clock::now() + timeout_duration);
        }

        template<typename Clock, typename Duration>
        channel_op_status pop_wait_until(value_type &value,
                                         std::chrono::time_point<Clock, Duration> const &timeout_time_) {
            std::chrono::system_clock::time_point timeout_time = detail::convert(timeout_time_);
            std::unique_lock<std::mutex> lk{mutex_};

            if (is_closed_()) {
                return channel_op_status::closed;
            }

            if (is_empty_()) {
                auto status = waiting_consumers_.wait_until(lk, timeout_time, [&]() {
                    return !(is_empty_() && !is_closed_());
                });

                if (!status) {
                    return channel_op_status::timeout;
                }
            }

            if (is_closed_()) {
                return channel_op_status::closed;
            }

            value = std::move(slots_[cidx_]);
            cidx_ = (cidx_ + 1) % capacity_;
            waiting_producers_.notify_one();
            return channel_op_status::success;
        }

        class iterator {
        private:
            typedef typename std::aligned_storage<sizeof(value_type), alignof(value_type)>::type storage_type;

            buffered_channel *chan_{nullptr};
            storage_type storage_;

            void increment_(bool initial = false) {
                assert(nullptr != chan_);
                try {
                    if (!initial) {
                        reinterpret_cast< value_type * >( std::addressof(storage_))->~value_type();
                    }
                    ::new(static_cast< void * >( std::addressof(storage_))) value_type{chan_->value_pop()};
                } catch (std::exception const &) {
                    // @todo: maybe need more specific exception handlers here ..
                    chan_ = nullptr;
                }
            }

        public:
            using iterator_category = std::input_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using pointer = value_type *;
            using reference = value_type &;

            using pointer_t = pointer;
            using reference_t = reference;

            iterator() noexcept = default;

            explicit iterator(buffered_channel<T> *chan) noexcept:
                    chan_{chan} {
                increment_(true);
            }

            iterator(iterator const &other) noexcept:
                    chan_{other.chan_} {
            }

            iterator &operator=(iterator const &other) noexcept {
                if (this != &other) {
                    chan_ = other.chan_;
                }
                return *this;
            }

            bool operator==(iterator const &other) const noexcept {
                return other.chan_ == chan_;
            }

            bool operator!=(iterator const &other) const noexcept {
                return other.chan_ != chan_;
            }

            iterator &operator++() {
                reinterpret_cast< value_type * >( std::addressof(storage_))->~value_type();
                increment_();
                return *this;
            }

            const iterator operator++(int) = delete;

            reference_t operator*() noexcept {
                return *reinterpret_cast< value_type * >( std::addressof(storage_));
            }

            pointer_t operator->() noexcept {
                return reinterpret_cast< value_type * >( std::addressof(storage_));
            }
        };

        friend class iterator;
    };

    template<typename T>
    typename buffered_channel<T>::iterator
    begin(buffered_channel<T> &chan) {
        return typename buffered_channel<T>::iterator(&chan);
    }

    template<typename T>
    typename buffered_channel<T>::iterator
    end(buffered_channel<T> &) {
        return typename buffered_channel<T>::iterator();
    }
}
