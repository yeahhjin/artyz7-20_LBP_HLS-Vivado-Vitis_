#include "lbp_filter.h"

#ifndef MAX_WIDTH
#define MAX_WIDTH 1920
#endif

// RGB to Grayscale ë³??™˜ ?•¨?ˆ˜
static inline ap_uint<8> rgb2gray(ap_uint<24> rgb) {
#pragma HLS INLINE
    ap_uint<8> r = (rgb >> 16) & 0xFF;
    ap_uint<8> g = (rgb >> 8)  & 0xFF;
    ap_uint<8> b =  rgb        & 0xFF;
    ap_uint<16> y = r * 77 + g * 150 + b * 29;
    return (ap_uint<8>)(y >> 8);
}

void lbp_filter(AXI_STREAM &input, AXI_STREAM &output, int height, int width) {
#pragma HLS INTERFACE axis port=input
#pragma HLS INTERFACE axis port=output
#pragma HLS INTERFACE s_axilite port=height bundle=control
#pragma HLS INTERFACE s_axilite port=width bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

    // 3?¼?¸ ?ˆœ?™˜ ë²„í¼
    ap_uint<8> linebuf[3][MAX_WIDTH];
#pragma HLS ARRAY_PARTITION variable=linebuf complete dim=1

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
#pragma HLS PIPELINE II=1
            pixel_t in_pix = input.read();
            ap_uint<8> g = rgb2gray(in_pix.data);

            // ?ˆœ?™˜ ë²„í¼?˜ ?˜„?¬ ?–‰ ?¸?±?Š¤
            ap_uint<2> current_row = y % 3;
            linebuf[current_row][x] = g;

            bool valid = (y >= 2) && (x >= 2);
            ap_uint<8> out_g = 0;

            if (valid) {
                // LBP ?œˆ?„?š° ê³„ì‚°?„ ?œ„?•œ ?˜¬ë°”ë¥¸ ?¼?¸ ë²„í¼ ?¸?±?Š¤
                ap_uint<2> row_m1 = (y - 1) % 3; // ì¤‘ì‹¬ ?”½?? ?–‰
                ap_uint<2> row_m2 = (y - 2) % 3; // ?œ„ìª? ?”½?? ?–‰

                // ì¤‘ì‹¬ ?”½??
                ap_uint<8> c  = linebuf[row_m1][x-1];

                // 8ê°? ì£¼ë? ?”½??
                ap_uint<8> p0 = linebuf[row_m2][x-2]; // ?ƒì¢?
                ap_uint<8> p1 = linebuf[row_m2][x-1]; // ?ƒ
                ap_uint<8> p2 = linebuf[row_m2][x  ]; // ?ƒ?š°
                ap_uint<8> p3 = linebuf[row_m1][x  ]; // ?š°
                ap_uint<8> p4 = linebuf[current_row][x]; // ?•˜?š°
                ap_uint<8> p5 = linebuf[current_row][x-1]; // ?•˜
                ap_uint<8> p6 = linebuf[current_row][x-2]; // ?•˜ì¢?
                ap_uint<8> p7 = linebuf[row_m1][x-2]; // ì¢?

                ap_uint<8> code = 0;
                code |= ((p0 >= c) << 7);
                code |= ((p1 >= c) << 6);
                code |= ((p2 >= c) << 5);
                code |= ((p3 >= c) << 4);
                code |= ((p4 >= c) << 3);
                code |= ((p5 >= c) << 2);
                code |= ((p6 >= c) << 1);
                code |= ((p7 >= c) << 0);

                out_g = code;
            } else {
                out_g = 0; // ê²½ê³„ ?˜?—­?? 0?œ¼ë¡? ì²˜ë¦¬
            }

            pixel_t out_pix;
            out_pix.data = ((ap_uint<24>)out_g << 16) | ((ap_uint<24>)out_g << 8) | out_g;
            out_pix.keep = in_pix.keep;
            out_pix.strb = in_pix.strb;
            out_pix.user = (x == 0 && y == 0);
            out_pix.last = (x == width - 1);
            out_pix.id   = in_pix.id;
            out_pix.dest = in_pix.dest;

            output.write(out_pix);
        }
    }
}
