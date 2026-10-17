#ifndef __RP_TYPES_H__
#define __RP_TYPES_H__

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

#endif
