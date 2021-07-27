/** Intuitus python to C++ driver interface 
 * Based on Neural Network C Extension SWIG Wrapper File by Benjamin Kulnik
 * 
 * Autor: Lukas Baischer
 * Date: 16.02.2021
 * 
 * Good reference: https://www.numpy.org.cn/en/reference/swig/interface-file.html#other-situations
 */ 

%module intuitus_nn

%pythonbegin %{
from __future__ import absolute_import
%}

%{
// This has to be declared so the %init block gets called
#define SWIG_FILE_WITH_INIT

#include <iostream>
#include <chrono>

#include "intuitus-intf.h"
#include "intuitus.hpp"
#include "driver_exceptions.hpp"
#include "framebuffer.hpp"
#include "v4l_camera.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

%}

/* Include to use datatypes like int32_t */
%include "stdint.i"

/* Include  typemaps */
%include "typemaps.i"

%include "exception.i"

/* Include support for numpy */
%include "numpy.i"

%init %{
// Import Numpy Arrays
import_array();
// import_umath();
%}




// ---------------------------- Numpy Typemaps  --------------------------------
//
// This remaps the C convention for arrays (parameter pointer + dims) to a single
// parameter in python (a numpy array)
// 
// Examples: https://docs.scipy.org/doc/numpy-1.13.0/reference/swig.interface-file.html

// Support all default numeric types
// float
// double
// int8_t
// int16_t
// int32_t
// int64_t
// uint8_t
// uint16_t
// uint32_t
// uint64_t

// Map the inputs to numpy
%apply (uint32_t *IN_ARRAY1, int DIM1) {
    (const uint32_t *com_lengths, int com_block_cnt)
};
%apply (int32_t *IN_ARRAY1, int DIM1) {
    (const int32_t *com_block, int com_block_dim)
};

%apply (uint32_t *IN_ARRAY2, int DIM1, int DIM2) {
    (const uint32_t *tile_tx_arr, int tile_tx_cnt, int tile_tx_dim),
    (const uint32_t *tile_rx_arr, int tile_rx_cnt, int tile_rx_dim)  
};

%apply (uint8_t *IN_ARRAY3, int DIM1, int DIM2, int DIM3) {
    (const uint8_t *fmap_in, int ci, int h_in, int w_in), 
    (const uint8_t *img_ptr, int height, int length, int depth)
};


// Typemaps for Output Arrays of Conv2D and MaxPool2D
%apply (uint8_t** ARGOUTVIEW_ARRAY3, int *DIM1, int *DIM2, int *DIM3) { 
  (uint8_t **fmap_out, int *co, int *h_out, int *w_out),
  (uint8_t **img_out, int *h_out, int *w_out, int *co)
};
%apply (int8_t** ARGOUTVIEW_ARRAY3, int *DIM1, int *DIM2, int *DIM3) { 
  (int8_t **fmap_out, int *co, int *h_out, int *w_out)
};
%apply (float** ARGOUTVIEW_ARRAY3, int *DIM1, int *DIM2, int *DIM3) { 
  (float **fmap_out, int *co, int *h_out, int *w_out)
};
%apply (uint8_t** ARGOUTVIEW_ARRAY1, int *DIM1) { 
  (uint8_t **fmap_out, int *out_size)
}
%apply (int8_t** ARGOUTVIEW_ARRAY1, int *DIM1) { 
  (int8_t **fmap_out, int *out_size)
}
%apply (int32_t** ARGOUTVIEW_ARRAY1, int *DIM1) { 
  (int32_t **screen_size, int *dim)
}

// ------------------------------------ Wrapping ----------------------------------------
// Wrap everything declared in this header
%include "src/intuitus.hpp"
%include "src/fb/framebuffer.hpp"
%include "src/cam/v4l_camera.hpp"






