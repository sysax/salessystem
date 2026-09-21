#pragma once

// Dinero exacto en COP: la BD guarda REAL pero los cálculos de negocio
// (IVA 19%, mora 2%, pagos mixtos) se hacen en céntimos enteros para
// evitar errores de coma flotante. Sustituye a Decimal de Python.
#include <QLocale>
#include <QString>
#include <QtTypes>

class Money
{
public:
    // 1 COP = 100 céntimos
    static constexpr qint64 CentsPerCop = 100;
    // IVA Colombia (DIAN)
    static constexpr int IvaPercent = 19;

    constexpr Money() : m_cents(0) {}
    constexpr explicit Money(qint64 cents) : m_cents(cents) {}

    static Money fromCop(double cop) {
        return Money(static_cast<qint64>(cop * CentsPerCop + (cop >= 0 ? 0.5 : -0.5)));
    }
    static Money fromCents(qint64 cents) { return Money(cents); }

    constexpr qint64 cents() const { return m_cents; }
    double toCop() const { return static_cast<double>(m_cents) / CentsPerCop; }

    // "$ 1.850.000" (es_CO, sin decimales salvo que haya céntimos)
    static QString format(qint64 cents) {
        static const QLocale co(QLocale::Spanish, QLocale::Colombia);
        const qint64 cop = cents / CentsPerCop;
        const int rem = static_cast<int>(qAbs(cents % CentsPerCop));
        QString s = QStringLiteral("$ ") + co.toString(cop);
        if (rem != 0)
            s += co.decimalPoint() + QString::number(rem).rightJustified(2, u'0');
        return s;
    }
    QString format() const { return format(m_cents); }

    // IVA incluido a partir del neto, con redondeo al céntimo
    static Money withIva(Money net, int percent = IvaPercent) {
        return Money((net.m_cents * (100 + percent) + 50) / 100);
    }

    constexpr Money operator+(Money o) const { return Money(m_cents + o.m_cents); }
    constexpr Money operator-(Money o) const { return Money(m_cents - o.m_cents); }
    constexpr Money operator*(qint64 k) const { return Money(m_cents * k); }
    constexpr bool operator==(Money o) const { return m_cents == o.m_cents; }
    constexpr bool operator<(Money o) const { return m_cents < o.m_cents; }

private:
    qint64 m_cents = 0;
};
