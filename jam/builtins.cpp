/*
 * Copyright 1993-2002 Christopher Seiwald and Perforce Software, Inc.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * builtins.c - builtin jam rules
 *
 * External routines:
 *
 * 	load_builtin() - define builtin rules
 *
 * Internal routines:
 *
 *	builtin_depends() - DEPENDS/INCLUDES rule
 *	builtin_echo() - ECHO rule
 *	builtin_exit() - EXIT rule
 *	builtin_flags() - NOCARE, NOTFILE, TEMPORARY rule
 *	builtin_glob() - GLOB rule
 *	builtin_match() - MATCH rule
 *
 * 01/10/01 (seiwald) - split from compile.c
 * 01/08/01 (seiwald) - new 'Glob' (file expansion) builtin
 * 03/02/02 (seiwald) - new 'Match' (regexp match) builtin
 * 04/03/02 (seiwald) - Glob matches only filename, not directory
 * 10/22/02 (seiwald) - list_new() now does its own newstr()/copystr()
 * 10/22/02 (seiwald) - working return/break/continue statements
 * 11/04/02 (seiwald) - const-ing for string literals
 * 12/03/02 (seiwald) - fix odd includes support by grafting them onto depends
 * 01/14/03 (seiwald) - fix includes fix with new internal includes TARGET
 */

# include "jam.h"

# include "lists.h"
# include "parse.h"
# include "builtins.h"
# include "rules.h"
# include "filesys.h"
# include "newstr.h"
# include "regexp.h"
# include "pathsys.h"

#include <iostream>
#include <stdlib.h>

/*
 * compile_builtin() - define builtin rules
 */

StringList builtin_depends( PARSE *parse, LOL *args, int *jmp );
StringList builtin_echo( PARSE *parse, LOL *args, int *jmp );
StringList builtin_exit( PARSE *parse, LOL *args, int *jmp );
StringList builtin_flags( PARSE *parse, LOL *args, int *jmp );
//StringList builtin_glob( PARSE *parse, LOL *args, int *jmp );
StringList builtin_match( PARSE *parse, LOL *args, int *jmp );

int glob( const char *s, const char *c );

void
load_builtins()
{
    bindrule( "Always" )->procedure = 
    bindrule( "ALWAYS" )->procedure = 
	parse_make( builtin_flags, nullptr, nullptr, nullptr, nullptr, nullptr, T_FLAG_TOUCHED );

    bindrule( "Depends" )->procedure = 
    bindrule( "DEPENDS" )->procedure = 
	parse_make( builtin_depends, nullptr, nullptr, nullptr, nullptr, nullptr, 0 );

    bindrule( "echo" )->procedure = 
    bindrule( "Echo" )->procedure = 
    bindrule( "ECHO" )->procedure = 
	parse_make( builtin_echo, nullptr, nullptr, nullptr, nullptr, nullptr, 0 );

    bindrule( "exit" )->procedure = 
    bindrule( "Exit" )->procedure = 
    bindrule( "EXIT" )->procedure = 
	parse_make( builtin_exit, nullptr, nullptr, nullptr, nullptr, nullptr, 0 );

    /*bindrule( "Glob" )->procedure = 
    bindrule( "GLOB" )->procedure = 
	parse_make( builtin_glob, nullptr, nullptr, nullptr, nullptr, nullptr, 0 );*/

    bindrule( "Includes" )->procedure = 
    bindrule( "INCLUDES" )->procedure = 
	parse_make( builtin_depends, nullptr, nullptr, nullptr, nullptr, nullptr, 1 );

    bindrule( "Leaves" )->procedure = 
    bindrule( "LEAVES" )->procedure = 
	parse_make( builtin_flags, nullptr, nullptr, nullptr, nullptr, nullptr, T_FLAG_LEAVES );

    bindrule( "Match" )->procedure = 
    bindrule( "MATCH" )->procedure = 
	parse_make( builtin_match, nullptr, nullptr, nullptr, nullptr, nullptr, 0 );

    bindrule( "NoCare" )->procedure = 
    bindrule( "NOCARE" )->procedure = 
	parse_make( builtin_flags, nullptr, nullptr, nullptr, nullptr, nullptr, T_FLAG_NOCARE );

    bindrule( "NOTIME" )->procedure = 
    bindrule( "NotFile" )->procedure = 
    bindrule( "NOTFILE" )->procedure = 
	parse_make( builtin_flags, nullptr, nullptr, nullptr, nullptr, nullptr, T_FLAG_NOTFILE );

    bindrule( "NoUpdate" )->procedure = 
    bindrule( "NOUPDATE" )->procedure = 
	parse_make( builtin_flags, nullptr, nullptr, nullptr, nullptr, nullptr, T_FLAG_NOUPDATE );

    bindrule( "Temporary" )->procedure = 
    bindrule( "TEMPORARY" )->procedure = 
	parse_make( builtin_flags, nullptr, nullptr, nullptr, nullptr, nullptr, T_FLAG_TEMP );
}

/*
 * builtin_depends() - DEPENDS/INCLUDES rule
 *
 * The DEPENDS builtin rule appends each of the listed sources on the 
 * dependency list of each of the listed targets.  It binds both the 
 * targets and sources as TARGETs.
 */

StringList
builtin_depends(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	StringList targets = lol_get( args, 0 );
	StringList sources = lol_get( args, 1 );
	StringList l;

	for (size_t offset = 0; offset < targets.Size(); ++offset)
	{
	    TARGET *t = bindtarget( targets[offset].get().c_str() );

	    /* If doing INCLUDES, switch to the TARGET's include */
	    /* TARGET, creating it if needed.  The internal include */
	    /* TARGET shares the name of its parent. */

	    if( parse->num )
	    {
		if( !t->includes )
		    t->includes = copytarget( t );
		t = t->includes;
	    }

	    t->depends = targetlist( t->depends, sources );
	}

	return {};
}

/*
 * builtin_echo() - ECHO rule
 *
 * The ECHO builtin rule echoes the targets to the user.  No other 
 * actions are taken.
 */

StringList
builtin_echo(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	std::cout << lol_get(args, 0) << std::endl;

	return {};
}

/*
 * builtin_exit() - EXIT rule
 *
 * The EXIT builtin rule echoes the targets to the user and exits
 * the program with a failure status.
 */

StringList
builtin_exit(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	std::cout << lol_get(args, 0) << std::endl;
	exit( EXITBAD ); /* yeech */
	
	return {};
}

/*
 * builtin_flags() - NOCARE, NOTFILE, TEMPORARY rule
 *
 * Builtin_flags() marks the target with the appropriate flag, for use
 * by make0().  It binds each target as a TARGET.
 */

StringList
builtin_flags(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	StringList l = lol_get( args, 0 );

	for(size_t offset = 0; offset < l.Size(); ++offset)
	{
		bindtarget( l[0].get().c_str() )->flags |= parse->num;
	}

	return {};
}

#if 0
/*
 * builtin_globbing() - GLOB rule
 */

struct globbing {
	const StringList &patterns;
	StringList &results;
} ;

static void
builtin_glob_back(
	void	*closure,
	const char *file,
	int	status,
	time_t	time )
{
	struct globbing *globbing = (struct globbing *)closure;
	LIST		*l;
	PATHNAME	f;
	char		buf[ MAXJPATH ];

	/* Null out directory for matching. */
	/* We wish we had file_dirscan() pass up a PATHNAME. */

	path_parse( file, &f );
	f.f_dir.len = 0;
	path_build( &f, buf, 0 );

	for( l = globbing->patterns; l; l = l->next )
	    if( !glob( l->string, buf ) )
	{
	    globbing->results = list_new( globbing->results, file, 0 );
	    break;
	}
}

LIST *
builtin_glob(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	LIST *l = lol_get( args, 0 );
	LIST *r = lol_get( args, 1 );

	struct globbing globbing;

	globbing.results = L0;
	globbing.patterns = r;

	for( ; l; l = list_next( l ) )
	    file_dirscan( l->string, builtin_glob_back, &globbing );

	return globbing.results;
}
#endif

/*
 * builtin_match() - MATCH rule, regexp matching
 */

StringList
builtin_match(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
    StringList left = lol_get(args, 0);
	StringList right = lol_get(args, 1);
    StringList result;

    /* For each pattern */

	for(size_t leftOffset = 0; leftOffset < left.Size(); ++leftOffset)
    {
	regexp *re = regcomp( left[leftOffset].get().c_str() );

	/* For each string to match against */

	for( size_t rightOffset = 0; rightOffset < right.Size(); ++rightOffset)
	    if( regexec( re, right[rightOffset].get().c_str() ) )
	    {
		int i, top;

		/* Find highest parameter */

		for( top = NSUBEXP; top-- > 1; )
		    if( re->startp[top] )
			break;

		/* And add all parameters up to highest onto list. */
		/* Must have parameters to have results! */

		for( i = 1; i <= top; i++ )
		{
		    char buf[ MAXSYM ];
		    int l = re->endp[i] - re->startp[i];
		    if (l > MAXSYM) {
			printf("MAXSYM is too low! NEed at least %d\n", l);
			exit(-1);
		    }
		    memcpy( buf, re->startp[i], l );
		    buf[ l ] = 0;
		    //result = list_new( result, buf, 0 );
			result.Append(buf);
		}
	    }

	free( (char *)re );
    }

    return result;
}
