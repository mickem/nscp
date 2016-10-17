#pragma once

/** @file clr_scoped_ptr.h
 ** @author R Benjamin Voigt (richardvoigt@gmail.com)
 ** 
 ** Rights reserved.  This code is not public domain.
 ** 
 ** Licensed under CC BY-SA 3.0             http://creativecommons.org/licenses/by-sa/3.0/
 **             or Lesser GPL 3 or later    http://www.gnu.org/copyleft/lesser.html
 ** with the following restrictions (per GPL section 7):
 **  - all warranties are disclaimed, and if this is prohibited by law, your sole remedy shall be recovery of the price you paid to receive the code
 **  - derived works must not remove this license notice or author attribution
 **  - modifications must not be represented as the work of the original author
 **  - attribution is required in the "about" or "--version" display of any work linked hereto, or wherever copyright notices are displayed by the composite work
 **/

namespace clr {

struct safe_bool { private: safe_bool(); };

/** \brief C++/CLI analogue to boost::scoped_ptr, also similar to std::unique_ptr, for management of the lifetime of an unmanaged class instance by a managed object
 **/
template<typename T>
public ref class clr_scoped_ptr
{
    T* m_native_ptr;

    // declare copy-constructors and assignment operators to prevent double-free
    clr_scoped_ptr( clr_scoped_ptr<T>% ) /* = delete */ { throw gcnew System::InvalidOperationException("clr_scoped_ptr is non-copyable"); }

    template<typename U>
    clr_scoped_ptr( clr_scoped_ptr<U>% ) { throw gcnew System::InvalidOperationException("clr_scoped_ptr is non-copyable"); }

    clr_scoped_ptr% operator=( clr_scoped_ptr<T>% ) /* = delete */ { throw gcnew System::InvalidOperationException("clr_scoped_ptr is non-copyable"); }

    template<typename U>
    clr_scoped_ptr% operator=( clr_scoped_ptr<U>% ) { throw gcnew System::InvalidOperationException("clr_scoped_ptr is non-copyable"); }

public:
    clr_scoped_ptr( void ) : m_native_ptr(nullptr) {}
    explicit clr_scoped_ptr( T* ptr ) : m_native_ptr(ptr) {}
    !clr_scoped_ptr( void ) { reset(); }
    ~clr_scoped_ptr( void ) { clr_scoped_ptr::!clr_scoped_ptr(); }

    template<typename U>
    clr_scoped_ptr( U ptr ) : m_native_ptr(ptr) {}

    void reset( T* ptr ) { delete m_native_ptr; m_native_ptr = ptr; }
    void reset( void ) { reset(nullptr); }

    clr_scoped_ptr% operator=( T* ptr ) { reset(ptr); return *this;}
/*
    template<typename U>
    clr_scoped_ptr% operator=( U ptr ) { reset(ptr); return *this;}
*/
    operator struct safe_bool*() { return reinterpret_cast<struct safe_bool*>(m_native_ptr); }

    void swap( clr_scoped_ptr<T>% other )
    {
        using std::swap;
        swap(m_native_ptr, other.m_native_ptr);
    }

    T* release( void ) { T* retval = m_native_ptr; m_native_ptr = nullptr; return retval; }
    T* get( void ) { return m_native_ptr; }

    static T* operator->( clr_scoped_ptr<T>% sptr ) { return sptr.get(); }
    static T& operator*( clr_scoped_ptr<T>% sptr ) { return *sptr.get(); }
};

template<typename T>
inline void swap( clr_scoped_ptr<T>% left, clr_scoped_ptr<T>% right )
{
    left.swap(right);
}
}