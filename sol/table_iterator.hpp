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

#ifndef SOL_TABLE_ITERATOR_HPP
#define SOL_TABLE_ITERATOR_HPP

#include "object.hpp"
#include <iterator>

namespace sol {

class table_iterator : public std::iterator<std::input_iterator_tag, std::pair<object, object>> {
private:
    typedef std::iterator<std::input_iterator_tag, std::pair<object, object>> base_t;
public:
    typedef object key_type;
    typedef object mapped_type;
    typedef base_t::value_type value_type;
    typedef base_t::iterator_category iterator_category;
    typedef base_t::difference_type difference_type;
    typedef base_t::pointer pointer;
    typedef base_t::reference reference;
    typedef const value_type& const_reference;

private:
    typedef ::sol::reference ref_t;
    std::pair<object, object> kvp;
    ref_t ref;
    int tableidx = 0;
    std::ptrdiff_t idx = 0;

public:

    table_iterator () : idx(-1) {

    }

    table_iterator(ref_t x) : ref(std::move(x)) {
        ref.push();
        tableidx = lua_gettop(ref.lua_state());
        stack::push(ref.lua_state(), nil);
        this->operator++();
        --idx;
    }

    table_iterator& operator++() {
        if (idx == -1)
            return *this;

        if (lua_next(ref.lua_state(), tableidx) == 0) {
            idx = -1;
            return *this;
        }
        ++idx;
        kvp.first = object(ref.lua_state(), -2);
        kvp.second = object(ref.lua_state(), -1);
        lua_pop(ref.lua_state(), 1);
        // leave key on the stack
        return *this;
    }

    table_iterator operator++(int) {
        auto saved = *this;
        this->operator++();
        return saved;
    }

    reference operator*() {
        return kvp;
    }

    const_reference operator*() const {
        return kvp;
    }

    bool operator== (const table_iterator& right) const {
        return idx == right.idx;
    }

    bool operator!= (const table_iterator& right) const {
        return idx != right.idx;
    }

    ~table_iterator() {
        if (ref.valid()) {
            stack::remove(ref.lua_state(), tableidx, 1);
        }
    }
};

} // sol

#endif // SOL_TABLE_ITERATOR_HPP
