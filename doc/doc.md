# Language Documentation

## 1. Introduction
This document introduces the syntax and semantics of the language. It combines usage documentation, specification, and design rationale without doing any of them particularly well.

---

## 2. Hello, World!
The language does not require an explicit `main` function. Program execution begins top-to-bottom at the start of the source file provided to the compiler.

A minimal "Hello, world!" program can be written directly using a string literal:

```qvi
"Hello, world!"; // Implicitly prints the string
```

A bare string literal is a **print statement**, it prints the literal, with arguments filling the format directives (see Section 13):

```qvi
"Hello, %r!" "world"; // Prints "Hello, world!"
```

### 2.1. Compiling and Running
Compile a source file (e.g., `hello.qvi`) into an executable (`app.exe`):
```bash
compiler build hello.qvi app.exe
```

Compile and run immediately:
```bash
compiler run hello.qvi
```

Emit intermediate representation code without generating a binary:
```bash
compiler translate hello.qvi
```

---

## 3. Lexical Structure
### 3.1. Character Set
Source files are encoded in UTF-8. Unicode characters may be used in specific constructs, such as character and string literals.

### 3.2. Comments
Comments are ignored by the compiler. Two forms are supported:

#### 3.2.1. Single-Line Comments
Single-line comments start with `//` and extend to the end of the line:
```qvi
// This is a single-line comment
```

#### 3.2.2. Multi-Line Comments
Multi-line comments are enclosed in `/{` and `/}`. Unlike C-style comments, they can be safely nested:
```qvi
/{
    This is a multi-line comment.
    /{
        Nested comments are completely valid
    /}
/}
```

### 3.3. Identifiers
Identifiers may contain ASCII letters, underscores, and digits, but may not start with a digit or be a lone underscore.

### 3.4. Keywords
Reserved words cannot be used as identifiers.

```qvi
i8, i16, i32, i64, u8, u16, u32, u64, f32, f64, type,
const, embed, struct, union, if, else, loop, when,
case, enum, return, continue, break, using, alloc,
error, catch, defer, import, as, at, true, false, null
```

### 3.5. Names, Scopes, and Namespaces
A scope dictates the lifetime and visibility of variables and forms a strictly encapsulated namespace.

A new scope is created by a block enclosed in curly braces `{ ... }`. Scopes are usually part of other statements rather than used on their own, such as in control-flow statements or function definitions:

```qvi
if is_valid {
    process();
    save();
}
```

> ***Rationale: No Inline Option.***
>
> *Usually, languages provide a way to follow statements that require a body with a single statement, such as `if (x < 3) x++;` in C. Because this language does not require parentheses, a delimiter is needed to prevent the following statement from blending into the condition expression.*
>
> *In the case of a block, the problem solves itself because it has natural boundaries. For inline statements, however, this is not the case.*
>
> *In general, there are two approaches. One is to use a token as an always-present delimiter, such as `if x < 3 do { x++ }` or `if x < 3 do x++;`. This is uniform as the body can be treated as a statement but it is somewhat tedious to read and write. The other approach is to make an exception for inline statements and use `do` (or any another token) only in that case.*
>
> *The problem here is largely a matter of taste. I guess I just don't feel like it's right to mix a keyword token into a symbol-delimited block. They should match in kind. A good symbol candidate would be `:`, but it clashes with ranges and, to some extent, definitions. And I haven't yet dated any good replacement, so we keep going without it.*

Each scope introduces a fresh namespace. A variable declared within it is visible only in that scope and its children.

A variable declared with the same name as an outer one **shadows** it:

```qvi
count : i32 = 10;

{
    // This shadows the outer 'count'
    count : i32 = 99;
    "%v" count; // Prints: 99

    tmp : i32 = 5;
}

"%v" count; // Prints: 10
// "%v" tmp; -> COMPILE ERROR: 'tmp' does not exist
```

An anonymous scope is fully encapsulated: its contents cannot be accessed or observed from the outside. Attaching a label turns a scope into a **namespace**, whose symbols the outer scope can reach with the `::` operator:

```qvi
config : {
    MAX_SPEED : i32 = 300;
}

"%v" config::MAX_SPEED; // Prints: 300
```

> ***Rationale: Scope Resolution.***
>
> *TODO.*

### 3.6. Structural Constraints
'Declarative' definitions, such as structs, functions and enums, represent the structural blueprint of a program rather than its execution flow. Therefore, these definitions are accessible throughout their scope without ordering constraints, meaning they may be referenced before their declaration. Consequently, any data from parent scopes can be used regardless of its ordering relative to the definition.

Imperative definitions, such as variable definitions, represent actual logic of the program. Therefore, they obey ordering.

---

## 4. Declarations

> The deeper I dug into bytecode-like code, the more I sniffed...
> There is just something about labels. They are simple, but they get the thing done. They are the most straightforward way to name stuff—and there's quite a lot to name, right? So why not use them uniformly to name **everything**? The more orthogonality, the more it leaves a bit of Lisp on the lips, which is never bad idea to lick...

Every named structural element is declared as a label `identifier: <construct>`, the language relies on a single, uniform declaration syntax:
```qvi
// Variables
MAX_BUFFER : u32 = 4096;
count : i32 = 0;

// Structs / Types
Vector2 : type = struct {
    x: f32;
    y: f32;
};

// Callable Blocks (Functions)
add : (a: i32, b: i32) -> i32 {
    return a + b;
}

// etc..
```

---

## 5. Types

Every value or type declaration in the language revolves around a single, uniform syntax:

```qvi
<label> : <type> = <expression>;
```

In value declarations, the concrete type is supplied explicitly:

```qvi
count : i32 = 42;
```

As in C, omitting the initializer (x : i32;) leaves the variable uninitialized.

Because types are **first-class compile-time values**, they live inside this exact same declaration model. Their type is the metatype **`type`**:

```qvi
Integer : type = i32;
```

> ***Rationale:***
>
> *To enable proper meta-interactions with types, there are essentially two approaches: a **template-based** one, where we introduce separate type interactions, or having **types as values**.*
>
> *The first approach effectively introduces a language within the language: types would require separate syntax and a separate set of rules, even if those rules largely overlap with the ordinary system.*
>
> *Since the goal is to keep the language simple, uniform, and somewhat elegant while still providing sufficient expressive power, the 'types as values' path was chosen.*

Since `type` is the root of all types, the `<type>` slot in a declaration **defaults** to `type` when omitted. Leaving the `<type>` slot empty collapses `: =` into `:=`:

```qvi
// The following statements are equivalent:
Integer : type = i32;
Integer := i32;
```

As `:=` shortcut is valid only when the right-hand side is a type expression, the `:` / `:=` split naturally divides variable definitions from type definitions.

Once declared, a new type can be used anywhere a built-in type can:

```qvi
Integer := i32;

// Using our new type to declare a value:
x : Integer = 10;
```

> ***Note:***
>
> *The same rule also applies to composite types like structs, unions, and enums, which are introduced in the following sections.*

### 5.1. Primitive Data Types
Primitives are built directly into the compiler:

| Type | Description | Range / Values |
| :--- | :--- | :--- |
| **`i8`** | Signed 8-bit integer | -128 … 127 |
| **`i16`** | Signed 16-bit integer | -32,768 … 32,767 |
| **`i32`** | Signed 32-bit integer | -2,147,483,648 … 2,147,483,647 |
| **`i64`** | Signed 64-bit integer | -9,223,372,036,854,775,808 … 9,223,372,036,854,775,807 |
| **`u8`** | Unsigned 8-bit integer | 0 … 255 |
| **`u16`** | Unsigned 16-bit integer | 0 … 65,535 |
| **`u32`** | Unsigned 32-bit integer | 0 … 4,294,967,295 |
| **`u64`** | Unsigned 64-bit integer | 0 … 18,446,744,073,709,551,615 |
| **`f32`** | IEEE-754 32-bit floating-point | ≈ ±1.18e-38 … ±3.4e38 |
| **`f64`** | IEEE-754 64-bit floating-point | ≈ ±2.23e-308 … ±1.8e308 |
| **`type`** | Metatype; the type of a type value | Compile-time only (zero runtime size) |

> ***Note:***
>
> *The metatype `type` exists purely at compile time, no qualifier is required when passing types around. Pointer and array types are not primitives — they are formed with decorators (see Section 5.2).*

### 5.2. Decorators
Decorators allow you to declare indirect and compound types such as pointers and arrays.

> ***Note:***
>
> *The term "array" is used broadly here to cover all contiguous sequences (fixed-size arrays and descriptors).*

Type decorators are always placed on the **left** (in front of the base type) and read naturally from left to right:

```qvi
ptr        : ^i32;       // Pointer to i32
arr        : [4]f32;     // Array of 4 f32s
view       : [const]u8;  // Descriptor (fixed view) of u8s

ptr_to_arr : ^[4]i32;   // Pointer to an array of 4 i32s
arr_of_ptr : [4]^i32;   // Array of 4 pointers to i32
```

While type declarations are **left-oriented**, value usage in expressions is **right-oriented**:

```qvi
first : i32 = numbers[0]; // Indexing on the right
val   : i32 = ptr^;       // Dereferencing on the right
```

> ***Note.***
>
> *A raw pointer supports subscripting: `ptr[i]` accesses the element `i` positions past `ptr`, equivalent to `(ptr + i)^` (pointer arithmetic is always in elements)*.

```qvi
buf : ^i32 = malloc(8);
buf[3] = 42;
```

> ***Rationale:***
>
> *Decorators are placed to the left, in front of the type, because it allows for both easier visual scanning and unambiguous formal parsing when types appear inside expressions, without introducing special syntax (such as wrapping types in `<>` brackets).*
>
> *Otherwise, right-side type decorators would constantly collide with value operators in expressions:*
> ```qvi
> // Ambiguous: Array type of 4, or indexing '[4]' into the result?
> y := a -> Type[4];
> ```

### 5.3. Type Aliases
Creating a type alias is simply declaring a symbol of type `type` with a type expression. The alias and the underlying type are freely interchangeable without casting. For distinct types with the same representation, use a single-field struct as it can be converted via layout casts (Section 5.10.2):

```qvi
UserId    := u64;  // Transparent aliases: interchangeable
Timestamp := u64;

// Distinct: does not interconvert with i64 implicitly
Meter := struct { value : i64; };
meter : 2 -> Meter; // Layout cast
```

> ***TODO: Default values on type definitions?***
>
> *Should we allow default values attached directly to type definitions?*
> ```qvi
> Idx := i32 = 0; // Defines a type 'Idx' with implicit initialization to 0
> ```

### 5.4. Arrays And Descriptors

The language provides two sequence representations: **fixed-size arrays** and **descriptors**. Both provide a uniform interface for accessing and manipulating contiguous sequences.

#### 5.4.1. Fixed-Size Arrays: `[N]T` / `[_]T`
A fixed-size array stores its elements inline in contiguous memory. Its length is part of its type, either stated `[N]T` or inferred from the initializer `[_]T`:

```qvi
numbers  : [3]i32 = .[1, 2, 3];
inferred : [_]i32 = .[1, 2, 3]; // Inferred as [3]i32
```

Array literals can pin the element type with the uniform typed-literal syntax `T.[...]` (see the rationale in Section 5.5.3):

```qvi
typed : [3]i32 = i32.[1, 2, 3];
```

Because the length is known at compile time, a fixed-size array incurs zero runtime overhead and does not store its length in memory.

#### 5.4.2. Descriptors: `[]`, `[const]`, `[alloc]`
A descriptor is a fat pointer representing a contiguous region of memory: a `data` pointer plus a `length` (plus allocation metadata for owners). A descriptor value is called a **handle**. The bracket qualifier determines the representation:

| Type | Representation |
| :--- | :--- |
| **`[]T`** | Slice, non-owning descriptor with a rebindable handle. |
| **`[const]T`** | Non-owning descriptor. The handle is fixed and cannot be rebound. |
| **`[alloc]T`** | Descriptor over an owned heap allocation, can grow. |

```qvi
arr : [5]i32 = .[10, 20, 30, 40, 50];

// [const] fixed handle:
s_const : [const]i32 = arr;
// s_const = arr[1:3]; // Error: [const] handle cannot be rebound

// [] rebindable slice:
s : []i32 = arr[1:4];
s = arr[2:5]; // OK

// [alloc] owned allocation:
v_alloc : [alloc]i32 = array_new(_, 3);
```

> ***Note:***
>
> *`const` also exists as a type operator that freezes the **represented data** (`const []T`); `[const]` freezes the **handle** itself (see Section 5.11).*

#### 5.4.3. Descriptor Layout And Construction
Descriptors are ordinary builtin struct types with a documented layout. They are constructed with the same `.{}` literal as any struct, and their fields are the sequence interface itself:

| Field | Present in | Meaning |
| :--- | :--- | :--- |
| **`.data`** | all | Pointer to the first element (`^T`). |
| **`.length`** | all | Number of elements. |
| **`.capacity`** | `[alloc]` | Allocated element count; the descriptor may grow up to it. |
| **`.allocator`** | `[alloc]` | The allocator backing the allocation; defaults to `::allocator`. |

```qvi
// A view over a raw buffer:
v : [const]i32 = .{ .data = buf, .length = 7 };

// An owning descriptor bound to a specific allocator, over memory you acquired yourself:
p : ^i32 = malloc(i32, 16, c_alloc);
o : [alloc]i32 = [alloc]i32.{
    .data      = p,
    .length    = 0,
    .capacity  = 16,
    .allocator = c_alloc,
};
```

Unset fields default to `null`/`0`, so `.{}` is the empty descriptor:

```qvi
empty : []i32 = .{};
```

Fixed-size arrays share the same interface — `.data`, `.length` are synthesized from the type rather than stored.

> ***Note:***
>
> *Construction does not validate pairing: a descriptor with `.length = 7` and null `.data`, or an `[alloc]` wrapped over non-heap memory, are both constructible. Descriptors grant capabilities, they do not enforce them.*

#### 5.4.4. Per-Element Operations
Any operation between arrays of the same size happens index-by-index. If an array is mixed with a scalar, the scalar is applied to every element.

```qvi
arr1 : [_]i32 = .[1, 2, 3] + .[1, 2, 3]; // [2, 4, 6]
arr2 : [_]i32 = .[1, 2, 3] * 10;         // [10, 20, 30]
```

Because the compiler knows the destination buffer, it can fuse the whole expression into a single write loop with zero hidden allocations.

> ***Rationale: TODO.***

#### 5.4.5. Concatenation
Concatenation is a slightly different operator, which applies to arrays as a whole and chains them together. The compiler writes `a` and then `b` directly into the target buffer.

```qvi
a : [_]i32 = .[1, 2];
b : [_]i32 = .[3, 4, 5];

// 'c' is inferred as [5]i32: the compiler writes 'a' into c[0:2], then 'b' into c[2:5].
c : [_]i32 = a .. b; // [1, 2, 3, 4, 5]
```

#### 5.4.6. Assignment And Access
`=` is not treated as an operator. It assigns exactly what the expression denotes. A bare name denotes the **handle**, an indexed expression denotes the **data**.

```qvi
a : []i32 = arr[1:4];

a = arr[2:5];      // Rebinds the handle: 'a' now views a different window
a[:] = .[9, 8, 7]; // Overwrites the data 'a' currently represents
a[0:2] = .[1, 2];  // Overwrites a window of the data
```

> ***Note:***
>
> *For fixed-size arrays, whose handle **is** their data, the two forms coincide (`a[:] = b` and `a = b` both copy).*
>
> *For `[alloc]` descriptors, `=` only copies the descriptor. Both handles will reference same allocation. So, just be cautious.*

Lengths are checked at compile time wherever they are static. At runtime, access is driven by the left side without checks.

#### 5.4.7. Logical Indexing
Conditions can index into arrays:

```qvi
values : [_]i32 = .[10, -5, 20, -1, 30];

// Zero out all negative numbers:
values[values < 0] = 0; // values is now [10, 0, 20, 0, 30]
```

> ***Note:***
>
> *Until bound to a variable, compiler is free to interpret the mask but it shall not allocate.*

### 5.5. Structs
A `struct` is a contiguous, user-defined composite data type.

#### 5.5.1. Defining a Struct
Structs are defined using the `struct { ... }` syntax.

Because a struct definition evaluates to a `type`, it can be used directly as an anonymous type or bound to an identifier:

```qvi
// Anonymous struct type:
vec2 : struct { x : f32; y : f32; };

// Named struct type definition:
Vec2 := struct {
    x : f32 = 0.0;
    y : f32 = 0.0;
};
```

Every entry inside a struct body is either a **field declaration** or a **nested type definition**:

```qvi
Entity := struct {
    // Nested type definition (accessed externally via Entity::Kind):
    Kind := enum u8 {
        Player,
        Enemy,
        Npc,
    };

    // Fields with optional default initializers:
    id   : u64;
    kind : Kind = Kind::Enemy;
    x    : f32  = 0.0;
    y    : f32  = 0.0;
};
```

#### 5.5.2. Memory Layout
Struct memory layout is explicit and C-ABI compatible:
1. **Sequential Layout:** Fields are placed in memory in the exact physical order declared.
2. **Natural Alignment:** Standard padding bytes are inserted to align types to their natural boundaries.
3. **Zero Hidden Overhead:** There are no hidden vtables, runtime type headers, or automatic field reorderings.

```text
Memory Layout of Entity:
[ id (8B) ] [ kind (1B) ] [ padding (3B) ] [ x (4B) ] [ y (4B) ] [ padding (4B) ]
Total Size: 24 Bytes (Aligned to 8 bytes)
```

Layout annotations may deviate from the default:

```qvi
RawHeader : struct [#packed] {
    magic : u32;
    kind  : u16;
    spare : u8;
};  // No padding: size 7, alignment 1

Optimized := struct [#reorder] {
    smallA : u8;
    big    : u64;
    smallB : u8;
};  // Reorder fields to minimize padding: { big, smallA, smallB }
```

- **`#packed`** — no padding bytes. The struct's alignment becomes 1. Fields may be misaligned relative to their natural alignment.
- **`#reorder`** — the compiler may reorder fields to minimize padding. The layout is then implementation-defined. The compiler still knows the layout it chose, so layout casts (Section 5.10.2) and reflection (Section 15.1) reflect the final layout.

> ***Note:***
>
> *If mixed together, `[#packed, #reorder]`, both are applied. Reorder first, packing after.*

#### 5.5.3. Instantiation
Structs can be instantiated using named fields, literals, or rest defaults. Initializers always use the dot (`.`), which explicitly attaches them to the preceding type expression. When the expected type is known, the type may be omitted from the literal:

```qvi
// 1. Named fields (unspecified fields use their defaults):
p1 : Entity = Entity.{ id : 1 }; // kind = Enemy, x = 0.0, y = 0.0

// 2. Inferred literal, with named or positional fields:
p2 : Entity = .{ id : 2 };
p3 : Entity = .{ 3, Entity::Kind::Npc, 10.0, 20.0 };

// 3. Rest defaults (all unset fields take the given value):
p4 : Entity = .{ id : 4, .. : 0 };
```

> ***Rationale: The `.` Literal Prefix.***
>
> *A bare `{` could be confused with a control-flow body (`if <cond> { ... }`). More importantly, `[` is used in type expressions. Since types are values, `[2]i32` is a valid expression, making it difficult to distinguish array literals from types without lookahead.*
>
> *The `.` prefix provides a universal literal disambiguator, unifying structs, arrays, and strings under one syntax: `.{ ... }`, `.[ ... ]`, and `"..."`. Literals can optionally be prefixed with a type expression, as in `Entity.{ ... }`, `i32.[ ... ]`, and `u16."..."`.*

> ***Note:***
>
> *`..` in rest defaults never collides with the concatenation operator (Section 5.4.5) — rest defaults exist only in initializer context, concatenation only in expressions.*

#### 5.5.4. Member Access
Fields are accessed using the dot operator (`.`). Pointers are **automatically dereferenced**:

```qvi
e : Entity = .{ id : 10 };

// Direct access:
e.x = 42.0;

// Pointer access:
ptr : ^Entity = &e;

// Implicit dereference (no '->' or '(ptr^).' needed):
ptr.y = 100;
```

> ***Note:***
>
> *If you need to dereference the pointer itself (e.g. to copy the entire entity value), use the postfix dereference operator: `copied : Entity = ptr^;`.*

### 5.6. Unions
A `union` follows the same declaration rules as a struct. The fundamental difference is that **all fields share the same memory offset (offset zero)**. The total size is the size of the largest member plus alignment padding.

Unions can be declared standalone or placed anonymously inside a struct to form variants:

```qvi
// Standalone Union:
Value := union {
    as_int   : i64;
    as_float : f64;
    as_bytes : [8]u8;
};

// Anonymous Union nested inside a Struct:
Packet := struct {
    id   : u32;
    port : u16;

    // Anonymous union: 'movement', 'chat', and 'raw_data' overlap at offset 0:
    union {
        Movement := struct {
            x : f32 = 0.0;
            y : f32 = 0.0;
            z : f32 = 0.0;
        };

        ChatMessage := struct {
            channel_id : u8;
            text_len   : u8;
            message    : [64]u8;
        };

        movement : Movement;
        chat     : ChatMessage;
        raw_data : [66]u8;
    };

    checksum : u32;
};
```

#### 5.6.1. Member Access in Anonymous Unions
Members of an anonymous union are accessed like ordinary fields of the parent struct:

```qvi
pkt : Packet = .{ id : 1, port : 8080 };

// Access union members directly on the struct:
pkt.movement.x = 10.5;

// Reinterpreting the same memory as raw bytes:
first_byte : u8 = pkt.raw_data[0];
```

### 5.7. Enums

An `enum` defines a distinct type consisting of a closed set of named integer constants:

```qvi
Color := enum {
    Red,    // 0
    Green,  // 1
    Blue,   // 2
};
```

#### 5.7.1. Custom Underlying Backing Types
You can override the default `i64` storage with any integer type after the `enum` keyword:

```qvi
// Compact 1-byte enum:
LogLevel := enum u8 {
    Debug,   // 0
    Info,    // 1
    Warning, // 2
    Error,   // 3
};

// 2-byte signed enum:
DeviceCode := enum i16 {
    SensorA = 10,
    SensorB = 20,
};
```

> ***Note:***
>
> *Enums default to `i64` so their members naturally match the default integer literal type, so their implicit decay to integers (Section 5.7.4) lands on the default type losslessly.*

#### 5.7.2. Explicit Values
Enum members can be assigned explicit values. Unassigned members following an explicit value continue incrementing by `+1`:

```qvi
HttpStatus := enum u16 {
    Ok           = 200,
    Created,            // 201 (auto-incremented)
    Accepted,           // 202 (auto-incremented)

    BadRequest   = 400,
    Unauthorized,       // 401 (auto-incremented)
    Forbidden,          // 402 (auto-incremented)
    NotFound     = 404,
};
```

#### 5.7.3. Namespacing & Matching
Enum members are strictly namespaced under the enum's identifier:

```qvi
current_status : HttpStatus = HttpStatus::Ok;

// Pattern matching with `case`:
case current_status {
    when HttpStatus::Ok {
        proceed();
    }
    when HttpStatus::NotFound {
        log_error("Resource missing");
    }
    else {
        retry();
    }
}
```

> ***TODO: Inferred Member Access***
>
> *For now we have a way to use `_` to infer lvalue type, but it leaves a bit ugly syntax for general case. Also we already have '.' like syntax for literals, so it would be nice to do the same here, but for now bare '::' would collide with 'root' namespace...*

#### 5.7.4. Conversions
Enums **decay implicitly to their backing integer type** — usable directly in math, comparisons, and as indices:

```qvi
status : HttpStatus = HttpStatus::Ok;

code  : u16 = status;        // Implicit decay to the backing type
index : i64 = Color::Red;    // Decay, then widening (Section 5.10.4)
arr[Color::Red] = 10;        // As an index
next  : i64 = status + 1;    // In math; the result is an integer
```

The reverse direction, integer to enum, is explicit. Casting a compile-time-known non-member is a compile error. Runtime values are trusted:

```qvi
ok    : HttpStatus = 200 -> HttpStatus;   // OK
// bad : HttpStatus = 999 -> HttpStatus;  // COMPILE ERROR: 999 is not a member

value    : i64        = get_code();
restored : HttpStatus = value -> HttpStatus;  // Runtime value: unchecked
```

Binary operators on two operands of the same enum type operate on their underlying values, with the result retaining the enum type:

```qvi
Flags := enum u32 {
    Read    = 0b0001,
    Write   = 0b0010,
    Execute = 0b0100,
};

rw     : Flags = Flags::Read | Flags::Write;  // Result carries the Flags type
masked : Flags = rw & Flags::Write;           // 0b0010, still Flags
rw |= Flags::Execute;                         // Compound assignment keeps the type

mixed : u32 = rw & 0b0011;  // Mixed operands decay (above): integer result
```

#### 5.7.5. Enums as Type Values
An enum is a type value like any other, so it carries reflection as fields (Section 15.1), and its members are compile-time constants of the backing type:

```qvi
Foo := enum {
    A,
    B,
    C,
};

count : i64 = Foo.count;  // 3 — number of members
// Foo.members            — the member list
// Foo.backing            — the backing type
```

Because members are compile-time constants, they compose into type positions like any other value:

```qvi
arr : [Foo::A]i32;     // Member value as the length
lut : [Foo.count]i32;  // Member count as the length
```

### 5.8. Generics

To make a generic composite type, you parameterize it. Because types are first-class values, a generic type is simply a compile-time function that returns a `type`:

```qvi
// Explicit function returning a type:
MakePair : (T : type) -> type {
    return struct {
        first  : T;
        second : T;
    };
};
```

As syntactic sugar, you can declare parameterized type constructors attached directly to the type symbol:

```qvi
// Struct constructor accepting a type parameter:
Vec2 := struct(T : type) {
    x : T;
    y : T;
};

// Instantiation:
v1 : Vec2(i32);
v2 : Vec2(f32) = .{ x : 1.0, y : 2.5 };
```

The sugar desugars to the explicit function form — `Vec2` is a function whose type is its signature `(T : type) -> type`:

```qvi
Vec2 : (T : type) -> type {
    return struct {
        x : T;
        y : T;
    };
};
```

> ***Note:***
>
> *Type constructors run exclusively at compile time. It is assumed that every argument to a type constructor is compile-time known by default, so no special annotations are required.*

A function that takes a `type` parameter but returns a **value** is a **generic function**. As the `type` has zero runtime size, it is not propagated to runtime, but function itself may freely appear in runtime code. The compiler simply generates one specialization per distinct type argument.

#### 5.8.1. Destination Inference and the `_` Token
When a type is required but can be cleanly deduced from the surrounding context, you can use the **context token `_`**. This tells the compiler to infer the type from the **destination** (the expected type on the left-hand side), eliminating redundant type declarations.

It is a universal inference mechanic. Because it evaluates to a first-class `type` value dictated by the left-hand side, it can be used as a plain type reference, a cast target, or the base for contextual literals:

```qvi
// 1. As a cast target (infers the cast destination):
f : f64 = 15.0;
i : i32 = f -> _;  // Resolves to 'f -> i32'

// 2. As a full-fledged type value (accessing reflection fields):
ptr : ^i32 = malloc(_, _.align);

// 3. As the base for a Struct Literal (resolves to the whole type):
vec : Vec3 = _.{ x : 1.0, y : 2.0, z : 3.0 }; // '_' becomes Vec3

// 4. As the base for an Array Literal (resolves to the element type):
arr : [3]i32 = _.[10, 20, 30];                // '_' becomes i32
```

Solving unifies the **written** destination type before any implicit conversions apply.

> ***Note on Literals:***
>
> *The base of a literal always pins exactly what the payload cannot. For a struct literal (`_.{...}`), the payload only provides field names, so `_` resolves to the **whole struct type**. For an array literal (`_.[...]`), the payload provides the length and values, so `_` resolves specifically to the **element type**.*

##### Idiomatic Generics: The `_` Default Parameter
The most powerful use of `_` is as a default parameter in generic functions. If a function's return type depends on a generic type `T`, you can set `_` as the default value for `T`. 

When the caller omits the argument, the compiler inserts `_` at the call site, which then deduces the type from the left-hand assignment destination:

```qvi
// T defaults to '_', automatically solving from the caller's destination
default : (T : type = _) -> T { ... }

// Usage:
v : Vec3 = default();  // T is omitted -> T becomes '_' -> solves to Vec3
```

To maximize ergonomics, data-level arguments can be placed first and generic type parameters last. This allows callers to override normal arguments while still letting the compiler implicitly solve the type:

```qvi
malloc : (count : u64 = 1, T : type = _) -> ^T { ... }

// 1. Fully inferred (count = 1, T = f32)
val : ^f32 = malloc();

// 2. Overriding 'count', while 'T' is still inferred
arr : ^i32 = malloc(10);

// 3. Overriding both (no inference needed)
buf : ^u8  = malloc(1, u8);
```
A call whose `_` cannot be solved—either because there is no destination, or the destination does not unify with the signature—is a compile error. Producer generics cannot exist unbound (`malloc(10);` as a bare statement is illegal).

### 5.9. Composition And Flattening

A struct can inherit the fields of another struct using the `using` keyword. Fields of the embedded struct are flattened into the outer struct, and the outer struct becomes implicitly convertible to the inner type:

```qvi
Vec2 := struct {
    x : f32 = 0.0;
    y : f32 = 0.0;
};

Vec3 := struct {
    using Vec2;
    z : f32 = 0.0;
};

v3 : Vec3 = _.{ x : 1.0, y : 2.0, z : 3.0 };

// 1. Direct field access (x and y are flattened into Vec3):
v3.x = 10.0;

// 2. Implicit upcast (Vec3 can be passed wherever Vec2 is expected):
print_vec2 : (vec : Vec2) { ... };
print_vec2(v3);
```

#### 5.9.1. Named Bases
You can optionally bind the embedded struct to a field name. This allows you to interact with the embedded struct as a single cohesive unit or resolve naming conflicts, while still retaining the flattened, direct access to its fields:

```qvi
Player := struct {
    using transform : Vec3;
    health : i32;
};

p : Player = default(_);

// Both access methods are valid:
p.x = 10.0;                  // Flattened access
p.transform = default(_);    // Grouped access as a whole Vec3
```

#### 5.9.2. Position Independence for Casts
Unlike raw layout casts (Section 5.10.2), which strictly require structural prefix matching, an implicit conversion generated by `using` is **position independent**.

The `using` declaration can appear anywhere within the struct definition. When implicitly casting the outer struct to the embedded type, the compiler automatically calculates and applies the correct memory offset, so the subtyping relationship holds regardless of physical memory layout:

```qvi
Collider := struct {
    radius : f32;
    using center : Vec2; // Declared after 'radius' (not at offset 0)
};

c : Collider = _.{ radius : 5.0, center : _.{ x : 1.0, y : 1.0 } };

// Valid: The compiler automatically applies the offset adjustment
// to pass the embedded Vec2.
print_vec2(c);
```

### 5.10. Type Casts and Conversions

The language provides two explicit cast operators, alongside strict rules for implicit conversions:
- **`->` (Value Cast):** Transforms data mathematically or structurally. The underlying bit representation **may change**.
- **`=>` (Bit-Cast):** Reinterprets memory directly at the bit level. The underlying bit representation **never changes**.

#### 5.10.1. Value Cast: `expr -> Type`

Transforms data into a new format. This is used for numeric conversions, enum mappings, and layout-based struct truncation. Any conversion that the compiler can perform implicitly (Section 5.10.4) can also be written explicitly with `->` to the exact same effect.

```qvi
// 1. Float-to-Int Truncation:
pi : f32 = 3.14159;
x  : i32 = pi -> i32; // x = 3

// 2. Width and Sign Conversions:
big   : i64 = 1000;
small : i16 = big -> i16;

// 3. Autocast to the destination's type:
tiny : u8 = big -> _; // Target solved from 'tiny', identical semantics to -> u8
```

#### 5.10.2. Layout-Based Casts
A value may be explicitly cast to (or from) a struct if the target struct's layout matches a prefix of the source type's layout:

```qvi
Vec2 := struct { x : f32; y : f32; };
Vec3 := struct { x : f32; y : f32; z : f32; };

vec3 : Vec3 = .{ 1.0, 2.0, 3.0 };

// Valid: Vec3 starts with f32, f32, which is the same as Vec2
vec2 : Vec2 = vec3 -> Vec2; // vec2 becomes { x : 1.0, y : 2.0 }

// Compile-Time Error: Vec3 is not a prefix of Vec2
bad : Vec3 = vec2 -> Vec3;

// A single-field struct is nominally distinct
// while sharing the representation, so conversions are layout casts:
Meters := struct { value : i64; };

m : Meters = 100 -> Meters;
v : i64   = m -> i64;
```

> ***Note:***
>
> *The rule is purely structural — nominal identity is irrelevant. Any value whose layout aligns with the target struct's prefix qualifies, which makes single-field structs the distinct-type mechanism. Padding and alignment follow the layout rules of Section 5.5.2.*

#### 5.10.3. Bit-Cast / Reinterpretation: `expr => Type`
Raw bit-level reinterpretation (transmute / type punning):

```qvi
// 1. Inspecting raw IEEE-754 float bits:
f        : f32 = 1.0;
raw_bits : u32 = f => u32; // raw_bits = 0x3F800000

// 2. Pointer reinterpretation:
raw_packet : ^u8     = get_buffer();
header     : ^Header = raw_packet => ^Header;
```

Reinterpretation strictly enforces Source.size == Target.size. If the source and target types do not have the exact same byte width, it results in a compile-time error.

#### 5.10.4. Implicit Cast Rules
An implicit cast is the compiler **automatically inserting a `->` conversion** when the operation is guaranteed to be safe and unambiguous.

- **Lossless Numerics.** A source type `S` implicitly converts to `T` only if `T` can represent *every* possible value of `S`.
  - **Allowed:** Widening (`u8 → u16`, `f32 → f64`), safe cross-sign widening (`u8 → i16`), and exact integer-to-float conversions (`i16 → f32`).
  - **Requires explicit `->`:** Anything lossy. Narrowing, float-to-int, sign reinterpretation (`u8 → i8`), and lossy integer-to-float conversions (`i32 → f32`) must be cast explicitly.
- **Enum decay.** An enum implicitly converts (decays) to its backing integer type. Because its values are just a subset of the backing type, this is always lossless.
- **Compile-Time Literals.**  Literals are implicitly converted to any numeric primitive, provided the specific value fits exactly without truncation (value-checked at compile time, see Section 6.1).
- **Struct subtyping.** If a struct embeds another struct via the `using` keyword, an instance of the outer struct implicitly upcasts wherever the embedded struct is expected (see Section 5.9).
- **Array And Descriptors.** Fixed-size arrays implicitly borrow as non-owning descriptors (slices) pointing to their data, and non-owning descriptors can be copied implicitly.

### 5.11. Const
`const` is a type qualifier: `const T` is `T` with its data frozen for modification, composable anywhere a type appears — variables, parameters, struct fields, pointers.

> ***Note:***
>
> *`const` always freezes something; its position determines what. Attached to the type (`const []T`), it freezes the **represented data**. Inside the brackets (`[const]T`), it freezes the **handle** itself.*

```qvi
x : const []i32 = arr;      // Data immutable, handle rebindable
y : [const]i32 = arr;       // Data mutable, handle fixed
z : const [const]i32 = arr; // Both frozen

n : const i32 = 10;         // An immutable integer
```

---

## 6. Literals
Literals are the basic data values of the language.

### 6.1. Integer Literals
Integers can be written in decimal, hexadecimal (`0x`), or binary (`0b`). Underscores (`_`) can be inserted anywhere as digit separators:

```qvi
decimal_val : i32 = 1_000_000;
hex_val      : u32 = 0xFF_00_AA;
binary_val   : u8  = 0b1010_0101;
```

A literal token has no sign. `-` is the negation operator, not part of the number. Therefore, integers are **stored as `u64`**. Their default type is the default integer, **`i64`** (values beyond `i64`'s range default to `u64`). Because literals are compile-time-known, they convert implicitly to any numeric primitive. The value must fit exactly, otherwise it is a compile error. Integers never convert to pointers or `type`:

```qvi
fits : u8 = 200;   // Value-checked narrowing: 200 fits
// bad : u8 = 300; // COMPILE ERROR: 300 does not fit u8
```

### 6.2. Floating-Point Literals
Floating-point numbers must include a decimal point or an exponent. They default to `f64`, as compile-time-known values they convert implicitly to `f32`. A trailing `f` suffix forces the literal to `f32` when an explicit type is needed.

```qvi
pi             : f64 = 3.1415926535;    // Defaults to f64
speed_of_light : f32 = 299_792_458.0;   // Converts to f32 at the declaration
half           : f32 = x + 1.5f;        // Suffix forces f32 inside the expression
scientific     : f64 = 1.5e-4;          // 0.00015
```

### 6.3. Special Constants
A null pointer represents an empty or uninitialized memory address:

```qvi
ptr : ^i32 = null;

if ptr != null {
    ptr^ = 10;
}
```

`true` and `false` are literals for the integer values `1` and `0`, usable with any integer type. There is no separate boolean type:

```qvi
ready : u8 = false;
ready = true;
```

### 6.4. Character Literals
Enclosed in single quotes (`'...'`), character literals represent Unicode code points or packed integer values:

```qvi
char_a  : u8  = 'a';   // 0x61
char_cz : u16 = 'č';   // Unicode code point U+010D

// Packs 4 ASCII characters into a big-endian u32:
mbyte : u32 = 'abcd';  // 0x61626364
```

> ***Note:***
>
> *Multi-byte characters are packed in **big-endian** (left-to-right) order, so `'RIFF'` mathematically equals `0x52494646`. This is designed for fast, single-cycle comparisons of magic numbers and file tags.*

### 6.5. String Literals
String literals are enclosed in double quotes (`"..."`). There is no special string type in the language: a string is simply an array of integers.

By default, a **bare string literal** evaluates to an array of raw UTF-8 bytes of type `[_]u8`:

```qvi
hello : [_]u8 = "Hello";
bytes : [_]u8 = "Манты"; // Raw UTF-8 bytes (array length is > 5)
```

To decode a string into specific codepoints, use the typed dot-prefix (`T."..."`) or the inference dot (`."..."`). This signals the compiler to treat the string as a sequence of code points rather than raw bytes, with `T` defining the maximum code point size (`u8`, `u16`, or `u32`).

This decoding is **value-checked** at compile time: if any character's code point does not fit into the specified element width, compilation fails.

```qvi
// Explicitly typed code points:
text : []u16 = u16."пук";  // OK: 'п' fits in u16
wide : []u32 = u32."hi";   // OK: ASCII fits in u32

// Inference from the destination type:
utf16_text : []u16 = ."čus";
ascii_only : []u8  = ."abc";

// Compile-Time Errors (Width Mismatches):
// bad_u8  : []u8  = ."č";      // ERROR: 'č' (U+010D) does not fit in u8
// bad_u16 : []u16 = u16."😎"; // ERROR: emoji (U+1F60E) does not fit in u16
```

String literals support standard escapes (`\"`, `\\`, `\n`, `\t`, ...), plus `\xhh` (hexadecimal) and `\ooo` (octal, e.g. `\033`) byte escapes.

---

## 7. Operators
Operators are ranked from **0 (highest precedence)** to **12 (lowest)**. Operators of the same rank associate left-to-right; unary operators are prefix.

| Rank | Operators | Names | Associativity |
| :---: | :--- | :--- | :--- |
| **0** | `()` `[]` `.` `^` | Function call, subscript/slicing, member access, dereference (postfix) | Left-to-right |
| **1** | `+` `-` `&` `!` `~` | Unary plus/minus, address-of, logical NOT, bitwise NOT | Prefix |
| **2** | `->` `=>` | Value cast, bit-cast (Section 5.10) | Left-to-right |
| **3** | `*` `/` `%` | Multiplication, division, modulo | Left-to-right |
| **4** | `+` `-` `..` | Addition, subtraction, concatenation | Left-to-right |
| **5** | `<<` `>>` | Bitwise shift left/right | Left-to-right |
| **6** | `<` `<=` `>` `>=` | Relational comparisons | Left-to-right |
| **7** | `==` `!=` | Equality / inequality | Left-to-right |
| **8** | `&` | Bitwise AND | Left-to-right |
| **9** | `^` | Bitwise XOR | Left-to-right |
| **10** | `\|` | Bitwise OR | Left-to-right |
| **11** | `&&` | Logical AND (short-circuiting) | Left-to-right |
| **12** | `\|\|` | Logical OR (short-circuiting) | Left-to-right |

Casts bind tighter than any binary operator and chain left-to-right: `a + b -> T` parses as `a + (b -> T)`.

> ***Note:***
>
> *postfix `^` (dereference) and binary `^` (bitwise XOR) share a token and are disambiguated by context.*

Comparisons and logical operators work on the default integer type `i64`: comparisons yield `1` or `0`, and logical operators treat `0` as false and any other value as true.

Assignment is a statement, not an operator: it cannot appear inside expressions, and any binary operator can be prefixed to it to form a compound assignment (`+=`, `-=`, `*=`, ...).

---

## 8. Ranges
Ranges are defined with the colon (`:`). The same syntax drives **loop iteration**, **array slicing**, and **memory windowing**:

```qvi
[start] : [step] : [end]
```

### 8.1. Syntax & Variations
- **Basic range (`start:end`):** Defaults to a step of `+1`.
- **Strided range (`start:step:end`):** Custom increment/decrement.
- **Open bounds:** Omitted parts use sensible defaults (`start` defaults to `0`, `end` defaults to the collection length).

```qvi
0:10        // 0, 1, 2, ..., 9   (10 elements)
:10         // 0, 1, 2, ..., 9   (start defaults to 0)
0:2:10      // 0, 2, 4, 6, 8     (stops before 10)
10:-1:0     // 10, 9, 8, ..., 1  (stops before 0)
2:          // 2 to the very end of a collection
:2:         // Entire collection, skipping every other element
```

### 8.2. Index Bounds
Ranges are **half-open intervals**, meaning they are inclusive at the `start` and strictly exclusive at the `end` (`[start, end)`).

Because the `end` boundary is excluded, the total number of elements in a standard range is always exactly `end - start`. This maps perfectly to memory offsets, prevents unsigned underflow on empty bounds:

```qvi
buffer : [10]u8 = get_data();

// 1. Slicing the whole array is safe:
all : []u8 = buffer[0:10];

// 2. An empty slice safely evaluates to length 0:
empty : []u8 = buffer[0:0];

// 3. Splitting a buffer exactly in half:
mid   : u64  = buffer.length / 2;
left  : []u8 = buffer[0 : mid];             // Elements 0..4
right : []u8 = buffer[mid : buffer.length]; // Elements 5..9
```

When a custom step is used, iteration generates values strictly as long as the index has not crossed the `end` boundary:
- **Positive step:** Continues while `index < end`. `0:2:5` yields `0, 2, 4` (stops because 6 exceeds 5).
- **Negative step:** Continues while `index > end`. `10:-1:0` yields `10, 9, 8, ..., 1` (steps downward, stopping before 0).

### 8.3. Runtime Evaluation
If a range uses variables or expressions (e.g. `(base + 1):(stride * 2):len`), all boundary terms are **evaluated once** upon entering the slice or loop. Mutating the boundary variables afterward does not affect the active range.

---

## 9. Functions And Error Handling
A function is an ordinary label assigned to a signature. Parameters are **always passed by value**:

```qvi
add : (a : i32, b : i32) -> i32 {
    return a + b;
};
```

To mutate the caller's data, pass a pointer:

```qvi
increment : (ptr : ^i32) {
    ptr^ += 1; // Modifies the caller's variable directly
};
```

Because parameters follow the exact same uniform declaration rules as variables, they can include default initialization expressions (`name : type = value`). A parameter with a default value must trail all non-defaulted parameters. Inside generic functions, these defaults can be type-dependent (see Section 5.8).

### 9.1. Return
`return` yields at most one value. An optional second, error-channel value may be returned with `return _, err;` (see Section 9.6).

### 9.2. Overloading
The language supports function overloading based on parameter signatures.

#### 9.2.1. Implicit Resolution (Default)
By default, call resolution scores every candidate and picks the best. A variant is a **candidate** when:

- **Arity fits.** Fixed-arity variants must match the argument count. Variants must receive at least their fixed parameters, with the rest filling the `...` tail.
- **Return unifies.** If the destination type is known and the return type contains a type parameter, candidates that cannot unify with it are filtered before scoring (Section 5.8).
- **Every argument converts.** Each argument must match its parameter exactly or be implicitly castable. One unconvertible argument disqualifies the variant.

If multiple candidates are valid, the compiler scores them. Scores are calculated in **strict tiers**, a higher-tier match on one argument will always outrank any number of lower-tier matches on other arguments.

Fixed-arity variants receive a massive flat bonus, guaranteeing they beat variadic variants if all else is equal. Then, each argument is scored from best to worst:

| Argument → Parameter | Match Level |
| :--- | :--- |
| Exact type | **Exact** |
| Compile-time literal (value-checked to fit exactly) | **Exact** |
| Any valid implicit cast (lossless widening, struct subtyping, enum decay, descriptor borrow) | **Implicit** |
| Variadic tail argument (`...`) | **Fallback** |

The variant with the highest total score wins. If the top variants share the exact same score, the call is **ambiguous** and results in a compile error.

```qvi
foo : (x : i32) { ... };
foo : (x : f64) { ... };

// 1. Exact Matches
foo(10);          // Exact match: 10 is an integer literal, calls the i32 variant
foo(1.0);         // Exact match: 1.0 is a float literal, calls the f64 variant

// 2. Implicit Casts (Widening)
val : i8 = 5;
foo(val);         // Implicit match: i8 losslessly widens to i32

// 3. Disqualified (Requires Explicit Cast)
big : i64 = 1000;
// foo(big);      // COMPILE ERROR: i64 -> i32 is narrowing (lossy), no implicit cast allowed.
foo(big -> i32);  // OK: Explicit cast forces the i32 variant.
```

#### 9.2.2. Strict Resolution (!)
Append `!` to the function name to guarantee that no implicit conversions occur. The argument types must exactly match one of the signatures.

```qvi
foo!(1);   // OK: exact match with the i32 variant
foo!(1.0); // ERROR: no strict match for a float (conversion disallowed)
```

### 9.3. Function Pointers
A function's signature (e.g., `(i32, i32) -> i32`) represents the physical, compile-time code block. To store a function dynamically at runtime, you use a **function pointer**, created by applying the standard pointer decorator `^` to the signature.

Function pointer types omit the parameter names, keeping only the types:

```qvi
multiply : (a : i32, b : i32) -> i32 {
    return a * b;
};

// 'operation' is a mutable variable holding a pointer to a function
operation : ^(i32, i32) -> i32 = multiply;

// The function pointer can be invoked directly (implicit dereference)
result : i32 = operation(6, 7); // 42
```

#### 9.3.1 Disambiguation
Using the `^` decorator cleanly separates function definitions from function pointer variables, eliminating parsing ambiguity:

```qvi
// 1. Function Definition (Compile-time code block)
foo : (x : i32) -> i32 { return x * 2; };

// 2. Abstract Declaration (e.g., inside an interface or FFI block)
bar : (x : i32) -> i32;

// 3. Function Pointer Variable (Runtime state, initialized to null)
baz : ^(i32) -> i32 = null; 

// Mutating the runtime pointer:
baz = foo;
```

### 9.4. Variadics
Variadic parameters are declared using `...` and allow a function to accept an arbitrary number of arguments.

Behind the scenes, the compiler automatically packages each argument into a built-in `::Any` struct (see Section 15.1.2), containing a pointer to the value's metadata (`^::TypeInfo`) and a raw pointer to the data itself.

The variadic parameter is then exposed to the function simply as a descriptor of these bundles (`[]::Any`).

```qvi
log_values : (args : ...) {
    // 'args' behaves exactly like []::Any
    loop args as arg {
        if arg.info.kind == ::TypeKind::I64 {
            // Reinterpret the raw data back to a usable pointer:
            val_ptr : ^i64 = arg.data => ^i64;
            printf("I64: %v\n", val_ptr^);
        } else {
            printf("Other type (Size: %v)\n", arg.info.size);
        }
    }
};

// Usage:
log_values(42, 3.14, "Error");
```

### 9.5. Local Functions
Functions may be defined inside the body of another function. A local function exists strictly within the enclosing stack frame:

```qvi
foo : (x : i32) {
    total : i32 = 0;

    // Local subroutine: executes within foo's stack frame
    add : (val : i32) {
        total += val + x; // Directly accesses outer variables
    };

    add(10);
    add(20);
};
```

A local function that does not reference any outer variables can be captured into a standard function pointer. However, **taking the address of a local function that captures enclosing stack variables is a compile-time error**.

A local function that references no enclosing variables is a plain function and may be stored and called freely.

### 9.6. Error Handling (TODO)
Errors are treated simply as another state of the program (although special one).

#### 9.6.1. Error Sets
`error` is the unified error type. Its values are named error identifiers with underlying integer representation. 0 is reserved for no-error, so comparing against `null` is the error test.

Errors are organized into **error sets** — type values defined with the error constructor:

```qvi
ReadFile := error {
    NotFound;
    PermissionDenied;
};

WriteFile := error {
    Locked;
    DiskFull;
};

FileOps := error {
    ReadFile;   // Includes every member of ReadFile
    WriteFile;
};
```

A set nested inside another contributes all of its members. Members are addressed with the scope operator (error sets are namespaces specialized for errors) and their values have the unified type:

```qvi
err : error = FileOps::ReadFile::NotFound;
err = null;
```

Sets are open for extension. New members can be added without breaking code that uses the original set, since the qualified paths of existing members are untouched. An identifier may also carry an explicit integer value (like an enum member), so error codes can index arrays through an explicit cast.

#### 9.6.2. Function Signatures And Returning Errors
A function explicitly declares the set of errors it can return using the `using` keyword in its signature.

Errors are returned alongside the normal return value via `return value, err;`. If the function fails before producing a meaningful value, the discard token `_` is used to skip the primary return initialization, giving the programmer control over whether a dummy value needs to be constructed.

```qvi
read_file : (path : [const]u8) using FileError -> []u8 {
    if !exists(path) {
        return _, FileError::NotFound; // Discard value, return error
    }
    return get_data(path), null;       // Return value, no error
};
```

#### 9.6.3. The `catch` Syntax
A function has exactly one return value, an error is returned through a separate channel, not as a second return value. An error is an ordinary program state, so it travels like any value, with no hidden control flow (unlike exceptions).

> ***Rationale: why not a second return value?***
>
> *Making the error a second return value — Go's (value, error) — forces every call to resolve two outputs, which ends in either implicit rules or verbose syntax.*
> ```go
> val1, err := foo();
> val2, err := foo();  // err is NOT redeclared — a special := rule
> ```
> *Here err is syntactically an ordinary second return value, but semantically it gets special treatment under `:=` purely because of its type — a conflict of syntax and semantics. Qualifiers make it worse: marking `val1` `const` but not `err` demands either verbose syntax or yet more implicit rules.*

Errors are therefore captured on the right side of an expression. This syntactically separates the primary statement from the error handling logic.

There are three ways a `catch` statement can accomplish this.

##### Catch To Variable
Captures the error into an existing variable in the current scope.

```qvi
err : FileError = null;
data : []u8 = read_file("config.txt") catch err;

if err != null { ... }
```

##### Catch Block
Captures the error into a new identifier that is valid *only* within the immediately following block. The block is only executed if an error actually occurred.

```qvi
data : []u8 = read_file("config.txt") catch e {
    // 'e' is only valid here, and is guaranteed to be != null
    if e == FileError::NotFound {
        log("File missing");
    }
    return;
};
```

##### Automatic Propagation
If a function wants to immediately pass an error up the call stack to its caller, it can use `catch return`.

```qvi
process : () using FileError {
    // If read_file fails, process() immediately returns _, err
    data : []u8 = read_file("config.txt") catch return;
};
```
> ***Note:***
>
> *`foo() catch return;` is syntactic sugar for `foo() catch err { return _, err; }`.*

#### 9.6.4. State Only
The error channel carries pure state, not data — an error member cannot hold a payload (like a string or a struct).

> ***Rationale.***
>
> *Generalizing the model (e.g. allowing a struct or union as the error) would contradict the standardization goal: the system becomes a general-purpose tool with many uses, and the problem moves rather than resolves.
>
> *In practice the limitation bites only when logging — descriptions are concrete matters, not standardizable ones. Everywhere else in the code, the pure state is exactly the necessary description of the program's flow. When a state genuinely deserves to be an error, the correct tool is a new member, not a payload.*

---

## 10. Control Flow
Control flow consists of loops, conditional statements, and labeled constructs.

### 10.1. If and Else
`if` executes its body when the condition holds, `else` runs otherwise. Bodies are blocks, attached directly (see Section 3.5):

```qvi
if x > 0 {
    positive();
} else if x < 0 {
    negative();
} else {
    zero();
}

if ready {
    start();
} else {
    retry();
}
```

### 10.2. Break Statement
The `break` statement allows exiting a statement body early.

Bare `break` exits the nearest enclosing loop statement.

```qvi
loop 0:10 at i {
    if i > 5 { break; } // Exits this loop
}
```

A labeled `break` can exit any outer loop, statement or block from any depth:

```qvi
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
            when Action::Cancel { break main_menu; }
            when Action::Apply  { save(); }
        }
    }
    else { reset(); }
}
```

### 10.3. Continue Statement
The `continue` statement forces a loop to advance to the next iteration.

Unlike `break`, `continue` only applies to loops — targeting a non-loop label is compile-time error.

Bare `continue` advances the nearest enclosing loop.

```qvi
loop :10 at i {
    if i % 2 == 0 { continue; } // Skips even numbers
    print(i);
}
```

A labeled `continue` directly advances an outer loop from inside nested structures.

```qvi
outer: loop lines as line {
    loop line.tokens as token {
        if is_comment(token) {
            continue outer; // Done with this line, advance outer loop
        }
        process(token);
    }
}
```

### 10.4. Case Statement
The `case` statement provides branch selection using the `when` keyword.

```qvi
case status {
    when Status::Ok      { handle_ok(); }
    when Status::Timeout { retry(); }
    else                 { log_error(); }
}
```

Unlike C's `switch`:
- there is no implicit fallthrough between `when` branches — each one exits the `case` on its own once matched.
- `when` conditions don't have to be compile-time known.

If none condition is satisfied, the keyword `else` can be used to handle that case.

```qvi
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

> ***TODO:***
>
> *add fallthrough using `continue`*

### 10.5. Loops
There is a single `loop` construct, built from an optional **target** (a condition, range, or collection) and two optional binding clauses: **`as`** (item alias) and **`at`** (index tracker).

```qvi
loop [target] [as item] [at idx] {
    ...
}
```

#### 10.5.1. Target Modes
- **Infinite loop:** Omit the target entirely, use `break` to exit.
    ```qvi
    loop {
        tok : Token = nextToken();
        if tok.kind == TK_END { break; }
    }
    ```
- **Condition:** Pass a condition expression, the loop repeats as long as it is true (non-zero).
    ```qvi
    loop connection.is_alive {
        process(connection);
    }
    ```
- **Ranges:** using half-open intervals `[start, end)`. Boundaries are evaluated **once** at entry (see Section 8.3).
    ```qvi
    loop 0:10 at i { ... }     // i = 0, 1, 2, ..., 9 (10 iterations)
    loop 0:2:10 at i { ... }   // i = 0, 2, 4, 6, 8 (step by 2, stops before 10)
    loop 10:-1:0 at i { ... }  // i = 10, 9, ..., 1 (step by -1, stops before 0)
    ```
- **Arrays And descriptors:** Pass an array or descriptor to iterate over its elements.
    ```qvi
    arr : [3]i32 = i32.[10, 20, 30];
    loop arr as val {
        "%v" val;
    }
    ```

#### 10.5.2. Binding Items And Indices: `as` and `at`
- **`as <item>` — item alias:** `item` is a direct l-value alias of `arr[idx]`; writing to it modifies the array element in place.
    ```qvi
    loop pixels as px {
        px = 0xFF; // Directly writes to the pixel in memory
    }
    ```

- **`at <idx>` — loop index:** A local, mutable counter. It can be declared with an explicit type (e.g. `at i: i32`) or without one (defaults to `u64`). Being a standard mutable variable, it can be advanced or skipped manually inside the loop body.

```qvi
// Default index type (u64):
loop tokens as tok at i {
    if is_escape(tok) { i += 1; } // Manually advance: skips the next token
}

// Explicitly typed index:
loop 0:100 at i: i32 {
    "%v" i;
}
```

When searching, prefixing the index with `&` binds the loop counter to an existing outer variable, capturing the exact index of an event:

```qvi
found_at : u32 = 0;

loop users as u at &found_at {
    if u.id == target_id { break; } // 'found_at' keeps the exact index of the match
    // found_at is still incremented at the end as normal index
}

"User found at index: %v" found_at;
// found_at is still incremented at the end as normal index
```


---

### 10.6. The `defer` Statement
The `defer` statement schedules a statement or block to execute automatically when execution leaves the current enclosing scope.

```qvi
process_file : (path : [const]u8) {
    f : ^File = open(path);
    defer close(f); // Guarantees 'close' runs whenever this scope exits

    if has_error() {
        return; // 'close(f)' is automatically executed here
    }

    // Multiple statements:
    defer {
        log("Done");
        cleanup();
    }
};
```

Deferred statements are executed in **reverse order of appearance**.
Deferring is resolved entirely at compile time by inlining the deferred code at every exit point.

---

## 11. Memory
The compiler has no allocation vocabulary. There are no builtin `alloc`/`free` intrinsics. Only the *contract*, the `Allocator` interface, is specified, which should be a sufficient stone block for any userland memory managing function.

### 11.1. The Allocation Interface
An allocator is any `Allocator` value — a struct carrying its state and a raw byte-level
procedure:

```qvi
AllocMode := enum u8 {
    ALLOC,
    FREE,
    RESIZE,
};

Allocator := struct {
    data : ^u8;
    proc : ^(self : ^u8, mode : AllocMode, size : u64, align : u64,
            old_ptr : ^u8, old_size : u64) -> ^u8;
};
```

**Contract:**
- **`ALLOC`:** `old_ptr`/`old_size` are ignored; returns a `size`-byte block aligned to
  `align`, or `null`.
- **`RESIZE`:** grows or relocates the block `old_ptr` (of `old_size` bytes) to `size`
  bytes, preserving contents; returns the new address, or `null` leaving the block intact.
  `old_size` must be exact.
- **`FREE`:** releases the block `old_ptr`. `old_size` is advisory — an allocator must
  tolerate `0` (unknown); owning descriptors always pass the exact value
  (`capacity * T.size`), so size-aware allocators get their best behavior through the
  descriptor path, not the raw path.

TODO: name for default allocator
`::allocator` (Section 15.3) is the default allocator — an ordinary mutable `Allocator`
value in the compiler namespace, used as the default argument throughout the library.

---

## 12. Native Interoperability
External libraries can be used both at runtime and during compile-time evaluation.

### 12.1. The ABI Attribute
The `[<tag>]` attribute specifies the ABI used to load an external library, or the convention by which a symbol is exported.

```qvi
native_callback : [C] (id : i32) {
    "Callback triggered for: %v" id;
};
```

The Compiler has an embedded C-ABI support available out of the box. Other ABIs will require a separate plugin. (TODO : To be yet developed...).

### 12.2. Linking Libraries
To use an external library, register it with the `import` keyword. Unlike standard source file imports (see Section 16), you need explicitly declare needed foreign symbols:

```qvi
import [C] <library_name> as <NamespaceName> : {
    <function_declarations>
}
```

Example:

```qvi
import [C] ucrtbase as LibC : {
    pow   : (x : f64, y : f64) -> f64;
    floor : (x : f64) -> f64;
    srand : (seed : u32);
}
```

**Naming and Search Rules:**
- **No Extensions:** Omit `.dll`, `.lib`, `.so`, or `.a`.
- **Search Paths:** The compiler follows standard OS conventions to locate libraries.

---

## 13. Format Strings
Format strings can be denoted using meta qualifier `#format` to preserve meaning of such string through functions calls or variable assignment in general. Its purpose is to trigger compile-time validation of the format string and its associated arguments when the time comes:

```qvi
printf : (fmt : [#format]u8, args : ...) { ... };
```

Format strings are composed of literal text and `%`-prefixed directives. There are exactly three formatting patterns:

1. **`%%`** — Escapes the `%` character, printing a literal `%`.
2. **`%<option>`** — A 1-character shortcut for common, simple formatting (e.g., `%v`, `%x`, `%t`).
3. **`%{Options:Indent:MaxWidth:Precision}`** — A full formatting block.

> ***Rationale.***
>
> *The syntax is explicitly designed so that every directive requires either a strict 1-character option flag (`%v`) or a braced block (`%{...}`). This makes format strings deterministically parsable with zero ambiguity. Furthermore, by anchoring directives with `%` instead of bare braces, the syntax pollutes only a single symbol in the literal text space.*

### 13.1. The Full Format Block
The full format block allows precise control over the output. It is divided into four fields separated by colons: `%{Options:Indent:MaxWidth:Precision}`.

All sub-fields are optional, meaning trailing or empty fields can be omitted:
- `%{R+:1:10}` — Options `R` and `+`, indent `1`, max width `10`.
- `%{x::16}` — Hexadecimal option, default indent, max width `16`.
- `%{::20}` — Default value formatting, max width `20`.

### 13.2. Option Flags
The first field of a format block (or the single character in a shortcut) defines the structural transformation of the data. Options are 1-character flags that can be combined in any order within `%{...}`.

Because formatting is type-aware, you never have to specify the underlying data type.

| Flag | Name | Description |
| :---: | :--- | :--- |
| **`v`** | Value | Default type-inferred value formatting (implicit if omitted). |
| **`t`** | Type | Emits the type name of the argument instead of its value. |
| **`R`** | Right Align | Right-aligns output within the specified `MaxWidth`. |
| **`L`** | Left Align | Left-aligns output within the specified `MaxWidth`. |
| **`+`** | Force Sign | Forces a `+` sign for positive numeric values. |
| **`x`** | Hexadecimal | Formats integers or pointers in hexadecimal notation. |
| **`b`** | Binary | Formats integers in binary notation. |
| **`r`** | Raw | Prints the argument as raw data, treating its memory directly as a raw byte string. |
| **`#`** | Pretty | Pretty-prints structs and complex collections with line breaks. |
| **`<`** | Crop | Truncates the output to `MaxWidth` instead of overflowing it. |

```qvi
// Examples:
"Hello, %v!" "world";         // Shortcut: "Hello, world!"
"Memory: %x" address;         // Shortcut: "Memory: 0x7FFA"
"Total: %{R+:0:8}" 42;        // Block:    "Total:      +42"
"Data: %{#}" config;          // Block:    Pretty-prints the config struct
```
### 13.3. The Print Statement
The language provides syntactic sugar for formatted text output. When a statement begins directly with a string literal, it is evaluated as a **print statement**. Any expressions following the string literal are passed as arguments to fill its format directives.

```qvi
// The print statement:
"Hello, %v!" "world";

// Multiple arguments are comma-separated:
"Point: (%v, %v)" x, y;
```

#### 13.3.1. Desugaring and `::print`
Under the hood, a print statement is not a hardcoded compiler intrinsic. It is purely syntactic sugar that translates directly into a call to the `::print` function pointer located in the compiler root namespace:

```qvi
// The print statement:
"Point: (%v, %v)" x, y;

// Desugars exactly to:
::print("Point: (%v, %v)", x, y);
```

#### 13.3.2. Redirection and Rebinding
Because `::print` is an ordinary mutable function pointer in the compiler namespace (much like the `::allocator` allocator), userland code can freely rebind it. 

This allows you to globally redirect all print statements in your program to a custom destination without changing any of the syntax at the call sites.

```qvi
// The default compiler-provided print pointer:
// ::print : ^(fmt : [#format]u8, args : ...) = default_os_print;

custom_logger : (fmt : [#format]u8, args : ...) {
    // Custom routing logic (e.g., write to a file or UART)
    write_to_file(log_file, fmt, args);
};

setup_system : () {
    // Rebind the global print statement destination:
    ::print = custom_logger;
    
    // This now seamlessly routes through custom_logger:
    "System booted successfully at %v" get_time();
};
```

---

## 14. Compile-Time Execution
Compile-time execution (CTE) guarantees that a calculation happens during compilation, regardless of optimization flags. Rather than relying on silent optimization, CTE is enforced explicitly with the **`embed`** qualifier.

### 14.1. The `embed` Qualifier
Variables declared with `embed` do not exist at runtime. Their values are calculated during compilation and embedded into the final code wherever referenced. `embed` is part of the type (`embed i32`), so the uniform declaration syntax is preserved.

**Standard variable (runtime):**

```qvi
x : i32 = 5 + 3 * 9 - 2;
```

**Compile-time variable (CTE):**

```qvi
y : embed i32 = 5 + 3 * 9 - 2;
```

> ***Note:***
>
> *`embed` is the sufficient entry point for CTE. It can run arbitrary functions and therefore any language construct (see Section 14.2).*

### 14.2. Calling Functions at Compile Time
Functions need no special annotations to be executed at compile time — simply assign the result to an `embed` variable:

```qvi
// A completely normal function
add : (a : i32, b : i32) -> i32 {
    return a + b;
};

// Executed by the compiler: 'ans' becomes a hardcoded '6'
ans : embed i32 = add(4, 2);
```

---

## 15. Meta
Because the language treats types as first-class values, metaprogramming does not require a separate macro language or string-replacement preprocessor. It is simply standard language code interacting with compiler-provided data.

### 15.1. Type Reflection
Since types are first-class values, compile-time reflection is just standard property access using the `.` operator. Reflection can be used at compile time or materialized at runtime for dynamic behavior.

#### 15.1.1. Compile-Time Properties
Every type shares a uniform set of foundational properties, which are expanded by proprietary fields depending on the type's `kind`.

##### Foundational Properties (All types)

```qvi
k : TypeKind = i32.kind;  // The fundamental category (Struct, Enum, Pointer, etc.)
s : u32      = i32.size;  // Size in bytes
a : u8       = i32.align; // Alignment requirement in bytes
r : u8       = i32.rank;  // Type rank (used internally for numeric promotions)
```

##### Type-Specific Properties
When a type is known to be a specific composite, it exposes proprietary fields mirroring the compiler's internal structures:

```qvi
// Structs
Entity.name;     // The struct's identifier as a string
Entity.members;  // Array of struct fields: { name: String, type: type, offset: u64 }

// Enums
HttpStatus.count;    // Total number of members
HttpStatus.members;  // Array of members: { name: String, value: i64 }
HttpStatus.backing;  // The backing integer type (e.g., u16)

// Functions, Arrays, and Pointers also expose `.element`, `.args`, `.ret`, etc.
```

#### 15.1.2. Runtime Type Information
The intrinsic `::type(expr)` evaluates an expression and returns its static **`type`**. Like the properties above, this resolves entirely at compile time.

However, building dynamic runtime systems requires types to have a physical presence in memory. To solve this, **any `type` value implicitly casts to `^::TypeInfo`**:

```qvi
info : ^::TypeInfo = i32;
```

**The Built-in `::Any` Type**
Because pairing "raw data" with "runtime type information" is such a fundamental pattern, the language provides the built-in `::Any` struct:

```qvi
::Any := struct {
    data : ^u8;
    info : ^::TypeInfo;
};
```

This struct is exactly what the compiler uses under the hood to power **variadic arguments** (Section 9.4).

You can also construct and use `::Any` manually to build dynamic containers or custom runtime reflection logic:

```qvi
make_any : (value : ^f32) -> ::Any {
    return _.{
        data : value => ^u8,
        info : f32, // 'f32' implicitly casts to ^::TypeInfo
    };
};

// Ex. Usage:
x   : f32 = 3.14;
dyn : ::Any = make_any(&x);

// Inspecting the type at runtime:
if dyn.info.kind == ::TypeKind::Float {
    val_ptr : ^f32 = dyn.data => ^f32;
    printf("It's a float: %v\n", val_ptr^);
}
```

### 15.2. Compile-Time Control Flow
Sometimes you need to conditionally include code or unroll operations based on compile-time data. This is done by prefixing any control-flow statement with `#`. This forces them to resolve their 'flow' at compile time.

#### 15.2.1. `#if` (Conditional Compilation)
The `#if` statement evaluates a compile-time condition. Only branch that evaluates to true is compiled into the program. False branches are entirely discarded.

```qvi
add : (T: type, a : T, b : T) -> T {
    // Generates completely different code based on the type:
    #if T.is_integer {
        return a + b;
    } else #if T.is_pointer {
        return ptr_offset(a, b);
    } else {
        ::error("Unsupported type passed to add()");
    }
};
```

#### 15.2.2. `#loop` (Compile-Time Unrolling)
`#loop` behaves exactly like a standard loop (Section 10.5), but it unrolls its body at compile time. It is heavily used with reflection to iterate over fields or enum members to generate code dynamically.

```qvi
// Automatically generates a print block for every member of an enum:
print_status : (status : HttpStatus) {
    #loop HttpStatus.members as member {
        if status == member.value {
            printf("Status is: %v", member.name);
            return;
        }
    }
    printf("Unknown status");
};
```

> ***Note:***
>
> *Because `#loop` expands code at compile time, its target must be a compile-time known collection, such as a range or a type reflection array.*

### 15.3. The Compiler Namespace
Compiler-provided facilities are accessed through the root namespace `::`. There are no bare built-in functions polluting the global scope.

Common root facilities include:
- **`::type(expr)`** — Yields the `type` of the expression.
- **`::node(expr)`** — Yields the AST node of the expression.
- **`::error(msg)`** — Immediately halts compilation and prints a custom error message.
- **`::allocator`** — Default allocator.

> ***Note:***
>
> *Metaprograms may inspect existing structures and create new ones, but they may not mutate existing declarations or AST nodes once they are resolved.*

---

## 16. Imports
The `import` statement acts as a **structural parsing directive**. The importing file dictates exactly *how* that AST block is wrapped, scoped, typed, and exposed within the current program, using the standard uniform declaration syntax:

```qvi
import <file_name> as <declaration>
```

By changing the declaration type, you change the structural container of the imported code.

### 16.1. The Parsability Constraint
Unlike C's `#include`, which performs blind text substitution, `import` operates strictly on the Abstract Syntax Tree (AST). 

Because of this, **every imported file must be independently parsable.** A file must contain a syntactically valid sequence of statements, declarations, or a comma-separated list of expressions. You cannot import a file containing mismatched braces, dangling operators, or half-finished control-flow statements.

```qvi
// file_a.qvi
if condition {

// file_b.qvi
    process();
}

// COMPILE ERROR: Neither file_a nor file_b are valid ASTs on their own. 
```

### 16.2. Namespaces (Default Wrapping)
If you provide only an identifier, the compiler automatically encapsulates the imported file's AST into an opaque namespace of that name. This is the standard way to consume external code without polluting the local scope.

```qvi
// Evaluates 'math_utils.qvi' and wraps it inside the 'Math' namespace:
import math_utils as Math;

// Usage:
result : f32 = Math::sqrt(16.0);
```

### 16.3. Scope Flattening (`_`)
If you import a file `as _`, you are telling the compiler to unpack the file's AST directly into the **current scope**, rather than wrapping it in a new namespace. 

```qvi
// Dumps the AST of 'math_constants.qvi' directly into the current file's scope:
import math_constants as _;

// Usage (no namespace prefix required):
area : f32 = PI * radius * radius;
```

### 16.4. Structural Builders
Because there is no main function in language, list of statements is a valid standalone AST block. Therefore, you can import a file directly into a data structure definition. The importing file dictates the type and layout, and the imported file merely provides the body. 

```qvi
import tcp_header as TcpPacket : struct [#packed];
```

**`tcp_header.qvi`** (A parsable block of declarations):
```qvi
source_port : u16;
dest_port   : u16;
sequence    : u32;
ack_num     : u32;
```

### 16.5. Function Bodies
You can declare a function signature and use the imported file directly as its body. The code inside the file executes as if it were written directly inside the curly braces of the function, gaining full access to the defined parameters.

```qvi
// Wraps 'process_data.qvi' into a function named 'process':
import process_data as process : (data : []u8, factor : f32) -> u64;

// Usage:
result : u64 = process(my_buffer, 1.5);
```

**`process_data.qvi`**:
```qvi
total : u64 = 0;
loop data as byte {
    total += byte * factor -> u64;
}
return total;
```

### 16.6. Explicit Interfaces (Restricted Imports)
If you want to strictly control what an imported namespace is allowed to expose, you can declare an explicit struct-like interface block.

The imported file must contain matching definitions for the requested symbols, and *only* those explicitly declared symbols will be visible to the importing file. Everything else in the imported file remains strictly inaccessible.

```qvi
// Wraps 'physics.qvi' into 'Sim', but strictly filters the API:
import physics as Sim : {
    step    : (delta : f32);
    gravity : Vec3;
}

// Valid:
Sim::step(0.016);

// COMPILE ERROR: 'apply_force' exists in physics.qvi, 
// but is not exposed through the requested interface above.
// Sim::apply_force(player); 
```

### TODO: add `[#raw]`?

---











## TODO: The Lock Statement
The `lock` statement temporarily overrides a variable for the duration of a lexical scope and its entire call stack.

```qvi
lock variable = new_value;
```

Key Invariants:
- **Read-Only Freeze:** The locked variable becomes **strictly `const`** while the lock is active. It cannot be directly mutated.
- **Call-Stack Inheritance:** Any function called from within the scope inherits the locked value. Child functions can create their own nested `lock`, but cannot mutate the parent's lock.
- **Automatic Unwinding:** When the scope closes (or exits early via `break` or `return`), the variable automatically restores its previous value.

```qvi
render_frame : () {
    // Locks the system allocator to `frame_arena` for this scope
    lock .alloc = frame_arena;

    // All functions called here inherit `frame_arena`
    update_particles();
    draw_geometry();

} // .alloc automatically restores to its previous allocator here!
```
