/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2013, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * All contributors are listed in the S2E-AUTHORS file.
 */

/**
 *  The S2E VM image format.
 *
 *  Traditional image formats are not suitable for multi-path execution, because
 *  they usually mutate internal bookkeeping structures on read operations.
 *  Worse, they write these mutations back to the disk image file, causing
 *  VM image corruptions. QCOW2 is one example of such formats.
 *
 *  The S2E image format, unlike the other formats, is multi-path aware.
 *  When in S2E mode, writes are local to each state and do not clobber other states.
 *  Moreover, writes are NEVER written on the image. This makes it possible
 *  to share one disk image among many instances of S2E.
 *
 *  The S2E image format is identical to the RAW format, except that the
 *  image file name has the ".s2e" extension. Therefore, to convert from
 *  RAW to S2E, renaming the file is enough (a symlink is fine too).
 *
 *  Snapshots are stored in a separate file, suffixed by the name of the
 *  snapshot. For example, if the base image is called "my_image.raw.s2e",
 *  the snapshot "ready" (as in "savevm ready") will be saved in the file
 *  "my_image.raw.s2e.ready" in the same folder as "my_image.raw.s2e".
 *
 *  If the base image is modified, all snapshots become invalid.
 */

#include <dirent.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "qemu-common.h"
#include "block_int.h"
#include "module.h"

int (*__hook_bdrv_read)(struct BlockDriverState *bs, int64_t sector_num,
                  uint8_t *buf, int nb_sectors);

int (*__hook_bdrv_write)(struct BlockDriverState *bs, int64_t sector_num,
                   const uint8_t *buf, int nb_sectors);

static const unsigned int S2EB_L2_SIZE = 65536;
static const unsigned int S2EB_SECTOR_SIZE = BDRV_SECTOR_SIZE;
static const unsigned int S2EB_L2_SECTORS = S2EB_L2_SIZE / S2EB_SECTOR_SIZE;

typedef uint64_t bitmap_entry_t;
static const unsigned int S2EB_BITS_PER_ENTRY = sizeof(bitmap_entry_t) * 8;


typedef struct S2EBLKL2 {
    /* The sectors that differ from the base snapshot */
    bitmap_entry_t dirty_bitmap[S2EB_L2_SECTORS / S2EB_BITS_PER_ENTRY];
    uint8_t block[S2EB_L2_SIZE];
} S2EBLKL2;


typedef struct BDRVS2EState {
    uint64_t sector_count;
    uint64_t dirty_count;
    uint64_t l1_entries;
    S2EBLKL2 **l1;

    /* Temporary vm state */
    uint8_t *snapshot_vmstate;
    size_t snapshot_vmstate_size;
} BDRVS2EState;

typedef struct S2ESnapshotHeader {
    /* Used to detect changes in the base image */
    uint64_t base_image_timestamp;

    uint64_t sector_map_offset;
    uint64_t sector_map_entries;
    uint64_t sectors_start;

    /* The VM state is saved after the disk data */
    uint64_t vmstate_start; /* In sectors */
    uint64_t vmstate_size; /* In bytes */

    char id_str[128]; /* unique snapshot id */
    /* the following fields are informative. They are not needed for
       the consistency of the snapshot */
    char name[256]; /* user chosen name */

    uint32_t date_sec; /* UTC date of the snapshot */
    uint32_t date_nsec;
    uint64_t vm_clock_nsec; /* VM clock relative to boot */
} S2ESnapshotHeader;

static void s2e_blk_init(BlockDriverState *bs)
{
    BDRVS2EState *s = bs->opaque;

    s->snapshot_vmstate_size = 0;
    s->snapshot_vmstate = NULL;

    /* Initialize the copy-on-write page table */
    uint64_t length = bdrv_getlength(bs) & BDRV_SECTOR_MASK;
    s->l1_entries = length / S2EB_L2_SECTORS;
    s->l1 = g_malloc0(sizeof(*s->l1) * s->l1_entries);

    s->sector_count = length / S2EB_SECTOR_SIZE;
}

static int s2e_open(BlockDriverState *bs, int flags)
{
    s2e_blk_init(bs);
    return 0;
}

static int s2e_blk_is_dirty(BDRVS2EState *s, uint64_t sector_num)
{
    uint64_t l1_index = sector_num / S2EB_L2_SECTORS;
    uint64_t l2_index = sector_num % S2EB_L2_SECTORS;
    if (!s->l1[l1_index]) {
        return 0;
    }

    uint64_t word = s->l1[l1_index]->dirty_bitmap[l2_index / S2EB_BITS_PER_ENTRY];
    if (word & (1LL << (l2_index % S2EB_BITS_PER_ENTRY))) {
        return 1;
    }
    return 0;
}

static void s2e_blk_copy_and_set_dirty(BDRVS2EState *s, uint8_t *buffer, uint64_t sector_num, unsigned count)
{
    uint64_t l1_index = sector_num / S2EB_L2_SECTORS;
    uint64_t l2_index = sector_num % S2EB_L2_SECTORS;

    assert(l2_index + count <= S2EB_L2_SECTORS);

    if (!s->l1[l1_index]) {
        s->l1[l1_index] = g_malloc0(sizeof(S2EBLKL2));
    }

    S2EBLKL2 *b = s->l1[l1_index];

    uint64_t idx = l2_index;
    uint64_t cnt = count;
    while (cnt > 0) {
        bitmap_entry_t mask = 1LL << (idx % S2EB_BITS_PER_ENTRY);
        unsigned entry_idx = idx / S2EB_BITS_PER_ENTRY;

        if (!(b->dirty_bitmap[entry_idx] & mask)) {
            s->dirty_count++;
        }

        b->dirty_bitmap[entry_idx] |= mask;
        ++idx;
        --cnt;
    }

    uint8_t *dest = &b->block[l2_index * S2EB_SECTOR_SIZE];
    memcpy(dest, buffer, count * S2EB_SECTOR_SIZE);
}

static int s2e_read_dirty(BDRVS2EState *s, uint8_t *buffer, uint64_t sector_num, int nb_sectors)
{
    /* Check for copy-on-write data */
    int found_dirty = 0;

    while (nb_sectors > 0) {
        uint64_t l1_index = ((uint64_t) sector_num) / S2EB_L2_SECTORS;
        uint64_t l2_index = ((uint64_t) sector_num) % S2EB_L2_SECTORS;

        /* Quick check if the entire page is non-dirty */
        if (!s->l1[l1_index]) {
            /* increment may be bigger than nb_sector, but it's ok,
               nb_sectors is signed. */
            uint64_t increment = S2EB_L2_SECTORS - l2_index;
            nb_sectors -= increment;
            sector_num += increment;
            buffer += increment * BDRV_SECTOR_SIZE;
            continue;
        }

        if (s2e_blk_is_dirty(s, sector_num)) {
            uint8_t *data = s->l1[l1_index]->block;
            memcpy(buffer, &data[l2_index * BDRV_SECTOR_SIZE], BDRV_SECTOR_SIZE);
            found_dirty = 1;
        }

        buffer += BDRV_SECTOR_SIZE;
        nb_sectors--;
        sector_num++;
    }

    return found_dirty;
}

/* This is for S2E mode, add latest writes from the current state */
static int s2e_read_dirty_klee(BlockDriverState *bs, uint8_t *buffer, uint64_t sector_num, unsigned nb_sectors)
{
    if (!__hook_bdrv_read) {
        return 0;
    }

    int found_dirty = false;

    while (nb_sectors) {
        int read_count = __hook_bdrv_read(bs, sector_num, buffer, nb_sectors);
        if (read_count > 0) {
            found_dirty = true;
            sector_num += read_count;
            nb_sectors -= read_count;
            buffer += S2EB_SECTOR_SIZE * read_count;
        } else {
            ++sector_num;
            --nb_sectors;
            buffer += S2EB_SECTOR_SIZE;
        }
    }

    return found_dirty;
}

static int coroutine_fn s2e_co_readv(BlockDriverState *bs, int64_t sector_num,
                                     int nb_sectors, QEMUIOVector *qiov)
{
    BDRVS2EState *s = bs->opaque;
    //printf("read %ld %d\n", sector_num, nb_sectors);

    assert(nb_sectors > 0 && "Something wrong happened in the block layer");

    /* Read the whole backing store speculatively */
    int ret = bdrv_co_readv(bs->file, sector_num, nb_sectors, qiov);
    assert(!ret);
    if (ret < 0) {
        return ret;
    }

    unsigned alloc_bytes = qiov->size;
    assert(alloc_bytes >= nb_sectors * BDRV_SECTOR_SIZE);
    uint8_t *temp_buffer = qemu_memalign(BDRV_SECTOR_SIZE, alloc_bytes);
    qemu_iovec_to_buffer(qiov, temp_buffer);

    int found_dirty = s2e_read_dirty(s, temp_buffer, sector_num, nb_sectors);
    found_dirty |= s2e_read_dirty_klee(bs, temp_buffer, sector_num, nb_sectors);

    if (found_dirty) {
        qemu_iovec_from_buffer(qiov, temp_buffer, alloc_bytes);
    }

    qemu_vfree(temp_buffer);

    return 0;
}

static void s2e_write_dirty(BDRVS2EState *s, uint8_t *buffer, uint64_t sector_num, unsigned nb_sectors)
{
    while (nb_sectors > 0) {
        uint64_t offset = (uint64_t) sector_num % S2EB_L2_SECTORS;
        uint64_t transfer_count = S2EB_L2_SECTORS - offset;
        if (transfer_count > nb_sectors) {
            transfer_count = nb_sectors;
        }

        s2e_blk_copy_and_set_dirty(s, buffer, sector_num, transfer_count);

        nb_sectors -= transfer_count;
        sector_num += transfer_count;
        buffer += transfer_count * S2EB_SECTOR_SIZE;
    }
}

static int coroutine_fn s2e_co_writev(BlockDriverState *bs, int64_t sector_num,
                                      int nb_sectors, QEMUIOVector *qiov)
{
    BDRVS2EState *s = bs->opaque;

    assert(nb_sectors > 0 && "Something wrong happened in the block layer");
    /* Don't write beyond the disk boundaries */
    if (sector_num >= s->sector_count || sector_num + nb_sectors > s->sector_count) {
        assert(0);
        return -1;
    }

    //printf("write %ld %d\n", sector_num, nb_sectors);

    /* The entire block goes into the copy-on-write store */
    unsigned alloc_bytes = qiov->size;
    assert(alloc_bytes >= nb_sectors * BDRV_SECTOR_SIZE);

    uint8_t *temp_buffer = qemu_memalign(BDRV_SECTOR_SIZE, alloc_bytes);
    qemu_iovec_to_buffer(qiov, temp_buffer);

    if (__hook_bdrv_write) {
        int ret = __hook_bdrv_write(bs, sector_num, temp_buffer, nb_sectors);
        assert(ret == 0);
        goto end1;
    }

    s2e_write_dirty(s, temp_buffer, sector_num, nb_sectors);

    end1:
    qemu_vfree(temp_buffer);
    return 0;
}

static void s2e_close(BlockDriverState *bs)
{
    BDRVS2EState *s = bs->opaque;

    printf("s2e-block: dirty sectors on close:%ld\n", s->dirty_count);
    for (int64_t i = 0; i < s->l1_entries; ++i) {
        g_free(s->l1[i]);
    }

    if (s->l1) {
        g_free(s->l1);
    }

    if (s->snapshot_vmstate) {
        g_free(s->snapshot_vmstate);
    }

    s->dirty_count = 0;
    s->l1 = NULL;
    s->l1_entries = 0;
}

static int coroutine_fn s2e_co_flush(BlockDriverState *bs)
{
    //Nothing to flush
    return 0;
}

static int64_t s2e_getlength(BlockDriverState *bs)
{
    return bdrv_getlength(bs->file);
}

static int s2e_truncate(BlockDriverState *bs, int64_t offset)
{
    //How do we truncate a read-only disk?
    return -1;
}

static int s2e_probe(const uint8_t *buf, int buf_size, const char *filename)
{
    //Only disk images that have an s2e extension can be opened.
    const char *ext = strstr(filename, ".s2e");
    if (!ext || *(ext + 4)) {
        return 0;
    }

    return 1;
}

static int coroutine_fn s2e_co_discard(BlockDriverState *bs,
                                       int64_t sector_num, int nb_sectors)
{
    return bdrv_co_discard(bs->file, sector_num, nb_sectors);
}

static int s2e_is_inserted(BlockDriverState *bs)
{
    return bdrv_is_inserted(bs->file);
}

static int s2e_media_changed(BlockDriverState *bs)
{
    return bdrv_media_changed(bs->file);
}

static void s2e_eject(BlockDriverState *bs, bool eject_flag)
{
    bdrv_eject(bs->file, eject_flag);
}

static void s2e_lock_medium(BlockDriverState *bs, bool locked)
{
    bdrv_lock_medium(bs->file, locked);
}

static int s2e_ioctl(BlockDriverState *bs, unsigned long int req, void *buf)
{
   return bdrv_ioctl(bs->file, req, buf);
}

static BlockDriverAIOCB *s2e_aio_ioctl(BlockDriverState *bs,
        unsigned long int req, void *buf,
        BlockDriverCompletionFunc *cb, void *opaque)
{
   return bdrv_aio_ioctl(bs->file, req, buf, cb, opaque);
}

static int s2e_create(const char *filename, QEMUOptionParameter *options)
{
    return bdrv_create_file(filename, options);
}

static QEMUOptionParameter s2e_create_options[] = {
    {
        .name = BLOCK_OPT_SIZE,
        .type = OPT_SIZE,
        .help = "S2E virtual disk drive"
    },
    { NULL }
};

static int s2e_has_zero_init(BlockDriverState *bs)
{
    return bdrv_has_zero_init(bs->file);
}


static char *s2e_get_snapshot_file(const char *base_image, const char *snapshot)
{
    char snapshot_file[1024];
    int max_len = sizeof(snapshot_file) - 1;
    strncpy(snapshot_file, base_image, max_len);
    strncat(snapshot_file, ".", max_len);
    strncat(snapshot_file, snapshot, max_len);
    return strdup(snapshot_file);
}

static time_t s2e_get_mtime(const char *path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) == -1) {
        perror(path);
        exit(1);
    }
    return statbuf.st_mtime;
}

static int s2e_snapshot_create(BlockDriverState *bs, QEMUSnapshotInfo *sn_info)
{
    //printf("s2e_snapshot_create\n");
    int ret = 0;
    BDRVS2EState *s = bs->opaque;

    char *snapshot_filename = s2e_get_snapshot_file(bs->filename, sn_info->name);
    if (!snapshot_filename) {
        ret = -1;
        goto fail1;
    }

    FILE *fp = fopen(snapshot_filename, "wb");
    if (!fp) {
        ret = -1;
        goto fail2;
    }

    S2ESnapshotHeader header;
    memcpy(header.name, sn_info->name, sizeof(header.name));
    memcpy(header.id_str, sn_info->id_str, sizeof(header.id_str));
    header.date_sec = sn_info->date_sec;
    header.date_nsec = sn_info->date_nsec;
    header.vm_clock_nsec = sn_info->vm_clock_nsec;

    header.base_image_timestamp = s2e_get_mtime(bs->filename);
    header.sector_map_offset = 1;
    header.sector_map_entries = s->dirty_count;

    printf("s2e-block: dirty at save: %ld\n", s->dirty_count);

    unsigned sector_map_size = header.sector_map_entries * sizeof(uint32_t);
    header.sectors_start = 1 + sector_map_size / S2EB_SECTOR_SIZE;
    if (sector_map_size % S2EB_SECTOR_SIZE) {
        header.sectors_start++;
    }

    header.vmstate_start = header.sectors_start + s->dirty_count;
    header.vmstate_size = s->snapshot_vmstate_size;

    if (fwrite(&header, sizeof(header), 1, fp) != 1) {
        ret = -1;
        goto fail2;
    }

    if (fseek(fp, S2EB_SECTOR_SIZE, SEEK_SET) < 0) {
        ret = -1;
        goto fail2;
    }

    /* Build the list of dirty sectors */
    uint32_t *sector_map = g_malloc0(header.sector_map_entries * sizeof(uint32_t));
    uint8_t *sector_data = g_malloc0(header.sector_map_entries * S2EB_SECTOR_SIZE);

    unsigned written = 0;
    if (header.sector_map_entries > 0) {
        uint32_t *sector_map_ptr = sector_map;
        uint8_t *sector_data_ptr = sector_data;

        for (uint64_t i = 0; i < s->l1_entries; ++i) {
            if (!s->l1[i]) {
                continue;
            }

            uint64_t sector_index = i * S2EB_L2_SECTORS;
            for (unsigned j = 0; j < S2EB_L2_SECTORS; ++j) {
                if (s2e_blk_is_dirty(s, sector_index + j)) {
                    *sector_map_ptr = sector_index + j;
                    memcpy(sector_data_ptr, s->l1[i]->block + (j * S2EB_SECTOR_SIZE), S2EB_SECTOR_SIZE);

                    ++sector_map_ptr;
                    sector_data_ptr += S2EB_SECTOR_SIZE;
                    ++written;
                }
            }
        }

        assert(written == s->dirty_count);

        /* Write them to disk */

        if (fwrite(sector_map, header.sector_map_entries * sizeof(uint32_t), 1, fp) != 1) {
            ret = -1;
            goto fail3;
        }

        if (fseek(fp, header.sectors_start * S2EB_SECTOR_SIZE, SEEK_SET) < 0) {
            ret = -1;
            goto fail3;
        }

        if (fwrite(sector_data, header.sector_map_entries * S2EB_SECTOR_SIZE, 1, fp) != 1) {
            ret = 1;
            goto fail3;
        }
    }

    /* Write the VM state */
    if (fseek(fp, header.vmstate_start * S2EB_SECTOR_SIZE, SEEK_SET) < 0) {
        ret = -1;
        goto fail3;
    }

    if (fwrite(s->snapshot_vmstate, s->snapshot_vmstate_size, 1, fp) != 1) {
        ret = -1;
        goto fail3;
    }


    free(s->snapshot_vmstate);
    s->snapshot_vmstate = NULL;
    s->snapshot_vmstate_size = 0;

    fail3: g_free(sector_data);
           g_free(sector_map);

    fail2: fclose(fp);
    fail1: free(snapshot_filename);
           return ret;
}

static FILE *s2e_read_snapshot_info(BlockDriverState *bs, const char *snapshot_name, S2ESnapshotHeader *header)
{
    char *snapshot_file = s2e_get_snapshot_file(bs->filename, snapshot_name);
    if (!snapshot_file) {
        return NULL;
    }

    FILE *fp = fopen(snapshot_file, "rb");
    if (!fp) {
        return NULL;
    }

    if (fread(header, sizeof(*header), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    return fp;
}

static int s2e_snapshot_goto(BlockDriverState *bs, const char *snapshot_id)
{
    //printf("s2e_snapshot_goto %s\n", snapshot_id);

    BDRVS2EState *s = bs->opaque;
    int ret = 0;

    S2ESnapshotHeader header;
    FILE *fp = s2e_read_snapshot_info(bs, snapshot_id, &header);
    if (!fp) {
        ret = -1;
        goto fail1;
    }

    if (header.base_image_timestamp != s2e_get_mtime(bs->filename)) {
        printf("Modification timestamp of %s changed since the creation of "
               "the snapshot %s.\n"
               "Please recreate a new snapshot.\n", bs->filename, snapshot_id);
        ret = -1;
        goto fail1;
    }

    /* Read the sector data */
    uint8_t *sector_data = g_malloc(header.sector_map_entries * S2EB_SECTOR_SIZE);
    uint32_t *sector_map = g_malloc(header.sector_map_entries * sizeof(uint32_t));

    if (header.sector_map_entries > 0) {
        if (fseek(fp, header.sector_map_offset * S2EB_SECTOR_SIZE, 0) < 0) {
            ret = -1;
            goto fail2;
        }

        if (fread(sector_map, header.sector_map_entries * sizeof(uint32_t), 1, fp) != 1) {
            ret = -1;
            goto fail2;
        }

        if (fseek(fp, header.sectors_start * S2EB_SECTOR_SIZE, 0) < 0) {
            ret = -1;
            goto fail2;
        }

        if (fread(sector_data, header.sector_map_entries * S2EB_SECTOR_SIZE, 1, fp) != 1) {
            ret = -1;
            goto fail2;
        }
    }

    /* Read the VM data */
    uint8_t *vm_state = g_malloc(header.vmstate_size);
    if (fseek(fp, header.vmstate_start * S2EB_SECTOR_SIZE, 0) < 0) {
        ret = -1;
        goto fail3;
    }

    if (fread(vm_state, header.vmstate_size, 1, fp) != 1) {
        ret = -1;
        goto fail3;
    }

    /* Discard whatever state we had before */
    /* Do this as late as possible, as there is no return from there */
    s2e_close(bs);
    s2e_blk_init(bs);

    uint8_t *sector_data_ptr = sector_data;
    for (uint64_t i = 0; i < header.sector_map_entries; ++i) {
        uint64_t sector_num = sector_map[i];
        s2e_blk_copy_and_set_dirty(s, sector_data_ptr, sector_num, 1);
        sector_data_ptr += S2EB_SECTOR_SIZE;
    }

    printf("s2e-block: dirty after restore: %ld\n", s->dirty_count);

    s->snapshot_vmstate = vm_state;
    s->snapshot_vmstate_size = header.vmstate_size;

    goto fail2; /* Don't free vm_state */

    fail3: g_free(vm_state);
    fail2: g_free(sector_map);
           g_free(sector_data);

    fail1: return ret;
}

static int s2e_snapshot_delete(BlockDriverState *bs, const char *snapshot_id)
{
    char *snapshot_file = s2e_get_snapshot_file(bs->filename, snapshot_id);
    return unlink(snapshot_file);
}


static int s2e_snapshot_list(BlockDriverState *bs, QEMUSnapshotInfo **psn_info)
{
    //printf("s2e_snapshot_list\n");

    int ret = 0;

    /* List all the snapshots in the base image's directory */
    char *dirstring = strdup(bs->filename);
    char *filestring = strdup(bs->filename);
    char *directory = dirname(dirstring);
    char *image_name = basename(filestring);
    size_t image_name_len = strlen(image_name);

    DIR *dir = opendir (directory);
    if (!dir) {
        ret = -1;
        goto fail1;
    }

    struct dirent *ent;
    unsigned snapshot_count = 0;
    QEMUSnapshotInfo *sn_tab = NULL, *sn_info;

    while ((ent = readdir (dir)) != NULL) {
        if (strstr(ent->d_name, image_name) == ent->d_name) {
            const char *snapshot_name = ent->d_name + image_name_len;
            if (snapshot_name[0] != '.' || snapshot_name[1] == 0) {
                continue;
            }

            snapshot_name++;
            //printf ("snapshot: %s (%s)\n", ent->d_name, snapshot_name);

            S2ESnapshotHeader header;
            FILE *fp = s2e_read_snapshot_info(bs, snapshot_name, &header);
            if (!fp) {
                continue;
            }
            fclose(fp);

            ++snapshot_count;
            sn_tab = realloc(sn_tab, snapshot_count * sizeof(QEMUSnapshotInfo));
            sn_info = sn_tab + snapshot_count - 1;

            memcpy(sn_info->name, header.name, sizeof(sn_info->name));
            memcpy(sn_info->id_str, header.id_str, sizeof(sn_info->id_str));
            sn_info->vm_state_size = header.vmstate_size;
            sn_info->date_sec = header.date_sec;
            sn_info->date_nsec = header.date_nsec;
            sn_info->vm_clock_nsec = header.vm_clock_nsec;
        }
    }

    *psn_info = sn_tab;
    ret = snapshot_count;

    closedir (dir);

    fail1:
    free(dirstring);
    free(filestring);
    return ret;
}

static int s2e_snapshot_load_tmp(BlockDriverState *bs, const char *snapshot_name)
{
    //printf("s2e_snapshot_load_tmp\n");
    return -1;
}

static int s2e_get_info(BlockDriverState *bs, BlockDriverInfo *bdi)
{
    //printf("s2e_get_info\n");
    //BDRVS2EState *s = bs->opaque;
    bdi->cluster_size = S2EB_SECTOR_SIZE;
    bdi->vm_state_offset = 0;
    return 0;
}

static int s2e_save_vmstate(BlockDriverState *bs, const uint8_t *buf,
                              int64_t pos, int size)
{
    BDRVS2EState *s = bs->opaque;

    /* Accumulate the data into the temporary buffer */
    if (pos + size > s->snapshot_vmstate_size) {
        s->snapshot_vmstate = realloc(s->snapshot_vmstate, pos + size);
        s->snapshot_vmstate_size = pos + size;
    }

    memcpy(s->snapshot_vmstate + pos, buf, size);
    return 0;
}

static int s2e_load_vmstate(BlockDriverState *bs, uint8_t *buf,
                              int64_t pos, int size)
{
    //printf("s2e_load_vmstate\n");

    BDRVS2EState *s = bs->opaque;
    if (pos + size >= s->snapshot_vmstate_size) {
        size = s->snapshot_vmstate_size - pos;
    }

    memcpy(buf, s->snapshot_vmstate + pos, size);

    return size;
}

static BlockDriver bdrv_s2e = {
    .format_name        = "s2e",

    /* It's really 0, but we need to make g_malloc() happy */
    .instance_size      = sizeof(BDRVS2EState),

    .bdrv_open          = s2e_open,
    .bdrv_close         = s2e_close,

    .bdrv_co_readv          = s2e_co_readv,
    .bdrv_co_writev         = s2e_co_writev,
    .bdrv_co_flush_to_disk  = s2e_co_flush,
    .bdrv_co_discard        = s2e_co_discard,

    .bdrv_probe         = s2e_probe,
    .bdrv_getlength     = s2e_getlength,
    .bdrv_truncate      = s2e_truncate,

    .bdrv_is_inserted   = s2e_is_inserted,
    .bdrv_media_changed = s2e_media_changed,
    .bdrv_eject         = s2e_eject,
    .bdrv_lock_medium   = s2e_lock_medium,

    .bdrv_snapshot_create   = s2e_snapshot_create,
    .bdrv_snapshot_goto     = s2e_snapshot_goto,
    .bdrv_snapshot_delete   = s2e_snapshot_delete,
    .bdrv_snapshot_list     = s2e_snapshot_list,
    .bdrv_snapshot_load_tmp = s2e_snapshot_load_tmp,
    .bdrv_get_info          = s2e_get_info,

    .bdrv_save_vmstate    = s2e_save_vmstate,
    .bdrv_load_vmstate    = s2e_load_vmstate,

    .bdrv_ioctl         = s2e_ioctl,
    .bdrv_aio_ioctl     = s2e_aio_ioctl,

    .bdrv_create        = s2e_create,
    .create_options     = s2e_create_options,
    .bdrv_has_zero_init = s2e_has_zero_init,
};

static void bdrv_s2e_init(void)
{
    bdrv_register(&bdrv_s2e);
}

block_init(bdrv_s2e_init);
