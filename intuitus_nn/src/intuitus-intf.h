/* This header file is shared between the interface application and the Intuitus device driver. It defines the
 * shared interface to allow to configurate and execute a CNN from a python application.
 *
 * Note: That this file must not be changed.
 */

#ifndef SRC_INTUITUS_KERNEL_INTF_H_
#define SRC_INTUITUS_KERNEL_INTF_H_

#ifndef __KERNEL__
#include <stdint.h>
#else
#include <linux/types.h>
#endif

#define INTF_BUFFER_SIZE (32 * 220 * 220 * 2)

enum intuitus_layer_types
{
	Input,
	Output,
	Conv1x1,
	InvBottleneck3x3,
	InvBottleneck5x5,
	Conv3x3,
	Conv5x5,
	Residual,
	Concat,
	Split,
	Upsample,
	Maxpooling2d,
	Copy,
	Test_loop
};

enum intuitus_ioctl_cmd
{
	INPUT_LAYER,
	OUTPUT_LAYER,	
	LAYER_CREATE,
	LAYER_ADD_RX_TILE,
	LAYER_ADD_TX_COM,
	LAYER_CONCAT,
	BUFFER_SPLIT,
	LAYER_OPTIMIZE_DMA,
	LAYER_EXECUTE,
	NETWORK_EXECUTE,
	NETWORK_STATUS,
	EXECUTION_STATUS,
	REQUEST_INTF,
	PRINT_NETWORK,
	PRINT_LAYER,
	SELF_TEST
};
enum proxy_status
{
	PROXY_NO_ERROR = 0,
	PROXY_BUSY = 1,
	PROXY_TIMEOUT = 2,
	PROXY_ERROR = 3
};

#define ERROR_MEMORY_ALLOC_FAIL (-1)
#define ERROR_DIMENSION_MISMATCH (-2)
#define ERROR_NULL_POINTER_PARAMETER (-3)
#define ERROR_MAX_MEMORY_LIMIT (-4)
#define ERROR_CREATE_DEVICE (-5)
#define ERROR_DMA (-6)
#define ERROR_OTHER (-7)

/**
 * intuitus_interface -> Kernel to user space interface  
 * @buffer: data buffer. Used for large transfers. Static size.
 * @length: buffer length. 
 * @status: shows interface status -> check for interface availablitity before read or write from it 
 */
struct intuitus_interface
{
	uint8_t buffer[INTF_BUFFER_SIZE];
	uint32_t length;
	uint32_t height;
	uint32_t depth;
	enum proxy_status status;
};

/**
 * tile_idx - Image tile indexing
 * @x0: start x position (value is included) example: x0 = 3, x1 = 7: included indizes: 3,4,5,6
 * @y0: start y position (value is included)
 * @x1: end x position (value is not included) 
 * @y2: end y position (value is not included)
 */
struct tile_idx
{
	uint32_t x0;
	uint32_t y0;
	uint32_t x1;
	uint32_t y1;
};

/**
 * intuitus_layer_args -> command for hardware control 
 * @layer_type: layer tpye, defines which transfers are exectued 
 * @src_buffer_id: id of source buffer. Only used for Residual layer. Otherwise dst buffer of previous layer is used
 * @tile_cnt: tile number 
 * @ci_cnt: input channel number 
 * @co_cnt: output channel number
 */
struct intuitus_layer_args
{
	enum intuitus_layer_types layer_type;
	int src_buffer_id;
	int layer_id;
	uint32_t rx_tile_cnt;
	uint32_t tx_tile_cnt;
	uint32_t ci_cnt;
	uint32_t co_cnt;
	uint32_t dst_length;
	uint32_t dst_height;
	int8_t scattered_lines;
};

/** intuitus_command_args -> Arguments for LAYER_ADD_COMMAND
 * @layer_id: layer id to which the command is added 
 * @src_tile: source tile size 
 * @channel_idx: Input channel index 
 * @command_id: Command identification number
 */
struct intuitus_command_args
{
	int layer_id;
	struct tile_idx src_tile;
	int channel_idx;
	int command_id;
};

/** intuitus_rx_tile_args -> Arguments for LAYER_ADD_COMMAND
 * @layer_id: layer id to which the rx tile is added 
 * @dst_tile: destination tile size (differs from source tile size because filters can overlap)
 * @channel_idx: Output channel index of first channel in this load 
 * @num_channels: Number of output channels in this load 
 */
struct intuitus_rx_tile_args
{
	int layer_id;
	struct tile_idx dst_tile;
	int channel_idx;
	uint8_t last_tile;
	int tile_id;
};

/** intuitus_concat_args -> Arguments for LAYER_CONCAT
 * @concat_layer_id: id of the concat layer 
 * @layer1_id: id of first layer to be concatenated 
 * @layer2_id: id of second layer to be concatenated  
 */
struct intuitus_concat_args
{
	int concat_layer_id;
	int layer1_id;
	int layer2_id;
};

/** intuitus_split_args -> Arguments for LAYER_SPLIT
 * @split_layer_id: first id of newly created split layer
 * @in_layer_id: id of buffer which is to be splitted  
 * @groups: number of created groups --> number of parts into which the buffer is divided 
 */
struct intuitus_split_args
{
	int split_layer_id;
	int in_layer_id;
	int groups;
};

/** intuitus_opt_args -> Arguments for LAYER_OPTIMIZE_DMA
 * @layer_id: layer id to which the rx tile is added 
 * @dst_tile: destination tile size (differs from source tile size because filters can overlap)
 * @channel_idx: Output channel index of first channel in this load 
 * @num_channels: Number of output channels in this load 
 */
struct intuitus_opt_args
{
	int layer_id;
	uint32_t tx_scatterlist_size;
	uint32_t rx_scatterlist_size;
};

#endif /* SRC_INTUITUS_KERNEL_INTF_H_ */
