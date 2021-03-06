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

#ifndef SOL_FUNCTION_HPP
#define SOL_FUNCTION_HPP

#include "reference.hpp"
#include "stack.hpp"
#include "resolve.hpp"
#include "function_result.hpp"
#include "function_types.hpp"
#include <cstdint>
#include <functional>
#include <memory>

namespace sol {
template <typename Sig, typename... Functions>
struct function_packer { 
    std::tuple<Functions...> set;
    template <typename... Args>
    function_packer(Args&&... args) : set(std::forward<Args>(args)...) {}
};

template <typename Sig, typename... Args>
function_packer<Sig, Args...> function_pack( Args&&... args ) { 
    return function_packer<Sig, Args...>(std::forward<Args>(args)...);
}

class function : public reference {
private:
    void luacall( std::ptrdiff_t argcount, std::ptrdiff_t resultcount ) const {
        lua_callk( lua_state( ), static_cast<int>( argcount ), static_cast<int>( resultcount ), 0, nullptr );
    }

    template<std::size_t... I, typename... Ret>
    auto invoke( types<Ret...>, std::index_sequence<I...>, std::ptrdiff_t n ) const {
        luacall( n, sizeof...( Ret ) );
        return stack::pop<std::tuple<Ret...>>( lua_state( ) );
    }

    template<std::size_t I, typename Ret>
    Ret invoke(types<Ret>, std::index_sequence<I>, std::ptrdiff_t n ) const {
        luacall( n, 1 );
        return stack::pop<Ret>( lua_state( ) );
    }

    template <std::size_t I>
    void invoke(types<void>, std::index_sequence<I>, std::ptrdiff_t n) const {
        luacall( n, 0 );
    }

    function_result invoke(types<>, std::index_sequence<>, std::ptrdiff_t n ) const {
        int stacksize = lua_gettop( lua_state( ) );
        int firstreturn = std::max( 1, stacksize - static_cast<int>( n ) );
        luacall(n, LUA_MULTRET);
        int poststacksize = lua_gettop( lua_state( ) );
        int returncount = poststacksize - (firstreturn - 1);
        return function_result( lua_state( ), firstreturn, returncount );
    }

public:
    using reference::reference;
    
    template<typename... Args>
    function_result operator()( Args&&... args ) const {
        return call<>( std::forward<Args>( args )... );
    }

    template<typename... Ret, typename... Args>
    decltype(auto) operator()( types<Ret...>, Args&&... args ) const {
        return call<Ret...>( std::forward<Args>( args )... );
    }

    template<typename... Ret, typename... Args>
    decltype(auto) call( Args&&... args ) const {
        push( );
        int pushcount = stack::multi_push( lua_state( ), std::forward<Args>( args )... );
        return invoke( types<Ret...>( ), std::index_sequence_for<Ret...>(), pushcount );
    }
};

namespace stack {
template<typename... Sigs>
struct pusher<function_sig<Sigs...>> {

    template<typename R, typename... Args, typename Fx, typename = std::result_of_t<Fx(Args...)>>
    static void set_memfx(types<R(Args...)> t, lua_State* L, Fx&& fx) {
        typedef std::decay_t<meta::Unwrapped<meta::Unqualified<Fx>>> raw_fx_t;
        typedef R(* fx_ptr_t)(Args...);
        typedef std::is_convertible<raw_fx_t, fx_ptr_t> is_convertible;
        set_isconvertible_fx(is_convertible(), t, L, std::forward<Fx>(fx));
    }

    template<typename Fx>
    static void set_memfx(types<>, lua_State* L, Fx&& fx) {
        typedef meta::Unwrapped<meta::Unqualified<Fx>> fx_t;
        set(L, &fx_t::operator(), std::forward<Fx>(fx));
    }

    template<typename... Args, typename R>
    static void set(lua_State* L, R fxptr(Args...)){
        set_fx(std::false_type(), L, fxptr);
    }

    template<typename Sig>
    static void set(lua_State* L, Sig* fxptr){
        set_fx(std::false_type(), L, fxptr);
    }

    template<typename... Args, typename R, typename C>
    static void set(lua_State* L, R (C::*memfxptr)(Args...)) {
        // Layout:
        // idx 1...n: verbatim data of member function pointer
        lua_CFunction freefunc = &function_detail::upvalue_this_member_function<C, R(C::*)(Args...)>::call;
        int upvalues = stack::stack_detail::push_as_upvalues(L, memfxptr);
        stack::push(L, freefunc, upvalues);
    }

    template<typename Sig, typename C>
    static void set(lua_State* L, Sig C::* memfxptr) {
        // Layout:
        // idx 1...n: verbatim data of member function pointer
        lua_CFunction freefunc = &function_detail::upvalue_this_member_function<C, Sig C::*>::call;
        int upvalues = stack::stack_detail::push_as_upvalues(L, memfxptr);
        stack::push(L, freefunc, upvalues);
    }

    template<typename... Args, typename R, typename C, typename T>
    static void set(lua_State* L, R (C::*memfxptr)(Args...), T&& obj) {
        typedef meta::Bool<meta::is_specialization_of<meta::Unqualified<T>, std::reference_wrapper>::value || std::is_pointer<T>::value> is_reference;
        set_reference_fx(is_reference(), L, memfxptr, std::forward<T>(obj));
    }

    template<typename Sig, typename C, typename T>
    static void set(lua_State* L, Sig C::* memfxptr, T&& obj) {
        typedef meta::Bool<meta::is_specialization_of<meta::Unqualified<T>, std::reference_wrapper>::value || std::is_pointer<T>::value> is_reference;
        set_reference_fx(is_reference(), L, memfxptr, std::forward<T>(obj));
    }

    template<typename... Sig, typename Fx>
    static void set(lua_State* L, Fx&& fx) {
        set_memfx(types<Sig...>(), L, std::forward<Fx>(fx));
    }

    template<typename Fx, typename R, typename... Args>
    static void set_isconvertible_fx(std::true_type, types<R(Args...)>, lua_State* L, Fx&& fx) {
        using fx_ptr_t = R(*)(Args...);
        fx_ptr_t fxptr = detail::unwrap(std::forward<Fx>(fx));
        set(L, fxptr);
    }

    template<typename Fx, typename R, typename... Args>
    static void set_isconvertible_fx(std::false_type, types<R(Args...)>, lua_State* L, Fx&& fx) {
        typedef meta::Unwrapped<std::decay_t<Fx>> fx_t;
        std::unique_ptr<function_detail::base_function> sptr = std::make_unique<function_detail::functor_function<fx_t>>(std::forward<Fx>(fx));
        set_fx(L, std::move(sptr));
    }

    template<typename Fx, typename T>
    static void set_reference_fx(std::true_type, lua_State* L, Fx&& fx, T&& obj) {
        set_fx(std::true_type(), L, std::forward<Fx>(fx), std::forward<T>(obj));
    }

    template<typename Fx, typename T>
    static void set_reference_fx(std::false_type, lua_State* L, Fx&& fx, T&& obj) {
        typedef std::remove_pointer_t<std::decay_t<Fx>> clean_fx;
        std::unique_ptr<function_detail::base_function> sptr = std::make_unique<function_detail::member_function<clean_fx, meta::Unqualified<T>>>(std::forward<T>(obj), std::forward<Fx>(fx));
        return set_fx(L, std::move(sptr));
    }

    template<typename Fx, typename T>
    static void set_fx(std::true_type, lua_State* L, Fx&& fx, T&& obj) {
        // Layout:
        // idx 1...n: verbatim data of member function pointer
        // idx n + 1: is the object's void pointer
        // We don't need to store the size, because the other side is templated
        // with the same member function pointer type
        typedef std::decay_t<Fx> dFx;
        typedef meta::Unqualified<Fx> uFx;
        dFx memfxptr(std::forward<Fx>(fx));
        auto userptr = detail::ptr(obj);
        void* userobjdata = static_cast<void*>(userptr);
        lua_CFunction freefunc = &function_detail::upvalue_member_function<std::decay_t<decltype(*userptr)>, uFx>::call;

        int upvalues = stack::stack_detail::push_as_upvalues(L, memfxptr);
        upvalues += stack::push(L, userobjdata);
        
        stack::push(L, freefunc, upvalues);
    }

    template<typename Fx>
    static void set_fx(std::false_type, lua_State* L, Fx&& fx) {
        std::decay_t<Fx> target(std::forward<Fx>(fx));
        lua_CFunction freefunc = &function_detail::upvalue_free_function<Fx>::call;

        int upvalues = stack::stack_detail::push_as_upvalues(L, target);
        stack::push(L, freefunc, upvalues);
    }

    static void set_fx(lua_State* L, std::unique_ptr<function_detail::base_function> luafunc) {
        function_detail::base_function* target = luafunc.release();
        void* targetdata = static_cast<void*>(target);
        lua_CFunction freefunc = function_detail::call;

        stack::push(L, userdata_value(targetdata));
        function_detail::free_function_cleanup(L);
        lua_setmetatable(L, -2);
        stack::push(L, freefunc, 1);
    }

    template<typename... Args>
    static int push(lua_State* L, Args&&... args) {
        // Set will always place one thing (function) on the stack
        set<Sigs...>(L, std::forward<Args>(args)...);
        return 1;
    }
};

template<typename T, typename... Args>
struct pusher<function_packer<T, Args...>> {
    template <std::size_t... I, typename FP>
    static int push_func(std::index_sequence<I...>, lua_State* L, FP&& fp) {
        return stack::push<T>(L, detail::forward_get<I>(fp.set)...);
    }

    template <typename FP>
    static int push(lua_State* L, FP&& fp) {
        return push_func(std::index_sequence_for<Args...>(), L, std::forward<FP>(fp));
    }
};

template<typename Signature>
struct pusher<std::function<Signature>> {
    static int push(lua_State* L, std::function<Signature> fx) {
        return pusher<function_sig<>>{}.push(L, std::move(fx));
    }
};

template<typename... Functions>
struct pusher<overload_set<Functions...>> {
    static int push(lua_State* L, overload_set<Functions...>&& set) {
        pusher<function_sig<>>{}.set_fx(L, std::make_unique<function_detail::overloaded_function<Functions...>>(std::move(set.set)));
        return 1;
    }

    static int push(lua_State* L, const overload_set<Functions...>& set) {
        pusher<function_sig<>>{}.set_fx(L, std::make_unique<function_detail::overloaded_function<Functions...>>(set.set));
        return 1;
    }
};

template<typename Signature>
struct getter<std::function<Signature>> {
    typedef meta::function_traits<Signature> fx_t;
    typedef typename fx_t::args_type args_types;
    typedef meta::tuple_types<typename fx_t::return_type> return_types;

    template<typename... Args, typename... Ret>
    static std::function<Signature> get_std_func(types<Ret...>, types<Args...>, lua_State* L, int index = -1) {
        sol::function f(L, index);
        auto fx = [f, L, index](Args&&... args) -> meta::return_type_t<Ret...> {
            return f.call<Ret...>(std::forward<Args>(args)...);
        };
        return std::move(fx);
    }

    template<typename... FxArgs>
    static std::function<Signature> get_std_func(types<void>, types<FxArgs...>, lua_State* L, int index = -1) {
        sol::function f(L, index);
        auto fx = [f, L, index](FxArgs&&... args) -> void {
            f(std::forward<FxArgs>(args)...);
        };
        return std::move(fx);
    }

    template<typename... FxArgs>
    static std::function<Signature> get_std_func(types<>, types<FxArgs...> t, lua_State* L, int index = -1) {
        return get_std_func(types<void>(), t, L, index);
    }

    static std::function<Signature> get(lua_State* L, int index) {
        return get_std_func(return_types(), args_types(), L, index);
    }
};
} // stack
} // sol

#endif // SOL_FUNCTION_HPP
