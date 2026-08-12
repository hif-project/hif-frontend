%{

/// @file vhdl.lex
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <iostream>

#include <hif/hif.hpp>

#include "vhdl2hif/vhdl_parser.hpp"
#include "vhdl2hif/vhdl_support.hpp"
#include "vhdl2hif/vhdl_parser_struct.hpp"

#include "vhdlParser.hpp"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined __GNUC__
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif


// print tokens recognized by the lexer
#define LEXER_VERBOSE_MODE

#if (defined _MSC_VER)
#pragma warning(disable:4267)

// Windows version seems to not have destroy().
int yylex_destroy  (void)
{
#if 0
    /* Pop the buffer stack, destroying each element. */
    while(YY_CURRENT_BUFFER)
    {
        yy_delete_buffer( YY_CURRENT_BUFFER );
        YY_CURRENT_BUFFER_LVALUE = nullptr;
        yypop_buffer_state();
    }

    /* Destroy the stack itself. */
    yyfree((yy_buffer_stack) );
    (yy_buffer_stack) = nullptr;

    /* Reset the globals. This is important in a non-reentrant scanner so the next time
     * yylex() is called, initialization will occur. */
    yy_init_globals( );
#endif
    return 0;
}

int fake( int i )
{
    return _isatty( i );
}

#undef isatty
#define isatty fake

#endif

extern char *yysptr;
extern char *yysbuf;
/*K: sux, but seems required...*/
#if (defined _MSC_VER)
int yylineno = 1;
#endif
extern char yytchar;
extern VhdlParser * parserInstance;     // defined in vhdlParser.cc


#define YYLMAX 1024
#define MVL_LG_MC 15

// Size of token tables
#define VHDL_TOKENS_TABLE_LEN 98
#define PSL_TOKENS_TABLE_LEN 92

typedef struct 
{
    char nom[MVL_LG_MC];
    int kval;
} el_mc;

// the index of the last recognized token
extern int lastToken;
int lastToken = -1;

 /*
  *  VHDL reserved keywords
  *  WARNING: Increment MVL_NB_MC value whenever a new entry is appended 
  */
static el_mc vhdl_tokens []=
{
    {"abs"          ,t_ABS},
    {"access"       ,t_ACCESS},
    {"after"        ,t_AFTER},
    {"alias"        ,t_ALIAS},
    {"all"          ,t_ALL},
    {"and"          ,t_AND},
    {"architecture" ,t_ARCHITECTURE},
    {"array"        ,t_ARRAY},
    {"assert"       ,t_ASSERT},
    {"attribute"    ,t_ATTRIBUTE},
    {"begin"        ,t_BEGIN},
    {"block"        ,t_BLOCK},
    {"body"         ,t_BODY},
    {"buffer"       ,t_BUFFER},
    {"bus"          ,t_BUS},
    {"case"         ,t_CASE},
    {"component"    ,t_COMPONENT},
    {"configuration",t_CONFIGURATION},
    {"constant"     ,t_CONSTANT},
    {"disconnect"   ,t_DISCONNECT},
    {"downto"       ,t_DOWNTO},
    {"else"         ,t_ELSE},
    {"elsif"        ,t_ELSIF},
    {"end"          ,t_END},
    {"entity"       ,t_ENTITY},
    {"exit"         ,t_EXIT},
    {"file"         ,t_FILE},
    {"for"          ,t_FOR},
    {"function"     ,t_FUNCTION},
    {"generate"     ,t_GENERATE},
    {"generic"      ,t_GENERIC},
    {"guarded"      ,t_GUARDED},
    {"group"        ,t_GROUP},
    {"if"           ,t_IF},
    {"impure"       ,t_IMPURE},
    {"in"           ,t_IN},
    {"inertial"     ,t_INERTIAL},
    {"inout"        ,t_INOUT},
    {"is"           ,t_IS},
    {"label"        ,t_LABEL},
    {"library"      ,t_LIBRARY},
    {"linkage"      ,t_LINKAGE},
    {"literal"      ,t_LITERAL},
    {"loop"         ,t_LOOP},
    {"map"          ,t_MAP},
    {"mod"          ,t_MOD},
    {"nand"         ,t_NAND},
    {"new"          ,t_NEW},
    {"next"         ,t_NEXT},
    {"nor"          ,t_NOR},
    {"not"          ,t_NOT},
    {"null"         ,t_NULL},
    {"of"           ,t_OF},
    {"on"           ,t_ON},
    {"open"         ,t_OPEN},
    {"or"           ,t_OR},
    {"others"       ,t_OTHERS},
    {"out"          ,t_OUT},
    {"package"      ,t_PACKAGE},
    {"port"         ,t_PORT},
    {"postponed"    ,t_POSTPONED},
    {"procedure"    ,t_PROCEDURE},
    {"process"      ,t_PROCESS},
    {"protected"    ,t_PROTECTED},
    {"pure"         ,t_PURE},
    {"range"        ,t_RANGE},
    {"record"       ,t_RECORD},
    {"register"     ,t_REGISTER},
    {"reject"       ,t_REJECT},
    {"rem"          ,t_REM},
    {"report"       ,t_REPORT},
    {"return"       ,t_RETURN},
    {"rol"          ,t_ROL},
    {"ror"          ,t_ROR},
    {"select"       ,t_SELECT},
    {"severity"     ,t_SEVERITY},
    {"shared"       ,t_SHARED},
    {"signal"       ,t_SIGNAL},
    {"sla"          ,t_SLA},
    {"sll"          ,t_SLL},
    {"sra"          ,t_SRA},
    {"srl"          ,t_SRL},
    {"subtype"      ,t_SUBTYPE},
    {"then"         ,t_THEN},
    {"to"           ,t_TO},
    {"transport"    ,t_TRANSPORT},
    {"type"         ,t_TYPE},
    {"unaffected"   ,t_UNAFFECTED},
    {"units"        ,t_UNITS},
    {"until"        ,t_UNTIL},
    {"use"          ,t_USE},
    {"variable"     ,t_VARIABLE},
    {"wait"         ,t_WAIT},
    {"when"         ,t_WHEN},
    {"while"        ,t_WHILE},
    {"with"         ,t_WITH},
    {"xnor"         ,t_XNOR},
    {"xor"          ,t_XOR}
};

static el_mc psl_tokens []=
{
    {"A"                ,t_A},
    {"AF"               ,t_AF},
    {"AG"               ,t_AG},
    {"AT"               ,t_AG},                 // vhdl
    {"AX"               ,t_AX},
    {"E"                ,t_E},
    {"EF"               ,t_EF},
    {"EG"               ,t_EG},
    {"EX"               ,t_EX},
    {"F"                ,t_F},
    {"G"                ,t_G},
    {"U"                ,t_U},
    {"UNION"            ,t_UNION},
    {"W"                ,t_W},
    {"X"                ,t_X},
    {"X!"               ,t_X_EXCL},
    {"abort"            ,t_ABORT},
    {"always"           ,t_ALWAYS},
    {"and"              ,t_AND},                // vhdl
    {"assert"           ,t_ASSERT},             // vhdl
    {"assume"           ,t_ASSUME},
    {"async_abort"      ,t_ASYNC_ABORT},
    {"before"           ,t_BEFORE},
    {"before!"          ,t_BEFORE_EXCL},
    {"before!_"         ,t_BEFORE_EXCL_UNDERSCORE},
    {"before_"          ,t_BEFORE_UNDERSCORE},
    {"bit"              ,t_BIT},
    {"bitvector"        ,t_BITVECTOR},
    {"boolean"          ,t_BOOLEAN},
    {"clock"            ,t_CLOCK},
    {"const"            ,t_CONST},
    {"countnes"         ,t_COUNTONES},
    {"cover"            ,t_COVER},
    {"default"          ,t_DEFAULT},
    {"ended"            ,t_ENDED},
    {"eventually!"      ,t_EVENTUALLY_EXCL},
    {"fairness"         ,t_FAIRNESS},
    {"fell"             ,t_FELL},
    {"for"              ,t_FOR},
    {"forall"           ,t_FORALL},
    {"free"             ,t_FREE},
    {"hdltype"          ,t_HDLTYPE},
    {"in"               ,t_IN},                 // vhdl
    {"inf"              ,t_INF},
    {"inherit"          ,t_INHERIT},
    {"is"               ,t_IS},                 // vhdl
    {"isunknown"        ,t_ISUNKNOWN},
    {"mutable"          ,t_MUTABLE},
    {"never"            ,t_NEVER},
    {"next"             ,t_NEXT},               // vhdl
    {"next!"            ,t_NEXT_EXCL},
    {"next_a"           ,t_NEXT_A},
    {"next_a!"          ,t_NEXT_A_EXCL},
    {"next_e"           ,t_NEXT_E},
    {"next_e!"          ,t_NEXT_E_EXCL},
    {"next_event"       ,t_NEXT_EVENT},
    {"next_event!"      ,t_NEXT_EVENT_EXCL},
    {"next_event_a"     ,t_NEXT_EVENT_A},
    {"next_event_a!"    ,t_NEXT_EVENT_A_EXCL},
    {"next_event_e"     ,t_NEXT_EVENT_E},
    {"next_event_e!"    ,t_NEXT_EVENT_E_EXCL},
    {"nondet"           ,t_NONDET},
    {"nondet_vector"    ,t_NONDET_VECTOR},
    {"nontransitive"    ,t_NONTRANSITIVE},
    {"not"              ,t_NOT},                // vhdl
    {"numeric"          ,t_NUMERIC},
    {"onehot"           ,t_ONEHOT},
    {"onehot0"          ,t_ONEHOT0},
    {"or"               ,t_OR},                 // vhdl
    {"override"         ,t_OVERRIDE},
    {"prev"             ,t_PREV},
    {"property"         ,t_PROPERTY},
    {"report"           ,t_REPORT},             // vhdl
    {"restrict"         ,t_RESTRICT},
    {"restrict!"        ,t_RESTRICT_EXCL},
    {"rose"             ,t_ROSE},
    {"sequence"         ,t_SEQUENCE},
    {"stable"           ,t_STABLE},
    {"string"           ,t_STRING},
    {"strong"           ,t_STRONG},
    {"sync_abort"       ,t_SYNC_ABORT},
    {"to"               ,t_TO},                 // vhdl
    {"until"            ,t_UNTIL},              // vhdl
    {"until!"           ,t_UNTIL_EXCL},
    {"until!_"          ,t_UNTIL_EXCL_UNDERSCORE},
    {"until_"           ,t_UNTIL_UNDERSCORE},
    {"vmode"            ,t_VMODE},
    {"vpkg"             ,t_VPKG},
    {"vprop"            ,t_VPROP},
    {"vunit"            ,t_VUNIT},
    {"when"             ,t_WHEN},
    {"within"           ,t_WITHIN}
};


typedef int compare_function_type(const void *, const void *);

///
/// @param s    source-file string ( {letter}(_?{letter_or_digit})* )
/// @return -1 if is not a VHDL keyword, VHDL lexer token otherwise
///
static int getVHDLToken(std::string s)
{
    el_mc *pt = nullptr;

    // VHDL is case-insensitive
    s = stringToLower(s);

    void * bs = bsearch(s.c_str(), static_cast<void *>(vhdl_tokens), VHDL_TOKENS_TABLE_LEN,
            sizeof(el_mc), reinterpret_cast<compare_function_type *>(strcmp));

    pt = static_cast<el_mc *>( bs );

    return pt == nullptr ? -1 : pt->kval;
}

///
/// @param s    source-file string ( {letter}(_?{letter_or_digit_or_special})* )
/// @return -1 if is not a PSL keyword, PSL lexer token otherwise
///
static int getPSLToken(std::string s)
{
    el_mc *pt = nullptr;

    // PSL is case-sensitive
    void * bs = bsearch(s.c_str(), static_cast<void *>(psl_tokens), PSL_TOKENS_TABLE_LEN,
            sizeof(el_mc), reinterpret_cast<compare_function_type *>(strcmp));

    pt = static_cast<el_mc *>( bs );

    return pt == nullptr ? -1 : pt->kval;
}

// Store and return the index of the last recognized token
static int setAndReturnToken( int tok )
{
    lastToken = tok;
    return tok;
}

%}

/* 
 *  DEFINITIONS SECTION
 * ------------------------------------------------------------------------------ */

/* Name Definitions */
upper_case_letter                   [A-Z]
lower_case_letter                   [a-z]
digit                               [0-9]
special_character                   [\#\&\'\(\)\*\+\,\-\.\/\:\;\<\=\>\_\|]
space_character                     [ \t]
format_effector                     [\t\v\r\l\f]
end_of_line                         \n
other_special_character             [\!\$\@\?\[\\\]\^\`\{\}\~]

graphic_character                   ({basic_graphic_character}|{lower_case_letter}|{other_special_character})
basic_graphic_character             ({upper_case_letter}|{digit}|{special_character}|{space_character})
letter                              ({upper_case_letter}|{lower_case_letter})
letter_or_digit                     ({letter}|{digit})
letter_or_digit_or_special          ({letter}|{digit}|!|_)
decimal_literal                     {integer}(\.{integer})?({exponent})?
integer                             {digit}(_?{digit})*
exponent                            ([eE][-+]?{integer})
base                                {integer}
based_integer                       {extended_digit}(_?{extended_digit})*
extended_digit                      ({digit}|[a-fA-F])
base_specifier                      (B|b|O|o|X|x)
hex_specifier                       (X|x)
bit_specifier                       (B|b)
oct_specifier                       (O|o)
bit                                 [01]
oct                                 [0-7]

WC [ \t\b\f\r\v]+

/* Exclusive Start Conditions */
%x SKIP_TO_EOL
%option never-interactive

%option noyywrap

%%
 /* 
  *  RULES SECTION
  * ------------------------------------------------------------------------------ */

<<EOF>>             { return setAndReturnToken(0); }

{WC}                { yycolumno += static_cast<int>(strlen(yytext)); }

<SKIP_TO_EOL>.*$    {
    yycolumno += static_cast<int>(strlen(yytext));
    /*++yylineno;*/
    BEGIN(INITIAL);
}
<SKIP_TO_EOL>{WC}   { yycolumno += static_cast<int>(strlen(yytext)); }

{space_character}   { yycolumno++; }

\-\>                { yycolumno += 2;   return setAndReturnToken(t_BOOLEAN_IMPLICATION);         }
\<\-\>              { yycolumno += 3;   return setAndReturnToken(t_DOUBLE_BOOLEAN_IMPLICATION);  }
\|\-\>              { yycolumno += 3;   return setAndReturnToken(t_SEQUENCE_IMPLICATION);        }
\|=\>               { yycolumno += 3;   return setAndReturnToken(t_DOUBLE_SEQUENCE_IMPLICATION); }
\&\&                { yycolumno += 2;   return setAndReturnToken(t_DOUBLE_AND);                  }
\[\[                { yycolumno += 2;   return setAndReturnToken(t_DOUBLE_OPEN_BRACKETS);        }
\]\]                { yycolumno += 2;   return setAndReturnToken(t_DOUBLE_CLOSE_BRACKETS);       }

\&                  { yycolumno++;      return setAndReturnToken(t_Ampersand);    }
\'                  { yycolumno++;      return setAndReturnToken(t_Apostrophe);   }
\(                  { yycolumno++;      return setAndReturnToken(t_LeftParen);    }
\)                  { yycolumno++;      return setAndReturnToken(t_RightParen);   }
\*\*                { yycolumno += 2;   return setAndReturnToken(t_DoubleStar);   }
\*                  { yycolumno++;      return setAndReturnToken(t_Star);         }
\+                  { yycolumno++;      return setAndReturnToken(t_Plus);         }
\,                  { yycolumno++;      return setAndReturnToken(t_Comma);        }
\-                  { yycolumno++;      return setAndReturnToken(t_Minus);        }
\:=                 { yycolumno += 2;   return setAndReturnToken(t_VarAsgn);      }
\:                  { yycolumno++;      return setAndReturnToken(t_Colon);        }
\;                  { yycolumno++;      return setAndReturnToken(t_Semicolon);    }
\<=                 { yycolumno += 2;   return setAndReturnToken(t_LESym);        }
\>=                 { yycolumno += 2;   return setAndReturnToken(t_GESym);        }
\<                  { yycolumno++;      return setAndReturnToken(t_LTSym);        }
\>                  { yycolumno++;      return setAndReturnToken(t_GTSym);        }
=\>                 { yycolumno += 2;   return setAndReturnToken(t_Arrow);        }
=                   { yycolumno++;      return setAndReturnToken(t_EQSym);        }
\/=                 { yycolumno += 2;   return setAndReturnToken(t_NESym);        }
\/                  { yycolumno++;      return setAndReturnToken(t_Slash);        }
\|                  { yycolumno++;      return setAndReturnToken(t_Bar);          }
!                   { yycolumno++;      return setAndReturnToken(t_Excl);         }
\.                  { yycolumno++;      return setAndReturnToken(t_Dot);          }
\[                  { yycolumno++;      return setAndReturnToken(t_LeftBracket);  }
\]                  { yycolumno++;      return setAndReturnToken(t_RightBracket); }
\{                  { yycolumno++;      return setAndReturnToken(t_LeftBrace);    }
\}                  { yycolumno++;      return setAndReturnToken(t_RightBrace);   }
\@                  { yycolumno++;      return setAndReturnToken(t_AT);           }


 /*{letter}(_?{letter_or_digit})* { */
{letter}(_?{letter_or_digit_or_special})* {

    //
    // This regex matches:
    // - vhdl/psl identifiers
    // - vhdl/psl keywords
    //

    int itoken = -1;

    if ( parserInstance->getContext() == VhdlParser::PSL_ctx )
    {
        // ** PSL MODE **

        int psl_itoken = getPSLToken( yytext );
        int vhdl_itoken = getVHDLToken( yytext );

        // no keyword
        if ( psl_itoken == -1 && vhdl_itoken == -1 )
        {
            // no-keyword tokens are returned as simple identifiers
            itoken = -1;
        }
        // keyword only in VHDL
        else if ( psl_itoken == -1 && vhdl_itoken != -1 )
        {
            itoken = vhdl_itoken;
        }
        // keyword in PSL and VHDL
        else if ( psl_itoken != -1 && vhdl_itoken != -1 )
        {
            itoken = psl_itoken;
        }
        // keyword only in PSL
        else if ( psl_itoken != -1 && vhdl_itoken == -1 )
        {
            //
            // Hierarchical identifiers may contain PSL keywords.
            // If a token recognized as psl-keyword follows the token 't_Dot',
            // then it is returned as a simple identifier.
            // This way properties can refer to identifiers of the related design
            // that are keyword in PSL.
            //
            if ( lastToken != t_Dot )
                itoken = psl_itoken;
            else
                itoken = -1;
        }
    }
    else
    {
        // ** VHDL MODE **

        if ( strcmp( yytext, "library" ) == 0 || strcmp( yytext, "LIBRARY" ) == 0 ||
             strcmp( yytext, "use" ) == 0 || strcmp( yytext, "USE" ) == 0 ||
             strcmp( yytext, "architecture" ) == 0 || strcmp( yytext, "ARCHITECTURE" ) == 0 || 
             strcmp( yytext, "package" ) == 0 || strcmp( yytext, "PACKAGE" ) == 0 ||
             strcmp( yytext, "entity" ) == 0 || strcmp( yytext, "ENTITY" ) == 0 ||
             strcmp( yytext, "configuration" ) == 0 || strcmp( yytext, "CONFIGURATION" ) == 0 )
        {
            // After one of these VHDL keywords we exit from the global-scope
            parserInstance->setGlobalScope(false);
        }

        if ( parserInstance->isGlobalScope() )
        {
            //
            // If we are in global scope, identifiers "vunit", "vpkg", "vprop" and "vmode"
            // are valid PSL keywords and they starts a verification-unit  (package etc ..)
            // within a VHDL module. In 'vunit_type' rule, the parser invokes the function
            // parserContextSwitch() to switch in PSL mode. 
            //
            //
            // NOTE: PSL tokens are case-sensitive
            // (VUNIT, VPKG, VPROP, VMODE are valid VHDL identifiers)
            //
            if ( strcmp( yytext, "vunit" ) == 0 )
                itoken = t_VUNIT;
            else if ( strcmp( yytext, "vpkg" ) == 0 )
                itoken = t_VPKG;
            else if ( strcmp( yytext, "vprop" ) == 0 )
                itoken = t_VPROP;
            else if ( strcmp( yytext, "vmode" ) == 0 )
                itoken = t_VMODE;
            else
                itoken = getVHDLToken( yytext );
        }
        else
        {
            //
            // If we are inside a vhdl global statement (architecture, entity etc...),
            // we do not care about PSL keywords.
            //
            itoken = getVHDLToken( yytext );
        }
    }

    if ( itoken == -1 )
    {
        std::string identifier = stringToLower(yytext);

        // No keyword recognized
        yylval.Identifier_data.column = yycolumno;
        yylval.Identifier_data.len = static_cast<int>(strlen(yytext));
        yylval.Identifier_data.line = yylineno;
        yylval.Identifier_data.name = hif::application_utils::hif_strdup(identifier.c_str());

        yycolumno += static_cast<int>(identifier.size());

        #ifdef LEXER_VERBOSE_MODE
            *debugStream << "[ IDENTIFIER : " << yytext << " ] " << std::endl;
        #endif

        return setAndReturnToken( t_Identifier );
    }
    else
    {
        yylval.Keyword_data.line = yylineno;
        yylval.Keyword_data.column = yycolumno;

        yycolumno += static_cast<int>(strlen(yytext));

        #ifdef LEXER_VERBOSE_MODE
            *debugStream << "[ KEYWORD : " << yytext << " ] " << std::endl;
        #endif

        return setAndReturnToken(itoken);
    }
}



({base}#{based_integer}(\.{based_integer})?#({exponent})?) {

    yylval.Identifier_data.column = yycolumno;
    yylval.Identifier_data.len = static_cast<int>(strlen(yytext));
    yylval.Identifier_data.line = yylineno;
    yylval.Identifier_data.name = hif::application_utils::hif_strdup(yytext);

    yycolumno += static_cast<int>(strlen(yytext));
    
#ifdef LEXER_VERBOSE_MODE
    *debugStream << "[ BASED LITERAL: " << yylval.Identifier_data.name << " ] " << std::endl;
#endif
    
    return setAndReturnToken(t_BasedLit);
}

({decimal_literal})|({base}:{based_integer}(\.{based_integer})?:({exponent})?) {
  
    yylval.Identifier_data.column = yycolumno;
    yylval.Identifier_data.len = static_cast<int>(strlen(yytext));
    yylval.Identifier_data.line = yylineno;
    yylval.value = hif::application_utils::hif_strdup(yytext);

    yycolumno += static_cast<int>(strlen(yytext));
    
#ifdef LEXER_VERBOSE_MODE
    *debugStream << "[ ABSTRACT LITERAL: " << yylval.value << " ] " << std::endl;
#endif

    return setAndReturnToken(t_AbstractLit);
}

'({graphic_character}|\"|\%)' {

    yylval.Identifier_data.column = yycolumno;
    yylval.Identifier_data.len = static_cast<int>(strlen(yytext));
    yylval.Identifier_data.line = yylineno;
    yylval.Identifier_data.name = hif::application_utils::hif_strdup(yytext);

    yycolumno += static_cast<int>(strlen(yytext));

#ifdef LEXER_VERBOSE_MODE
    *debugStream << "[ CHARACTER LITERAL: " << yylval.Identifier_data.name << " ] " << std::endl;
#endif

    return setAndReturnToken(t_CharacterLit);
}

(\"({graphic_character}|(\"\")|\%)*\")|(\%({graphic_character}|(\%\%)|\")*\%) {

    yylval.Identifier_data.column = yycolumno;
    yylval.Identifier_data.len = static_cast<int>(strlen(yytext));
    yylval.Identifier_data.line = yylineno;
    yylval.Identifier_data.name = hif::application_utils::hif_strdup(yytext);

    yycolumno += static_cast<int>(strlen(yytext));

#ifdef LEXER_VERBOSE_MODE
    *debugStream << "[ STRING LITERAL: " << yylval.Identifier_data.name << " ] " << std::endl;
#endif

    return setAndReturnToken(t_StringLit);
}

{hex_specifier}(\"{extended_digit}(_?{extended_digit})*\"|\%{extended_digit}(_?{extended_digit})*\%) {

    yylval.Identifier_data.column = yycolumno;
    yylval.Identifier_data.len = static_cast<int>(strlen(yytext));
    yylval.Identifier_data.line = yylineno;
    yylval.Identifier_data.name = hif::application_utils::hif_strdup(yytext);

    yycolumno += static_cast<int>(strlen(yytext));

#ifdef LEXER_VERBOSE_MODE
    *debugStream << "[ HEX STRING LITERAL: " << yylval.Identifier_data.name << " ] " << std::endl;
#endif
    
    return setAndReturnToken(t_HexStringLit);
}

{oct_specifier}(\"{oct}(_?{oct})*\"|\%{oct}(_?{oct})*\%) {

    yylval.Identifier_data.column = yycolumno;
    yylval.Identifier_data.len = static_cast<int>(strlen(yytext));
    yylval.Identifier_data.line = yylineno;

    std::string temp = yytext;
    yylval.Identifier_data.name =
            hif::application_utils::hif_strdup(temp.substr(2, strlen(yytext)-3).c_str());

    yycolumno += static_cast<int>(strlen(yytext));

#ifdef LEXER_VERBOSE_MODE
    *debugStream << "OCT STRING LITERAL: " << yylval.Identifier_data.name << std::endl;
#endif

    return setAndReturnToken(t_OctStringLit);
}

{bit_specifier}(\"{bit}(_?{bit})*\"|\%{bit}(_?{bit})*\%) {

    yylval.Identifier_data.column = yycolumno;
    yylval.Identifier_data.len = static_cast<int>(strlen(yytext));
    yylval.Identifier_data.line = yylineno;
    yylval.Identifier_data.name = hif::application_utils::hif_strdup(yytext);

    yycolumno += static_cast<int>(strlen(yytext));

#ifdef LEXER_VERBOSE_MODE
    *debugStream << "[ BIT STRING LITERAL: " << yylval.Identifier_data.name << " ] " << std::endl;
#endif

    return setAndReturnToken(t_BitStringLit);
}


\n {
    yylineno++;
    yycolumno = 1;
}

-- {
    yycolumno += static_cast<int>(strlen(yytext));
  
    /* return t_COMMENT; */
    BEGIN(SKIP_TO_EOL);
}


%%
/*
. {
    yycolumno += static_cast<int>(strlen(yytext));
}
*/


/*
{base_specifier}(\"{extended_digit}(_?{extended_digit})*\"|\%{extended_digit}(_?{extended_digit})*\%) {
    yycolumno += static_cast<int>(strlen(yytext));
    return t_BitStringLit;
}
*/

#ifdef YY_NUM_RULES
#endif
#ifdef REJECT
#endif
#ifdef INT8_MAX
#endif
#ifdef YY_FLEX_MAJOR_VERSION
#endif
#ifdef BEGIN
#endif
#ifdef INT16_MAX
#endif
#ifdef yy_new_buffer
#endif
#ifdef INT8_MIN
#endif
#ifdef YY_LESS_LINENO
#endif
#ifdef INT16_MIN
#endif
#ifdef yyless
#endif
#ifdef FLEX_BETA
#endif
#ifdef yy_set_bol
#endif
#ifdef YYTABLES_NAME
#endif
#ifdef FLEXINT_H
#endif
#ifdef yymore
#endif
#ifdef FLEX_SCANNER
#endif
#ifdef YY_STRUCT_YY_BUFFER_STATE
#endif
#ifdef YY_TYPEDEF_YY_BUFFER_STATE
#endif
#ifdef INT32_MIN
#endif
#ifdef UINT32_MAX
#endif
#ifdef yy_set_interactive
#endif
#ifdef YYSTATE
#endif
#ifdef YY_FLEX_MINOR_VERSION
#endif
#ifdef INT32_MAX
#endif
#ifdef YY_DECL_IS_OURS
#endif
#ifdef YY_AT_BOL
#endif
#ifdef YY_FLUSH_BUFFER
#endif
#ifdef UINT8_MAX
#endif
#ifdef UINT16_MAX
#endif
#ifdef unput
#endif
#ifdef YY_TYPEDEF_YY_SIZE_T
#endif
#ifdef YY_INT_ALIGNED
#endif
#ifdef YY_START_STACK_INCR
#endif

