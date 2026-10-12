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

  typedef struct symrec symrec;
  typedef struct context context;
  struct context {
    symrec *sym_table;
    number last_x, last_res, last_ans;
    FILE *in, *out, *err;
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
  #define RPN_STDERR (env->err ? env->err : stderr)
  #define RPN_STDOUT (env->out ? env->out : stdout)
  #define RPN_STDIN (env->in ? env->in : stdin)
  int RPN_lex (RPN_STYPE *, context *context);
  void RPN_error (context *context, char const *);

  void RPN_init (context *env);
  void RPN_cleanup (context *);

  number RPN_add (number, number);
  number RPN_mul (number, number);
  number RPN_f0a (symbol *);
  number RPN_f1a (symbol *, number);
  number RPN_f2a (symbol *, number, number);

  number RPN_set_var (symbol *symref, number val);
  symbol *RPN_add_var (symrec **sym_table, wchar_t *name);

  int RPN_print_number (number, context *);
  void RPN_print_vars (symrec *sym_table, context *);
}

// Code needed by the parser, inserted at the beginning of the parser implementation file only, before the definition of yyparse.
%code {
}

// Declarations.
%define api.pure
%define parse.error detailed
%define api.token.prefix {RPN_}
%define api.prefix {RPN_}

%param {context *env}  // Context passed to the lexer and the parser.

// For debugging purpose
%printer { RPN_print_number ($$, env); } <number>
%printer { fprintf (RPN_STDERR, "%ls", $$->name); } <symbol>

// Character tokens (among the ten digits, the 52 lower- and upper-case English letters, and \a\b\t\n\v\f\r !\"#%&'()*+,-./:;<=>?[\\]^_{|}~) are declared automatically.
%token END 0 _("end of file")  // the end token (token 0) is specifically redefined.
%token EOL _("end of line")
%token UNRECOGNIZED _("unrecognized word")

%token MEMORY

%token <number> NUMBER _("number")
%token <symbol> CONSTANT _("constant")
%token <symbol> VARIABLE _("variable")
%token <newvar_name> NEW_VARIABLE _("new variable")

%token <symbol> F0A _("constant function")
%token <symbol> F1A _("unary function")
%token <symbol> F2A _("binary function")

%type <number> expression sum_of_expressions product_of_expressions calculation

// Invoked for the specifically redefined end token (token 0) when normal exit (no error).
%destructor { fprintf (RPN_STDERR, "Bye !\n"); } END
// Destructors directives define code that is called on error recovery when a symbol is automatically discarded.
// Right-hand side symbols of a rule that explicitly triggers a syntax error via YYERROR are not discarded automatically.
%destructor { free ($$); } <newvar_name>
// Invoked on error recovery and normal exit (no error).
//%destructor {  } input;

%glr-parser // Needed for sum_of_expressions and product_of_expressions to work.
%expect 12
%expect-rr 4
%start input
%%
// Grammar rules.
// Always use left recursion only.
input:
    %empty { fprintf (RPN_STDERR, "rpn> "); }
  | input EOL { fprintf (RPN_STDERR, "rpn> "); }
  | input command EOL { fprintf (RPN_STDERR, "rpn> "); }
  //| error EOL { fprintf (RPN_STDERR, "Recovering...\nrpn> "); }
  ;

command:
    calculation
  | set_var
  | set_newvar
  | statement
  //| error

statement:
    MEMORY { RPN_print_vars (env->sym_table, env); }
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
    sum_of_expressions '+' { env->last_ans = env->last_res = $$ = $1 ; RPN_print_number ($1, env) && fprintf (RPN_STDERR, "\n"); }
  | product_of_expressions '*' { env->last_ans = env->last_res = $$ = $1 ; RPN_print_number ($1, env) && fprintf (RPN_STDERR, "\n"); }
  | expression { env->last_ans = env->last_res = $$ = $1 ; RPN_print_number ($1, env) && fprintf (RPN_STDERR, "\n"); }
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
  ;

%%

// Code needed by the parser, copied verbatim to the end of the parser implementation file only, after the definition of yyparse.
// Definitions of yylex and yyerror often go here.


