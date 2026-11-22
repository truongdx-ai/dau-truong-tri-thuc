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

## Demo phòng chờ và vòng mở màn với hai client

Mở ba terminal WSL trong thư mục dự án. Terminal đầu chạy server:

```bash
./build/server -p 5500 -d data
```

Hai terminal còn lại đều chạy `./build/client -h 127.0.0.1 -p 5500`.
Client hiện menu lệnh và thông báo dễ đọc. Các mã 200, 403, PLAYERS,
QUESTION và ROLE bên dưới mô tả giao thức; thêm `-r` khi mở client nếu
muốn xem nguyên dòng giao thức để đối chiếu. Ở client đầu nhập lần lượt:

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
trạng thái đang chơi. Sau PLAYERS, cả hai client nhận cùng QUESTION loại
QUALIFY, ví dụ với server vừa khởi động:

```text
QUESTION|1|QUALIFY|30|2 + 2 bằng bao nhiêu?|1|2|3|4
```

Trong 30 giây, nhập D ở client an trước, chờ thông báo server đã nhận đáp
án, rồi nhập A ở client tu. Client an được thông báo là người chơi chính;
client tu được thông báo người chơi chính là an. Client tự ghép qid vào
ANSWER. Vẫn có thể nhập lệnh đầy đủ ANSWER|1|D để kiểm tra giao thức;
khi đó phải thay 1 bằng qid thực tế của câu hỏi đang chơi.
Mỗi người chỉ được trả lời một lần; đáp án đúng nhanh nhất còn trong phòng
được chọn khi thu đủ đáp án hoặc hết giờ. Server không gửi đáp án đúng
trong QUESTION. Nếu không ai đúng, cả hai nhận GAME_OVER|NO_WINNER và
host có thể START lại. Có thể thử không trả lời để kiểm tra hạn 30 giây.

Thời hạn mở màn, xử lý đồng thời điểm, không trả lời, rời phòng và quay
vòng bộ câu hỏi là quy ước triển khai, được ghi rõ trong docs/protocol.md.
Phần này dừng ở việc chọn người chính; vòng TURN, chia điểm và SKIP sẽ
được triển khai tiếp.

Client mặc định trình bày câu hỏi như sau:

```text
[Cau hoi #1 - Vong mo man]
2 + 2 bằng bao nhiêu?
A. 1
B. 2
C. 3
D. 4
Thoi gian: 30 giay. Nhap A/B/C/D roi nhan Enter.
```

Đáp án viết hoa hoặc thường đều được chấp nhận, kể cả có khoảng trắng
hai bên. Client nhắc khi chưa có câu hỏi hoặc đã gửi đáp án; nhận ROLE,
GAME_OVER hoặc gửi LEAVE/LOGOUT sẽ đóng phần nhập đáp án của câu hiện tại.
Thời gian hiển thị là thời hạn do server công bố, không phải bộ đếm giảm
dần trên client; server quyết định đáp án còn đúng hạn hay không.
Các lệnh giao thức đầy đủ vẫn dùng được. Chế độ xem nguyên giao thức:

```bash
./build/client -r -h 127.0.0.1 -p 5500
```

LEAVE rời phòng và giữ phiên đăng nhập. LOGOUT xoá phiên và rời phòng.
Mất kết nối hoặc không hoạt động trong 5 phút cũng xoá phiên. Client tự
gửi PING mỗi phút. Quyền host chuyển cho người còn lại có thời điểm JOIN
sớm nhất. Khi mọi người đã rời phòng, có thể JOIN để thử lại.

`make test` chạy cả Unity và kịch bản TCP tự động với hai tài khoản cùng
client thực. Các test sử dụng thư mục dữ liệu tạm.

Bộ test TCP còn kiểm tra giới hạn 100 kết nối, từ chối kết nối thứ 101,
duy trì phản hồi cho các kết nối đang mở và tái sử dụng chỗ trống sau khi
ngắt kết nối. Client được kiểm tra với đầu vào chứa NUL và lệnh cuối chưa
có LF. Log ghi nhận LOGOUT, đóng phiên, hết hạn phiên, nhận đáp án và kết
quả chọn người chính.

Các test vòng mở màn kiểm tra thứ tự đáp án, đồng thời điểm, trả lời lặp,
sai qid, rời phòng, tái sử dụng khe và phát ROLE. Test TCP còn chờ đủ một
vòng 30 giây không có đáp án để kiểm tra bộ đếm giờ tự chạy; vì vậy make
test mất thêm khoảng 30 giây. Có thể chạy riêng bằng:

```bash
make all
python3 tests/integration/test_qualification.py build
```

Test riêng phần hiển thị và nhập đáp án của client (không phải chờ 30 giây):

```bash
make all
python3 tests/integration/test_client_ui.py build
```

## Hook kiểm tra khoảng cách commit

Chạy một lần trong mỗi bản sao repo:

```bash
chmod +x .githooks/pre-commit
git config core.hooksPath .githooks
```

Hook cập nhật thông tin từ `origin`, rồi kiểm tra mốc 30 phút dựa trên các
commit có trong repo cục bộ và các nhánh đã fetch.
