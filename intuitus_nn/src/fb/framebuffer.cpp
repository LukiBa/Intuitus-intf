#include "framebuffer.hpp"

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/kd.h>

#define FB_LINE(i) (fb_pos + vinfo.xres * i * 3)
#define IMG_LINE(i) (img_pos + length * i * 3)

Framebuffer::Framebuffer(const char *dev)
{
    this->fbfd = open(dev, O_RDWR);
    if (this->fbfd == -1)
    {
        printf("Error: cannot open framebuffer device");
        exit(1);
    }

    /* Set the tty to graphics mode */
    /* Get fixed screen information */
    if (ioctl(this->fbfd, FBIOGET_FSCREENINFO, &(this->finfo)) == -1)
    {
        printf("Error reading fixed information");
        exit(2);
    }

    /* Get variable screen information */
    if (ioctl(this->fbfd, FBIOGET_VSCREENINFO, &(this->vinfo)) == -1)
    {
        printf("Error reading variable information");
        exit(3);
    }
#ifdef DEBUG
    printf("%s\n", this->finfo.id);
    printf("%dx%d, %dbpp\n", this->vinfo.xres, this->vinfo.yres, this->vinfo.bits_per_pixel);
#endif
    this->screensize_arr = (int *) malloc(2*sizeof(int));
    this->screensize_arr[0] = vinfo.yres; 
    this->screensize_arr[1] = vinfo.xres;

    this->screensize = this->vinfo.xres * this->vinfo.yres * this->vinfo.bits_per_pixel / 8;
#ifdef DEBUG
    printf("Screen: %d bytes\n", this->screensize);
#endif
}

Framebuffer::~Framebuffer()
{
    if (this->tty_open)
    {
        this->close_tty();
    }
    free(this->screensize_arr);
    close(this->fbfd);
}

int Framebuffer::show(const uint8_t *img_ptr, int height, int length, int depth, int offs)
{

    int i;
    const uint8_t *img_pos = img_ptr;
    uint8_t *fb_pos;

    size_t img_size = depth * height * length;

    if (3 != depth)
    {
        printf("Error: Framebuffer uses BGR fromat. Got matrix with depth %d. Aborting", depth);
        return 5;
    }

    if (((int)img_size + offs) > this->screensize)
    {
        printf("Error: Image size exceeds frame buffer size");
        return 5;
    }

    this->fbp = (uint8_t *)mmap(0, this->screensize, PROT_READ | PROT_WRITE, MAP_SHARED, this->fbfd, 0);

    if ((int)this->fbp == -1)
    {
        printf("Error: failed to map framebuffer device to memory\n");
        return (4);
    }

    /* Attempt to open the tty and set it to graphics mode */
    if (0 == this->tty_open)
    {
        this->ttyfd = open("/dev/tty1", O_RDWR);
        this->tty_open = 1; 
    }
    
    if (this->ttyfd == -1)
    {
        printf("Error: could not open the tty\n");
    }
    else
    {
        ioctl(this->ttyfd, KDSETMODE, KD_GRAPHICS);
    }
    fb_pos = this->fbp + offs;
    for (i = 0; i < height; i++)
    {
        memcpy(FB_LINE(i), IMG_LINE(i), length * 3);
    }

    /*for (i=0; i<vinfo.xres; i++) {
		  for (j=0; j<vinfo.yres; j++) {
			  for(k=0; k<3; k++){
				  fbp[m] = test_img[l];
				  m++;
			  }
			  l++;
		  }
	  }*/
    return 0;
}

void Framebuffer::close_tty()
{
    /* Unmap the memory and release all the files */    
    munmap(this->fbp, this->screensize);

    if (this->ttyfd != -1)
    {
        ioctl(this->ttyfd, KDSETMODE, KD_TEXT);
        close(this->ttyfd);
    }
    return;
}

void Framebuffer::get_screensize(int **screen_size, int *dim)
{
    *screen_size = this->screensize_arr;
    *dim = 2;
    return;
}