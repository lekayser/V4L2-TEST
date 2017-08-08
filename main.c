#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

typedef struct UserspaceBuffers {
        void *start;
        size_t length;
} UserspaceBuffers;

typedef struct v4l2_buffer v4l2_buffer;

void open_V4lDevice();
void close_V4lDevice();
void init_V4lDevice();
void checkV4lDeviceStreamingCapabilities();
void setV4lDeviceVideoFormat();
void initV4lDeviceStreamingBuffer();
void uninit_device();
static void start_capturing();
static void stop_capturing();
static void main_loop();
static int read_frame();

static int fd = -1;
static unsigned int n_buffers;
UserspaceBuffers *buffers;
static char *dev_name;
static int frame_count = 60;

int main()
{
    dev_name = "/dev/video0";

    open_V4lDevice();
    init_V4lDevice();
    start_capturing();
    stop_capturing();
    uninit_device();
    close_V4lDevice();

    return EXIT_SUCCESS;
}

void open_V4lDevice()
{
    if((fd = open(dev_name, O_RDWR)) < 0){
        perror("open");
        exit(1);
    }
}

void close_V4lDevice()
{
    if(close(fd)!=0){
        fprintf(stderr,"error closing the device");
        exit(1);
    }
}

void init_V4lDevice()
{
    checkV4lDeviceStreamingCapabilities();
    setV4lDeviceVideoFormat();
    initV4lDeviceStreamingBuffer();
}

void checkV4lDeviceStreamingCapabilities()
{
    struct v4l2_capability cap;

    if(ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0){
        perror("VIDIOC_QUERYCAP");
        exit(1);
    }

    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE )){
        fprintf(stderr, "The device does not have video capture cap");
        exit(1);
    }

    if(!(cap.capabilities & V4L2_CAP_STREAMING )){
        fprintf(stderr, "The device does not have video streaming cap");
        exit(1);
    }
}

void setV4lDeviceVideoFormat()
{
    struct v4l2_format format;
    CLEAR(format);

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    format.fmt.pix.width = 640;
    format.fmt.pix.height = 360;

    if(ioctl(fd, VIDIOC_S_FMT, &format) < 0)
    {
        perror("VIDIOC_S_FMT");
        exit(1);
    }
}


void initV4lDeviceStreamingBuffer()
{
    struct v4l2_requestbuffers req;
    CLEAR(req);

    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req)<0)
    {
        if (EINVAL == errno) {
            fprintf(stderr, "%s does not support mem mapping", dev_name);
            exit(EXIT_FAILURE);
        } else {
            perror("VIDIOC_REQBUFS");
            exit(EXIT_FAILURE);
        }
    }

    if (req.count < 2){
        fprintf(stderr, "insufficient buffer memory on %s \\n", dev_name);
        exit(EXIT_FAILURE);
    }

    buffers = calloc(req.count, sizeof(*buffers));

    if (!buffers){
        fprintf(stderr,"out of memory error\\n");
        exit(EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < req.count ; ++n_buffers) {
        struct v4l2_buffer buf;
        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (ioctl(fd,VIDIOC_QUERYBUF, &buf)<0){
            perror("VIDIOC_QUERYBUF");
            exit(EXIT_FAILURE);
        }

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start = mmap(NULL, buf.length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start){
            fprintf(stderr,"can not map memmory from device %s", dev_name);
            exit(EXIT_FAILURE);
        }
    }
}

void uninit_device()
{
    unsigned int i;

    for (i = 0; i < n_buffers; i++)
    if ( -1 == munmap(buffers[i].start, buffers[i].length)){
        perror("munmap");
        exit(EXIT_FAILURE);
    }

    free(buffers);
}

static void start_capturing()
{
    unsigned int i;
    enum v4l2_buf_type type;

    for( i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd, VIDIOC_QBUF, &buf)<0) {
            perror("VIDIOC_QBUF");
            exit(EXIT_FAILURE);
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if ( ioctl(fd, VIDIOC_STREAMON, &type)<0) {
        perror("VIDIOC_STREAMON");
        exit(EXIT_FAILURE);
    }
}

static void stop_capturing()
{
    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd, VIDIOC_STREAMOFF, &type)<0) {
        perror("VIDIOC_STREAMOFF");
        exit(EXIT_FAILURE);
    }
}

static void main_loop()
{

    unsigned int count;

    count = frame_count;

    while (count -- > 0) {
        for (;;) {
            fd_set fds;
            struct timeval tv;
            int r;

            FD_ZERO(&fds);
            FD_SET(fd, &fds);

            // Timeout
            tv.tv_sec = 2;
            tv.tv_usec = 0;

            r = select(fd + 1, &fds, NULL, NULL, &tv);

            if (-1 == r) {
                if(EINTR == errno)
                    continue;
            }

            if (0 == r) {
                fprintf(stderr, "select timeout\\n");
                exit(EXIT_FAILURE);
            }

            if (read_frame())
                break;
        }
    }
}

static int read_frame()
{
    struct v4l2_buffer buf;

    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if ( ioctl(fd, VIDIOC_DQBUF, &buf)<0) {
        switch(errno) {
        case EAGAIN:
            return 0;

        case EIO:

        default:
            perror("VIDIOC_DQBUF");
        }
    }

    assert(buf.index < n_buffers);

    process_image(buffers[buf.index].start, buf.bytesused);

    if (ioctl(fd, VIDIOC_QBUF, &buf)<0)
    {
        perror("VIDIOC_QBUF");
        exit(EXIT_FAILURE);
    }

    return 1;

}

