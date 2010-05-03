#ifndef _S2E_BLOCK_H_

#define _S2E_BLOCK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <block.h>
#include <stdint.h>

typedef int (*s2e_raw_read)(struct BlockDriverState *bs, int64_t sector_num,
                    uint8_t *buf, int nb_sectors);

/* Disk-related copy on write */
int s2e_bdrv_read(struct BlockDriverState *bs, int64_t sector_num,
                  uint8_t *buf, int nb_sectors,
                  int *fallback,
                  s2e_raw_read fb);

int s2e_bdrv_write(struct BlockDriverState *bs, int64_t sector_num,
                   const uint8_t *buf, int nb_sectors);

BlockDriverAIOCB *s2e_bdrv_aio_read(struct BlockDriverState *bs, int64_t sector_num,
                                    uint8_t *buf, int nb_sectors,
                                    BlockDriverCompletionFunc *cb, void *opaque,
                                    int *fallback, s2e_raw_read f);

struct BlockDriverAIOCB *s2e_bdrv_aio_write(
                                     struct BlockDriverState *bs, int64_t sector_num,
                                     const uint8_t *buf, int nb_sectors,
                                     BlockDriverCompletionFunc *cb, void *opaque);

extern int (*__hook_bdrv_read)(
                  struct BlockDriverState *bs, int64_t sector_num,
                  uint8_t *buf, int nb_sectors,
                  int *fallback,
                  s2e_raw_read fb);

extern int (*__hook_bdrv_write)(
                   struct BlockDriverState *bs, int64_t sector_num,
                   const uint8_t *buf, int nb_sectors);


extern struct BlockDriverAIOCB* (*__hook_bdrv_aio_read)(
    struct BlockDriverState *bs, int64_t sector_num,
   uint8_t *buf, int nb_sectors,
   BlockDriverCompletionFunc *cb, void *opaque,
   int *fallback, s2e_raw_read fb);

extern struct  BlockDriverAIOCB* (*__hook_bdrv_aio_write)(
   BlockDriverState *bs, int64_t sector_num,
   const uint8_t *buf, int nb_sectors,
   BlockDriverCompletionFunc *cb, void *opaque);


extern struct S2EExecutionState **g_block_s2e_state;

#ifdef __cplusplus
}
#endif

#endif
