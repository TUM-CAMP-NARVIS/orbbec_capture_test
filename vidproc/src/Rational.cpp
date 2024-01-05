#include "tcn/vpf/Rational.h"
#include "tcn/vpf/ffmpeg.h"

namespace tcn::vpf {
    Rational::Rational(const AVRational& other): data_{std::make_unique<AVRational>()}
    {
        data_->num = other.num;
        data_->den = other.den;
    };

    Rational::Rational(int numerator, int denominator): data_{std::make_unique<AVRational>()}
    {
        data_->num = numerator;
        data_->den = denominator;
    };

    Rational::Rational(const Rational& other): data_{std::make_unique<AVRational>()}
    {
        data_->num = other.data_->num;
        data_->den = other.data_->den;
    }

    Rational::Rational(Rational&& other) noexcept: data_{std::move(other.data_)}{}

    Rational& Rational::operator=(const Rational& other)
    {
        data_->num = other.data_->num;
        data_->den = other.data_->den;
        return *this;
    }

    Rational& Rational::operator=(const AVRational& other)
    {
        data_->num = other.num;
        data_->den = other.den;
        return *this;
    }

    Rational& Rational::operator=(Rational&& other) noexcept
    {
        if(data_ != other.data_)
        {
            data_.reset();
            data_ = std::move(other.data_);
        }
        return *this;
    }

    int Rational::numerator() const
    {
        return data_->num;
    }

    int Rational::denominator() const
    {
        return data_->den;
    }

    double Rational::toDouble() const
    {
        return av_q2d(*data_);
    }

/// Returns true if lhs == rhs. Behaviour is undefined if one of the input is 0/0.
    bool operator==(const Rational& lhs, const Rational& rhs)
    {
        return av_cmp_q(*lhs.data_, *rhs.data_) == 0;
    }

/// Returns true if lhs != rhs. Behaviour is undefined if one of the input is 0/0.
    bool operator!=(const Rational& lhs, const Rational& rhs)
    {
        return !(lhs == rhs);
    }

/// Return true if lhs > rhs. Behaviour is undefined if one of the input is 0/0.
    bool operator>(const Rational& lhs, const Rational& rhs)
    {
        return av_cmp_q(*lhs.data_, *rhs.data_) > 0;
    }

/// Return true if lhs <= rhs. Behaviour is undefined if one of the input is 0/0.
    bool operator<=(const Rational& lhs, const Rational& rhs)
    {
        return !(lhs > rhs);
    }

/// Return true if lhs < rhs. Behaviour is undefined if one of the input is 0/0.
    bool operator<(const Rational& lhs, const Rational& rhs)
    {
        return av_cmp_q(*lhs.data_, *rhs.data_) < 0;
    }

/// Return true if lhs >= rhs. Behaviour is undefined if one of the input is 0/0.
    bool operator>=(const Rational& lhs, const Rational& rhs)
    {
        return !(lhs < rhs);
    }

/// Returns lhs + rhs.
    Rational operator+(const Rational& lhs, const Rational& rhs)
    {
        auto rational = av_add_q(*lhs.data_, *rhs.data_);
        return Rational{rational.num, rational.den};
    }

/// Returns lhs = lhs + rhs.
    Rational& Rational::operator+=(const Rational& rhs)
    {
        *this = *this + rhs;
        return *this;
    }

/// Returns lhs - rhs.
    Rational operator-(const Rational& lhs, const Rational& rhs)
    {
        auto rational = av_sub_q(*lhs.data_, *rhs.data_);
        return Rational{rational.num, rational.den};
    }

/// Returns lhs = lhs - rhs.
    Rational& Rational::operator-=(const Rational& rhs)
    {
        *this = *this - rhs;
        return *this;
    }

/// Returns lhs x rhs.
    Rational operator*(const Rational& lhs, const Rational& rhs)
    {
        auto rational = av_mul_q(*lhs.data_, *rhs.data_);
        return Rational{rational.num, rational.den};
    }

/// Returns lhs = lhs * rhs.
    Rational& Rational::operator*=(const Rational& rhs)
    {
        *this = *this * rhs;
        return *this;
    }

/// Returns lhs / rhs.
    Rational operator/(const Rational& lhs, const Rational& rhs)
    {
        auto rational = av_div_q(*lhs.data_, *rhs.data_);
        return Rational{rational.num, rational.den};
    }

/// Returns lhs = lhs / rhs.
    Rational& Rational::operator/=(const Rational& rhs)
    {
        *this = *this / rhs;
        return *this;
    }

}

