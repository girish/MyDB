/***********************************************************************
 * SECTION 1
 ***********************************************************************/
/* The code in %{ %} is included as it is in lex.yyold.c file
 * it has C global variables, prototypes, and comments 
 */


%{

#include <string.h> // For strdup()
#include <stdlib.h> // For malloc()
#include "ParseTree.h"
#include "yyold.tab.h"

//int yyoldlineno = 1;
void yyolderror(char*s);

static YY_BUFFER_STATE yyold_buf_state;
void init_lexical_parser_old (char *src) { yyold_buf_state = yyold_scan_string (src); }
void close_lexical_parser_old () { yyold_delete_buffer (yyold_buf_state); }

%}


/******************************************************************************
 * SECTION 2
 ******************************************************************************/
/* This is the DEFINITION section which contains substitutions, code, and
 * start stats; will be copied into lex.yyold.c
 */

/******************************************************************************
 * SECTION 3
 ******************************************************************************/
/* This is the RULES section which defines how to "scan" and what action
 * to take for each token
 */

%%

"("			return('(');

")"    	        	return(')');

"<"    		      	return('<');

">"    		 	return('>');

"=" 			return('=');

-?[0-9]+ 	       {yyoldlval.actualChars = strdup(yyoldtext);
  			return(Int); 
		        }

-?[0-9]+\.[0-9]*       {yyoldlval.actualChars = strdup(yyoldtext); 
  			return(Float);
			}

"OR"      		return(OR);

"AND"			return(AND);

\'[^'\n]*\' 	       {/* take care of ' in a string */
	 		if (yyoldtext[yyoldleng - 2] == '\\') {
     				//yymore();
   			} else {
				yyoldlval.actualChars = strdup(yyoldtext + 1);	
				yyoldlval.actualChars[strlen(yyoldlval.actualChars) - 1] = 0;
     				return(String);
   			}
			}

[A-Za-z][A-Za-z0-9_-]* {yyoldlval.actualChars = strdup(yyoldtext);
  			return(Name);
			}     
[A-Za-z][A-Za-z0-9_-]*\.[A-Za-z][A-Za-z0-9_-]* {yyoldlval.actualChars = strdup(yyoldtext);
            return(Name);
            }    
\n                	yyoldlineno++;

[ \t]             	;

.                 	yyolderror("LEX_ERROR: invalid character");

%%

void yyolderror(char *s) {
  printf("%d: %s at %s\n", yyoldlineno, s, yyoldtext);
}

int yyoldwrap(void){
  return 1;
}


