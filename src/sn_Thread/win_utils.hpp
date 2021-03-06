#ifndef SN_THREAD_WIN_UTILS_H
#define SN_THREAD_WIN_UTILS_H

#include "../sn_CommonHeader.h"

namespace sn_Thread {

#if defined(__WIN32__) && defined(SN_ENABLE_WINDOWS_API)

	// ref: https://gist.github.com/LYP951018/88eb616b939f19dad90bd6b087baa4d9
	namespace win32_handler {
		using Win32Handle = std::unique_ptr<void, void(*)(void *) noexcept>;
		Win32Handle make_win32handle(void* handle) {
			return Win32Handle(handle, [](void* h) noexcept {
				::CloseHandle(h);
			});
		}

		struct Done {};

		template <typename T>
		struct Payload {
			Win32Handle m_event;
			std::function<T()> m_func;
			std::variant<std::monostate, std::exception_ptr, T, Done> m_data;

			template <typename F>
			Payload(F f)
				: m_func{std::move(f)}, 
				m_event(make_win32handle(::CreateEventW({}, FALSE, FALSE, {}))) {
					assert(m_event);
			}
		};

		template <typename T>
		class Future {
		public:
			T get() {
				const auto wait_result = WaitForSingleObject(m_payload->m_event.get(), INFINITE);
				if (wait_result == WAIT_OBJECT_0) {
					switch(m_payload->m_data.index()) {
						case 1:
							done();
							std::rethrow_exception(std::get<1>(m_payload->m_data));
						case 2: {
							const auto result = std::get<2>(std::move(m_payload->m_data));
							done();
							return result;
						}
						default:
							std::terminate();
					}
				}
				else {
					throw std::system_error{static_cast<int>(::GetLastError()), std::system_category()};
				}
			}
			
			~Future() {
				if (m_payload->m_data.index() != 3) {
					WaitForSingleObject(m_payload->m_event.get(), INFINITE);
					done();
				}
			}

			Future(std::unique_ptr<Payload<T>> payload)
				: m_payload{std::move(payload)} {}

			Future(const Future& rhs)
				: m_payload{rhs.m_payload} {}

			Future(Future&& rhs)
				: m_payload{std::move(rhs.m_payload)} {}

		private:

			void done() {
				m_payload->m_data = Done{};
			}

			template <typename F>
			friend Future<T> async(F f);

			std::unique_ptr<Payload<T>> m_payload;
		};

		// std::invoke_result_of<F> = std::result_of_t<F()>
		// better use sn_Assist::sn_function_traits instead
		template <typename F>
		Future<std::result_of_t<F()>> async(F f) {
			using result_t = std::result_of_t<F()>;
			auto payload = std::make_unique<Payload<result_t>>(std::move(f));
			::QueueUserWorkItem([](void* param) -> DWORD {
				const auto p = static_cast<Payload<result_t>*>(param);
				{
					try {
						p->m_data = std::invoke(p->m_func);
					}
					catch (...) {
						p->m_data = std::current_exception();
					}
				}
				SetEvent(p->m_event.get());
				return 0;
			}, payload.get(), WT_EXECUTEDEFAULT);
			return Future<result_t>{std::move(payload)};
		}
	}

	inline int futex_wake(std::atomic<std::int32_t>* addr) {
		::WakeByAddressSingle( static_cast< void * >(addr) );
		return 0;
	}

	inline int futex_wait(std::atomic<std::int32_t> * addr, std::int32_t x) {
		::WaitOnAddress(static_cast<volatile void *>( addr), &x, sizeof(x), INFINITE);
		return 0;
	}

	// synchapi.h
	namespace fast_mutex {
		class win_mutex {
			std::atomic<bool> is_locked = false;
		public:
			// try lock and wait is just change parameters
			void lock() {
				bool expected = false;
				while (!is_locked.compare_exchange_weak(expected, true
					std::memory_order_acq_rel,
					std::memory_order_acquire
				)) {
					// futex(x, FUTEX_WAIT_PRIVATE, v)
					WaitOnAddress(&is_locked, &expected, 1, INFINITE);
				}
				// If not atomic
				/*
					while(!is_locked) {
						WaitOnAddress(@is_locked, &expected, 1, INFINITE);
					}
				*/
			}

			void unlock() {
				bool expected = true;
				while (!is_locked.compare_exchange_weak(expected, false
					std::memory_order_release,
					std::memory_order_relaxed
				))
					;
				// futex(x, FUTEX_WAKE_PRIVATE, 1)
				WakeByAddressSingle(&is_locked);
				// If not atomic
				// just set and wakeup
			}

			win_mutex() = default;
			win_mutex(const win_mutex &) = delete;
			win_mutex & operator = (const win_mutex &) = delete;

		};


	}

#endif


}



#endif