# An RPN calculator with bison

This is an extended version of the minimalistic Reverse Polish Notation calculator (using postfix operators) given as an example in the [GNU bison documentation](https://www.gnu.org/software/bison/manual/bison.html#RPN-Calc).

As a parser (`bison`) is used, the stack of the terms of the RPN expression is automatically managed by the parser through its rules (as compared to our old beloved HP calculators).

The calculator handles:

- integers;
- decimals;
- conversion between integers and decimals;
- usual arithmetic operators and constants;
- user-defines variables;
- shortcuts for addition and product: when at the end of the line, `+` and `*` apply to all the expressions from the beginning of the line.

## Examples

Some examples of command lines.

> Terms are separated by spaces.

Line | Result | Comment
--|--|--
`2 3 *` | `6 [integer]` | RPN is a postfix calculator (applying the operator to the previous arguments).
`2.2e2 -810 +` | `-590` | Conversion to decimal.
`1 2 + 7 * 5 //` | `4 [integer]` | Several postfix operations in a row.
`2 7 / sqr pi *` | `0.256457` | Intermediate results are stacked (`2` and `7`, then `2 7 /`, then `2 7 / sqr` and `pi`).
`pi neg cos` | `-1` | `pi` is a predefined constant.
`11 1 2 + % _ ^` | `1 [integer]` | `_` equals to the last argument of the previous operation `%`, `1 2 +`.
`ans neg` | `-1 [integer]` | `ans` equals to the last result (`1`).
`1 2 3 *` | `6 [integer]` | `*` at the end applies to the previous three arguments.
`1 2 3 2 2 * +` | `10 [integer]` | `+` at the end applies to the previous four arguments `1`, `2`, `3` and `2 2 *`.
`a = 9 10 / inv exp` | `a = 3.03773` | A user-defined variable `a`.
`-1.7e3 a * sin` | `0.592614` | `a` is reused.
`100 log` | `2` | Conversion to decimal.
`e ans 1 + ** ln` | `3` | `ans` is equal to the previous result `2`, `e` is a predefined constant.
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
`vars` | `r = 129474 ...` | List all constants and user-defined variables.
`exit` | `Good bye!` | Quit.


## Commands

Word | Description
--|--
`vars` | List all the constants and user-defined variables
`exit` | Quit

## Functions

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

### Unary functions converting to integer

Word | Description
--|--
`round` | Round
`ceil` | Ceil
`floor` | Floor
`~` | [Two's complement](https://en.wikipedia.org/wiki/Two%27s_complement)

### Unary functions

Word | Description
--|--
`sqr` | Square
`abs` | Absolute value
`neg` | Opposite

### Binary functions

Word | Description
--|--
`+` | Addition
`-` | Substraction
`*` | Product
`/` | Division
`**` | Power of

### Integer binary functions

Word | Description
--|--
`//` | Quotient
`%` | Modulo
`&` | And
`\|` | Or
`^` | Xor

