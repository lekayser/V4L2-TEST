#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/videodev2.h>

int openV4lDevice();
int closeV4lDevice(int fd);
void getV4lDeviceCapabilities(int fd);
void setV4lDeviceVideoFormat(int fd);

int main()
{
    int fd, result;

    fd = openV4lDevice();

    getV4lDeviceCapabilities(fd);
    setV4lDeviceVideoFormat(fd);

    result = closeV4lDevice(fd);

    if (result == 0){
        return EXIT_SUCCESS;
    } else {
        return EXIT_FAILURE;
    }
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

int closeV4lDevice(int fd)
{
    return close(fd);
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
