// Appear near the top of the parser IMPLEMENTATION file only, before the YYSTYPE definition
%code top {
  #include <stdio.h>
  #include <stdlib.h>
  #include <errno.h>
  #include <math.h>
  #include <setjmp.h>
  #include <stdarg.h>
  #include <time.h>
  #include <limits.h>

  #define _(str) (str) // i18n

  typedef jmp_buf exception;
  #define throw_exception(exception) longjmp (exception, (__COUNTER__ + 1))
  #define catch_exception(exception) if (setjmp (exception))
} // %code top

// Required by the YYSTYPE definition in both the parser IMPLEMENTATION file and the parser HEADER file.
%code requires {
  #include <wchar.h>

  typedef long long int integer;
  typedef long double decimal;
  typedef struct number number;
  struct number {
    enum { UNDEFINED = 0,
           INTEGER,
           DECIMAL,
    } type;
    union {
      integer integer;
      decimal decimal;
    } value;
  };

  typedef integer (*iF) (void);
  typedef integer (*iFi) (integer);
  typedef integer (*iFd) (decimal);
  typedef integer (*iFii) (integer, integer);
  typedef integer (*iFid) (integer, decimal);
  typedef integer (*iFdi) (decimal, integer);
  typedef integer (*iFdd) (decimal, decimal);
  typedef decimal (*dF) (void);
  typedef decimal (*dFi) (integer);
  typedef decimal (*dFd) (decimal);
  typedef decimal (*dFii) (integer, integer);
  typedef decimal (*dFid) (integer, decimal);
  typedef decimal (*dFdi) (decimal, integer);
  typedef decimal (*dFdd) (decimal, decimal);
  typedef number (*nF) (void);
  typedef number (*nFn) (number);
  typedef number (*nFnn) (number, number);

  typedef struct function function;
  struct function {
    enum { IF,
           DF,
           IFI,
           DFI,
           IFD,
           DFD,
           IFII,
           IFID,
           IFDI,
           IFDD,
           DFII,
           DFID,
           DFDI,
           DFDD,
           NF,
           NFN,
           NFNN,
    } type;
    union {
      iF iF;
      iFi iFi;
      iFd iFd;
      iFii iFii;
      iFid iFid;
      iFdi iFdi;
      iFdd iFdd;
      dF dF;
      dFi dFi;
      dFd dFd;
      dFii dFii;
      dFid dFid;
      dFdi dFdi;
      dFdd dFdd;
      nF nF;
      nFn nFn;
      nFnn nFnn;
    } value;
  };

  typedef struct symbol symbol;
  struct symbol {
    wchar_t *name;
    enum { FUNCTION,
           VARIABLE,
           CONSTANT,
    } type;
    union {
      function function; // for FUNCTION
      number number;     // for CONSTANT and VARIABLE
    } value;
  };

  typedef struct context context;
} // %code requires

// The union YYSTYPE (or union name) definition
%union value {
  number number;
  symbol *symbol;
  wchar_t *newvar_name;
} // %union

// Prototypes inserted into both the parser HEADER file and the parser IMPLEMENTATION file that depend on YYSTYPE or YYLTYPE.
// Declare functions yylex and yyerror here (needed for the definition of yyparse).
%code provides {
  struct report {
    void (*error) (const char *);
    void (*decimal) (decimal);
    void (*integer) (integer);
    void (*message) (const char *format, va_list);
  };
  context *RPN_init (struct report r);
  void RPN_cleanup (context *env);

  int RPN_lex (RPN_STYPE *RPN_lval, context *env, FILE *in);
  symbol *from_name (context *env, const wchar_t *name);
  number from_integer (integer);
  number from_decimal (decimal);
} // %code provides

// Code needed by the parser, inserted at the beginning of the parser IMPLEMENTATION file only, BEFORE the definition of yyparse.
%code {
  #include "rp_parser.c"  // Code included here.
} // %code

// Declarations.
%define api.pure
%define parse.error detailed
%define api.token.prefix {RPN_}
%define api.prefix {RPN_}

%param {context *env}{FILE *input}

// For debugging purpose
%printer { _rpn_printer_number ($$); } <number>
%printer { fprintf (stderr, "%ls", $$->name); } <symbol>

// Character tokens (among the ten digits, the 52 lower- and upper-case English letters, and \a\b\t\n\v\f\r !\"#%&'()*+,-./:;<=>?[\\]^_{|}~) are declared automatically.
%token END 0 _("end of file")  // the end token (token 0) is specifically redefined.
%token EOL _("end of line")
%token UNRECOGNIZED _("unrecognized word")

%token <number> NUMBER _("number")
%token <symbol> CONSTANT _("constant")
%token <symbol> KNOWN_VARIABLE _("known variable")
%token <newvar_name> NEW_VARIABLE _("new variable")

%token <number> LAST_X _("last operand")
%token <number> LAST_RESULT _("last operation")
%token <number> LAST_ANSWER _("last result")

%token <symbol> F0A _("constant function")
%token <symbol> F1A _("unary function")
%token <symbol> F2A _("binary function")

%type <number> expression sum_of_expressions product_of_expressions calculation

// Initialisation before parsing.
%initial-action { (void)_rpn_printer_number; if (!env) abort (); _rpn_report_message (env, _ ("Type Ctrl-d or '%s' to quit.\n"), _ ("exit"));}

// Invoked for the specifically redefined end token (token 0) when normal exit (no error).
%destructor { _rpn_report_message (env, _ ("Bye !\n")); } END
// Destructors directives define code that is called on error recovery when a symbol is automatically discarded.
// Right-hand side symbols of a rule that explicitly triggers a syntax error via YYERROR are not discarded automatically.
%destructor { free ($$); } <newvar_name>
// Invoked on error recovery and normal exit (no error).
//%destructor {  } input;

%glr-parser // Needed for sum_of_expressions and product_of_expressions to work.
%expect 21
%expect-rr 7
%start input
%%
// Grammar rules.
// Always use left recursion only.
input:
    %empty { _rpn_report_message (env, "rpn> "); }
  | input EOL { _rpn_report_message (env, "rpn> "); }
  | input command EOL { _rpn_report_message (env, "rpn> "); }
  //| error EOL { yyerrok; }
  ;

command:
    calculation
  | set_var
  | set_newvar
  //| error
  ;

set_var:
    KNOWN_VARIABLE[var] '=' calculation { if ($calculation.type != UNDEFINED) RPN_set_var ($var, $calculation); }
  //| error
  ;

set_newvar:
    NEW_VARIABLE[varname] '=' calculation { if ($calculation.type != UNDEFINED) RPN_set_var (RPN_add_var (env, $varname), $calculation);
                                            free ($varname); }
  //| error
  ;

calculation:
    sum_of_expressions '+' { env->last_ans = env->last_res = $$ = $1 ; _rpn_report_number (env, $1); }
  | product_of_expressions '*' { env->last_ans = env->last_res = $$ = $1 ; _rpn_report_number (env, $1); }
  | expression { env->last_ans = env->last_res = $$ = $1 ; _rpn_report_number (env, $1); }
  //| error
  ;

// Always use left recursion only.
sum_of_expressions:
    expression[first] expression[second] expression[third] { env->last_res = $$ = _rpn_add (_rpn_add ($first, $second), env->last_x = $third); }
  | sum_of_expressions[first] expression[second] { env->last_res = $$ = _rpn_add ($first, env->last_x = $second); }
  ;

// Always use left recursion only.
product_of_expressions:
    expression[first] expression[second] expression[third] { env->last_res = $$ = _rpn_mul (_rpn_mul ($first, $second), env->last_x = $third); }
  | product_of_expressions[first] expression[second] { env->last_res = $$ = _rpn_mul ($first, env->last_x = $second); }
  ;

expression:
    NUMBER { env->last_res = $$ = $1; }
  | LAST_X { env->last_res = $$ = env->last_x; }
  | LAST_RESULT { env->last_res = $$ = env->last_res; }
  | LAST_ANSWER { env->last_res = $$ = env->last_ans; }
  | CONSTANT { env->last_res = $$ = _rpn_f0a (env, $1); }
  | KNOWN_VARIABLE { env->last_res = $$ = _rpn_f0a (env, $1); }
  | F0A { env->last_res = $$ = _rpn_f0a (env, $1); }
  | expression[first] F1A[function] { env->last_res = $$ = _rpn_f1a (env, $function, env->last_x = $first); }
  | expression[first] expression[second] F2A[function] { env->last_res = $$ = _rpn_f2a (env, $function, $first, env->last_x = $second); }
  ;

%%

// Code needed by the parser, copied verbatim to the end of the parser IMPLEMENTATION file only, AFTER the definition of yyparse.
// Definitions of yylex and yyerror often go here.

// Lexer in rp_lexer.c
