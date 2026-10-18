# Hệ thống giám sát pin và sạc xe điện (ESP32 + MQTT + Web)

Mô phỏng trên Wokwi ngay trong VS Code. Không cần phần cứng thật.

## Cấu trúc
- `src/main.cpp`        firmware ESP32 (đo, tính SOC, bảo vệ, MQTT)
- `diagram.json`        sơ đồ mạch Wokwi (ESP32, 2 biến trở, NTC, relay, LED, còi)
- `wokwi.toml`          trỏ Wokwi tới firmware do PlatformIO build
- `platformio.ini`      cấu hình build
- `dashboard/index.html` web dashboard (mở bằng trình duyệt)

## Cài đặt (làm một lần)
1. Cài VS Code, rồi cài 2 extension: **PlatformIO IDE** và **Wokwi Simulator**.
2. Mở đúng thư mục chứa `platformio.ini` bằng VS Code: `ev-bms-iot/ev-bms-iot`. Đợi PlatformIO tải xong toolchain (lần đầu khá lâu).
3. Nhấn F1, chạy **Wokwi: Request a Free License** và đăng nhập để kích hoạt.

## Chạy
1. Build firmware trước khi mở Wokwi: biểu tượng dấu tích của PlatformIO ở thanh dưới, hoặc `Ctrl+Alt+B`.
	Có thể chạy thủ công trong Terminal tại thư mục project:
	`pio run -e esp32`
2. Xác nhận hai file sau đã được tạo:
	`.pio/build/esp32/firmware.bin` và `.pio/build/esp32/firmware.elf`.
3. Nhấn F1, chạy **Wokwi: Start Simulator**.
4. Mở `dashboard/index.html` bằng trình duyệt (nhấp đúp, hoặc chuột phải > Open with Live Server).
5. Trong Wokwi, kéo biến trở và quan sát dashboard.

### Sửa lỗi `firmware.bin not found`

Lỗi này có nghĩa là Wokwi được mở trước khi PlatformIO build xong, hoặc VS Code đang mở nhầm thư mục cha. Hãy mở thư mục có `platformio.ini`, cài extension **PlatformIO IDE**, chờ toolchain cài xong rồi chạy `pio run -e esp32`. Không đổi đường dẫn trong `wokwi.toml`: đường dẫn đó trỏ tương đối tới thư mục project và sẽ đúng sau khi build.

## Đổi topic cho riêng bạn
Broker HiveMQ công cộng ai cũng đọc được. Đổi `evbms-uth-demo` thành chuỗi riêng ở **cả hai nơi**:
`TOPIC_BASE` trong `src/main.cpp` và `BASE` trong `dashboard/index.html`.

## Kịch bản demo
| Thao tác trong Wokwi / web | Kết quả |
|---|---|
| Kéo biến trở điện áp lên giữa | SOC khoảng 50%, relay đóng (đang sạc) |
| Tắt "Cho phép sạc" trên web | Relay ngắt, trạng thái "theo yêu cầu của bạn" |
| Kéo thanh giới hạn xuống 60% khi SOC cao hơn | Relay ngắt, "đạt giới hạn" |
| Tăng nhiệt độ NTC trên 45 °C | Banner đỏ, LED sáng, còi kêu, relay ngắt |
| Kéo biến trở dòng ra xa mức 0 (trên 15 A) | Cảnh báo quá dòng |
| Kéo biến trở điện áp sát tối đa hoặc tối thiểu | Cảnh báo quá áp hoặc thấp áp |

## Ngưỡng mặc định (sửa trong main.cpp)
Pack 3S: 0% tại 9.3 V, 100% tại 12.6 V. Quá dòng 15 A. Quá nhiệt 45 °C.
SOC ước lượng tuyến tính theo điện áp. Pin thật dùng đường cong OCV hoặc Coulomb counting.

## Ước lượng thời gian sạc trên dashboard

Chức năng được tính hoàn toàn trong `dashboard/index.html`, dùng các trường MQTT sẵn có `soc`, `i`, `limit`, `relay`, `charge_enabled`, `fault`. Không cần sửa hoặc build lại firmware khi chỉ cập nhật web.

### Cách sử dụng

1. Chạy Wokwi và mở dashboard, chờ thiết bị gửi dữ liệu.
2. Trong **Điều khiển sạc**, nhập **Dung lượng pack pin (Ah)** theo pack đang mô phỏng. Mặc định **20 Ah chỉ là giá trị minh họa**, không suy ra từ cấu hình pack 3S. Giá trị hợp lệ từ 0,1 đến 10.000 Ah; tải lại trang sẽ trở về 20 Ah. Pack 3S mắc nối tiếp có dung lượng Ah bằng dung lượng một cell nếu không có nhánh song song.
3. Bật **Cho phép sạc**, đặt giới hạn lớn hơn SOC hiện tại và chỉnh biến trở dòng để dòng dương, lớn hơn 0,1 A và không vượt 15 A. Giữ nhiệt độ và điện áp trong ngưỡng cho phép.
4. Đọc **Tới giới hạn …%** để biết thời gian còn lại tới ngưỡng thiết bị xác nhận qua MQTT. **Tới đầy 100%** là dự báo nếu tiếp tục sạc với dòng hiện tại tới 100%; giới hạn dưới 100% vẫn làm firmware ngắt sạc trước mốc này.
5. Thay đổi dung lượng để tính lại ngay; thay đổi dòng, SOC hoặc giới hạn sẽ cập nhật sau bản tin MQTT tiếp theo (khoảng 1 giây). Thanh giới hạn chỉ có hiệu lực với dự báo sau khi thiết bị gửi lại giá trị đã áp dụng.

### Công thức và trạng thái hiển thị

`Thời gian (giờ) = dung lượng pack (Ah) × (SOC đích − SOC hiện tại) / 100 / dòng sạc (A)`.

Ví dụ: pack 20 Ah, SOC 50%, dòng 5 A, giới hạn 80% → còn khoảng **1 giờ 12 phút** tới 80%, **2 giờ** tới 100%. Tăng dòng lên 10 A → tương ứng **36 phút** và **1 giờ**. Thời gian làm tròn lên phút, tối thiểu 1 phút khi chưa tới đích.

| Điều kiện | Hiển thị |
|---|---|
| Chưa nhận bản tin | Chờ dữ liệu |
| Mất kết nối broker hoặc quá 5 giây không có bản tin | Mất dữ liệu; ẩn dự báo cũ |
| Dung lượng trống, ngoài khoảng hoặc dữ liệu SOC/dòng/giới hạn không hợp lệ | Yêu cầu nhập dung lượng hoặc báo dữ liệu không hợp lệ |
| Có lỗi bảo vệ | Tạm dừng do lỗi |
| SOC đã đạt đích, không có lỗi | Đã đạt giới hạn / Đã đầy |
| Relay ngắt hoặc tắt cho phép sạc khi chưa đạt đích | Đang ngừng sạc |
| Relay đang đóng, dòng âm | Đang xả pin |
| Relay đang đóng, dòng từ 0 đến 0,1 A | Dòng sạc quá thấp; không chia cho 0 |

Đây là dự báo với dòng không đổi, chưa tính tổn hao hay giai đoạn giảm dòng gần đầy. Trong mô phỏng hiện tại, SOC lấy từ biến trở điện áp, không tự tăng theo thời gian hoặc dòng sạc. Vì vậy số phút sẽ được tính lại theo dữ liệu mới, không tự đếm ngược khi các giá trị đứng yên.

## Linh kiện và nguyên lý hoạt động

| Linh kiện trong `diagram.json` | Kết nối | Hoạt động và cách sử dụng |
|---|---|---|
| ESP32 DevKit (`esp`) | Wi-Fi Wokwi-GUEST; MQTT HiveMQ | Đọc ADC 12 bit, lấy trung bình 16 mẫu mỗi đầu vào; khoảng mỗi giây tính SOC, kiểm tra bảo vệ, điều khiển đầu ra và gửi telemetry. Web dùng telemetry để hiển thị và tính thời gian. |
| Biến trở điện áp (`potV`) | VCC 3V3, GND, SIG GPIO34 | Giả lập điện áp pack 9–13 V bằng vị trí biến trở. SOC tuyến tính từ 9,3 V (0%) tới 12,6 V (100%), chặn trong 0–100%. Kéo để thay SOC và lượng Ah còn phải nạp; không cấp điện áp pack thật vào chân ADC. |
| Biến trở dòng (`potI`) | VCC 3V3, GND, SIG GPIO35 | Giả lập −20 đến +20 A; giữa là gần 0 A, phía dương là sạc, phía âm là xả. Web dùng dòng dương để tính thời gian; tăng dòng làm dự báo ngắn hơn. Trị tuyệt đối trên 15 A gây lỗi bảo vệ. |
| Cảm biến NTC (`ntc1`) | VCC 3V3, GND, OUT GPIO32 | Firmware đổi ADC sang °C với hệ số Beta 3950. Chỉnh nhiệt độ trong Wokwi; trên 45 °C ngắt sạc và tạm dừng dự báo. |
| Module relay (`relay1`) | VCC VIN, GND, IN GPIO27 | Firmware xuất HIGH khi cho phép sạc, không có lỗi và SOC dưới giới hạn; ngược lại xuất LOW. Web dùng trạng thái `relay` do firmware gửi. Sơ đồ hiện chỉ nối phần điều khiển relay, chưa có bộ sạc hay pack pin nối vào tiếp điểm. |
| LED đỏ (`led1`) và điện trở 220 Ω (`r1`) | GPIO25 → điện trở → anode LED; cathode → GND | Điện trở hạn dòng LED; LED sáng khi có bất kỳ lỗi bảo vệ nào, tắt khi hết lỗi. |
| Còi (`bz1`) | Chân 2 GPIO26, chân 1 GND | ESP32 phát âm 2.000 Hz khi có lỗi, tắt khi hết lỗi. Thử tăng nhiệt độ hoặc đưa dòng vượt ngưỡng để quan sát cùng LED và relay. |

Luồng hoạt động: **biến trở/NTC → ESP32 → MQTT → dashboard**. Lệnh bật/tắt và giới hạn đi ngược từ web qua MQTT tới ESP32; dung lượng Ah chỉ dùng tại web, không gửi xuống thiết bị. Relay trong mô phỏng không làm thay đổi giá trị biến trở, vì vậy phải tự chỉnh biến trở khi trình diễn quá trình pin tăng SOC.

## Lưu ý
- Nếu dây trong `diagram.json` bị lệch, mở file đó trong VS Code và kéo lại dây trực tiếp trên giao diện Wokwi.
- Chân kết nối: V=GPIO34, I=GPIO35, NTC=GPIO32, LED=GPIO25, còi=GPIO26, relay=GPIO27.

## Gauge và đồ thị nhiều chỉ số

Dashboard hiển thị gauge vòng tiến độ SOC tới giới hạn sạc và hai đồ thị: SOC/dòng điện, điện áp/nhiệt độ. Mỗi chỉ số có trục và đơn vị riêng; dòng dương là sạc, dòng âm là xả. Có thể bấm chú giải để ẩn/hiện từng đường.

Chọn **Khoảng thời gian xem**: **1 phút**, **5 phút** (mặc định) hoặc **15 phút** để cập nhật cả hai đồ thị. Cửa sổ trượt theo thời gian trên trình duyệt, kể cả khi thiết bị ngừng gửi dữ liệu. Dữ liệu chỉ được thu thập từ lúc mở trang; đổi khoảng xem không tạo thêm dữ liệu quá khứ. CSV chứa toàn bộ mẫu nhận trong phiên, không phụ thuộc bộ lọc. **Xóa biểu đồ** xóa cả dữ liệu đồ thị và lịch sử CSV; tải lại trang cũng xóa lịch sử.

## Phát hiện bất thường và cảnh báo sớm thermal runaway

Dashboard phân tích telemetry bằng `dashboard/anomaly.js`, không cần build lại firmware:

- **Tăng nhiệt nhanh:** ước lượng đạo hàm `dT/dt ≈ (T hiện tại − T đầu cửa sổ) / Δt × 60`, đơn vị °C/phút. Cửa sổ tối đa 10 giây, cần ít nhất 5 giây dữ liệu liên tục; cảnh báo khi tốc độ ≥ 6 °C/phút và mức tăng ≥ 0,5 °C trong 3 mẫu liên tiếp. Nhờ vậy có thể cảnh báo ngay khi nhiệt độ còn dưới ngưỡng quá nhiệt 45 °C. Cảnh báo hết khi điều kiện không còn thỏa.
- **Sụt áp khi tăng dòng:** so sánh hai mẫu cách nhau 0,5–3 giây, cảnh báo nếu điện áp giảm ≥ 0,3 V và độ lớn dòng tăng ≥ 2 A, không đổi chiều sạc/xả. Dùng |I| vì dòng âm là xả. Cảnh báo giữ 10 giây từ lần phát hiện cuối, trừ khi mất dữ liệu hoặc kết nối.
- Ngắt kết nối, khoảng trống dữ liệu > 5 giây, thời gian không tăng hoặc mẫu V/I/T không hợp lệ sẽ đặt lại mốc phân tích. Bộ phát hiện dùng đồng hồ đơn điệu của trình duyệt. Xóa biểu đồ không đặt lại bộ phát hiện.
- Cảnh báo màu vàng xuất hiện riêng với lỗi bảo vệ màu đỏ; có thể hiển thị đồng thời cả hai bất thường. CSV bổ sung tốc độ nhiệt, cờ tăng nhiệt nhanh và cờ sụt áp. Khi chưa đủ dữ liệu, tốc độ nhiệt trong CSV để trống.

Các ngưỡng trên là **heuristic minh họa cho Wokwi**, chưa hiệu chuẩn theo hóa học pin, SOC, tải và cảm biến thực tế. Nhiệt tăng nhanh có thể được trình bày như dấu hiệu cảnh báo sớm nguy cơ **thermal runaway (mất kiểm soát nhiệt)**; phép đo này không xác nhận hay bảo đảm dự báo hiện tượng đó. Sụt áp khi tăng tải cũng có thể là đáp ứng tải thông thường, không đủ để kết luận thermal runaway. Cảnh báo sớm chỉ hoạt động khi dashboard đang mở và nhận dữ liệu; không tự gửi lệnh ngắt sạc. Firmware vẫn ngắt theo các ngưỡng bảo vệ tuyệt đối hiện có.

### Demo và kiểm tra

1. Mở dashboard và Wokwi, giữ nhiệt độ khoảng 30 °C ít nhất 10 giây.
2. Tăng nhiệt độ NTC khoảng 0,2 °C mỗi giây trong 8–10 giây, giữ dưới 45 °C: kiểm tra tốc độ xấp xỉ 12 °C/phút và cảnh báo sớm màu vàng. Giảm nhiệt để xóa điều kiện cảnh báo.
3. Giữ dòng dương khoảng 2 A và điện áp khoảng 12 V. Trong một chu kỳ telemetry, tăng dòng lên khoảng 5 A đồng thời hạ điện áp xuống khoảng 11,5 V: xuất hiện cảnh báo sụt áp; giữ ổn định để thấy cảnh báo hết sau 10 giây. Có thể thử tương tự với dòng xả −2 A → −5 A.
4. Dừng mô phỏng > 5 giây rồi chạy lại: dashboard phải thu thập lại mốc nhiệt, không so sánh qua khoảng mất dữ liệu.
5. Chạy kiểm thử logic bằng Node.js: `node --test ev-bms-iot/dashboard/anomaly.test.cjs` từ thư mục gốc repository (hoặc `node --test dashboard/anomaly.test.cjs` từ thư mục project).
