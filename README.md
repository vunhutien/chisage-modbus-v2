## Code đọc modbus bằng ESP8266 + MAX485 cho anh em xài inverter Chisage ESS 1pha (6kw - tested) - KHÔNG CẦN DÙNG Home Assistant nhé
### Lưu ý code mạch này đang dùng cho ESP8266, bản ESP32 mình đang đặt hàng về, có thiết bị mình sẽ update code cho bản ESP32
Nguyên liệu cần có:
- ESP8266 - https://s.shopee.vn/3q8Orm1hSu 47k VND
- Module Max485 - https://s.shopee.vn/3foyfT2Knt - 10k VND
- Dây mạng bấm 1 đầu, vài cọng dây nhỏ để kết nối 2 thiết bị với nhau

Sơ đồ đấu nối:

|ESP8266  |	MAX485 |
|---|---|
|D2 (GPIO4, RX) |	RO (Receiver Output) |
|D1 (GPIO5, TX)	| DI (Driver Input)|
|D4 (GPIO2, DE/RE) |	DE & RE (Tied together) - 2 chân này cùng nối vào 1 chân D4 trên ESP8266 |
|3.3V |	VCC |
| GND |	GND |

Dây mạng chỉ bấm code 1 đầu và cắm vào cổng Modbus trên biến tần, đầu còn lại lấy 2 dây số 1 và số 2 thì dây số 1 nối vào chân A trên MAX485, dây số 2 nối vào chấn B - 
Cấp nguồn cho ESP8266 qua cổng usb cắm vào máy tính.

Cài đặt Arduino IDE trên máy tính, copy code ở file chisage_basic.ino, thay Wifi name + Password trong code và Flash code vào mạch là xong. Sau khi Upload code xong thì xem địa chỉ ip của ESP là gì truy cập trực tiếp vào qua trình duyệt sẽ hiện thị dashboard



https://github.com/user-attachments/assets/282079e5-1876-45cb-96da-997bd3567f51

