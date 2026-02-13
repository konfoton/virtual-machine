#include "vm.h"
#include "assembler.h"
#include <iostream>
#include <fstream>
#include <sstream>

// ============================================================================
// Example Programs
// ============================================================================

// 1. Fibonacci: compute fib(N) iteratively
static const char* PROGRAM_FIBONACCI = R"(
; ============================================
; Fibonacci sequence — computes fib(10)
; Result is printed at the end
; ============================================

    movi r0, #10        ; n = 10
    movi r1, #0         ; a = 0 (fib(0))
    movi r2, #1         ; b = 1 (fib(1))
    movi r3, #1         ; counter = 1

fib_loop:
    cmp r3, r0
    jge @fib_done       ; if counter >= n, done
    add r4, r1, r2      ; temp = a + b
    mov r1, r2          ; a = b
    mov r2, r4          ; b = temp
    inc r3              ; counter++
    jmp @fib_loop

fib_done:
    prints #"fib(10) = "
    println r2
    halt
)";

// 2. Factorial using function calls
static const char* PROGRAM_FACTORIAL = R"(
; ============================================
; Factorial — computes 12! using CALL/RET
; ============================================

    movi r0, #12        ; compute 12!
    call @factorial
    prints #"12! = "
    println rrv
    halt

; --- factorial(r0) -> rrv ---
factorial:
    push r0              ; save n
    push r1              ; save work register

    movi r1, #1          ; check if n <= 1
    cmp r0, r1
    jle @fact_base

    ; Recursive case: n * factorial(n-1)
    dec r0               ; n - 1
    call @factorial      ; rrv = factorial(n-1)
    pop r1               ; restore r1
    pop r0               ; restore original n
    mul rrv, r0, rrv     ; rrv = n * factorial(n-1)
    ret

fact_base:
    pop r1
    pop r0
    movi rrv, #1         ; return 1
    ret
)";

// 3. Bubble sort using arrays
static const char* PROGRAM_BUBBLESORT = R"(
; ============================================
; Bubble Sort — sort an array of integers
; ============================================

    ; Create array with unsorted values
    movi r0, #8          ; array size
    anew r1, r0          ; r1 = new array[8]

    ; Fill array: [64, 34, 25, 12, 22, 11, 90, 1]
    movi r2, #0
    movi r3, #64
    aset r1, r2, r3
    movi r2, #1
    movi r3, #34
    aset r1, r2, r3
    movi r2, #2
    movi r3, #25
    aset r1, r2, r3
    movi r2, #3
    movi r3, #12
    aset r1, r2, r3
    movi r2, #4
    movi r3, #22
    aset r1, r2, r3
    movi r2, #5
    movi r3, #11
    aset r1, r2, r3
    movi r2, #6
    movi r3, #90
    aset r1, r2, r3
    movi r2, #7
    movi r3, #1
    aset r1, r2, r3

    ; Print unsorted
    prints #"Before: "
    call @print_array

    ; Bubble sort: outer loop i = 0..n-1
    movi r5, #0          ; i = 0
outer_loop:
    cmp r5, r0
    jge @sort_done

    ; inner loop j = 0..n-i-2
    movi r6, #0          ; j = 0
    sub r7, r0, r5       ; r7 = n - i
    dec r7               ; r7 = n - i - 1

inner_loop:
    cmp r6, r7
    jge @inner_done

    ; Compare arr[j] and arr[j+1]
    aget r8, r1, r6      ; r8 = arr[j]
    movi r9, #1
    add r9, r6, r9       ; r9 = j + 1
    aget r10, r1, r9     ; r10 = arr[j+1]

    cmp r8, r10
    jle @no_swap

    ; Swap arr[j] and arr[j+1]
    aset r1, r6, r10
    aset r1, r9, r8

no_swap:
    inc r6
    jmp @inner_loop

inner_done:
    inc r5
    jmp @outer_loop

sort_done:
    prints #"After:  "
    call @print_array
    halt

; --- print_array: prints array in r1 with length in r0 ---
print_array:
    push r2
    push r3
    prints #"["
    movi r2, #0
pa_loop:
    cmp r2, r0
    jge @pa_done
    aget r3, r1, r2
    print r3
    movi r3, #1
    add r3, r2, r3
    cmp r3, r0
    jge @pa_no_comma
    prints #", "
pa_no_comma:
    inc r2
    jmp @pa_loop
pa_done:
    prints #"]\n"
    pop r3
    pop r2
    ret
)";

// 4. Exception handling demo
static const char* PROGRAM_EXCEPTIONS = R"(
; ============================================
; Exception handling demo
; ============================================

    prints #"Starting exception test...\n"

    ; Set up exception handler
    try @handler

    prints #"About to divide by zero...\n"
    movi r0, #42
    movi r1, #0
    div r2, r0, r1       ; This will cause a runtime error

    ; Should not reach here
    prints #"This should not print\n"
    endtry
    jmp @after_handler

handler:
    ; Exception handler — error code in r0
    prints #"Caught exception! Error code: "
    println r0

after_handler:
    prints #"Program continues normally.\n"
    halt
)";

// 5. Floating-point math demo
static const char* PROGRAM_MATH = R"(
; ============================================
; Floating-point math demo
; ============================================

    ; Compute area of circle: pi * r^2
    movi r0, #3.14159265
    movi r1, #5.0         ; radius = 5
    fmul r2, r1, r1       ; r^2
    fmul r3, r0, r2       ; pi * r^2
    prints #"Area of circle (r=5): "
    println r3

    ; Compute hypotenuse: sqrt(a^2 + b^2)
    movi r0, #3.0         ; a = 3
    movi r1, #4.0         ; b = 4
    fmul r2, r0, r0       ; a^2
    fmul r3, r1, r1       ; b^2
    fadd r4, r2, r3       ; a^2 + b^2
    fsqrt r5, r4          ; sqrt(a^2 + b^2)
    prints #"Hypotenuse (3,4): "
    println r5

    ; Sine and cosine
    movi r0, #1.5707963   ; pi/2
    fsin r1, r0
    prints #"sin(pi/2) = "
    println r1
    fcos r2, r0
    prints #"cos(pi/2) = "
    println r2

    ; Power: 2^10
    movi r0, #2.0
    movi r1, #10.0
    fpow r2, r0, r1
    prints #"2^10 = "
    println r2

    halt
)";

// 6. Heap memory demo — linked-list-style structure
static const char* PROGRAM_HEAP = R"(
; ============================================
; Heap memory: build a simple linked list
; Each node: [value, next_ptr]  (2 slots)
; ============================================

    ; Allocate 3 nodes
    movi r5, #2           ; node size = 2 (value + next)

    ; Node 1: value = 100
    alloc r1, r5
    movi r0, #100
    storeh r1, #0, r0     ; node1.value = 100

    ; Node 2: value = 200
    alloc r2, r5
    movi r0, #200
    storeh r2, #0, r0     ; node2.value = 200

    ; Node 3: value = 300
    alloc r3, r5
    movi r0, #300
    storeh r3, #0, r0     ; node3.value = 300

    ; Link: node1 -> node2 -> node3 -> null
    storeh r1, #1, r2     ; node1.next = node2
    storeh r2, #1, r3     ; node2.next = node3
    movi r0, #0
    storeh r3, #1, r0     ; node3.next = null

    ; Traverse and print
    prints #"Linked list: "
    mov r4, r1            ; current = head

traverse:
    ; Load value
    loadh r6, r4, #0     ; r6 = current.value
    print r6
    prints #" -> "

    ; Load next pointer
    loadh r4, r4, #1     ; current = current.next
    movi r7, #0
    cmp r4, r7
    jnz @traverse

    prints #"null\n"

    ; Free all nodes
    free r1
    free r2
    free r3

    prints #"All nodes freed.\n"
    dump_heap
    halt
)";

// 7. Compute sum of squares 1^2 + 2^2 + ... + N^2 with LOOP instruction
static const char* PROGRAM_SUM_SQUARES = R"(
; ============================================
; Sum of squares: 1^2 + 2^2 + ... + 100^2
; Uses the LOOP instruction for the counter
; ============================================

    movi r0, #100         ; N = 100
    movi r1, #0           ; sum = 0
    movi r2, #1           ; current = 1
    mov  r3, r0           ; loop counter = N

sum_loop:
    mul  r4, r2, r2       ; r4 = current^2
    add  r1, r1, r4       ; sum += current^2
    inc  r2               ; current++
    loop @sum_loop, r3    ; counter--, jump if != 0

    prints #"Sum of squares (1..100) = "
    println r1            ; Expected: 338350
    halt
)";

// 8. Garbage Collection demo
static const char* PROGRAM_GC = R"(
; ============================================
; Garbage Collection Demo
; Shows automatic memory reclamation
; ============================================

    ; --- Phase 1: Allocate without freeing, GC off ---
    gc_off
    prints #"=== Phase 1: Allocating 5 heap objects (GC off) ===\n"

    movi r5, #4           ; block size = 4
    alloc r1, r5          ; obj1 (will stay reachable)
    movi r0, #111
    storeh r1, #0, r0

    alloc r2, r5          ; obj2 (will become unreachable)
    movi r0, #222
    storeh r2, #0, r0

    alloc r3, r5          ; obj3 (will become unreachable)
    movi r0, #333
    storeh r3, #0, r0

    alloc r4, r5          ; obj4 (will stay reachable via r1 link)
    movi r0, #444
    storeh r4, #0, r0
    storeh r1, #1, r4     ; link obj1 -> obj4

    alloc r6, r5          ; obj5 (will become unreachable)
    movi r0, #555
    storeh r6, #0, r0

    prints #"Heap after 5 allocations:\n"
    dump_heap
    gc_stats

    ; --- Phase 2: Lose references, then GC ---
    prints #"\n=== Phase 2: Losing refs to obj2, obj3, obj5 ===\n"
    movi r2, #0           ; lose ref to obj2
    movi r3, #0           ; lose ref to obj3
    movi r6, #0           ; lose ref to obj5

    prints #"Running GC...\n"
    gc_on
    gc_collect
    gc_stats

    prints #"Heap after GC (obj2,obj3,obj5 freed):\n"
    dump_heap

    ; --- Phase 3: Verify reachable objects survived ---
    prints #"\n=== Phase 3: Reachable objects survived ===\n"
    prints #"obj1.value = "
    loadh r0, r1, #0
    println r0
    prints #"obj4.value (via obj1 link) = "
    loadh r7, r1, #1      ; follow link from obj1
    loadh r0, r7, #0      ; read obj4.value
    println r0

    ; --- Phase 4: Array GC ---
    prints #"\n=== Phase 4: Array garbage collection ===\n"
    movi r8, #3
    anew r9, r8            ; array1 (will stay reachable)
    movi r0, #10
    movi r2, #0
    aset r9, r2, r0
    anew r10, r8           ; array2 (will become unreachable)
    movi r0, #20
    aset r10, r2, r0

    prints #"Created 2 arrays. Losing ref to array2...\n"
    movi r10, #0           ; lose ref to array2
    gc_collect
    gc_stats

    prints #"array1[0] = "
    movi r2, #0
    aget r0, r9, r2
    println r0

    prints #"\n=== GC Demo Complete ===\n"
    halt
)";

// ============================================================================
// Main
// ============================================================================

static void runProgram(const std::string& name, const char* source) {
    std::cout << "\n========== " << name << " ==========\n" << std::endl;

    Assembler assembler;
    try {
        auto result = assembler.assemble(source);

        // Print disassembly
        std::cout << "--- Disassembly ---" << std::endl;
        std::cout << Assembler::disassemble(result.instructions, result.constants);
        std::cout << "-------------------" << std::endl;

        VirtualMachine vm;
        vm.loadProgram(result.instructions, result.constants);
        // vm.setTrace(true); // uncomment for instruction tracing

        std::cout << "\n--- Output ---" << std::endl;
        auto execResult = vm.run();

        std::cout << "\n--- Stats ---" << std::endl;
        auto& stats = vm.stats();
        std::cout << "Instructions executed: " << stats.instructionsExecuted << std::endl;
        std::cout << "Function calls:       " << stats.functionCalls << std::endl;
        std::cout << "Memory allocations:   " << stats.memoryAllocations << std::endl;
        std::cout << "Peak stack depth:     " << stats.peakStackDepth << std::endl;

        if (execResult == VirtualMachine::ExecResult::ERROR) {
            std::cout << "** Program terminated with error **" << std::endl;
        }

    } catch (const AssemblerError& e) {
        std::cerr << "Assembly error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "=== Complex Virtual Machine ===" << std::endl;
    std::cout << "Registers: R0-R15 + RSP, RFP, RPC, RFLAGS, RRV" << std::endl;
    std::cout << "Features: Stack, Heap, Functions, Exceptions, Arrays, FP Math" << std::endl;

    if (argc > 1) {
        // Load from file
        std::string filename = argv[1];
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: cannot open file: " << filename << std::endl;
            return 1;
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        runProgram(filename, ss.str().c_str());
        return 0;
    }

    // Run all built-in demos
    runProgram("Fibonacci",       PROGRAM_FIBONACCI);
    runProgram("Factorial",       PROGRAM_FACTORIAL);
    runProgram("Bubble Sort",     PROGRAM_BUBBLESORT);
    runProgram("Exception Demo",  PROGRAM_EXCEPTIONS);
    runProgram("FP Math",         PROGRAM_MATH);
    runProgram("Heap / Linked List", PROGRAM_HEAP);
    runProgram("Sum of Squares",  PROGRAM_SUM_SQUARES);
    runProgram("Garbage Collection", PROGRAM_GC);

    return 0;
}
