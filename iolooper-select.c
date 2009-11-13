#include "iolooper.h"
#include "qemu-common.h"

/* An implementation of iolooper.h based on Unix select() */
#ifdef _WIN32
#  include <winsock2.h>
#else
#  include <sys/types.h>
#  include <sys/select.h>
#endif

struct IoLooper {
    fd_set   reads[1];
    fd_set   writes[1];
    fd_set   reads_result[1];
    fd_set   writes_result[1];
    int      max_fd;
    int      max_fd_valid;
};

IoLooper*
iolooper_new(void)
{
    IoLooper*  iol = qemu_malloc(sizeof(*iol));
    iolooper_reset(iol);
    return iol;
}

void
iolooper_free( IoLooper*  iol )
{
    qemu_free(iol);
}

void
iolooper_reset( IoLooper*  iol )
{
    FD_ZERO(iol->reads);
    FD_ZERO(iol->writes);
    iol->max_fd = -1;
    iol->max_fd_valid = 1;
}

static void
iolooper_add_fd( IoLooper*  iol, int fd )
{
    if (iol->max_fd_valid && fd > iol->max_fd) {
        iol->max_fd = fd;
    }
}

static void
iolooper_del_fd( IoLooper*  iol, int fd )
{
    if (iol->max_fd_valid && fd == iol->max_fd)
        iol->max_fd_valid = 0;
}

static int
iolooper_fd_count( IoLooper*  iol )
{
    int  max_fd = iol->max_fd;
    int  fd;

    if (iol->max_fd_valid)
        return max_fd + 1;

    /* recompute max fd */
    for (fd = 0; fd < FD_SETSIZE; fd++) {
        if (!FD_ISSET(fd, iol->reads) && !FD_ISSET(fd, iol->writes))
            continue;

        max_fd = fd;
    }
    iol->max_fd       = max_fd;
    iol->max_fd_valid = 1;

    return max_fd + 1;
}

void
iolooper_add_read( IoLooper*  iol, int  fd )
{
    if (fd >= 0) {
        iolooper_add_fd(iol, fd);
        FD_SET(fd, iol->reads);
    }
}

void
iolooper_add_write( IoLooper*  iol, int  fd )
{
    if (fd >= 0) {
        iolooper_add_fd(iol, fd);
        FD_SET(fd, iol->writes);
    }
}

void
iolooper_del_read( IoLooper*  iol, int  fd )
{
    if (fd >= 0) {
        iolooper_del_fd(iol, fd);
        FD_CLR(fd, iol->reads);
    }
}

void
iolooper_del_write( IoLooper*  iol, int  fd )
{
    if (fd >= 0) {
        iolooper_del_fd(iol, fd);
        FD_CLR(fd, iol->reads);
    }
}

int
iolooper_poll( IoLooper*  iol )
{
    int     count = iolooper_fd_count(iol);
    int     ret;
    fd_set  errs;

    if (count == 0)
        return 0;

    FD_ZERO(&errs);

    do {
        struct timeval  tv;

        tv.tv_sec = tv.tv_usec = 0;

        iol->reads_result[0]  = iol->reads[0];
        iol->writes_result[0] = iol->writes[0];

        ret = select( count, iol->reads_result, iol->writes_result, &errs, &tv);
    } while (ret < 0 && errno == EINTR);

    return ret;
}

int
iolooper_wait( IoLooper*  iol, int64_t  duration )
{
    int     count = iolooper_fd_count(iol);
    int     ret;
    fd_set  errs;

    if (count == 0)
        return 0;

    FD_ZERO(&errs);

    do {
        iol->reads_result[0]  = iol->reads[0];
        iol->writes_result[0] = iol->writes[0];

        ret = select( count, iol->reads_result, iol->writes_result, &errs, NULL);
    } while (ret < 0 && errno == EINTR);

    return ret;
}


int
iolooper_is_read( IoLooper*  iol, int  fd )
{
    return FD_ISSET(fd, iol->reads_result);
}

int
iolooper_is_write( IoLooper*  iol, int  fd )
{
    return FD_ISSET(fd, iol->writes_result);
}
