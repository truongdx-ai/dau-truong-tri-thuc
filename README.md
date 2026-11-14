# Đấu trường tri thức

Dự án môn Lập trình mạng mô phỏng gameshow nhiều người chơi bằng C11 trên Linux.
Server TCP hiện nhận kết nối IPv4 tại `127.0.0.1` và hỗ trợ lệnh `PING`.
Client dòng lệnh đang ở mức khung.

## Build và kiểm thử trên Ubuntu hoặc WSL

```bash
sudo apt update
sudo apt install -y git build-essential gcovr valgrind clang-format libssl-dev
make all
./build/client
make test
make coverage
make valgrind
make format-check
```

Để thử server, chạy `./build/server -p 5500` trong một terminal WSL.
Trong terminal thứ hai, chạy `nc 127.0.0.1 5500`, nhập `PING` và nhấn Enter.
Server trả về `PONG`. Nhấn Ctrl+C trong terminal của server để dừng.

`make coverage` tạo báo cáo tại `coverage/index.html`. Gcovr đo số dòng
được chạy trong mã nguồn dự án, loại trừ các tệp `main.c` dành cho điểm vào
của server và client. Tỉ lệ hiện tại chỉ phản ánh phần mã đã có.
Unity v2.6.1 được lưu cùng giấy phép ở `third_party/unity/`.

## Hook kiểm tra khoảng cách commit

Chạy một lần trong mỗi bản sao repo:

```bash
chmod +x .githooks/pre-commit
git config core.hooksPath .githooks
```

Hook cập nhật thông tin từ `origin`, rồi kiểm tra mốc 30 phút dựa trên các
commit có trong repo cục bộ và các nhánh đã fetch.
