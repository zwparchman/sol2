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

#ifndef SOL_THREAD_HPP
#define SOL_THREAD_HPP

#include "reference.hpp"
#include "stack.hpp"

namespace sol {
class thread : public reference {
public:
    using reference::reference;

    state_view state() const {
        return state_view(this->thread_state());
    }

    lua_State* thread_state () const {
        auto pp = stack::push_pop(*this);
        lua_State* lthread = lua_tothread(lua_state(), -1);
        return lthread;
    }

    thread_status status () const {
        lua_State* lthread = thread_state();
        thread_status lstat = static_cast<thread_status>(lua_status(lthread));
        if (lstat != thread_status::normal && lua_gettop(lthread) == 0) {
            // No thing on the thread's stack means its dead
            return thread_status::dead;
        }
        return lstat;
    }

    thread create () {
        return create(lua_state());
    }

    static thread create (lua_State* L) {
        lua_newthread(L);
        thread result(L);
        lua_pop(L, 1);
        return result;
    }
};
} // sol

#endif // SOL_THREAD_HPP
