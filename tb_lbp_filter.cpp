#include "lbp_filter.h"
#include <iostream>
#include <cstdlib>   // rand()
#include <ctime>     // time()

using namespace std;

int main() {
    // �׽�Ʈ�� �̹��� ũ��
    const int HEIGHT = 8;
    const int WIDTH  = 8;

    AXI_STREAM in_stream;
    AXI_STREAM out_stream;

    srand(time(0)); // ���� �õ�

    // �Է� �ȼ� ä��� (R,G,B �ٸ���: ��ܽ� + ���� ����)
    for (int r = 0; r < HEIGHT; r++) {
        for (int c = 0; c < WIDTH; c++) {
            pixel_t pix;

            // �� ���� �پ�ȭ
            ap_uint<8> R = (r * 30 + rand() % 20) & 0xFF; // �� ���� + ����
            ap_uint<8> G = (c * 25 + rand() % 30) & 0xFF; // �� ���� + ����
            ap_uint<8> B = ((r+c) * 10 + rand() % 40) & 0xFF;

            // 24bit RGB�� ��ŷ
            pix.data.range(23,16) = R;
            pix.data.range(15, 8) = G;
            pix.data.range(7, 0)  = B;

            pix.keep = -1;
            pix.strb = -1;
            pix.user = 0;
            pix.id   = 0;
            pix.dest = 0;
            pix.last = 0; // last �����ϴ� ������ �׻� 0���� ��

            in_stream.write(pix);
        }
    }

    // LBP ���� ����
    lbp_filter(in_stream, out_stream, HEIGHT, WIDTH);

    // ��� �ȼ� �б�
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
