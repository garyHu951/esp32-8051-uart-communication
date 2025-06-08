# main.py - ESP32雙向通訊程式
from machine import Pin, UART
import time
import xtools
from xrequests import post, get

# ======== 使用者設定區 ========
ADAFRUIT_IO_USERNAME = "g123"
ADAFRUIT_IO_KEY = "aio_wQsl895gQxYStG0mueMgoaeyGMyd"
FEED_KEY = "matrix-keypad"        # Dashboard Number Pad的Feed名稱
UART_PORT = 2                  # 使用UART2
UART_BAUDRATE = 9600
UART_TX_PIN = 17
UART_RX_PIN = 16
POLL_INTERVAL = 2              # 輪詢Adafruit IO間隔(秒)
# ===========================

# Adafruit IO API URLs
ADA_IO_POST_URL = f"https://io.adafruit.com/api/v2/{ADAFRUIT_IO_USERNAME}/feeds/{FEED_KEY}/data"
ADA_IO_GET_URL = f"https://io.adafruit.com/api/v2/{ADAFRUIT_IO_USERNAME}/feeds/{FEED_KEY}/data/last"

# 初始化UART
uart = UART(UART_PORT, UART_BAUDRATE, tx=UART_TX_PIN, rx=UART_RX_PIN)
uart.init(UART_BAUDRATE, bits=8, parity=None, stop=1)

# 狀態變數
last_adafruit_value = None
last_poll_time = 0
last_uploaded_key = None  # 記錄最後上傳的按鍵值
upload_cooldown_time = 0  # 上傳後的冷卻時間

def send_key_to_adafruit(key):
    """發送按鍵信息到Adafruit IO Dashboard"""
    global last_uploaded_key, upload_cooldown_time
    
    headers = {"X-AIO-Key": ADAFRUIT_IO_KEY, "Content-Type": "application/json"}
    payload = {"value": str(key)}
    
    print(f"[上傳] 8051按鍵: {key}")
    
    try:
        response = post(ADA_IO_POST_URL, json=payload, headers=headers)
        print(f"[回應] 狀態碼: {response.status_code}")
        if response.status_code == 200:
            print("✓ Adafruit IO Dashboard已更新")
            # 記錄上傳的按鍵值和時間，避免重複處理
            last_uploaded_key = str(key)
            upload_cooldown_time = time.time() + 3  # 3秒冷卻時間
        else:
            print(f"✗ 上傳錯誤! 狀態碼: {response.status_code}")
            xtools.show_error()
    except Exception as e:
        print(f"✗ 上傳失敗: {e}")
        xtools.show_error()

def get_adafruit_value():
    """從Adafruit IO Dashboard取得最新值"""
    global last_adafruit_value, last_uploaded_key, upload_cooldown_time
    
    # 如果在上傳冷卻期間，跳過輪詢避免重複處理
    current_time = time.time()
    if current_time < upload_cooldown_time:
        return None
    
    headers = {"X-AIO-Key": ADAFRUIT_IO_KEY}
    
    try:
        response = get(ADA_IO_GET_URL, headers=headers)
        if response.status_code == 200:
            data = response.json()
            current_value = data.get("value")
            
            # 檢查是否有新的值，且不是剛剛上傳的值
            if (current_value != last_adafruit_value and 
                current_value != last_uploaded_key):
                print(f"[下載] Dashboard新值: {current_value}")
                last_adafruit_value = current_value
                return current_value
            else:
                # 更新last_adafruit_value但不回傳，避免重複處理
                last_adafruit_value = current_value
        else:
            print(f"✗ 下載錯誤! 狀態碼: {response.status_code}")
    except Exception as e:
        print(f"✗ 下載失敗: {e}")
    
    return None

def send_to_8051(value):
    """發送數字到8051七段顯示器"""
    try:
        # 確保值在0-9範圍內
        if value.isdigit() and 0 <= int(value) <= 9:
            message = f"DISPLAY:{value}\r\n"
            uart.write(message.encode('utf-8'))
            print(f"[發送到8051] 顯示數字: {value}")
        else:
            print(f"[警告] 無效數字: {value}")
    except Exception as e:
        print(f"✗ 發送到8051失敗: {e}")

def process_8051_data():
    """處理8051傳來的數據"""
    if uart.any() > 0:
        try:
            data = uart.readline()
            if data:
                message = data.decode('utf-8').strip()
                if message.startswith("KEY:"):
                    key = message[4:]
                    print(f"[接收] 8051按鍵: {key}")
                    send_key_to_adafruit(key)
                    return True
        except Exception as e:
            print(f"✗ 8051數據解碼錯誤: {e}")
    return False

def poll_dashboard():
    """輪詢Dashboard數據"""
    global last_poll_time
    current_time = time.time()
    
    if (current_time - last_poll_time) >= POLL_INTERVAL:
        new_value = get_adafruit_value()
        if new_value is not None:
            print(f"[Dashboard控制] 發送顯示指令到8051: {new_value}")
            send_to_8051(new_value)
        last_poll_time = current_time

# 初始化WiFi連接
print("正在連接WiFi...")
ip = xtools.connect_wifi_led()
if ip:
    print(f"✓ WiFi已連接，IP: {ip}")
else:
    print("✗ WiFi連接失敗!")
    xtools.show_error(1)

# 獲取初始Dashboard值
print("正在同步Dashboard初始狀態...")
initial_value = get_adafruit_value()
if initial_value:
    send_to_8051(initial_value)

print("=" * 50)
print("系統準備完成!")
print("功能說明:")
print("1. 8051矩陣鍵盤 → Dashboard Number Pad")
print("2. Dashboard Number Pad → 8051七段顯示器")
print(f"3. Dashboard輪詢間隔: {POLL_INTERVAL}秒")
print("=" * 50)

# 主循環
while True:
    # 處理8051傳來的按鍵數據
    if process_8051_data():
        # 如果收到8051數據，暫停輪詢避免立即回傳相同值
        print("[系統] 8051按鍵處理完成，暫停Dashboard輪詢3秒")
        time.sleep(0.5)
    
    # 定期輪詢Dashboard (但會跳過冷卻期)
    poll_dashboard()
    
    # 短暫延遲減少CPU負載
    time.sleep(0.1)