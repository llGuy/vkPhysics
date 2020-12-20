#include "vk_context.hpp"
#include <common/allocators.hpp>

namespace vk {

ctx_t *g_ctx = NULL;

void allocate_context() {
    g_ctx = flmalloc<ctx_t>();
}

void deallocate_context() {
    flfree(g_ctx);
    g_ctx = NULL;
}

}
