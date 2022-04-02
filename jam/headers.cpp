/*
 * Copyright 1993, 2000 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * headers.c - handle #includes in source files
 *
 * Using regular expressions provided as the variable $(HDRSCAN), 
 * headers() searches a file for #include files and phonies up a
 * rule invocation:
 * 
 *	$(HDRRULE) <target> : <include files> ;
 *
 * External routines:
 *    headers() - scan a target for include files and call HDRRULE
 *
 * Internal routines:
 *    headers1() - using regexp, scan a file and build include LIST
 *
 * 04/13/94 (seiwald) - added shorthand L0 for null list pointer
 * 09/10/00 (seiwald) - replaced call to compile_rule with evaluate_rule,
 *		so that headers() doesn't have to mock up a parse structure
 *		just to invoke a rule.
 * 03/02/02 (seiwald) - rules can be invoked via variable names
 * 10/22/02 (seiwald) - list_new() now does its own newstr()/copystr()
 * 11/04/02 (seiwald) - const-ing for string literals
 * 12/09/02 (seiwald) - push regexp creation down to headers1().
 */

# include "jam.h"
# include "lists.h"
# include "parse.h"
# include "compile.h"
# include "rules.h"
# include "variable.h"
# include "regexp.h"
# include "headers.h"
# include "newstr.h"

#ifdef OPT_HEADER_CACHE_EXT
# include "hcache.h"
#endif

#ifndef OPT_HEADER_CACHE_EXT
static StringList headers1( const char *file, StringList hdrscan );
#endif

/*
 * headers() - scan a target for include files and call HDRRULE
 */

# define MAXINC 10

void
headers( TARGET *t )
{
	StringList hdrscan;
	StringList hdrrule;
	StringList hdrcache;
	LOL	lol;

	if( ( hdrscan = var_get( "HDRSCAN" )).Size() == 0  || 
	    ( hdrrule = var_get( "HDRRULE" )).Size() == 0 )
	        return;

	/* Doctor up call to HDRRULE rule */
	/* Call headers1() to get LIST of included files. */

	if( DEBUG_HEADER )
	    printf( "header scan %s\n", t->name );

	lol_init( &lol );

	lol_add( &lol, StringList(t->name) );
#ifdef OPT_HEADER_CACHE_EXT
	lol_add( &lol, hcache( t, hdrscan ) );
#else
	lol_add( &lol, headers1( t->boundname, hdrscan ) );
#endif

	if( lol_get( &lol, 1 ).Size() > 0 )
	{
	    int jmp = JMP_NONE;
	    evaluate_rule( hdrrule.CStringAt(0), &lol, {}, &jmp );
	}

	/* Clean up */

	lol_free( &lol );
}

/*
 * headers1() - using regexp, scan a file and build include LIST
 */

#ifdef OPT_HEADER_CACHE_EXT
StringList
#else
static StringList
#endif
headers1( 
	const char *file,
	StringList hdrscan )
{
	FILE	*f;
	int	i;
	int	rec = 0;
	StringList result = {};
	regexp	*re[ MAXINC ];
	char	buf[ 1024 ];

	if( !( f = fopen( file, "r" ) ) )
	    return result;

	size_t offset = 0;
	while( rec < MAXINC && offset < hdrscan.Size() )
	{
	    re[rec++] = regcomp( hdrscan.CStringAt(offset) );
		++offset;
	}

	while( fgets( buf, sizeof( buf ), f ) )
	{
	    for( i = 0; i < rec; i++ )
		if( regexec( re[i], buf ) && re[i]->startp[1] )
	    {
		/* Copy and terminate extracted string. */

		char buf2[ MAXSYM ];
		int l = re[i]->endp[1] - re[i]->startp[1];
		if (l > MAXSYM) {
			printf("MAXSYM is too low! Need at least %d\n", l);
			exit(-1);
		}
		memcpy( buf2, re[i]->startp[1], l );
		buf2[ l ] = 0;
		result.Append(buf2);

		if( DEBUG_HEADER )
		    printf( "header found: %s\n", buf2 );
	    }
	}

	while( rec )
	    free( (char *)re[--rec] );

	fclose( f );

	return result;
}
