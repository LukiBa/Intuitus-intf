/*
 * framebuffer.hpp
 *
 *  Created on: 19 Nov 2020
 *      Author: Lukas Baischer
 */

#ifndef SRC_FRAMEBUFFER_H_
#define SRC_FRAMEBUFFER_H_
#include <linux/fb.h>
#include <stdint.h>

class Framebuffer {
    public:
        Framebuffer(const char* dev);
        ~Framebuffer();
        int show(const uint8_t* img_ptr, int height, int length, int depth, int offs);
        void close_tty();
        void get_screensize(int32_t **screen_size, int *dim);
    private:
        struct fb_var_screeninfo vinfo;
        struct fb_fix_screeninfo finfo;    
        int screensize;
        int fbfd;
        int ttyfd;
        int tty_open; 
        uint8_t *fbp;
        int *screensize_arr;

};

#endif /* SRC_FRAMEBUFFER_H_ */