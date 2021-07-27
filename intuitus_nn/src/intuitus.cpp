#include "intuitus.hpp"
#include "intuitus-intf.h"
#include "driver_exceptions.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <asm-generic/ioctl.h>
#include <math.h>

#include <iostream>
#include <chrono>

#define TILE_TX_ARRAY(i, j) *(tile_tx_arr + j + (4 * i))
#define TILE_RX_ARRAY(i, j) *(tile_rx_arr + j + (6 * i))
#define COMMAND_LENGTHS(i, j) *(command_lengths + j + (in_channel_cnt * i))
#define FMAP(x, c, h, w) *(x + w + +(in_channel_cnt * i))
#define LOAD_IN_CHANNELS(x) *(load_arr + (x * 2))
#define LOAD_OUT_CHANNELS(x) *(load_arr + (x * 2) + 1)

#define VERBOSE

/** input_layer --> Creates an input layer
 * @depth: channel number of input layer (3 for RGB, 1 for grayscale)
 * @height: height of input image 
 * @width: width of input image 
 */
int Intuitus_intf::input_layer(uint32_t depth, uint32_t height, uint32_t length)
{
	int err;

	struct intuitus_layer_args kernel_args = {
		.layer_type = Input,
		.src_buffer_id = 0,
		.layer_id = 0,
		.rx_tile_cnt = 1,
		.tx_tile_cnt = 1,
		.ci_cnt = depth,
		.co_cnt = depth,
		.dst_length = length,
		.dst_height = height};

	this->input_width = length;
	this->input_height = height;
	this->input_depth = depth;

	err = ioctl(this->intuitus_fd, _IOW(0, INPUT_LAYER, sizeof(struct intuitus_layer_args)), &kernel_args);
	CHECK(0 == err, err, "Failed to create input layer.\n")
	this->output_intf_ptr = (uint8_t *)this->interface_p->buffer + (length * height * depth); // Set output interface start pointer at the end of the input interface
	debug("Input intf_p: %p | Output intf_p: %p.\n", this->interface_p->buffer, this->output_intf_ptr);
	return 0;
}

/** output_layer --> Creates an input layer
 * @depth: channel number of input layer (3 for RGB, 1 for grayscale)
 * @height: height of input image 
 * @width: width of input image 
 */
int Intuitus_intf::output_layer(int layer_id, int src_layer_id)
{
	int err;

	struct intuitus_layer_args kernel_args = {
		.layer_type = Output,
		.src_buffer_id = src_layer_id,
		.layer_id = layer_id,
		.rx_tile_cnt = 1,
		.tx_tile_cnt = 1,
		.ci_cnt = 0,
		.co_cnt = 0,
		.dst_length = 0,
		.dst_height = 0};

	err = ioctl(this->intuitus_fd, _IOW(0, OUTPUT_LAYER, sizeof(struct intuitus_layer_args)), &kernel_args);
	CHECK(0 == err, err, "Failed to create output layer.\n")

	this->output_size += this->interface_p->length * this->interface_p->height * this->interface_p->depth;
	this->out_buffer = (int8_t *)realloc(this->out_buffer, this->output_size);
	debug("New Output size: %d\n",this->output_size);
	return 0;
}

/** conv2d --> Creates a conv2d layer in kernel driver.
 * @layer_id: id of layer. Has to fit to position of layer in the network. 
 * @layer_type: type of layer (Conv1x1, Conv3x3, Conv5x5).
 * @input_buffer_id: id of input buffer (indicates which layer output is used as input for this layer. Output buffer id is equal to layer id.).
 * @in_channel_cnt: number of input channels.
 * @out_height: height of ouput feature map.
 * @out_width: width of ouput feature map.
 * @out_channel_cnt: number of ouput channels.
 * @src_tile: src tile array. Start and endpoints of tiles. 
 * @src_tile_cnt: number of src tiles. Length of src tile array. 
 * @dst_tile: dst tile array. Start and endpoints of tiles.
 * @dst_tile_cnt: number of dst tiles. Length of dst tile array. 
 * @com_ptr: command block array. 
 * @com_length: length of command blocks.
 * 
 */
int Intuitus_intf::conv2d(int layer_id, int layer_type, int input_buffer_id, uint32_t in_channel_cnt,
						  uint32_t out_height, uint32_t out_width,
						  uint32_t out_channel_cnt, uint32_t scattered_lines,
						  const uint32_t *tile_tx_arr, int tile_tx_cnt, int tile_tx_dim,
						  const uint32_t *tile_rx_arr, int tile_rx_cnt, int tile_rx_dim,
						  const int32_t *command_block, int com_block_dim,
						  const uint32_t *command_lengths, int com_block_cnt)
{
	enum intuitus_layer_types layer_type_enum = (enum intuitus_layer_types)layer_type;
	int i, j, k, err;
	int check_length = 0;
	uint8_t last_tile = 0;
	const int32_t *command_block_pos = 0;
	struct tile_idx tile;
	uint32_t tx_scatter_list_size = 0;
	uint32_t rx_scatter_list_size = 0;

	/*std::cout << "tile_tx: [" << tile_tx_cnt << "," << tile_tx_dim << "]" << std::endl;
	std::cout << "tile_rx: [" << tile_rx_cnt << "," << tile_rx_dim << "]" << std::endl;
	std::cout << "com_block_dim: " << com_block_dim << std::endl;
	std::cout << "com_block_cnt: " << com_block_cnt << std::endl;*/

	CHECK(6 == tile_rx_dim, ERROR_DIMENSION_MISMATCH, "Dimension missmatch in rx tile array. Got %d, expected 8", tile_rx_dim)
	CHECK(4 == tile_tx_dim, ERROR_DIMENSION_MISMATCH, "Dimension missmatch in tx tile array. Got %d, expected 8", tile_tx_dim)

	CHECK(((int)(tile_tx_cnt * in_channel_cnt)) == com_block_cnt, ERROR_DIMENSION_MISMATCH, "Dimension missmatch of tile array and command block number. Got %d, expected to be %d times %d.", com_block_cnt, tile_tx_cnt, in_channel_cnt)

	check_length = 0;
	for (i = 0; i < com_block_cnt; i++)
	{
		check_length += command_lengths[i];
	}
	CHECK(com_block_dim == check_length, ERROR_DIMENSION_MISMATCH, "Command block lenght does not match sum of command lengths. Got %d, expected to be %d.", com_block_dim, check_length)

	struct intuitus_layer_args kernel_args = {
		.layer_type = layer_type_enum,
		.src_buffer_id = input_buffer_id,
		.layer_id = layer_id,
		.rx_tile_cnt = (uint32_t)tile_rx_cnt,
		.tx_tile_cnt = (uint32_t)tile_tx_cnt,
		.ci_cnt = in_channel_cnt,
		.co_cnt = out_channel_cnt,
		.dst_length = out_width,
		.dst_height = out_height,
		.scattered_lines = (int8_t)scattered_lines};

	err = ioctl(this->intuitus_fd, _IOW(0, LAYER_CREATE, sizeof(struct intuitus_layer_args)), &kernel_args);
	CHECK(0 == err, err, "Failed to create layer %d.\n", layer_id)

	tx_scatter_list_size = 0;
	last_tile = 0;
	command_block_pos = command_block;
	for (k = 0; k < (tile_tx_cnt); k++)
	{
		tile.y0 = TILE_TX_ARRAY(k, 0);
		tile.y1 = TILE_TX_ARRAY(k, 1);
		tile.x0 = TILE_TX_ARRAY(k, 2);
		tile.x1 = TILE_TX_ARRAY(k, 3);
		/*std::cout << "Tile tx: " << k << ": [" << tile.y0 << "," << tile.y1 << ","
				  << tile.x0 << "," << tile.x1 << "]"
				  << " channels: [" << TILE_RX_ARRAY(k, 5)
				  << "," << TILE_RX_ARRAY(k, 4) << "]" << std::endl;*/
		for (j = 0; j < ((int)(in_channel_cnt)); j++)
		{
			err = layer_add_command(tile, command_block_pos, COMMAND_LENGTHS(k, j),
									j, k * in_channel_cnt + j, layer_id);
			CHECK(0 == err, err, "Add commands to layer %d failed at tile %d channel %d", layer_id, k, j)

			command_block_pos += COMMAND_LENGTHS(k, j);
			tx_scatter_list_size += (tile.y1 - tile.y0) + 1;
		}
	}
	rx_scatter_list_size = 0;
	i = 0;
	for (k = 0; k < (tile_rx_cnt); k++)
	{
		tile.y0 = TILE_RX_ARRAY(k, 0);
		tile.y1 = TILE_RX_ARRAY(k, 1);
		tile.x0 = TILE_RX_ARRAY(k, 2);
		tile.x1 = TILE_RX_ARRAY(k, 3);
		/*std::cout << "Tile rx: " << k << ": [" << tile.y0 << "," << tile.y1 << ","
				  << tile.x0 << "," << tile.x1 << "]"
				  << " channels: [" << TILE_RX_ARRAY(k, 5)
				  << "," << TILE_RX_ARRAY(k, 4) << "]" << std::endl;*/

		if (k == (tile_rx_cnt - 1) || (out_height == tile.y1 && out_width == tile.x1))
		{
			last_tile = 1;
		}

		for (j = TILE_RX_ARRAY(k, 4); j < ((int)(TILE_RX_ARRAY(k, 5))); j++)
		{
			err = layer_add_rx_tile(tile, j, i, last_tile, layer_id);
			CHECK(0 == err, err, "Add rx tile to layer %d failed at tile %d channel %d", layer_id, k, j)
			rx_scatter_list_size += (tile.y1 - tile.y0);
			i++;
			//std::cout 	<< "In channel: " << j << " id: " << i << std::endl;
		}
	}
	//err = layer_opt_dma(tx_scatter_list_size, rx_scatter_list_size, layer_id);
	return err;
}

/** layer_add_command ->	adds command to layer in kernel driver
 * 							a dma_coherent buffer is allocated in 
 * 							kernel space
 * @src_tile: start and end points of src tile
 * @dst_tile: start and end points of dst tile
 * @com_ptr: pointer to command block array --> 32 bit vectors 
 * @com_length: lenght of command block array in 32 bit vectors 
 * @channel_idx: channel id. Indicates position tile in associated tensor
 * @command_id: command identification number
 * @layer_id: layer id number
 */
int Intuitus_intf::layer_add_command(struct tile_idx src_tile, const int32_t *com_ptr, uint32_t com_length,
									 int channel_idx, int command_id, int layer_id)
{
	int err;
	struct intuitus_command_args kernel_args = {
		.layer_id = layer_id,
		.src_tile = src_tile,
		.channel_idx = channel_idx,
		.command_id = command_id};
	memcpy((void *)this->interface_p->buffer, (const void *)com_ptr, sizeof(int32_t) * com_length);
	this->interface_p->length = sizeof(int32_t) * com_length;
	err = ioctl(this->intuitus_fd, _IOW(0, LAYER_ADD_TX_COM, sizeof(struct intuitus_command_args)), &kernel_args);
	CHECK(0 == err, err, "Failed to add command for input channel %d.\n", channel_idx)
	return 0;
}

/** layer_add_rx_tile ->	adds rx tile to layer in kernel driver
 * 							creates a dma rx descriptor 
 * @dst_tile: start and end points of dst tile
 * @channel_idx: channel id. Indicates position tile in associated tensor
 * @num_channels: number of ouput channels in this load
 * @layer_id: layer id number
 */
int Intuitus_intf::layer_add_rx_tile(struct tile_idx dst_tile, int channel_idx,
									 int tile_id, uint8_t last_tile, int layer_id)
{
	int err;
	struct intuitus_rx_tile_args kernel_args = {
		.layer_id = layer_id,
		.dst_tile = dst_tile,
		.channel_idx = channel_idx,
		.last_tile = last_tile,
		.tile_id = tile_id};
	err = ioctl(this->intuitus_fd, _IOW(0, LAYER_ADD_RX_TILE, sizeof(struct intuitus_rx_tile_args)), &kernel_args);
	CHECK(0 == err, err, "Failed to add rx tile for ouput channel %d.\n", channel_idx)
	return 0;
}

/** layer_add_rx_tile ->	adds rx tile to layer in kernel driver
 * 							creates a dma rx descriptor 
 * @dst_tile: start and end points of dst tile
 * @channel_idx: channel id. Indicates position tile in associated tensor
 * @num_channels: number of ouput channels in this load
 * @layer_id: layer id number
 */
int Intuitus_intf::layer_opt_dma(uint32_t tx_scatterlist_size, uint32_t rx_scatterlist_size, int layer_id)
{
	int err = 0;
	struct intuitus_opt_args kernel_args = {
		.layer_id = layer_id,
		.tx_scatterlist_size = tx_scatterlist_size,
		.rx_scatterlist_size = rx_scatterlist_size};

	std::cout << "Tx dma scatterlist size: " << tx_scatterlist_size << std::endl;
	std::cout << "Rx dma scatterlist size: " << rx_scatterlist_size << std::endl;

	err = ioctl(this->intuitus_fd, _IOW(0, LAYER_OPTIMIZE_DMA, sizeof(struct intuitus_opt_args)), &kernel_args);
	CHECK(0 == err, err, "Failed to optimize dma transfers for layer %d.\n", layer_id)
	return err;
}

/** concat ->	concatenates the ouput buffer of two layers 
 * @concat_layer_id: id of newly created concat layer 
 * @layer_1_id: id of first layer to be concatenated 
 * @layer_2_id: id of second layer to be concatenated 
 */
int Intuitus_intf::concat(int concat_layer_id, int layer_1_id, int layer_2_id)
{
	int err = 0;
	struct intuitus_concat_args kernel_args = {
		.concat_layer_id = concat_layer_id,
		.layer1_id = layer_1_id,
		.layer2_id = layer_2_id};

	err = ioctl(this->intuitus_fd, _IOW(0, LAYER_CONCAT, sizeof(struct intuitus_concat_args)), &kernel_args);
	CHECK(0 == err, err, "Failed to concat layer %d and %d.\n", layer_1_id, layer_2_id)
	return err;
}

/** split ->	split buffer into groups parts. groups layer are created with ongoing layer numbers. 
 * 				Example: split_layer_id = 5, buffer_id = 4, goups = 4. 4 split layers with id 5,6,7,8 are created. 
 * 				Each ouput buffer points to its corresponding part of the input buffer.  
 * @split_layer_id: first id of newly created split layers. 
 * @buffer_id: id of buffer to be splitted  
 * @groups: number of created groups --> number of parts into which the buffer is divided
 */
int Intuitus_intf::split(int split_layer_id, int in_layer_id, int groups)
{
	int err = 0;
	struct intuitus_split_args kernel_args = {
		.split_layer_id = split_layer_id,
		.in_layer_id = in_layer_id,
		.groups = groups};

	err = ioctl(this->intuitus_fd, _IOW(0, BUFFER_SPLIT, sizeof(struct intuitus_split_args)), &kernel_args);
	CHECK(0 == err, err, "Failed to split buffer of layer %d.\n", in_layer_id)
	return err;
}

/** upsample ->	upsamples layer using scale (2,2)  
 * @upsample_layer_id: id of newly created upsample layer. 
 * @in_buffer_id: id of buffer to be upsampled 
 * @in_channel_cnt: number of input channels
 * @out_height: output height 
 * @out_width: output width
 */
int Intuitus_intf::upsample(int upsample_layer_id, int in_buffer_id, uint32_t in_channel_cnt,
							uint32_t out_height, uint32_t out_width)
{
	int err = 0;

	struct intuitus_layer_args kernel_args = {
		.layer_type = Upsample,
		.src_buffer_id = in_buffer_id,
		.layer_id = upsample_layer_id,
		.rx_tile_cnt = 0,
		.tx_tile_cnt = 0,
		.ci_cnt = in_channel_cnt,
		.co_cnt = in_channel_cnt,
		.dst_length = out_width,
		.dst_height = out_height,
		.scattered_lines = 0};

	err = ioctl(this->intuitus_fd, _IOW(0, LAYER_CREATE, sizeof(struct intuitus_layer_args)), &kernel_args);
	CHECK(0 == err, err, "Failed to create layer %d.\n", upsample_layer_id)
	return err;
}

/** maxpool2d ->	maxpool2d layer using scale (2,2)  
 * @maxpool_layer_id: id of newly created maxpool layer. 
 * @in_buffer_id: id of buffer for maxpooling
 * @in_channel_cnt: number of input channels
 * @out_height: output height 
 * @out_width: output width
 * @stride: stride 
 */
int Intuitus_intf::maxpool2d(int maxpool_layer_id, int in_buffer_id, uint32_t in_channel_cnt,
							 uint32_t out_height, uint32_t out_width, int8_t stride)
{
	int err = 0;

	struct intuitus_layer_args kernel_args = {
		.layer_type = Maxpooling2d,
		.src_buffer_id = in_buffer_id,
		.layer_id = maxpool_layer_id,
		.rx_tile_cnt = 0,
		.tx_tile_cnt = 0,
		.ci_cnt = in_channel_cnt,
		.co_cnt = in_channel_cnt,
		.dst_length = out_width,
		.dst_height = out_height,
		.scattered_lines = stride};

	err = ioctl(this->intuitus_fd, _IOW(0, LAYER_CREATE, sizeof(struct intuitus_layer_args)), &kernel_args);
	CHECK(0 == err, err, "Failed to create layer %d.\n", maxpool_layer_id)
	return err;
}

/** copy ->	copy layer output to new buffer  
 * @copy_layer_id: id of newly created copy layer. 
 * @in_buffer_id: id of buffer to be copied 
 * @in_channel_cnt: number of input channels
 * @out_height: output height 
 * @out_width: output width
 */
int Intuitus_intf::copy(int copy_layer_id, int in_buffer_id, uint32_t in_channel_cnt,
						uint32_t out_height, uint32_t out_width)
{
	int err = 0;

	struct intuitus_layer_args kernel_args = {
		.layer_type = Copy,
		.src_buffer_id = in_buffer_id,
		.layer_id = copy_layer_id,
		.rx_tile_cnt = 0,
		.tx_tile_cnt = 0,
		.ci_cnt = in_channel_cnt,
		.co_cnt = in_channel_cnt,
		.dst_length = out_width,
		.dst_height = out_height,
		.scattered_lines = 0};

	err = ioctl(this->intuitus_fd, _IOW(0, LAYER_CREATE, sizeof(struct intuitus_layer_args)), &kernel_args);
	CHECK(0 == err, err, "Failed to create layer %d.\n", copy_layer_id)
	return err;
}

/** execute_layer -> executes a layer --> UNTESTET (most likely not working)
 * @layer_id: layer id of layer which is to be executed 
 * @fmap_in: input tensor (contignous allocation required)
 * @ci: channel number of input tensor
 * @h_in: height of input tensor 
 * @w_in: width of input tensor 
 * @fmap_out: pointer to output tensor 
 * @co: output channel number 
 * @h_out: output tensor height 
 * @w_out: output tensor width 
 */
/*int Intuitus_intf::execute_layer(int layer_id,
								 const uint8_t *fmap_in, int ci, int h_in, int w_in,
								 int8_t **fmap_out, int *co, int *h_out, int *w_out)
{

	int err;
	size_t size_in = ci * h_in * w_in;
	size_t size_out;

	std::cout << "start execution of layer " << layer_id << std::endl;

	CHECK(size_in < INTF_BUFFER_SIZE, ERROR_DIMENSION_MISMATCH, "Feature map size exeeds buffer size.")
	memcpy((void *)this->interface_p->buffer, (const void *)fmap_in, size_in);
	this->interface_p->status = PROXY_NO_ERROR;
	this->interface_p->depth = ci;
	this->interface_p->height = h_in;
	this->interface_p->length = w_in;

	auto start = std::chrono::high_resolution_clock::now();
	err = ioctl(this->intuitus_fd, _IOW(0, LAYER_EXECUTE, sizeof(int)), &layer_id);
	CHECK(0 == err, err, "Failed to execute layer %d.\n", layer_id)
	auto stop = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
	std::cout << "Layer execution took: "
			  << duration.count() << "µs" << std::endl;
	std::cout << "execution of layer " << layer_id << " done. Result dim [" << this->interface_p->depth << "," << this->interface_p->height << "," << this->interface_p->length << "]" << std::endl;

	size_out = this->interface_p->length * this->interface_p->height * this->interface_p->depth;
	this->out_buffer = (int8_t *)realloc(this->out_buffer, size_out);
	memcpy((void *)this->out_buffer, (void *)(this->interface_p->buffer + INTF_BUFFER_SIZE / 2), size_out);
	*co = this->interface_p->depth;
	*h_out = this->interface_p->height;
	*w_out = this->interface_p->length;
	*fmap_out = this->out_buffer;
	return 0;
}*/

/** execute -> executes the network 
 * @fmap_in: input tensor (contignous allocation required)
 * @ci: channel number of input tensor
 * @h_in: height of input tensor 
 * @w_in: width of input tensor 
 * @fmap_out: pointer to output tensor 
 * @co: output channel number 
 * @h_out: output tensor height 
 * @w_out: output tensor width 
 */
int Intuitus_intf::execute(const uint8_t *fmap_in, int ci, int h_in, int w_in,
						   int8_t **fmap_out, int *out_size)
{
	int err;
	size_t size_in = ci * h_in * w_in;

	int dummy;
	CHECK(size_in < INTF_BUFFER_SIZE, ERROR_DIMENSION_MISMATCH, "Feature map size exeeds buffer size.")
	memcpy((void *)this->interface_p->buffer, (const void *)fmap_in, size_in);
	this->interface_p->status = PROXY_NO_ERROR;
	this->interface_p->depth = ci;
	this->interface_p->height = h_in;
	this->interface_p->length = w_in;

	auto start = std::chrono::high_resolution_clock::now();
	err = ioctl(this->intuitus_fd, _IO(0, NETWORK_EXECUTE), &dummy);
	CHECK(0 == err, err, "Failed to execute network.")
	/*auto stop = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
	cout << "Execution completed successfully after: "
		 << duration.count() << "µs" << endl;*/

	memcpy((void *)this->out_buffer, (void *)(this->output_intf_ptr), this->output_size);
	*out_size = this->output_size;
	*fmap_out = this->out_buffer;
	return 0;
}

/** float8_to_float32 -> converts float8 network output to float32 
 * @fmap_in: input tensor 
 * @ci: channel number of input tensor
 * @h_in: height of input tensor 
 * @w_in: width of input tensor 
 * @fmap_out: pointer to output tensor 
 * @co: output channel number 
 * @h_out: output tensor height 
 * @w_out: output tensor width 
 */
int Intuitus_intf::float8_to_float32(const uint8_t *fmap_in, int ci, int h_in, int w_in,
									 float **fmap_out, int *co, int *h_out, int *w_out)
{
	float out[ci * h_in * w_in];
	size_t size = ci * h_in * w_in;
	size_t i;

	for (i = 0; i < size; i++)
	{
		out[i] = ((float)(fmap_in[i] & 0xf)) * pow(2.0, (-1) * (fmap_in[i] >> 4) - 4);
	}
	*fmap_out = out;
	*co = ci;
	*h_out = h_in;
	*w_out = w_in;

	return 0;
}

int Intuitus_intf::self_test()
{
	int err;
	unsigned long dummy;
	auto start = std::chrono::high_resolution_clock::now();

	err = ioctl(this->intuitus_fd, _IO(0, SELF_TEST), &dummy);
	CHECK(0 == err, err, "Self test failed. See kernel log for details.\n")
	/*auto stop = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
	cout << "Self test completed successfully after: "
		 << duration.count() << "µs" << endl;*/

	return 0;
}

int Intuitus_intf::print_network()
{
	int err;
	unsigned long dummy;

	err = ioctl(this->intuitus_fd, _IO(0, PRINT_NETWORK), &dummy);
	CHECK(0 == err, err, "Failed to print network. See kernel log for details.\n")
	std::cout << "Network structure prined to kernel log. " << std::endl;
	return 0;
}

int Intuitus_intf::print_layer(int layer_id)
{
	int err;
	err = ioctl(this->intuitus_fd, _IO(0, PRINT_LAYER), &layer_id);
	CHECK(0 == err, err, "Failed to print network. See kernel log for details.\n")
	std::cout << "Layer structure prined to kernel log. " << std::endl;
	return 0;
}

Intuitus_intf::Intuitus_intf()
{
	debug("Start init ");
	this->out_buffer = NULL;
	// try to get root privileges
	CHECK_AND_EXIT(geteuid() == 0, ERROR_CREATE_DEVICE, "Driver requires root privileges");
	// Load dma-proxy driver into kernel
	if (access(LINUX_DEV_PATH, F_OK) != 0) // Check availability of device
	{
		debug("Execute: %s", LINUX_ADD_KERNEL_MODULE_COMMAND);
		CHECK_AND_EXIT(system(LINUX_ADD_KERNEL_MODULE_COMMAND) == 0, ERROR_CREATE_DEVICE, "Error loading intuitus-vdma kernel module.\nCheck if kernel module is available in given path and AXI VDMA IP core is correctly connected in device tree.");
	}
	// open intuitus vdma driver
	this->intuitus_fd = open(LINUX_DEV_PATH, O_RDWR);
	CHECK_AND_EXIT(this->intuitus_fd >= 1, ERROR_CREATE_DEVICE, "Unable to open intuitus_vdma device file.\nCheck if intuitus.ko is inserted and intuitus_vdma are available in /dev");

	// Map memory with proxy interfaces for tx and rx device
	this->interface_p = (struct intuitus_interface *)mmap(NULL, sizeof(struct intuitus_interface),
														  PROT_READ | PROT_WRITE, MAP_SHARED, this->intuitus_fd, 0);
}

Intuitus_intf::~Intuitus_intf()
{
	// Unmap memory
	if (this->interface_p != NULL)
	{
		CHECK_AND_EXIT(munmap(this->interface_p, sizeof(struct intuitus_interface)) == 0, ERROR_CREATE_DEVICE, "Error unmap kernel interface");
	}
	if (this->out_buffer != NULL)
	{
		free(this->out_buffer);
	}
	close(this->intuitus_fd);
	debug("Exit intuitus interface.\n");
}
