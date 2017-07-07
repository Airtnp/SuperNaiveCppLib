#ifndef SN_FUNCTION_H
#define SN_FUNCTION_H

#include "sn_CommonHeader.h"
#include "sn_Assist.hpp"
#include "sn_Type.hpp"

// ref: https://github.com/vczh-libraries/Release/blob/master/Import/Vlpp.h (Change ref by license)
// ref: qicsomos/cosmos/Lazy qicosmos/cosmos/modern_functor
namespace sn_Function {
	
	namespace function {
		template <typename T>
		class Func {};

		namespace invoker {
			template <typename R, typename ...Args>
			class Invoker {
			public:
				virtual R invoke(Args&& ...args) = 0;
			};

			template <typename R, typename ...Args>
			class StaticInvoker : public Invoker<R, Args...> {
			protected:
				R(*m_func)(Args... args);
			public:
				StaticInvoker(R(*func)(Args...)) : m_func(func) {}
				R invoke(Args&& ...args) override {
					return m_func(std::forward<Args>(args)...);
				}
			};

			template <typename C, typename R, typename ...Args>
			class MemberInvoker : public Invoker<R, Args...> {
			protected:
				C* m_class;
				R(C::*m_func)(Args... args);
			public:
				MemberInvoker(C* _class, R(C::*func)(Args...)) : m_class(_class), m_func(func) {}
				R invoke(Args&& ...args) override {
					return (m_class->*m_func)(std::forward<Args>(args)...);
				}
			};

			template <typename C, typename R, typename ...Args>
			class CallableInvoker : public Invoker<R, Args...> {
			protected:
				C m_callable;
			public:
				CallableInvoker(const C& callable) : m_callable(callable) {}
				R invoke(Args&& ...args) override {
					return m_callable(std::forward<Args>(args)...);
				}
			};

			template <typename C, typename ...Args>
			class CallableInvoker<C, void, Args...> : public Invoker<void, Args...> {
			protected:
				C m_callable;
			public:
				CallableInvoker(const C& callable) : m_callable(callable) {}
				void invoke(Args&& ...args) override {
					m_callable(std::forward<Args>(args)...);
				}
			};


		}

		template <typename R, typename ...Args>
		class Func<R(Args...)> {
		protected:
			observer_ptr<invoker::Invoker<R, Args...>> m_invoker;
		public:
			typedef R function_type(Args...);
			using result_type = R;

			Func() {}
			Func(const Func<R(Args...)>& rhs) : m_invoker(rhs.m_invoker) {}
			Func(R(*func)(Args...)) {
				m_invoker = new invoker::StaticInvoker<R, Args...>(func);
			}
			template <typename C>
			Func(C* obj, R(C::*func)(Args...)) {
				m_invoker = new invoker::MemberInvoker<C, R, Args...>(obj, func);
			}
			template <typename C>
			Func(const C& func) {
				m_invoker = new invoker::CallableInvoker<C, R, Args...>(func);
			}

			R operator()(Args&&... args) const {
				return m_invoker->invoke(std::forward<Args>(args)...);
			}

			bool operator==(const Func<R(Args...)>& rhs) const {
				return m_invoker == rhs.m_invoker;
			}

			bool operator!=(const Func<R(Args...)>& rhs) const {
				return m_invoker != rhs.m_invoker;
			}

			operator bool() const {
				return m_invoker;
			}

		};

		template <typename R, typename ...Args>
		Func<R(Args...)> make_func(R(*function)(Args...)) {
			return Func<R(Args...)>(function);
		}

		template <typename C, typename R, typename ...Args>
		Func<R(Args...)> make_func(C* obj, R(C::*function)(Args...)) {
			return Func<R(Args...)>(obj, function);
		}

		template <typename R, typename ...Args>
		Func<R(Args...)> make_func(const R(&function)(Args...)) {
			return Func<R(Args...)>(function);
		}

		template <typename C>
		auto make_func(const C& function) {
			using T = sn_Assist::sn_function_traits::function_traits<C>;
			using FT = typename T::function_type;
			return Func<FT>(function);
		}

	}

	namespace bind {
		template <typename T, typename R, typename ...Args>
		auto member_lambda_bind(R(T::* const m_fun)(Args...), const T* obj) {
			return [=](auto&& ...args) {
				(obj->*m_fun)(std::forward<decltype(args)>(args)...);
			};
		}

#ifdef _MSC_VER
		template <typename T, typename R, typename ...Args, std::size_t Is>
		auto member_stl_bind_impl(R(T::* const m_fun)(Args...), const T* obj, std::index_sequence<Is...>) {
			return std::bind(m_fun, obj, std::_Ph<Is + 1>{}...);
		}

		template <std::size_t I, typename T, typename R, typename ...Args>
		auto member_stl_bind(R(T::* const m_fun)(Args...), const T* obj) {
			return member_stl_bind_impl(m_fun, obj, std::make_index_sequence<I>{});
		}
#endif
	}

	namespace currying {
		
		template <typename T>
		struct Currying {};


		template <typename R, typename Arg0, typename ...Args>
		struct Currying<R(Arg0, Args...)> {
			typedef R function_type(Arg0, Args...);
			typedef R curried_type(Args...);
			using first_parameter_type = Arg0;

			class Binder {
			protected:
				function::Func<function_type> m_target;
				Arg0 m_firstParam;
			public:
				Binder(const function::Func<function_type>& target, Arg0&& param) : m_target(target), m_firstParam(std::forward<Arg0>(param)) {}

				R operator()(Args&&... args) {
					return m_target(std::forward<Arg0>(m_firstParam), std::forward<Args>(args)...);
				}

				function::Func<function_type> temp_func() {
					return m_target;
				}
			};

			class Currier {
			protected:
				function::Func<function_type> m_target;
			public:
				Currier(const function::Func<function_type>& target) : m_target(target) {}
				template <typename RT, typename ...TArgs>
				Currier(const typename Currying<RT(Args...)>::Binder& binder) : m_target(binder) {}

				typename Currying<R(Args...)>::Currier operator()(Arg0&& param) const {
					return typename Currying<R(Args...)>::Currier(Binder(m_target, std::forward<Arg0>(param)));
				}

			};

			class SingleCurrier {
			protected:
				function::Func<function_type> m_target;
			public:
				SingleCurrier(const function::Func<function_type>& target) : m_target(target) {}

				Binder operator()(Arg0&& param) const {
					return Binder(m_target, std::forward<Arg0>(param));
				}

			};

			template <std::size_t N, typename V = void>
			class MultiCurrier {
			protected:
				function::Func<function_type> m_target;
			public:
				MultiCurrier(const function::Func<function_type>& target) : m_target(target) {}
				template <typename RT, typename ...TArgs>
				MultiCurrier(const typename Currying<RT(Args...)>::Binder& binder) : m_target(binder) {}

				typename Currying<R(Args...)>::template MultiCurrier<N - 1> operator()(Arg0&& param) const {
					return typename Currying<R(Args...)>::template MultiCurrier<N - 1>(Binder(m_target, std::forward<Arg0>(param)));
				}
			};

			template <typename V>
			class MultiCurrier<1, V> {
			protected:
				function::Func<function_type> m_target;
			public:
				MultiCurrier(const function::Func<function_type>& target) : m_target(target) {}

				Binder operator()(Arg0&& param) const {
					return Binder(m_target, std::forward<Arg0>(param));
				}
			};
		};

		template <typename R, typename Arg0>
		struct Currying<R(Arg0)> {
			typedef R function_type(Arg0);
			typedef R curried_type(Arg0);
			using first_parameter_type = Arg0;

			class Binder {
			protected:
				function::Func<function_type> m_target;
				Arg0 m_firstParam;
			public:
				Binder(const function::Func<function_type>& target, Arg0&& param) : m_target(target), m_firstParam(std::forward<Arg0>(param)) {}

				R operator()() {
					return m_target(std::forward<Arg0>(m_firstParam));
				}

				function::Func<function_type> temp_func() {
					return m_target;
				}
			};

			class Currier {
			protected:
				function::Func<function_type> m_target;
			public:
				Currier(const function::Func<function_type>& target) : m_target(target) {}

				Binder operator()(Arg0&& param) const {
					return Binder(m_target, std::forward<Arg0>(param));
				}

			};

			class SingleCurrier {
			protected:
				function::Func<function_type> m_target;
			public:
				SingleCurrier(const function::Func<function_type>& target) : m_target(target) {}

				Binder operator()(Arg0&& param) const {
					return Binder(m_target, param);
				}

			};

			template <std::size_t N, typename = void>
			class MultiCurrier {
			protected:
				function::Func<function_type> m_target;
			public:
				MultiCurrier(const function::Func<function_type>& target) : m_target(target) {}

				Binder operator()(Arg0&& param) const {
					return Binder(m_target, std::forward<Arg0>(param));
				}
			};

			template <typename V>
			class MultiCurrier<1, V> {
			protected:
				function::Func<function_type> m_target;
			public:
				MultiCurrier(const function::Func<function_type>& target) : m_target(target) {}

				Binder operator()(Arg0&& param) const {
					return Binder(m_target, std::forward<Arg0>(param));
				}
			};
		};



		template <typename R, typename ...Args>
		typename Currying<R(Args...)>::Currier make_curry(R(*function)(Args...)) {
			return typename Currying<R(Args...)>::Currier(function);
		}

		template <typename C, typename R, typename ...Args>
		typename Currying<R(Args...)>::Currier make_curry(C* obj, R(C::*function)(Args...)) {
			return typename Currying<R(Args...)>::Currier(function::Func<R(Args...)>(obj, function));
		}

		template <typename R, typename ...Args>
		typename Currying<R(Args...)>::Currier make_curry(R(&function)(Args...)) {
			return typename Currying<R(Args...)>::Currier(function);
		}

		template <typename R, typename ...Args>
		typename Currying<R(Args...)>::Currier make_single_curry(R(*function)(Args...)) {
			return typename Currying<R(Args...)>::SingleCurrier(function);
		}

		template <typename C, typename R, typename ...Args>
		typename Currying<R(Args...)>::Currier make_single_curry(C* obj, R(C::*function)(Args...)) {
			return typename Currying<R(Args...)>::SingleCurrier(function::Func<R(Args...)>(obj, function));
		}

		template <typename R, typename ...Args>
		typename Currying<R(Args...)>::Currier make_single_curry(R(&function)(Args...)) {
			return typename Currying<R(Args...)>::SingleCurrier(function);
		}

		template <std::size_t N, typename R, typename ...Args>
		typename Currying<R(Args...)>::Currier make_multi_curry(R(*function)(Args...)) {
			return typename Currying<R(Args...)>::template MultiCurrier<N>(function);
		}

		template <std::size_t N, typename C, typename R, typename ...Args>
		typename Currying<R(Args...)>::Currier make_multi_curry(C* obj, R(C::*function)(Args...)) {
			return typename Currying<R(Args...)>::template MultiCurrier<N>(function::Func<R(Args...)>(obj, function));
		}

		template <std::size_t N, typename R, typename ...Args>
		typename Currying<R(Args...)>::Currier make_multi_curry(const R(&function)(Args...)) {
			return typename Currying<R(Args...)>::template MultiCurrier<N>(function);
		}

		template <typename C>
		auto make_curry(const C& function) {
			using T = sn_Assist::sn_function_traits::function_traits<C>;
			using FT = typename T::function_type;
			return typename Currying<FT>::Currier(function);
		}

		template <typename C>
		auto make_single_curry(const C& function) {
			using T = sn_Assist::sn_function_traits::function_traits<C>;
			using FT = typename T::function_type;
			return typename Currying<FT>::SingleCurrier(function);
		}

		template <std::size_t N, typename C>
		auto make_multi_curry(const C& function) {
			using T = sn_Assist::sn_function_traits::function_traits<C>;
			using FT = typename T::function_type;
			return typename Currying<FT>::template MultiCurrier<N>(function);
		}

		template <typename F>
		class CurryProxy {
			F fn;

			template <typename C, typename Arg, typename ...Args>
			auto call_args(C&& cfn, Arg&& arg, Args&&... args) {
				auto pcfn = cfn(std::forward<Arg>(arg));
				return call_args(pcfn, std::forward<Args>(args)...);
			}

			template <typename C, typename Arg>
			auto call_args(C&& cfn, Arg&& arg) {
				return cfn(std::forward<Arg>(arg));
			}

		public:
			CurryProxy(const F& function)
				: fn(function) {}
			CurryProxy(F&& function)
				: fn(std::move(function)) {}
			template <typename ...Args>
			auto operator()(Args&&... args) {
                auto cfn = make_multi_curry<sizeof...(args)>(fn);
                auto pcfn = call_args(cfn, std::forward<Args>(args)...);
                return CurryProxy<decltype(pcfn)>(std::move(pcfn));
			}
            auto value() {
                return fn();   
            }
		};

		template <typename C>
		auto make_curry_proxy(C&& function) {
			return CurryProxy(std::forward<C>(function));
		}
	}

	namespace template_currying {
		
		/*
			Usage:
				template <typename ...LArgs>
				using AC = TypeCurry_t<Op, Args...>;
				
				using T = AC<Args...>;

			Or more general:
				template <typename ...LArgs>
				using T = typename TypeCurryProxy<LArgs...>::template type<Op, Args...>;
		*/


		template <template <typename ...TArgs> typename Op, typename ...FArgs>
		struct TypeCurry {
			/*
				template <typename ...LArgs>
				struct LTemplate {
					using type = Op<Args..., LArgs...>;
				};
				template <typename ...LArgs>
				using type = typename LTemplate<LArgs...>::type;
			*/
			template <typename ...LArgs>
			using type = Op<FArgs..., LArgs...>;
		};

		template <typename ...LArgs>
		struct TypeCurryProxy {
			template <template <typename ...TArgs> typename Op, typename ...FArgs>
			using type = typename TypeCurry<Op, FArgs...>::template type<LArgs...>;    
		};

// No, standard forbids this
// ref: https://www.zhihu.com/question/61944238
#ifdef __GNUC__
			template <typename ...LArgs>
			template <template <typename ...TArgs> typename Op, typename ...FArgs>
			using TypeCurry_t = typename TypeCurry<Op, FArgs...>::template     type<LArgs...>;
#endif


		// wrapper
		template <template <typename ...> typename Op>
		struct TypeLazy {
			template <typename ...Ts>
			struct Lazy {
				using lazy = TypeTrue;
				template <typename ...Ts_>
				using type = Op<Ts_...>;
			};
			template <typename ...Ts>
			using type = Lazy<Ts...>;
		};

		template <typename ...Ts>
		struct TypeLazyProxy {
			template <template <typename ...> typename Op>
			using type = typename TypeCurry<Op>::template type<Ts...>;    
		};

#ifdef __GNUC__
			template <typename ...Ts>
			template <template <typename ...> typename Op>
			using TypeLazy_t = typename TypeLazy<Op>::template type<Ts...>;
#endif

	}

#if defined(__GNUC__) && __GNUC__ >= 7
	// ref: https://vittorioromeo.info/index/blog/cpp17_curry.html
	// TODO: reduce capture
	namespace lambda_currying {
		
		template <typename TF, typename Tp>
		decltype(auto) for_tuple(TF&& f, Tp&& tp) {
			return sn_Assist::sn_tuple_assist::invoke_tuple(
				[&f](auto&&... xs) {
					std::initializer_list<int>{(std::forward<TF>(f)(std::forward<decltype(xs)>(xs)), 0)...};
				},
				std::forward<Tp>(tp)
			);
		}

		template <typename T>
		class forward_capture_tuple : private std::tuple<T> {
		private:
			using decay_t = std::decay_t<T>;
			using base_t = std::tuple<T>;
		protected:
			constexpr auto& as_tuple() const noexcept {
				return static_cast<base_t&>(*this);
			}
			constexpr const auto& as_tuple() noexcept {
				return static_cast<const base_t&>(*this);
			}
			template <typename TF, typename V = std::enable_if_t<!std::is_same<std::decay_t<TF>, forward_capture_tuple>::value>>
			constexpr forward_capture_tuple(TF&& x) noexcept(std::is_nothrow_constructible<base_t, TF>::value)
				: base_t(std::forward<TF>(x)) {}
		public:
			constexpr auto& get() noexcept {
				return std::get<0>(as_tuple());
			}
			constexpr const auto& get() const & noexcept {
				return std::get<0>(as_tuple());
			}
			constexpr auto get() && noexcept(std::is_move_constructible<decay_t>::value) {
				return std::move(std::get<0>(as_tuple()));
			}	
		};

		template <typename T>
		class forward_capture_wrapper : public forward_capture_tuple<T> {
		private:
			using base_t = forward_capture_tuple<T>;
		public:
			template <typename TF, typename V = std::enable_if_t<!std::is_same<std::decay_t<TF>, forward_capture_wrapper>::value>>
			constexpr forward_capture_wrapper(TF&& x) noexcept(std::is_nothrow_constructible<base_t, TF>::value)
				: base_t(std::forward<TF>(x)) {}
		};

		template <typename T>
		class forward_copy_capture_wrapper : public forward_capture_tuple<T> {
		private:
			using base_t = forward_capture_tuple<T>;
		public:
			template <typename TF>
			constexpr forward_capture_wrapper(TF&& x) noexcept(std::is_nothrow_constructible<base_t, TF>::value)
				: base_t(x) {}
		};

		template <typename T>
		constexpr auto make_forward_capture(T&& x) noexcept(forward_capture_wrapper<T>(std::forward<T>(x))) {
			return forward_capture_wrapper<T>(std::forward<T>(x));
		}
		template <typename T>
		constexpr auto make_forward_copy_capture(T&& x) noexcept(forward_copy_capture_wrapper<T>(std::forward<T>(x))) {
			return forward_copy_capture_wrapper<T>(std::forward<T>(x));
		}

		template <typename ...Ts>
		constexpr auto make_forward_capture_as_tuple(Ts&&... xs) noexcept(std::make_tuple(forward_capture_wrapper(std::forward<Ts>(xs))...)) {
			return std::make_tuple(forward_capture_wrapper(std::forward<Ts>(xs))...);
		}
		template <typename ...Ts>
		constexpr auto make_forward_copy_capture_as_tuple(Ts&&... xs) noexcept(std::make_tuple(forward_copy_capture_wrapper(std::forward<Ts>(xs))...)) {
			return std::make_tuple(forward_copy_capture_wrapper(std::forward<Ts>(xs))...);
		}

		template <typename TF, typename ...TFCs>
		constexpr decltype(auto) apply_forward_capture(TF&& f, TFCs&&... fcs) {
			return sn_Assist::sn_tuple_assist::invoke_tuple(
				[&f](auto&&... xs) -> decltype(auto) {
					return std::forward<TF>(f)(std::forward<decltype(xs)>(xs).get()...);
				},
				std::tuple_cat(std::forward<TFCs>(fcs)...)
			);
		}

		template <typename T, typename U>
		using copy_referenceness_impl =
			std::conditional_t<!std::is_reference<U>::value, T,
				std::conditional_t<std::is_lvalue_reference<U>{},
					std::add_lvalue_reference_t<T>,
					std::conditional_t<std::is_rvalue_reference<U>{},
						std::add_rvalue_reference_t<T>, void>>>;

		template <typename T, typename U>
		using as_if_forwarded = std::conditional_t<!std::is_reference<U>::value,
			std::add_rvalue_reference_t<std::remove_reference_t<T>>,
		    copy_referenceness<T, U>>;

		// forwards the passed argument with the same value category of the potentially-unrelated specified type. It basically copies the "lvalue/rvalue-ness" of the user-provided template parameter and applies it to its argument.
		template <typename U, typename T>
		inline constexpr decltype(auto) forward_like(T&& x) noexcept {
			static_assert(!(std::is_rvalue_reference<T>::value &&
							std::is_lvalue_reference<U>::value));
			return static_cast<as_if_forwarded<T, U>>(x);
		}

		template <typename TF>
		constexpr decltype(auto) make_lambda_curry(TF&& f) {
			if constexpr (std::is_callable_v<TF&&()>) {
				return std::forward<TF>(f)();
			}
			else {
				return [xf = make_forward_capture(std::forward<TF>(f))](auto&&... ps) mutable constexpr {
					return make_lambda_curry(
						[
							partial_pack = make_forward_capture_as_tuple(std::forward<decltype(ps)>(ps)...),
							yf = std::move(xf)
						]
						(auto&&... xs) constexpr
							-> decltype(forward_like<TF>(xf.get())(std::forward<decltype(ps)>(ps)..., std::forward<decltype(xs)>(xs)...)) {
								return apply_forward_capture(
									[&yf](auto&&... ys) constexpr
										-> decltype(forward_like<TF>(yf.get())(std::forward<decltype(ys)>(ys)...)) {
										return forward_like<TF>(yf.get())(std::forward<decltype(ys)>(ys)...);
									},
									partial_pack, make_forward_capture_as_tuple(std::forward<decltype(xs)>(xs)...);
								)
						}
					);
				};
			}
		}	
	}
#endif

	namespace combining {
		template <typename A, typename B, typename C>
		class Combining {};

		template <typename R1, typename R2, typename R, typename ...Args>
		class Combining<R1(Args...), R2(Args...), R(R1, R2)> {
		protected:
			function::Func<R1(Args...)> m_func1;
			function::Func<R2(Args...)> m_func2;
			function::Func<R(R1, R2)> m_converter;
		public:
			typedef R1 first_function_type(Args...);
			typedef R2 second_function_type(Args...);
			typedef R converter_function_type(R1, R2);
			typedef R function_type(Args...);

			using FT1 = first_function_type;
			using FT2 = second_function_type;
			using FCT = converter_function_type;
			using FT = function_type;


			Combining(const FT1& func1, const FT2& func2, const FCT& converter)
				: m_func1(func1)
				, m_func2(func2)
				, m_converter(converter) {}

			R operator()(Args&&... args) const {
				return m_converter(m_func1(std::forward<Args>(args)...), m_func2(std::forward<Args>(args)...));
			}

		};

		using function::Func;
		using currying::make_curry;

		template <typename F1, typename F2, typename C>
		Func<typename Combining<F1, F2, C>::function_type> make_combine(Func<C> converter, Func<F1> func1, Func<F2> func2) {
			return Combining<F1, F2, C>(func1, func2, converter);
		}

		template <typename T>
		Func<Func<T>(Func<T>, Func<T>)> make_homomorphy_combine(const Func<typename Func<T>::result_type(typename Func<T>::result_type, typename Func<T>::result_type)> converter) {
			using R = typename Func<T>::result_type;
			// Or use bind to make converter the 3rd argument
			return make_curry<Func<T>(Func<R(R, R)>, Func<T>, Func<T>)>(make_combine)(converter);
		}


	}

	namespace lazy {
		using function::Func;
		template <typename T, typename ...Args>
		class Lazy {
		protected:
			class Internal {
			public:
				Func<T(Args...)> m_evaluator;
				std::tuple<const Args&...> m_args;
				T m_value;
				bool m_isEvaluated;
			};
			observer_ptr<Internal> m_internal = nullptr;
		public:
			Lazy() {}
			Lazy(const Func<T(Args...)>& eval, Args&&... args) {
				m_internal = new Internal;
				m_internal->m_isEvaluated = false;
				m_internal->m_evaluator = eval;
				m_internal->m_args = std::make_tuple(std::forward<Args>(args)...);
			}
			Lazy(const T& value) {
				m_internal = new Internal;
				m_internal->m_isEvaluated = true;
				m_internal->m_value = value;
			}
			Lazy(const Lazy& rhs) : m_internal(rhs.m_internal) {}
			Lazy(Lazy&& rhs) : m_internal(std::move(m_internal)) {}

			/*
			Lazy& operator=(const Func<T(Args...)>& eval, Args&&... args) {
				m_internal = new Internal;
				m_internal->m_isEvaluated = false;
				m_internal->m_evaluator = eval;
				m_internal->m_args = std::make_tuple(std::forward<Args>(args)...);
				return *this;
			}*/
			
			Lazy& operator=(const T& value) {
				m_internal = new Internal;
				m_internal->m_isEvaluated = true;
				m_internal->m_value = value;
				return *this;
			}

			Lazy& operator=(const Lazy& rhs) {
				m_internal = new Internal;
				m_internal->m_isEvaluated = rhs.m_internal->m_isEvaluated;
				m_internal->m_value = rhs.m_internal->m_value;
				m_internal->m_evaluator = rhs.m_internal->m_evaluator;
				m_internal->m_args = rhs.m_internal->m_args;
				return *this;
			}
			Lazy& operator=(Lazy&& rhs) {
				m_internal = std::move(rhs.m_internal);
				rhs.m_internal = nullptr;
				return *this;
			}

			const T& value() const {
				if (m_internal != nullptr) {
					if (!m_internal->m_isEvaluated) {
						m_internal->m_isEvaluated = true;
						m_internal->m_value = sn_Assist::sn_tuple_assist::invoke_tuple(m_internal->m_evaluator, m_internal->m_args);
					}
					return m_internal_value;
				}
				throw std::bad_exception("Uninitialized internal value");
			}

			const bool is_evaluated() const {
				return m_internal->m_isEvaluated();
			}

			const bool is_available() const {
				return m_internal != nullptr;
			}
		};

		template <typename T>
		Lazy<T> make_lazy(T&& value) {
			return Lazy(std::forward<T>(value));
		}

		template <typename R, typename ...Args>
		Lazy<R, Args...> make_lazy(Func<R(Args...)>&& func, Args&&... args) {
			return Lazy(std::forward<Func(R(Args...))> func, std::forward<Args>(args)...);
		}

	}

	//Note: functor wrapper is just like AOP
	namespace functor_wrapper {

#define SN_MAKE_FUNCTOR(func_name) \
	class sn_fn_##func_name { \
	public : \
		template <typename ...Args> \
		auto operator()(Args&&... args) const { \
			return func_name(std::forward<Args>(args)...); \
		} \
	} \


		template <typename F, typename Before = std::tuple<>, typename After = std::tuple<>>
		class FunctorWrapper {
		private:
			F m_func;
			Before m_before;
			After m_after;
		public:
			FunctorWrapper(F&& f) : m_func(std::forward<F>(f)), m_before(std::tuple<>()), m_after(std::tuple<>()) {}
			FunctorWrapper(const F& f, const Before& b, const After& a) : m_func(f), m_before(b), m_after(a) {}

			template <typename ...Args>
			auto operator()(Args&&... args) const
				-> decltype(sn_Assist::sn_tuple_assist::invoke_tuple(
					m_func, std::tuple_cat(
						m_before,
						std::make_tuple(std::forward<Args>(args)...),
						m_after))) {
				return sn_Assist::sn_tuple_assist::invoke_tuple(m_func, std::tuple_cat(m_before, std::make_tuple(std::forward<Args>(args)...), m_after));
			}

			template <typename T>
			auto add_aspect_head(T&& param) const {
				using Before_t = decltype(std::tuple_cat(std::make_tuple(std::forward<T>(param)), m_before));
				return FunctorWrapper<F, Before_t, After>(m_func, std::tuple_cat(std::make_tuple(std::forward<T>(param)), m_before), m_after);
			}

			template <typename T>
			auto add_aspect_inplace(T&& param) const {
				using Before_t = decltype(std::tuple_cat(m_before, std::make_tuple(std::forward<T>(param))));
				return FunctorWrapper<F, Before_t, After>(m_func, std::tuple_cat(m_before, std::make_tuple(std::forward<T>(param))), m_after);
			}

			template <typename T>
			auto add_aspect_tail(T&& param) const {
				using After_t = decltype(std::tuple_cat(std::make_tuple(std::forward<T>(param)), m_after));
				return FunctorWrapper<F, Before, After_t>(m_func, m_before, std::tuple_cat(std::make_tuple(std::forward<T>(param)), m_after));
			}
		};

		template <typename F>
		auto make_functor_wrapper(F&& f) {
			return FunctorWrapper<F>(std::forward<F>(f));
		}

		// 120 >> 3 >> wrapper_map << 2 << 3 << 20
		// (wrapper_map + 2 + 3) << 4
		// 1 | (add << 4 << 6) | print = 11
		template <typename F, typename Arg>
		auto operator+(const FunctorWrapper<F>& pa, Arg&& arg) {
			return pa.template add_aspect_inplace<Arg>(std::forward<Arg>(arg));
		}

		template <typename F, typename Arg>
		auto operator<<(const FunctorWrapper<F>& pa, Arg&& arg) {
			return pa.template add_aspect_tail<Arg>(std::forward<Arg>(arg));
		}

		template <typename Arg, typename F>
		auto operator>>(Arg&& arg, const FunctorWrapper<F>& pa) {
			return pa.template add_aspect_head<Arg>(std::forward<Arg>(arg));
		}

#define SN_MAKE_FUNCTOR_WRAPPER(wrapper_name, func_name) \
	SN_MAKE_FUNCTOR(func_name) \
	const auto wrapper_name = make_functor_wrapper(sn_fn_##func_name());


	}

	namespace maybe_just {
		template <typename T>
		class maybe {
		private:
			sn_Type::optional::Optional<T> m_optional;
		public:
			maybe() : m_optional() {}
			maybe(T&& value) : m_optional(std::forward<T>(value)) {}
			maybe(const T& value) : m_optional(value) {}

			template <typename F>
			auto operator()(F&& f) const -> maybe<decltype(f(std::declval<T>()))> {
				using result_type = decltype(f(std::declval<T>()));
				if (!m_optional.is_init())
					return maybe<result_type>();
				return maybe<result_type>(f(m_optional.value()));
			}

			template <typename U>
			T value_or(U&& another_value) {
				return m_optional.value_or(std::forward<U>(another_value));
			}
		};

		// maybe<int>() | print || 2 -> 2
		// just(2) | print || 4 -> print(4)
		template <typename T, typename F>
		inline auto operator|(maybe<T>&& m, F&& f) -> decltype(m(f)) {
			return m(std::forward<F>(f));
		}

		template <typename T, typename U>
		inline auto operator||(maybe<T>&& m, U&& v) -> decltype(m.value_or(std::forward<U>(v))) {
			return m.value_or(std::forward<U>(v));
		}

		template <typename T>
		maybe<T> just(T&& t) {
			return maybe<T>(std::forward<T>(t));
		}

	}

	namespace pipeline {
		// 2 | add
		template <typename T, typename F>
		auto operator|(T&& param, const F& f) -> decltype(f(std::forward<T>(param))) {
			return f(std::forward<T>(param));
		}

		template <typename ...Fns>
		class Chain {
		private:
			const std::tuple<Fns...> m_funcs;
			const static size_t m_tpSize = sizeof...(Fns);

			template <typename Arg, std::size_t I>
			auto call_impl(Arg&& arg, const std::index_sequence<I>&) const
				-> decltype(std::get<I>(m_funcs)(std::forward<Arg>(arg))) {
				return std::get<I>(m_funcs)(std::forward<Arg>(arg));
			}

			template <typename Arg, std::size_t I, std::size_t ...Is>
			auto call_impl(Arg&& arg, const std::index_sequence<I, Is...>&) const
				-> decltype(call_impl(std::get<I>(m_funcs)(std::forward<Arg>(arg)), std::index_sequence<Is...>{})) {
				return call_impl(std::get<I>(m_funcs)(std::forward<Arg>(arg)), std::index_sequence<Is...>{});
			}

			template <typename Arg>
			auto call(Arg&& arg) const -> decltype(call_impl(std::forward<Arg>(arg), std::make_index_sequence<m_tpSize>{})) {
				return call_impl(std::forward<Arg>(arg), std::make_index_sequence<m_tpSize>{});
			}

		public:

			Chain() : m_funcs(std::tuple<>{}) {}
			Chain(std::tuple<Fns...> funcs) : m_funcs(funcs) {}
			
			template <typename F>
			inline auto add(const F& f) const {
				return Chain<Fns..., F>(std::tuple_cat(m_funcs, std::make_tuple(f)));
			}

			template <typename Arg>
			inline auto operator()(Arg&& arg) const -> decltype(call(std::forward<Arg>(arg))) {
				return call(std::forward<Arg>(arg));
			}

		};

		// f = ChainHead | [](auto s){} | wrapper_map | ...
		// {2, 3} | f
		template <typename ...Fns, typename F>
		inline auto operator|(Chain<Fns...>&& chain, F&& f) {
			return Chain.add(std::forward<F>(f));
		}

	}

	namespace operation {

		template <typename T, typename ...Args, template <typename...> typename C, typename F>
		auto map(const C<T, Args...>& container, const F& f) -> C<decltype(f(std::declval<T>()))> {
			using result_type = decltype(f(std::declval<T>()));
			C<result_type> res;
			for (const auto& item : container)
				res.push_back(f(item));
			return res;
		}

		template <typename T, typename ...Args, template <typename...> typename C, typename F, typename R>
		auto reduce(const C<T, Args...>& container, const R& start, const F& f) {
			R res = start;
			for (const auto& item : container)
				res = f(res, item);
			return res;
		}

		template <typename T, typename ...Args, template <typename...> typename C, typename F>
		auto filter(const C<T, Args...>& container, const F& f) {
			C<T, Args...> res;
			for (const auto& item : container)
				if (f(item))
					res.push_back(item);
			return res;
		}

	}

	// ref: https://codereview.stackexchange.com/questions/161875/c-constexpr-trampoline
	namespace trampoline {
		template <typename T>
		constexpr auto trampoline(T&& init) {
			std::decay_t<T> answer{ std::forward<T>(init) };
			while (!answer.finished())
				answer = answer.tail_call();
			return answer.value();
		}

		template <typename F, typename Args>
		class TrampolineFunc {
			F m_func;
			Args m_args;
		public:
			using args_t = Args;
			constexpr TrampolineFunc() = default;
			constexpr TrampolineFunc(TrampolineFunc&& other) = default;
			constexpr TrampolineFunc(const TrampolineFunc& other) = default;
			constexpr TrampolineFunc& operator=(TrampolineFunc rhs) {
				m_func = std::move(rhs);
				assign_args(std::move(rhs.m_args), std::make_index_sequence<std::tuple_size<Args>::value>{});
			}
			constexpr TrampolineFunc(F&& fn, Args&& args)
				: m_func(std::forward<F>(fn), std::forward<Args>(args)) {}
			constexpr auto operator()() const {
				return m_func(std::forward<Args>(args));
			}
		private:
			template <std::size_t ...I>
			constexpr void assign_args(Args args, std::index_sequence<I...>) {
				std::initializer_list<int>{(std::get<I>(m_args) = std::get<I>(args), 0)...};
			}
		};

		template <typename F, typename Args>
		constexpr auto make_trampoline_func(F&& fn, Args&& args) {
			return TrampolineFunc<F, Args>(std::forward<F>(fn), std::forward<Args>(args));
		}

		template <typename T, typename U>
		class TrampolineWrapper {
			using F = TrampolineFunc<TrampolineWrapper<T, U>(*)(U), U>;
			bool m_finished;
			F m_func;
			T m_value;
		public:
			constexpr TrampolineWrapper() = default;
			constexpr TrampolineWrapper(TrampolineWrapper&&) = default;
			constexpr TrampolineWrapper(const TrampolineWrapper&) = default;
			
			constexpr TrampolineWrapper(bool finished, F&& fn, T value)
				: m_finished(finished), m_func(fn), m_value(value) {}
			constexpr bool finished() const {
				return m_finished;
			}
			constexpr T value() const {
				return m_value;
			}
			constexpr TrampolineWrapper tail_call() const {
				return m_func();
			}
		};

		template <typename T, typename F>
		constexpr auto make_trampoline_wrapper(bool finished, T&& value, F&& fn) {
			return TrampolineWrapper<std::decay_t<T>, typename F::args_t>{
				finished, std::forward<F>(fn), std::forward<T>(value)
			};
		}

		template <typename F, typename T, typename ...Args>
		constexpr auto make_trampoline(F&& fn, T&& init, Args&&... args) {
			return trampoline(
				make_trampoline_wrapper(
					false,
					std::forward<T>(init),
					make_trampoline_func(
						std::forward<F>(fn),
						std::make_tuple(std::forward<Args>(args)...)
					)
				)
			);
		}

		/*
		Usage:
			constexpr auto F(std::tuple<Args...> args) {
				// By condition
				return make_trampoline_wrapper<T, std::tuple<Args...>>(...);
				-> true
				-> false
			}
		*/
	}

	namespace lens {
		template <typename V, typename F>
		struct Lens {
			std::function<F(V)> getter;
			std::function<V(V, F)> setter;

			V apply(const V& v, const std::function<F(F)>& trans) const {
				auto z = getter(value);
				return setter(v, trans(z));
			}
		}

		

		template <typename LS

		template <typename V, typename F>
		Lens<V, F> make_lens(const std::function<F(V)>& getter, const std::function<V(V, F)>& setter) {
			Lens<V, F> lens;
			lens.getter = getter;
			lens.setter = setter;
			return lens;
		}

		template <typename ...Ls>
		struct LensStack {
			template <typename Reroll, typename Lx>
			void reroll_impl(Reroll& rerolled, const Lx& lx) const {
				rerolled.m_lens = lx;
			}
			template <typename F>
			F get(const V& f) const {
				return f;
			}
			template <typename F>
			F apply(const F& value, const std::function<F(F)>& trans) const {
				return trans(value);
			}
		};

		template <typename L, typename ...Ls>
		struct LensStack<L, Ls...> : LensStack<Ls...> {
			using base_type = LensStack<Ls...>;
			LensStack(L lens, Ls... tail)
				: LensStack<Ls...>(tail...),
				m_lens(lens) {}
			
			template <typename Reroll, typename Lx>
			void reroll_impl(Reroll& rerolled, const Lx& lx) const {
				rerolled.m_lens = m_lens;
				m_base.reroll(rerolled.m_base, lx);
			}

			template <typename Lx>
			LensStack<L, Ls..., Lx> reroll(const Lx& lx) const {
				LensStack<L, Ls..., Lx> rerolled;
				rerolled.m_lens = m_lens;
				m_base.reroll_impl(rerolled.m_base, lx);
				return rerolled;
			}

			template <typename V>
			auto get(const V& value) const {
				auto z = m_lens.getter(value);
				return m_base.get(z);
			}

			template <typename V, typename F>
			V apply(const V& value, const std::function<F(F)>& trans) const {
				auto z = m_lens.getter(value);
				return m_lens.setter(value, m_base.apply(z, trans));
			}
			
			base_type& m_base = static_cast<base_type&>(*this);
			L m_lens;
			
		}

		// Implement of infix
		struct Proxy {};
		constexpr const Proxy proxy;
		template <typename V, typename F>
		LensStack<Lens<V, F>> operator< (const Lens<V, F>& lens, const Proxy&) {
			return LensStack<Lens<V, F>>(lens);
		}

		template <typename LS, typename L>
		typename LS::template reroll_type<L> operator> (const LS& stack, const L& lens) {
			return stack.reroll(lens);
		}

		#define to < sn_Function::lens::proxy >

		template <typename V, typename F>
		F view(const Lens<V, F>& lens, const V& v) {
			return lens.getter(v);
		}

		template <typename LS, typename V>
		auto view(const LS& ls, const V& v) {
			return ls.get(v);
		}

		template <typename V, typename F>
		V set(const Lens<V, F>& lens, const V& v, const F& f) {
			return lens.setter(v, f);
		}

		template <typename LS, typename V, typename F>
		V set(const LS& ls, const V& v, const F& f) {
			return ls.apply(v, [=](F){ return f; });
		}

		template <typename V, typename F>
		V over(const Lens<V, F>& lens, const V& v, const std::function<F(F)>& trans) {
			auto z = lens.getter(v);
			return lens.setter(v, trans(z));
		}

		template <typename LS, typename V, typename F>
		V set(const LS& ls, const V& v, const std::function<F(F)>& trans) {
			return ls.apply(v, trans);
		}
	}

	namespace YCombinator {
		// ref: https://gist.github.com/kenpusney/9274727

		// auto-version
		template <typename TF>
		class YBuilder {
		private:
			TF partial;
		public:
			YBuilder(TF partial_)
				: partial(std::forward<TF>(partial_)) {}	
			template <typename ...Args>
			decltype(auto) operator()(Args&&... args) {
				return partial(
					std::ref(*this),
					std::forward<Args>(args)...
				);
			}
		};

		template <typename R, typename ...Args>
		class YBuilder<R(Args...)> {
		private:
			using FT = std::function<R(std::function<R(Args...)>, Args...)>;
			std::function<R(std::function<R(Args...)>, Args...)> partial;
		public:
			YBuilder(FT partial_)
				: partial(std::forward<FT>(partial_)) {}
			
			R operator(Args&&... args) {
				return partial(
					[this](Args&&... args) {
						return this->operator()(std::forward<Args>(args)...);
					}, std::forward<Args>(args)...);
				)
			}
		};

		template <typename C>
		auto Y(C&& partial) {
			using Tr = sn_Assist::sn_function_traits::function_traits;
			using FT = typename Tr<C>::function_type;
			return YBuilder<FT>(std::forward<C>(partial));
		}

		// lambda with auto self
		template <typename C>
		auto YA(C&& partial) {
			return YBuilder<C>(std::forward<C>(partial));
		}

		/*
		Usage:
			auto fact = Y([](std::function<int(int)> self, int n) -> int {
				return n < 2 ? 1 : n * self(n - 1)
			});
		*/

	}

#ifdef SN_ENABLE_CPP17_EXPERIMENTAL
	namespace failable {
		
		template <typename R, typename ... P>
		auto make_failable(R (*f)(P ... ps)) {
			return [f](std::optional<P> ... xs) -> std::optional<R> {
				if ((xs && ...)) {
					return {f(*(xs)...)};
				} else {
					return {};
				}
			};
		}

		template <typename F>
		auto make_failable(F&& f) {
			return [f = std::forward<F>(f)](auto&&... xs)
				-> std::optional<decltype(std::forward<F>(f).get()(*(std::forward<decltype(xs)>(xs))...))> {
				if ((xs && ...)) {
					return std::forward<F>(f).get()(*(std::forward<decltype(xs)>(xs)...));
				} else {
					return {};
				}
			}
		}

		// join
		template <typename R, typename ... P>
		auto make_failable(std::optional<R> (*f)(P ... ps)) {
			return [f](std::optional<P> ... xs) -> std::optional<R> {
				if ((xs && ...)) {
					return f(*(xs)...);
				} else {
					return {};
				}
			};
		}
	}


#endif


	using function::make_func;
	using currying::make_curry;
	using currying::make_single_curry;
	using currying::make_multi_curry;
	using currying::make_curry_proxy;
	using lambda_currying::make_lambda_curry;
	using combining::make_combine;
	using combining::make_homomorphy_combine;
	using maybe_just::maybe;
	using maybe_just::just;
	using lazy::make_lazy;
	using functor_wrapper::make_functor_wrapper;
	using trampoline::make_trampoline;
	using trampoline::make_trampoline_wrapper;
	using YCombinator::Y;
	using YCombinator::YA;
	using template_currying::TPC;

	const auto ChainHead = pipeline::Chain<>();
}





#endif