/*
 * This source file is part of RmlUi, the HTML/CSS Interface Middleware
 *
 * For the latest information, see http://github.com/mikke89/RmlUi
 *
 * Copyright (c) 2008-2010 CodePoint Ltd, Shift Technology Ltd
 * Copyright (c) 2019 The RmlUi Team, and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef RMLUI_RBFX_CONFIG_H
#define RMLUI_RBFX_CONFIG_H

#include <utility>
#include <type_traits>
#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/vector.h>
#include <EASTL/unordered_map.h>
#include <EASTL/set.h>
#include <EASTL/unordered_set.h>
#include <EASTL/stack.h>
#include <EASTL/list.h>
#include <EASTL/functional.h>
#include <EASTL/queue.h>
#include <EASTL/array.h>

namespace Rml
{

// Default matrix type to be used. This alias may be set to ColumnMajorMatrix4f or RowMajorMatrix4f. This alias can not
// be set here because matrix types are defined after this include in Core/Types.h.
#define RMLUI_MATRIX4_TYPE RowMajorMatrix4f

// Containers
template<typename T>
using Vector = eastl::vector<T>;
template<typename T, size_t N = 1>
using Array = eastl::array<T, N>;
template<typename T>
using Stack = eastl::stack<T>;
template<typename T>
using List = eastl::list<T>;
template<typename T>
using Queue = eastl::queue<T>;
template<typename T1, typename T2>
using Pair = std::pair<T1, T2>;
template <typename Key, typename Value>
using UnorderedMap = eastl::unordered_map< Key, Value >;
template <typename Key, typename Value>
using UnorderedMultimap = eastl::unordered_multimap< Key, Value >;
template <typename Key, typename Value>
using SmallUnorderedMap = UnorderedMap< Key, Value >;
template <typename T>
using UnorderedSet = eastl::unordered_set< T >;
template <typename T>
using SmallUnorderedSet = eastl::unordered_set< T >;
template <typename T>
using SmallOrderedSet = eastl::set< T >;
template<typename Iterator>
inline eastl::move_iterator<Iterator> MakeMoveIterator(Iterator it) { return eastl::make_move_iterator(it); }

// Utilities
template <typename T>
using Hash = eastl::hash<T>;
template<typename T>
using Function = eastl::function<T>;

// Strings
using String = eastl::string;
using StringList = Vector< String >;
using U16String = eastl::u16string;

// Smart pointer types
template<typename T>
using UniquePtr = eastl::unique_ptr<T>;
template<typename T>
class Releaser;
template<typename T>
using UniqueReleaserPtr = eastl::unique_ptr<T, Releaser<T>>;
template<typename T>
using SharedPtr = eastl::shared_ptr<T>;
template<typename T>
using WeakPtr = eastl::weak_ptr<T>;
template<typename T, typename... Args>
inline SharedPtr<T> MakeShared(Args... args) { return eastl::make_shared<T, Args...>(std::forward<Args>(args)...); }
template<typename T, typename... Args>
inline UniquePtr<T> MakeUnique(Args... args) { return eastl::make_unique<T, Args...>(std::forward<Args>(args)...); }

}

#include <Urho3D/Math/Color.h>
#include <Urho3D/Math/Vector2.h>
#include <Urho3D/Math/Vector3.h>
#include <Urho3D/Math/Vector4.h>
#include <Urho3D/Math/Matrix4.h>

#define RMLUI_COLOUR_USER_EXTRA                                                                         \
    template<typename U = ColourType, typename std::enable_if_t<std::is_same_v<U, byte>>* = nullptr>    \
    operator Urho3D::Color() const { return Urho3D::Color(                                              \
        (float)red / 256.0f, (float)green / 256.0f, (float)blue / 256.0f, (float)alpha / 256.0f); }     \
    template<typename U = ColourType, typename std::enable_if_t<std::is_same_v<U, float>>* = nullptr>   \
    operator Urho3D::Color() const { return Urho3D::Color(red, green, blue, alpha); }                   \

#define RMLUI_VECTOR2_USER_EXTRA                                                                        \
    template<typename U = Type, typename std::enable_if_t<std::is_same_v<U, int>>* = nullptr>           \
    operator typename Urho3D::IntVector2() const { return Urho3D::IntVector2(x, y); }                   \
    template<typename U = Type, typename std::enable_if_t<std::is_same_v<U, float>>* = nullptr>         \
    operator Urho3D::Vector2() const { return Urho3D::Vector2(x, y); }                                  \
    template<typename U = Type, typename std::enable_if_t<std::is_same_v<U, int>>* = nullptr>           \
    Vector2(Urho3D::IntVector2 value) : Vector2(value.x_, value.y_) { }                                 \
    template<typename U = Type, typename std::enable_if_t<std::is_same_v<U, float>>* = nullptr>         \
    Vector2(Urho3D::Vector2 value) : Vector2(value.x_, value.y_) { }                                    \

#define RMLUI_VECTOR3_USER_EXTRA                                                                        \
    template<typename U = Type, typename std::enable_if_t<std::is_same_v<U, int>>* = nullptr>           \
    operator typename Urho3D::IntVector3() const { return Urho3D::IntVector3(x, y, z); }                \
    template<typename U = Type, typename std::enable_if_t<std::is_same_v<U, float>>* = nullptr>         \
    operator Urho3D::Vector3() const { return Urho3D::Vector3(x, y, z); }                               \
    template<typename U = Type, typename std::enable_if_t<std::is_same_v<U, int>>* = nullptr>           \
    Vector3(Urho3D::IntVector3 value) : Vector3(value.x_, value.y_, value.z_) { }                       \
    template<typename U = Type, typename std::enable_if_t<std::is_same_v<U, float>>* = nullptr>         \
    Vector3(Urho3D::Vector3 value) : Vector3(value.x_, value.y_, value.z_) { }                          \

#define RMLUI_VECTOR4_USER_EXTRA                                                                        \
    template<typename U = Type, typename std::enable_if_t<std::is_same_v<U, float>>* = nullptr>         \
    operator Urho3D::Vector4() const { return Urho3D::Vector4(x, y, z, w); }                            \
    template<typename U = Type, typename std::enable_if_t<std::is_same_v<U, float>>* = nullptr>         \
    Vector4(Urho3D::Vector4 value) : Vector4(value.x_, value.y_, value.z_, value.w_) { }                \

#define RMLUI_MATRIX4_USER_EXTRA operator Urho3D::Matrix4() const { return Urho3D::Matrix4(data()); }

#endif  // RMLUI_RBFX_CONFIG_H
