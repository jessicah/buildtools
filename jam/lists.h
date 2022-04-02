/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * lists.h - the LIST structure and routines to manipulate them
 *
 * The whole of jam relies on lists of strings as a datatype.  This
 * module, in conjunction with newstr.c, handles these relatively
 * efficiently.
 *
 * Structures defined:
 *
 *	LIST - list of strings
 *	LOL - list of LISTs
 *
 * External routines:
 *
 *	list_append() - append a list onto another one, returning total
 *	list_new() - tack a string onto the end of a list of strings
 * 	list_copy() - copy a whole list of strings
 *	list_sublist() - copy a subset of a list of strings
 *	list_free() - free a list of strings
 *	list_print() - print a list of strings to stdout
 *	list_printq() - print a list of safely quoted strings to a file
 *	list_length() - return the number of items in the list
 *
 *	lol_init() - initialize a LOL (list of lists)
 *	lol_add() - append a LIST onto an LOL
 *	lol_free() - free the LOL and its LISTs
 *	lol_get() - return one of the LISTs in the LOL
 *	lol_print() - debug print LISTS separated by ":"
 *
 * 04/13/94 (seiwald) - added shorthand L0 for null list pointer
 * 08/23/94 (seiwald) - new list_append()
 * 10/22/02 (seiwald) - list_new() now does its own newstr()/copystr()
 * 11/04/02 (seiwald) - const-ing for string literals
 * 12/09/02 (seiwald) - new list_printq() for writing lists to Jambase
 */

#include <iostream>
#include <vector>
#include <string>
#include <unordered_set>

#include "boost/flyweight.hpp"
#include "boost/flyweight/set_factory.hpp"
#include "boost/flyweight/no_locking.hpp"
#include "boost/flyweight/no_tracking.hpp"

typedef boost::flyweights::flyweight<std::string,
	boost::flyweights::set_factory<>,
	boost::flyweights::no_locking,
	boost::flyweights::no_tracking> atom;


// probably don't use this
#if 0
const atom get_atom(const std::string str)
{
	atom _atom(std::move(str));

	return _atom;
}
#endif


class StringList {
public:
	StringList() {};

	StringList(const std::string token) : StringList()
	{
		list.emplace_back(std::move(token));
	}

	StringList& Append(const std::string token)
	{
		list.emplace_back(std::move(token));

		return *this;
	}

	StringList& AppendList(const StringList& otherList)
	{
		for (size_t offset = 0; offset < list.size(); ++offset)
		{
			list.push_back(otherList.list[offset]);
		}

		return *this;
	}

	StringList SubList(size_t start, size_t length) const
	{
		return { list, start, length };
	}

	StringList Copy() const
	{
		return { list, 0, list.size() };
	}

	const std::string& StringAt(size_t offset) const
	{
		return list[offset].get();
	}

	const char* CStringAt(size_t offset) const
	{
		return list[offset].get().c_str();
	}

	int Size() const { return list.size(); }

	bool Empty() const { return list.size() == 0; }

	const atom& operator[](size_t offset)
	{
		return list[offset];
	}

	friend std::ostream& operator<<(std::ostream &output, const StringList &stringList)
	{
		if (stringList.list.size() == 0)
			return output;
		
		output << stringList.list[0];

		for (size_t offset = 1; offset < stringList.list.size(); ++offset)
		{
			output << " " << stringList.list[offset];
		}

		return output;
	}

private:
	StringList(const std::vector<atom> list, size_t offset, size_t length) : StringList()
	{
		this->list.resize(length);
		for ( ; offset < length && offset < list.size(); ++offset)
		{
			this->list.emplace_back(list[offset]);
		}
	}

	std::vector<atom> list;
};


/*
 * LOL - list of LISTs
 */

typedef struct _lol LOL;

# define LOL_MAX 9

struct _lol {
	int	count;
	StringList	list[ LOL_MAX ];
} ;

#if 0
LIST *	list_append( LIST *l, LIST *nl );
LIST *	list_copy( LIST *l, LIST  *nl );
void	list_free( LIST *head );
LIST *	list_new( LIST *head, const char *string, int copy );
void	list_print( LIST *l );
int	list_length( LIST *l );
LIST *	list_sublist( LIST *l, int start, int count );
#endif

//# define list_next( l ) ((l)->next)

//# define L0 ((LIST *)0)

void	lol_add( LOL *lol, StringList l );
void	lol_init( LOL *lol );
void	lol_free( LOL *lol );
StringList&	lol_get( LOL *lol, int i );
void	lol_print( LOL *lol );
