#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define BUFFER_NUMBER 20

typedef struct UserspaceBuffers {
        void *start;
        size_t length;
} UserspaceBuffers;

typedef struct v4l2_buffer v4l2_buffer;

int openV4lDevice();
void closeV4lDevice(int fd);
void getV4lDeviceCapabilities(int fd);
void setV4lDeviceVideoFormat(int fd);
void negociateMappingBuffer(int fd, UserspaceBuffers *buffers, struct v4l2_requestbuffers bufferInfo);
void streamOn(int fd, v4l2_buffer bufferInfo);
void streamOff(int fd, v4l2_buffer bufferInfo);
void initQueue(int fd, v4l2_buffer bufferInfo);

int main()
{
    int fd;
    UserspaceBuffers *buffers;
    struct v4l2_requestbuffers reqbuf;
    unsigned int i;

    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = BUFFER_NUMBER;

    buffers = calloc(BUFFER_NUMBER, sizeof(*buffers));
    assert(buffers != NULL);

    v4l2_buffer info;
    memset(&info, 0, sizeof(info));
    info.type = reqbuf.type;
    info.type = reqbuf.memory;

    fd = openV4lDevice();

    getV4lDeviceCapabilities(fd);
    setV4lDeviceVideoFormat(fd);
    negociateMappingBuffer(fd, buffers, reqbuf);

    streamOn(fd, info);
    initQueue(fd, info);

    // take 20 frames
    /*for (i=0; i<BUFFER_NUMBER; i++)
    {

        if (ioctl(fd, VIDIOC_QBUF, ))

    }*/

    streamOff(fd, info);


    assert(buffers != NULL);

    // free some shit
    for (i = 0; i < 20; i++)
        munmap(buffers[i].start, buffers[i].length);

    free(buffers);

    closeV4lDevice(fd);

    return EXIT_SUCCESS;
}

int openV4lDevice()
{
    int fd;
    if((fd = open("/dev/video0", O_RDWR)) < 0){
        perror("open");
        exit(1);
    }

    return fd;
}

void closeV4lDevice(int fd)
{
    if(close(fd)!=0){
        fprintf(stderr,"error closing the device");
        exit(1);
    }
}

void getV4lDeviceCapabilities(int fd)
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

void setV4lDeviceVideoFormat(int fd)
{
    struct v4l2_format format;
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

void negociateMappingBuffer(int fd, UserspaceBuffers *buffers, struct v4l2_requestbuffers info)
{

    unsigned int i;

    if ( -1 == ioctl(fd, VIDIOC_REQBUFS, &info)){
        if (errno == EINVAL )
            printf("mmap streaming is not supported\\n");
        else
            perror("VIDIOC_REQBUFS");

        exit(EXIT_FAILURE);
    }

    printf("we got %d buffers thanks man",info.count);

    for (i=0; i<info.count; i++)
    {

        struct v4l2_buffer buffer;

        memset(&buffer, 0, sizeof(buffer));
        buffer.type = info.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if (-1 == ioctl (fd, VIDIOC_QUERYBUF, &buffer)){
            perror(VIDIOC_QUERYBUF);
            exit(EXIT_FAILURE);
        }

        buffers[i].length = buffer.length;

        buffers[i].start = mmap(NULL, buffer.length,
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED,
                                fd, buffer.m.offset);


        if (MAP_FAILED == buffers[i].start){
            perror("mmap");
            exit(EXIT_FAILURE);
        }
    }

    //for (i = 0; i < reqbuf.count; i++)
      //  munmap(buffers[i].start, buffers[i].length);

    //free(buffers);

}

void streamOn(int fd, v4l2_buffer bufferInfo)
{
    // clean shit
    int type = bufferInfo.type;
    if ( ioctl(fd, VIDIOC_STREAMON, &type) < 0){
        perror("VIDIOC_STREAMON");
        exit(EXIT_FAILURE);
    }
}

void streamOff(int fd, v4l2_buffer bufferInfo)
{
    // clean shit

    int type = bufferInfo.type;
    if ( ioctl(fd, VIDIOC_STREAMOFF, &type) < 0){
        perror("VIDIOC_STREAMOFF");
        exit(EXIT_FAILURE);
    }
}

void initQueue(int fd, v4l2_buffer bufferInfo)
{

    v4l2_buffer info;
    memset(&info, 0, sizeof(info));
    info.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    info.type = V4L2_MEMORY_MMAP;
    info.index = 1;

    if(ioctl(fd, VIDIOC_QBUF, &info) < 0 ){
        perror("VIDIOC_QBUF INIT");
        exit(EXIT_FAILURE);
    }
}
