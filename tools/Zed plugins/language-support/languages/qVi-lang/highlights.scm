; ====================================================================
; Keywords
; ====================================================================

[
  "if"
  "else"
  "loop"
  "case"
  "when"
  "return"
  "break"
  "continue"
] @keyword.control

[
  "as"
  "at"
  "using"
  "import"
] @keyword

[
  "struct"
  "union"
  "enum"
] @keyword.type

(qualifier) @keyword.modifier

"print" @function.builtin

; ====================================================================
; Directives & Attributes
; ====================================================================

(directive) @attribute
(language_tag) @attribute

; ====================================================================
; Comments
; ====================================================================

(line_comment) @comment
(nested_comment) @comment

; ====================================================================
; Literals
; ====================================================================

(integer_literal) @number
(float_literal) @number
(number) @number
(string_literal) @string
(char_literal) @character
(boolean_literal) @boolean
(null_literal) @constant.builtin

; ====================================================================
; Types
; ====================================================================

(base_type) @type.builtin
(int_type) @type.builtin
(float_type) @type.builtin

(data_type
  constructor: (identifier) @type)

(data_type
  constructor: (scoped_identifier) @type)

(cast_expression
  type: (data_type) @type)

; ====================================================================
; Functions & Calls
; ====================================================================

; Function definitions (e.g. main: () -> void { ... })
(declaration
  name: (identifier) @function
  definition: (function_definition))

(declaration
  name: (scoped_identifier) @function
  definition: (function_definition))

; Function calls (matches foo() and Foo::bar())
(call_expression
  function: (_) @function.call)

; ====================================================================
; Variables, Parameters & Properties
; ====================================================================

; Parameter names
(parameter_list
  (binding
    name: (identifier) @variable.parameter))

; Loop variables
(loop_statement
  value: (identifier) @variable)

(loop_statement
  index: (identifier) @variable)

; Struct fields (definitions)
(struct_field
  (binding
    name: (identifier) @property))

; Struct literal initializers (e.g. { x = 10 })
(field_initializer
  name: (identifier) @property)

; Member access (e.g. obj.field)
(member_expression
  property: (identifier) @property)

; Enum variants
(enum_member
  name: (identifier) @constant)

; Imports
(import_statement
  alias: (identifier) @module)

(import_statement
  path: (file_name) @string)

; Declarations (variables/constants)
(declaration
  name: (identifier) @variable)

(declaration
  name: (scoped_identifier) @variable)

; Break / continue labels
(flow_control_statement
  label: (identifier) @label)

; Default fallback for identifiers
(identifier) @variable

; ====================================================================
; Operators & Delimiters
; ====================================================================

[
  "+"
  "-"
  "*"
  "/"
  "%"
  "<<"
  ">>"
  "&"
  "|"
  "^"
  "~"
  "!"
  "++"
  "--"
  "=="
  "!="
  "<"
  "<="
  ">"
  ">="
  "&&"
  "||"
  "->"
  "=>"
  "="
  ":"
] @operator

[
  ";"
  ","
  "::"
  "."
] @punctuation.delimiter

[
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @punctuation.bracket
