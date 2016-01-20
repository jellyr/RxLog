/*
 * Copyright Krzysztof Sinica 2015.
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */
#ifndef RXLOG_V1_RXLOG_
#define RXLOG_V1_RXLOG_
#include <ostream>
#include <sstream>
#include <string>
#include <rxcpp/rx-observable.hpp>
#include <rxcpp/rx-subjects.hpp>

namespace rxlog { inline namespace v1 {
namespace detail {
struct base_expr_tag {};

template<typename T>
struct is_base_expr : public std::is_base_of<
    base_expr_tag
  , typename std::remove_reference<T>::type
> {};

template<typename T>
struct base_expr : public base_expr_tag {
    const T& self() const { return static_cast<const T&>(*this); }
    
    virtual ~base_expr() {}

    template<typename Ctx>
    decltype(auto) operator()(Ctx&& ctx) const {
        return self()(std::forward<Ctx>(ctx));
    }
};

struct terminal_tag {};

template<typename T>
struct terminal : public base_expr<terminal<T>> {
    terminal() : op{} {}

    terminal(T&& op) : op(std::forward<T>(op)) {}

    template<typename Ctx>
    decltype(auto) operator()(Ctx&& ctx) const {
        return std::forward<Ctx>(ctx)(terminal_tag{}, *this);
    }

    decltype(auto) operator()() const { return op; }

private:
    T op;
};

template<typename Tag, typename T, typename U>
struct binary_expr : public base_expr<binary_expr<Tag, T, U>> {
    binary_expr(T&& op1, U&& op2)
      : op1{std::forward<T>(op1)}, op2{std::forward<U>(op2)} {}

    template<typename Ctx>
    decltype(auto) operator()(Ctx&& ctx) const {
        return std::forward<Ctx>(ctx)(Tag{}, op1(std::forward<Ctx>(ctx)),
                                             op2(std::forward<Ctx>(ctx)));
    }

private:
    T op1;
    U op2;
};

template<typename Tag, typename T, typename U>
auto make_binary_expr(T&& op1, U&& op2) {
    return binary_expr<Tag, T, U>{std::forward<T>(op1), std::forward<U>(op2)};
}

template<typename T>
auto make_terminal(T&& op) { return terminal<T>{std::forward<T>(op)}; }

struct left_shift_tag {};

template<
    typename T
  , typename U
  , typename std::enable_if<is_base_expr<T>::value &&
                            is_base_expr<U>::value>::type* = nullptr
> auto operator<<(T&& op1, U&& op2) {
    return make_binary_expr<left_shift_tag>(std::forward<T>(op1),
                                            std::forward<U>(op2));
}

template<
    typename T
  , typename U
  , typename std::enable_if<is_base_expr<T>::value &&
                            !is_base_expr<U>::value>::type* = nullptr
> auto operator<<(T&& op1, U&& op2) {
    return std::forward<T>(op1) << make_terminal(std::forward<U>(op2));
}

struct default_context {
    template<typename T, typename U>
    decltype(auto) operator()(detail::left_shift_tag, T&& op1, U&& op2) const {
        return std::forward<T>(op1) << std::forward<U>(op2);
    }

    template<typename T>
    decltype(auto) operator()(terminal_tag, T&& op) const {
        return std::forward<T>(op)();
    }
};

struct ignore {};

template<typename CharT, typename TraitsT>
std::basic_ostream<CharT, TraitsT>&
operator<<(std::basic_ostream<CharT, TraitsT>& output, ignore) {
    return output;
}
} // detail

enum class severity {
    trace,
    debug,
    info,
    warning,
    error,
    fatal
};

template<typename CharT, typename TraitsT = std::char_traits<CharT>>
struct basic_record {
    const severity severity;
    const std::basic_string<CharT, TraitsT> message;
};
    
using record = basic_record<char>;
using wrecord = basic_record<wchar_t>;
    
template<typename CharT, typename TraitsT>
bool operator==(const basic_record<CharT, TraitsT>& lhs,
                const basic_record<CharT, TraitsT>& rhs) {
    return lhs.severity == rhs.severity && lhs.message == rhs.message;
}

template<typename CharT, typename TraitsT>
auto make_record(severity severity,
                 std::basic_string<CharT, TraitsT>&& message) {
    return basic_record<CharT, TraitsT>{
        severity, std::forward<decltype(message)>(message)};
}

struct sink_placeholder {};

using sink_placeholder_terminal = detail::terminal<sink_placeholder>;

template<typename CharT, typename TraitsT = std::char_traits<CharT>>
class basic_logger {
public:
    struct context : public detail::default_context {
        context() {
            sink.exceptions(std::ios_base::failbit | std::ios_base::badbit);
        }

        using default_context::operator();

        std::basic_ostream<CharT, TraitsT>&
        operator()(detail::terminal_tag, sink_placeholder_terminal) {
            return sink;
        }

        auto operator()(detail::terminal_tag, detail::terminal<severity> op) {
            level = op(); return detail::ignore{};
        }

        std::basic_stringstream<CharT, TraitsT> sink{};
        severity level{severity::info};
    };

    ~basic_logger() {
        auto subscriber = subject_.get_subscriber();
        if (subscriber.is_subscribed()) {
            subscriber.on_completed();
        }
    }

    template<typename T>
    basic_logger& operator=(const detail::base_expr<T>& expression) {
        auto subscriber = subject_.get_subscriber();
        if (subscriber.is_subscribed()) {
            try {
                context ctx{};
                expression(ctx);
                subscriber.on_next(make_record(ctx.level, ctx.sink.str()));
            } catch(...) {
                subscriber.on_error(std::current_exception());
            }
        }
        return *this;
    }
    
    rxcpp::observable<basic_record<CharT, TraitsT>> on_record() const {
        return subject_.get_observable();
    }

private:
    rxcpp::subjects::subject<basic_record<CharT, TraitsT>> subject_;
};

using logger = basic_logger<char>;
using wlogger = basic_logger<wchar_t>;

#define LOG_IMPL(a, b) a = ::rxlog::sink_placeholder_terminal{} << b
#define LOG_TRACE(a) LOG_IMPL(a, ::rxlog::severity::trace)
#define LOG_DEBUG(a) LOG_IMPL(a, ::rxlog::severity::debug)
#define LOG_INFO(a) LOG_IMPL(a, ::rxlog::severity::info)
#define LOG_WARNING(a) LOG_IMPL(a, ::rxlog::severity::warning)
#define LOG_ERROR(a) LOG_IMPL(a, ::rxlog::severity::error)
#define LOG_FATAL(a) LOG_IMPL(a, ::rxlog::severity::fatal)
} // v1
} // rxlog
#endif // RXLOG_V1_RXLOG_