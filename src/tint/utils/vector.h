// Copyright 2022 The Tint Authors.
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

#ifndef SRC_TINT_UTILS_VECTOR_H_
#define SRC_TINT_UTILS_VECTOR_H_

#include <stddef.h>
#include <stdint.h>
#include <array>
#include <iterator>
#include <utility>
#include <vector>

#include "src/tint/utils/bitcast.h"

namespace tint::utils {

/// Forward declarations
template <typename>
class VectorRef;
template <typename>
class ConstVectorRef;

}  // namespace tint::utils

namespace tint::utils {

namespace detail {

/// A slice represents a contigious array of elements of type T.
template <typename T>
struct Slice {
    /// The pointer to the first element in the slice
    T* data = nullptr;

    /// The total number of elements in the slice
    size_t len = 0;

    /// The total capacity of the backing store for the slice
    size_t cap = 0;

    /// Index operator
    /// @param i the element index. Must be less than `len`.
    /// @returns a reference to the i'th element.
    T& operator[](size_t i) { return data[i]; }

    /// Index operator
    /// @param i the element index. Must be less than `len`.
    /// @returns a reference to the i'th element.
    const T& operator[](size_t i) const { return data[i]; }

    /// @returns a reference to the first element in the vector
    T& Front() { return data[0]; }

    /// @returns a reference to the first element in the vector
    const T& Front() const { return data[0]; }

    /// @returns a reference to the last element in the vector
    T& Back() { return data[len - 1]; }

    /// @returns a reference to the last element in the vector
    const T& Back() const { return data[len - 1]; }

    /// @returns a pointer to the first element in the vector
    T* begin() { return data; }

    /// @returns a pointer to the first element in the vector
    const T* begin() const { return data; }

    /// @returns a pointer to one past the last element in the vector
    T* end() { return data + len; }

    /// @returns a pointer to one past the last element in the vector
    const T* end() const { return data + len; }

    /// @returns a reverse iterator starting with the last element in the vector
    auto rbegin() { return std::reverse_iterator<T*>(end()); }

    /// @returns a reverse iterator starting with the last element in the vector
    auto rbegin() const { return std::reverse_iterator<const T*>(end()); }

    /// @returns the end for a reverse iterator
    auto rend() { return std::reverse_iterator<T*>(begin()); }

    /// @returns the end for a reverse iterator
    auto rend() const { return std::reverse_iterator<const T*>(begin()); }
};

}  // namespace detail

/// Vector is a small-object-optimized, dynamically-sized vector of contigious elements of type T.
///
/// Vector will fit `N` elements internally before spilling to heap allocations. If `N` is greater
/// than zero, the internal elements are stored in a 'small array' held internally by the Vector.
///
/// Vectors can be copied or moved.
///
/// Copying a vector will either copy to the 'small array' if the number of elements is equal to or
/// less than N, otherwise elements will be copied into a new heap allocation.
///
/// Moving a vector will reassign ownership of the heap-allocation memory, if the source vector
/// holds its elements in a heap allocation, otherwise a copy will be made as described above.
///
/// Vector is optimized for CPU performance over memory efficiency. For example:
/// * Moving a vector that stores its elements in a heap allocation to another vector will simply
///   assign the heap allocation, even if the target vector can hold the elements in its 'small
///   array'. This reduces memory copying, but may incur additional memory usage.
/// * Resizing, or popping elements from a vector that has spilled to a heap allocation does not
///   revert back to using the 'small array'. Again, this is to reduce memory copying.
template <typename T, size_t N = 0>
class Vector {
  public:
    /// Type of `T`.
    using value_type = T;

    /// Constructor
    Vector() = default;

    /// Constructor
    /// @param length the initial length of the vector. Elements will be zero-initialized.
    explicit Vector(size_t length) {
        Reserve(length);
        for (size_t i = 0; i < length; i++) {
            new (&impl_.slice.data[i]) T{};
        }
        impl_.slice.len = length;
    }

    /// Constructor
    /// @param length the initial length of the vector
    /// @param value the value to copy into each element of the vector
    Vector(size_t length, const T& value) {
        Reserve(length);
        for (size_t i = 0; i < length; i++) {
            new (&impl_.slice.data[i]) T{value};
        }
        impl_.slice.len = length;
    }

    /// Constructor
    /// @param elements the elements to place into the vector
    explicit Vector(std::initializer_list<T> elements) {
        Reserve(elements.size());
        for (auto& el : elements) {
            new (&impl_.slice.data[impl_.slice.len++]) T{el};
        }
    }

    /// Copy constructor
    /// @param other the vector to copy
    Vector(const Vector& other) { Copy(other.impl_.slice); }

    /// Move constructor
    /// @param other the vector to move
    Vector(Vector&& other) { MoveOrCopy(VectorRef<T>(std::move(other))); }

    /// Copy constructor (differing N length)
    /// @param other the vector to copy
    template <size_t N2>
    Vector(const Vector<T, N2>& other) {
        Copy(other.impl_.slice);
    }

    /// Move constructor (differing N length)
    /// @param other the vector to move
    template <size_t N2>
    Vector(Vector<T, N2>&& other) {
        MoveOrCopy(VectorRef<T>(std::move(other)));
    }

    /// Move constructor from a mutable vector reference
    /// @param other the vector reference to move
    Vector(VectorRef<T>&& other) {  // NOLINT(runtime/explicit)
        MoveOrCopy(std::move(other));
    }

    /// Copy constructor from an immutable vector reference
    /// @param other the vector reference to copy
    Vector(const ConstVectorRef<T>& other) {  // NOLINT(runtime/explicit)
        Copy(other.slice_);
    }

    /// Destructor
    ~Vector() { ClearAndFree(); }

    /// Assignment operator
    /// @param other the vector to copy
    /// @returns this vector so calls can be chained
    Vector& operator=(const Vector& other) {
        if (&other != this) {
            Copy(other.impl_.slice);
        }
        return *this;
    }

    /// Move operator
    /// @param other the vector to move
    /// @returns this vector so calls can be chained
    Vector& operator=(Vector&& other) {
        if (&other != this) {
            MoveOrCopy(VectorRef<T>(std::move(other)));
        }
        return *this;
    }

    /// Assignment operator (differing N length)
    /// @param other the vector to copy
    /// @returns this vector so calls can be chained
    template <size_t N2>
    Vector& operator=(const Vector<T, N2>& other) {
        Copy(other.impl_.slice);
        return *this;
    }

    /// Move operator (differing N length)
    /// @param other the vector to copy
    /// @returns this vector so calls can be chained
    template <size_t N2>
    Vector& operator=(Vector<T, N2>&& other) {
        MoveOrCopy(VectorRef<T>(std::move(other)));
        return *this;
    }

    /// Index operator
    /// @param i the element index. Must be less than `len`.
    /// @returns a reference to the i'th element.
    T& operator[](size_t i) { return impl_.slice[i]; }

    /// Index operator
    /// @param i the element index. Must be less than `len`.
    /// @returns a reference to the i'th element.
    const T& operator[](size_t i) const { return impl_.slice[i]; }

    /// @return the number of elements in the vector
    size_t Length() const { return impl_.slice.len; }

    /// @return the number of elements that the vector could hold before a heap allocation needs to
    /// be made
    size_t Capacity() const { return impl_.slice.cap; }

    /// Reserves memory to hold at least `new_cap` elements
    /// @param new_cap the new vector capacity
    void Reserve(size_t new_cap) {
        if (new_cap > impl_.slice.cap) {
            auto* old_data = impl_.slice.data;
            impl_.Allocate(new_cap);
            for (size_t i = 0; i < impl_.slice.len; i++) {
                new (&impl_.slice.data[i]) T(std::move(old_data[i]));
                old_data[i].~T();
            }
            impl_.Free(old_data);
        }
    }

    /// Resizes the vector to the given length, expanding capacity if necessary.
    /// New elements are zero-initialized
    /// @param new_len the new vector length
    void Resize(size_t new_len) {
        Reserve(new_len);
        for (size_t i = impl_.slice.len; i > new_len; i--) {  // Shrink
            impl_.slice.data[i - 1].~T();
        }
        for (size_t i = impl_.slice.len; i < new_len; i++) {  // Grow
            new (&impl_.slice.data[i]) T{};
        }
        impl_.slice.len = new_len;
    }

    /// Copies all the elements from `other` to this vector, replacing the content of this vector.
    /// @param other the
    template <typename T2, size_t N2>
    void Copy(const Vector<T2, N2>& other) {
        Copy(other.impl_.slice);
    }

    /// Clears all elements from the vector, keeping the capacity the same.
    void Clear() {
        for (size_t i = 0; i < impl_.slice.len; i++) {
            impl_.slice.data[i].~T();
        }
        impl_.slice.len = 0;
    }

    /// Appends a new element to the vector.
    /// @param el the element to copy to the vector.
    void Push(const T& el) {
        if (impl_.slice.len >= impl_.slice.cap) {
            Grow();
        }
        new (&impl_.slice.data[impl_.slice.len++]) T(el);
    }

    /// Appends a new element to the vector.
    /// @param el the element to move to the vector.
    void Push(T&& el) {
        if (impl_.slice.len >= impl_.slice.cap) {
            Grow();
        }
        new (&impl_.slice.data[impl_.slice.len++]) T(std::move(el));
    }

    /// Appends a new element to the vector.
    /// @param args the arguments to pass to the element constructor.
    template <typename... ARGS>
    void Emplace(ARGS&&... args) {
        if (impl_.slice.len >= impl_.slice.cap) {
            Grow();
        }
        new (&impl_.slice.data[impl_.slice.len++]) T(std::forward<ARGS>(args)...);
    }

    /// Removes and returns the last element from the vector.
    /// @returns the popped element
    T Pop() { return std::move(impl_.slice.data[--impl_.slice.len]); }

    /// @returns true if the vector is empty.
    bool IsEmpty() const { return impl_.slice.len == 0; }

    /// @returns a reference to the first element in the vector
    T& Front() { return impl_.slice.Front(); }

    /// @returns a reference to the first element in the vector
    const T& Front() const { return impl_.slice.Front(); }

    /// @returns a reference to the last element in the vector
    T& Back() { return impl_.slice.Back(); }

    /// @returns a reference to the last element in the vector
    const T& Back() const { return impl_.slice.Back(); }

    /// @returns a pointer to the first element in the vector
    T* begin() { return impl_.slice.begin(); }

    /// @returns a pointer to the first element in the vector
    const T* begin() const { return impl_.slice.begin(); }

    /// @returns a pointer to one past the last element in the vector
    T* end() { return impl_.slice.end(); }

    /// @returns a pointer to one past the last element in the vector
    const T* end() const { return impl_.slice.end(); }

    /// @returns a reverse iterator starting with the last element in the vector
    auto rbegin() { return impl_.slice.rbegin(); }

    /// @returns a reverse iterator starting with the last element in the vector
    auto rbegin() const { return impl_.slice.rbegin(); }

    /// @returns the end for a reverse iterator
    auto rend() { return impl_.slice.rend(); }

    /// @returns the end for a reverse iterator
    auto rend() const { return impl_.slice.rend(); }

  private:
    /// Friend class (differing specializations of this class)
    template <typename, size_t>
    friend class Vector;

    /// Friend class
    template <typename>
    friend class VectorRef;

    /// Friend class
    template <typename>
    friend class ConstVectorRef;

    /// The slice type used by this vector
    using Slice = detail::Slice<T>;

    /// Expands the capacity of the vector
    void Grow() { Reserve(impl_.slice.cap * 2); }

    /// Moves 'other' to this vector, if possible, otherwise performs a copy.
    void MoveOrCopy(VectorRef<T>&& other) {
        if (other.can_move_) {
            ClearAndFree();
            impl_.slice = other.slice_;
            other.slice_ = {};
        } else {
            Copy(other.slice_);
        }
    }

    /// Copies all the elements from `other` to this vector, replacing the content of this vector.
    /// @param other the
    void Copy(const Slice& other) {
        if (impl_.slice.cap < other.len) {
            ClearAndFree();
            impl_.Allocate(other.len);
        } else {
            Clear();
        }

        impl_.slice.len = other.len;
        for (size_t i = 0; i < impl_.slice.len; i++) {
            new (&impl_.slice.data[i]) T{other.data[i]};
        }
    }

    /// Clears the vector, then frees the slice data.
    void ClearAndFree() {
        Clear();
        impl_.Free(impl_.slice.data);
    }

    /// True if this vector uses a small array for small object optimization.
    constexpr static bool HasSmallArray = N > 0;

    /// A structure that has the same size and alignment as T.
    /// Replacement for std::aligned_storage as this is broken on earlier versions of MSVC.
    struct alignas(alignof(T)) TStorage {
        /// @returns the storage reinterpreted as a T*
        T* Get() { return Bitcast<T*>(&data[0]); }
        /// @returns the storage reinterpreted as a T*
        const T* Get() const { return Bitcast<const T*>(&data[0]); }
        /// Byte array of length sizeof(T)
        uint8_t data[sizeof(T)];
    };

    /// The internal structure for the vector with a small array.
    struct ImplWithSmallArray {
        std::array<TStorage, N> small_arr;
        Slice slice = {small_arr[0].Get(), 0, N};

        /// Allocates a new vector of `T` either from #small_arr, or from the heap, then assigns the
        /// pointer it to #slice.data, and updates #slice.cap.
        void Allocate(size_t new_cap) {
            if (new_cap < N) {
                slice.data = small_arr[0].Get();
                slice.cap = N;
            } else {
                slice.data = Bitcast<T*>(new TStorage[new_cap]);
                slice.cap = new_cap;
            }
        }

        /// Frees `data`, if not nullptr and isn't a pointer to #small_arr
        void Free(T* data) const {
            if (data && data != small_arr[0].Get()) {
                delete[] Bitcast<TStorage*>(data);
            }
        }

        /// Indicates whether the slice structure can be std::move()d.
        /// @returns true if #slice.data does not point to #small_arr
        bool CanMove() const { return slice.data != small_arr[0].Get(); }
    };

    /// The internal structure for the vector without a small array.
    struct ImplWithoutSmallArray {
        Slice slice = {nullptr, 0, 0};

        /// Allocates a new vector of `T` and assigns it to #slice.data, and updates #slice.cap.
        void Allocate(size_t new_cap) {
            slice.data = Bitcast<T*>(new TStorage[new_cap]);
            slice.cap = new_cap;
        }

        /// Frees `data`, if not nullptr.
        void Free(T* data) const {
            if (data) {
                delete[] Bitcast<TStorage*>(data);
            }
        }

        /// Indicates whether the slice structure can be std::move()d.
        /// @returns true
        bool CanMove() const { return true; }
    };

    /// Either a ImplWithSmallArray or ImplWithoutSmallArray based on N.
    std::conditional_t<HasSmallArray, ImplWithSmallArray, ImplWithoutSmallArray> impl_;
};

/// VectorRef is a weak reference to a Vector, used to pass vectors as parameters, avoiding copies
/// between the caller and the callee. VectorRef can accept a Vector of any 'N' value, decoupling
/// the caller's vector internal size from the callee's vector size.
///
/// A VectorRef tracks the usage of moves either side of the call. If at the call site, a Vector
/// argument is moved to a VectorRef parameter, and within the callee, the VectorRef parameter is
/// moved to a Vector, then the Vector heap allocation will be moved. For example:
///
/// ```
///     void func_a() {
///        Vector<std::string, 4> vec;
///        // logic to populate 'vec'.
///        func_b(std::move(vec)); // Constructs a VectorRef tracking the move here.
///     }
///
///     void func_b(VectorRef<std::string> vec_ref) {
///        // A move was made when calling func_b, so the vector can be moved instead of copied.
///        Vector<std::string, 2> vec(std::move(vec_ref));
///     }
/// ```
template <typename T>
class VectorRef {
    /// The slice type used by this vector reference
    using Slice = detail::Slice<T>;

  public:
    /// Constructor from a Vector.
    /// @param vector the vector reference
    template <size_t N>
    VectorRef(Vector<T, N>& vector)  // NOLINT(runtime/explicit)
        : slice_(vector.impl_.slice), can_move_(false) {}

    /// Constructor from a std::move()'d Vector
    /// @param vector the vector being moved
    template <size_t N>
    VectorRef(Vector<T, N>&& vector)  // NOLINT(runtime/explicit)
        : slice_(vector.impl_.slice), can_move_(vector.impl_.CanMove()) {}

    /// Copy constructor
    /// @param other the vector reference
    VectorRef(const VectorRef& other) : slice_(other.slice_), can_move_(false) {}

    /// Move constructor
    /// @param other the vector reference
    VectorRef(VectorRef&& other) = default;

    /// Index operator
    /// @param i the element index. Must be less than `len`.
    /// @returns a reference to the i'th element.
    T& operator[](size_t i) { return slice_[i]; }

    /// Index operator
    /// @param i the element index. Must be less than `len`.
    /// @returns a reference to the i'th element.
    const T& operator[](size_t i) const { return slice_[i]; }

    /// @return the number of elements in the vector
    size_t Length() const { return slice_.len; }

    /// @return the number of elements that the vector could hold before a heap allocation needs to
    /// be made
    size_t Capacity() const { return slice_.cap; }

    /// @returns true if the vector is empty.
    bool IsEmpty() const { return slice_.len == 0; }

    /// @returns a reference to the first element in the vector
    T& Front() { return slice_.Front(); }

    /// @returns a reference to the first element in the vector
    const T& Front() const { return slice_.Front(); }

    /// @returns a reference to the last element in the vector
    T& Back() { return slice_.Back(); }

    /// @returns a reference to the last element in the vector
    const T& Back() const { return slice_.Back(); }

    /// @returns a pointer to the first element in the vector
    T* begin() { return slice_.begin(); }

    /// @returns a pointer to the first element in the vector
    const T* begin() const { return slice_.begin(); }

    /// @returns a pointer to one past the last element in the vector
    T* end() { return slice_.end(); }

    /// @returns a pointer to one past the last element in the vector
    const T* end() const { return slice_.end(); }

    /// @returns a reverse iterator starting with the last element in the vector
    auto rbegin() { return slice_.rbegin(); }

    /// @returns a reverse iterator starting with the last element in the vector
    auto rbegin() const { return slice_.rbegin(); }

    /// @returns the end for a reverse iterator
    auto rend() { return slice_.rend(); }

    /// @returns the end for a reverse iterator
    auto rend() const { return slice_.rend(); }

  private:
    /// Friend classes
    template <typename, size_t>
    friend class Vector;

    /// The slice of the vector being referenced.
    Slice& slice_;
    /// Whether the slice data is passed by r-value reference, and can be moved.
    bool can_move_ = false;
};

/// ConstVectorRef is a weak, immutable reference to a Vector, used to pass vectors as parameters,
/// avoiding copies between the caller and the callee. VectorRef can accept a Vector of any 'N'
/// value, decoupling the caller's vector internal size from the callee's vector size.
template <typename T>
class ConstVectorRef {
    /// The slice type used by this vector reference
    using Slice = detail::Slice<T>;

  public:
    /// Constructor from a Vector.
    /// @param vector the vector reference
    template <size_t N>
    ConstVectorRef(const Vector<T, N>& vector)  // NOLINT(runtime/explicit)
        : slice_(vector.impl_.slice) {}

    /// Copy constructor
    /// @param other the vector reference
    ConstVectorRef(const ConstVectorRef& other) = default;

    /// Index operator
    /// @param i the element index. Must be less than `len`.
    /// @returns a reference to the i'th element.
    const T& operator[](size_t i) const { return slice_[i]; }

    /// @return the number of elements in the vector
    size_t Length() const { return slice_.len; }

    /// @return the number of elements that the vector could hold before a heap allocation needs to
    /// be made
    size_t Capacity() const { return slice_.cap; }

    /// @returns true if the vector is empty.
    bool IsEmpty() const { return slice_.len == 0; }

    /// @returns a reference to the first element in the vector
    const T& Front() const { return slice_.Front(); }

    /// @returns a reference to the last element in the vector
    const T& Back() const { return slice_.Back(); }

    /// @returns a pointer to the first element in the vector
    const T* begin() const { return slice_.begin(); }

    /// @returns a pointer to one past the last element in the vector
    const T* end() const { return slice_.end(); }

    /// @returns a reverse iterator starting with the last element in the vector
    auto rbegin() const { return slice_.rbegin(); }

    /// @returns the end for a reverse iterator
    auto rend() const { return slice_.rend(); }

  private:
    /// Friend classes
    template <typename, size_t>
    friend class Vector;

    /// The slice of the vector being referenced.
    const Slice& slice_;
};

/// Helper for converting a Vector to a std::vector.
/// @note This helper exists to help code migration. Avoid if possible.
template <typename T, size_t N>
std::vector<T> ToStdVector(const Vector<T, N>& vector) {
    std::vector<T> out;
    out.reserve(vector.Length());
    for (auto& el : vector) {
        out.emplace_back(el);
    }
    return out;
}

/// Helper for converting a std::vector to a Vector.
/// @note This helper exists to help code migration. Avoid if possible.
template <typename T, size_t N = 0>
Vector<T, N> ToVector(const std::vector<T>& vector) {
    Vector<T, N> out;
    out.Reserve(vector.size());
    for (auto& el : vector) {
        out.Push(el);
    }
    return out;
}

/// Helper for constructing a Vector from a set of elements.
/// The returned Vector's small-array size (`N`) is equal to the number of provided elements.
/// @param elements the elements used to construct the vector.
template <typename T, typename... Ts>
auto MakeVector(Ts&&... elements) {
    return Vector<T, sizeof...(Ts)>({std::forward<Ts>(elements)...});
}

}  // namespace tint::utils

#endif  // SRC_TINT_UTILS_VECTOR_H_