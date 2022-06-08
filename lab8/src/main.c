#include "stm32f0xx.h"
#include <stdio.h>
// Be sure to change this to your login...
const char login[] = "huan1645";

void nano_wait(unsigned int n) {
    asm(    "        mov r0,%0\n"
            "repeat: sub r0,#83\n"
            "        bgt repeat\n" : : "r"(n) : "r0", "cc");
}


//===========================================================================
// 2.1 FROM LAB 6
// ..........................................................................
// Configuring GPIO
//===========================================================================
void enable_ports(void)
{
    RCC->AHBENR|=RCC_AHBENR_GPIOAEN;
    RCC->AHBENR|=RCC_AHBENR_GPIOBEN;
    RCC->AHBENR|=RCC_AHBENR_GPIOCEN;
    GPIOB->MODER&=~0x3fffff;
    GPIOB->MODER|=0x155555;
    GPIOC->MODER&=~0xffff;
    GPIOC->MODER|=0x5500;
    GPIOC->PUPDR&=~0xff;
    GPIOC->PUPDR|=0xaa;
}

//===========================================================================
// Debouncing a Keypad
//===========================================================================
const char keymap[] = "DCBA#9630852*741";
uint8_t hist[16];
uint8_t col;
char queue[2];
int qin;
int qout;

void drive_column(int c)
{
    GPIOC->BSRR = 0xf00000 | (1 << (c + 4));
}

int read_rows()
{
    return GPIOC->IDR & 0xf;
}

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

void init_tim6()
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
    TIM6->PSC = 1000 - 1;
    TIM6->ARR = 48 - 1;
    TIM6->DIER |= TIM_DIER_UIE;
    TIM6->CR1 |= TIM_CR1_CEN;
    NVIC->ISER[0] |= 1 << TIM6_DAC_IRQn;
}

void TIM6_DAC_IRQHandler(void)
{
    TIM6->SR &= ~TIM_SR_UIF;
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
uint16_t msg[8] = { 0x0000,0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700 };
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
// This Lab 7
// ..........................................................................
// 2.2 Bit Bang SPI LED Array
//===========================================================================
int msg_index = 0;
extern const char font[];

void setup_bb(void)
{
    GPIOB->MODER&=~0xcf000000;
    GPIOB->MODER|=0x45000000;
    GPIOB->BSRR=0x1000;
    GPIOB->BRR=0x2000;
}

void small_delay(void)
{
    //nano_wait(50000000);
}

void bb_write_bit(int val)
{
  if(val==0)
  {
      GPIOB->BRR=0x8000;
  }
  else
  {
      GPIOB->BSRR=0x8000;
  }
  small_delay();
  GPIOB->BSRR=0x2000;
  small_delay();
  GPIOB->BRR=0x2000;
}

void bb_write_halfword(int halfword)
{
    GPIOB->BRR=0x1000;
    for(int i=15;i>=0;i--)
    {
        bb_write_bit((halfword>>i)&1);
    }
    GPIOB->BSRR=0x1000;
}

void init_tim7(void)
{
    RCC->APB1ENR|=RCC_APB1ENR_TIM7EN;
    TIM7->PSC=1000-1;
    TIM7->ARR=48-1;
    TIM7->DIER |= TIM_DIER_UIE;
    TIM7->CR1 |= TIM_CR1_CEN;
    NVIC->ISER[0] |= 1 << TIM7_IRQn;
}

void TIM7_IRQHandler(void)
{
    TIM7->SR &= ~TIM_SR_UIF;
    bb_write_halfword(msg[msg_index]);
    msg_index++;
    msg_index=msg_index%8;
}
//===========================================================================
// 2.3 SPI DMA LED Array
//===========================================================================
void init_spi2(void)
{
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

void setup_dma(void)
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

void enable_dma(void)
{
    DMA1_Channel5->CCR|=DMA_CCR_EN;
}

//===========================================================================
// 2.4 SPI OLED Display
//===========================================================================
void setup_spi1()
{

    GPIOA->MODER&=~0xc000fc00;
    GPIOA->MODER|=0x8000a800;
    RCC->APB2ENR|=RCC_APB2ENR_SPI1EN;
    SPI1->CR1&=~SPI_CR1_SPE;
    SPI1->CR1|=SPI_CR1_BR_0|SPI_CR1_BR_1|SPI_CR1_BR_2;
    SPI1->CR2=SPI_CR2_DS_0|SPI_CR2_DS_3;
    SPI1->CR2|=SPI_CR2_SSOE|SPI_CR2_NSSP;
    SPI1->CR1|=SPI_CR1_MSTR;
    SPI1->CR1|=SPI_CR1_SPE;
}

void spi_cmd(unsigned int data)
{
    while(!(SPI1->SR&SPI_SR_TXE));
    SPI1->DR=data;
}

void spi_data(unsigned int data)
{
    while(!(SPI1->SR&SPI_SR_TXE));
    SPI1->DR=data|0x200;
}

void spi_init_oled()
{
    nano_wait(1000000);
    spi_cmd(0x38);
    spi_cmd(0x08);
    spi_cmd(0x01);
    nano_wait(2000000);
    spi_cmd(0x06);
    spi_cmd(0x02);
    spi_cmd(0x0c);
}

void spi_display1(const char *string)
{
    int i=0;
    spi_cmd(0x02);
    while(string[i]!='\0')
    {
           spi_data(string[i]);
           i++;
    }
}

void spi_display2(const char *string)
{
    int i=0;
    spi_cmd(0xc0);
    while(string[i]!='\0')
    {
        spi_data(string[i]);
        i++;
    }
}
//===========================================================================
// Main and supporting functions
//===========================================================================

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

    // GPIO enable
    enable_ports();

    // setup keypad
    init_tim6();

    // 2.2 LED array Bit Bang
    setup_bb();
    init_tim7();

    // 2.3 LED array SPI
     //setup_dma();
     //enable_dma();
     //init_spi2();

    // 2.4 SPI OLED
     //setup_spi1();
     //spi_init_oled();
    //spi_display1("Hello again,");
     //spi_display2(login);

    int key = 0;
    for(;;) {
        char key = get_keypress();
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
