#include "lbp_filter.h"
#include <iostream>
#include <cstdlib>   // rand()
#include <ctime>     // time()

using namespace std;

int main() {
    // 테스트용 이미지 크기
    const int HEIGHT = 8;
    const int WIDTH  = 8;

    AXI_STREAM in_stream;
    AXI_STREAM out_stream;

    srand(time(0)); // 랜덤 시드

    // 입력 픽셀 채우기 (R,G,B 다르게: 계단식 + 랜덤 섞기)
    for (int r = 0; r < HEIGHT; r++) {
        for (int c = 0; c < WIDTH; c++) {
            pixel_t pix;

            // 값 패턴 다양화
            ap_uint<8> R = (r * 30 + rand() % 20) & 0xFF; // 행 기준 + 랜덤
            ap_uint<8> G = (c * 25 + rand() % 30) & 0xFF; // 열 기준 + 랜덤
            ap_uint<8> B = ((r+c) * 10 + rand() % 40) & 0xFF;

            // 24bit RGB로 패킹
            pix.data.range(23,16) = R;
            pix.data.range(15, 8) = G;
            pix.data.range(7, 0)  = B;

            pix.keep = -1;
            pix.strb = -1;
            pix.user = 0;
            pix.id   = 0;
            pix.dest = 0;
            pix.last = 0; // last 무시하는 구조라 항상 0으로 둠

            in_stream.write(pix);
        }
    }

    // LBP 필터 실행
    lbp_filter(in_stream, out_stream, HEIGHT, WIDTH);

    // 출력 픽셀 읽기
    cout << "==== LBP Output (8x8) ====" << endl;
    for (int r = 0; r < HEIGHT; r++) {
        for (int c = 0; c < WIDTH; c++) {
            if (!out_stream.empty()) {
                pixel_t out_pix = out_stream.read();
                ap_uint<8> R = out_pix.data.range(23,16);
                ap_uint<8> G = out_pix.data.range(15, 8);
                ap_uint<8> B = out_pix.data.range(7, 0);
                cout << "(" << (int)R << "," << (int)G << "," << (int)B << ") ";
            }
        }
        cout << endl;
    }

    return 0;
}
