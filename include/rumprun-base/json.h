/*
 * This file is a rollup of the json, ptrvec, twine and sanity modules from the
 * jsoncvt project into a single self-contained compilation unit, with the
 * following modifications:
 *
 * - The 'sanity' functions have been reimplemented using err(3) and internal
 *   calls to err() and die() have been changed to use errx(3).
 * - All public functions are declared as 'static'.
 * - Public functions which are not required by rumprun are removed from
 *   compilation with #if 0 blocks and marked as UNUSED.
 * - jparse() takes a string as input rather than a FILE * and the underlying
 *   ifile functions have been modified to suit.
 *
 * The following license applies to this file:
 *
 * == License ==
 * 
 * Copyright ⓒ 2014, 2015 Robert S. Krzaczek.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * “Software”), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * == Other Copyrights ==
 * 
 * While the code presented in *sanity.h* and *sanity.c* is original, it
 * is certainly inspired by the excellent book, “The Practice of
 * Programming” by Brian W. Kernighan and Rob Pike. Quoting from that
 * source:
 * 
 * [quote,'http://cm.bell-labs.com/cm/cs/tpop/[The Practice Of Programming]']
 * _____________________________________________________________________
 * You may use this code for any purpose, as long as you leave the
 * copyright notice and book citation attached. Copyright © 1999 Lucent
 * Technologies. All rights reserved. Mon Mar 19 13:59:27 EST 2001
 * _____________________________________________________________________
 */

#ifndef _BMKCOMMON_RUMPRUN_JSON_H_
#define _BMKCOMMON_RUMPRUN_JSON_H_

#define _POSIX_C_SOURCE 200112L
#include <ctype.h>
#include <err.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define extern static

/* See one of the index files for license and other details. */
#ifndef jsoncvt_json_h
#define jsoncvt_json_h

/** The different types of values in our JSON parser. Unlike the
 *  standard, we discriminate between integer and real values. */
enum jtypes {
    jnull,      /**< The JSON "null" value. */
    jtrue,      /**< Just a simple "true" value. */
    jfalse,     /**< Just a simple "false" value. */
    jstring,    /**< Just your run of the mill string. */
    jnumber,    /**< A JSON number (still just a string). */
    jarray,     /**< A vector of values. */
    jobject,    /**< An assoc. array of names and arbitrary values. */
    jint,       /**< A JSON number parsed into a native integer. */
    jreal,      /**< A JSON number parsed into a long double. */
};

/** A jvalue represents the different values found in a parse of a
 *  JSON doc. A value can be terminal, like a string or a number, or
 *  it can nest, as with arrays and objects. The value of #d reflects
 *  which part of the union is value. */
typedef struct jvalue {

    /** Just your basic discriminator, describing which part of the
     *  union below is active. When this is jtrue, jfalse, or jnull,
     *  nothing in #u is valid (being unnecessary); all other values
     *  correspond to one of the #u members as described below. */
    enum jtypes d;

    /** Some values have a name associated with them; in a JSON
     *  object, for example, the value is assigned to a specific name.
     *  When #d is jobject, this string should point to the name of a
     *  member (whose value is in #u). For other values of #d, this
     *  member should be null. A previous implementation used a
     *  separate structure for these pairings, but placing the name
     *  inside each value only costs an extra 4 or 8 bytes yet
     *  simplifies the tree quite a bit for our client. */
    char *n;

    /** According to #d above, one or none of these are the active value. */
    union {
        /** When #d is jstring or jnumber, this string is active in
         *  the union. While obvious for jstring, why would this be
         *  used for jnumber? Because, often, there's no need to parse
         *  the number value into something native. While integers are
         *  exact, there's often an unavoidable loss of precision
         *  when converting real numbers. So, we defer it as long as
         *  we can. If the client application actually *wants* a
         *  parsed value, it can convert the string to a native value,
         *  cache it away in the #i or #r members, and change the
         *  discriminator to jint or jreal accordingly. This avoids
         *  unnecessary parsing work and loss of precision, but
         *  doesn't make it unduly hard for a client to deal with. See
         *  jupdate() as a function the client can call to do just
         *  that. */
        char *s;

        /** When the discriminator is jint, this integer is active. */
        long long i;

        /** When the discriminator is jreal, this long double is
         *  active. */
        long double r;

        /** When the discriminator is jarray or jobject, this
         *  zero-terminated vector of pointers to jvalue is active.
         *  You'll find the ptrvec routines make building these
         *  easy. */
        struct jvalue **v;
    } u;
} jvalue;

extern jvalue *jnew(void);
extern jvalue *jclear( jvalue * );
extern void jdel( jvalue * );
extern jvalue *jparse( const char *json );
#if 0 /* UNUSED */
extern jvalue *jupdate(  jvalue * );
extern int jdump( FILE *fp, const jvalue *j );
#endif

#endif

/* See one of the index files for license and other details. */
#ifndef jsoncvt_ptrvec_h
#define jsoncvt_ptrvec_h

/** ptrvec is just used to make creating pointer-to-pointer lists
 *  (like an argv) easy to build. It automatically manages its memory,
 *  reallocating when necessary, and so on.
 *
 *  Expected usage is something like
 *
 *  1. Obtain new ptrvec via pvnew(), or initialize one to all zeroes.
 *
 *  2. Use pvadd() to add pointers to the vector. The underlying
 *  vector is always null terminated, even while building, so you can
 *  access the in-progress vector safely via p.
 *
 *  3. Use pvdup() to create a new void** that is exactly the size
 *  needed for the resulting string, or use pvfinal() below. The
 *  elements of the vector are just copied; only the vector itself is
 *  allocated anew.
 *
 *  4. Free up any current space space via pvclear(), resetting things
 *  to "empty" again. Set a hard size via pvsize().
 *
 *  5. if you called pvnew() earlier, call pvdel() to free it. If you
 *  just want to free up the memory it uses but not the ptrvec itself,
 *  call pvclear(). pvfinal() combines both pvdup() and pvclear(). */
typedef struct ptrvec {
    /** A table of pointers to anything living here. The number of
     * actual pointers allocated is tracked in sz, and the number of
     * active pointers is tracked in len. */
    void **p;

    /** How many of the pointers at p are in use? */
    size_t len;

    /** How many pointers have been allocated at p? */
    size_t sz;
} ptrvec;

#if 0 /* UNUSED */
extern ptrvec *pvnew(void);
#endif
extern ptrvec *pvclear( ptrvec * );
extern void **pvfinal( ptrvec * );
#if 0 /* UNUSED */
extern void pvdel( ptrvec * );
#endif
extern void **pvdup( const ptrvec * );
extern ptrvec *pvsize( ptrvec *, size_t );
extern ptrvec *pvensure( ptrvec *, size_t );
extern ptrvec *pvadd( ptrvec *, void * );

#endif

/* See one of the index files for license and other details. */
#ifndef jsoncvt_twine_h
#define jsoncvt_twine_h

/** twine is like a string, it manages memory for a string, and can
 *  be used to create new C strings as necessary. It's called twine
 *  because it's like a string, but a little thicker; there's usually a
 *  spool of it handy, like the memory management it comes with; and
 *  it's easy to extract a string from it when you're done.
 *
 *  It serves primarily as a C-string builder with memory management.
 *  With it, you can build strings incrementally from a variety of
 *  sources, without the overhead of length calculations and
 *  realloc(3) calls every time. A trailing null is included even in
 *  the in-progress string, so that the internal string can be used
 *  directly when necessary.
 *
 *  Expected usage is something like
 *
 *  1. Obtain new twine via twnew(), or initialize one to all zeroes.
 *
 *  2. Use the twadd*() and twset*() functions to build up its contents.
 *  You can access the in-progress string via the p member at any
 *  time. Under the hood, p points to a null-terminated region bigger
 *  than necessary to reduce the number of reallocs necessary.
 *
 *  3. Use twdup() to create a new C string that is exactly the size
 *  needed for the resulting string, or use twfinal() below.
 *
 *  4. Free up any current space space via twclear(), resetting things
 *  to "empty" again. Set a hard size via twsize().
 *
 *  5. if you called twnew() earlier, call twdel() to free it. If you
 *  just want to free up the memory it uses but not the twine itself,
 *  call twclear(). twfinal() combines both twdup() and twclear().
 */
typedef struct twine {
    char *p;               /**< null terminated C string data */
    size_t len;            /**< size of the string, not counting null */
    size_t sz;             /**< size of the underlying buffer */
} twine;

#if 0 /* UNUSED */
extern twine *twnew(void);
#endif
extern twine *twclear( twine * );
extern char *twfinal( twine * );
#if 0 /* UNUSED */
extern void twdel( twine * );
#endif

extern char *twdup( const twine * );

extern twine *twsize( twine *, size_t );

#if 0 /* UNUSED */
extern twine *twset( twine *, const char *, size_t );
extern twine *twsetz( twine *, const char * );
#endif

#if 0 /* UNUSED */
extern twine *twadd( twine *, const twine * );
#endif
extern twine *twaddc( twine *, char );
extern twine *twaddu( twine *, uint32_t );
#if 0 /* UNUSED */
extern twine *twaddz( twine *, const char * );
#endif

#endif

/* See one of the index files for license and other details. */
#ifndef jsoncvt_sanity_h
#define jsoncvt_sanity_h

/* While this code is original, it is certainly inspired by the
 * excellent book, "The Practice of Programming" by Brian W. Kernighan
 * and Rob Pike.
 * 
 * Quoting from the book: "You may use this code for any purpose, as
 * long as you leave the copyright notice and book citation attached.
 * Copyright © 1999 Lucent Technologies. All rights reserved. Mon Mar
 * 19 13:59:27 EST 2001" */

extern void *emalloc( size_t );
extern void *erealloc( void *, size_t );
#if 0 /* UNUSED */
extern char *estrdup( const char * );
#endif

#endif

/* See one of the index files for license and other details. */

/** Allocate some number of bytes from the system and return a pointer
 *  to them, or exit. */
void *
emalloc( size_t nb )
{
    void *p = malloc( nb );
    if( !p )
        err( 1, "unable to allocate %zu bytes", nb );
    return p;
}

/** Change an allocated buffer to another size, returning a pointer to
 *  the new buffer. If the buffer could not be grown, an error is
 *  displayed and the process exits (this function does not return). */
void *
erealloc( void *ptr, size_t nb )
{
    void *p = realloc( ptr, nb );
    if( !p )
        err( 1, "unable to reallocate %zu bytes", nb );
    return p;
}

#if 0 /* UNUSED */
/** Just like strdup(3), but exits on failure instead of returning
 *  crap. */
char *
estrdup( const char *s )
{
    return s ? strcpy( emalloc( strlen( s ) + 1 ), s ) : 0;
}
#endif

/* See one of the index files for license and other details. */

enum {
    /** Ptrvecs start out with space for this many pointers. The number
     *  is pretty much arbitrary; if you think all of your ptrvecs are
     *  going to be extensive, free free to bump this value up to a
     *  bigger initial size to reduce the load on realloc(3). */
    pv_initial_size = 8
};

#if 0 /* UNUSED */
/** Allocate a new ptrvec from the heap, initialize it as zero, and
 *  return a pointer to it. */
ptrvec *
pvnew(void)
{
    ptrvec *p = emalloc( sizeof( *p ));
    *p = (ptrvec){0};
    return p;
}
#endif

/** If the supplied ptrvec has any storage allocated, return it to the
 *  heap. The ptrvec itself is not freed. Whereas pvdel() is useful for
 *  entirely heap-based objects (typically obtained from pvnew()),
 *  pvclear() is useful at tne end of functions that use a stack-based
 *  ptrvec object. */
ptrvec *
pvclear( ptrvec *pv )
{
    if( !pv )
	return 0;
    if( pv->p )
	free( pv->p );
    *pv = (ptrvec){ 0 };
    return pv;
}

/** Return a new void** that is a copy of the one we've been building
 *  in our ptrvec. Unlike our member p, this one will be allocated from
 *  the heap and contains just enough space to hold the current
 *  contents of p including its terminating null. */
void **
pvdup( const ptrvec *pv )
{
    void **v;

    if( !pv ) {
	v = emalloc( sizeof( *v ));
	*v = 0;
    } else {
	size_t nb = sizeof( void* ) * ( pv->len + 1 );
	v = emalloc( nb );
	if( pv->p )
	    memcpy( v, pv->p, nb );
	else
	    memset( v, 0, nb );
    }

    return v;
}

/** A wrapper for the common case at the end of working with a ptrvec.
 *  Return a null terminated void** ready for storage somewhere, and
 *  kill our own storage so that the next thing to come along can use
 *  our memory. */
void **
pvfinal( ptrvec *pv )
{
    void **v = pvdup( pv );
    pvclear( pv );
    return v;
}

#if 0 /* UNUSED */
/** Return a ptrvec and its pointers to the head. Once called, the
 *  supplied pointer is <em>no longer valid.</em> Memory at this old
 *  ptrvec is zeroed prior to being freed. */
void
pvdel( ptrvec *pv )
{
    if( pv )
	free( pvclear( pv ));
}
#endif

/** Force the supplied ptrvec to contain exactly some number of
 *  pointers. */
ptrvec *
pvsize( ptrvec *pv, size_t sz )
{
    if( !sz )
	return pvclear( pv );

    pv->p = erealloc( pv->p, ( pv->sz = sz ) * sizeof( *pv->p ));
    if( pv->len >= pv->sz ) {
	pv->len = pv->sz - 1;
	pv->p[ pv->len ] = 0;
    }
    return pv;
}

/** Ensures that the supplied ptrvec has at least some number of
 *  pointers. If it doesn't, the region of pointers in the ptrvec are
 *  reallocated. Unlike pvsize(), pvensure() grows the ptrvec in a way
 *  that hopefully avoids constant reallocations. */
ptrvec *
pvensure( ptrvec *pv, size_t sz )
{
    size_t newsz;
    
    if( !pv )
	return 0;
    else if( !sz || sz <= pv->sz )
	return pv;
    else if( !pv->sz && sz <= pv_initial_size )
	return pvsize( pv, pv_initial_size );

    /* Choose the next size up for this ptrvec as either 150% of its
       current size, or if that's not big enough, 150% of the
       requested size. Either is meant to add enough padding so that
       we hopefully don't come back here too soon. */
    newsz = pv->sz * 3 / 2;
    if( newsz < sz )
	newsz = sz * 3 / 2;

    /* Imperfect, but should catch most overflows, when newsz has
       rolled past SIZE_MAX. */
    if( newsz < pv->sz )
	errx( 1, "ptrvec overflow" );

    return pvsize( pv, newsz );
}

/** Add a pointer to the end of the set of pointers managed in this
 *  ptrvec. The size of the region is managed. The sz might grow a lot,
 *  but len will only ever grow by one. */
ptrvec *
pvadd( ptrvec *pv, void *v )
{
    pvensure( pv, pv->len + 2 );
    pv->p[ pv->len++ ] = v;
    pv->p[ pv->len ] = 0;
    return pv;
}

/* See one of the index files for license and other details. */

enum {
    /** The initial size allocation for a twine. Twines start off
     * empty (zero bytes in size), and when they grow, this is their
     * first size. It's arbitrary, really, what you start with; we're
     * going with 16 because it's the cache line size on modern x86
     * hardware; anything smaller would be pointless. */
    tw_initial_size = 16
};

#if 0 /* UNUSED */
/** Create a new empty twine and return a pointer to it. This
 *  function never returns if it cannot allocate the requested
 *  memory. */
twine *
twnew(void)
{
    twine *t = emalloc( sizeof( *t ));
    *t = (twine){ 0 };
    return t;
}

/** Release a twine obtained via strnew() and all of its memory. Once
 *  you've called this, \a t is no longer valid. */
void
twdel( twine *t )
{
    twclear( t );
    free( t );
}
#endif

/** Zero a twine, returning its storage back to the system, but
 *  leaving the twine structure still valid (though zeroed). Useful
 *  for twine structure defined on the stack, for example. Also, it's
 *  a severe way to clear a twine of current data, but useful if
 *  you've got to return memory. */
twine *
twclear( twine *t )
{
    if( t->p )
        free( t->p );
    *t = (twine){ 0 };
    return t;
}

/** Return a pure C string that is a copy of the string we've been
 *  building in our twine. Unlike our string, this one will be
 *  allocated from the heap and contains just enough space to hold it. */
char *
twdup( const twine *t )
{
    char *p = emalloc( t->len + 1 );
    return strcpy( p, t->p ? t->p : "" );
}

/** A wrapper for the common case at the end of working with twine.
 *  Return a C string ready for storage somewhere, and kill our own
 *  storage so that the next thing to come along can use our memory. */
char *
twfinal( twine *t )
{
    char *p = twdup( t );
    twclear( t );
    return p;
}

/** Given a target size, resize the underlying buffer to be just large
 *  enough to handle it. This function doesn't pad space like the
 *  twadd*() functions will, and it might even shrink the buffer
 *  depending on how this system's realloc(3) is set up. This is
 *  because if you've added once, you're likely to add again, but if
 *  you have a size in mind in advance, you probably don't need to
 *  grow it soon. */
twine *
twsize( twine *t, size_t nb )
{
    if( !nb )
        return twclear( t );

    t->p = erealloc( t->p, t->sz = nb );
    if( t->len >= t->sz ) {
        t->len = t->sz - 1;
        t->p[ t->len ] = 0;
    }
    return t;
}

#if 0 /* UNUSED */
/** Copy into one of our twines a null-terminated C twine. Does not
 *  return if we cannot allocate enough memory. This operation doesn't
 *  pad the size of the twine with any reserve space, because most of
 *  the time, ssetz() is called on static twines that aren't going
 *  to be modified. */
twine *
twsetz( twine *t, const char *z )
{
    size_t zlen = strlen( z );

    /* Life is easy if we know that the source isn't overlapping with
       our twine. Given that the source and the destination are two
       different types, this should never happen, anyway. But if
       something is screwed up, we'll try to dodge the imminent core
       dump and do this in a slower, more wasteful fashion. */
    char *src = ( !t->p || (( z >= t->p + t->sz ) && ( z + zlen < t->p )))
        ? __UNCONST(z)
        : estrdup( z );

    if( zlen + 1 < tw_initial_size ) {
        twsize( t, tw_initial_size );
        t->len = zlen;
    } else
        twsize( t, ( t->len = zlen ) + 1 );
    strcpy( t->p, src );

    if( src != z )
        free( src );
    return t;
}

/** Copy into one of our twines some number of characters. */
twine *
twset( twine *t, const char *z, size_t nb )
{
    /* Life is easy if we know that the source isn't overlapping with
       our twine. Given that the source and the destination are two
       different types, this should never happen, anyway. But if
       something is screwed up, we'll try to dodge the imminent core
       dump and do this in a slower, more wasteful fashion. */
    char *src = ( !t->p || (( z >= t->p + t->sz ) && ( z + nb < t->p )))
        ? __UNCONST(z)
        : estrdup( z );

    if( nb + 1 < tw_initial_size ) {
        twsize( t, tw_initial_size );
        t->len = nb;
    } else
        twsize( t, ( t->len = nb ) + 1 );

    strncpy( t->p, src, nb );
    t->p[nb] = 0;

    if( src != z )
        free( src );
    return t;
}
#endif

/** Grow a twine as much as necessary to satisfy some number of bytes.
 *  This only concerns itself with size, not the logical length, so be
 *  sure to add the null in yourself to \a sz. The 1.5X growth factor
 *  is meant to balance between calling realloc too often, but not
 *  wasting memory like mad like some libraries do. */
static twine *
twensure( twine *t, size_t sz )
{
    size_t newsz;

    if( !sz || sz <= t->sz )
        return t;
    if( sz <= tw_initial_size )
        return twsize( t, tw_initial_size );

    newsz = t->sz * 3 / 2;
    if( newsz < sz )
        newsz = sz * 3 / 2;
    if( newsz < t->sz )
        errx( 1, "twine overflow" );

    return twsize( t, newsz );
}

/** Add a single plain character to our twine. This will grow the
 *  twine with padding if necessary; see twensure(). As with the other
 *  functions, if we can't get the memory we need, we just error and
 *  die. In this application, there's no point in trying to recover
 *  from an out of memory error. */
twine *
twaddc( twine *t, char c )
{
    twensure( t, t->len + 2 );
    char *p = t->p + t->len;
    *p++ = c;
    *p++ = 0;
    ++t->len;
    return t;
}

#if 0 /* UNUSED */
/** Like twaddc(), but this adds a null terminated C string. */
twine *
twaddz( twine *t, const char *z )
{
    size_t zlen = strlen( z );
    twensure( t, t->len + zlen + 1 );
    strcpy( t->p + t->len, z );
    t->len += zlen;
    return t;
}

/** Like twaddz(), but this adds another twine to us. */
twine *
twadd( twine *dst, const twine *src )
{
    twensure( dst, dst->len + src->len + 1 );
    strcpy( dst->p + dst->len, src->p );
    dst->len += src->len;
    return dst;
}
#endif

/** Adds a Unicode code point into the twine in UTF-8 format. Handles
 *  the full range of code points, up to 0x7ffffff. This is
 *  simplistic, and I think it's not quite comforming (apparently
 *  there's UTF-8 and there'S CESU and one has intentional omissions
 *  the other doesn't? */
twine *
twaddu( twine *t, uint32_t c )
{
    if( c <= 0x007f )
        twaddc( t, c );
    else if( c <= 0x07ff ) {
        twaddc( t, 0xc0 | ( c >> 6 ));
        twaddc( t, 0x80 | ( c & 0x3f ));
    } else if( c <= 0xffff ) {
        twaddc( t, 0xe0 | ( c >> 12 ));
        twaddc( t, 0x80 | ( c >> 6 & 0x3f ));
        twaddc( t, 0x80 | ( c & 0x3f ));
    } else if( c <= 0x1fffff ) {
        twaddc( t, 0xf0 | ( c >> 18 ));
        twaddc( t, 0x80 | ( c >> 12 & 0x3f ));
        twaddc( t, 0x80 | ( c >> 6 & 0x3f ));
        twaddc( t, 0x80 | ( c & 0x3f ));
    } else if( c <= 0x3ffffff ) {
        twaddc( t, 0xf8 | ( c >> 24 ));
        twaddc( t, 0x80 | ( c >> 18 & 0x3f ));
        twaddc( t, 0x80 | ( c >> 12 & 0x3f ));
        twaddc( t, 0x80 | ( c >> 6 & 0x3f ));
        twaddc( t, 0x80 | ( c & 0x3f ));
    } else if( c <= 0x7ffffff ) {
        twaddc( t, 0xfc | ( c >> 30 ));
        twaddc( t, 0x80 | ( c >> 24 & 0x3f ));
        twaddc( t, 0x80 | ( c >> 18 & 0x3f ));
        twaddc( t, 0x80 | ( c >> 12 & 0x3f ));
        twaddc( t, 0x80 | ( c >> 6 & 0x3f ));
        twaddc( t, 0x80 | ( c & 0x3f ));
    } else
        errx(1, "unicode code point cannot be >0x7ffffff" );
    return t;
}

/* See one of the index files for license and other details. */

/** This just makes it easier for us to track a line counter along
 *  with an input stream, so when we report errors, we can say
 *  something useful about where the error appeared. getch() will bump
 *  #line when a newline appears on the file stream. Initialize this
 *  with a file stream opened for reading, and set #line to 1. */
typedef struct ifile {
    const char *s;              /**< Current input position */
    const char *end;            /**< End of input */
    char ungetch;               /**< Char pushed by ungetch or EOF */
    size_t line;                /**< Line number */
} ifile;

static jvalue *readvalue( ifile * );

/** Returns the next character in the open input stream under \a f,
 *  and bump the line counter in \a f when appropriate. Errors can
 *  always be reported using line. */
static int
getch( ifile *f )
{
    int c;

    if( f->s == f->end )
        return EOF;
    if( f->ungetch != EOF ) {
        c = f->ungetch;
        f->ungetch = EOF;
    } else {
        c = *f->s;
        ++f->s;
    }
    if( c == '\n' )
        ++f->line;
    return c;
}

/** Return a character back to the file stream under \a f, like
 *  ungetc() would, but also manage its line counter. Though many
 *  stdio's can handle it, there is actually no guarantee that more
 *  than a single character can ever be pushed back onto the file
 *  stream. */
static int
ungetch( ifile *f, char c )
{
    if( c == '\n' )
        --f->line;
    f->ungetch = c;
    return c;
}

/** A wrapper around getch() that skips any leading whitespace before
 *  the character eventually returned, or EOF. Rather than a
 *  traditional parsing of whitespace, we limit ourselves to only the
 *  ws characters defined in JSON. */
static int
getchskip( ifile *f )
{
    int c;
    do {
        c = getch( f );
    } while( c == ' ' || c == '\t' || c == '\n' || c == '\r' );
    return c;
}

/** Skip ahead over any leading whitespace, leaving the next
 *  non-whitespace character in the stream ready for reading. The
 *  character returned is effectively a "peek" ahead at the next
 *  character that will be obtained from getch(). */
static int
skipws( ifile *f )
{
    int c = getchskip( f );
    if( c != EOF )
        ungetch( f, c );
    return c;
}

/** Create and return a new jvalue, initialized to be a jnull. Does
 *  not return if a new jvalue could not be allocated. */
jvalue *
jnew(void)
{
    jvalue *j = emalloc( sizeof( *j ));
    *j = (jvalue){ 0 };
    j->d = jnull;
    return j;
}

/** Walk a tree of jvalue, or even just a single jvalue, and free
 *  everything it contains, leaving \j intact but set to jnull.
 *  Normally, we'd set the various freed pointers to null explicitly,
 *  but at the end of the function, we zero the entire structure. */
jvalue *
jclear( jvalue *j )
{
    if( j ) {
        free( j->n );

        switch( j->d ) {
        case jarray:
        case jobject:
            if( j->u.v )
                for( jvalue **jv = j->u.v; *jv; ++jv )
                    jdel( *jv );
            free( j->u.v );
            break;

        case jstring:
        case jnumber:
            free( j->u.s );
            break;

        default:
            break;
        }

        *j = (jvalue){ 0 };
        j->d = jnull;
    }

    return j;
}

/** Walk a tree of jvalue, or even just a single value, and free
 *  everything it contains. Everything, even \a j itself, is freed.
 *  When this is complete, \a j is <em>no longer valid.</em> */
void
jdel( jvalue *j )
{
    free( jclear( j ));
}

/** Report an early EOF; that is, that the input stream ended before a
 *  value being read was finished. Bad syntax, truncated files, all
 *  the usual errors like that will trigger this. There's no point in
 *  reporting the line number, since this is an EOF. This is a
 *  function of its own, since it happens so often. */
static void
earlyeof(void)
{
    errx(1, "premature EOF in JSON data" );
}

/** Almost all of our errors end with a line number and a message that
 *  the user should be looking in the JSON data. Sure, it's a little
 *  clunky, but factoring it out into a common routine shortens a lot
 *  of our subsequent code. The standard for variable arguments,
 *  though, doesn't allow for the obvious simple implementation of
 *  this function, so we have to do it the annoying way with temporary
 *  buffers and such. Also, we could use vasprintf, instead of
 *  vsnprintf; we don't, because most implementations of *asprintf are
 *  naive about initial buffer allocations and do all sorts of malloc
 *  and reallocs under the hood. This approach sucks up a page on the
 *  stack, but lets go of the space immediately and doesn't fragment
 *  our heap further. */
static void
ierr( const ifile *f, const char *msg, ... )
{
    va_list ap;
    char buf[ 512 ];

    va_start( ap, msg );
    vsnprintf( buf, sizeof( buf ), msg, ap );
    va_end( ap );
    errx(1, "%s on line %zu in JSON data", buf, f->line );
}

/** The next character read from \a f must be a double quote. */
static bool
expectdq( ifile *f )
{
    int c = getch( f );

    if( c == EOF ) {
        earlyeof();
        return false;
    } else if( c != '"' ) {
        ierr( f, "missing quote from string" );
        return false;
    } else
        return true;
}

/** Reads a JSON string that is wrapped with quotes from \a f, parsing
 *  all the various string escapes therein. Returns a C string (sans
 *  quotes) freshly allocated from the heap, or a null on error. When
 *  null is returned, a diagnostic will have been sent to the standard
 *  error stream. */
static char *
readstring( ifile *f )
{
    if( !expectdq( f ))
        return 0;

    bool esc = false;           /* the next character is escaped */
    bool oops = false;          /* a bad string was seen? */
    unsigned int hex = 0;       /* read this many chars as a hex
                                 * Unicode code point */
    unsigned int x = 0;
    twine tw = (twine){ 0 };

    while( !oops ) {
        int c = getch( f );

        if( c == EOF ) {
            earlyeof();
            oops = true;

        } else if( hex ) {
            if( isxdigit( c )) {
	        if( isdigit( c ))
		    x = 16 * x + c - '0';
		else if( isupper( c ))
		    x = 16 * x + c - 'A' + 10;
		else
		    x = 16 * x + c - 'a' + 10;
                if( !--hex )
                    twaddu( &tw, x );
            } else {
                ierr( f, "expected hex digit" );
                oops = true;
            }

        } else if( esc ) {
            switch( c ) {
            case '"':  twaddc( &tw, '"' ); break;
            case '/':  twaddc( &tw, '/' ); break;
            case '\\': twaddc( &tw, '\\' ); break;
            case 'b':  twaddc( &tw, '\b' ); break;
            case 'f':  twaddc( &tw, '\f' ); break;
            case 'n':  twaddc( &tw, '\n' ); break;
            case 'r':  twaddc( &tw, '\r' ); break;
            case 't':  twaddc( &tw, '\t' ); break;
            case 'u':
                x = 0;
                hex = 4;
                break;
            default:
                ierr( f, "unknown escape code '\\%c'", (char)c );
                oops = true;
            }
            esc = 0;
        
        } else if( c == '"' )    /* done parsing the string! bye! */
            return twfinal( &tw );

        else if( c == '\\' )
            esc = 1;

        else if( c >= ' ' )
            twaddc( &tw, c );

        else if( isspace( c )) {
            ierr( f, "unescaped whitespace" );
            oops = true;

        } else {
            ierr( f, "unknown byte (0x%02x)", c );
            oops = true;
        }
    }

    twclear( &tw );             /* oops. bad string. give up and go
                                 * home. */
    return 0;
}

/** Used by readnumber(), this is passed the twine under construction
 *  and returns the value we should return to the readnumber() caller.
 *  It ensures that any storage accumulated in supplied twine is
 *  returned to the system, and returns 0, which readnumber() should
 *  chain return back to its caller, indicating a failure. Because
 *  this is only used by readnumber(), we'll mark it static. */
static char *
readnumberfail( twine *tw )
{
    twclear( tw );
    return 0;
}

/** We just peeked ahead and saw something that introduces a number.
 *  Gather it up into a string. The client can opt to convert this
 *  into a real number (integer or real) via jupdate() if they choose.
 *  Now, we could simply collect characters from a set [-+.0-9eE] and
 *  that would suffice, but instead, we'll do this the long way so
 *  that we can catch errors in bogus numeric fields (e.g.,
 *  "123.456.789"). */
static char *
readnumber( ifile *f )
{
    int c;
    twine tw = (twine){ 0 };

    if(( c = getch( f )) == '-' )		/* sign bit */
        twaddc( &tw, c );
    else
        ungetch( f, c );

    if(( c = getch( f )) == '0' ) {		/* integer */
        twaddc( &tw, c );
	c = getch( f );
    } else if( isdigit( c ) && c != '0' ) {
        do {
	    twaddc( &tw, c );
	    c = getch( f );
	} while( isdigit( c ));
    } else {
        ierr( f, "unexpected '%c'", c );
	return readnumberfail( &tw );
    }

    if( c == '.' )				/* fraction */
	do {
	    twaddc( &tw, c );
	    c = getch( f );
	} while( isdigit( c ));

    if( c == 'e' || c == 'E' ) {		/* exponent */
	twaddc( &tw, c );
	c = getch( f );
	if( c == '+' || c == '-' ) {
	    twaddc( &tw, c );
	    c = getch( f );
	}
	while( isdigit( c )) {
	    twaddc( &tw, c );
	    c = getch( f );
	}
    }

    if( c == ',' || c == ']' || c == '}' )	/* acceptable term */
	ungetch( f, c );
    else if( c != EOF && !isspace( c )) {	/* unacceptable */
        ierr( f, "unexpected '%c'", c );
	return readnumberfail( &tw );
    }

    return twfinal( &tw );
}

/** The next characters in the file stream \a f must match the ones
 *  we've been given in the string \a s. */
static bool
must( ifile *f, const char *s )
{
    int c;
    const char *p = s;

    while( *p && (( c = getch( f )) != EOF ))
        if( *p++ != c )
            break;

    if( !*p )
        return true;

    if( c == EOF )
        earlyeof();
    else
        ierr( f, "expected %s", s );

    return false;
}

/** With the stream pointing to a JSON string, read the object element
 *  at this point. Returns a new jvalue, or null when there is an
 *  error. */
static jvalue *
readobjel( ifile *f )
{
    char *n;

    if( !( n = readstring( f )))
        return 0;

    if( getchskip( f ) != ':' ) {
        ierr( f, "expected colon in object element" );
        free( n );
        return 0;
    }

    jvalue *j = readvalue( f );
    if( !j )
        free( n );
    else
        j->n = n;
    return j;
}

/** Reads a series of values from the JSON input stream at \a f,
 *  storing it in the #u.v member of \a j. We're passed a
 *  discriminator so we know whether we're parsing a simple array or
 *  an object; an object is just an array with names and colons before
 *  it. Processing of the actual elements is pretty simple, actually.
 *  Returns false when a parsing error is detected (which is
 *  reported). */
static bool
readseries( jvalue *j, ifile *f, enum jtypes t )
{
    char term;
    jvalue *(*reader)( ifile* ) = 0; /* reads an element */

    switch( t ) {
    case jarray:
        term = ']';
        reader = readvalue;
        break;
    case jobject:
        term = '}';
        reader = readobjel;
        break;
    default:
        ierr( f, "internal error jtype %d in readseries", (int)t );
        return false;
    }

    /* Peeking ahead in the stream saw [ or { which is how we got
       called. So go ahead and throw it away. */
    getch( f );

    ptrvec pv = (ptrvec){ 0 };
    for( bool oops = false; !oops; ) {
        jvalue *x = 0;
        int c = skipws( f );
        
        if( c == EOF ) {
            earlyeof();
            oops = true;

        } else if( c == ',' ) {
            if( pv.len == 0 ) {
                ierr( f, "missing value before comma" );
                oops = true;
            }
            getch( f );             /* consume the , */

        } else if( c == term ) {     /* we're done! */
            getch( f );             /* consume the } */
            j->u.v = (jvalue**)pvfinal( &pv );
            return true;

        } else if( !( x = reader( f )))
            oops = true;

        else
            pvadd( &pv, x );
    }

    pvclear( &pv );
    return false;
}

/** Get the next value out of the file stream \a f, storing it in a
 *  jvalue newly allocated from the heap. Returns 0 on a parsing
 *  failure (with diagnostic(s) sent to the standard error stream).
 *  This function is recursive; if the value in \a f is an array or an
 *  object, a properly nested jvalue tree is returned. Any leading
 *  whitespace is skipped. */
static jvalue *
readvalue( ifile *f )
{
    int c;
    jvalue *j = jnew();

    switch(( c = skipws( f ))) {
    case EOF:
        earlyeof();
        break;
    case 'f':
        if( must( f, "false" )) {
            j->d = jfalse;
            return j;
        }
        break;
    case 'n':
        if( must( f, "null" )) {
            j->d = jnull;
            return j;
        }
        break;
    case 't':
        if( must( f, "true" )) {
            j->d = jtrue;
            return j;
        }
        break;
    case '{':
        if( readseries( j, f, j->d = jobject ))
            return j;
        break;
    case '[':
        if( readseries( j, f, j->d = jarray ))
            return j;
        break;
    case '"':
        if(( j->u.s = readstring( f ))) {
            j->d = jstring;
            return j;
        }
        break;
    case '-': case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        if(( j->u.s = readnumber( f ))) {
            j->d = jnumber;
            return j;
        }
        break;
    default:
        ierr( f, "unexpected '%c'", (char)c );
    }

    jdel( j );
    return 0;
}

/** Parse the string passed in json \a into a new jvalue,
 *  which is returned. That jvalue is actually a parse tree, but for
 *  some limited cases, a single top level value might be returned.
 *  The function returns 0 on a failed parse (and diagnostics will be
 *  printed to stderr). This is slightly more liberal than the JSON
 *  standard, in that the outermost value of the stream at \a fp
 *  isn't limited to being just an array or object, but can also be
 *  any other JSON value. */
jvalue *
jparse( const char *json )
{
    if( !json )
        return 0;

    ifile f = (ifile){ .s = json, .end = json + strlen(json), .ungetch = EOF,
        .line = 1 };
    return readvalue( &f );
}

#if 0 /* UNUSED */
/** This is only used by jupdate, so we hide it static to this file.
 *  It returns true if the supplied string appears to just be an
 *  integer number.  Specifically, this means it does not contain an
 *  exponent nor a decimal point, as those typically introduce reals.
 *  It's arguable that an exponent, itself, doesn't necessarily mean
 *  a value is a real.  But, because it's so easy to exceed the range
 *  of an integer with a really huge exponent, nothing short of
 *  parsing the number itself makes it a safe call to deal with
 *  exponential integers.  So, exponents mean "not integer".  Also,
 #  because we assume that we're parsing numbers gathered by
 *  readnumber() via jparse(), we don't bother testing on isdigit()
 #  like a well behaved function would; otherwise, failing isdigit()
 #  in the while loop would also be an immediate return false. */
static bool
integerp( const char *str )
{
    if( *str == '-' || *str == '+' )
        ++str;
    while( *str )
        if( *str == '.' || *str == 'e' || *str == 'E' )
            return false;
	else
	    ++str;
    return true;
}

/** Given a jvalue, "update" it or its children. "Update" means
 *  several things, but it basically finishes the work started by
 *  jparse(). jparse() implements a quick parse of a JSON stream, but
 *  does things like leaving numbers as strings, in the event that the
 *  caller doesn't need lossy conversions introduced by atof().
 *  Calling jupdate() effectively "finishes" the parse, converting
 *  everything into native formats. */
jvalue *
jupdate( jvalue *j )
{
    if( j )
        switch( j->d ) {
        case jnumber:
	    if( integerp( j->u.s )) {
                j->u.i = strtoll( j->u.s, 0, 10 );
                j->d = jint;
            } else {
                j->u.r = strtold( j->u.s, 0 );
                j->d = jreal;
            }
            break;
        case jarray:
	case jobject:
            for( jvalue **jv = j->u.v; *jv; ++jv )
                jupdate( *jv );
            break;
        default:
            break;
        }

    return j;
}

/** Emit some number of spaces for each level of indentation we're at. */
static void
indent( FILE *fp, unsigned int n )
{
    while( n-- > 0 )
        fputs( "    ", fp );
}

/** Dump out a jvalue in some structured form. This is not a
 *  converter, this is just a debug activity. */
static int
jdumpval( FILE *fp, const jvalue *j, unsigned int depth )
{
    indent( fp, depth );

    switch( j->d ) {
    case jnull:
        fputs( "null\n", fp );
        break;
    case jtrue:
        fputs( "true\n", fp );
        break;
    case jfalse:
        fputs( "false\n", fp );
        break;
    case jstring:
        if( j->u.s ) {
            fputs( "string \"", fp );
            for( char *c = &j->u.s[0]; *c; ++c )
                if( *c == 8 )                   fputs( "\\t", fp );
                else if( *c == 10 )             fputs( "\\n", fp );
                else if( *c == 13 )             fputs( "\\r", fp );
                else if( *c == 34 )             fputs( "\\\"", fp );
                else if( *c == 92 )             fputs( "\\\\", fp );
                else if( 32 <= *c && *c < 127 ) fputc( *c, fp );
                else
                    fprintf( fp, "\\x%02x", (unsigned) *c );
            fputs( "\"\n", fp );
        } else
            fputs( "NULL string (oops)\n", fp );
        break;
    case jnumber:
        if( j->u.s )
            fprintf( fp, "number %s\n", j->u.s );
        else
            fputs( "NULL number (oops)\n", fp );
        break;
    case jint:
        fprintf( fp, "integer %lld\n", j->u.i );
        break;
    case jreal:
        fprintf( fp, "real %Lg\n", j->u.r );
        break;
    case jarray:
        fputs( "array\n", fp );
        for( jvalue **jv = j->u.v; *jv; ++jv )
            jdumpval( fp, *jv, depth+1 );
        break;
    case jobject:
        fputs( "object\n", fp );
        for( jvalue **jv = j->u.v; *jv; ++jv ) {
            indent( fp, depth+1 );
            if( (*jv)->n )
                fputs( (*jv)->n, fp );
            else
                fputs( "NULL name (oops)", fp );
            fputc( '\n', fp );
            jdumpval( fp, *jv, depth+2 );
        }
        break;
    default:
        fprintf( fp, "unknown %d (oops)\n", j->d );
        break;
    }
    return 0;
}

/** Dump out a JSON parse tree onto \a fp. This is really the entry
 *  point for the jdumpval recursion. */
int
jdump( FILE *fp, const jvalue *j )
{
    return jdumpval( fp, j, 0 );
}
#endif

#undef extern
#endif
