#include <gtest/gtest.h>
#include "vm.h"
#include "assembler.h"
#include <sstream>
#include <cmath>

// ============================================================================
// Helper Functions
// ============================================================================

// Run a program and return the VM after execution
static VirtualMachine runProgram(const char* source) {
    Assembler assembler;
    auto result = assembler.assemble(source);
    VirtualMachine vm;
    vm.loadProgram(result.instructions, result.constants);
    vm.run();
    return vm;
}

// ============================================================================
// Test: Fibonacci
// ============================================================================

TEST(ProgramTest, Fibonacci) {
    const char* source = R"(
        movi r0, #10        ; n = 10
        movi r1, #0         ; a = 0 (fib(0))
        movi r2, #1         ; b = 1 (fib(1))
        movi r3, #1         ; counter = 1

    fib_loop:
        cmp r3, r0
        jge @fib_done
        add r4, r1, r2      ; temp = a + b
        mov r1, r2          ; a = b
        mov r2, r4          ; b = temp
        inc r3
        jmp @fib_loop

    fib_done:
        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    // fib(10) = 55
    EXPECT_EQ(vm.reg(R2).asInt(), 55);
}

TEST(ProgramTest, FibonacciSmall) {
    const char* source = R"(
        movi r0, #7         ; n = 7
        movi r1, #0         ; a = 0
        movi r2, #1         ; b = 1
        movi r3, #1         ; counter = 1

    fib_loop:
        cmp r3, r0
        jge @fib_done
        add r4, r1, r2
        mov r1, r2
        mov r2, r4
        inc r3
        jmp @fib_loop

    fib_done:
        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    // fib(7) = 13
    EXPECT_EQ(vm.reg(R2).asInt(), 13);
}

// ============================================================================
// Test: Factorial
// ============================================================================

TEST(ProgramTest, Factorial) {
    const char* source = R"(
        movi r0, #5         ; compute 5!
        call @factorial
        halt

    factorial:
        push r0
        push r1

        movi r1, #1
        cmp r0, r1
        jle @fact_base

        dec r0
        call @factorial
        pop r1
        pop r0
        mul rrv, r0, rrv
        ret

    fact_base:
        pop r1
        pop r0
        movi rrv, #1
        ret
    )";

    VirtualMachine vm = runProgram(source);
    
    // 5! = 120
    EXPECT_EQ(vm.reg(RRV).asInt(), 120);
}

TEST(ProgramTest, FactorialTwelve) {
    const char* source = R"(
        movi r0, #12        ; compute 12!
        call @factorial
        halt

    factorial:
        push r0
        push r1

        movi r1, #1
        cmp r0, r1
        jle @fact_base

        dec r0
        call @factorial
        pop r1
        pop r0
        mul rrv, r0, rrv
        ret

    fact_base:
        pop r1
        pop r0
        movi rrv, #1
        ret
    )";

    VirtualMachine vm = runProgram(source);
    
    // 12! = 479001600
    EXPECT_EQ(vm.reg(RRV).asInt(), 479001600);
}

// ============================================================================
// Test: Bubble Sort
// ============================================================================

TEST(ProgramTest, BubbleSort) {
    const char* source = R"(
        ; Create array with unsorted values
        movi r0, #5          ; array size
        anew r1, r0          ; r1 = new array[5]

        ; Fill array: [5, 3, 1, 4, 2]
        movi r2, #0
        movi r3, #5
        aset r1, r2, r3
        movi r2, #1
        movi r3, #3
        aset r1, r2, r3
        movi r2, #2
        movi r3, #1
        aset r1, r2, r3
        movi r2, #3
        movi r3, #4
        aset r1, r2, r3
        movi r2, #4
        movi r3, #2
        aset r1, r2, r3

        ; Bubble sort: outer loop i = 0..n-1
        movi r5, #0
    outer_loop:
        cmp r5, r0
        jge @sort_done

        movi r6, #0
        sub r7, r0, r5
        dec r7

    inner_loop:
        cmp r6, r7
        jge @inner_done

        aget r8, r1, r6
        movi r9, #1
        add r9, r6, r9
        aget r10, r1, r9

        cmp r8, r10
        jle @no_swap

        aset r1, r6, r10
        aset r1, r9, r8

    no_swap:
        inc r6
        jmp @inner_loop

    inner_done:
        inc r5
        jmp @outer_loop

    sort_done:
        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    // Verify array is sorted: [1, 2, 3, 4, 5]
    auto& arr = vm.getArray(vm.reg(R1).ptr);
    EXPECT_EQ(arr.elements[0].asInt(), 1);
    EXPECT_EQ(arr.elements[1].asInt(), 2);
    EXPECT_EQ(arr.elements[2].asInt(), 3);
    EXPECT_EQ(arr.elements[3].asInt(), 4);
    EXPECT_EQ(arr.elements[4].asInt(), 5);
}

// ============================================================================
// Test: Exception Handling
// ============================================================================

TEST(ProgramTest, ExceptionHandling) {
    const char* source = R"(
        movi r5, #0          ; flag: 0 = handler not called

        try @handler

        movi r0, #42
        movi r1, #0
        div r2, r0, r1       ; Division by zero!

        movi r5, #99         ; Should NOT reach here
        endtry
        jmp @done

    handler:
        movi r5, #1          ; Handler was called
        ; r0 contains error code

    done:
        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    // Handler should have been called (r5 = 1)
    EXPECT_EQ(vm.reg(R5).asInt(), 1);
    // Error code should be 1 (division by zero)
    EXPECT_EQ(vm.reg(R0).asInt(), 1);
}

// ============================================================================
// Test: Floating Point Math
// ============================================================================

TEST(ProgramTest, FloatCircleArea) {
    const char* source = R"(
        movi r0, #3.14159265
        movi r1, #5.0         ; radius = 5
        fmul r2, r1, r1       ; r^2
        fmul r3, r0, r2       ; pi * r^2
        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    // Area of circle with r=5: pi * 25 ≈ 78.54
    EXPECT_NEAR(vm.reg(R3).asFloat(), 78.539816, 0.001);
}

TEST(ProgramTest, FloatHypotenuse) {
    const char* source = R"(
        movi r0, #3.0         ; a = 3
        movi r1, #4.0         ; b = 4
        fmul r2, r0, r0       ; a^2
        fmul r3, r1, r1       ; b^2
        fadd r4, r2, r3       ; a^2 + b^2
        fsqrt r5, r4          ; sqrt(a^2 + b^2)
        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    // sqrt(9 + 16) = sqrt(25) = 5
    EXPECT_NEAR(vm.reg(R5).asFloat(), 5.0, 0.0001);
}

TEST(ProgramTest, FloatTrigonometry) {
    const char* source = R"(
        movi r0, #1.5707963   ; pi/2
        fsin r1, r0
        fcos r2, r0
        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    // sin(pi/2) = 1, cos(pi/2) = 0
    EXPECT_NEAR(vm.reg(R1).asFloat(), 1.0, 0.0001);
    EXPECT_NEAR(vm.reg(R2).asFloat(), 0.0, 0.0001);
}

TEST(ProgramTest, FloatPower) {
    const char* source = R"(
        movi r0, #2.0
        movi r1, #10.0
        fpow r2, r0, r1
        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    // 2^10 = 1024
    EXPECT_NEAR(vm.reg(R2).asFloat(), 1024.0, 0.0001);
}

// ============================================================================
// Test: Heap / Linked List
// ============================================================================

TEST(ProgramTest, HeapLinkedList) {
    const char* source = R"(
        movi r5, #2           ; node size = 2 (value + next)

        ; Node 1: value = 100
        alloc r1, r5
        movi r0, #100
        storeh r1, #0, r0

        ; Node 2: value = 200
        alloc r2, r5
        movi r0, #200
        storeh r2, #0, r0

        ; Node 3: value = 300
        alloc r3, r5
        movi r0, #300
        storeh r3, #0, r0

        ; Link: node1 -> node2 -> node3 -> null
        storeh r1, #1, r2
        storeh r2, #1, r3
        movi r0, #0
        storeh r3, #1, r0

        ; Read values by traversing
        loadh r6, r1, #0      ; node1.value
        loadh r7, r1, #1      ; node1.next (= node2)
        loadh r8, r7, #0      ; node2.value
        loadh r9, r7, #1      ; node2.next (= node3)
        loadh r10, r9, #0     ; node3.value

        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    EXPECT_EQ(vm.reg(R6).asInt(), 100);   // node1.value
    EXPECT_EQ(vm.reg(R8).asInt(), 200);   // node2.value
    EXPECT_EQ(vm.reg(R10).asInt(), 300);  // node3.value
}

// ============================================================================
// Test: Sum of Squares
// ============================================================================

TEST(ProgramTest, SumOfSquares) {
    const char* source = R"(
        movi r0, #10          ; N = 10
        movi r1, #0           ; sum = 0
        movi r2, #1           ; current = 1
        mov  r3, r0           ; loop counter = N

    sum_loop:
        mul  r4, r2, r2       ; r4 = current^2
        add  r1, r1, r4       ; sum += current^2
        inc  r2               ; current++
        loop @sum_loop, r3    ; counter--, jump if != 0

        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    // Sum of squares 1..10 = 1+4+9+16+25+36+49+64+81+100 = 385
    EXPECT_EQ(vm.reg(R1).asInt(), 385);
}

TEST(ProgramTest, SumOfSquaresHundred) {
    const char* source = R"(
        movi r0, #100         ; N = 100
        movi r1, #0           ; sum = 0
        movi r2, #1           ; current = 1
        mov  r3, r0           ; loop counter = N

    sum_loop:
        mul  r4, r2, r2
        add  r1, r1, r4
        inc  r2
        loop @sum_loop, r3

        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    // Sum of squares 1..100 = 338350
    EXPECT_EQ(vm.reg(R1).asInt(), 338350);
}

// ============================================================================
// Test: Garbage Collection
// ============================================================================

TEST(ProgramTest, GarbageCollection) {
    const char* source = R"(
        gc_off
        movi r5, #4           ; block size = 4

        ; Allocate 3 objects
        alloc r1, r5          ; obj1 (will stay reachable)
        movi r0, #111
        storeh r1, #0, r0

        alloc r2, r5          ; obj2 (will become unreachable)
        movi r0, #222
        storeh r2, #0, r0

        alloc r3, r5          ; obj3 (will stay reachable via link)
        movi r0, #333
        storeh r3, #0, r0
        storeh r1, #1, r3     ; link obj1 -> obj3

        ; Lose reference to obj2
        movi r2, #0

        ; Run GC
        gc_on
        gc_collect

        ; Verify obj1 and obj3 survived
        loadh r6, r1, #0      ; obj1.value
        loadh r7, r1, #1      ; obj1.next (= obj3)
        loadh r8, r7, #0      ; obj3.value

        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    // obj1 survived
    EXPECT_EQ(vm.reg(R6).asInt(), 111);
    // obj3 survived (reachable via obj1)
    EXPECT_EQ(vm.reg(R8).asInt(), 333);
    // At least one object was freed
    EXPECT_GE(vm.stats().gcObjectsFreed, 1);
}

TEST(ProgramTest, ArrayGarbageCollection) {
    const char* source = R"(
        gc_on
        movi r0, #5

        anew r1, r0            ; array1 (will stay reachable)
        movi r2, #0
        movi r3, #42
        aset r1, r2, r3

        anew r4, r0            ; array2 (will become unreachable)
        movi r3, #99
        aset r4, r2, r3

        ; Lose ref to array2
        movi r4, #0

        gc_collect

        ; Verify array1 survived
        movi r2, #0
        aget r5, r1, r2

        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    EXPECT_EQ(vm.reg(R5).asInt(), 42);
    EXPECT_GE(vm.stats().gcArraysFreed, 1);
}

// ============================================================================
// Test: Basic Operations
// ============================================================================

TEST(VMTest, Arithmetic) {
    const char* source = R"(
        movi r0, #100
        movi r1, #50
        add r2, r0, r1        ; 150
        sub r3, r0, r1        ; 50
        mul r4, r0, r1        ; 5000
        div r5, r0, r1        ; 2
        mod r6, r0, r1        ; 0
        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    EXPECT_EQ(vm.reg(R2).asInt(), 150);
    EXPECT_EQ(vm.reg(R3).asInt(), 50);
    EXPECT_EQ(vm.reg(R4).asInt(), 5000);
    EXPECT_EQ(vm.reg(R5).asInt(), 2);
    EXPECT_EQ(vm.reg(R6).asInt(), 0);
}

TEST(VMTest, BitwiseOperations) {
    const char* source = R"(
        movi r0, #255
        movi r1, #15
        and r2, r0, r1        ; 15
        or  r3, r0, r1        ; 255
        xor r4, r0, r1        ; 240
        not r5, r1            ; ~15
        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    EXPECT_EQ(vm.reg(R2).asInt(), 15);   // 0x0F
    EXPECT_EQ(vm.reg(R3).asInt(), 255);  // 0xFF
    EXPECT_EQ(vm.reg(R4).asInt(), 240);  // 0xF0
}

TEST(VMTest, StackOperations) {
    const char* source = R"(
        movi r0, #10
        movi r1, #20
        movi r2, #30
        push r0
        push r1
        push r2
        pop r3               ; r3 = 30
        pop r4               ; r4 = 20
        pop r5               ; r5 = 10
        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    EXPECT_EQ(vm.reg(R3).asInt(), 30);
    EXPECT_EQ(vm.reg(R4).asInt(), 20);
    EXPECT_EQ(vm.reg(R5).asInt(), 10);
}

TEST(VMTest, ConditionalJumps) {
    const char* source = R"(
        movi r0, #5
        movi r1, #10
        movi r2, #0          ; result flag

        cmp r0, r1           ; 5 < 10
        jl @less
        jmp @done

    less:
        movi r2, #1          ; set flag

    done:
        halt
    )";

    VirtualMachine vm = runProgram(source);
    
    EXPECT_EQ(vm.reg(R2).asInt(), 1);  // Flag should be set
}
// ============================================================================
// Main
// ============================================================================
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
