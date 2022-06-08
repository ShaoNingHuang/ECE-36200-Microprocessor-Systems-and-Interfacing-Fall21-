#include "stm32f0xx.h"
#include <string.h> // for memset() declaration
#include <math.h>   // for MA_PI

// Be sure to change this to your login...
const char login[] = "huan1645";

void nano_wait(unsigned int);
const char font[];


//===========================================================================
// 2.1 FROM LAB 8
// ..........................................................................
// Debouncing a Keypad
//===========================================================================
const char keymap[] = "DCBA#9630852*741";
uint8_t hist[16];
uint8_t col;
char queue[2];
int qin;
int qout;
uint16_t msg[8] = { 0x0000,0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700 };
void drive_column(int);
int read_rows();

void push_queue(int n)
{
    n = (n & 0xff) | 0x80;
    queue[qin] = n;
    qin ^= 1;

}

uint8_t pop_queue()
{
    uint8_t tmp = queue[qout] & 0x7f;
    queue[qout] = 0;
    qout ^= 1;
    return tmp;
}

void update_history(int c, int rows)
{
    for(int i = 0; i < 4; i++)
    {
            hist[4*c+i] = (hist[4*c+i]<<1) + ((rows>>i)&1);
            if (hist[4*c+i] == 1)
              push_queue(4*c+i);
    }
}

void init_tim7()
{
    RCC->APB1ENR|=RCC_APB1ENR_TIM7EN;
    TIM7->PSC=1000-1;
    TIM7->ARR=48-1;
    TIM7->DIER |= TIM_DIER_UIE;
    TIM7->CR1 |= TIM_CR1_CEN;
    NVIC->ISER[0] |= 1 << TIM7_IRQn;
}
int msg_index=0;
void TIM7_IRQHandler(void)
{
    TIM7->SR &= ~TIM_SR_UIF;
    int rows = read_rows();
    update_history(col, rows);
    col = (col + 1) & 3;
    drive_column(col);

}

char get_keypress()
{
    for(;;)
     {
        asm volatile ("wfi" : :);   // wait for an interrupt
        if (queue[qout] == 0)
            continue;
        return keymap[pop_queue()];
     }
}


//===========================================================================
// SPI DMA LED Array
//===========================================================================

void init_spi2(void)
{
    RCC->AHBENR|=RCC_AHBENR_GPIOBEN;
    GPIOB->MODER&=~0xcf000000;
    GPIOB->MODER|=0x8a000000;
    RCC->APB1ENR|=RCC_APB1ENR_SPI2EN;
    SPI2->CR1&=~SPI_CR1_SPE;
    SPI2->CR1|=SPI_CR1_BR_0|SPI_CR1_BR_1|SPI_CR1_BR_2;
    SPI2->CR2=SPI_CR2_DS_0|SPI_CR2_DS_1|SPI_CR2_DS_2|SPI_CR2_DS_3;
    SPI2->CR1|=SPI_CR1_MSTR;
    SPI2->CR2|=SPI_CR2_SSOE|SPI_CR2_NSSP;
    SPI2->CR2|=SPI_CR2_TXDMAEN;
    SPI2->CR1|=SPI_CR1_SPE;
}

void setup_spi2_dma(void)
{
    RCC->AHBENR|=RCC_AHBENR_DMA1EN;
    DMA1_Channel5->CCR&=~DMA_CCR_EN;
    DMA1_Channel5->CPAR=(uint32_t) (&(SPI2->DR));
    DMA1_Channel5->CMAR=(uint32_t) msg;
    DMA1_Channel5->CNDTR=8;
    DMA1_Channel5->CCR|=DMA_CCR_DIR;
    DMA1_Channel5->CCR|=DMA_CCR_MINC;
    DMA1_Channel5->CCR|=DMA_CCR_MSIZE_0;
    DMA1_Channel5->CCR|=DMA_CCR_PSIZE_0;
    DMA1_Channel5->CCR|=DMA_CCR_CIRC;
}

void enable_spi2_dma(void)
{
    DMA1_Channel5->CCR|=DMA_CCR_EN;
}

void append_display(char val)
{
    int i=0;
    for(i=0;i<7;i++)
    {
        msg[i]=(msg[i]&~0x00ff)+(msg[i+1]&0x00ff);
    }
        msg[7]=(msg[7]&~0xff)+(val&0xff);
}


//===========================================================================
// THIS LAB (9)
// ..........................................................................
// 2.2 Initialize I2C
//===========================================================================
#define GPIOEX_ADDR 0x20  // ENTER GPIO EXPANDER I2C ADDRESS HERE
#define EEPROM_ADDR 0x50  // ENTER EEPROM I2C ADDRESS HERE
void init_i2c(void)
{
    RCC->AHBENR|=RCC_AHBENR_GPIOBEN;
    RCC->APB1ENR|=RCC_APB1ENR_I2C1EN;
    GPIOB->MODER&=~0xf000;
    GPIOB->MODER|=0xa000;
    GPIOB->AFR[0]|=1<<(4*6)|1<<(4*7);
    I2C1->CR1&=~I2C_CR1_PE;
    I2C1->CR1&=~I2C_CR1_ANFOFF;
    I2C1->CR1&=~I2C_CR1_ERRIE;
    I2C1->CR1&=~I2C_CR1_NOSTRETCH;

    I2C1->TIMINGR&=~I2C_TIMINGR_PRESC;
    I2C1->TIMINGR|=0<<28;
    I2C1->TIMINGR|=3<<20;
    I2C1->TIMINGR|=1<<16;
    I2C1->TIMINGR|=3<<8;
    I2C1->TIMINGR|=9<<0;

    I2C1->OAR1&=~I2C_OAR1_OA1EN;
    I2C1->OAR2&=~I2C_OAR2_OA2EN;

    I2C1->CR2&=~I2C_CR2_ADD10;
    I2C1->CR2|=I2C_CR2_AUTOEND;

    I2C1->CR1|=I2C_CR1_PE;
}


//===========================================================================
// 2.3 I2C helpers
//===========================================================================

void i2c_waitidle(void)
{
    while((I2C1->ISR&I2C_ISR_BUSY)==I2C_ISR_BUSY);
}

void i2c_start(uint32_t devaddr, uint8_t size, uint8_t dir)
{
    uint32_t tmpreg=I2C1->CR2;
    tmpreg&=~(I2C_CR2_SADD|I2C_CR2_NBYTES|I2C_CR2_RELOAD|I2C_CR2_AUTOEND|I2C_CR2_RD_WRN|I2C_CR2_START|I2C_CR2_STOP);
    if (dir==1)
     {
        tmpreg|=I2C_CR2_RD_WRN;
     }
    else
    {
        tmpreg&=~I2C_CR2_RD_WRN;
    }
        tmpreg|=((devaddr<<1)&I2C_CR2_SADD)|((size<<16)&I2C_CR2_NBYTES);
        tmpreg|=I2C_CR2_START;
        I2C1->CR2=tmpreg;

}

void i2c_stop(void)
{
    if(I2C1->ISR&I2C_ISR_STOPF)
        return;
    I2C1->CR2|=I2C_CR2_STOP;

    while((I2C1->ISR&I2C_ISR_STOPF)==0);
    I2C1->ICR|=I2C_ICR_STOPCF;
}

int i2c_checknack(void)
{
   if(I2C1->ISR&I2C_ISR_NACKF)
       return 1;
   else
       return 0;

}

void i2c_clearnack(void)
{
    I2C1->ICR|=I2C_ICR_NACKCF;
}

int i2c_senddata(uint8_t devaddr, const void *data, uint8_t size)
{
   int i;
   if(size<=0||data==0) return -1;
   uint8_t *udata=(uint8_t*)data;
   i2c_waitidle();
   i2c_start(devaddr, size,0);
   for(i=0;i<size;i++)
   {
       int count=0;
       while((I2C1->ISR&I2C_ISR_TXIS)==0)
       {
           count++;
           if(count>1000000) return -1;
           if(i2c_checknack())
           {
               i2c_clearnack();
               i2c_stop();
               return -1;
           }
       }
       I2C1->TXDR=udata[i]&I2C_TXDR_TXDATA;
   }
       while((I2C1->ISR&I2C_ISR_TC)==0&&(I2C1->ISR&I2C_ISR_NACKF)==0);
   if((I2C1->ISR&I2C_ISR_NACKF)!=0)
       return -1;
   i2c_stop();
   return 0;

}

int i2c_recvdata(uint8_t devaddr, void *data, uint8_t size)
{
    int i;
    if(size<=0||data==0) return -1;
    uint8_t*udata=(uint8_t*)data;
    i2c_waitidle();
    i2c_start(devaddr,size,1);
    for(i=0;i<size;i++)
    {
        int count=0;
        while((I2C1->ISR&I2C_ISR_RXNE)==0)
        {
            count++;
            if(count>1000000) return -1;
            if(i2c_checknack())
            {
                i2c_clearnack();
                i2c_stop();
                return -1;
            }
        }
        udata[i]=I2C1->RXDR;
    }
    while((I2C1->ISR&I2C_ISR_TC)==0&&(I2C1->ISR&I2C_ISR_NACKF)==0);
    if((I2C1->ISR&I2C_ISR_NACKF)!=0)
        return -1;
    i2c_stop();
    return 0;
}


//===========================================================================
// 2.5 GPIO Expander
//===========================================================================
void gpio_write(uint8_t reg, uint8_t val)
{
   uint8_t data[2]={reg,val};
   i2c_senddata(GPIOEX_ADDR,data,2);
}

uint8_t gpio_read(uint8_t reg)
{
  uint8_t data[1]={reg};
  i2c_senddata(GPIOEX_ADDR,data,1);
  i2c_recvdata(GPIOEX_ADDR,data,1);
  return data[0];
}
void init_expander(void)
{
    gpio_write(0x00,0xf0);
    gpio_write(0x06,0xf0);
    gpio_write(0x01,0xf0);
}

void drive_column(int c)
{
    gpio_write(10, ~(1 << (3 - c)) );
}

int read_rows()
{
    uint8_t data = gpio_read(9);
    data &= 0xf0;
    for (int i = 0; i < 4; i++) {
        uint8_t bit = data & (1 << (4 + i));
        bit >>= (4 + i);
        bit <<= (3 - i);
        data |= bit;
    }
    return data & 0xf;
}

//===========================================================================
// 2.5 EEPROM functions
//===========================================================================
void eeprom_write(uint16_t loc, const char* data, uint8_t len)
{
    int i;
    uint8_t array[34];
    array[0]=(loc&0xf00)>>8;
    array[1]=(loc&0x0ff);
    for(i=0;i<len;i++)
    {
        array[i+2]=data[i];
    }
    i2c_senddata(EEPROM_ADDR,array,len+2);
}

int eeprom_write_complete(void)
{
     i2c_waitidle();
     i2c_start(EEPROM_ADDR,0,0);
     int x=0;
     while((I2C1->ISR&I2C_ISR_NACKF)==0&&(I2C1->ISR&I2C_ISR_TC)==0)
         x++;
     if(I2C1->ISR&I2C_ISR_NACKF)
     {
         I2C1->ICR|=I2C_ICR_NACKCF;
         i2c_stop();
         return 0;
     }
     else
     {
         i2c_stop();
         return 1;
     }
}

void eeprom_read(uint16_t loc, char data[], uint8_t len) {
    TIM7->CR1 &= ~TIM_CR1_CEN; // Pause keypad scanning.

    uint8_t array[2];
    array[0]=(loc&=0xf00)>>8;
    array[1]=loc&0x0ff;
    i2c_senddata(EEPROM_ADDR,array,2);
    i2c_recvdata(EEPROM_ADDR,data, len);
    TIM7->CR1 |= TIM_CR1_CEN; // Resume keypad scanning.
}

//===========================================================================
// Main and supporting functions
//===========================================================================
void nano_wait(unsigned int n) {
    asm(    "        mov r0,%0\n"
            "repeat: sub r0,#83\n"
            "        bgt repeat\n" : : "r"(n) : "r0", "cc");
}

void eeprom_blocking_write(uint16_t loc, const char* data, uint8_t len) {
    TIM7->CR1 &= ~TIM_CR1_CEN; // Pause keypad scanning.
    eeprom_write(loc, data, len);
    while (!eeprom_write_complete());
    TIM7->CR1 |= TIM_CR1_CEN; // Resume keypad scanning.
}

void clear_display(void) {
    for (int i=0; i < 8; i++) {
        msg[i] &= ~0xff;
    }
}

int main(void)
{
    msg[0] |= font['E'];
    msg[1] |= font['C'];
    msg[2] |= font['E'];
    msg[3] |= font[' '];
    msg[4] |= font['3'];
    msg[5] |= font['6'];
    msg[6] |= font['2'];
    msg[7] |= font[' '];


    // LED array SPI
    setup_spi2_dma();
    enable_spi2_dma();
    init_spi2();

    // This LAB

    // 2.2 Initialize I2C
    init_i2c();

    // 2.3 Example code for testing
//   for(;;)
//   {
//        i2c_waitidle();
//        i2c_start(GPIOEX_ADDR,0,0);
//        i2c_clearnack();
//       i2c_stop();
//    }
//    for(;;)
//    {
//        int tmp;
//        uint8_t data[2]={0x00,0x01};
//        i2c_senddata(0x20,data,2);
//        i2c_senddata(0x20,data,1);
//       i2c_recvdata(0x20,&tmp,1);
//   }
      //2.5 Expander setup
    init_expander();
    init_tim7();

    // 2.6 Interface for reading/writing memory.
    serial_ui();

    for(;;) {
        uint8_t key = get_keypress();
        append_display(font[key]);
    }
}

const char font[] = {
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x00, // 32: space
    0x86, // 33: exclamation
    0x22, // 34: double quote
    0x76, // 35: octothorpe
    0x00, // dollar
    0x00, // percent
    0x00, // ampersand
    0x20, // 39: single quote
    0x39, // 40: open paren
    0x0f, // 41: close paren
    0x49, // 42: asterisk
    0x00, // plus
    0x10, // 44: comma
    0x40, // 45: minus
    0x80, // 46: period
    0x00, // slash
    // digits
    0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x67,
    // seven unknown
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    // Uppercase
    0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71, 0x6f, 0x76, 0x30, 0x1e, 0x00, 0x38, 0x00,
    0x37, 0x3f, 0x73, 0x7b, 0x31, 0x6d, 0x78, 0x3e, 0x00, 0x00, 0x00, 0x6e, 0x00,
    0x39, // 91: open square bracket
    0x00, // backslash
    0x0f, // 93: close square bracket
    0x00, // circumflex
    0x08, // 95: underscore
    0x20, // 96: backquote
    // Lowercase
    0x5f, 0x7c, 0x58, 0x5e, 0x79, 0x71, 0x6f, 0x74, 0x10, 0x0e, 0x00, 0x30, 0x00,
    0x54, 0x5c, 0x73, 0x7b, 0x50, 0x6d, 0x78, 0x1c, 0x00, 0x00, 0x00, 0x6e, 0x00
};
