# C++20 Three-way Comparison (Spaceship Operator) Guide

## Quick Reference

The spaceship operator (`<=>`) is one of C++20's most useful features for implementing comparison operations.

---

## Basic Usage

### Default Implementation
For simple types, use `= default`:

```cpp
struct Point {
    int x, y;
    auto operator<=>(const Point&) const = default;
};

Point a{1, 2}, b{3, 4};
bool result = a < b;  // Works!
```

### Custom Implementation
For complex types, implement manually:

```cpp
class MyClass {
    int value;
public:
    std::strong_ordering operator<=>(const MyClass& other) const {
        return value <=> other.value;
    }

    // Often need to define == explicitly for optimization
    bool operator==(const MyClass& other) const {
        return value == other.value;
    }
};
```

---

## Ordering Categories

### `std::strong_ordering`
**Use when**: Values are fully comparable and substitutable
- **Values**: `less`, `equal`, `greater`
- **Example**: integers, pointers, strings

```cpp
std::strong_ordering operator<=>(const blob_t& other) const {
    if (auto cmp = size <=> other.size; cmp != 0)
        return cmp;
    return std::memcmp(data, other.data, size) <=> 0;  // C++20 memcmp spaceship
}
```

### `std::weak_ordering`
**Use when**: Values are comparable but not substitutable
- **Values**: `less`, `equivalent`, `greater`
- **Example**: case-insensitive strings, pointers to unrelated objects

```cpp
std::weak_ordering operator<=>(const CaseInsensitiveString& other) const {
    int cmp = strcasecmp(str, other.str);
    if (cmp < 0) return std::weak_ordering::less;
    if (cmp > 0) return std::weak_ordering::greater;
    return std::weak_ordering::equivalent;
}
```

### `std::partial_ordering`
**Use when**: Not all values are comparable
- **Values**: `less`, `equivalent`, `greater`, `unordered`
- **Example**: floating-point (NaN), partially ordered sets

```cpp
std::partial_ordering operator<=>(double other) const {
    if (std::isnan(value) || std::isnan(other))
        return std::partial_ordering::unordered;
    return value <=> other;
}
```

---

## Performance Best Practices

### 1. Always Define `operator==` Explicitly
Even with `<=>`, define `==` for optimal performance:

```cpp
// GOOD: Optimized equality check
bool operator==(const T& other) const {
    return fast_equality_check(other);
}

auto operator<=>(const T& other) const {
    return full_comparison(other);
}
```

### 2. Use `noexcept` When Possible
Mark comparison operators `noexcept` for better optimization:

```cpp
auto operator<=>(const T& other) const noexcept {
    // ...
}

bool operator==(const T& other) const noexcept {
    // ...
}
```

### 3. Early Return Optimization
Compare cheap fields first:

```cpp
std::strong_ordering operator<=>(const Person& other) const {
    // Compare integer first (cheap)
    if (auto cmp = age <=> other.age; cmp != 0)
        return cmp;

    // Then compare string (expensive)
    return name <=> other.name;
}
```

### 4. Avoid Redundant Comparisons
For containers, leverage existing comparisons:

```cpp
std::strong_ordering operator<=>(const Container& other) const {
    // std::vector already has <=>
    return items <=> other.items;
}
```

---

## Common Patterns

### Pattern 1: Lexicographical Comparison
```cpp
std::strong_ordering operator<=>(const MultiField& other) const {
    if (auto cmp = field1 <=> other.field1; cmp != 0) return cmp;
    if (auto cmp = field2 <=> other.field2; cmp != 0) return cmp;
    return field3 <=> other.field3;
}
```

### Pattern 2: Size-Then-Content
```cpp
std::strong_ordering operator<=>(const blob_t& other) const noexcept {
    // Compare sizes first
    if (auto cmp = _size <=> other._size; cmp != 0)
        return cmp;

    // Empty blobs are equal
    if (_size == 0)
        return std::strong_ordering::equal;

    // Compare contents
    int result = memcmp(_data, other._data, _size);
    if (result < 0) return std::strong_ordering::less;
    if (result > 0) return std::strong_ordering::greater;
    return std::strong_ordering::equal;
}
```

### Pattern 3: Derived Comparison
```cpp
std::strong_ordering operator<=>(const Derived& other) const {
    // Compare base class first
    if (auto cmp = Base::operator<=>(other); cmp != 0)
        return cmp;

    // Then compare derived fields
    return derived_field <=> other.derived_field;
}
```

---

## Migration from Legacy Operators

### Before (C++17)
```cpp
class OldStyle {
    int value;
public:
    bool operator==(const OldStyle& other) const { return value == other.value; }
    bool operator!=(const OldStyle& other) const { return value != other.value; }
    bool operator<(const OldStyle& other) const { return value < other.value; }
    bool operator<=(const OldStyle& other) const { return value <= other.value; }
    bool operator>(const OldStyle& other) const { return value > other.value; }
    bool operator>=(const OldStyle& other) const { return value >= other.value; }
};
```

### After (C++20)
```cpp
class NewStyle {
    int value;
public:
    auto operator<=>(const NewStyle& other) const = default;
    // That's it! All six operators generated automatically
};
```

---

## ServerLink Usage

In ServerLink, we use the spaceship operator for `blob_t` comparison:

```cpp
// File: src/msg/blob.hpp
#if SL_HAVE_THREE_WAY_COMPARISON
    [[nodiscard]] std::strong_ordering operator<=>(const blob_t &other_) const noexcept
    {
        // First compare sizes (cheap operation)
        if (auto cmp = _size <=> other_._size; cmp != 0) {
            return cmp;
        }

        // Handle empty blobs
        if (_size == 0) {
            return std::strong_ordering::equal;
        }

        // Compare contents lexicographically
        const int result = memcmp(_data, other_._data, _size);
        if (result < 0) return std::strong_ordering::less;
        if (result > 0) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }

    // Optimized equality check
    [[nodiscard]] bool operator==(const blob_t &other_) const noexcept
    {
        return _size == other_._size &&
               (_size == 0 || memcmp(_data, other_._data, _size) == 0);
    }
#else
    // C++17 fallback
    bool operator<(blob_t const &other_) const {
        const int cmpres = memcmp(_data, other_._data, std::min(_size, other_._size));
        return cmpres < 0 || (cmpres == 0 && _size < other_._size);
    }
#endif
```

---

## Compiler Support

| Compiler | Version | Support |
|----------|---------|---------|
| GCC | 10+ | ✅ Full |
| Clang | 10+ | ✅ Full |
| MSVC | 19.20+ | ✅ Full |

---

## Common Pitfalls

### Pitfall 1: Not Defining `operator==`
```cpp
// BAD: Inefficient, == uses <=>
auto operator<=>(const T&) const;

// GOOD: Optimized equality
bool operator==(const T&) const;
auto operator<=>(const T&) const;
```

### Pitfall 2: Wrong Ordering Category
```cpp
// BAD: Should use strong_ordering
std::weak_ordering operator<=>(const int& other) const;

// GOOD: Integers have strong ordering
std::strong_ordering operator<=>(const int& other) const;
```

### Pitfall 3: Forgetting `const`
```cpp
// BAD: Not const
auto operator<=>(const T& other);

// GOOD: Const member function
auto operator<=>(const T& other) const;
```

---

## Testing Comparison Operators

```cpp
void test_comparison() {
    T a, b, c;

    // Test reflexivity: a == a
    assert(a == a);

    // Test symmetry: if a == b, then b == a
    if (a == b) assert(b == a);

    // Test transitivity: if a == b and b == c, then a == c
    if (a == b && b == c) assert(a == c);

    // Test antisymmetry: if a <= b and b <= a, then a == b
    if (a <= b && b <= a) assert(a == b);

    // Test trichotomy: exactly one of a < b, a == b, a > b
    int count = (a < b) + (a == b) + (a > b);
    assert(count == 1);
}
```

---

## References

- [C++20 Standard: Three-way comparison](https://en.cppreference.com/w/cpp/language/operator_comparison)
- [P0515R3: Consistent comparison](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0515r3.pdf)
- [CppCon 2019: Jonathan Müller "The C++20 Spaceship Operator"](https://www.youtube.com/watch?v=S9ShnAFmiWM)

---

## Summary

The spaceship operator is a powerful C++20 feature that:
- ✅ Reduces boilerplate code (6 operators → 1)
- ✅ Prevents inconsistent implementations
- ✅ Improves type safety with ordering categories
- ✅ Enables compiler optimizations
- ✅ Makes code more maintainable

Use it whenever you need to implement comparison operations!
