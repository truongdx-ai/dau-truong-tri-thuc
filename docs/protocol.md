# Giao thức Đấu trường tri thức v0.1

Server và client trao đổi qua TCP. Mỗi thông điệp là một dòng kết thúc bằng
LF (\n), có thể kèm CR trước LF khi nhập từ terminal. Giới hạn là 1024 byte
tính cả ký tự kết thúc dòng. Các trường phân cách bằng dấu |. Trường không
được rỗng hoặc chứa dấu |, CR, LF hay byte NUL. Một lần đọc TCP có thể trả
về một phần dòng hoặc nhiều dòng.

Server hiện lắng nghe tại 127.0.0.1 và hỗ trợ PING → PONG và REGISTER. Dòng sai định
dạng nhận 400|BAD_REQUEST. Lệnh đúng định dạng nhưng chưa hỗ trợ nhận
400|UNSUPPORTED_COMMAND. Các lệnh và sự kiện còn lại dưới đây là đặc tả dự kiến.
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
| JOIN | 200 OK, 503 ROOM_FULL, 409 GAME_RUNNING |
| START | 200 OK, 403 NOT_HOST, 412 NOT_ENOUGH_PLAYERS |
| ANSWER với qid và đáp án A–D | 200 RECEIVED, 408 TIMEOUT, 403 NOT_ALLOWED |
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
