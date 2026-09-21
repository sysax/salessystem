#pragma once

// Resultado explícito de operaciones de servicio. Sustituye al decorador
// @handle_errors de utils/error_handler.py: en vez de tragar excepciones,
// cada servicio retorna Result<T> con error legible para la UI.
#include <QString>
#include <utility>
#include <variant>

template <typename T>
class Result
{
public:
    static Result success(T value) { return Result(true, std::move(value), {}); }
    static Result failure(QString error) { return Result(false, T{}, std::move(error)); }

    bool ok() const { return m_ok; }
    const T &value() const { return m_value; }
    T &value() { return m_value; }
    const QString &error() const { return m_error; }

private:
    Result(bool ok, T value, QString error)
        : m_ok(ok), m_value(std::move(value)), m_error(std::move(error)) {}

    bool m_ok = false;
    T m_value{};
    QString m_error;
};

// Para operaciones que solo pueden fallar (alta/baja, pagos, 2FA…)
using StatusResult = Result<std::monostate>;
