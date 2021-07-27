/*
 * intuitus.hpp
 *
 *  Created on: 25 Nov 2020
 *      Author: Lukas Baischer
 * 
 * build driver interface using swig:
 *  python setup.py build_ext --inplace
 */
#ifndef SRC_INTUITUS_H_
#define SRC_INTUITUS_H_

#include "intuitus-intf.h"
#include <stdint.h>
//#include <opencv2/core/core.hpp>

#define DRIVER_KEXT_NAME "intuitus.ko"
#define LINUX_KERNEL_MODULE_PATH "~/intuitus.ko"
//#define LINUX_KERNEL_MODULE_PATH "/lib/modules/4.9.0-xilinx-v2017.4/extra/intuitus.ko"
#define LINUX_ADD_KERNEL_MODULE_COMMAND "insmod " LINUX_KERNEL_MODULE_PATH
#define LINUX_DEV_PATH "/dev/intuitus_vdma"

#define NDEBUG

class Intuitus_intf
{
public:
    Intuitus_intf();
    ~Intuitus_intf();

    int self_test();
    int print_network();
    int print_layer(int layer_id);
    int input_layer(uint32_t depth, uint32_t height, uint32_t length);
    int output_layer(int layer_id, int src_layer_id);
    int conv2d(int layer_id, int layer_type, int input_buffer_id, uint32_t in_channel_cnt,
               uint32_t out_height, uint32_t out_width,
               uint32_t out_channel_cnt, uint32_t scattered_lines,
               const uint32_t *tile_tx_arr, int tile_tx_cnt, int tile_tx_dim,
               const uint32_t *tile_rx_arr, int tile_rx_cnt, int tile_rx_dim,
               const int32_t *com_block, int com_block_dim,
               const uint32_t *com_lengths, int com_block_cnt);
    int concat(int concat_layer_id, int layer_1_id, int layer_2_id);
    int split(int split_layer_id, int in_layer_id, int groups);
    int upsample(int upsample_layer_id, int in_buffer_id, uint32_t in_channel_cnt,
                 uint32_t out_height, uint32_t out_width);
    int maxpool2d(int maxpool_layer_id, int in_buffer_id, uint32_t in_channel_cnt,
                  uint32_t out_height, uint32_t out_width, int8_t stride);
    int copy(int copy_layer_id, int in_buffer_id, uint32_t in_channel_cnt,
             uint32_t out_height, uint32_t out_width);

    /*int execute_layer(int layer_id,
                      const uint8_t *fmap_in, int ci, int h_in, int w_in,
                      int8_t **fmap_out, int *co, int *h_out, int *w_out);*/

    int execute(const uint8_t *fmap_in, int ci, int h_in, int w_in,
                int8_t **fmap_out, int *out_size);

    int float8_to_float32(const uint8_t *fmap_in, int ci, int h_in, int w_in,
                          float **fmap_out, int *co, int *h_out, int *w_out);

private:
    struct intuitus_interface *interface_p;

    int intuitus_fd;
    uint32_t input_height;
    uint32_t input_width;
    uint32_t input_depth; // 1 for grayscale, 3 for RGB | Attention: Color channels of cv images have to be splitted

    int8_t *out_buffer;
    uint8_t *output_intf_ptr;
    int output_size = 0;

    int layer_add_command(struct tile_idx src_tile,
                          const int32_t *com_ptr, uint32_t com_length,
                          int channel_idx, int command_id, int layer_id);
    int layer_add_rx_tile(struct tile_idx dst_tile, int channel_idx,
                          int tile_id, uint8_t last_tile, int layer_id);

    int layer_opt_dma(uint32_t tx_scatterlist_size, uint32_t rx_scatterlist_size, int layer_id);
};

#endif /* SRC_INTUITUS_H_ */