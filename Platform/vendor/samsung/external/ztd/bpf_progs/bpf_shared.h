
#ifndef BPF_SHARED_H
#define BPF_SHARED_H

#include <bpf_helpers.h>

#define bpf_printk(fmt, ...)                                       \
    ({                                                             \
        char ____fmt[] = fmt;                                      \
        bpf_trace_printk(____fmt, sizeof(____fmt), ##__VA_ARGS__); \
    })

#define DEFINE_ZT_TRACEPOINT(the_system, the_event) \
    DEFINE_BPF_PROG("tracepoint/" #the_system "/" #the_event, AID_ROOT, AID_SYSTEM, tp_##the_event)

#define DEFINE_ZT_KPROBE(the_ksymbol) \
    DEFINE_BPF_PROG("kprobe/" #the_ksymbol, AID_ROOT, AID_SYSTEM, kp##the_ksymbol)

#define DEFINE_BPF_SHARED_MAP_UGM(the_map, TYPE, KeyType, ValueType, num_entries, usr, grp, md) \
    DEFINE_BPF_MAP_EXT(the_map, TYPE, KeyType, ValueType, num_entries, usr, grp, md,            \
                       DEFAULT_BPF_MAP_SELINUX_CONTEXT, DEFAULT_BPF_MAP_PIN_SUBDIR, SHARED,       \
                       BPFLOADER_MIN_VER, BPFLOADER_MAX_VER, /*ignore_on_eng*/LOAD_ON_ENG,            \
                       /*ignore_on_user*/LOAD_ON_USER, /*ignore_on_userdebug*/LOAD_ON_USERDEBUG)

#define DEFINE_BPF_SHARED_MAP_GRW(the_map, TYPE, KeyType, ValueType, num_entries, gid) \
    DEFINE_BPF_SHARED_MAP_UGM(the_map, TYPE, KeyType, ValueType, num_entries,          \
                       DEFAULT_BPF_MAP_UID, gid, 0660)

/*
 * POINTER_OF_USER_SPACE must be used only by 'bpf_probe_read_user/str'.
 * It does not work for 'bpf_probe_read_kernel'
 */
#define POINTER_OF_USER_SPACE(reg) ((void*)(reg & 0x0000FFFFFFFFFFFF))

#define TRACEPOINT_RETURN 1     // return 1 to avoid blocking simpleperf from receiving events

typedef struct {
    uint32_t value:12;
} uint12_t;

#endif // BPF_SHARED_H
