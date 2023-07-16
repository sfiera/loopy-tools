#include "loopy.h"

#define SERIAL_BUFFER_SIZE 64

volatile char serialBuffer[SERIAL_BUFFER_SIZE];
volatile int serialBufferWrite = 0;
volatile int serialBufferRead = 0;

void getSerialDivisor(int baud, int* div, int* cks) {
    // Sets divisor N (0..255) and clock source n (0..3)
    // where output rate is B = F_CPU / (32 x 4^n x (N+1))
    // Supports standard rates up to 38400baud at F_CPU=16M
    int _div, _cks;
    if(baud > 2000) {
        _cks = 0;
        _div = (((F_CPU / (baud*16))+1) / 2) - 1;
    } else {
        _cks = 3;
        _div = (((F_CPU / (baud*64*16))+1) / 2) - 1;
    }
    if(_div < 0) _div = 0;
    if(_div > 255) _div = 255;
    *div = _div;
    *cks = _cks;
}

void serialBegin(int baud) {
    // Set up for 8N1 full duplex
    int div, cks;
    getSerialDivisor(baud, &div, &cks);
    PBCR1 = (PBCR1 & 0xFFF0) | 0x000A;
    SMR0 = 0x00 | cks;
    BRR0 = div;
    SCR0 = 0x70; // Enable transmit, receive, receive interrupts
    SSR0; SSR0 = 0x80; // Clear all flags except TDRE
    // Set nonzero interrupt priority for SCI0
    IPRD = (IPRD & 0xFFF0) | 0x000F;
}
void midiBegin() {
    // Set up for 8N1 transmit only
    int div, cks;
    getSerialDivisor(31250, &div, &cks);
    PBCR1 = (PBCR1 & 0xFF0F) | 0x00A0;
    SMR1 = 0x00 | cks;
    BRR1 = div;
    SCR1 = 0x20; // Enable transmit only
    SSR1; SSR1 = 0x80; // Clear all flags except TDRE
}

void serialWrite(char c) {
    while(!(SSR0 & 0x80)); // Wait for serial ready
    TDR0 = c; // Set serial data
    SSR0 &= 0x7F; // Send data
}
void midiWrite(char c) {
    while(!(SSR1 & 0x80)); // Wait for serial ready
    TDR1 = c; // Set serial data
    SSR1 &= 0x7F; // Send data
}

void serialPrint(char *s) {
    while(*s) serialWrite(*s++);
}
void midiPrint(char *s) {
    while(*s) midiWrite(*s++);
}

int serialAvailable() {
    return (SERIAL_BUFFER_SIZE + serialBufferWrite - serialBufferRead) % SERIAL_BUFFER_SIZE;
}

char serialRead() {
    if(serialBufferWrite == serialBufferRead) return 0;
    char c = serialBuffer[serialBufferRead];
    serialBufferRead = (serialBufferRead+1) % SERIAL_BUFFER_SIZE;
    return c;
}

__attribute__((interrupt_handler))
__attribute__((section(".smallfunc")))
void serial_RxI0() {
    char c = (char)RDR0;
    SSR0; SSR0 &= 0xBF; // Clear RDRF flag
    int serialBufferWriteNew = (serialBufferWrite+1) % SERIAL_BUFFER_SIZE;
    if(serialBufferWriteNew != serialBufferRead) {
        serialBuffer[serialBufferWrite] = c;
        serialBufferWrite = serialBufferWriteNew;
    }
}

__attribute__((interrupt_handler))
__attribute__((section(".smallfunc")))
void serial_ERI0() {
    SSR0; SSR0 &= 0xC7; // Clear error flags
}
