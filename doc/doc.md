# Language Documentation

## Introduction
This document introduces the basic syntax and semantics of the language. It primarily focuses on how to use the language, rather than the design rationale behind its features.

---

## Hello, World!
The language does not require an explicit `main` function. Program execution begins top-to-bottom at the start of the source file provided to the compiler.

A minimal "Hello, world!" program can be written directly using a string literal:

```mylang
"Hello, world!"; // Implicitly prints the string
```

String literals also support inline formatting when followed by arguments:

```mylang
"Hello, %!" "world"; // Prints "Hello, world!"
```

### Compiling and Running
Compile a source file (e.g., `hello.txt`) into an executable (`app.exe`):
```bash
compiler build hello.txt app.exe
```

Compile and run immediately:
```bash
compiler run hello.txt
```

Emit intermediate representation (IR) code without generating a binary:
```bash
compiler translate hello.txt
```

---

## Character Set
Source files are expected to be encoded in UTF-8. Unicode characters are supported directly within source code, including in character constants, and string literals. 

---

## Comments
Comments are ignored by the compiler. Two forms are supported:

### 1. Single-Line Comments
Single-line comments start with `//` and extend to the end of the line:
```mylang
// This is a single-line comment
```

### 2. Multi-Line Comments (Nestable)
Multi-line comments are enclosed in `/{{` and `/}}`. Unlike C-style comments, they can be safely nested:
```mylang
/{{
    This is a multi-line comment.
    /{{
        Nested comments are completely valid
    /}}
/}}
```

---

## Label oriented grammar (TODO)
The deeper I dug into bytecode-like code, the more I sniffed...
There is just something about labels. They are simple, but they get the thing done. They are the most straightforward way to name stuff—and there's quite a lot to name, right? So why not use them uniformly to name **everything**? The more orthogonality, the more it leaves a bit of Lisp on the lips, which is never bad idea to lick...

Every named structural element is declared as a label `identifier: <construct>`, the language relies on a single, uniform declaration syntax:
```mylang
// 1. Constant / Variable
MAX_BUFFER: u32 = 4096;
count: i32 = 0;

// 2. Struct / Type
Vector2: struct {
    x: f32;
    y: f32;
}

// 3. Callable Block (Function)
add: (a: i32, b: i32) -> i32 {
    return a + b;
}

// 4. Pointer Types (using `^`)
ptr: i32^ = &count;
```

---

## Scopes

Scope dictates the lifetime and visibility of your variables. It creates a strictly encapsulated naming space. 

You can open a scope in two ways:
*   **Single-statement scope:** Started via a colon `:`, closed via ';'. 
*   **Block scope:** Enclosed in curly braces `{ ... }`.

```mylang
// Block scope (for multiple statements)
if is_valid {
    process();
    save();
}

// Single-statement scope
if is_valid: process();
```

### Encapsulation & Shadowing
Whenever you open a scope, you create a fresh naming space. Any local variables declared inside it are destroyed as soon as the scope ends. 

If you declare a variable with the same name as one outside the scope, the inner variable will **shadow** the outer one.

```mylang
count: i32 = 10;

{
    // This shadows the outer 'count'
    count: i32 = 99; 
    print(count); // Prints: 99
    
    temp: i32 = 5;
}

print(count); // Prints: 10
// print(temp); -> COMPILE ERROR: 'temp' does not exist
```

### Namespaces
An anonymous scope is a complete black box, its contents cannot be accessed or observed from the outside. But if you attach a label to a scope, it becomes a **Namespace**. This allows the outer scope to reach in and observe compile-time symbols using `::` notation.

```mylang
config: {
    MAX_SPEED: i32 = 300;
}

print(config.MAX_SPEED); // Prints: 300
```

---

## Structural Constraints
'Declarative' definitions, such as structs, functions and enums, represent the structural blueprint of a program rather than its execution flow. Therefore, these definitions are accessible throughout their scope without ordering constraints, meaning they may be referenced before their declaration. Consequently, any data from parent scopes can be used regardless of its ordering relative to the definition.

Imperative definitions, such as variable definitions, represent actual logic of the program. Therefore, they obey ordering.

---

## Primitive Data Types

| Type | Description | Range / Values |
| :--- | :---        | :---           |
| **`i8`**  | Signed 8-bit integer    | $-128 \dots 127$ |
| **`i16`** | Signed 16-bit integer   | $-32,768 \dots 32,767$ |
| **`i32`** | Signed 32-bit integer   | $-2,147,483,648 \dots 2,147,483,647$ |
| **`i64`** | Signed 64-bit integer   | $-9,223,372,036,854,775,808 \dots 9,223,372,036,854,775,807$ |
| **`u8`**  | Unsigned 8-bit integer  | $0 \dots 255$ |
| **`u16`** | Unsigned 16-bit integer | $0 \dots 65,535$ |
| **`u32`** | Unsigned 32-bit integer | $0 \dots 4,294,967,295$ |
| **`u64`** | Unsigned 64-bit integer | $0 \dots 18,446,744,073,709,551,615$ |
| **`f32`** | IEEE-754 32-bit floating-point numbers | ... |
| **`f64`** | IEEE-754 64-bit floating-point numbers | ... |
| **`T^`**	| Typed pointer to memory storing type T valid memory address or null | ... |

---

## Literals

### 1. Integer Literals
Integers can be written in decimal, hexadecimal (`0x`), or binary (`0b`). Underscores (`_`) can be inserted anywhere as visual digit separators:

```mylang
decimal_val: i32 = 1_000_000;
hex_val:     u32 = 0xFF_00_AA;
binary_val:  u8  = 0b1010_0101;
```

### 2. Floating-Point Literals
Floating-point numbers must include a decimal point or exponent. A trailing `f` suffix creates an `f32`, otherwise, the default type is `f64`:

```mylang
pi:             f64 = 3.1415926535;   // Default f64
speed_of_light: f32 = 299_792_458.0f; // Explicit f32
scientific:     f64 = 1.5e-4;         // 0.00015
```

### 3. Character Literals
Enclosed in single quotes (`'...'`), character literals represent Unicode code points or packed integer values:

```mylang
// 1. ASCII / Single Byte
char_a: u8 = 'a'; // 0x61

// 2. Unicode Code Point
char_cz: u16 = 'č'; // Unicode code point U+010D

// 3. Packed Multi-Byte Integer
// Packs 4 ASCII characters into a big-endian u32 (0x61626364)
mbyte: u32 = 'abcd';
```

**Note on Multi-Byte Literals**  
> Multi-byte characters (ex. `'RIFF'`) are packed in **Big-Endian** order (`0x52494646`). This preserves natural left-to-right reading order when cast directly to a string buffer (`u8[]`), while enabling instant, single-cycle integer comparisons for magic numbers and file tags (`if tag == 'RIFF'`).

### 4. String Literals
String literals are enclosed in double quotes (`"..."`) and encoded in UTF-8 by default:

```mylang
// 1. Character sequence (inferred array)
hello: u8[] = "Hello";

// 2. Unicode String
greeting: u16[] = "čau";

// 3. Raw Byte Literal (`b` suffix)
// Forces interpretation as an exact sequence of raw UTF-8 bytes
raw_bytes: u8[] = "čau"b; 
```

There is no special string type, all strings are just arrays.

---

## Declaration Qualifiers

Qualifiers alter the storage duration and mutability of a declaration:

### 1. embed (Compile-Time Constant)
Marks a declaration as a compile-time constant. The compiler is forced to evaluate the right-hand side expression during compilation:

```mylang
MAX_ENTITIES: embed u32 = 1024;
BUFFER_SIZE:  embed u32 = MAX_ENTITIES * resolve_entity_size();
```

**Note:** `embed` is the sufficient entry point for **Compile-Time Execution (CTE)** as it can run arbitrary functions, therefore any language constructs.
(See Section 11: Compile-Time Execution (CTE) for full details and examples).

### 2. const (Runtime Read-Only)
`const` marks a variable as immutable after its initial assignment at runtime:

```mylang
// Assigned at runtime, but cannot be modified thereafter
device_id: const u32 = read_hardware_id();

device_id = 12; // COMPILE ERROR: Cannot mutate a const variable
```

---

## Pointers

Pointers store raw memory addresses and are denoted using the caret symbol (`^`).

Use `T^` to declare a pointer to type `T`, and `&` to take the memory address of a variable:
```mylang
value: i32 = 42;

// `ptr` holds the memory address of `value`
ptr: i32^ = &value;
```

Prefixing a pointer with `^` dereferences it to read or write the underlying memory:

```mylang
// Read underlying value
fetched: i32 = ^ptr; // 42

// Write to underlying memory
^ptr = 99; 

// `value` is now 99
```

A null pointer represents an empty or uninitialized memory address:
```mylang
ptr: i32^ = null;

if ptr != null {
    ^ptr = 10;
}
```
Here is the complete, polished **Operators and Precedence** documentation chapter generated directly from your compiler’s operator table.

---

## Operators

Operators are ranked by precedence (from **Rank 0: Highest** down to **Rank 11: Lowest**). When operators share the same precedence rank, they associate from left to right (except unary operators, which are prefix).

---

### 1. Precedence Table

| Rank | Operator | Name / Description | Associativity |
| :---: | :--- | :--- | :---: |
| **0** | `()` <br> `[]` <br> `.` <br> `^.` | Function Call <br> Array Subscript / Indexing <br> Slicing Operator <br> Member Selection | Left-to-Right |
| **1** | `+`, `-` <br> `&` <br> `^` <br> `!` <br> `~` | Unary Plus, Unary Minus <br> Address-Of <br> Pointer Dereference (Get Value) <br> Logical Negation (NOT) <br> Bitwise Negation (NOT) | Prefix (Right-to-Left) |
| **2** | `*`, `/`, `%` | Multiplication, Division, Modulo | Left-to-Right |
| **3** | `+`, `-` <br> `..` | Addition, Subtraction <br> Array Concatenation | Left-to-Right |
| **4** | `<<`, `>>` | Bitwise Shift Left, Shift Right | Left-to-Right |
| **5** | `<`, `<=`, `>`, `>=` | Relational Comparisons (Less, Greater, Equal) | Left-to-Right |
| **6** | `==`, `!=` | Equality / Inequality | Left-to-Right |
| **7** | `&` | Bitwise AND | Left-to-Right |
| **8** | `^` | Bitwise XOR | Left-to-Right |
| **9** | `\|` | Bitwise OR | Left-to-Right |
| **10** | `&&` | Logical AND (Short-circuiting) | Left-to-Right |
| **11** | `\|\|` | Logical OR (Short-circuiting) | Left-to-Right |

---

### 2. Examples

#### 2.1. Primary & Postfix (Rank 0)
```mylang
result = calculate(10, 20);     // `()` Function Call
byte_val = buffer[4];           // `[]` Indexing
sub_slice = buffer[1:4];        // `[]` Slicing
user_name = user.name;          // `.`  Direct member access
field_val = ptr.field;          // `.` Dereference and member access
```

---

#### 2.2. Unary Operators (Rank 1)
```mylang
count: i32 = 10;

// Pointer Operations
ptr: i32^  = &count;  // `&` Address-of
val: i32   = ^ptr;    // `^` Dereference (Get value)

// Logic & Bitwise Inversion
is_valid: bool = !is_empty; // `!` Logical NOT
mask: u8       = ~flags;    // `~` Bitwise NOT (One's complement)

// Signs
negative: i32  = -val;      // `-` Unary negation
positive: i32  = +val;      // `+` Unary plus
```

---

#### 2.3. Arithmetic (Ranks 2 & 3)
```mylang
// Multiplicative (Rank 2)
area = width * height;
quotient = total / count;
remainder = index % 16;

// Additive & Concatenation (Rank 3)
total = base + offset;
delta = high - low;

// Array Concatenation (`..`)
merged: i32[] = [1, 2] .. [3, 4]; // [1, 2, 3, 4]
```

---

#### 2.4. Bitwise & Shift Operations (Ranks 4, 7, 8, 9)
```mylang
// Shifts (Rank 4)
aligned = size << 2;        // Left shift (multiply by 4)
scaled  = value >> 1;       // Right shift (divide by 2)

// Bitwise Logic (Ranks 7, 8, 9)
masked  = flags & 0x0F;     // `&` Bitwise AND (Rank 7)
toggled = flags ^ 0xFF;     // `^` Bitwise XOR (Rank 8)
combined = flags | 0x80;    // `|` Bitwise OR  (Rank 9)
```

---

#### 2.5. Relational & Equality (Ranks 5 & 6)
```mylang
// Relational (Rank 5)
is_valid = (age >= 18) && (score < 100);

// Equality (Rank 6)
is_match = (status == Status.Ready);
is_active = (state != State.Dead);
```

---

#### 2.6. Logical Operators (Ranks 10 & 11)
```mylang
// Logical AND (Rank 10) - stops if `ptr == null`
if (ptr != null) && (^ptr > 0) {
    process(ptr);
}

// Logical OR (Rank 11) - stops if `is_cached` is true
if is_cached || load_from_disk() {
    render();
}
```

---

## Labels
A label attaches an identifier to a scope or statement so it can be targeted explicitly by control flow statements (`break` and `continue`).

```mylang
// Labeled bare block
process_data: {
    if !load_header() { break process_data; }
    if !load_body()   { break process_data; }
}

// Labeled conditional
check_ready: if is_pending() {
    poll_hardware();
    if has_error() { break check_ready; }
}
```

**Supported labeled statements:** `loop`, `if`, `case`, and scopes `{}`.

**TODO:** flags can only be targeted by control statements its and all upper scopes

---

## Break Statement
The `break`statement allows to exit a statement body early.

Bare `break` exits the nearest enclosing loop statement.
```mylang
loop 0:10 at i {
    if i > 5 { break; } // Exits this loop
}
```

A labeled `break` can exit any outer loop, statement or block from any depth:
```mylang
// Multi-level loop break
outer: loop 0:10 at x {
    loop 0:10 at y {
        if matrix[x][y] == target {
            break outer; // Exits both loops immediately
        }
    }
}

// Breaking an outer case statement
main_menu: case selected_tab {
    when 1 {
        case sub_action {
            when Action.Cancel { break main_menu; }
            when Action.Apply  { save(); }
        }
    }
    else { reset(); }
}
```

---

## Continue Statement
The `continue` statement force loop to advance to the next iteration.

Unlike `break`, `continue` only applies to loops — targeting a non-loop label is compile-time error.
 
Bare `continue` advances the nearest enclosing loop.
```mylang
loop :10 at i {
    if i % 2 == 0 { continue; } // Skips even numbers
    print(i);
}
```

A labeled `continue` directly advances an outer loop from inside nested structures.
```mylang
outer: loop lines as line {
    loop line.tokens as token {
        if is_comment(token) {
            continue outer; // Done with this line, advance outer loop
        }
        process(token);
    }
}
```

---

## Case Statement
The `case` statement provides branch selection using the `when` keyword. 

```mylang
case status {
    when Status.Ok:       handle_ok();
    when Status.Timeout:  retry();
    else:                 log_error();
}
```

Unlike C's `switch`:
* there is no implicit fallthrough between `when` branches — each one exits the `case` on its own once matched.
* `when` conditions doesn't have to be compile-time known.
 
If none condition is satisfied keyword `else` can be used to catch that case.
```mylang
case x {
    when 1 {
        prepare();
        run();
    }
    when get_threshold() {
        alert();
    }
    else {
        idle();
    }
}
```

**TODO:** add fallthrough using `continue`

---

## Ranges

Ranges are defined using the colon `:` syntax following MATLAB-Style. They represent an interval of numbers with an optional stride.

Ranges are universal: the exact same syntax is used for **loop iterations**, **array slicing**, and **memory windowing**.

```text
[start] : [step] : [end]
```

### 1. Syntax & Variations

* **Basic Range (`start:end`):** Defaults to a step of `+1`.
* **Strided Range (`start:step:end`):** Custom increment/decrement.
* **Open Bounds:** Omitted parts use sensible defaults (`start` defaults to `0`, `end` defaults to collection length).

```mylang
0:10        // 0, 1, 2, ..., 9       (10 elements)
:10         // 0, 1, 2, ..., 9       (start defaults to 0)
0:2:10      // 0, 2, 4, 6, 8         (step by +2)
10:-1:0     // 10, 9, 8, ..., 1      (step by -1)
2:          // 2 to the very end of a collection
::2         // Entire collection, skipping every other element
```

### 2. Boundaries: Half-Open `[start, end)`

Ranges are strictly **half-open**: the `start` is included, but the `end` is excluded.

* A range of `0:10` has a length of exactly $10 - 0 = 10$.
* `0:arr.len` touches every valid element without exceeding array boundaries.

### 3. Runtime Evaluation

If a range uses variables or expressions (e.g. `(base + 1):(stride * 2):len`), all boundary terms are **evaluated once** upon entering the slice or loop. Mutating the boundary variables afterward will not affect the active range.

---

## Arrays, Slices & Memory Models

Arrays are linear, contiguous groups of elements of the same kind and almost every operation applied to them happens strictly per-element (per-index). 

### 1. Fixed Arrays & Size Deduction
A standard array owns its data (usually on the stack). If you omit the size inside the brackets `[]`, the compiler will simply deduce it from whatever you assign to it on the right-hand side.

```mylang
i32[3] explicit_arr = [1, 2, 3];
i32[]  inferred_arr = [1, 2, 3]; // Inferred as i32[3]
```

### 2. Per-Element & Destination-Driven Rules
Any operation between arrays of the same size happens index-by-index. If you mix an array with a scalar, that scalar is automatically applied to every element.

```mylang
i32[] arr1 = [1, 2, 3] + [1, 2, 3]; // [2, 4, 6]
i32[] arr2 = [1, 2, 3] * 10;        // [10, 20, 30]
```

Because the compiler knows the destination buffer in advance, it can fuse the entire math equation into a single, loop that writes straight into the LHS. Zero hidden allocations, zero temporary buffers.

### 3. Concatenation
Operations that actually affect the total size, like concatenation, follow the same destination-driven rule. The compiler doesn't allocate a new buffer for the concatenation itself, it just uses sequential `for` loops to write `a` and then `b` directly into the target buffer.

```mylang
i32[] a = [1, 2];
i32[] b = [3, 4, 5];

// 'c' is deduced as i32[5]. 
// The compiler just writes 'a' into c[0:2], then 'b' into c[2:5].
i32[] c = a .. b; // [1, 2, 3, 4, 5]
```

### 4. Slices
Often, you don't want a fixed array—you want to refer to a subsequence of an existing one. You do this using `array[<Range>]`. 

A **Slice** is just a fat pointer, a pointer to the data and a length. It doesn't copy the data. Language supports three types of slices:

```mylang
arr: i32[5] = [10, 20, 30, 40, 50];

// 1. [const] — Locked View
// You can't repoint this slice to look at a different array later.
s_const: i32[const] = arr[:]; 

// 2. [fluid] — Movable View
// You can repoint the descriptor (ptr + len) to look at other data.
s_fluid: i32[fluid] = arr[1:4];
s_fluid = arr[2:5]; 

// 3. [alloc] — The Owner (Dynamic Vector)
// Unlike the other two, this actually talks to an allocator, owns its heap memory, and grows.
v_alloc: i32[alloc] = [1, 2, 3];
```

### 5. Assignment: Descriptor vs. Data Overwrite
Because the default rule is that operations happen *per element*, assignment behaves very intuitively:

*   **`s[:] = <expr>` (Buffer Overwrite):** You are operating on the elements. This copies data into the memory the slice is currently looking at. (Valid for both `[const]` and `[fluid]`).
*   **`s = <expr>` (Descriptor Reassign):** You are rewiring the slice itself. This just updates the `pointer + length` to look at something else. (Only valid for `[fluid]`, because `[const]` locks the descriptor!).

```mylang
s: i32[const] = arr[:];

s[:] = [9, 8, 7]; // OK: Overwrites the actual elements in `arr`.
s = other_arr[:]; // ERROR: `s` is a [const] slice, you can't rewire its descriptor!
```

### 6. Logical Indexing (TODO: Concept)
You can use boolean conditions to index into arrays:

```mylang
i32[] values = [10, -5, 20, -1, 30];

// Zero out all negative numbers instantly:
values[values < 0] = 0; 
// values is now [10, 0, 20, 0, 30]
```

### 7. Unified Interface
Regardless of whether you are working with a fixed array, a `[const]`, `[fluid]` or an `[alloc]`, they all share a standard interface under the hood:
*   **`.data`**: Pointer to the memory (`T^`).
*   **`.length`**: Number of elements.
*   **`.size`**: Total footprint in bytes.

These aren't expensive runtime constraints. They are just a unified interface. For a fixed array, they are filled in at compile-time. For a dynamic slice, they are read from the descriptor at runtime.

---

## Casts

### 1. Value / Static Cast: `expr -> Type`
Transforms data mathematically or structurally. Underlying bit representation **may change**.

```mylang
// Float to Int Truncation
f32 pi = 3.14159;
i32 x = pi -> i32; // 3

// Explicit Array to Slice
i32[3] arr = [1, 2, 3];
i32[const] view = arr -> i32[const]; // (or arr[:])

// Structural Prefix Cast (Packet starts with all fields of Header)
Header: struct { magic: u32; len: u32; }
Packet: struct { magic: u32; len: u32; data: u8[64]; }

Packet pkt = ...;
Header hdr   = pkt -> Header;
```

### 2. Bit-cast / Reinterpretation: `expr => Type`
Raw bit-level reinterpretation (transmute / union punning). Underlying bit representation **dont change**

```mylang
// 1. Inspecting raw float bits
f32 f = 1.0;
u32 raw_bits = f => u32; // 0x3F800000

// 2. Pointer Reinterpretation
u8^ raw_packet = get_buffer();
Header^ hdr = raw_packet => Header^;
```

#### 3. Compile-Time rules:
1. **`->` (Convert):** Valid only between structurally compatible types.
2. **`=>` (Bitcast):** Enforces `sizeof(Source) == sizeof(Target)`.

---


## Loops
There are no separate keywords for `for`, `while`, or `foreach`. There is only one lonely **`loop`**. 

A loop is built from an optional **Target** (a condition, range, or collection) and two optional binding clauses: **`as`** (to name the item) and **`at`** (to track the index).

```text
loop [Target] [as item] [at idx] {
    ...
}
```

Everything after the keyword `loop` is optional. Pick only what you need.

---

### 1. Target Modes

#### A. Infinite Loop
Omit the target entirely to loop indefinitely. Use `break` to exit.
```mylang
loop {
    Message msg = poll();
    if msg.is_quit() { break; }
}
```

#### B. Condition (While)
Pass a `bool` expression to repeat as long as it evaluates to `true`.
```mylang
loop connection.is_alive() {
    process_traffic();
}
```

#### C. Ranges
Generate numeric sequences using <Range>. Boundaries are evaluated **once** at entry.
```mylang
loop 0:10 at i { ... }       // i = 0, 1, 2, ..., 9
loop 0:2:10 at i { ... }     // i = 0, 2, 4, 6, 8 (step by 2)
loop 10:-1:0 at i { ... }    // i = 10, 9, 8, ..., 1 (step by -1)
```

#### D. Arrays & Slices
Pass an array or slice to iterate over its elements.
```mylang
loop [10, 20, 30] as val {
    print(val);
}
```

---

### 2. Binding Items & Indices: `as` and `at`

To avoid dummy placeholders like `_, i`, element aliases and indices use separate keywords.

#### 2.1. as <item> — The Item Alias
`item` is just **l-value alias** directly pointing to `arr[idx]`. There are no hidden copies or borrow-checker acrobatics:
* **Reading** `item` reads `arr[idx]`.
* **Writing** `item = val` modifies `arr[idx]` in-place (if the array is mutable).

```mylang
loop pixels as px {
    px = 0xFF; // Directly writes to the pixel in memory
}
```

Prefixing the alias with `&` resolves alias as `arr + idx` instead.
```mylang
```

#### 2.2. at <idx> — The Loop Index
`at` provides a local, mutable counter starting at `0` (or the range's start boundary). 

You can declare the index with an **explicit type** (`at i32 i`), or omit the type (`at i`) which **defaults to `u64`**. 

Because it is a standard mutable variable, you can freely modify it inside the loop body to advance or skip iterations manually:

```mylang
// 1. Default index type (inferred as u64)
loop tokens as tok at i {
    if tok.is_escape() {
        i += 1; // Manually advance: skips the next token
    }
}

// 2. Explicitly typed index
loop 0:100 at i32 i {
    print(i);
}
```
---

When searching an array, you often want to know where the loop stopped. Prefixing the index with `&` binds the loop counter directly to an existing outer variable:

```mylang
u32 found_at = 0;

loop users as u at &found_at {
    if u.id == target_id {
        break; // `found_at` keeps the exact index of the match
    }
}

"User found at index: " found_at;
```

---

## Functions
A function is a labeled callable block. Function can take are declared using the unified `name: (params) -> ReturnType { ... }` syntax and are capable of returning one value:

```mylang
add: (a: i32, b: i32) -> i32 {
    return a + b;
}
```

### 1. Parameters
Parameters are **always passed by value**. The function receives a local copy of each argument and is free to mutate it:
```mylang
try_increment: (val: i32) {
    val += 1; // Only modifies the local stack copy
}
```

To mutate data pass them via pointer:
```mylang
increment: (ptr: i32^) {
    ^ptr += 1; // Modifies the caller's variable directly
}
```

Arrays and slices are passed as their underlying representation, therefore data are not copied and can be modified.

#### 1.1. Variadic (TODO)
Function can be defined for taking varying number of arguments. Variadic arguments are passed as array of meta type `any`:
```mylang
print: (args: ...) {
    if args.type.kind == any::I64: printI64(args.data -> i64);
}
```

---

### 2. Overloading
Language supports function overloading, allowing multiple functions to share the same identifier as long as their parameter signatures differ. 

#### 2.1. Implicit Resolution (Default)
By default, function call resolution allows implicit type conversions. The compiler will automatically select the best matching variant and apply casts to the arguments if necessary.

```mylang
foo: (i32 x) { ... }
foo: (i8 x) { ... }

foo(1);     // Matches i32 variant
foo(1.0);   // Matches i32 variant (implicit conversion applied)
```

#### 2.2 Strict Resolution (!)
If you need to guarantee that no implicit conversions occur during a call, append the `!` operator to the function name. This forces **strict matching**—the provided argument types must exactly match a function signature, otherwise the compiler will emit an error.

```mylang
foo: (i32 x) { ... }
foo: (i8 x) { ... }

foo!(1);    // OK: Exact match with i32
foo!(1.0);  // ERROR: No strict match found for float (conversion disallowed)
```

### 3. Function Pointers
Function pointer types use the exact same signature syntax, but with parameter names omitted:

```mylang
// Function Definition
multiply: (a: i32, b: i32) -> i32 {
    return a * b;
}

// Declaring a Function Pointer variable
operation: (i32, i32) -> i32 = multiply;

// Calling through the Function Pointer
result: i32 = operation(6, 7); // 42
```

### 4. Defer (TODO)

---

## Error Handling (TODO)
Errors are unique identifiers defined using `error`.
```mylang
error FileError {{
    NotFound;
    PermissionDenied;
}}

fcn read_config(u8[] path) using FileError -> i32 {{
    return _, FileError::NotFound;
}}

i32 result = read_config("test.txt") catch err {{
    if err != null {{
       // handle error
    }}
}};
```

---

## Structs
A `struct` is a contiguous, user-defined composite data type. 

Structs follow the standard memory layout and alignment rules of C. Every field inside the definition is declared as a **complete variable declaration**.

### 1. Defining a Struct
Structs are declared using the uniform `Name: struct { ... }` syntax:

```mylang
Vector2: struct {
    x: f32 = 0.0; // Complete variable declaration with default value
    y: f32 = 0.0;
}

Entity: struct {
    id: u32;           // Uninitialized field
    pos: Vector2;      // Nested struct
    health: i32 = 100; // Field with default value
}
```

### 2. Memory Layout
Struct memory layout is completely transparent and follows the standard memory layout and alignment rules of C (is ABI-compatible):

1. **Sequential Layout:** Fields are placed in memory in the exact order they are declared.
2. **Natural Alignment:** The compiler inserts standard padding between fields to align types to their natural byte boundaries (ex. a `u32` is aligned to a 4-byte boundary).
3. **Zero Hidden Overhead:** There are no hidden virtual tables (vtables), metadata headers, or automatic field reordering.

```text
Memory Layout of Entity (Example):
[ id (4B) ] [ pos.x (4B) ] [ pos.y (4B) ] [ health (4B) ] [ is_active (1B) ] [ padding (3B) ]
Total Size: 20 Bytes
```

### 3. Instantiation & Initialization
Structs can be instantiated using named field syntax xor positional values. Either way unspecified fields defaults to default values:

```mylang
// 1. Named Field Initialization
p1: Entity = Entity {
    id: 1,
    pos: Vector2{ x: 10.0, y: 20.0 },
    health: 75,
};

// 2. Using Default Values
// `pos` and `health` take their default values defined in the struct:
p2: Entity = Entity { id: 2 };

// 3. Positional Initialization (in order of declaration)
p3: Entity = Vector2 { 3, {} };
// `id` = 3, `pos` = { 0, 0 }, `health` = 100
```

### 4. Member Access
Access fields using the dot operator (`.`) which implicitly dereference a pointer if needed:

```mylang
p: Entity = Entity { id: 10 };

// Read and write fields directly
p.pos.x = 42.0;
p.health -= 25;

// Pointer member access
ptr: Entity^ = &p;

// Implicit ^. and simple member access via `.`
ptr.health = 100;
```

---

## 5. Associated Functions (TODO: think of)

---

## Unions (TODO)

---

## Enums
An `enum` defines a distinct type with a set of named integer compile-time constants. 

Enums default to an underlying type of **`i64`**. Values start at `0` and automatically increment by `+1` from the last defined value.

### 1. Defining an Enum
Enums are declared using the uniform `Name: enum { ... }` syntax:

```mylang
// Defaults to underlying `i64` storage
Color: enum {
    Red,    // 0
    Green,  // 1
    Blue,   // 2
}
```

### 2. Auto-Incrementing & Defined Values
If an enum member is given an explicit value, subsequent members continue incrementing from that point:

```mylang
HttpStatus: enum {
    Ok = 200,
    Created,        // 201 (auto-incremented)
    Accepted,       // 202 (auto-incremented)

    BadRequest = 400,
    Unauthorized,   // 401 (auto-incremented)
    Forbidden,      // 402 (auto-incremented)
    NotFound = 404,
}
```

### 3. Custom Underlying Types
You can override the default `i64` by specifying any integer type:

```mylang
// Compact 1-byte enum
LogLevel: enum : u8 {
    Debug = 0,
    Info,
    Warning,
    Error,
}

// 2-byte signed enum
DeviceCode: enum : i16 {
    SensorA = 10,
    SensorB = 20,
}
```

### 4. Member Access
Enum members are strictly namespaced under the enum's identifier and accessed using the dot operator (`.`):

```mylang
current_status: HttpStatus = HttpStatus.Ok;

// Using with `case`
case current_status {
    when HttpStatus.Ok: {
        proceed();
    }
    when HttpStatus.NotFound: {
        log_error("Resource missing");
    }
    else: {
        retry();
    }
}
```

### 5. Integer compatibility
Enums are **implicitly convertible to their underlying integer type**.

When converting an integer literal to an enum, the compiler validates the value at compile-time to ensure it matches a defined enum variant. If an out-of-range value is assigned directly, the compiler raises a compile-time error.

If an unchecked, out-of-bounds, or dynamic runtime conversion is intended, an **explicit cast (`->`)** must be performed:
```mylang
HttpStatus: enum {
    Ok = 200,
    NotFound = 404,
}

// 1. Implicit conversion: Enum -> Integer (Always safe)
code: i64 = HttpStatus.Ok; // Evaluates to 200

// 2. Compile-Time Validation: Integer -> Enum
// status: HttpStatus = 999; // COMPILE ERROR: 999 is not a valid HttpStatus variant

// 3. Explicit Cast: Forces conversion (for runtime or custom values)
raw_code: i64 = 999;
forced_status: HttpStatus = raw_code -> HttpStatus; //  Allowed with explicit `->`
```

---

## The lock Statement
The `lock` statement temporarily overrides a variable for the duration of a lexical scope and its entire call stack.

```mylang
lock variable = new_value;
```

Key Invariants:
* **Read-Only Freeze:** The locked variable becomes **strictly `const`** while the lock is active. It cannot be directly mutated.
* **Call-Stack Inheritance:** Any function called from within the scope inherits the locked value. Child functions can create their own nested `lock`, but cannot mutate the parent's lock.
* **Automatic Unwinding:** When the scope closes (or exits early via `break` or `return`), the variable automatically restores its previous value.

```mylang
render_frame: () {
    // Locks the system allocator to `frame_arena` for this scope
    lock .alloc = frame_arena;

    // All functions called here inherit `frame_arena`
    update_particles(); 
    draw_geometry();

} // .alloc automatically restores to its previous allocator here!
```

---

## Dynamic Memory Allocations (TODO)
Dynamic allocation is not an operator or sub-expression—it is a **dedicated assignment form**. This guarantees that all dynamically allocated memory is immediately bound to a target variable.

### 1. Basic Allocation Syntax
`alloc` allocates memory from the ambient `.alloc` by default and returns a raw pointer (`T^`):

```mylang
// 1. Inferred typed allocation (8 * sizeof(i32))
ptr: i32^ = alloc [8];

// 2. Allocation with immediate initialization (via `:`)
val: i32^ = alloc : 42;

// 3. Dynamic array capacity + initial elements
arr: i32[alloc] = alloc [3]: [10, 20, 30];
```

---

### 2. Explicit Allocator Overrides
To bypass the ambient `.alloc` for a single statement, append the `using` clause:

```mylang
// Forces this buffer to use `os_heap`, ignoring `.alloc`
buffer: u8^ = alloc [4096] using os_heap;
```

---

### 3. Deallocation
`free` is the symmetric counterpart to `alloc`. It releases memory back to either the ambient `.alloc` or an explicitly provided allocator:

```mylang
// Frees using the current ambient .alloc
free ptr;

// Frees using a specific allocator
free buffer using os_heap;
```

### 4. Custom Allocator
```
AllocMode: enum { Alloc; Remap; Free; }

Allocator: struct {
    data: u8^;
    proc: (self: u8^, mode: AllocMode, size: u64, align: u64, old_ptr: u8^, old_size: usize) -> u8^;
}
```

---

## Type Aliases (TODO)
A type alias assigns a new identifier to an existing type. The compiler treats the alias and the underlying type as 100% interchangeable without requiring any cast.

```mylang
UserId:    type = u64;
Timestamp: type = u64;
```

---

## Compile-Time Execution (TODO)
Language provides guaranteed Compile-Time Execution to ensure complex calculations happen during compilation, regardless of optimization flags.

Instead of relying on the compiler to silently optimize math expressions under the hood, you can explicitly enforce compile-time evaluation using the **`embed`** qualifier.

### 1. The `embed` Qualifier
Variables declared with `embed` do not exist in the runtime environment. No memory is allocated for them. Instead, their values are calculated during compilation and directly "embedded" into the final machine code wherever they are referenced.

```mylang
// A standard variable (computed and stored in memory at runtime)
x: i32 = 5 + 3 * 9 - 2;

// A compile-time variable (computed strictly during compilation)
y: embed i32 = 5 + 3 * 9 - 2;
```

### 2. Simple But Powerful
Functions do not need special annotations to be executed at compile time. Any standard function can be *attempted* to be evaluated by the compiler.

To run a function at compile time, you simply assign its result to an `embed` variable:

```mylang
// A completely normal function
add: (i32 a, i32 b) -> i32 {
    return a + b;
}

// Executed by the compiler. 'ans' becomes a hardcoded '6' in the binary.
ans: embed i32 = add(4, 2); 
```

Because `embed` is a strict assertion, the compiler will either successfully compute the value or halt compilation with an error. It will never silently degrade an `embed` variable to runtime execution. 

This strictness provides a massive architectural advantage:
* **High Reusability:** You don't have to write separate runtime and compile-time versions of your functions.
* **Regression Testing:** If a library update introduces a runtime-only dependency into a function you rely on for CTE, the compiler catches it instantly at the `embed` declaration.
* **Easy Fallback:** If a calculation genuinely needs to be deferred to runtime (e.g., the logic changed to require network access), you simply remove the `embed` qualifier (or change it to `const`), and the exact same code will now run at runtime.

---

## Meta (TODO)
```
::AST::Type.size
::AST::lhs.type
```
---

## Imports (TODO)
---

## Native Interoperability

The language allows you to use external libraries. You can use these libraries both during the final run of your program and during **compile-time evaluation**.

### 1. The ABI attribute
The `[<tag>]` attribute tells the Application Binary Interface (ABI) used to load an external library or the convention by which a symbol is exported

The Compiler has an embedded C-ABI support available out of the box. Other ABIs will require a separate plugin. (TODO : To be yet developed...).

### 2. Linking Libraries
To use an external library, you must first register it using the `import` keyword. Note, unlike standard source file imports, desired foreign symbols have to be explicitly declared within the following block to become a part of the program's namespace.

```lang
import [C] <library_name> as namespace <NamespaceName> {
    <function_declarations>
}
```

```
import [C] ucrtbase as namespace LibC {
    fcn pow(f64 x, f64 y) -> f64;
    fcn floor(f64 x) -> f64;
    fcn srand(u32 seed);
}
```

**Naming And Search Rules:**
*   **No Extensions:** Omit `.dll`, `.lib`, `.so`, or `.a`. Correct format is resolved based on the OS.
*   **Search Paths:** The compiler follows standard OS conventions to locate libraries, searching system directories (such as System32 or /usr/lib) and user-defined library paths.

### 3. Standalone Declarations
The ABI attribute can also be applied to individual function definitions to specify which ABI they should follow while being compiled.

```lang
fcn [C] native_callback(int id) {
    "Callback triggered for: %" id;
}
```
