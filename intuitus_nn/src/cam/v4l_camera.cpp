#include "v4l_camera.hpp"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <iostream>
#include <fstream>
#include <linux/videodev2.h>
#include <opencv2/opencv.hpp>

static int xioctl(int fd, unsigned int request, void *arg)
{
	int r;
	do
	{
		r = ioctl(fd, request, arg);
	} while (-1 == r && EINTR == errno);
	return r;
}

Camera::Camera(const char *dev)
{
// 0. Initialize Camera sensor and Mipi-CSI submodule
#ifdef DEBUG
	std::cout << MEDIA_INIT_OV5640 << std::endl;
	std::cout << MEDIA_INIT_MIPI_CSI_SUBMODULE << std::endl;
#endif
	if (system(MEDIA_INIT_OV5640) != 0)
	{
		std::cout << "Failed to initialize camera sensor using command:" << std::endl;
		std::cout << MEDIA_INIT_OV5640 << std::endl;
	}
	if (system(MEDIA_INIT_MIPI_CSI_SUBMODULE) != 0)
	{
		std::cout << "Failed to initialize MIPI-CSI module using command:" << std::endl;
		std::cout << MEDIA_INIT_MIPI_CSI_SUBMODULE << std::endl;
	}
	// 1. Open Video Device.
	this->fd = open("/dev/video0", O_RDWR, 0);
	if (this->fd == -1)
	{
		std::cout << "Failed to open video device." << std::endl;
		exit(1);
	}

	// 2. Querying video capabilities.
	struct v4l2_capability caps;
	memset(&caps, 0, sizeof(caps));
	if (-1 == xioctl(this->fd, VIDIOC_QUERYCAP, &caps))
	{
		std::cout << "Failed to query capabilities." << std::endl;
		exit(1);
	}
#ifdef DEBUG
	std::cout << "bus_info	: " << caps.bus_info << std::endl;
	std::cout << "card		: " << caps.card << std::endl;
	std::cout << "driver	: " << caps.driver << std::endl;
	std::cout << "version	: " << caps.version << std::endl;
#endif
	// 3. Format Specification.
	{
		struct v4l2_format fmt;
		memset(&(fmt), 0, sizeof(fmt));

		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		fmt.fmt.pix_mp.width = WIDTH;
		fmt.fmt.pix_mp.height = HEIGHT;
		fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_UYVY;
		fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;

		if (-1 == xioctl(this->fd, VIDIOC_S_FMT, &fmt))
		{
			std::cout << "Failed to set pixel format." << std::endl;
			exit(1);
		}
#ifdef DEBUG
		std::cout << "Video Format: " << std::endl;
		std::cout << fmt.fmt.pix_mp.width << std::endl;
		std::cout << fmt.fmt.pix_mp.height << std::endl;
		std::cout << fmt.fmt.pix_mp.pixelformat << std::endl;
		std::cout << fmt.fmt.pix_mp.field << std::endl;
#endif
	}
	struct v4l2_requestbuffers reqbuf;
	const int MAX_BUF_COUNT = 2; /*we want at least 3 buffers*/

	// 4. Request Buffer
	{
		memset(&(reqbuf), 0, sizeof(reqbuf));
		reqbuf.count = FMT_NUM_PLANES;
		reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		reqbuf.memory = V4L2_MEMORY_MMAP;

		if (-1 == xioctl(this->fd, VIDIOC_REQBUFS, &reqbuf))
		{
			std::cout << "Failed to request buffer." << std::endl;
			exit(1);
		}
		if (reqbuf.count < MAX_BUF_COUNT)
		{
			std::cout << "Not enought buffer memory." << std::endl;
			exit(1);
		}
#ifdef DEBUG
		std::cout << "reqbuf.count : " << reqbuf.count << std::endl;
#endif
		this->buffers = (buffer_addr_struct_t *)calloc(reqbuf.count, sizeof(*(this->buffers)));
		assert(this->buffers != NULL);
	}

	// 5. Query Buffer
	{
		for (unsigned int i = 0; i < reqbuf.count; i++)
		{

			struct v4l2_plane planes[FMT_NUM_PLANES];

			memset(&buf, 0, sizeof(buf));
			memset(planes, 0, sizeof(planes));
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.m.planes = planes;
			buf.length = FMT_NUM_PLANES;
			buf.index = i;
			if (-1 == xioctl(this->fd, VIDIOC_QUERYBUF, &buf))
			{
				std::cout << "Failed to query buffer." << std::endl;
				exit(1);
			}
			this->num_planes = buf.length;
#ifdef DEBUG
			std::cout << "buf.length : " << buf.length << std::endl;
			std::cout << "buf.m.offset : " << buf.m.offset << std::endl;
#endif

			for (int j = 0; j < this->num_planes; j++)
			{
				this->buffers[i].length[j] = buf.m.planes[j].length;
#ifdef DEBUG
				std::cout << "buf.m.planes[j].length : " << buf.m.planes[j].length << std::endl;
#endif
				this->buffers[i].start[j] = mmap(NULL, buf.m.planes[j].length, PROT_READ | PROT_WRITE, MAP_SHARED, this->fd, buf.m.planes[j].m.mem_offset);
				if (MAP_FAILED == this->buffers[i].start[j])
				{
					std::cout << "mmap error" << std::endl;
				}
#ifdef DEBUG
				std::cout << "buffers[i].start[j] : " << this->buffers[i].start[j] << std::endl;
#endif
			}
		}
	}

	this->camdata = (uint8_t *)malloc(WIDTH * HEIGHT * 2 * sizeof(uint8_t));
	if (this->camdata == NULL)
	{
		std::cout << "Fail allocate memory for cam data.";
		exit(1);
	}
}
int Camera::capture(uint8_t **img_out, int *h_out, int *w_out, int *co)
{
	// 6. Start Streaming
	{
		//struct 	v4l2_buffer buf;
		struct v4l2_buffer buf;
		memset(&(buf), 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		if (-1 == xioctl(fd, VIDIOC_STREAMON, &buf.type))
		{
			std::cout << "Fail to start Capture" << std::endl;
			exit(1);
		}
	}
	struct v4l2_buffer buf;
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; //V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;
	struct v4l2_plane planes[FMT_NUM_PLANES];
	buf.m.planes = planes;
	buf.length = FMT_NUM_PLANES;

	// 7. Capture Image
	{
		// Connect buffer to queue for next capture.
		if (-1 == xioctl(this->fd, VIDIOC_QBUF, &buf))
		{
			std::cout << "VIDIOC_QBUF" << std::endl;
		}
#ifdef DEBUG
		std::cout << "Queue buffer done " << std::endl;
#endif
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(this->fd, &fds);
		struct timeval tv = {0};
		tv.tv_sec = 2;
		int r = select(this->fd + 1, &fds, NULL, NULL, &tv);

		if (-1 == r)
		{
			std::cout << "Waiting for Frame" << std::endl;
			return 1;
		}

		if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
		{
			std::cout << "Retrieving Frame" << std::endl;
			return 1;
		}
	}
	// 8. Store Image
#ifdef DEBUG
	std::cout << "Copy received image" << std::endl;
	std::cout << "Plane number: " << num_planes << std::endl;
#endif
	{
		for (int j = 0; j < this->num_planes; j++)
		{
			memcpy(this->camdata, buffers[0].start[j], WIDTH * HEIGHT * 2);
		}
		//memcpy(bayerRaw.data, buffer, WIDTH * HEIGHT);
		//cv::cvtColor(bayerRaw, color, CV_BayerGB2BGR);
	}
	// 10. Turn off streaming.
	if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &buf.type))
	{
		std::cout << "VIDIOC_STREAMOFF" << std::endl;
	}
	*img_out = this->camdata;
	*co = 2;
	*h_out = HEIGHT;
	*w_out = WIDTH;
	return 0;
}

Camera::~Camera()
{
	free(this->buffers);
	free(this->camdata);
	close(this->fd);
}
