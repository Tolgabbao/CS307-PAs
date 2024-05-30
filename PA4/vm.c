#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "vm_dbg.h"

#define NOPS (16)

#define OPC(i) ((i)>>12)
#define DR(i) (((i)>>9)&0x7)
#define SR1(i) (((i)>>6)&0x7)
#define SR2(i) ((i)&0x7)
#define FIMM(i) ((i>>5)&01)
#define IMM(i) ((i)&0x1F)
#define SEXTIMM(i) sext(IMM(i),5)
#define FCND(i) (((i)>>9)&0x7)
#define POFF(i) sext((i)&0x3F, 6)
#define POFF9(i) sext((i)&0x1FF, 9)
#define POFF11(i) sext((i)&0x7FF, 11)
#define FL(i) (((i)>>11)&1)
#define BR(i) (((i)>>6)&0x7)
#define TRP(i) ((i)&0xFF)   

//New OS declarations
//  OS Bookkeeping constants 
#define OS_MEM_SIZE 4096    // OS Region size. At the same time, constant free-list starting header 

#define Cur_Proc_ID 0       // id of the current process
#define Proc_Count 1        // total number of processes, including ones that finished executing.
#define OS_STATUS 2         // Bit 0 shows whether the PCB list is full or not

//  Process list and PCB related constants
#define PCB_SIZE 6  // Number of fields in a PCB

#define PID_PCB 0   // holds the pid for a process
#define PC_PCB 1    // value of the program counter for the process
#define BSC_PCB 2   // base value of code section for the process
#define BDC_PCB 3   // bound value of code section for the process
#define BSH_PCB 4   // value of heap section for the process
#define BDH_PCB 5   // holds the bound value of heap section for the process

#define CODE_SIZE 4096
#define HEAP_INIT_SIZE 4096
//New OS declarations


bool running = true;

typedef void (*op_ex_f)(uint16_t i);
typedef void (*trp_ex_f)();

enum { trp_offset = 0x20 };
enum regist { R0 = 0, R1, R2, R3, R4, R5, R6, R7, RPC, RCND, RBSC, RBDC, RBSH, RBDH, RCNT };
enum flags { FP = 1 << 0, FZ = 1 << 1, FN = 1 << 2 };

uint16_t mem[UINT16_MAX] = {0};
uint16_t reg[RCNT] = {0};
uint16_t PC_START = 0x3000;

void initOS();
int createProc(char *fname, char* hname);
void loadProc(uint16_t pid);
static inline void tyld();
uint16_t allocMem(uint16_t size);
int freeMem(uint16_t ptr);
static inline uint16_t mr(uint16_t address);
static inline void mw(uint16_t address, uint16_t val);
static inline void tbrk();
static inline void thalt();
static inline void trap(uint16_t i);

static inline uint16_t sext(uint16_t n, int b) { return ((n>>(b-1))&1) ? (n|(0xFFFF << b)) : n; }
static inline void uf(enum regist r) {
    if (reg[r]==0) reg[RCND] = FZ;
    else if (reg[r]>>15) reg[RCND] = FN;
    else reg[RCND] = FP;
}
static inline void add(uint16_t i)  { reg[DR(i)] = reg[SR1(i)] + (FIMM(i) ? SEXTIMM(i) : reg[SR2(i)]); uf(DR(i)); }
static inline void and(uint16_t i)  { reg[DR(i)] = reg[SR1(i)] & (FIMM(i) ? SEXTIMM(i) : reg[SR2(i)]); uf(DR(i)); }
static inline void ldi(uint16_t i)  { reg[DR(i)] = mr(mr(reg[RPC]+POFF9(i))); uf(DR(i)); }
static inline void not(uint16_t i)  { reg[DR(i)]=~reg[SR1(i)]; uf(DR(i)); }
static inline void br(uint16_t i)   { if (reg[RCND] & FCND(i)) { reg[RPC] += POFF9(i); } }
static inline void jsr(uint16_t i)  { reg[R7] = reg[RPC]; reg[RPC] = (FL(i)) ? reg[RPC] + POFF11(i) : reg[BR(i)]; }
static inline void jmp(uint16_t i)  { reg[RPC] = reg[BR(i)]; }
static inline void ld(uint16_t i)   { reg[DR(i)] = mr(reg[RPC] + POFF9(i)); uf(DR(i)); }
static inline void ldr(uint16_t i)  { reg[DR(i)] = mr(reg[SR1(i)] + POFF(i)); uf(DR(i)); }
static inline void lea(uint16_t i)  { reg[DR(i)] =reg[RPC] + POFF9(i); uf(DR(i)); }
static inline void st(uint16_t i)   { mw(reg[RPC] + POFF9(i), reg[DR(i)]); }
static inline void sti(uint16_t i)  { mw(mr(reg[RPC] + POFF9(i)), reg[DR(i)]); }
static inline void str(uint16_t i)  { mw(reg[SR1(i)] + POFF(i), reg[DR(i)]); }
static inline void rti(uint16_t i) {} // unused
static inline void res(uint16_t i) {} // unused
static inline void tgetc() { reg[R0] = getchar(); }
static inline void tout() { fprintf(stdout, "%c", (char)reg[R0]); }
static inline void tputs() {
    uint16_t *p = mem + reg[R0];
    while(*p) {
        fprintf(stdout, "%c", (char)*p);
        p++;
    }
}
static inline void tin() { reg[R0] = getchar(); fprintf(stdout, "%c", reg[R0]); }
static inline void tputsp() { /* Not Implemented */ }

static inline void tinu16() { fscanf(stdin, "%hu", &reg[R0]); }
static inline void toutu16() { fprintf(stdout, "%hu\n", reg[R0]); }


trp_ex_f trp_ex[10] = { tgetc, tout, tputs, tin, tputsp, thalt, tinu16, toutu16, tyld, tbrk };
static inline void trap(uint16_t i) { trp_ex[TRP(i)-trp_offset](); }
op_ex_f op_ex[NOPS] = { /*0*/ br, add, ld, st, jsr, and, ldr, str, rti, not, ldi, sti, jmp, res, lea, trap };

void ld_img(char *fname, uint16_t offset, uint16_t size) {
    FILE *in = fopen(fname, "rb");
    if (NULL==in) {
        fprintf(stderr, "Cannot open file %s.\n", fname);
        exit(1);    
    }
    uint16_t *p = mem + offset;
    fread(p, sizeof(uint16_t), (size), in);
    fclose(in);
}

void run(char* code, char* heap) {

    while(running) {
        uint16_t i = mr(reg[RPC]++);
        op_ex[OPC(i)](i);
    }
}


// YOUR CODE STARTS HERE

void initOS() {
    mem[Cur_Proc_ID] = 0xffff;
    mem[Proc_Count] = 0;
    mem[OS_STATUS] = 0x0000;
}

// process functions to implement
int createProc(char *fname, char* hname) {
    // Check if OS memory is full
    if (mem[OS_STATUS] & 0x0001) {
        fprintf(stderr, "The OS memory region is full. Cannot create a new PCB.\n");
        return 0;
    }

    // Get the next PID
    uint16_t pid = mem[Proc_Count];
    mem[Proc_Count]++;

    // Allocate space for the PCB
    uint16_t pcb_addr = (Proc_Count - 1) * PCB_SIZE + OS_MEM_SIZE;

    // Set PID in PCB
    mem[pcb_addr + PID_PCB] = pid;

    // Set PC in PCB
    mem[pcb_addr + PC_PCB] = PC_START;

    // Allocate space for the code segment
    uint16_t code_base = allocMem(CODE_SIZE);
    if (code_base == 0) {
        fprintf(stderr, "Cannot create code segment.\n");
        return 0;
    }

    // Load code segment
    ld_img(fname, code_base, CODE_SIZE);

    // Set code segment base and bound in PCB
    mem[pcb_addr + BSC_PCB] = code_base;
    mem[pcb_addr + BDC_PCB] = CODE_SIZE;

    // Allocate space for the heap segment
    uint16_t heap_base = allocMem(HEAP_INIT_SIZE);
    if (heap_base == 0) {
        fprintf(stderr, "Cannot create heap segment.\n");
        return 0;
    }

    // Set heap segment base and bound in PCB
    mem[pcb_addr + BSH_PCB] = heap_base;
    mem[pcb_addr + BDH_PCB] = HEAP_INIT_SIZE;

    return 1;
}

void loadProc(uint16_t pid) {
    // Calculate PCB address
    uint16_t pcb_addr = (pid) * PCB_SIZE + OS_MEM_SIZE;

    // Load PC, base, and bound values from PCB
    reg[RPC] = mem[pcb_addr + PC_PCB];
    reg[RBSC] = mem[pcb_addr + BSC_PCB];
    reg[RBDC] = mem[pcb_addr + BDC_PCB];
    reg[RBSH] = mem[pcb_addr + BSH_PCB];
    reg[RBDH] = mem[pcb_addr + BDH_PCB];

    // Set current process ID
    mem[Cur_Proc_ID] = pid;
}

int freeMem (uint16_t ptr) {
    // Check if ptr is in OS region or not the beginning of an allocated region
    if (ptr < OS_MEM_SIZE || mem[ptr - 2] != 42) {
        return 0;
    }

    // Get the size of the chunk
    uint16_t size = mem[ptr - 2];

    // Coalescing
    // Check if the previous chunk is free
    if (ptr > OS_MEM_SIZE && mem[ptr - 4] == 0) {
        // Coalesce with the previous chunk
        size += mem[ptr - 4] + 2; // Add the size of the previous chunk and its header
        ptr -= mem[ptr - 4] + 2; // Update ptr to the beginning of the merged chunk
    }

    // Check if the next chunk is free
    if (mem[ptr + size] == 0) {
        // Coalesce with the next chunk
        size += mem[ptr + size] + 2; // Add the size of the next chunk and its header
    }

    // Insert the freed chunk into the free list
    // Find the correct position in the free list
    uint16_t current = OS_MEM_SIZE;
    uint16_t previous = 0; // Keep track of the previous free chunk
    while (mem[current] != 0 && mem[current] < ptr) {
        previous = current; // Update previous free chunk
        current = mem[current];
    }

    // Insert the freed chunk
    mem[ptr - 2] = size;
    mem[ptr - 1] = current;
    if (previous != 0) { // If not the first free chunk
        mem[previous] = ptr - 2;
    } else {
        mem[OS_MEM_SIZE] = ptr - 2; // Update the first free chunk
    }

    // Handle the case where the freed chunk is at the end of the list
    if (mem[ptr + size] == 0) {
        if (previous != 0) {
            mem[previous] = 0; // Set the next pointer of the previous free chunk to 0
        } else {
            mem[OS_MEM_SIZE] = 0; // Set the first free chunk's next pointer to 0
        }
    }

    return 1;
}

uint16_t allocMem (uint16_t size) {
    // Iterate through the free list
    uint16_t current = OS_MEM_SIZE;
    while (mem[current] != 0) {
        // Check if the current free chunk has enough space
        if (mem[current] + 2 >= size + 2) {
            // Allocate space from the end of the free chunk
            uint16_t alloc_ptr = mem[current] + size + 2;
            uint16_t remaining_size = mem[current] - size - 2;

            // Update the free list (this is the key change)
            mem[current] = alloc_ptr; // Update the next pointer of the previous free chunk
            mem[alloc_ptr - 2] = size; // Set the size of the allocated chunk
            mem[alloc_ptr - 1] = 42; // Set the magic number for allocated chunk

            // Update the remaining free chunk
            if (remaining_size > 0) {
                mem[alloc_ptr + size] = remaining_size; // Set the size of the remaining free chunk
                mem[alloc_ptr + size + 1] = mem[alloc_ptr - 1]; // Set the next pointer of the remaining free chunk
            } else {
                mem[alloc_ptr + size] = 0; // Set the next pointer to 0 if no remaining free chunk
            }

            return alloc_ptr;
        }

        current = mem[current];
    }

    return 0; // No free chunk found
}

// instructions to implement
static inline void tbrk() {
    uint16_t new_heap_size = reg[R0];
    uint16_t current_heap_size = reg[RBDH];
    uint16_t current_heap_base = reg[RBSH];

    // Check if the new heap size is bigger than the old one
    if (new_heap_size > current_heap_size) {
        // Calculate the difference
        uint16_t diff = new_heap_size - current_heap_size;

        // Check if there is enough free space
        if (mem[current_heap_base + current_heap_size] == 0) {
            // Check if the free space is enough
            if (mem[current_heap_base + current_heap_size] + 2 >= diff) {
                // Allocate the additional space
                allocMem(diff);
                reg[RBDH] = new_heap_size;
            } else {
                fprintf(stderr, "Cannot allocate more space for the heap of pid %hu since total free space here is not enough.\n", mem[Cur_Proc_ID]);
                return;
            }
        } else {
            fprintf(stderr, "Cannot allocate more space for the heap of pid %hu since we bumped into an allocated region.\n", mem[Cur_Proc_ID]);
            return;
        }
    } else if (new_heap_size < current_heap_size) {
        // Free the unused space
        freeMem(current_heap_base + new_heap_size);
        reg[RBDH] = new_heap_size;
    }
}

static inline void tyld() {
    // Save the current process's state
    uint16_t current_pid = mem[Cur_Proc_ID];
    uint16_t pcb_addr = current_pid * PCB_SIZE + OS_MEM_SIZE;
    mem[pcb_addr + PC_PCB] = reg[RPC];
    mem[pcb_addr + BSC_PCB] = reg[RBSC];
    mem[pcb_addr + BDC_PCB] = reg[RBDC];
    mem[pcb_addr + BSH_PCB] = reg[RBSH];
    mem[pcb_addr + BDH_PCB] = reg[RBDH];

    // Find the next runnable process
    uint16_t next_pid = (current_pid + 1) % mem[Proc_Count];
    while (mem[next_pid * PCB_SIZE + OS_MEM_SIZE + PID_PCB] == 0xffff) {
        next_pid = (next_pid + 1) % mem[Proc_Count];
    }

    // Load the next process's state
    loadProc(next_pid);
}

// instructions to modify
static inline void thalt() {
    // Free the code and heap segments
    uint16_t current_pid = mem[Cur_Proc_ID];
    uint16_t pcb_addr = current_pid * PCB_SIZE + OS_MEM_SIZE;
    freeMem(mem[pcb_addr + BSC_PCB]);
    freeMem(mem[pcb_addr + BSH_PCB]);

    // Mark the process as terminated
    mem[pcb_addr + PID_PCB] = 0xffff;

    // Find the next runnable process
    tyld();
} 

static inline uint16_t mr(uint16_t address) {
    // Check for segmentation fault
    uint16_t segment = address >> 12;
    uint16_t offset = address & 0x0FFF;

    // Check if the address is in the code segment
    if (segment == 0x03) {
        if (offset >= reg[RBDC]) {
            fprintf(stderr, "Segmentation Fault Inside Code Segment.\n");
            exit(1);
        }
        return mem[reg[RBSC] + offset];
    }

    // Check if the address is in the heap segment
    if (segment == 0x01) {
        if (offset >= reg[RBDH]) {
            fprintf(stderr, "Segmentation Fault Inside Heap Segment.\n");
            exit(1);
        }
        return mem[reg[RBSH] + offset];
    }

    // Check for invalid segment
    if (segment == 0x00 || segment == 0x02 || segment >= 0x08) {
        fprintf(stderr, "Segmentation Fault.\n");
        exit(1);
    }

    // Should not reach here
    return 0;
}

static inline void mw(uint16_t address, uint16_t val) {
    // Check for segmentation fault
    uint16_t segment = address >> 12;
    uint16_t offset = address & 0x0FFF;

    // Check if the address is in the code segment
    if (segment == 0x03) {
        if (offset >= reg[RBDC]) {
            fprintf(stderr, "Segmentation Fault Inside Code Segment.\n");
            exit(1);
        }
        mem[reg[RBSC] + offset] = val;
        return;
    }

    // Check if the address is in the heap segment
    if (segment == 0x01) {
        if (offset >= reg[RBDH]) {
            fprintf(stderr, "Segmentation Fault Inside Heap Segment.\n");
            exit(1);
        }
        mem[reg[RBSH] + offset] = val;
        return;
    }

    // Check for invalid segment
    if (segment == 0x00 || segment == 0x02 || segment >= 0x08) {
        fprintf(stderr, "Segmentation Fault.\n");
        exit(1);
    }

    // Should not reach here
    return;
}
// YOUR CODE ENDS HERE