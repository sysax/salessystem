#pragma once

// Dinero exacto: la BD guarda REAL pero los cálculos de negocio (IVA,
// mora, pagos mixtos) se hacen en céntimos enteros para evitar errores de
// coma flotante. Sustituye a Decimal de Python.
#include <QLocale>
#include <QString>
#include <QtGlobal> // qint64, qAbs (QtTypes no existe como header top-level en Qt 6.4)
#include <cmath>    // std::llround

class Money
{
  public:
    // 1 COP = 100 céntimos
    static constexpr qint64 CentsPerCop = 100;
    // IVA Colombia (DIAN)
    static constexpr int IvaPercent = 19;

    constexpr Money() : m_cents(0)
    {
    }
    constexpr explicit Money(qint64 cents) : m_cents(cents)
    {
    }

    static Money fromCop(double cop)
    {
        return Money(static_cast<qint64>(cop * CentsPerCop + (cop >= 0 ? 0.5 : -0.5)));
    }
    static Money fromCents(qint64 cents)
    {
        return Money(cents);
    }

    constexpr qint64 cents() const
    {
        return m_cents;
    }
    double toCop() const
    {
        return static_cast<double>(m_cents) / CentsPerCop;
    }

    // "$ 1.850.000" (es_CO, sin decimales salvo que haya céntimos).
    // Overload con símbolo/decimales de SettingsService (Fase 0 multinegocio);
    // el overload simple conserva el default COP para compatibilidad.
    static QString format(qint64 cents, const QString &symbol, int decimals)
    {
        static const QLocale co(QLocale::Spanish, QLocale::Colombia);
        const qint64 major = cents / CentsPerCop;
        const int rem = static_cast<int>(qAbs(cents % CentsPerCop));
        QString s = symbol + QStringLiteral(" ") + co.toString(major);
        if (decimals > 0) {
            s += co.decimalPoint()
                 + QString::number(rem).rightJustified(2, u'0').left(decimals).leftJustified(
                     decimals, u'0');
        } else if (rem != 0) {
            s += co.decimalPoint() + QString::number(rem).rightJustified(2, u'0');
        }
        return s;
    }
    static QString format(qint64 cents)
    {
        return format(cents, QStringLiteral("$"), 0);
    }
    QString format() const
    {
        return format(m_cents);
    }

    // IVA incluido a partir del neto, con redondeo al céntimo.
    // Tasa explícita (multinegocio: la tasa viene de SettingsService, nunca
    // del default COP/19). Redondeo round-half-up en positivos (llround =
    // half-away; las bases imponibles no son negativas: el descuento se
    // resta como monto positivo).
    static qint64 taxCents(qint64 baseCents, double ratePct)
    {
        return static_cast<qint64>(std::llround(static_cast<double>(baseCents) * ratePct / 100.0));
    }
    static Money withRate(Money net, double ratePct)
    {
        return Money(net.m_cents + taxCents(net.m_cents, ratePct));
    }
    // Compatibilidad: IVA Colombia 19 % (los tests y tickets legacy lo usan).
    static Money withIva(Money net, int percent = IvaPercent)
    {
        return withRate(net, static_cast<double>(percent));
    }

    constexpr Money operator+(Money o) const
    {
        return Money(m_cents + o.m_cents);
    }
    constexpr Money operator-(Money o) const
    {
        return Money(m_cents - o.m_cents);
    }
    constexpr Money operator*(qint64 k) const
    {
        return Money(m_cents * k);
    }
    constexpr bool operator==(Money o) const
    {
        return m_cents == o.m_cents;
    }
    constexpr bool operator<(Money o) const
    {
        return m_cents < o.m_cents;
    }

  private:
    qint64 m_cents = 0;
};
