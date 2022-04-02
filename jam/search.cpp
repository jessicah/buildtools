/*
 * Copyright 1993-2002 Christopher Seiwald and Perforce Software, Inc.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * search.c - find a target along $(SEARCH) or $(LOCATE) 
 *
 * 11/04/02 (seiwald) - const-ing for string literals
 */

# include "jam.h"
# include "lists.h"
# include "search.h"
# include "timestamp.h"
# include "pathsys.h"
# include "variable.h"
# include "newstr.h"

const char *
search( 
	const char *target,
	time_t	*time )
{
	PATHNAME f[1];
	StringList varlist;
	char	buf[ MAXJPATH ];

	/* Parse the filename */

	path_parse( target, f );

	f->f_grist.ptr = 0;
	f->f_grist.len = 0;

	if( (varlist = var_get( "LOCATE" )).Empty() == false )
	{
		printf("SEARCH: in `LOCATE`, %u items:\n", varlist.Size());
		printf("  %s\n", varlist.CStringAt(0));
	    f->f_root.ptr = varlist.CStringAt(0);
	    f->f_root.len = varlist.StringAt(0).size();

	    path_build( f, buf, 1 );

	    if( DEBUG_SEARCH )
		printf( "locate %s: %s\n", target, buf );

	    timestamp( buf, time );

	    return newstr( buf );
	}
	else if( (varlist = var_get( "SEARCH" )).Empty() == false)
	{
		for (size_t offset = 0; offset < varlist.Size(); ++offset)
	    {
		f->f_root.ptr = varlist.CStringAt(offset);
		f->f_root.len = varlist.StringAt(offset).size();

		path_build( f, buf, 1 );

		if( DEBUG_SEARCH )
		    printf( "search %s: %s\n", target, buf );

		timestamp( buf, time );

		if( *time )
		    return newstr( buf );
	    }
	}

	/* Look for the obvious */
	/* This is a questionable move.  Should we look in the */
	/* obvious place if SEARCH is set? */

	f->f_root.ptr = 0;
	f->f_root.len = 0;

	path_build( f, buf, 1 );

	if( DEBUG_SEARCH )
	    printf( "search %s: %s\n", target, buf );

	timestamp( buf, time );

	return newstr( buf );
}
