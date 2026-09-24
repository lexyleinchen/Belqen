#ifndef BPE_H
#define BPE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BPE_MAGIC0 'B'
#define BPE_MAGIC1 'P'
#define BPE_MAGIC2 'E'
#define BPE_MAGIC3 '1'
#define BPE_VERSION 1
#define BPE_MACHINE_X86_64 1
#define BPE_FLAG_CODE_EXECUTABLE 0x0001
#define BPE_FLAG_DATA_WRITABLE 0x0002
#define BPE_DEFAULT_STACK_PAGES 1

typedef struct {
    uint32_t magic[4];
    uint16_t version;
    uint16_t machine;
    uint16_t header_size;
    uint16_t flags;
    uint64_t entry_offset;
    uint64_t code_offset;
    uint64_t code_size;
    uint64_t data_offset;
    uint64_t data_size;
    uint64_t bss_size;
    uint64_t image_size;
    uint32_t stack_pages;
    uint32_t reserved;
} __attribute__((packed)) BpeHeader;

#ifdef __cplusplus
}
#endif

#endif // BPE_H