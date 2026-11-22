# Giao thức Đấu trường tri thức v0.3

Server và client trao đổi qua TCP. Mỗi thông điệp là một dòng kết thúc bằng
LF (\n), có thể kèm CR trước LF khi nhập từ terminal. Giới hạn là 1024 byte
tính cả ký tự kết thúc dòng. Các trường phân cách bằng dấu |. Trường không
được rỗng hoặc chứa dấu |, CR, LF hay byte NUL. Một lần đọc TCP có thể trả
về một phần dòng hoặc nhiều dòng.

Server hiện lắng nghe tại 127.0.0.1 và hỗ trợ PING, REGISTER, LOGIN, LOGOUT,
JOIN, LEAVE, START và ANSWER cho vòng mở màn. Dòng sai định
dạng nhận 400|BAD_REQUEST. Lệnh đúng định dạng nhưng chưa hỗ trợ nhận
400|UNSUPPORTED_COMMAND. Đã có QUESTION loại QUALIFY, ROLE và
GAME_OVER|NO_WINNER. SKIP, QUESTION loại TURN, TURN_RESULT, ELIMINATED và
GAME_OVER|WINNER thuộc phần vòng chơi tính điểm, chưa được triển khai.
Tên đăng ký có 1–32 ký tự ASCII trong tập chữ cái, chữ số, dấu gạch dưới
và dấu gạch ngang. Mật khẩu có 1–128 byte. Lỗi ghi cơ sở dữ liệu khi đăng
ký trả về `500|STORAGE_ERROR`.

## Lệnh từ client

Phản hồi có dạng MÃ|NỘI_DUNG, trừ PONG. Mã 2xx báo thành công, 4xx báo lỗi
phía client và 5xx báo lỗi phía server.

| Lệnh | Phản hồi |
| --- | --- |
| `REGISTER\|user\|pass` | `200\|OK`, `409\|USER_EXISTS`, `400\|BAD_REQUEST` |
| `LOGIN\|user\|pass` | `200\|OK`, `401\|WRONG_CREDENTIAL`, `409\|ALREADY_LOGGED_IN` |
| LOGOUT | 200 OK |
| LEAVE | 200 OK |
| JOIN | 200 OK, 503 ROOM_FULL, 409 GAME_RUNNING |
| START | 200 OK, 403 NOT_HOST, 412 NOT_ENOUGH_PLAYERS, 500 NO_QUESTIONS, 500 QUESTION_ERROR |
| ANSWER với qid và đáp án A–D | 200 RECEIVED, 408 TIMEOUT, 403 NOT_ALLOWED, 409 ALREADY_ANSWERED, 409 WRONG_QUESTION, 400 BAD_REQUEST |
| SKIP với qid | 200 OK, 403 NO_SKIP_LEFT, 403 NOT_MAIN |
| PING | PONG |

Ví dụ client gửi `ANSWER|7|D` và server trả về `200|RECEIVED`. Cả hai dòng đều
kết thúc bằng LF.

## Sự kiện do server gửi

Mỗi dòng trong ví dụ sau là một thông điệp riêng.

```text
PLAYERS|2|an,tu
QUESTION|7|TURN|30|2 + 2 bằng bao nhiêu?|1|2|3|4
ROLE|MAIN
ROLE|SUB|an
TURN_RESULT|D|CORRECT|15
ELIMINATED|WRONG_ANSWER
GAME_OVER|WINNER|an|15
GAME_OVER|NO_WINNER
```

PLAYERS được gửi khi danh sách phòng chờ thay đổi. QUESTION có loại QUALIFY
hoặc TURN và trường số giây dành cho người nhận. ROLE được gửi sau vòng
loại hoặc khi đổi người chơi chính. TURN_RESULT chứa đáp án đúng, trạng
thái CORRECT hoặc WRONG và điểm của người nhận.

## Vòng mở màn đã triển khai

Sau START hợp lệ, host nhận 200|OK, các thành viên nhận PLAYERS rồi cùng
nhận QUESTION. Khi server vừa khởi động, ví dụ câu hỏi đầu có qid là 1:

```text
QUESTION|1|QUALIFY|30|2 + 2 bằng bao nhiêu?|1|2|3|4
```

Chỉ những người đã JOIN khi START được chấp nhận được tham gia vòng này.
Đáp án đúng được giữ tại server. Client gửi ANSWER|qid|A (hoặc B, C, D).
qid phải là số nguyên dương trong phạm vi uint64; dấu cộng, dấu trừ và
tràn số bị từ chối. Đúng định dạng nhưng sai qid nhận 409|WRONG_QUESTION.
Chưa đăng nhập nhận 401|LOGIN_REQUIRED; không thuộc vòng nhận 403|NOT_ALLOWED.

Server chỉ ghi nhận đáp án hợp lệ đầu tiên của mỗi người và trả 200|RECEIVED,
kể cả đáp án sai. Gửi lại trước hạn nhận 409|ALREADY_ANSWERED, không thể sửa
đáp án. Đáp án đến tại hoặc sau hạn nhận 408|TIMEOUT. Thời điểm được ghi
bằng đồng hồ CLOCK_MONOTONIC tại server khi xử lý đủ dòng kết thúc bằng LF,
không dùng thời gian do client cung cấp hoặc thời gian trước khi chờ poll.

Vòng kết thúc khi tất cả thành viên còn lại đã trả lời hoặc hết hạn, kể cả
khi không có thêm lệnh từ client. Server so thời điểm nhận các đáp án đúng,
chọn người nhanh nhất còn trong phòng. Người đó nhận ROLE|MAIN, những người
còn lại nhận ROLE|SUB|ten_nguoi_chinh. Phản hồi ANSWER luôn được xếp trước
ROLE/GAME_OVER phát sinh từ chính lệnh đó. Sự kiện kết quả chỉ phát một lần
cho mỗi lần kết thúc hoặc đổi người chính.

Các quy ước triển khai dưới đây bổ sung những chỗ đề chưa quy định rõ:

- Vòng mở màn có 30 giây, tính từ lúc server chấp nhận START. Đáp án đúng
  nhanh nhất được chọn sau khi thu đủ đáp án hoặc hết giờ.
- Nếu hai đáp án có cùng thời điểm tính theo mili giây, ưu tiên thứ tự
  server xử lý đủ dòng. Thứ tự này có thể chịu ảnh hưởng thứ tự duyệt poll.
- Người trả lời sai hoặc không trả lời ở vòng mở màn vẫn là người phụ nếu
  chọn được người chính; chưa áp dụng luật loại và chia điểm của vòng TURN.
- Nếu không còn ai trả lời đúng (kể cả tất cả không trả lời), phát
  GAME_OVER|NO_WINNER và đưa phòng về trạng thái chờ. Thành viên còn lại
  vẫn ở phòng, host có thể START lại.
- LEAVE, LOGOUT, hết phiên và mất kết nối đều bỏ người đó khỏi vòng;
  đáp án của người đã rời phòng không còn được xét. Khe kết nối tái sử dụng
  không kế thừa quyền tham gia. Nếu người chính vừa chọn rời đi trước khi
  có vòng TURN, chọn người đúng nhanh nhất tiếp theo; không có thì kết thúc
  không người thắng. Khi phòng trống, hủy vòng.
- Mỗi START thành công dùng một qid mới tăng dần trong suốt lần chạy server.
  Câu hỏi được lấy tuần tự từ bộ dữ liệu, quay lại đầu khi hết; qid vẫn tăng.
  Sau khi khởi động lại server, qid bắt đầu lại từ 1.

Thiếu bộ câu hỏi hoặc không thể định dạng câu hỏi trả lỗi 500, giữ phòng
ở trạng thái chờ và không phát QUESTION. Khi đã chọn người chính, phòng
giữ trạng thái đang chơi để phần vòng TURN tiếp quản; START/JOIN bị chặn.

## Phiên và phòng chờ

JOIN, LEAVE, START, ANSWER và LOGOUT yêu cầu đăng nhập. Thiếu phiên nhận
`401|LOGIN_REQUIRED`. LOGIN sai tài khoản hoặc mật khẩu nhận
`401|WRONG_CREDENTIAL`. Một tài khoản chỉ đăng nhập trên một kết nối.
LOGIN khi kết nối đã đăng nhập cũng nhận `409|ALREADY_LOGGED_IN`.
Lỗi đọc dữ liệu tài khoản trả về `500|STORAGE_ERROR`.

Phiên hết hạn sau 300 giây không có lệnh hợp lệ được nhận. Client gửi
PING mỗi 60 giây. Hết hạn hoặc mất kết nối sẽ rời phòng và giải phóng
tên đăng nhập. LEAVE chỉ rời phòng. LOGOUT rời phòng và xoá phiên.
JOIN lặp lại giữ nguyên thứ tự tham gia. Host là người JOIN sớm nhất
còn ở phòng. START yêu cầu host và ít nhất hai người, chuyển phòng
sang trạng thái đang chơi. JOIN và START tiếp theo nhận 409 GAME_RUNNING.
Phòng quay về trạng thái chờ khi không còn thành viên hoặc vòng mở màn
kết thúc không có người thắng.

Server giới hạn 100 kết nối đồng thời. Kết nối vượt mức được đóng.
PLAYERS liệt kê theo khe kết nối và có thể khác thứ tự JOIN.
Khi danh sách vượt giới hạn 1024 byte, server gửi PLAYERS_BEGIN,
một hoặc nhiều dòng PLAYERS_PART với các tên cách nhau bằng dấu phẩy,
rồi PLAYERS_END. Client ghép các phần giữa hai mốc để lấy danh sách đầy đủ.
Mỗi thông điệp vẫn tuân theo giới hạn 1024 byte. Các tên tài khoản có độ
dài tối đa được hỗ trợ cho đủ 100 người.
