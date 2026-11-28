# Giao thức Đấu trường tri thức v0.4

Server và client trao đổi qua TCP. Mỗi thông điệp là một dòng kết thúc bằng
LF (\n), có thể kèm CR trước LF khi nhập từ terminal. Giới hạn là 1024 byte
tính cả ký tự kết thúc dòng. Các trường phân cách bằng dấu |. Trường không
được rỗng hoặc chứa dấu |, CR, LF hay byte NUL. Một lần đọc TCP có thể trả
về một phần dòng hoặc nhiều dòng.

Server hiện lắng nghe tại 127.0.0.1 và hỗ trợ PING, REGISTER, LOGIN, LOGOUT,
JOIN, LEAVE, START, ANSWER và SKIP. Dòng sai định
dạng nhận 400|BAD_REQUEST. Lệnh đúng định dạng nhưng chưa hỗ trợ nhận
400|UNSUPPORTED_COMMAND.
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
thái CORRECT, WRONG hoặc SKIPPED và điểm của người nhận.

## Phiên và phòng chờ

JOIN, LEAVE, START và LOGOUT yêu cầu đăng nhập. Thiếu phiên nhận
`401|LOGIN_REQUIRED`. LOGIN sai tài khoản hoặc mật khẩu nhận
`401|WRONG_CREDENTIAL`. Một tài khoản chỉ đăng nhập trên một kết nối.
LOGIN khi kết nối đã đăng nhập cũng nhận `409|ALREADY_LOGGED_IN`.
Lỗi đọc dữ liệu tài khoản trả về `500|STORAGE_ERROR`.

Phiên hết hạn sau 300 giây không có lệnh hợp lệ được nhận. Client gửi
PING mỗi 60 giây. Hết hạn hoặc mất kết nối sẽ rời phòng và giải phóng
tên đăng nhập. LEAVE chỉ rời phòng. LOGOUT rời phòng và xoá phiên.
JOIN lặp lại giữ nguyên thứ tự tham gia. Host là người JOIN sớm nhất
còn ở phòng. START yêu cầu host và ít nhất hai người, chuyển phòng
sang vòng loại, gửi QUESTION cho thành viên phòng. JOIN và START tiếp
theo nhận 409 GAME_RUNNING. Phòng về trạng thái chờ khi ván kết thúc.

Server giới hạn 100 kết nối đồng thời. Kết nối vượt mức được đóng.
PLAYERS liệt kê theo khe kết nối và có thể khác thứ tự JOIN.
Khi danh sách vượt giới hạn 1024 byte, server gửi PLAYERS_BEGIN,
một hoặc nhiều dòng PLAYERS_PART với các tên cách nhau bằng dấu phẩy,
rồi PLAYERS_END. Client ghép các phần giữa hai mốc để lấy danh sách đầy đủ.
Mỗi thông điệp vẫn tuân theo giới hạn 1024 byte. Các tên tài khoản có độ
dài tối đa được hỗ trợ cho đủ 100 người.

## Vòng chơi và tính thời gian

QUESTION có chín trường gồm tên sự kiện, qid, QUALIFY hoặc TURN, số giây,
nội dung và bốn đáp án. Đáp án đúng chỉ xuất hiện trong TURN_RESULT.
Vòng loại cho mọi người 30 giây. Lượt chính cho SUB 30 giây và MAIN 60
giây. Server dùng đồng hồ monotonic và thứ tự nhận dòng hoàn chỉnh.

ANSWER chỉ nhận một chữ A–D và qid số nguyên dương. Sai định dạng nhận
400 BAD_REQUEST, qid cũ nhận 409 STALE_QUESTION, đã trả lời hoặc SKIP
nhận 409 ALREADY_ANSWERED, quá hạn nhận 408 TIMEOUT, bị loại hoặc ngoài
ván nhận 403 NOT_ALLOWED. Thiếu đăng nhập nhận 401 LOGIN_REQUIRED.
SKIP dùng qid hiện tại, chỉ MAIN được dùng và giới hạn hai lần mỗi người
làm MAIN trong ván. Vượt giới hạn nhận 403 NO_SKIP_LEFT.

Không có người đúng qua ba lần vòng loại hoặc hết câu trong vòng loại
thì gửi GAME_OVER|CANCELLED. Hết lượt, TURN_RESULT đi trước ELIMINATED.
GAME_OVER gửi cho thành viên ván vẫn ở phòng, gồm cả người đã bị loại.
Sau khi kết thúc có thể START lại. qid tăng qua các ván để từ chối đáp án
trễ từ ván cũ. Nếu không có ngân hàng câu hỏi, START nhận 503 NO_QUESTIONS.

## Bảng điểm và kết thúc ván

Server gửi một ảnh chụp bảng điểm theo dạng sau:

```text
SCOREBOARD_BEGIN|7
SCORE|an|30|ACTIVE
SCORE|tu|0|ELIMINATED
SCORE|chi|0|DISCONNECTED
SCOREBOARD_END|7
```

Mỗi SCORE là một dòng riêng, gồm tên, điểm và trạng thái. SCOREBOARD_END
có cùng qid với BEGIN đánh dấu bảng điểm hoàn chỉnh.
Bảng có tối đa 100 người và mỗi dòng tuân theo giới hạn 1024 byte.

Bảng điểm được gửi sau QUESTION đầu ván, sau TURN_RESULT và ELIMINATED
ở mỗi lượt, trước ROLE hoặc QUESTION kế tiếp hay GAME_OVER. Người đã bị
loại và vẫn ở phòng tiếp tục nhận bảng điểm. Người đã rời hoặc phiên mới
dùng lại khe kết nối không nhận dữ liệu của phiên trước.

MAIN đã SKIP nhận TURN_RESULT có trạng thái SKIPPED. Khi không trả lời
và bị loại, lý do trong ELIMINATED là TIMEOUT. Đáp án sai nhận WRONG_ANSWER.
Các dòng GAME_OVER|WINNER, NO_WINNER hoặc CANCELLED được giữ nguyên.
