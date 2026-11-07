# An RPN calculator with bison

This is an extended version of the minimalistic Reverse Polish Notation calculator (using postfix operators) given as an example in the [GNU bison documentation](https://www.gnu.org/software/bison/manual/bison.html#RPN-Calc).

As a parser (`bison`) is used, the stack of the terms of the RPN expression is automatically managed by the parser through its rules (as compared to our old beloved [HP calculators](https://en.wikipedia.org/wiki/HP-42S)).

The calculator handles :

- integers ;
- decimals ;
- conversion between integers and decimals ;
- usual arithmetic operators and constants ;
- user-defines variables ;
- shortcuts for addition and product: when at the end of the line, `+` and `*` apply to all the expressions from the beginning of the line.

## Files

The parser is defined by :

- the types of its tokens ([rp_types.h](rp_types.h)) ;
- its grammar rules ([rpgrammar.y](rpgrammar.y)) ;
- its actions (operands and operators) ([rp_parser.h](rp_parser.h) and [rp_parser.c](rp_parser.c)).

The calculator comes in two flavours :

- as command-line ([rpcli.c](rpcli.c)) : run `make rpcli` and `./rpcli` ;
- as a [mongoose HTTP server](https://mongoose.ws/) ([rphttpserver.c](rphttpserver.c)) : run `make rphttpserver` and `./rphttpserver`.

This project contains as well :

- a rudimentary lexer ([rp_lexer.c](rp_lexer.c)) ;
- a  [generic agnostic stream reader](#generic-input-stream) with look-ahead capability ([genericstreamreader.h](genericstreamreader.h) and [genericstreamreader.c](genericstreamreader.c), tested with [test_streamreader.c](test_streamreader.c)).

## Calculator logic

### Examples

Below some examples of commands accepted by the calculator.

> Terms must be separated by spaces.

Line | Result | Comment
--|--|--
`2 3 *` | `6 [integer]` | RPN is a postfix calculator (applying the operator to the previous arguments).
`2.22e1 -800 +` | `-777.8` | `-800` is converted to decimal.
`1 2 + 7 * 5 //` | `4 [integer]` | Several postfix operations in a row.
`2 7 / sqr pi *` | `0.256457` | Intermediate results are stacked (`2` and `7`, then `2 7 /`, then `2 7 / sqr` and `pi`).
`pi neg cos` | `-1` | `pi` is a predefined constant.
`11 1 2 + % _ ^` | `1 [integer]` | `_` equals to the last argument of the previous operation `%`, `1 2 +`.
`ans neg` | `-1 [integer]` | `ans` equals to the last result (`1`).
`10.6 4 %` | `2.6` | The modulo function can apply to integers and decimals as well.
`1 2 3 *` | `6 [integer]` | `*` at the end applies to the previous three arguments.
`1 2 3 2 2 * +` | `10 [integer]` | `+` at the end applies to the previous four arguments `1`, `2`, `3` and `2 2 *`.
`a = 9 10 / inv exp` | `a = 3.03773` | A user-defined variable `a`.
`-1.7e3 a * sin` | `0.592614` | `a` is reused.
`200 log` | `2.30103` | `200` is converted to decimal.
`e ans 1 + pow ln` | `3.30103` | `ans` is equal to the previous result `2.30103`, `e` is a predefined constant.
`2 sqrt ceil neg` | `-2 [integer]` | Conversion to integer.
`10 sqrt round` | `3 [integer]` | Conversion to integer.
`r = 1e6 rand * floor` | `r = 129474 [integer]` | `r` is defined as a random integer in [ 0 ; 1000000 [.
`5 res * res * sqrt` | `25` | The first `res` equals `5`, the second `25`. After calculation, `_` equals `625`, `res` and `ans` equal `25`.
`5 ans * ans * sqrt` | `55.9017` | Both `ans` equal `25`, the result of the previous calculation. After calculation, `_` equals 3125, `res` and `ans` equal `55.9017`.
`5 7 * _ +` | `42 [integer]` | `_` equals `7`, the last operand of the previous operation `5 7 *`.
`5 7 * res +` | `70 [integer]` | `res` equals `42`, the previous operand, as calculated as `5 7 *`.
`6 3 + 2 res * -` | `5 [integer]` | `res` equals `2`, the previous operand.
`6 3 + 2 _ * -` | `3 [integer]` | `_` equals `3`, the last operand of the previous operation `6 3 +`.
`6 3 + 2 ans * -` | `-1 [integer]` | `ans` equals `5`, the result of the calculation of the last command line.
`vars` | `r = 129474 ...` | List all user-defined variables.
`exit` | `Bye!` | Quit.

### Commands

Word | Description
--|--
`vars` | List all the user-defined variables
`exit` | Quit

### Constants

Word | Description
--|--
`e` | *e*
`pi` | *π*

### Pre-defined variables

Word | Description
--|--
`rand` | Decimal random value in [ 0 ; 1 [
`_` | Value of the last operand of the previous operation
`res` | Value of the previous operand
`ans` | Value of the result of the calculation of the last command line

### Unary functions

Word | Description
--|--
`sqr` | Square
`abs` | Absolute value
`neg` | Opposite

### Unary functions converting to decimal

Word | Description
--|--
`inv` | Inverse
`sin` | Sinus
`cos` | Cosinus
`tan` | Tangent
`asin` | Arcsinus
`acos` | Arccosinus
`atan` | Arctangent
`sinh` | Hyperbolic sinus
`cosh` | Hyperbolic cosinus
`tanh` | Hyperbolic tangent
`asinh` | Hyperbolic arcsinus
`acosh` | Hyperbolic arccosinus
`atanh` | Hyperbolic arctangent
`exp` | Exponential
`ln` | Natural logarithm
`log` | Logarithm
`sqrt` | Square root

### Unary functions on integer

Word | Description
--|--
`~` | [Two's complement](https://en.wikipedia.org/wiki/Two%27s_complement)
`!` | Factorial

### Unary functions converting to integer

Word | Description
--|--
`round` | Round
`ceil` | Ceil
`floor` | Floor

### Binary functions

Word | Description
--|--
`+` | Addition
`-` | Substraction
`*` | Product
`/` | Division
`%` | Modulo
`pow` | Power of

### Binary functions on integers

Word | Description
--|--
`//` | Quotient
`&` | And
`\|` | Or
`^` | Xor

## Generic input stream

The generic stream reader with look-ahead capability ([genericstreamreader.h](genericstreamreader.h) and [genericstreamreader.c](genericstreamreader.c) works like this:

- Create a new stream with `stream_create (Reader reader)` where `reader` is a user-defined sequential reader.
- Read and consume one "character" from the stream with `stream_get (StreamReader *stream)` (which calls `read_fn` on the `instance` defined by the `reader`.)

  - Once this character has been used, the function returned by `stream_forgetter (StreamReader *stream)` **should be applied** to this character to avoid memory leaks,
    writing something like :

    - `stream_forgetter (stream) (c)` (`stream_forgetter` returns the function `forget_read_fn` defined by the `reader`);
    - or the shortcut `stream_get_and_forget (stream)` that reads, ignores and discards a character from the stream.

  - **Caution**: The i<sup>th</sup> call to `stream_get` **discards** the character previously returned by `stream_look_ahead (stream, `*i*`)`.

- Read ahead the n<sup>th</sup> "character", *without consuming it*, with `stream_look_ahead (StreamReader *stream, size_t n)`.
- Destroy a stream with `stream_destroy (StreamReader *stream)` (which calls `free_instance_fn` on the `instance` defined by the `reader`.) 

### Helpers

Four usual readers are provided for convenience, to be used directly by `stream_create`:

- `ch_file_reader (FILE *f)` calls `fgetc` internally on a file.

  - The reader provides pointers to `char`.

- `wc_file_reader (FILE *f)` calls `fgetwc` internally on a file.

  - The reader provides pointers to `wchar_t`.
  - Used for example by [rpcli.c](rpcli.c).

- `str_reader (char *source_str)` mimics `fgetc` on a file that would have been opened by `fmemopen`.

  - The reader provides pointers to `char`.
  - Used for example by [test_streamreader.c](test_streamreader.c).

- `wcs_reader (wchar_t *source_str)` mimics `fgetwc` on a file that would have been opened by `fmemopen`
  (but actually, `fmemopen` cannot be used on an multi-bytes character string, which is why this generic stream reader has been written!).

  - The reader provides pointers to `wchar_t`.
  - Used for example by [rphttpserver.c](rphttpserver.c).

