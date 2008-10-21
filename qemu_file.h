#ifndef _QEMU_FILE_H
#define _QEMU_FILE_H

#include <stddef.h>

typedef FILE QEMUFile;

void qemu_put_buffer(QEMUFile *f, const uint8_t *buf, int size);
void qemu_put_byte(QEMUFile *f, int v);
void qemu_put_be16(QEMUFile *f, unsigned int v);
void qemu_put_be32(QEMUFile *f, unsigned int v);
void qemu_put_be64(QEMUFile *f, uint64_t v);

int qemu_get_buffer(QEMUFile *f, uint8_t *buf, int size);
int qemu_get_byte(QEMUFile *f);
unsigned int qemu_get_be16(QEMUFile *f);
unsigned int qemu_get_be32(QEMUFile *f);
uint64_t qemu_get_be64(QEMUFile *f);

static inline void qemu_put_be64s(QEMUFile *f, const uint64_t *pv)
{
    qemu_put_be64(f, *pv);
}

static inline void qemu_put_be32s(QEMUFile *f, const uint32_t *pv)
{
    qemu_put_be32(f, *pv);
}

static inline void qemu_put_be16s(QEMUFile *f, const uint16_t *pv)
{
    qemu_put_be16(f, *pv);
}

static inline void qemu_put_8s(QEMUFile *f, const uint8_t *pv)
{
    qemu_put_byte(f, *pv);
}

static inline void qemu_get_be64s(QEMUFile *f, uint64_t *pv)
{
    *pv = qemu_get_be64(f);
}

static inline void qemu_get_be32s(QEMUFile *f, uint32_t *pv)
{
    *pv = qemu_get_be32(f);
}

static inline void qemu_get_be16s(QEMUFile *f, uint16_t *pv)
{
    *pv = qemu_get_be16(f);
}

static inline void qemu_get_8s(QEMUFile *f, uint8_t *pv)
{
    *pv = qemu_get_byte(f);
}

#if TARGET_LONG_BITS == 64
#define qemu_put_betl qemu_put_be64
#define qemu_get_betl qemu_get_be64
#define qemu_put_betls qemu_put_be64s
#define qemu_get_betls qemu_get_be64s
#else
#define qemu_put_betl qemu_put_be32
#define qemu_get_betl qemu_get_be32
#define qemu_put_betls qemu_put_be32s
#define qemu_get_betls qemu_get_be32s
#endif

int64_t qemu_ftell(QEMUFile *f);
int64_t qemu_fseek(QEMUFile *f, int64_t pos, int whence);

typedef enum {
    Q_FIELD_END,          /* mark end of field list */
    Q_FIELD_BYTE,         /* for 1-byte fields */
    Q_FIELD_INT16,        /* for 2-byte fields */
    Q_FIELD_INT32,        /* for 4-byte fields */
    Q_FIELD_INT64,        /* for 8-byte fields */
    Q_FIELD_BUFFER,       /* for buffer fields */
    Q_FIELD_BUFFER_SIZE,  /* to specify the size of buffers */

#if TARGET_LONG_BITS == 64
    Q_FIELD_TL = Q_FIELD_INT64,           /* target long, either 4 or 8 bytes */
#else
    Q_FIELD_TL = Q_FIELD_INT32
#endif

} QFieldType;

typedef struct {
    QFieldType  type : 16; /* field type */
    uint16_t    offset;    /* offset of field in structure */
} QField;

#define  QFIELD_BEGIN(name)  \
    static const QField    name[] = {

#define  _QFIELD_(t, f)    { t, offsetof(QFIELD_STRUCT,f) }
#define  QFIELD_BYTE(f)   _QFIELD_(Q_FIELD_BYTE, f)
#define  QFIELD_INT16(f)  _QFIELD_(Q_FIELD_INT16, f)
#define  QFIELD_INT32(f)  _QFIELD_(Q_FIELD_INT32, f)
#define  QFIELD_INT64(f)  _QFIELD_(Q_FIELD_INT64, f)
#define  QFIELD_TL(f)     _QFIELD_(Q_FIELD_TL, f)

#define  _QFIELD_SIZEOF(f)   sizeof(((QFIELD_STRUCT*)0)->f)

#define  QFIELD_BUFFER(f)  \
    _QFIELD_(Q_FIELD_BUFFER, f), \
    { Q_FIELD_BUFFER_SIZE, (uint16_t)(_QFIELD_SIZEOF(f) >> 16) }, \
    { Q_FIELD_BUFFER_SIZE, (uint16_t) _QFIELD_SIZEOF(f) }

#define  QFIELD_END           \
        { Q_FIELD_END, 0 },   \
    };

extern void  qemu_put_struct(QEMUFile*  f, const QField*  fields, const void*  s);
extern int   qemu_get_struct(QEMUFile*  f, const QField*  fields, void*  s);

#endif /* _QEMU_FILE_H */
