#include <reg52.h>
#include <string.h>

#define DataPort P0
#define KeyPort P1
sbit LATCH1 = P2^2;
sbit LATCH2 = P2^3;

unsigned char code dofly_DuanMa[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f,
                                     0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71};
unsigned char code dofly_WeiMa[] = {0xfe, 0xfd, 0xfb, 0xf7, 0xef, 0xdf, 0xbf, 0x7f};
unsigned char TempData[8];

// 串口通訊相關變數
unsigned char uart_buffer[20];
unsigned char uart_index = 0;
bit uart_flag = 0;

// 函數宣告
void UART_Init(void);
void UART_SendChar(unsigned char dat);
void UART_SendStr(unsigned char *s);
void SendKeyEvent(unsigned char key);
void ProcessUARTCommand(void);
void UpdateDisplay(unsigned char num);

void DelayMs(unsigned char t);
void Display(unsigned char FirstBit, unsigned char Num);
unsigned char KeyScan(void);
unsigned char KeyPro(void);
void Init_Timer0(void);

void main(void) {
    unsigned char num, j;
    
    // 初始化顯示緩衝區
    for(j=0; j<8; j++) 
        TempData[j] = 0;
    
    UART_Init();  // 初始化串口
    Init_Timer0();
    
    EA = 1;  // 開啟全域中斷
    
    while(1) {
        // 處理矩陣鍵盤輸入
        num = KeyPro();
        if(num != 0xff) {
            // 發送按鍵資訊至串口
            SendKeyEvent(num);
            
            // 更新顯示，新數字加入最右端
            for(j=0; j<7; j++)
                TempData[j] = TempData[j+1];
            TempData[7] = dofly_DuanMa[num];
        }
        
        // 處理串口接收的指令
        if(uart_flag) {
            ProcessUARTCommand();
            uart_flag = 0;
        }
    }
}

// 串口初始化 (9600 baud @ 11.0592MHz)
void UART_Init(void) {
    TMOD |= 0x20;   // 定時器1模式2
    TH1 = 0xFD;     // 9600波特率
    TL1 = 0xFD;
    TR1 = 1;        // 啟動定時器1
    SCON = 0x50;    // 模式1, 允許接收
    ES = 1;         // 開啟串口中斷
}

// 發送單個字元
void UART_SendChar(unsigned char dat) {
    SBUF = dat;
    while(!TI);
    TI = 0;
}

// 發送字串
void UART_SendStr(unsigned char *s) {
    while(*s != '\0') {
        UART_SendChar(*s);
        s++;
    }
}

// 發送按鍵事件
void SendKeyEvent(unsigned char key) {
    UART_SendStr("KEY:");
    UART_SendChar('0' + key);  // 直接發送數字字元
    UART_SendStr("\r\n");
}

// 處理串口接收的指令
void ProcessUARTCommand(void) {
    unsigned char i, display_num;
    
    // 檢查是否為顯示指令: "DISPLAY:X"
    if(uart_index >= 9 && 
       uart_buffer[0] == 'D' && uart_buffer[1] == 'I' && 
       uart_buffer[2] == 'S' && uart_buffer[3] == 'P' && 
       uart_buffer[4] == 'L' && uart_buffer[5] == 'A' && 
       uart_buffer[6] == 'Y' && uart_buffer[7] == ':') {
        
        // 取得要顯示的數字
        if(uart_buffer[8] >= '0' && uart_buffer[8] <= '9') {
            display_num = uart_buffer[8] - '0';
            UpdateDisplay(display_num);
        }
    }
    
    // 清空緩衝區
    uart_index = 0;
    for(i=0; i<20; i++) {
        uart_buffer[i] = 0;
    }
}

// 更新顯示緩衝區
void UpdateDisplay(unsigned char num) {
    unsigned char j;
    
    // 將新數字顯示在最右端，其他位左移
    for(j=0; j<7; j++) {
        TempData[j] = TempData[j+1];
    }
    TempData[7] = dofly_DuanMa[num];
}

// 串口中斷服務程式
void UART_ISR(void) interrupt 4 {
    if(RI) {
        RI = 0;
        
        // 接收字元並存入緩衝區
        if(uart_index < 19) {
            uart_buffer[uart_index] = SBUF;
            
            // 檢查是否收到換行符號，表示指令結束
            if(SBUF == '\n' || SBUF == '\r') {
                uart_flag = 1;  // 設定處理旗標
            } else {
                uart_index++;
            }
        } else {
            // 緩衝區滿，重置
            uart_index = 0;
        }
    }
}

void DelayMs(unsigned char t) {
    while(t--) {
        unsigned char i = 245;
        while(--i);
        i = 245;
        while(--i);
    }
}

void Display(unsigned char FirstBit, unsigned char Num) {
    static unsigned char i = 0;

    DataPort = 0;
    LATCH1 = 1;
    LATCH1 = 0;

    DataPort = dofly_WeiMa[i + FirstBit];
    LATCH2 = 1;
    LATCH2 = 0;

    DataPort = TempData[i];
    LATCH1 = 1;
    LATCH1 = 0;

    i++;
    if(i == Num)
        i = 0;
}

void Init_Timer0(void) {
    TMOD |= 0x01;
    EA = 1;
    ET0 = 1;
    TR0 = 1;
}

void Timer0_isr(void) interrupt 1 {
    TH0 = (65536 - 2000) / 256;
    TL0 = (65536 - 2000) % 256;
    Display(0, 8);
}

unsigned char KeyScan(void) {
    unsigned char cord_h, cord_l;
    KeyPort = 0x0f;
    cord_h = KeyPort & 0x0f;

    if(cord_h != 0x0f) {
        DelayMs(10);
        if((KeyPort & 0x0f) != 0x0f) {
            cord_h = KeyPort & 0x0f;
            KeyPort = cord_h | 0xf0;
            cord_l = KeyPort & 0xf0;
            while((KeyPort & 0xf0) != 0xf0);
            return (cord_h + cord_l);
        }
    }
    return (0xff);
}

// 矩陣鍵盤按鍵映射
unsigned char KeyPro(void) {
    switch(KeyScan()) {
        case 0x7e: return 1;  // 第一行第一列
        case 0x7d: return 4;  // 第一行第二列
        case 0x7b: return 7;  // 第一行第三列
        case 0xbe: return 2;  // 第二行第一列
        case 0xbd: return 5;  // 第二行第二列
        case 0xbb: return 8;  // 第二行第三列
        case 0xb7: return 0;  // 第二行第四列
        case 0xde: return 3;  // 第三行第一列
        case 0xdd: return 6;  // 第三行第二列
        case 0xdb: return 9;  // 第三行第三列
        default:   return 0xff; // 無按鍵或無效按鍵
    }
}
