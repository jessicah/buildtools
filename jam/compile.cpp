/*
 * Copyright 1993-2002 Christopher Seiwald and Perforce Software, Inc.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * compile.c - compile parsed jam statements
 *
 * External routines:
 *
 *	compile_append() - append list results of two statements
 *	compile_break() - compile 'break/continue/return' rule
 *	compile_eval() - evaluate if to determine which leg to compile
 *	compile_foreach() - compile the "for x in y" statement
 *	compile_if() - compile 'if' rule
 *	compile_include() - support for 'include' - call include() on file
 *	compile_list() - expand and return a list 
 *	compile_local() - declare (and set) local variables
 *	compile_null() - do nothing -- a stub for parsing
 *	compile_on() - run rule under influence of on-target variables
 *	compile_rule() - compile a single user defined rule
 *	compile_rules() - compile a chain of rules
 *	compile_set() - compile the "set variable" statement
 *	compile_setcomp() - support for `rule` - save parse tree 
 *	compile_setexec() - support for `actions` - save execution string 
 *	compile_settings() - compile the "on =" (set variable on exec) statement
 *	compile_switch() - compile 'switch' rule
 *
 * Internal routines:
 *
 *	debug_compile() - printf with indent to show rule expansion.
 *	evaluate_rule() - execute a rule invocation
 *
 * 02/03/94 (seiwald) -	Changed trace output to read "setting" instead of 
 *			the awkward sounding "settings".
 * 04/12/94 (seiwald) - Combined build_depends() with build_includes().
 * 04/12/94 (seiwald) - actionlist() now just appends a single action.
 * 04/13/94 (seiwald) - added shorthand L0 for null list pointer
 * 05/13/94 (seiwald) - include files are now bound as targets, and thus
 *			can make use of $(SEARCH)
 * 06/01/94 (seiwald) - new 'actions existing' does existing sources
 * 08/23/94 (seiwald) - Support for '+=' (append to variable)
 * 12/20/94 (seiwald) - NOTIME renamed NOTFILE.
 * 01/22/95 (seiwald) - Exit rule.
 * 02/02/95 (seiwald) - Always rule; LEAVES rule.
 * 02/14/95 (seiwald) - NoUpdate rule.
 * 01/20/00 (seiwald) - Upgraded from K&R to ANSI C
 * 09/07/00 (seiwald) - stop crashing when a rule redefines itself
 * 09/11/00 (seiwald) - new evaluate_rule() for headers().
 * 09/11/00 (seiwald) - rules now return values, accessed via [ rule arg ... ]
 * 09/12/00 (seiwald) - don't complain about rules invoked without targets
 * 01/13/01 (seiwald) - fix case where rule is defined within another
 * 01/10/01 (seiwald) - built-ins split out to builtin.c.
 * 01/11/01 (seiwald) - optimize compile_rules() for tail recursion
 * 01/21/01 (seiwald) - replace evaluate_if() with compile_eval()
 * 01/24/01 (seiwald) - 'while' statement
 * 03/23/01 (seiwald) - "[ on target rule ]" support
 * 02/28/02 (seiwald) - merge EXEC_xxx flags in with RULE_xxx 
 * 03/02/02 (seiwald) - rules can be invoked via variable names
 * 03/12/02 (seiwald) - &&,&,||,|,in now short-circuit again
 * 03/25/02 (seiwald) - if ( "" a b ) one again returns true
 * 06/21/02 (seiwald) - support for named parameters
 * 10/22/02 (seiwald) - list_new() now does its own newstr()/copystr()
 * 10/22/02 (seiwald) - working return/break/continue statements
 * 11/04/02 (seiwald) - const-ing for string literals
 * 11/18/02 (seiwald) - remove bogus search() in 'on' statement.
 * 12/17/02 (seiwald) - new copysettings() to protect target-specific vars
 */

# ifdef OPT_RULE_PROFILING_EXT
# include <sys/time.h>
# endif

# include "jam.h"

# include "lists.h"
# include "parse.h"
# include "compile.h"
# include "variable.h"
# include "expand.h"
# include "rules.h"
# include "newstr.h"
# include "search.h"

static const char *set_names[] = { "=", "+=", "?=" };
static void debug_compile( int which, const char *s );
int glob( const char *s, const char *c );



/*
 * compile_append() - append list results of two statements
 *
 *	parse->left	more compile_append() by left-recursion
 *	parse->right	single rule
 */

StringList
compile_append(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	/* Append right to left. */

	auto left = (*parse->left->func)( parse->left, args, jmp );
	auto right = (*parse->right->func)( parse->right, args, jmp );
	left.AppendList(right);
	return left;
}

/*
 * compile_break() - compile 'break/continue/return' rule
 *
 *	parse->left	results
 *	parse->num	JMP_BREAK/CONTINUE/RETURN
 */

StringList
compile_break(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	StringList lv = (*parse->left->func)( parse->left, args, jmp );
	*jmp = parse->num;
	return lv;
}

/*
 * compile_eval() - evaluate if to determine which leg to compile
 *
 * Returns:
 *	list 	if expression true - compile 'then' clause
 *	L0	if expression false - compile 'else' clause
 */

static bool
lcmp( StringList t, StringList s )
{
	bool status = false;

	for (size_t offset = 0; !status && offset < t.Size() && offset < s.Size(); ++offset)
	{
	    auto &st = t[offset].get();
	    auto &ss = s[offset].get();

	    status = st == ss;
	}

	return status;
}

StringList
compile_eval(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	//LIST *ll, *lr, *s, *t;
	StringList ll = (*parse->left->func)( parse->left, args, jmp );
	StringList lr = {};
	bool status = false;
	size_t leftOffset = 0;
	size_t rightOffset = 0;

	/* Short circuit lr eval for &&, ||, and 'in' */

	switch( parse->num )
	{
	case EXPR_AND: 
	case EXPR_IN: 	if( ll.Size() > 0 ) goto eval; break;
	case EXPR_OR: 	if( ll.Size() == 0 ) goto eval; break;
	default: eval: 	lr = (*parse->right->func)( parse->right, args, jmp );
	}

	/* Now eval */

	switch( parse->num )
	{
	case EXPR_NOT:	
		if( ll.Size() == 0 ) status = true;
		break;

	case EXPR_AND:
		if( ll.Size() > 0 && lr.Size() > 0 ) status = true;
		break;

	case EXPR_OR:
		if( ll.Size() > 0 || lr.Size() > 0 ) status = true;
		break;

	case EXPR_IN:
		/* "a in b": make sure each of */
		/* ll is equal to something in lr. */

		for ( ; leftOffset < ll.Size(); ++leftOffset)
		//for( t = ll; t; t = list_next( t ) )
		{
			for ( ; rightOffset < lr.Size(); ++rightOffset)
		    //for( s = lr; s; s = list_next( s ) )
			if( ll[leftOffset].get() == lr[rightOffset].get())
			//if (!strcmp( t->string, s->string ) )
			    break;
		    //if( s == nullptr ) break;
			if (rightOffset == lr.Size())
				break;
		}

		/* No more ll? Success */

		if( leftOffset == ll.Size()) status = true;

		break;

	case EXPR_EXISTS:       if( lcmp( ll, {} ) != 0 ) status = true; break;
	case EXPR_EQUALS:	if( lcmp( ll, lr ) == 0 ) status = true; break;
	case EXPR_NOTEQ:	if( lcmp( ll, lr ) != 0 ) status = true; break;
	case EXPR_LESS:		if( lcmp( ll, lr ) < 0  ) status = true; break;
	case EXPR_LESSEQ:	if( lcmp( ll, lr ) <= 0 ) status = true; break;
	case EXPR_MORE:		if( lcmp( ll, lr ) > 0  ) status = true; break;
	case EXPR_MOREEQ:	if( lcmp( ll, lr ) >= 0 ) status = true; break;

	}

	if( DEBUG_IF )
	{
	    debug_compile( 0, "if" );
	    std::cout << ll << "(" << status << ")" << lr << std::endl;
	}

	/* Find something to return. */
	/* In odd circumstances (like "" = "") */
	/* we'll have to return a new string. */

	if( status == false ) return {};
	else if( ll.Size() > 0 ) return ll;
	else if( lr.Size() > 0 ) return lr;
	else return StringList("1");
}

/*
 * compile_foreach() - compile the "for x in y" statement
 *
 * Compile_foreach() resets the given variable name to each specified
 * value, executing the commands enclosed in braces for each iteration.
 *
 *	parse->string	index variable
 *	parse->left	variable values
 *	parse->right	rule to compile
 */

StringList
compile_foreach(
	PARSE	*p,
	LOL	*args,
	int	*jmp )
{
	StringList nv = (*p->left->func)( p->left, args, jmp );
	StringList result = {};

	/* for each value for var */

	for( size_t offset = 0; offset < nv.Size() && *jmp == JMP_NONE; ++offset )
	{
	    /* Reset $(p->string) for each val. */

	    var_set( p->string, StringList(nv[offset].get()), VAR_SET );

	    /* Keep only last result. */

	    result = (*p->right->func)( p->right, args, jmp );

	    /* continue loop? */

	    if( *jmp == JMP_CONTINUE )
		*jmp = JMP_NONE;
	}

	/* Here by break/continue? */

	if( *jmp == JMP_BREAK || *jmp == JMP_CONTINUE )
	    *jmp = JMP_NONE;

	/* Returns result of last loop */

	return result;
}

/*
 * compile_if() - compile 'if' rule
 *
 *	parse->left		condition tree
 *	parse->right		then tree
 *	parse->third		else tree
 */

StringList
compile_if(
	PARSE	*p,
	LOL	*args,
	int	*jmp )
{
	StringList l = (*p->left->func)( p->left, args, jmp );

	p = l.Size() > 0 ? p->right : p->third;

	return (*p->func)( p, args, jmp );
}

/*
 * compile_include() - support for 'include' - call include() on file
 *
 * 	parse->left	list of files to include (can only do 1)
 */

StringList
compile_include(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	StringList nt = (*parse->left->func)( parse->left, args, jmp );

	if( DEBUG_COMPILE )
	{
	    debug_compile( 0, "include" );
		std::cout << nt << std::endl;
	}

	if( nt.Size() > 0 )
	{
	    TARGET *t = bindtarget( nt[0].get().c_str() );

	    /* Bind the include file under the influence of */
	    /* "on-target" variables.  Though they are targets, */
	    /* include files are not built with make(). */
	    /* Needn't copysettings(), as search sets no vars. */

	    pushsettings( t->settings );
	    t->boundname = search( t->name, &t->time );
	    popsettings( t->settings );

	    /* Don't parse missing file if NOCARE set */

	    if( t->time || !( t->flags & T_FLAG_NOCARE ) )
		parse_file( t->boundname );
	}

	return {};
}

/*
 * compile_list() - expand and return a list 
 *
 * 	parse->string - character string to expand
 */

StringList
compile_list(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	/* voodoo 1 means: s is a copyable string */
	const char *s = parse->string;
	return var_expand( {}, s, s + strlen( s ), args, 1 );
}

/*
 * compile_local() - declare (and set) local variables
 *
 *	parse->left	list of variables
 *	parse->right	list of values
 *	parse->third	rules to execute
 */

StringList
compile_local(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	SETTINGS *s = 0;
	StringList nt = (*parse->left->func)( parse->left, args, jmp );
	StringList ns = (*parse->right->func)( parse->right, args, jmp );
	StringList result = {};

	if( DEBUG_COMPILE )
	{
	    debug_compile( 0, "local" );
		std::cout << nt << " = " << ns << std::endl;
	}

	/* Initial value is ns */

	for(size_t offset = 0; offset < nt.Size(); ++offset)
		s = addsettings( s, 0, nt[offset].get().c_str(), ns.SubList(0, ns.Size()) );

	/* Note that callees of the current context get this "local" */
	/* variable, making it not so much local as layered. */

	pushsettings( s );
	result = (*parse->third->func)( parse->third, args, jmp );
	popsettings( s );
	freesettings( s );

	return result;
}

/*
 * compile_null() - do nothing -- a stub for parsing
 */

StringList
compile_null(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	return {};
}

/*
 * compile_on() - run rule under influence of on-target variables
 *
 * 	parse->left	target list; only first used
 *	parse->right	rule to run
 */

StringList
compile_on(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	StringList nt = (*parse->left->func)( parse->left, args, jmp );
	StringList result = {};

	if( DEBUG_COMPILE )
	{
	    debug_compile( 0, "on" );
		std::cout << nt << std::endl;
	}

	/* 
	 * Copy settings, so that 'on target var on target = val' 
	 * doesn't set var globally.
	 */

	if( nt.Size() > 0 )
	{
	    TARGET *t = bindtarget( nt[0].get().c_str() );
	    SETTINGS *s = copysettings( t->settings );

	    pushsettings( s );
	    result = (*parse->right->func)( parse->right, args, jmp );
	    popsettings( s );
	    freesettings( s );
	}

	return result;
}

/*
 * compile_rule() - compile a single user defined rule
 *
 *	parse->left	list of rules to run
 *	parse->right	parameters (list of lists) to rule, recursing left
 *
 * Wrapped around evaluate_rule() so that headers() can share it.
 */

StringList
compile_rule(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	LOL	nargs[1];
	StringList result = {};
	PARSE	*p;

	/* list of rules to run -- normally 1! */
	StringList ll = (*parse->left->func)( parse->left, args, jmp );
	
	/* Build up the list of arg lists */

	lol_init( nargs );

	for( p = parse->right; p; p = p->left )
	    lol_add( nargs, (*p->right->func)( p->right, args, jmp ) );

	/* Run rules, appending results from each */

	for (size_t offset = 0; offset < ll.Size(); ++offset)
	{
	    int localJmp = JMP_NONE;
	    result = evaluate_rule( ll[offset].get().c_str(), nargs, result, &localJmp );
	    if (localJmp == JMP_EOF)
	    {
			*jmp = JMP_EOF;
			break;
	    }
	}

	lol_free( nargs );

	return result;
}

/*
 * evaluate_rule() - execute a rule invocation
 */

StringList
evaluate_rule(
	const char *rulename,
	LOL	*args, 
	StringList result,
	int	*jmp )
{
	RULE	*rule = bindrule( rulename );

	if( 0 )
	{
	    debug_compile( 1, rulename );
	    lol_print( args );
	    printf( "\n" );
	}

	//printf("EVALUATE RULE: %s\n", rulename);
	//lol_print(args);
	//printf("\n\n");

	/* Check traditional targets $(<) and sources $(>) */

	if( !rule->actions && !rule->procedure )
	    printf( "warning: unknown rule %s\n", rule->name );

	/* If this rule will be executed for updating the targets */
	/* then construct the action for make(). */

	if( rule->actions )
	{
	    TARGETS	*t;
	    ACTION	*action;

	    /* The action is associated with this instance of this rule */

	    action = (ACTION *)malloc( sizeof( ACTION ) );
	    memset( (char *)action, '\0', sizeof( *action ) );

	    action->rule = rule;
	    action->targets = targetlist( (TARGETS *)0, lol_get( args, 0 ) );
	    action->sources = targetlist( (TARGETS *)0, lol_get( args, 1 ) );
	    
	    /* Make targets[1,N-1] depend on targets[0], to describe the */
	    /* generated targets for the rule. Do it with includes, to */
	    /* reflect non-build dependency. */
	    
	    if( action->targets )
	    {
	    	TARGET *t0 = action->targets->target;
			std::cout << "build " << t0->name << ": " << rulename << " " << lol_get(args, 1) << std::endl << std::endl;
	    	for( t = action->targets->next; t; t = t->next )
	    	{
	    		TARGET *tn = t->target;
	    		if( !tn->includes )
	    		{
	    			tn->includes = copytarget( tn );
	    		}
	    		tn = tn->includes;
	    		tn->depends = targetentry( tn->depends, t0 );

				// additional outputs for a rule, other than the main
				// target
				// ninja understands multiple outputs, so we should do that,
				// e.g. build <t0> <t1 ...tN>: <rule> <inputs...>
				printf("TARGET: %s\n", tn->name);
	    	}
	    }

	    /* Append this action to the actions of each target */

	    for( t = action->targets; t; t = t->next )
		t->target->actions = actionlist( t->target->actions, action );
	}

	/* Now recursively compile any parse tree associated with this rule */

	if( rule->procedure )
	{
	    PARSE *parse = rule->procedure;
	    SETTINGS *s = 0;
	    
# ifdef OPT_RULE_PROFILING_EXT
		struct timeval startTime, endTime;

		if ( DEBUG_PROFILE_RULES )
			gettimeofday(&startTime, 0);
# endif

	    /* build parameters as local vars */
		for(size_t offset = 0; offset < rule->params.Size(); ++offset)
		s = addsettings( s, 0, rule->params[offset].get().c_str(), 
			lol_get(args, offset).SubList(0, lol_get(args, offset).Size()));

	    /* Run rule. */
	    /* Bring in local params. */
	    /* refer/free to ensure rule not freed during use. */

	    parse_refer( parse );

		pushsettings( s );
	    result.AppendList((*parse->func)( parse, args, jmp ));
		popsettings( s );
	    freesettings( s );

	    parse_free( parse );

# ifdef OPT_RULE_PROFILING_EXT
		if ( DEBUG_PROFILE_RULES )
		{
			gettimeofday(&endTime, 0);

			rule->invocations++;
			rule->invocation_time
				+= (endTime.tv_sec - startTime.tv_sec) * (int64_t)1000000
					+ (endTime.tv_usec - startTime.tv_usec);
		}
# endif

	}

	if( 0 )
	    debug_compile( -1, 0 );

	return result;
}

/*
 * compile_rules() - compile a chain of rules
 *
 *	parse->left	single rule
 *	parse->right	more compile_rules() by right-recursion
 */

StringList
compile_rules(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	/* Ignore result from first statement; return the 2nd. */
	/* Optimize recursion on the right by looping. */

	StringList result = {};

	while( *jmp == JMP_NONE && parse->func == compile_rules )
	{
	    result = (*parse->left->func)( parse->left, args, jmp );
	    parse = parse->right;
	}

	if( *jmp == JMP_NONE )
	{
	    result = (*parse->func)( parse, args, jmp );
	}

	return result;
}

/*
 * compile_set() - compile the "set variable" statement
 *
 *	parse->left	variable names
 *	parse->right	variable values 
 *	parse->num	VAR_SET/APPEND/DEFAULT
 */

StringList
compile_set(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	StringList nt = (*parse->left->func)( parse->left, args, jmp );
	StringList ns = (*parse->right->func)( parse->right, args, jmp );

	if( DEBUG_COMPILE )
	{
	    debug_compile( 0, "set" );
		std::cout << nt << " " << set_names[parse->num] << " " << ns << std::endl;
	}

	/* Call var_set to set variable */
	/* var_set keeps ns, so need to copy it */

	for (size_t offset = 0; offset < nt.Size(); ++offset)
	    var_set( nt[offset].get().c_str(), ns.SubList(0, ns.Size()), parse->num );

	return ns;
}

/*
 * compile_setcomp() - support for `rule` - save parse tree 
 *
 *	parse->string	rule name
 *	parse->left	list of argument names
 *	parse->right	rules for rule
 */

StringList
compile_setcomp(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	RULE	*rule = bindrule( parse->string );
	StringList params = {};
	PARSE	*p;

	/* Build param list */

	for( p = parse->left; p; p = p->left )
		params.Append(p->string);

	if( DEBUG_COMPILE )
	{
	    debug_compile( 0, "rule" );
		std::cout << parse->string << " " << params << std::endl;
	}

	/* Free old one, if present */

	if( rule->procedure )
	    parse_free( rule->procedure );

	rule->procedure = parse->right;
	rule->params = params;

	/* we now own this parse tree */
	/* don't let parse_free() release it */

	parse_refer( parse->right );

	return {};
}


/*
 * compile_setexec() - support for `actions` - save execution string 
 *
 *	parse->string	rule name
 *	parse->string1	OS command string
 *	parse->num	flags
 *	parse->left	`bind` variables
 *
 * Note that the parse flags (as defined in compile.h) are transfered
 * directly to the rule flags (as defined in rules.h).
 */

StringList
compile_setexec(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	RULE	*rule = bindrule( parse->string );
	StringList bindlist = (*parse->left->func)( parse->left, args, jmp );
	
	/* Free old one, if present */

	if( rule->actions )
	{
	    freestr( rule->actions );
	}

	rule->actions = copystr( parse->string1 );
	rule->bindlist = bindlist;
	rule->flags = parse->num;

	output_rule(rule);

	return {};
}

/*
 * compile_settings() - compile the "on =" (set variable on exec) statement
 *
 *	parse->left	variable names
 *	parse->right	target name 
 *	parse->third	variable value 
 *	parse->num	VAR_SET/APPEND/DEFAULT
 */

StringList
compile_settings(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	StringList nt = (*parse->left->func)( parse->left, args, jmp );
	StringList ns = (*parse->third->func)( parse->third, args, jmp );
	StringList targets = (*parse->right->func)( parse->right, args, jmp );

	if( DEBUG_COMPILE )
	{
	    debug_compile( 0, "set" );
		std::cout << nt << " on " << targets << " " << set_names[parse->num] << " " << ns << std::endl;
	}

	/* Call addsettings to save variable setting */
	/* addsettings keeps ns, so need to copy it */
	/* Pass append flag to addsettings() */

	for(size_t offset = 0; offset < targets.Size(); ++offset)
	{
	    TARGET 	*t = bindtarget( targets[offset].get().c_str() );

		for (size_t ntOffset = 0; ntOffset < nt.Size(); ++ntOffset)
		t->settings = addsettings( t->settings, parse->num,
				nt[ntOffset].get().c_str(), ns.SubList(0, ns.Size()) );
	}

	return ns;
}

/*
 * compile_switch() - compile 'switch' rule
 *
 *	parse->left	switch value (only 1st used)
 *	parse->right	cases
 *
 *	cases->left	1st case
 *	cases->right	next cases
 *
 *	case->string	argument to match
 *	case->left	parse tree to execute
 */

StringList
compile_switch(
	PARSE	*parse,
	LOL	*args,
	int	*jmp )
{
	StringList nt = (*parse->left->func)( parse->left, args, jmp );
	StringList result = {};

	if( DEBUG_COMPILE )
	{
	    debug_compile( 0, "switch" );
		std::cout << nt << std::endl;
	    printf( "\n" );
	}

	/* Step through cases */

	for( parse = parse->right; parse; parse = parse->right )
	{
	    if( !glob( parse->left->string, nt.Size() > 0 ? nt[0].get().c_str() : "" ) )
	    {
		/* Get & exec parse tree for this case */
		parse = parse->left->left;
		result = (*parse->func)( parse, args, jmp );
		break;
	    }
	}

	return result;
}

/*
 * compile_while() - compile 'while' rule
 *
 *	parse->left		condition tree
 *	parse->right		execution tree
 */

StringList
compile_while(
	PARSE	*p,
	LOL	*args,
	int	*jmp )
{
	StringList result = {};
	StringList l = {};

	/* Returns the value from the last execution of the block */

	while( ( *jmp == JMP_NONE ) && 
	       ( l = (*p->left->func)( p->left, args, jmp ) ).Size() > 0 )
	{
	    /* Always toss while's expression */

	    //list_free( l );

	    /* Keep only last result. */

	    //list_free( result );
	    result = (*p->right->func)( p->right, args, jmp );

	    /* continue loop? */

	    if( *jmp == JMP_CONTINUE )
		*jmp = JMP_NONE;
	}

	/* Here by break/continue? */

	if( *jmp == JMP_BREAK || *jmp == JMP_CONTINUE )
	    *jmp = JMP_NONE;

	/* Returns result of last loop */

	return result;
}

/*
 * debug_compile() - printf with indent to show rule expansion.
 */

static void
debug_compile( int which, const char *s )
{
	static int level = 0;
	static char indent[36] = ">>>>|>>>>|>>>>|>>>>|>>>>|>>>>|>>>>|";
	int i = ((1+level) * 2) % 35;

	if( which >= 0 )
	    printf( "%*.*s ", i, i, indent );

	if( s )
	    printf( "%s ", s );

	level += which;
}
