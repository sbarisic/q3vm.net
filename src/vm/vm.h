/*
      ___   _______     ____  __
     / _ \ |___ /\ \   / /  \/  |
    | | | |  |_ \ \ \ / /| |\/| |
    | |_| |____) | \ V / | |  | |
     \__\_______/   \_/  |_|  |_|


   Quake III Arena Virtual Machine
*/

/******************************************************************************
 * SYSTEM INCLUDE FILES
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/******************************************************************************
 * PROJECT INCLUDE FILES
 ******************************************************************************/

/******************************************************************************
 * DEFINES
 ******************************************************************************/

// #define DEBUG_VM /**< ifdef: enable debug functions and additional checks */
#define ID_INLINE inline
#define MAX_QPATH 64 /**< max length of a pathname */

/** Redirect information output */
#define Com_Printf printf
/** Redirect memset call here */
#define Com_Memset memset
/** Redirect memcpy call here */
#define Com_Memcpy memcpy
/** Redirect malloc call here */
#define Com_malloc malloc
/** Redirect free call here */
#define Com_free free

/** Translate pointer from VM memory to system memory */
#define VMA(x) VM_ArgPtr(args[x])
/** Get float argument in syscall (used in system calls) and
 * don't cast it. */
#define VMF(x) _vmf(args[x])
void* VM_ArgPtr(intptr_t intValue); /**< for the VMA macro */

/** Define endianess of target platform */
#define Q3VM_LITTLE_ENDIAN

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

/** union to access a variable as float or int */
typedef union {
    float    f;  /**< float IEEE 754 32-bit single */
    int32_t  i;  /**< int32 part */
    uint32_t ui; /**< unsigned int32 part */
} floatint_t;

/** For debugging: symbols */
typedef struct vmSymbol_s
{
    struct vmSymbol_s* next;     /**< Linked list of symbols */
    int                symValue; /**< Value of symbol that we want to have the
                                   ASCII name for */
    int profileCount;            /**< For the runtime profiler. +1 for each
                                   call. */
    char symName[1];             /**< Variable sized symbol name. Space is
                                   reserved by malloc at load time. */
} vmSymbol_t;

/** File header of a bytecode .qvm file. Can be directly mapped to the start of
 *  the file. */
typedef struct
{
    int32_t vmMagic; /**< Bytecode image shall start with VM_MAGIC */

    int32_t instructionCount;

    int32_t codeOffset;
    int32_t codeLength;

    int32_t dataOffset;
    int32_t dataLength;
    int32_t
            litLength; /**< (dataLength-litLength) should be byteswapped on load */
    int32_t bssLength; /**< Zero filled memory appended to datalength */
} vmHeader_t;

/** Main struct (think of a kind of a main class) to keep all information of
 * the virtual machine together. Has pointer to the bytecode, the stack and
 * everything. Call VM_Create(...) to initialize this struct. Call VM_Free(...)
 * to cleanup this struct and free the memory. */
typedef struct vm_s
{
    // DO NOT MOVE OR CHANGE THESE WITHOUT CHANGING THE VM_OFFSET_* DEFINES
    // USED BY THE ASM CODE
    int programStack; /**< the vm may be recursively entered */
    /** Function pointer to callback function for native functions called by
     * the bytecode. The function is identified by an integer id
     * that corresponds to the bytecode function ids defined in g_syscalls.asm.
     * Note however that parms equals to (-1 - function_id). So -1 in
     * g_syscalls.asm
     * equals to 0 in the systemCall parms argument, -2 in g_syscalls.asm is 1
     * in parms,
     * -3 is 2 and so on. */
    intptr_t (*systemCall)(intptr_t* parms);

    //------------------------------------

    char  name[MAX_QPATH]; /** File name of the bytecode */
    void* searchPath;      /** unused */

    int currentlyInterpreting; /** Is the interpreter currently running? */

    int      compiled; /** Is a JIT active? Otherwise interpreted */
    uint8_t* codeBase; /** Bytecode code segment */
    int      entryOfs; /** unused */
    int      codeLength;

    intptr_t* instructionPointers;
    int       instructionCount;

    uint8_t* dataBase;
    int      dataMask;
    int      dataAlloc; /**< Number of bytes allocated for dataBase */

    int stackBottom; /**< If programStack < stackBottom, error */

    int         numSymbols; /**< Number of loaded symbols */
    vmSymbol_t* symbols;    /**< Loaded symbols for debugging */

    int callLevel;     /**< Counts recursive VM_Call */
    int breakFunction; /**< Debug breakpoints: increment breakCount on function
                         entry to this */
    int breakCount;    /**< Used for breakpoints (triggered by OP_BREAK) */
} vm_t;

/******************************************************************************
 * FUNCTION PROTOTYPES
 ******************************************************************************/

/** Implement this error callback function for error callbacks.
 * @param[in] level Error identifier.
 * @param[in] error Human readable error text. */
void Com_Error(int level, const char* error);

/** Initialize a virtual machine.
 * @param[out] vm Pointer to a virtual machine to initialize.
 * @param[in] module Path to the bytecode file. Used to load the
 *                   symbols. Otherwise not strictly required.
 * @param[in] bytecode Pointer to the bytecode. Directly byte by byte
 *                     the content of the .qvm file.
 * @param[in] systemCalls Function pointer to callback function for native
 *   functions called by the bytecode. The function is identified by an integer
 *   id that corresponds to the bytecode function ids defined in g_syscalls.asm.
 *   Note however that parms equals to (-1 - function_id). So -1 in
 *   g_syscalls.asm equals to 0 in the systemCall parms argument, -2 in
 *   g_syscalls.asm is 1 in parms, -3 is 2 and so on.
 * @return 0 if everything is OK. -1 if something went wrong. */
int VM_Create(vm_t* vm, const char* module, uint8_t* bytecode,
              intptr_t (*systemCalls)(intptr_t*));

/** Free the memory of the virtual machine.
 * @param[in] vm Pointer to initialized virtual machine. */
void VM_Free(vm_t* vm);

/** Run a function from the virtual machine.
 * @param[in] vm Pointer to initialized virtual machine.
 * @param[in] callNum Argument of function call.
 * @return Return value of the function call. */
intptr_t VM_Call(vm_t* vm, int callNum);

/******************************************************************************
 * INLINE
 ******************************************************************************/

/** Helper function for VMF. Used for float arguments on system calls.
 * @param[in] x Argument on call stack.
 * @return Value as float. */
static ID_INLINE float _vmf(intptr_t x)
{
    floatint_t fi;
    fi.i = (int)x;
    return fi.f;
}