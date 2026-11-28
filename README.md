# Đấu trường tri thức

Dự án môn Lập trình mạng mô phỏng gameshow nhiều người chơi bằng C11 trên Linux.
Server TCP nhận kết nối IPv4 tại `127.0.0.1`, hỗ trợ đăng ký, đăng nhập,
phòng chờ, vòng loại và lượt chơi có tính điểm. Client CLI vừa nhận sự kiện từ server vừa đọc
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
nhận 403 NOT_HOST. Nhập START ở host nhận 200 OK và câu hỏi vòng loại.

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

## Chơi một ván Tuần 3

Mở server và ba client, đăng nhập bằng ba tài khoản khác nhau rồi JOIN.
Người JOIN đầu tiên nhập START. Client hiển thị nội dung và bốn lựa chọn.
Nhập `ANSWER|qid|A` hoặc B, C, D theo qid vừa nhận. Mỗi người được gửi
một đáp án cho mỗi câu. MAIN được nhập `SKIP|qid` tối đa hai lần trong ván.

Vòng loại cho mọi người 30 giây. Người đúng nhanh nhất thành MAIN. Nếu
cùng mili giây, thứ tự server nhận dòng hoàn chỉnh quyết định. Không ai
đúng thì hỏi câu mới, tối đa ba lần. Lượt chính cho SUB 30 giây và MAIN
60 giây. Server đợi cửa sổ 30 giây đóng rồi mới tính điểm. MAIN chưa trả
lời được đợi đến 60 giây. Đáp án đến đúng mốc hết hạn bị từ chối.

Mỗi ván bắt đầu với 10 điểm. MAIN đúng nhận điểm của SUB sai. MAIN sai
bị loại, điểm MAIN chia cho SUB đúng, người đúng nhanh nhất thành MAIN.
SKIP chia phần nguyên một nửa điểm MAIN cùng điểm SUB sai cho SUB đúng.
Điểm dư chia cho người đúng nhanh nhất. Không SUB nào đúng khi SKIP thì
MAIN giữ điểm và thắng. MAIN sai mà không SUB nào đúng thì NO_WINNER.

Các cách hiểu này theo giả định ở mục 2 của kế hoạch và còn cần xác nhận
từ thầy. Chia đôi điểm lẻ thì MAIN giữ phần làm tròn lên. Hết câu hỏi,
người sống sót có điểm cao nhất thắng, bằng điểm chọn khe kết nối nhỏ
nhất. Ngân hàng câu hỏi được xáo khi server khởi động.

TURN_RESULT, ELIMINATED và GAME_OVER thông báo kết quả. Sau GAME_OVER
có thể START lại với điểm reset và qid mới. LEAVE, LOGOUT hoặc mất kết
nối trong lượt được xử lý như trả lời sai.

`make test` gồm đồng hồ giả cho 30/60 giây và kịch bản ba client qua
TCP chạy một lượt 30 giây thật, vì vậy test và coverage cần thêm thời gian.

## Bản hoàn thiện Tuần 4

Client hiển thị bảng điểm ở đầu ván, sau mỗi lượt và khi ván kết thúc.
Bảng điểm gồm tên, điểm và trạng thái ACTIVE, ELIMINATED hoặc DISCONNECTED.
MAIN bỏ qua nhận kết quả SKIPPED. Người hết thời gian nhận
ELIMINATED với lý do TIMEOUT.

Khi chạy trong terminal, client hiển thị đếm ngược trên stderr. Đồng hồ
bắt đầu khi nhận QUESTION, server quyết định thời điểm hết hạn. Khi chuyển
đầu ra sang file hoặc chạy test qua pipe, các dòng giao thức và kết quả vẫn
được giữ, countdown chỉ xuất hiện nếu stderr là terminal.

Log ghi nhận START, ANSWER, SKIP và các sự kiện QUESTION, ROLE,
TURN_RESULT, ELIMINATED, GAME_OVER. Tên người chơi và qid giúp đối chiếu
diễn biến, mật khẩu và nội dung câu hỏi không được ghi vào log game.

Các regression gồm MAIN mất kết nối sau SKIP, SKIP không có SUB đúng,
tính điểm dư cho 100 người và việc đăng nhập lại vào khe kết nối đã dùng.
Phiên đăng nhập lại không nhận kết quả của phiên cũ. Kịch bản TCP kiểm
tra điểm cuối cùng và bảng điểm của các client.

Tag `v0.9.0` đánh dấu mốc chức năng Tuần 4 sau khi CI đạt. Phần kiểm
thử chịu tải và báo cáo L1–L5 thuộc Tuần 5. Nhóm cần chạy demo với người
ngoài nhóm theo kế hoạch và review trước khi merge vào main.

## Hook kiểm tra khoảng cách commit

Chạy một lần trong mỗi bản sao repo:

```bash
chmod +x .githooks/pre-commit
git config core.hooksPath .githooks
```

Hook cập nhật thông tin từ `origin`, rồi kiểm tra mốc 30 phút dựa trên các
commit có trong repo cục bộ và các nhánh đã fetch.
