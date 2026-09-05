/**
 * @file Tree-sitter grammar for the qVi programming language
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

const PREC = {
  assign: 1,
  range: 2,
  logical_or: 3,
  logical_and: 4,
  bit_or: 5,
  bit_xor: 6,
  bit_and: 7,
  equality: 8,
  relational: 9,
  shift: 10,
  additive: 11,
  multiplicative: 12,
  cast: 13,
  unary: 14,
  call: 15,
  subscript: 16,
  member: 17,
};

function commaSep(rule) {
  return optional(commaSep1(rule));
}

function commaSep1(rule) {
  return seq(rule, repeat(seq(',', rule)));
}

function commaSepTrailing(rule) {
  return seq(rule, repeat(seq(',', rule)), optional(','));
}

module.exports = grammar({
  name: 'qvi',

  extras: $ => [
    /\s/,
    $.line_comment,
    $.nested_comment,
  ],

  // Active GLR ambiguity forks
  conflicts: $ => [
    [$.block, $.struct_literal],
    [$._primary_expression, $.field_initializer],
    [$._type_constructor, $._primary_expression],
    [$.data_type],
    [$.parameter_list, $.function_type],
  ],

  word: $ => $.identifier,

  rules: {
    source_file: $ => repeat($._top_level_item),

    _top_level_item: $ => choice(
      $.directive,
      $.declaration,
      $._statement,
      ';',
    ),

    // ------------------------------------------------------------------------
    // Directives & Language Tags
    // ------------------------------------------------------------------------

    directive: $ => choice(
      '#test',
      seq('#[', field('name', $.identifier), ']'),
    ),

    language_tag: $ => seq('[', field('lang', $.identifier), ']'),

    // ------------------------------------------------------------------------
    // Declarations & Bindings
    // ------------------------------------------------------------------------

    binding: $ => seq(
      field('name', $.identifier),
      ':',
      field('type', $.data_type),
      optional(seq('=', field('value', $._expression))),
    ),

    declaration: $ => seq(
      field('name', choice($.identifier, $.scoped_identifier)),
      ':',
      optional($.language_tag),
      field('definition', $._definition),
    ),

    _definition: $ => choice(
      $.variable_definition,
      $.function_definition,
      $.block,
    ),

    variable_definition: $ => seq(
      field('type', $.data_type),
      optional(seq('=', field('value', $._expression))),
      ';',
    ),

    function_definition: $ => seq(
      field('parameters', $.parameter_list),
      optional(seq('using', field('using', $.identifier))),
      optional(seq('->', field('return_type', $.data_type))),
      field('body', $.block),
    ),

    parameter_list: $ => seq('(', commaSep($.binding), ')'),

    // ------------------------------------------------------------------------
    // Types & Type Constructors
    // ------------------------------------------------------------------------

    data_type: $ => seq(
      optional($.qualifier),
      field('constructor', $._type_constructor),
      repeat($.type_decorator),
    ),

    _type_constructor: $ => choice(
      $.base_type,
      $.identifier,
      $.scoped_identifier,
      $.struct_type,
      $.enum_type,
      $.function_type,
    ),

    base_type: $ => choice(
      $.int_type,
      $.float_type,
      'bool',
      'void',
    ),

    int_type: $ => choice(
      'i8', 'i16', 'i32', 'i64',
      'u8', 'u16', 'u32', 'u64',
    ),

    float_type: $ => choice('f32', 'f64'),

    qualifier: $ => choice('const', 'embed', 'fluid', 'alloc'),

    struct_type: $ => seq(
      choice('struct', 'union'),
      '{',
      repeat($.struct_field),
      '}',
    ),

    struct_field: $ => seq($.binding, ';'),

    enum_type: $ => seq(
      'enum',
      optional(seq(':', field('underlying_type', $.int_type))),
      '{',
      optional(commaSepTrailing($.enum_member)),
      '}',
    ),

    enum_member: $ => seq(
      field('name', $.identifier),
      optional(seq('=', field('value', $._expression))),
    ),

    type_decorator: $ => choice(
      '^',
      seq('[', optional(choice($._expression, $.qualifier)), ']'),
    ),

    function_type: $ => seq(
      '(',
      optional(commaSep($.data_type)),
      ')',
      '->',
      field('return_type', $.data_type),
    ),

    // ------------------------------------------------------------------------
    // Statements & Control Flow
    // ------------------------------------------------------------------------

    _statement: $ => choice(
      $.block,
      $.if_statement,
      $.loop_statement,
      $.case_statement,
      $.return_statement,
      $.flow_control_statement,
      $.import_statement,
      $.print_statement,
      $.expression_statement,
    ),

    block: $ => choice(
      seq('{', repeat($._top_level_item), '}'),
      seq(':', $._statement),
    ),

    if_statement: $ => prec.right(seq(
      'if',
      field('condition', $._expression),
      field('consequence', $.block),
      repeat($.else_if_clause),
      optional($.else_clause),
    )),

    else_if_clause: $ => seq(
      'else', 'if',
      field('condition', $._expression),
      field('consequence', $.block),
    ),

    else_clause: $ => seq(
      'else',
      field('body', $.block),
    ),

    loop_statement: $ => seq(
      'loop',
      field('iterable', $._iterable),
      optional(seq('as', optional('&'), field('value', $.identifier))),
      optional(seq(
        'at',
        optional('&'),
        field('index', $.identifier),
        optional(seq(':', field('index_type', $.data_type))),
      )),
      field('body', $.block),
    ),

    case_statement: $ => seq(
      'case',
      field('value', $._expression),
      '{',
      repeat($.case_clause),
      '}',
    ),

    case_clause: $ => seq(
      choice(
        seq('when', field('condition', $._expression)),
        'else',
      ),
      field('body', $.block),
    ),

    return_statement: $ => seq(
      'return',
      optional(commaSep1($._expression)),
      ';',
    ),

    flow_control_statement: $ => seq(
      choice('break', 'continue'),
      optional(field('label', $.identifier)),
      ';',
    ),

    import_statement: $ => seq(
      'import',
      optional($.language_tag),
      field('path', $.file_name),
      'as',
      field('alias', $.identifier),
      choice(';', seq(':', field('definition', $._definition))),
    ),

    print_statement: $ => seq(
      'print',
      commaSep1($._expression),
      ';',
    ),

    expression_statement: $ => seq(
      $._expression,
      optional(seq('=', field('right', $._expression))),
      ';',
    ),

    // ------------------------------------------------------------------------
    // MATLAB-style Slices & Ranges
    // ------------------------------------------------------------------------

    range: $ => prec.left(PREC.range, seq(
      optional(field('start', $._expression)),
      ':',
      optional(choice(
        seq(field('step', $._expression), ':', optional(field('end', $._expression))),
        field('end', $._expression),
      )),
    )),

    _iterable: $ => choice($.range, $._expression),
    slice_element: $ => choice($.range, $._expression),

    // ------------------------------------------------------------------------
    // Expressions
    // ------------------------------------------------------------------------

    _expression: $ => choice(
      $.unary_expression,
      $.binary_expression,
      $.cast_expression,
      $.call_expression,
      $.subscript_expression,
      $.member_expression,
      $._primary_expression,
    ),

    unary_expression: $ => prec(PREC.unary, seq(
      field('operator', choice('+', '-', '&', '^', '!', '~', '++', '--')),
      field('argument', $._expression),
    )),

    binary_expression: $ => {
      const table = [
        [PREC.logical_or, '||'],
        [PREC.logical_and, '&&'],
        [PREC.bit_or, '|'],
        [PREC.bit_xor, '^'],
        [PREC.bit_and, '&'],
        [PREC.equality, choice('==', '!=')],
        [PREC.relational, choice('<', '<=', '>', '>=')],
        [PREC.shift, choice('<<', '>>')],
        [PREC.additive, choice('+', '-')],
        [PREC.multiplicative, choice('*', '/', '%')],
      ];

      return choice(...table.map(([precedence, operator]) =>
        prec.left(precedence, seq(
          field('left', $._expression),
          field('operator', operator),
          field('right', $._expression),
        ))
      ));
    },

    cast_expression: $ => prec(PREC.cast, seq(
      field('argument', $._expression),
      field('operator', choice('->', '=>')),
      field('type', $.data_type),
    )),

    call_expression: $ => prec(PREC.call, seq(
      field('function', $._expression),
      field('arguments', $.argument_list),
    )),

    argument_list: $ => seq('(', commaSep($._expression), ')'),

    subscript_expression: $ => prec(PREC.subscript, seq(
      field('argument', $._expression),
      '[',
      commaSep1($.slice_element),
      ']',
    )),

    member_expression: $ => prec(PREC.member, seq(
      field('argument', $._expression),
      '.',
      field('property', $.identifier),
    )),

    _primary_expression: $ => choice(
      $.identifier,
      $.scoped_identifier,
      $.number,
      $.string_literal,
      $.char_literal,
      $.boolean_literal,
      $.null_literal,
      $.parenthesized_expression,
      $.array_literal,
      $.struct_literal,
    ),

    parenthesized_expression: $ => seq('(', $._expression, ')'),

    array_literal: $ => seq('[', optional(commaSepTrailing($._expression)), ']'),

    struct_literal: $ => seq('{', optional(commaSepTrailing($.field_initializer)), '}'),

    field_initializer: $ => choice(
      seq(field('name', $.identifier), '=', field('value', $._expression)),
      field('value', $._expression),
    ),

    // ------------------------------------------------------------------------
    // Lexical Tokens
    // ------------------------------------------------------------------------

    scoped_identifier: $ => prec.left(seq(
      choice($.identifier, $.scoped_identifier),
      '::',
      $.identifier,
    )),

    identifier: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

    file_name: $ => /[a-zA-Z0-9_./-]+/,

    number: $ => choice($.integer_literal, $.float_literal),

    integer_literal: $ => token(choice(
      /0x[0-9a-fA-F][0-9a-fA-F_]*/,
      /0b[01][01_]*/,
      /[0-9][0-9_]*/,
    )),

    float_literal: $ => token(choice(
      /[0-9][0-9_]*\.[0-9][0-9_]*([fF])?/,
      /[0-9][0-9_]*[fF]/,
    )),

    string_literal: $ => token(seq(
      '"',
      repeat(choice(/[^"\\\n]+/, /\\./)),
      '"',
      optional('b'),
    )),

    char_literal: $ => token(seq(
      "'",
      choice(/[^'\\\n]/, /\\./),
      "'",
    )),

    boolean_literal: $ => choice('true', 'false'),
    null_literal: $ => 'null',

    line_comment: $ => token(seq('//', /.*/)),

    nested_comment: $ => token(seq('/{', repeat(choice(/[^{/]/, /\/[^{]/, /\{[^/]/)), '/}')),
  },
});
