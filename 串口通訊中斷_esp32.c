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

// ��f�q�T�����ܼ�
unsigned char uart_buffer[20];
unsigned char uart_index = 0;
bit uart_flag = 0;

// ��ƫŧi
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
    
    // ��l����ܽw�İ�
    for(j=0; j<8; j++) 
        TempData[j] = 0;
    
    UART_Init();  // ��l�Ʀ�f
    Init_Timer0();
    
    EA = 1;  // �}�ҥ��줤�_
    
    while(1) {
        // �B�z�x�}��L��J
        num = KeyPro();
        if(num != 0xff) {
            // �o�e�����T�ܦ�f
            SendKeyEvent(num);
            
            // ��s��ܡA�s�Ʀr�[�J�̥k��
            for(j=0; j<7; j++)
                TempData[j] = TempData[j+1];
            TempData[7] = dofly_DuanMa[num];
        }
        
        // �B�z��f���������O
        if(uart_flag) {
            ProcessUARTCommand();
            uart_flag = 0;
        }
    }
}

// ��f��l�� (9600 baud @ 11.0592MHz)
void UART_Init(void) {
    TMOD |= 0x20;   // �w�ɾ�1�Ҧ�2
    TH1 = 0xFD;     // 9600�i�S�v
    TL1 = 0xFD;
    TR1 = 1;        // �Ұʩw�ɾ�1
    SCON = 0x50;    // �Ҧ�1, ���\����
    ES = 1;         // �}�Ҧ�f���_
}

// �o�e��Ӧr��
void UART_SendChar(unsigned char dat) {
    SBUF = dat;
    while(!TI);
    TI = 0;
}

// �o�e�r��
void UART_SendStr(unsigned char *s) {
    while(*s != '\0') {
        UART_SendChar(*s);
        s++;
    }
}

// �o�e����ƥ�
void SendKeyEvent(unsigned char key) {
    UART_SendStr("KEY:");
    UART_SendChar('0' + key);  // �����o�e�Ʀr�r��
    UART_SendStr("\r\n");
}

// �B�z��f���������O
void ProcessUARTCommand(void) {
    unsigned char i, display_num;
    
    // �ˬd�O�_����ܫ��O: "DISPLAY:X"
    if(uart_index >= 9 && 
       uart_buffer[0] == 'D' && uart_buffer[1] == 'I' && 
       uart_buffer[2] == 'S' && uart_buffer[3] == 'P' && 
       uart_buffer[4] == 'L' && uart_buffer[5] == 'A' && 
       uart_buffer[6] == 'Y' && uart_buffer[7] == ':') {
        
        // ���o�n��ܪ��Ʀr
        if(uart_buffer[8] >= '0' && uart_buffer[8] <= '9') {
            display_num = uart_buffer[8] - '0';
            UpdateDisplay(display_num);
        }
    }
    
    // �M�Žw�İ�
    uart_index = 0;
    for(i=0; i<20; i++) {
        uart_buffer[i] = 0;
    }
}

// ��s��ܽw�İ�
void UpdateDisplay(unsigned char num) {
    unsigned char j;
    
    // �N�s�Ʀr��ܦb�̥k�ݡA��L�쥪��
    for(j=0; j<7; j++) {
        TempData[j] = TempData[j+1];
    }
    TempData[7] = dofly_DuanMa[num];
}

// ��f���_�A�ȵ{��
void UART_ISR(void) interrupt 4 {
    if(RI) {
        RI = 0;
        
        // �����r���æs�J�w�İ�
        if(uart_index < 19) {
            uart_buffer[uart_index] = SBUF;
            
            // �ˬd�O�_���촫��Ÿ��A��ܫ��O����
            if(SBUF == '\n' || SBUF == '\r') {
                uart_flag = 1;  // �]�w�B�z�X��
            } else {
                uart_index++;
            }
        } else {
            // �w�İϺ��A���m
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

// �x�}��L����M�g
unsigned char KeyPro(void) {
    switch(KeyScan()) {
        case 0x7e: return 1;  // �Ĥ@��Ĥ@�C
        case 0x7d: return 4;  // �Ĥ@��ĤG�C
        case 0x7b: return 7;  // �Ĥ@��ĤT�C
        case 0xbe: return 2;  // �ĤG��Ĥ@�C
        case 0xbd: return 5;  // �ĤG��ĤG�C
        case 0xbb: return 8;  // �ĤG��ĤT�C
        case 0xb7: return 0;  // �ĤG��ĥ|�C
        case 0xde: return 3;  // �ĤT��Ĥ@�C
        case 0xdd: return 6;  // �ĤT��ĤG�C
        case 0xdb: return 9;  // �ĤT��ĤT�C
        default:   return 0xff; // �L����εL�ī���
    }
}
