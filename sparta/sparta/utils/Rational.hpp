#ifndef _H_RATIONAL
#define _H_RATIONAL

#include <iostream>
#include <sstream>
#include <inttypes.h>
#include <cassert>
#include "sparta/utils/MathUtils.hpp"

namespace sparta
{
namespace utils
{
    template <class T>
    class Rational
    {
    public:
        Rational(const T &numerator = 0, const T &denominator = 1):
            n_(numerator),
            d_(denominator)
        {
            assert(denominator != 0);
            simplify();
        }

        Rational(const Rational<T> &r):
            n_(r.n_), d_(r.d_)
        {}

        inline T getNumerator() const
        {
            return n_;
        }

        inline T getDenominator() const
        {
            return d_;
        }

        explicit inline operator T() const
        {
            assert(d_ == 1);
            return n_;
        }

        explicit inline operator double() const
        {
            return double(n_) / double(d_);
        }

        inline Rational<T>& operator=(const Rational<T> &r)
        {
            if (&r != this) {
                n_ = r.n_;
                d_ = r.d_;
            }
            return *this;
        }

        explicit inline operator std::string() const
        {
            std::stringstream ss;
            if (d_ == 0) {
                if (n_ == 0) {
                    ss << "NaN";
                } else {
                    ss << "INF";
                }
            }
            else if (n_ == 0) {
                ss << "0";
            }
            else if (d_ == 1) {
                ss << n_;
            } else {
                ss << n_ << "/" << d_;
            }
            return ss.str();
        }

        inline void print(std::ostream& os) const
        {
            os << this->operator std::string();
        }

        inline void simplify()
        {
            if (d_ != 0) {
                if (n_ == 0) {
                    d_ = 1;
                } else {
                    T g = utils::gcd(n_, d_);
                    if (g > 1) {
                        n_ /= g;
                        d_ /= g;
                    }
                }
            }
        }

        inline Rational<T> operator*(const Rational<T> &r) const
        {
            return Rational<T>(n_ * r.n_, d_ * r.d_);
        }

        inline Rational<T>& operator*=(const Rational<T> &r)
        {
            n_ *= r.n_;
            d_ *= r.d_;
            simplify();
            return *this;
        }

        inline Rational<T> operator/(const Rational<T> &r) const
        {
            return Rational<T>(n_ * r.d_, d_ * r.n_);
        }

        inline Rational<T>& operator/=(const Rational<T> &r)
        {
            if (&r != this) {
                n_ *= r.d_;
                d_ *= r.n_;
            } else {
                T x = r.n_;
                n_ *= r.d_;
                d_ *= x;
            }
            simplify();
            return *this;
        }

        inline Rational<T> operator+(const Rational<T> &r) const
        {
            if (d_ != r.d_) {
                T m = utils::lcm(d_, r.d_);
                return Rational<T>((n_ * m / d_) + (r.n_ * m / r.d_), m);
            } else {
                return Rational<T>(n_ + r.n_, d_);
            }
        }

        inline Rational<T>& operator+=(const Rational<T> &r)
        {
            if (d_ != r.d_) {
                T m = utils::lcm(d_, r.d_);
                n_ = (n_ * m / d_) + (r.n_ * m / r.d_);
                d_ = m;
            } else {
                n_ += r.n_;
            }
            simplify();
            return *this;
        }

        // Computes absolute difference
        inline Rational<T> operator-(const Rational<T> &r) const
        {
            if (d_ != r.d_) {
                T m = utils::lcm(d_, r.d_);
                return Rational<T>(utils::abs((n_ * m / d_) - (r.n_ * m / r.d_)), m);
            } else {
                return Rational<T>(utils::abs(n_ - r.n_), d_);
            }
        }

        inline Rational<T>& operator-=(const Rational<T> &r)
        {
            if (d_ != r.d_) {
                T m = utils::lcm(d_, r.d_);
                n_ = utils::abs((n_ * m / d_) - (r.n_ * m / r.d_));
                d_ = m;
            } else {
                n_ = utils::abs(n_ - r.n_);
            }
            simplify();
            return *this;
        }

        // Invert
        inline Rational<T> inv()
        {
            return Rational<T>(d_, n_);
        }

        inline bool operator==(const Rational<T> &r)
        {
            Rational<T> t(r);
            t.simplify();
            return (n_ == t.n_) && (d_ == t.d_);
        }

    private:
        T    n_;  // Numerator
        T    d_;  // Denominator
    };

    template <class T>
    inline std::ostream& operator<<(std::ostream& os, const sparta::utils::Rational<T>& r)
    {
        r.print(os);
        return os;
    }

} // utils
} // sparta
#endif
