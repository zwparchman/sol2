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

#ifndef SOL_STATE_VIEW_HPP
#define SOL_STATE_VIEW_HPP

#include "error.hpp"
#include "table.hpp"
#include <memory>

namespace sol {
namespace detail {
inline int atpanic(lua_State* L) {
#ifdef SOL_NO_EXCEPTIONS
    (void)L;
    return -1;
#else
    const char* message = lua_tostring(L, -1);
    std::string err = message ? message : "An unexpected error occurred and forced the lua state to call atpanic";
    throw error(err);
#endif
}
} // detail

enum class lib : char {
    base,
    package,
    coroutine,
    string,
    os,
    math,
    table,
    debug,
    bit32,
    io,
    count
};

class state_view {
private:
    lua_State* L;
    table reg;
    global_table global;
public:
    state_view(lua_State* L):
    L(L),
    reg(L, LUA_REGISTRYINDEX),
    global(L, detail::global_) {

    }

    lua_State* lua_state() const {
        return L;
    }

    template<typename... Args>
    void open_libraries(Args&&... args) {
        static_assert(meta::are_same<lib, Args...>::value, "all types must be libraries");
        if(sizeof...(args) == 0) {
            luaL_openlibs(L);
            return;
        }

        lib libraries[1 + sizeof...(args)] = { lib::count, std::forward<Args>(args)... };

        for(auto&& library : libraries) {
            switch(library) {
#if SOL_LUA_VERSION <= 501 && defined(SOL_LUAJIT)
            case lib::coroutine:
#endif // luajit opens coroutine base stuff
            case lib::base:
                luaL_requiref(L, "base", luaopen_base, 1);
                lua_pop(L, 1);
                break;
            case lib::package:
                luaL_requiref(L, "package", luaopen_package, 1);
                lua_pop(L, 1);
                break;
#if SOL_LUA_VERSION > 501
            case lib::coroutine:
                luaL_requiref(L, "coroutine", luaopen_coroutine, 1);
                lua_pop(L, 1);
                break;
#endif // Lua 5.2+ only
            case lib::string:
                luaL_requiref(L, "string", luaopen_string, 1);
                lua_pop(L, 1);
                break;
            case lib::table:
                luaL_requiref(L, "table", luaopen_table, 1);
                lua_pop(L, 1);
                break;
            case lib::math:
                luaL_requiref(L, "math", luaopen_math, 1);
                lua_pop(L, 1);
                break;
            case lib::bit32:
#if SOL_LUA_VERSION > 510
                luaL_requiref(L, "bit32", luaopen_bit32, 1);
                lua_pop(L, 1);
#else
#endif // Lua 5.2+ only
                break;
            case lib::io:
                luaL_requiref(L, "io", luaopen_io, 1);
                lua_pop(L, 1);
                break;
            case lib::os:
                luaL_requiref(L, "os", luaopen_os, 1);
                lua_pop(L, 1);
                break;
            case lib::debug:
                luaL_requiref(L, "debug", luaopen_debug, 1);
                lua_pop(L, 1);
                break;
            case lib::count:
                break;
            }
        }
    }

    void script(const std::string& code) {
        if(luaL_dostring(L, code.c_str())) {
            lua_error(L);
        }
    }

    void script_file(const std::string& filename) {
        if(luaL_dofile(L, filename.c_str())) {
            lua_error(L);
        }
    }

    table_iterator begin () const {
        return global.begin();
    }

    table_iterator end() const {
        return global.end();
    }

    table_iterator cbegin() const {
        return global.cbegin();
    }

    table_iterator cend() const {
        return global.cend();
    }

    global_table globals() const {
        return global;
    }

    table registry() const {
        return reg;
    }

    void set_panic(lua_CFunction panic){
        lua_atpanic(L, panic);
    }

    template<typename... Args, typename... Keys>
    decltype(auto) get(Keys&&... keys) const {
        return global.get<Args...>(std::forward<Keys>(keys)...);
    }

    template<typename... Args>
    state_view& set(Args&&... args) {
        global.set(std::forward<Args>(args)...);
        return *this;
    }

    template<typename T, typename... Keys>
    decltype(auto) traverse_get(Keys&&... keys) const {
        return global.traverse_get<T>(std::forward<Keys>(keys)...);
    }

    template<typename... Args>
    state_view& traverse_set(Args&&... args) {
        global.traverse_set(std::forward<Args>(args)...);
        return *this;
    }

    template<typename T>
    state_view& set_usertype(usertype<T>& user) {
        return set_usertype(usertype_traits<T>::name, user);
    }

    template<typename Key, typename T>
    state_view& set_usertype(Key&& key, usertype<T>& user) {
        global.set_usertype(std::forward<Key>(key), user);
        return *this;
    }

    template<typename Class, typename... Args>
    state_view& new_usertype(const std::string& name, Args&&... args) {
        global.new_usertype<Class>(name, std::forward<Args>(args)...);
        return *this;
    }

    template<typename Class, typename CTor0, typename... CTor, typename... Args>
    state_view& new_usertype(const std::string& name, Args&&... args) {
        global.new_usertype<Class, CTor0, CTor...>(name, std::forward<Args>(args)...);
        return *this;
    }

    template<typename Class, typename... CArgs, typename... Args>
    state_view& new_usertype(const std::string& name, constructors<CArgs...> ctor, Args&&... args) {
        global.new_usertype<Class>(name, ctor, std::forward<Args>(args)...);
        return *this;
    }

    template <typename Fx>
    void for_each(Fx&& fx) {
        global.for_each(std::forward<Fx>(fx));
    }

    template<typename T>
    proxy<global_table&, T> operator[](T&& key) {
        return global[std::forward<T>(key)];
    }

    template<typename T>
    proxy<const global_table&, T> operator[](T&& key) const {
        return global[std::forward<T>(key)];
    }

    template<typename... Args, typename R, typename Key>
    state_view& set_function(Key&& key, R fun_ptr(Args...)){
        global.set_function(std::forward<Key>(key), fun_ptr);
        return *this;
    }

    template<typename Sig, typename Key>
    state_view& set_function(Key&& key, Sig* fun_ptr){
        global.set_function(std::forward<Key>(key), fun_ptr);
        return *this;
    }

    template<typename... Args, typename R, typename C, typename Key>
    state_view& set_function(Key&& key, R (C::*mem_ptr)(Args...)) {
        global.set_function(std::forward<Key>(key), mem_ptr);
        return *this;
    }

    template<typename Sig, typename C, typename Key>
    state_view& set_function(Key&& key, Sig C::* mem_ptr) {
        global.set_function(std::forward<Key>(key), mem_ptr);
        return *this;
    }

    template<typename... Args, typename R, typename C, typename T, typename Key>
    state_view& set_function(Key&& key, R (C::*mem_ptr)(Args...), T&& obj) {
        global.set_function(std::forward<Key>(key), mem_ptr, std::forward<T>(obj));
        return *this;
    }

    template<typename Sig, typename C, typename T, typename Key>
    state_view& set_function(Key&& key, Sig C::* mem_ptr, T&& obj) {
        global.set_function(std::forward<Key>(key), mem_ptr, std::forward<T>(obj));
        return *this;
    }

    template<typename... Sig, typename Fx, typename Key>
    state_view& set_function(Key&& key, Fx&& fx) {
        global.set_function<Sig...>(std::forward<Key>(key), std::forward<Fx>(fx));
        return *this;
    }

    template <typename Name>
    table create_table(Name&& name, int narr = 0, int nrec = 0) {
        return global.create(std::forward<Name>(name), narr, nrec);
    }

    template <typename Name, typename Key, typename Value, typename... Args>
    table create_table(Name&& name, int narr, int nrec, Key&& key, Value&& value, Args&&... args) {
        return global.create(std::forward<Name>(name), narr, nrec, std::forward<Key>(key), std::forward<Value>(value), std::forward<Args>(args)...);
    }

    table create_table(int narr = 0, int nrec = 0) {
        return create_table(lua_state(), narr, nrec);
    }

    template <typename Key, typename Value, typename... Args>
    table create_table(int narr, int nrec, Key&& key, Value&& value, Args&&... args) {
        return create_table(lua_state(), narr, nrec, std::forward<Key>(key), std::forward<Value>(value), std::forward<Args>(args)...);
    }

    template <typename... Args>
    table create_table_with(Args&&... args) {
        return create_table_with(lua_state(), std::forward<Args>(args)...);
    }

    static inline table create_table(lua_State* L, int narr = 0, int nrec = 0) {
        return global_table::create(L, narr, nrec);
    }

    template <typename Key, typename Value, typename... Args>
    static inline table create_table(lua_State* L, int narr, int nrec, Key&& key, Value&& value, Args&&... args) {
        return global_table::create(L, narr, nrec, std::forward<Key>(key), std::forward<Value>(value), std::forward<Args>(args)...);
    }

    template <typename... Args>
    static inline table create_table_with(lua_State* L, Args&&... args) {
        return global_table::create_with(L, std::forward<Args>(args)...);
    }
};
} // sol

#endif // SOL_STATE_VIEW_HPP
