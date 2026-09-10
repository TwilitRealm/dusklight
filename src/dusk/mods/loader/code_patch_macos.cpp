#include "code_patch_macos.hpp"

#include <libkern/OSCacheControl.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

#define PATCH_CODE                                                                                 \
    __attribute__((section("__TEXT,__code_patch,regular,pure_instructions"), noinline))

extern const char kPatchBegin[] asm("section$start$__TEXT$__code_patch");
extern const char kPatchEnd[] asm("section$end$__TEXT$__code_patch");

namespace {

constexpr unsigned kMaxThreads = 1024;
constexpr unsigned kMaxAttempts = 16;
constexpr size_t kMaxPatchSize = 16;
pthread_mutex_t sPatchMutex = PTHREAD_MUTEX_INITIALIZER;

struct ThreadList {
    thread_act_array_t threads = nullptr;
    mach_msg_type_number_t count = 0;
};

PATCH_CODE void release_threads(ThreadList& list) {
    for (unsigned i = 0; i < list.count; ++i) {
        mach_port_deallocate(mach_task_self(), list.threads[i]);
    }
    if (list.threads != nullptr) {
        vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(list.threads),
            list.count * sizeof(thread_t));
    }
}

PATCH_CODE kern_return_t read_pc(thread_t thread, uintptr_t& pc) {
#if defined(__aarch64__)
    arm_thread_state64_t state{};
    mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
    const auto result = thread_get_state(
        thread, ARM_THREAD_STATE64, reinterpret_cast<thread_state_t>(&state), &count);
    pc = arm_thread_state64_get_pc(state);
#elif defined(__x86_64__)
    x86_thread_state64_t state{};
    mach_msg_type_number_t count = x86_THREAD_STATE64_COUNT;
    const auto result = thread_get_state(
        thread, x86_THREAD_STATE64, reinterpret_cast<thread_state_t>(&state), &count);
    pc = state.__rip;
#else
#error Unsupported macOS architecture
#endif
    return result;
}

PATCH_CODE __attribute__((aligned(16384))) kern_return_t commit_patch(uintptr_t target,
    const unsigned char* expected, const unsigned char* replacement, size_t size, uintptr_t page,
    size_t pageSize, thread_t currentThread) {
    ThreadList initial;
    auto result = task_threads(mach_task_self(), &initial.threads, &initial.count);
    if (result != KERN_SUCCESS) {
        return result;
    }
    thread_t suspended[kMaxThreads];
    unsigned suspendedCount = 0;
    if (initial.count > kMaxThreads) {
        release_threads(initial);
        return KERN_RESOURCE_SHORTAGE;
    }

    for (unsigned i = 0; i < initial.count; ++i) {
        const auto thread = initial.threads[i];
        if (thread == currentThread) {
            continue;
        }
        result = thread_suspend(thread);
        if (result != KERN_SUCCESS) {
            break;
        }
        suspended[suspendedCount++] = thread;
        uintptr_t pc = 0;
        result = read_pc(thread, pc);
        if (result != KERN_SUCCESS) {
            break;
        }
        if (pc >= target && pc < target + size) {
            result = KERN_ABORTED;
            break;
        }
    }

    if (result == KERN_SUCCESS) {
        ThreadList current;
        result = task_threads(mach_task_self(), &current.threads, &current.count);
        if (result == KERN_SUCCESS) {
            for (unsigned i = 0; i < current.count; ++i) {
                bool known = false;
                for (unsigned j = 0; j < initial.count; ++j) {
                    known |= current.threads[i] == initial.threads[j];
                }
                if (!known) {
                    result = KERN_ABORTED;
                    break;
                }
            }
        }
        release_threads(current);
    }

    if (result == KERN_SUCCESS) {
        const auto* bytes = reinterpret_cast<const volatile unsigned char*>(target);
        for (size_t i = 0; i < size; ++i) {
            if (bytes[i] != expected[i]) {
                result = KERN_INVALID_VALUE;
                break;
            }
        }
    }

    if (result == KERN_SUCCESS) {
        result = mach_vm_protect(
            mach_task_self(), page, pageSize, false, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
        if (result == KERN_SUCCESS) {
            auto* bytes = reinterpret_cast<volatile unsigned char*>(target);
            for (size_t i = 0; i < size; ++i) {
                bytes[i] = replacement[i];
            }
            sys_icache_invalidate(reinterpret_cast<void*>(target), size);
            result = mach_vm_protect(
                mach_task_self(), page, pageSize, false, VM_PROT_READ | VM_PROT_EXECUTE);
            if (result != KERN_SUCCESS) {
                for (size_t i = 0; i < size; ++i) {
                    bytes[i] = expected[i];
                }
                sys_icache_invalidate(reinterpret_cast<void*>(target), size);
                if (mach_vm_protect(mach_task_self(), page, pageSize, false,
                        VM_PROT_READ | VM_PROT_EXECUTE) != KERN_SUCCESS)
                {
                    __builtin_trap();  // Can't resume into non-executable code
                }
            }
        }
    }

    for (unsigned i = 0; i < suspendedCount; ++i) {
        if (thread_resume(suspended[i]) != KERN_SUCCESS) {
            __builtin_trap();
        }
    }
    release_threads(initial);
    return result;
}

}  // namespace

extern "C" int commit_code_patch(
    void* targetPointer, const void* expected, const void* replacement, size_t size) {
    if (targetPointer == nullptr || expected == nullptr || replacement == nullptr || size == 0 ||
        size > kMaxPatchSize)
    {
        return KERN_INVALID_ARGUMENT;
    }
    const auto target = reinterpret_cast<uintptr_t>(targetPointer);
    const size_t pageSize = vm_page_size;
    if (target > UINTPTR_MAX - size - pageSize) {
        return KERN_INVALID_ADDRESS;
    }
    const auto page = target & ~(pageSize - 1);
    const size_t length = ((target + size + pageSize - 1) & ~(pageSize - 1)) - page;
    if (page < reinterpret_cast<uintptr_t>(kPatchEnd) &&
        page + length > reinterpret_cast<uintptr_t>(kPatchBegin))
    {
        return KERN_PROTECTION_FAILURE;
    }

    unsigned char oldCode[kMaxPatchSize];
    unsigned char newCode[kMaxPatchSize];
    for (size_t i = 0; i < size; ++i) {
        oldCode[i] = static_cast<const unsigned char*>(expected)[i];
        newCode[i] = static_cast<const unsigned char*>(replacement)[i];
    }

    pthread_mutex_lock(&sPatchMutex);
    mach_vm_address_t region = page;
    mach_vm_size_t regionSize = 0;
    vm_region_basic_info_data_64_t info{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    auto result = mach_vm_region(mach_task_self(), &region, &regionSize, VM_REGION_BASIC_INFO_64,
        reinterpret_cast<vm_region_info_t>(&info), &count, &object);
    if (object != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), object);
    }
    if (result == KERN_SUCCESS && (region > page || regionSize < page + length - region ||
                                      info.protection != (VM_PROT_READ | VM_PROT_EXECUTE)))
    {
        result = KERN_PROTECTION_FAILURE;
    }
    if (result == KERN_SUCCESS) {
        const auto currentThread = mach_thread_self();
        uintptr_t pc = 0;
        read_pc(currentThread, pc);
        thread_suspend(MACH_PORT_NULL);
        thread_resume(MACH_PORT_NULL);
        vm_deallocate(mach_task_self(), 0, 0);
        mach_port_deallocate(mach_task_self(), MACH_PORT_NULL);
        sys_icache_invalidate(targetPointer, size);
        result = mach_vm_protect(mach_task_self(), page, length, false, info.protection);
        if (result == KERN_SUCCESS) {
            for (unsigned attempt = 0; attempt < kMaxAttempts; ++attempt) {
                result = commit_patch(target, oldCode, newCode, size, page, length, currentThread);
                if (result != KERN_ABORTED || attempt + 1 == kMaxAttempts) {
                    break;
                }
                usleep(1000);
            }
        }
        mach_port_deallocate(mach_task_self(), currentThread);
    }
    pthread_mutex_unlock(&sPatchMutex);
    return result;
}
