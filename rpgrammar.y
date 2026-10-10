// Appear near the top of the parser implementation file only, before the YYSTYPE definition
%code top {
  #include <stdio.h>
  #define _(str) (str) // i18n
}

// Required by the YYSTYPE definition in both the parser implementation file and the parser header file.
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


  typedef integer (*iF) ();
  typedef integer (*iFi) (integer);
  typedef integer (*iFd) (decimal);
  typedef integer (*iFii) (integer, integer);
  typedef integer (*iFid) (integer, decimal);
  typedef integer (*iFdi) (decimal, integer);
  typedef integer (*iFdd) (decimal, decimal);
  typedef decimal (*dF) ();
  typedef decimal (*dFi) (integer);
  typedef decimal (*dFd) (decimal);
  typedef decimal (*dFii) (integer, integer);
  typedef decimal (*dFid) (integer, decimal);
  typedef decimal (*dFdi) (decimal, integer);
  typedef decimal (*dFdd) (decimal, decimal);

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

  typedef struct symrec symrec;
  typedef struct context context;
  struct context {
    symrec *sym_table;
    number last_x, last_res, last_ans;
  };
}

// The union YYSTYPE (or union name) definition
%union value {
  number number;
  symbol *symbol;
  wchar_t *newvar_name;
}

// Prototypes inserted into both the parser header file and the parser implementation file that depend on YYSTYPE or YYLTYPE.
// Declare functions yylex and yyerror here (needed for the definition of yyparse).
%code provides {
  int RPN_lex (RPN_STYPE *, context *context);
  void RPN_error (context *context, char const *);

  number RPN_add (number, number);
  number RPN_sub (number, number);
  number RPN_mul (number, number);
  number RPN_quotient (number, number);
  number RPN_div (number, number);
  number RPN_mod (number, number);
  number RPN_and (number, number);
  number RPN_or (number, number);
  number RPN_xor (number, number);
  number RPN_pow (number, number);
  number RPN_compl (number);
  number RPN_f0a (symbol *);
  number RPN_f1a (symbol *, number);
  number RPN_f2a (symbol *, number, number);

  number RPN_set_var (symbol *symref, number val);
  symbol *RPN_add_var (symrec **sym_table, wchar_t *name);

  void RPN_goodbye (symrec **sym_table);
  int RPN_print_number (number);
  void RPN_print_vars (symrec *sym_table);
}

// Code needed by the parser, inserted at the beginning of the parser implementation file only, before the definition of yyparse.
%code {
}

// Declarations.
%define api.pure
%define parse.error detailed
%define api.token.prefix {RPN_}
%define api.prefix {RPN_}

%param {context *env}

// For debugging purpose
%printer { RPN_print_number ($$); } <number>
%printer { fprintf (stderr, "%ls", $$->name); } <symbol>

// Character tokens (among the ten digits, the 52 lower- and upper-case English letters, and \a\b\t\n\v\f\r !\"#%&'()*+,-./:;<=>?[\\]^_{|}~) are declared automatically.
%token END 0 "end of file"  // the end token (token 0) is specifically redefined.
%token EOL _("end of line")
%token UNRECOGNIZED _("unrecognized word")

%token POW _("power")
%token QUOTIENT _("quotient")
%token MEMORY

%token <number> NUMBER _("number")
%token <symbol> CONSTANT _("constant")
%token <symbol> VARIABLE _("variable")
%token <newvar_name> NEW_VARIABLE _("new variable")

%token <symbol> F0A _("constant function")
%token <symbol> F1A _("unary function")
%token <symbol> F2A _("binary function")

%type <number> expression
%type <number> sum_of_expressions
%type <number> product_of_expressions
%type <number> calculation

// Invoked for the end token (token 0) when specifically redefined.
%destructor { RPN_goodbye (&env->sym_table); } END
// Destructors directives define code that is called on error recovery when a symbol is automatically discarded.
// Right-hand side symbols of a rule that explicitly triggers a syntax error via YYERROR are not discarded automatically.
%destructor { free ($$); } <newvar_name>

%glr-parser // Needed for sum_of_expressions and product_of_expressions to work.
%start input
%%
// Grammar rules.
// Always use left recursion only.
input:
    %empty { fprintf (stderr, "rpn> "); }
  | input EOL { fprintf (stderr, "rpn> "); }
  | input calculation EOL { fprintf (stderr, "rpn> "); }
  | input set_var EOL { fprintf (stderr, "rpn> "); }
  | input set_newvar EOL { fprintf (stderr, "rpn> "); }
  | input statement EOL { fprintf (stderr, "rpn> "); }
  //| error { fprintf (stderr, "rpn> "); }
  ;

statement:
    MEMORY { RPN_print_vars (env->sym_table); }
  //| error
  ;

set_var:
    VARIABLE[var] '=' calculation { if ($calculation.type != UNDEFINED) RPN_set_var ($var, $calculation); }
  //| error
  ;

set_newvar:
    NEW_VARIABLE[varname] '=' calculation { if ($calculation.type != UNDEFINED) RPN_set_var (RPN_add_var (&env->sym_table, $varname), $calculation);
                                            free ($varname); }
  //| error
  ;

calculation:
    sum_of_expressions '+' { env->last_ans = env->last_res = $$ = $1 ; RPN_print_number ($1) && fprintf (stderr, "\n"); }
  | product_of_expressions '*' { env->last_ans = env->last_res = $$ = $1 ; RPN_print_number ($1) && fprintf (stderr, "\n"); }
  | expression { env->last_ans = env->last_res = $$ = $1 ; RPN_print_number ($1) && fprintf (stderr, "\n"); }
  //| error
  ;

// Always use left recursion only.
sum_of_expressions:
    expression[first] expression[second] expression[third] { env->last_res = $$ = RPN_add (RPN_add ($first, $second), env->last_x = $third); }
  | sum_of_expressions[first] expression[second] { env->last_res = $$ = RPN_add ($first, env->last_x = $second); }
  ;

// Always use left recursion only.
product_of_expressions:
    expression[first] expression[second] expression[third] { env->last_res = $$ = RPN_mul (RPN_mul ($first, $second), env->last_x = $third); }
  | product_of_expressions[first] expression[second] { env->last_res = $$ = RPN_mul ($first, env->last_x = $second); }
  ;

expression:
    NUMBER { env->last_res = $$ = $1; }
  | CONSTANT { env->last_res = $$ = RPN_f0a ($1); }
  | VARIABLE { env->last_res = $$ = RPN_f0a ($1); }
  | F0A { env->last_res = $$ = RPN_f0a ($1); }
  | expression[first] F1A[function] { env->last_res = $$ = RPN_f1a ($function, env->last_x = $first); }
  | expression[first] expression[second] F2A[function] { env->last_res = $$ = RPN_f2a ($function, $first, env->last_x = $second); }
  | expression[first] expression[second] '+' { env->last_res = $$ = RPN_add ($first, env->last_x = $second); }
  | expression[first] expression[second] '-' { env->last_res = $$ = RPN_sub ($first, env->last_x = $second); }
  | expression[first] expression[second] '*' { env->last_res = $$ = RPN_mul ($first, env->last_x = $second); }
  | expression[first] expression[second] '/' { env->last_res = $$ = RPN_div ($first, env->last_x = $second); }
  | expression[first] expression[second] QUOTIENT { env->last_res = $$ = RPN_quotient ($first, env->last_x = $second); }
  | expression[first] expression[second] '%' { env->last_res = $$ = RPN_mod ($first, env->last_x = $second); }
  | expression[first] expression[second] POW { env->last_res = $$ = RPN_pow ($first, env->last_x = $second); }
  | expression[first] expression[second] '&' { env->last_res = $$ = RPN_and ($first, env->last_x = $second); }
  | expression[first] expression[second] '|' { env->last_res = $$ = RPN_or ($first, env->last_x = $second); }
  | expression[first] expression[second] '^' { env->last_res = $$ = RPN_xor ($first, env->last_x = $second); }
  | expression[first] '~' { env->last_res = $$ = RPN_compl (env->last_x = $first); }
  ;

%%

// Code needed by the parser, copied verbatim to the end of the parser implementation file only, after the definition of yyparse.
// Definitions of yylex and yyerror often go here.


