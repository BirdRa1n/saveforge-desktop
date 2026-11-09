#pragma once

#include "saveforge/core/Error.h"

#include <cassert>
#include <utility>
#include <variant>

namespace saveforge {

// Resultado de uma operação que pode falhar: contém um valor T ou um Error.
// Substitui std::expected (C++23) enquanto o projeto usa C++20.
template <typename T>
class [[nodiscard]] Result {
public:
    Result(T value) : m_data(std::in_place_index<0>, std::move(value)) {}
    Result(Error error) : m_data(std::in_place_index<1>, std::move(error)) {}

    [[nodiscard]] bool ok() const noexcept { return m_data.index() == 0; }
    explicit operator bool() const noexcept { return ok(); }

    [[nodiscard]] T &value() & { assert(ok()); return std::get<0>(m_data); }
    [[nodiscard]] const T &value() const & { assert(ok()); return std::get<0>(m_data); }
    [[nodiscard]] T &&value() && { assert(ok()); return std::get<0>(std::move(m_data)); }

    [[nodiscard]] const Error &error() const & { assert(!ok()); return std::get<1>(m_data); }

    template <typename U>
    [[nodiscard]] T valueOr(U &&fallback) const &
    {
        return ok() ? std::get<0>(m_data) : static_cast<T>(std::forward<U>(fallback));
    }

private:
    std::variant<T, Error> m_data;
};

// Especialização para operações sem valor de retorno.
template <>
class [[nodiscard]] Result<void> {
public:
    Result() = default;
    Result(Error error) : m_error(std::move(error)), m_ok(false) {}

    [[nodiscard]] bool ok() const noexcept { return m_ok; }
    explicit operator bool() const noexcept { return m_ok; }
    [[nodiscard]] const Error &error() const & { assert(!m_ok); return m_error; }

private:
    Error m_error;
    bool m_ok = true;
};

} // namespace saveforge
