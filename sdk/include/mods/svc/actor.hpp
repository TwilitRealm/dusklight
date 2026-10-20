#pragma once

#include "actor.h"
#include "f_op/f_op_actor_mng.h"

namespace mods::actor {

/**
 * Concept that defines an actor.
 */
template <typename T>
concept Actor = std::is_base_of_v<fopAc_ac_c, T>;

namespace detail {
template <Actor T>
static cPhs_Step FunctionCreate(void* i_this) {
    auto thisPtr = static_cast<T*>(i_this);
    fopAcM_ct(thisPtr, T);
    return thisPtr->Create();
}

template <Actor T>
static int FunctionDelete(void* i_this) {
    auto thisPtr = static_cast<T*>(i_this);
    return thisPtr->Delete();
}

template <Actor T>
static int FunctionIsDelete(void* i_this) {
    auto thisPtr = static_cast<T*>(i_this);
    return thisPtr->IsDelete();
}

template <Actor T>
static int FunctionExecute(void* i_this) {
    auto thisPtr = static_cast<T*>(i_this);
    return thisPtr->Execute();
}

template <Actor T>
static int FunctionDraw(void* i_this) {
    auto thisPtr = static_cast<T*>(i_this);
    return thisPtr->Draw();
}
}  // namespace detail

/**
 * Helper to fill out some boilerplate on @c ActorProfileDesc for C++ classes.
 *
 * @remarks Automatically fills out <tt>process_size</tt>, and fills out the following functions if a
 * suitable member function exists on the class:
 * - create_function: <tt>int Create()</tt>. Note that the constructor is called automatically
 * beforehand.
 * - delete_function: <tt>int Delete()</tt>.
 * - is_delete_function: <tt>int IsDelete()</tt>.
 * - execute_function: <tt>int Execute()</tt>.
 * - draw_function: <tt>int Draw()</tt>.
 * @tparam T The type of actor to register for.
 * @param desc Description struct detailing fields not set by this object.
 * @return A copy of @c desc with the specified info being filled in.
 */
template <Actor T>
constexpr ActorProfileDesc FillInfo(ActorProfileDesc desc) {
    desc.process_size = sizeof(T);
    if constexpr (requires(T* a) { a->Create(); }) {
        desc.create_function = detail::FunctionCreate<T>;
    }
    if constexpr (requires(T* a) { a->Delete(); }) {
        desc.delete_function = detail::FunctionDelete<T>;
    }
    if constexpr (requires(T* a) { a->IsDelete(); }) {
        desc.is_delete_function = detail::FunctionIsDelete<T>;
    }
    if constexpr (requires(T* a) { a->Execute(); }) {
        desc.execute_function = detail::FunctionExecute<T>;
    }
    if constexpr (requires(T* a) { a->Draw(); }) {
        desc.draw_function = detail::FunctionDraw<T>;
    }
    return desc;
}

}  // namespace mods::actor