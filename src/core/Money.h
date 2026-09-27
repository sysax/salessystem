#pragma once

// Dinero exacto: la BD guarda REAL (compatibilidad legada) pero el dominio y
// los cálculos de negocio (IVA, mora, pagos mixtos) trabajan en céntimos
// enteros (qint64) para evitar errores de coma flotante. Sustituye a Decimal.
// Fase 2 (cierre): redondeo half-up vía std::llround, operadores completos y
// parse/validación centralizada para la entrada de UI (máx. 2 decimales,
// negativos solo donde aplique, tope configurable).
#include <QLocale>
#include <QString>
#include <QtGlobal> // qint64, qAbs
#include <cmath>    // std::llround
#include <limits>

class Money
{
  public:
    // 1 COP = 100 céntimos
    static constexpr qint64 CentsPerCop = 100;
    static constexpr int MaxDecimals = 2;
    // Tope operativo por importe (≈ 9.9e12 COP; evita overflow en qty*price).
    static constexpr qint64 MaxCents = 999999999999900LL;
    // IVA Colombia (DIAN) — compatibilidad; la tasa real viene de Settings.
    static constexpr int IvaPercent = 19;

    constexpr Money() : m_cents(0)
    {
    }
    constexpr explicit Money(qint64 cents) : m_cents(cents)
    {
    }

    // Redondeo half-away (llround; en importes ≥ 0 equivale a half-up).
    // Las bases imponibles no son negativas: el descuento se resta como
    // monto positivo (ver withRate).
    static Money fromCop(double cop)
    {
        if (!std::isfinite(cop))
            return Money(0);
        const double clamped
            = qBound(-static_cast<double>(MaxCents) / CentsPerCop,
                     cop, static_cast<double>(MaxCents) / CentsPerCop);
        return Money(static_cast<qint64>(std::llround(clamped * CentsPerCop)));
    }
    static constexpr Money fromCents(qint64 cents)
    {
        return Money(cents);
    }
    static Money zero()
    {
        return Money(0);
    }

    constexpr qint64 cents() const
    {
        return m_cents;
    }
    double toCop() const
    {
        return static_cast<double>(m_cents) / CentsPerCop;
    }
    bool isZero() const
    {
        return m_cents == 0;
    }
    bool isNegative() const
    {
        return m_cents < 0;
    }
    bool isPositive() const
    {
        return m_cents > 0;
    }

    // Entrada de UI: acepta "10.000", "10000.50", "10.000,50", "$ 10.000",
    // con máx. 2 decimales. Miles estilo es_CO ('.') y decimal (',').
    // allowNegative=false ⇒ rechaza negativos (precios, pagos, caja).
    // Devuelve false + error en ES si no es válido o supera max.
    static bool tryParse(const QString &text, bool allowNegative, Money &out, QString &error,
                         Money max = Money(MaxCents))
    {
        QString t = text.trimmed();
        if (t.isEmpty()) {
            error = QStringLiteral("Importe vacío");
            return false;
        }
        // Quitar símbolo moneda y espacios internos.
        t.remove(u'$');
        t.remove(u' ');
        t.remove(QStringLiteral("COP"), Qt::CaseInsensitive);
        if (t.isEmpty()) {
            error = QStringLiteral("Importe vacío");
            return false;
        }
        const int dots = t.count(u'.');
        const int commas = t.count(u',');
        QString norm = t;
        if (dots > 0 && commas > 0) {
            // es_CO: '.' miles, ',' decimal → "1.850.000,50" ⇒ "1850000.50"
            norm.remove(u'.');
            norm.replace(u',', u'.');
        } else if (commas > 0) {
            // Solo comas: decimal es_CO → "10,5" ⇒ "10.5"
            norm.replace(u',', u'.');
        } else if (dots > 1) {
            // Varios puntos sin coma: miles → "1.850.000" ⇒ "1850000"
            norm.remove(u'.');
        } else if (dots == 1) {
            // Un punto: decimal solo si hay 1–2 dígitos al final,
            // si no es miles ("1.850" ⇒ 1850).
            const int pos = norm.lastIndexOf(u'.');
            const int dec = norm.size() - pos - 1;
            if (dec < 1 || dec > 2)
                norm.remove(u'.');
        }
        // Formato canónico: [-]d+(\.\d{1,2})?
        bool neg = false;
        QString chk = norm;
        if (chk.startsWith(u'-')) {
            neg = true;
            chk = chk.mid(1);
        } else if (chk.startsWith(u'+')) {
            chk = chk.mid(1);
        }
        if (chk.isEmpty()) {
            error = QStringLiteral("Importe inválido");
            return false;
        }
        const int ppos = chk.indexOf(u'.');
        const QString intPart = ppos < 0 ? chk : chk.left(ppos);
        const QString decPart = ppos < 0 ? QString() : chk.mid(ppos + 1);
        auto allDigits = [](const QString &s) {
            if (s.isEmpty())
                return false;
            for (const QChar c : s) {
                if (!c.isDigit())
                    return false;
            }
            return true;
        };
        if (!allDigits(intPart) || decPart.size() > 2
            || (!decPart.isEmpty() && !allDigits(decPart))
            || (ppos >= 0 && chk.count(u'.') != 1)) {
            error = QStringLiteral("Máximo 2 decimales (ej. 10.000,50)");
            return false;
        }
        bool ok = false;
        const double v = norm.toDouble(&ok);
        if (!ok || !std::isfinite(v)) {
            error = QStringLiteral("Importe inválido");
            return false;
        }
        if (neg && !allowNegative) {
            error = QStringLiteral("El importe no puede ser negativo");
            return false;
        }
        // Detección de >2 decimales reales ("10.999" normalizado ya falla;
        // "10,999" llega como "10.999" y se rechaza aquí por redondeo oculto).
        const qint64 c = static_cast<qint64>(std::llround(v * CentsPerCop));
        if (std::abs(v * CentsPerCop - static_cast<double>(c)) > 0.001 + 1e-9) {
            error = QStringLiteral("Máximo 2 decimales (ej. 10.000,50)");
            return false;
        }
        const Money m(c);
        if (!allowNegative && m.isNegative()) {
            error = QStringLiteral("El importe no puede ser negativo");
            return false;
        }
        if (qAbs(m.cents()) > qAbs(max.cents())) {
            error = QStringLiteral("Importe máximo excedido");
            return false;
        }
        out = m;
        error.clear();
        return true;
    }

    static bool isValid(const QString &text, bool allowNegative = false)
    {
        Money m;
        QString e;
        return tryParse(text, allowNegative, m, e);
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
    constexpr Money operator-() const
    {
        return Money(-m_cents);
    }
    // Cantidad entera (unidades) — exacto.
    constexpr Money operator*(qint64 k) const
    {
        return Money(m_cents * k);
    }
    constexpr Money operator*(int k) const
    {
        return Money(m_cents * static_cast<qint64>(k));
    }
    // Cantidad fraccionaria (granel, ej. 0.35 kg): redondeo al céntimo.
    Money operator*(double qty) const
    {
        return Money(static_cast<qint64>(std::llround(static_cast<double>(m_cents) * qty)));
    }
    friend Money operator*(double qty, Money m)
    {
        return m * qty;
    }
    Money &operator+=(Money o)
    {
        m_cents += o.m_cents;
        return *this;
    }
    Money &operator-=(Money o)
    {
        m_cents -= o.m_cents;
        return *this;
    }
    constexpr bool operator==(Money o) const
    {
        return m_cents == o.m_cents;
    }
    constexpr bool operator!=(Money o) const
    {
        return m_cents != o.m_cents;
    }
    constexpr bool operator<(Money o) const
    {
        return m_cents < o.m_cents;
    }
    constexpr bool operator<=(Money o) const
    {
        return m_cents <= o.m_cents;
    }
    constexpr bool operator>(Money o) const
    {
        return m_cents > o.m_cents;
    }
    constexpr bool operator>=(Money o) const
    {
        return m_cents >= o.m_cents;
    }

  private:
    qint64 m_cents = 0;
};

#include <QMetaType>
Q_DECLARE_METATYPE(Money)
