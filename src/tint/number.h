// Copyright 2021 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_TINT_NUMBER_H_
#define SRC_TINT_NUMBER_H_

#include <stdint.h>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <ostream>

#include "src/tint/utils/compiler_macros.h"
#include "src/tint/utils/result.h"

// Forward declaration
namespace tint {
/// Number wraps a integer or floating point number, enforcing explicit casting.
template <typename T>
struct Number;
}  // namespace tint

namespace tint::detail {
/// An empty structure used as a unique template type for Number when
/// specializing for the f16 type.
struct NumberKindF16 {};

/// Helper for obtaining the underlying type for a Number.
template <typename T>
struct NumberUnwrapper {
    /// When T is not a Number, then type defined to be T.
    using type = T;
};

/// NumberUnwrapper specialization for Number<T>.
template <typename T>
struct NumberUnwrapper<Number<T>> {
    /// The Number's underlying type.
    using type = typename Number<T>::type;
};

}  // namespace tint::detail

namespace tint {

/// Evaluates to true iff T is a floating-point type or is NumberKindF16.
template <typename T>
constexpr bool IsFloatingPoint =
    std::is_floating_point_v<T> || std::is_same_v<T, detail::NumberKindF16>;

/// Evaluates to true iff T is an integer type.
template <typename T>
constexpr bool IsInteger = std::is_integral_v<T>;

/// Evaluates to true iff T is an integer type, floating-point type or is NumberKindF16.
template <typename T>
constexpr bool IsNumeric = IsInteger<T> || IsFloatingPoint<T>;

/// Resolves to the underlying type for a Number.
template <typename T>
using UnwrapNumber = typename detail::NumberUnwrapper<T>::type;

/// NumberBase is a CRTP base class for Number<T>
template <typename NumberT>
struct NumberBase {
    /// @returns value of type `Number<T>` with the highest value for that type.
    static NumberT Highest() { return NumberT(NumberT::kHighestValue); }
    /// @returns value of type `Number<T>` with the lowest value for that type.
    static NumberT Lowest() { return NumberT(NumberT::kLowestValue); }
    /// @returns value of type `Number<T>` with the smallest value for that type.
    static NumberT Smallest() { return NumberT(NumberT::kSmallestValue); }
    /// @returns value of type `Number<T>` that represents NaN for that type.
    static NumberT NaN() {
        return NumberT(std::numeric_limits<UnwrapNumber<NumberT>>::quiet_NaN());
    }
    /// @returns value of type `Number<T>` that represents infinity for that type.
    static NumberT Inf() { return NumberT(std::numeric_limits<UnwrapNumber<NumberT>>::infinity()); }
};

/// Number wraps a integer or floating point number, enforcing explicit casting.
template <typename T>
struct Number : NumberBase<Number<T>> {
    static_assert(IsNumeric<T>, "Number<T> constructed with non-numeric type");

    /// type is the underlying type of the Number
    using type = T;

    /// Highest finite representable value of this type.
    static constexpr type kHighestValue = std::numeric_limits<type>::max();

    /// Lowest finite representable value of this type.
    static constexpr type kLowestValue = std::numeric_limits<type>::lowest();

    /// Smallest positive normal value of this type.
    static constexpr type kSmallestValue =
        std::is_integral_v<type> ? 0 : std::numeric_limits<type>::min();

    /// Smallest positive subnormal value of this type, 0 for integral type.
    static constexpr type kSmallestSubnormalValue =
        std::is_integral_v<type> ? 0 : std::numeric_limits<type>::denorm_min();

    /// Constructor. The value is zero-initialized.
    Number() = default;

    /// Constructor.
    /// @param v the value to initialize this Number to
    template <typename U>
    explicit Number(U v) : value(static_cast<T>(v)) {}

    /// Constructor.
    /// @param v the value to initialize this Number to
    template <typename U>
    explicit Number(Number<U> v) : value(static_cast<T>(v.value)) {}

    /// Conversion operator
    /// @returns the value as T
    operator T() const { return value; }

    /// Negation operator
    /// @returns the negative value of the number
    Number operator-() const { return Number(-value); }

    /// Assignment operator
    /// @param v the new value
    /// @returns this Number so calls can be chained
    Number& operator=(T v) {
        value = v;
        return *this;
    }

    /// The number value
    type value = {};
};

/// Writes the number to the ostream.
/// @param out the std::ostream to write to
/// @param num the Number
/// @return the std::ostream so calls can be chained
template <typename T>
inline std::ostream& operator<<(std::ostream& out, Number<T> num) {
    return out << num.value;
}

/// The partial specification of Number for f16 type, storing the f16 value as float,
/// and enforcing proper explicit casting.
template <>
struct Number<detail::NumberKindF16> : NumberBase<Number<detail::NumberKindF16>> {
    /// C++ does not have a native float16 type, so we use a 32-bit float instead.
    using type = float;

    /// Highest finite representable value of this type.
    static constexpr type kHighestValue = 65504.0f;  // 2¹⁵ × (1 + 1023/1024)

    /// Lowest finite representable value of this type.
    static constexpr type kLowestValue = -65504.0f;

    /// Smallest positive normal value of this type.
    /// binary16 0_00001_0000000000, value is 2⁻¹⁴.
    static constexpr type kSmallestValue = 0x1p-14f;

    /// Smallest positive subnormal value of this type.
    /// binary16 0_00000_0000000001, value is 2⁻¹⁴ * 2⁻¹⁰ = 2⁻²⁴.
    static constexpr type kSmallestSubnormalValue = 0x1p-24f;

    /// Constructor. The value is zero-initialized.
    Number() = default;

    /// Constructor.
    /// @param v the value to initialize this Number to
    template <typename U>
    explicit Number(U v) : value(Quantize(static_cast<type>(v))) {}

    /// Constructor.
    /// @param v the value to initialize this Number to
    template <typename U>
    explicit Number(Number<U> v) : value(Quantize(static_cast<type>(v.value))) {}

    /// Conversion operator
    /// @returns the value as the internal representation type of F16
    operator float() const { return value; }

    /// Negation operator
    /// @returns the negative value of the number
    Number operator-() const { return Number<detail::NumberKindF16>(-value); }

    /// Assignment operator with parameter as native floating point type
    /// @param v the new value
    /// @returns this Number so calls can be chained
    Number& operator=(type v) {
        value = Quantize(v);
        return *this;
    }

    /// Get the binary16 bit pattern in type uint16_t of this value.
    /// @returns the binary16 bit pattern, in type uint16_t, of the stored quantized f16 value. If
    /// the value is NaN, the returned value will be 0x7e00u. If the value is positive infinity, the
    /// returned value will be 0x7c00u. If the input value is negative infinity, the returned value
    /// will be 0xfc00u.
    uint16_t BitsRepresentation() const;

    /// @param value the input float32 value
    /// @returns the float32 value quantized to the smaller float16 value, through truncation of the
    /// mantissa bits (no rounding). If the float32 value is too large (positive or negative) to be
    /// represented by a float16 value, then the returned value will be positive or negative
    /// infinity.
    static type Quantize(type value);

    /// The number value, stored as float
    type value = {};
};

/// `AInt` is a type alias to `Number<int64_t>`.
using AInt = Number<int64_t>;
/// `AFloat` is a type alias to `Number<double>`.
using AFloat = Number<double>;

/// `i32` is a type alias to `Number<int32_t>`.
using i32 = Number<int32_t>;
/// `u32` is a type alias to `Number<uint32_t>`.
using u32 = Number<uint32_t>;
/// `f32` is a type alias to `Number<float>`
using f32 = Number<float>;
/// `f16` is a type alias to `Number<detail::NumberKindF16>`, which should be IEEE 754 binary16.
/// However since C++ don't have native binary16 type, the value is stored as float.
using f16 = Number<detail::NumberKindF16>;

/// Enumerator of failure reasons when converting from one number to another.
enum class ConversionFailure {
    kExceedsPositiveLimit,  // The value was too big (+'ve) to fit in the target type
    kExceedsNegativeLimit,  // The value was too big (-'ve) to fit in the target type
};

/// Writes the conversion failure message to the ostream.
/// @param out the std::ostream to write to
/// @param failure the ConversionFailure
/// @return the std::ostream so calls can be chained
std::ostream& operator<<(std::ostream& out, ConversionFailure failure);

/// Converts a number from one type to another, checking that the value fits in the target type.
/// @returns the resulting value of the conversion, or a failure reason.
template <typename TO, typename FROM>
utils::Result<TO, ConversionFailure> CheckedConvert(Number<FROM> num) {
    // Use the highest-precision integer or floating-point type to perform the comparisons.
    using T = std::conditional_t<IsFloatingPoint<UnwrapNumber<TO>> || IsFloatingPoint<FROM>,
                                 AFloat::type, AInt::type>;
    const auto value = static_cast<T>(num.value);
    if (value > static_cast<T>(TO::kHighestValue)) {
        return ConversionFailure::kExceedsPositiveLimit;
    }
    if (value < static_cast<T>(TO::kLowestValue)) {
        return ConversionFailure::kExceedsNegativeLimit;
    }
    return TO(value);  // Success
}

/// Equality operator.
/// @param a the LHS number
/// @param b the RHS number
/// @returns true if the numbers `a` and `b` are exactly equal. Also considers sign bit.
template <typename A, typename B>
bool operator==(Number<A> a, Number<B> b) {
    // Use the highest-precision integer or floating-point type to perform the comparisons.
    using T =
        std::conditional_t<IsFloatingPoint<A> || IsFloatingPoint<B>, AFloat::type, AInt::type>;
    auto va = static_cast<T>(a.value);
    auto vb = static_cast<T>(b.value);
    if constexpr (IsFloatingPoint<T>) {
        if (std::signbit(va) != std::signbit(vb)) {
            return false;
        }
    }
    return std::equal_to<T>()(va, vb);
}

/// Inequality operator.
/// @param a the LHS number
/// @param b the RHS number
/// @returns true if the numbers `a` and `b` are exactly unequal. Also considers sign bit.
template <typename A, typename B>
bool operator!=(Number<A> a, Number<B> b) {
    return !(a == b);
}

/// Equality operator.
/// @param a the LHS number
/// @param b the RHS number
/// @returns true if the numbers `a` and `b` are exactly equal.
template <typename A, typename B>
std::enable_if_t<IsNumeric<B>, bool> operator==(Number<A> a, B b) {
    return a == Number<B>(b);
}

/// Inequality operator.
/// @param a the LHS number
/// @param b the RHS number
/// @returns true if the numbers `a` and `b` are exactly unequal.
template <typename A, typename B>
std::enable_if_t<IsNumeric<B>, bool> operator!=(Number<A> a, B b) {
    return !(a == b);
}

/// Equality operator.
/// @param a the LHS number
/// @param b the RHS number
/// @returns true if the numbers `a` and `b` are exactly equal.
template <typename A, typename B>
std::enable_if_t<IsNumeric<A>, bool> operator==(A a, Number<B> b) {
    return Number<A>(a) == b;
}

/// Inequality operator.
/// @param a the LHS number
/// @param b the RHS number
/// @returns true if the numbers `a` and `b` are exactly unequal.
template <typename A, typename B>
std::enable_if_t<IsNumeric<A>, bool> operator!=(A a, Number<B> b) {
    return !(a == b);
}

/// Define 'TINT_HAS_OVERFLOW_BUILTINS' if the compiler provide overflow checking builtins.
/// If the compiler does not support these builtins, then these are emulated with algorithms
/// described in:
/// https://wiki.sei.cmu.edu/confluence/display/c/INT32-C.+Ensure+that+operations+on+signed+integers+do+not+result+in+overflow
#if defined(__GNUC__) && __GNUC__ >= 5
#define TINT_HAS_OVERFLOW_BUILTINS
#elif defined(__clang__)
#if __has_builtin(__builtin_add_overflow) && __has_builtin(__builtin_mul_overflow)
#define TINT_HAS_OVERFLOW_BUILTINS
#endif
#endif

/// @returns a + b, or an empty optional if the resulting value overflowed the AInt
inline std::optional<AInt> CheckedAdd(AInt a, AInt b) {
    int64_t result;
#ifdef TINT_HAS_OVERFLOW_BUILTINS
    if (__builtin_add_overflow(a.value, b.value, &result)) {
        return {};
    }
#else   // TINT_HAS_OVERFLOW_BUILTINS
    if (a.value >= 0) {
        if (AInt::kHighestValue - a.value < b.value) {
            return {};
        }
    } else {
        if (b.value < AInt::kLowestValue - a.value) {
            return {};
        }
    }
    result = a.value + b.value;
#endif  // TINT_HAS_OVERFLOW_BUILTINS
    return AInt(result);
}

/// @returns a + b, or an empty optional if the resulting value overflowed the AFloat
inline std::optional<AFloat> CheckedAdd(AFloat a, AFloat b) {
    auto result = a.value + b.value;
    if (!std::isfinite(result)) {
        return {};
    }
    return AFloat{result};
}

/// @returns a * b, or an empty optional if the resulting value overflowed the AInt
inline std::optional<AInt> CheckedMul(AInt a, AInt b) {
    int64_t result;
#ifdef TINT_HAS_OVERFLOW_BUILTINS
    if (__builtin_mul_overflow(a.value, b.value, &result)) {
        return {};
    }
#else   // TINT_HAS_OVERFLOW_BUILTINS
    if (a > 0) {
        if (b > 0) {
            if (a > (AInt::kHighestValue / b)) {
                return {};
            }
        } else {
            if (b < (AInt::kLowestValue / a)) {
                return {};
            }
        }
    } else {
        if (b > 0) {
            if (a < (AInt::kLowestValue / b)) {
                return {};
            }
        } else {
            if ((a != 0) && (b < (AInt::kHighestValue / a))) {
                return {};
            }
        }
    }
    result = a.value * b.value;
#endif  // TINT_HAS_OVERFLOW_BUILTINS
    return AInt(result);
}

/// @returns a * b + c, or an empty optional if the value overflowed the AInt
inline std::optional<AInt> CheckedMadd(AInt a, AInt b, AInt c) {
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80635
    TINT_BEGIN_DISABLE_WARNING(MAYBE_UNINITIALIZED);

    if (auto mul = CheckedMul(a, b)) {
        return CheckedAdd(mul.value(), c);
    }
    return {};

    TINT_END_DISABLE_WARNING(MAYBE_UNINITIALIZED);
}

}  // namespace tint

namespace tint::number_suffixes {

/// Literal suffix for abstract integer literals
inline AInt operator""_a(unsigned long long int value) {  // NOLINT
    return AInt(static_cast<int64_t>(value));
}

/// Literal suffix for abstract float literals
inline AFloat operator""_a(long double value) {  // NOLINT
    return AFloat(static_cast<double>(value));
}

/// Literal suffix for i32 literals
inline i32 operator""_i(unsigned long long int value) {  // NOLINT
    return i32(static_cast<int32_t>(value));
}

/// Literal suffix for u32 literals
inline u32 operator""_u(unsigned long long int value) {  // NOLINT
    return u32(static_cast<uint32_t>(value));
}

/// Literal suffix for f32 literals
inline f32 operator""_f(long double value) {  // NOLINT
    return f32(static_cast<double>(value));
}

/// Literal suffix for f32 literals
inline f32 operator""_f(unsigned long long int value) {  // NOLINT
    return f32(static_cast<double>(value));
}

/// Literal suffix for f16 literals
inline f16 operator""_h(long double value) {  // NOLINT
    return f16(static_cast<double>(value));
}

/// Literal suffix for f16 literals
inline f16 operator""_h(unsigned long long int value) {  // NOLINT
    return f16(static_cast<double>(value));
}

}  // namespace tint::number_suffixes

#endif  // SRC_TINT_NUMBER_H_
