/* Horizon cannot map CodeMemory (the ILP32 guest's data/stack) into a
 * service. Stage only those HIPC map-alias buffers in ordinary host heap.
 * Pointer descriptors remain kernel copies; normal host buffers stay zero-copy.
 * This also covers libnx/Mesa parcels built on a guest stack. */
#include <switch.h>
#include <stdlib.h>
#include <string.h>

#define IPC_COMMAND_SIZE 0x100
#define IPC_MAX_BUFFERS 20 /* (256-byte TLS command - 8-byte header) / 12 */

struct ipc_buffer {
    HipcBufferDescriptor *descriptor;
    HipcBufferDescriptor original;
    uintptr_t begin, end;
    unsigned group;
    bool output;
};
struct ipc_group { uintptr_t begin, end; void *storage; };

extern Result __real_svcSendSyncRequest(Handle session);
static uint64_t staged_requests;
uint64_t host_ipc_staged_requests(void)
{
    return __atomic_load_n(&staged_requests, __ATOMIC_RELAXED);
}

/* Check the entire range before copying, including ranges crossing mappings.
 * Never turn an invalid guest output pointer into an apparent successful call. */
static Result ipc_needs_staging(uintptr_t begin, size_t size, bool output, bool *stage)
{
    *stage = false;
    if (!size) return 0;
    if (size > UINTPTR_MAX - begin)
        return MAKERESULT(Module_Kernel, KernelError_InvalidMemoryState);
    uintptr_t end = begin + size;
    for (uintptr_t cursor = begin; cursor < end; ) {
        MemoryInfo info;
        u32 page;
        Result rc = svcQueryMemory(&info, &page, cursor);
        if (R_FAILED(rc)) return rc;
        if (info.addr > cursor || info.size > UINTPTR_MAX - info.addr ||
            info.addr + info.size <= cursor || !(info.perm & Perm_R) ||
            (output && !(info.perm & Perm_W)))
            return MAKERESULT(Module_Kernel, KernelError_InvalidMemoryState);
        if (info.type == MemType_CodeReadOnly || info.type == MemType_CodeWritable)
            *stage = true;
        cursor = info.addr + info.size;
    }
    return 0;
}

Result __wrap_svcSendSyncRequest(Handle session)
{
    unsigned char *command = armGetTls();
    HipcHeader header;
    memcpy(&header, command, sizeof(header));
    unsigned total = header.num_send_buffers + header.num_recv_buffers + header.num_exch_buffers;
    if (!total) return __real_svcSendSyncRequest(session);

    /* Validate descriptor bounds before hipcParseRequest follows the header.
     * Leave unusual/malformed requests to the kernel, without touching them. */
    size_t offset = sizeof(header);
    if (header.has_special_header) {
        HipcSpecialHeader special;
        memcpy(&special, command + offset, sizeof(special));
        offset += sizeof(special) + (special.send_pid ? 8 : 0) +
            4 * (special.num_copy_handles + special.num_move_handles);
    }
    offset += header.num_send_statics * sizeof(HipcStaticDescriptor);
    size_t descriptors_end = offset + total * sizeof(HipcBufferDescriptor);
    size_t receive_count = header.recv_static_mode == 2 ? 1 :
        (header.recv_static_mode > 2 ? header.recv_static_mode - 2 : 0);
    size_t message_end = descriptors_end + 4 * header.num_data_words +
        receive_count * sizeof(HipcRecvListEntry);
    if (total > IPC_MAX_BUFFERS || message_end > IPC_COMMAND_SIZE)
        return __real_svcSendSyncRequest(session);

    HipcParsedRequest request = hipcParseRequest(command);
    HipcBufferDescriptor *descriptors = (HipcBufferDescriptor *)(command + offset);
    struct ipc_buffer buffers[IPC_MAX_BUFFERS];
    struct ipc_group groups[IPC_MAX_BUFFERS];
    unsigned count = 0, group_count = 0;
    Result rc = 0;
    for (unsigned i = 0; i < total; i++) {
        HipcBufferDescriptor *desc = descriptors + i;
        uintptr_t address = (uintptr_t)hipcGetBufferAddress(desc);
        size_t size = hipcGetBufferSize(desc);
        bool output = i >= request.meta.num_send_buffers, stage;
        /* Only the guest's <4-GiB aliases need staging. Host heap, stacks and
         * driver allocations keep the original kernel validation/fast path. */
        if (!size || address >= UINT64_C(0x100000000)) continue;
        rc = ipc_needs_staging(address, size, output, &stage);
        if (R_FAILED(rc)) return rc;
        if (!stage) continue;
        buffers[count++] = (struct ipc_buffer){desc, *desc, address, address + size, 0, output};
    }
    if (!count) return __real_svcSendSyncRequest(session);

    /* Sort/union overlapping spans so input/output aliases retain their shared
     * bytes and the order of descriptor copy-back cannot discard an update. */
    for (unsigned i = 1; i < count; i++) {
        struct ipc_buffer value = buffers[i];
        unsigned j = i;
        while (j && buffers[j - 1].begin > value.begin) {
            buffers[j] = buffers[j - 1]; j--;
        }
        buffers[j] = value;
    }
    for (unsigned i = 0; i < count; i++) {
        if (!group_count || buffers[i].begin >= groups[group_count - 1].end) {
            groups[group_count++] = (struct ipc_group){buffers[i].begin, buffers[i].end, NULL};
        } else if (buffers[i].end > groups[group_count - 1].end) {
            groups[group_count - 1].end = buffers[i].end;
        }
        buffers[i].group = group_count - 1;
    }
    for (unsigned i = 0; i < group_count; i++) {
        size_t size = groups[i].end - groups[i].begin;
        groups[i].storage = malloc(size);
        if (!groups[i].storage) {
            rc = MAKERESULT(Module_Kernel, KernelError_OutOfMemory);
            goto cleanup;
        }
        /* Seed outputs too: services may write only part of a receive buffer. */
        memcpy(groups[i].storage, (void *)groups[i].begin, size);
    }
    for (unsigned i = 0; i < count; i++) {
        struct ipc_buffer *b = buffers + i;
        struct ipc_group *g = groups + b->group;
        *b->descriptor = hipcMakeBuffer((char *)g->storage + (b->begin - g->begin),
            b->end - b->begin, b->original.mode);
    }
    __atomic_add_fetch(&staged_requests, 1, __ATOMIC_RELAXED);
    rc = __real_svcSendSyncRequest(session);
    if (R_SUCCEEDED(rc)) {
        for (unsigned i = 0; i < count; i++) {
            struct ipc_buffer *b = buffers + i;
            struct ipc_group *g = groups + b->group;
            if (b->output)
                memcpy((void *)b->begin, (char *)g->storage + (b->begin - g->begin), b->end - b->begin);
        }
        /* TLS now holds the RESPONSE. Restoring request descriptors here would
         * overwrite returned handles/data. Keep it exactly as the kernel left it. */
    } else {
        /* No valid response on a failed SVC; remove pointers to freed staging. */
        for (unsigned i = 0; i < count; i++)
            *buffers[i].descriptor = buffers[i].original;
    }
cleanup:
    for (unsigned i = 0; i < group_count; i++) free(groups[i].storage);
    return rc;
}
