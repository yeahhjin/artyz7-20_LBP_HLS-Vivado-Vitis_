#ifndef LBP_FILTER_H
#define LBP_FILTER_H

#include <ap_int.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>

// AXI4-Stream 24b RGB + side-channel ����
typedef ap_axiu<24, 1, 1, 1> pixel_t;
typedef hls::stream<pixel_t> AXI_STREAM;

// Top function: �̹��� ũ�⸦ ���ڷ� �޵��� ����
void lbp_filter(AXI_STREAM &input, AXI_STREAM &output, int height, int width);

#endif
