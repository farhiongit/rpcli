// Appear near the top of the parser IMPLEMENTATION file only, before the YYSTYPE definition
%code top {
} // %code top

// Required by the YYSTYPE definition in both the parser IMPLEMENTATION file and the parser HEADER file.
%code requires {
  #include "rp_types.h"
  #include "genericstreamreader.h"
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
  #include "rp_parser.h"
} // %code provides

// Code needed by the parser, inserted at the beginning of the parser IMPLEMENTATION file only, BEFORE the definition of yyparse.
%code {
  #include "rp_parser.c"
  #include "rp_lexer.c"   // Lexer code included here.
} // %code

// Declarations.
%define api.pure
%define parse.error detailed
%define api.token.prefix {RPN_}
%define api.prefix {RPN_}

%param {context *env}{StreamReader *input}

// For debugging purpose
%printer { _rpn_printer_number ($$); } <number>
%printer { fprintf (stderr, "%ls", $$->name); } <symbol>
%printer { fprintf (stderr, "%ls", $$); } <newvar_name>

// Character tokens (among the ten digits, the 52 lower- and upper-case English letters, and \a\b\t\n\v\f\r !\"#%&'()*+,-./:;<=>?[\\]^_{|}~) are declared automatically.
%token END 0 _("end of file")  // the end token (token 0) is specifically redefined.
%token EOL _("end of line")
%token EOC _("end of command")
%token UNRECOGNIZED _("unrecognized word")

%token ASSIGN _("assignment")
%token ADD _("sum")
%token MUL _("product")

%token <number> NUMBER _("number")
%token <symbol> CONSTANT _("constant")
%token <symbol> KNOWN_VARIABLE _("known variable")
%token <newvar_name> NEW_VARIABLE _("new variable")

%token <number> LAST_X _("last operand")
%token <number> LAST_RESULT _("last operation")
%token <number> LAST_ANSWER _("last calculation")

%token <symbol> F0A _("constant function")
%token <symbol> F1A _("unary function")
%token <symbol> F2A _("binary function")

%token MEMORY

%type <number> terminal expression sum_of_expressions product_of_expressions calculation operation
%type <symbol> no_arg

// Initialisation before parsing.
%initial-action { (void)RPN_mul; (void)RPN_add; (void)_rpn_printer_number; if (!env) abort (); _rpn_report_message (env, _ ("Type Ctrl-d or '%s' to quit.\n"), "exit"); }

// Invoked for the specifically redefined end token (token 0) when normal exit (no error).
%destructor { _rpn_report_message (env, _ ("Bye !\n")); } END
// Destructors directives define code that is called on error recovery when a symbol is automatically discarded.
// Right-hand side symbols of a rule that explicitly triggers a syntax error via YYERROR are not discarded automatically.
%destructor { free ($$); } <newvar_name>
// Invoked on error recovery and normal exit (no error).
//%destructor {  } input;

// Needed for sum_of_expressions and product_of_expressions to work.
%glr-parser
%expect 23
%expect-rr 7

%start input
%%
// Grammar rules.
// Always use left recursion only.
input:
    %empty { _rpn_report_message (env, "rpn> "); }
  | input command EOL { _rpn_report_message (env, "rpn> "); }
  | input command EOC
  ;

command:
    %empty
  | operation { env->last_ans = env->last_res = $operation ; _rpn_report_number (env, $operation); }
  | statement
  | error { _rpn_report_error (env, _ ("ignored command")); }
  ;

statement:
    MEMORY { RPN_print_vars(env); }
  ;

operation:
    calculation
  | KNOWN_VARIABLE[var] ASSIGN calculation { if ($calculation.type != UNDEFINED) RPN_set_var ($var, $$ = $calculation); }
  | NEW_VARIABLE[varname] ASSIGN calculation { if ($calculation.type != UNDEFINED) RPN_set_var (RPN_add_var (env, $varname), $$ = $calculation);
                                               free ($varname); }
  ;

calculation:
    expression
  | sum_of_expressions ADD
  | product_of_expressions MUL
  ;

// Always use left recursion only.
sum_of_expressions:
    expression[first] expression[second] expression[third] { $$ = RPN_add (env, RPN_add (env, $first, $second), env->last_x = $third); }
  | sum_of_expressions[first] expression[second] { $$ = RPN_add (env, $first, env->last_x = $second); }
  ;

// Always use left recursion only.
product_of_expressions:
    expression[first] expression[second] expression[third] { $$ = RPN_mul (env, RPN_mul (env, $first, $second), env->last_x = $third); }
  | product_of_expressions[first] expression[second] { $$ = RPN_mul (env, $first, env->last_x = $second); }
  ;

expression:
    terminal { env->last_res = $$ = $terminal; }
  | no_arg[function] { env->last_res = $$ = _rpn_f0a (env, $function); }
  | expression[first] F1A[function] { env->last_res = $$ = _rpn_f1a (env, $function, env->last_x = $first); }
  | expression[first] expression[second] F2A[function] { env->last_res = $$ = _rpn_f2a (env, $function, $first, env->last_x = $second); }
  | expression[first] expression[second] ADD { env->last_res = $$ = RPN_add (env, $first, env->last_x = $second); }
  | expression[first] expression[second] MUL { env->last_res = $$ = RPN_mul (env, $first, env->last_x = $second); }
  ;

terminal:
    NUMBER
  | LAST_X { $$ = env->last_x; }
  | LAST_RESULT { $$ = env->last_res; }
  | LAST_ANSWER { $$ = env->last_ans; }
  ;

no_arg: CONSTANT | KNOWN_VARIABLE | F0A ;

%%

// Code needed by the parser, copied verbatim to the end of the parser IMPLEMENTATION file only, AFTER the definition of yyparse.
// Definitions of yylex and yyerror often go here.
