// The MIT License (MIT) 

// Copyright (c) 2013-2016 Rapptz, ThePhD and contributors

// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SOL_STACK_FIELD_HPP
#define SOL_STACK_FIELD_HPP

#include "stack_core.hpp"
#include "stack_push.hpp"
#include "stack_get.hpp"
#include "stack_check_get.hpp"

namespace sol {
namespace stack {
template <typename T, bool, typename>
struct field_getter {
    template <typename Key>
    void get(lua_State* L, Key&& key, int tableindex = -2) {
        push( L, std::forward<Key>( key ) );
        lua_gettable( L, tableindex );
    }
};

template <typename... Args, bool b, typename C>
struct field_getter<std::tuple<Args...>, b, C> {
    template <std::size_t... I, typename Keys>
    void apply(std::index_sequence<I...>, lua_State* L, Keys&& keys, int tableindex) {
        void(detail::swallow{ (get_field<I < 1 && b>(L, detail::forward_get<I>(keys), tableindex), 0)... });
        reference saved(L, -1);
        lua_pop(L, static_cast<int>(sizeof...(I)));
        saved.push();
    }

    template <typename Keys>
    void get(lua_State* L, Keys&& keys, int tableindex = -2) {
        tableindex = lua_absindex(L, tableindex);
        apply(std::index_sequence_for<Args...>(), L, std::forward<Keys>(keys), tableindex);
    }
};

template <typename A, typename B, bool b, typename C>
struct field_getter<std::pair<A, B>, b, C> {
    template <typename Keys>
    void get(lua_State* L, Keys&& keys, int tableindex = -2) {
        tableindex = lua_absindex(L, tableindex);
        get_field<b>(L, detail::forward_get<0>(keys), tableindex);
        get_field<false>(L, detail::forward_get<1>(keys), tableindex + 1);
        reference saved(L, -1);
        lua_pop(L, static_cast<int>(2));
        saved.push();
    }
};

template <typename T>
struct field_getter<T, true, std::enable_if_t<meta::is_c_str<T>::value>> {
    template <typename Key>
    void get(lua_State* L, Key&& key, int = -1) {
        lua_getglobal(L, &key[0]);
    }
};

template <typename T>
struct field_getter<T, false, std::enable_if_t<meta::is_c_str<T>::value>> {
    template <typename Key>
    void get(lua_State* L, Key&& key, int tableindex = -1) {
        lua_getfield(L, tableindex, &key[0]);
    }
};

#if SOL_LUA_VERSION >= 503
template <typename T>
struct field_getter<T, false, std::enable_if_t<std::is_integral<T>::value>> {
    template <typename Key>
    void get(lua_State* L, Key&& key, int tableindex = -1) {
        lua_geti(L, tableindex, static_cast<lua_Integer>(key));
    }
};
#endif // Lua 5.3.x

template <typename T, bool, typename>
struct field_setter {
    template <typename Key, typename Value>
    void set(lua_State* L, Key&& key, Value&& value, int tableindex = -3) {
        push(L, std::forward<Key>(key));
        push(L, std::forward<Value>(value));
        lua_settable(L, tableindex);
    }
};

template <typename T>
struct field_setter<T, true, std::enable_if_t<meta::is_c_str<T>::value>> {
    template <typename Key, typename Value>
    void set(lua_State* L, Key&& key, Value&& value, int = -2) {
        push(L, std::forward<Value>(value));
        lua_setglobal(L, &key[0]);
    }
};

template <typename T>
struct field_setter<T, false, std::enable_if_t<meta::is_c_str<T>::value>> {
    template <typename Key, typename Value>
    void set(lua_State* L, Key&& key, Value&& value, int tableindex = -2) {
        push(L, std::forward<Value>(value));
        lua_setfield(L, tableindex, &key[0]);
    }
};

#if SOL_LUA_VERSION >= 503
template <typename T>
struct field_setter<T, false, std::enable_if_t<std::is_integral<T>::value>> {
    template <typename Key, typename Value>
    void set(lua_State* L, Key&& key, Value&& value, int tableindex = -2) {
        push(L, std::forward<Value>(value));
        lua_seti(L, tableindex, static_cast<lua_Integer>(key));
    }
};
#endif // Lua 5.3.x
} // stack
} // sol

#endif // SOL_STACK_FIELD_HPP