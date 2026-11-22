# Đấu trường tri thức

Dự án môn Lập trình mạng mô phỏng gameshow nhiều người chơi bằng C11 trên Linux.
Server TCP nhận kết nối IPv4 tại `127.0.0.1`, hỗ trợ đăng ký, đăng nhập,
quản lý phiên và phòng chờ. Client CLI vừa nhận sự kiện từ server vừa đọc
lệnh người dùng bằng `poll()`.

## Build và kiểm thử trên Ubuntu hoặc WSL

```bash
sudo apt update
sudo apt install -y git build-essential gcovr valgrind clang-format libssl-dev python3
make all
make test
make coverage
make valgrind
make format-check
```

Để thử server, chạy `./build/server -p 5500 -d data` trong một terminal WSL.
Trong terminal thứ hai, chạy `nc 127.0.0.1 5500`, nhập `PING` và nhấn Enter.
Server trả về `PONG`. Nhập `REGISTER|an|123` để thử đăng ký lần đầu.
Server trả về `200|OK`, lưu bản ghi vào `data/users.db` và ghi sự kiện vào
`data/server.log`. Gửi lại cùng tên nhận `409|USER_EXISTS`. Nhấn Ctrl+C
trong terminal của server để dừng.

Mật khẩu được lưu dưới dạng PBKDF2-HMAC-SHA256 với salt ngẫu nhiên riêng
cho từng tài khoản. Khi khởi động, server kiểm tra `data/questions.txt`.
Mỗi dòng có dạng `Câu hỏi|A|B|C|D|Đáp án đúng`, ví dụ
`2 + 2 bằng bao nhiêu?|1|2|3|4|D`. Dùng UTF-8 và kết thúc mỗi dòng bằng LF.

`make coverage` tạo báo cáo tại `coverage/index.html`. Gcovr đo số dòng
được chạy trong mã nguồn dự án, loại trừ các tệp `main.c` dành cho điểm vào
của server và client. Tỉ lệ hiện tại chỉ phản ánh phần mã đã có.
Unity v2.6.1 được lưu cùng giấy phép ở `third_party/unity/`.

## Demo Tuần 2 với hai client

Mở ba terminal WSL trong thư mục dự án. Terminal đầu chạy server:

```bash
./build/server -p 5500 -d data
```

Hai terminal còn lại đều chạy `./build/client -h 127.0.0.1 -p 5500`.
Client hiện menu lệnh. Ở client đầu nhập lần lượt:

```text
REGISTER|an|123
LOGIN|an|123
JOIN
```

Ở client thứ hai nhập:

```text
REGISTER|tu|456
LOGIN|tu|456
JOIN
```

Nếu tài khoản đã có, tiếp tục bằng LOGIN. Hai client đều nhận danh sách
PLAYERS. Client đầu là host vì JOIN trước. Nhập START ở client thứ hai
nhận 403 NOT_HOST, nhập START ở host nhận 200 OK và phòng chuyển sang
trạng thái đang chơi. Mốc này chuẩn bị cho vòng loại trong Tuần 3.

LEAVE rời phòng và giữ phiên đăng nhập. LOGOUT xoá phiên và rời phòng.
Mất kết nối hoặc không hoạt động trong 5 phút cũng xoá phiên. Client tự
gửi PING mỗi phút. Quyền host chuyển cho người còn lại có thời điểm JOIN
sớm nhất. Khi mọi người đã rời phòng, có thể JOIN để thử lại.

`make test` chạy cả Unity và kịch bản TCP tự động với hai tài khoản cùng
client thực. Các test sử dụng thư mục dữ liệu tạm.

Bộ test TCP còn kiểm tra giới hạn 100 kết nối, từ chối kết nối thứ 101,
duy trì phản hồi cho các kết nối đang mở và tái sử dụng chỗ trống sau khi
ngắt kết nối. Client được kiểm tra với đầu vào chứa NUL và lệnh cuối chưa
có LF. Log ghi nhận LOGOUT, đóng phiên và hết hạn phiên.

## Hook kiểm tra khoảng cách commit

Chạy một lần trong mỗi bản sao repo:

```bash
chmod +x .githooks/pre-commit
git config core.hooksPath .githooks
```

Hook cập nhật thông tin từ `origin`, rồi kiểm tra mốc 30 phút dựa trên các
commit có trong repo cục bộ và các nhánh đã fetch.
