/*
 * v4l_camera.hpp
 *
 *  Created on: 19 Nov 2020
 *      Author: Lukas Baischer
 */
#ifndef SRC_V4L_CAMERA_H_
#define SRC_V4L_CAMERA_H_

#include <linux/videodev2.h>
#include <stddef.h>
#include <stdint.h>

#define FMT_NUM_PLANES 3
#define WIDTH 1920
#define HEIGHT 1080

#define MEDIA_INIT_OV5640 "sudo media-ctl -d /dev/media0 -V \'\"ov5640 2-003c\":0 [fmt:UYVY/\'1920x1080\'@1/\'15\' field:none]\'"
#define MEDIA_INIT_MIPI_CSI_SUBMODULE "sudo media-ctl -d /dev/media0 -V \'\"43c40000.mipi_csi2_rx_subsystem\":0 [fmt:UYVY/\'1920x1080\' field:none]\'"

typedef struct buffer_addr_struct_s{
	void *start[FMT_NUM_PLANES];
	size_t length[FMT_NUM_PLANES];
} buffer_addr_struct_t;

class Camera {        // The class
	public:          // Access specifier
		uint8_t* camdata = 0;
		Camera(const char* dev); // Constructor declaration
		~Camera();
        int capture(uint8_t **img_out, int *h_out, int *w_out, int *co);
	private:
		int fd;  // Attribute
		int num_planes;
        struct  v4l2_buffer buf;
		buffer_addr_struct_t* buffers;
};

#endif /* SRC_V4L_CAMERA_H_ */