# LBP ip integration project

### 🎯 Arty Z7-20 HDMI LBP Filter Project (Vitis HLS + Vivado + Vitis)

---

### 📌 Project Overview

* 본 프로젝트는 **Vitis HLS + Vivado + Vitis**를 이용하여 **LBP(Local Binary Pattern) 영상 처리 IP**를 FPGA에 구현하는 예제임
* 노트북 화면을 입력받아 **LBP 알고리즘이 적용된 영상**을 실시간으로 모니터에 송출하는 것을 목표로 함
* 환경은 **xc7z020clg400-1** FPGA 디바이스 기반이며, 보드는 **Arty Z7-20**을 사용함

---

### 📝 Workflow

### 1. Vitis HLS

* C/C++ 기반 LBP 알고리즘 IP 설계 (lbp.cpp, lbp.h, tb 포함)
* C Simulation (csim) 수행 완료
* Synthesis 완료
* Co-Simulation (cosim) 생략
* RTL (Verilog) Export 완료

### 2. Vivado

* Digilent에서 제공하는 HDMI Block Design 기반 프로젝트 구성
* HLS로 생성된 LBP IP를 Block Design에 삽입
* Clock 및 AXI 인터페이스 맞춤 연결
* HDMI 파이프라인에 LBP IP 통합

### 3. Bitstream Generation

* Vivado에서 Synthesis → Implementation → Bitstream 생성 완료
* Export Hardware 완료

### 4. Vitis (SDK)

* Digilent HDMI 라이브러리 기반 애플리케이션 프로젝트 생성
* `helloworld.c` 코드 내에 LBP 제어 함수 삽입
* HDMI 영상 출력 및 실시간 LBP 필터 적용 결과 확인

---

### 💻 Development Environment

* Board: Digilent Arty Z7-20
* FPGA Device: xc7z020clg400-
* Toolchain: Vitis HLS, Vivado, Vitis
* Reference Files: Digilent HDMI In/Out 예제 (HW & SW)

---

### 💡 Purpose

* 실시간 영상에 **LBP 필터 적용** 및 HDMI 출력 검증
* HDMI, AXI 인터페이스, VDMA 기반 데이터 플로우 학습
* C → RTL → Vivado → Vitis → HDMI 출력까지의 전체 설계 플로우 경험 축적

---

### 📊 Test Results

* HDMI 영상 출력 정상 동작 확인

* LBP 필터가 적용된 영상이 실시간으로 모니터에 출력됨

* ![lpb_diagram](https://github.com/user-attachments/assets/8f62502f-e9fa-4e81-b291-f046f1623092)


* ![KakaoTalk_20250920_135301696](https://github.com/user-attachments/assets/0c7d5cd8-d716-41a8-9f1e-3a7105bb90c0)


* 

https://github.com/user-attachments/assets/e9cb9fb2-21f4-47cc-8c72-a47abead4440



---

### 🔗 Reference

* Digilent Arty Z7 HDMI In Demo: [https://digilent.com/reference/learn/programmable-logic/tutorials/arty-z7-hdmi-in-demo](https://digilent.com/reference/learn/programmable-logic/tutorials/arty-z7-hdmi-in-demo)
