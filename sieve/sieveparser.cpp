// Copyright 2009 The Archiveopteryx Developers <info@aox.org>

#include "sieveparser.h"

#include "sievescript.h"
#include "sieveproduction.h"
#include "ustringlist.h"
#include "estringlist.h"
#include "utf.h"


class SieveParserData
    : public Garbage
{
public:
    SieveParserData(): bad( 0 ) {}

    List<SieveProduction> * bad;
    EStringList extensions;
};


/*! \class SieveParser sieveparser.h

    The SieveParser class does all the ABNF-related work for sieve
    scripts. The SieveProduction class and its subclasses do the other
    part of parsing: The Sieve ABNF grammar doesn't guarantee that
    "if" has a test as argument, etc.
*/


/*!  Constructs a Sieve parser for \a s. Doesn't actually do anything;
     the caller must call commands() or other functions to parse the
     script.
*/

SieveParser::SieveParser( const EString &s  )
    : AbnfParser( s ), d( new SieveParserData )
{
}



/*! Returns a list of the SieveProduction objects that have been
    parsed by this object, have errors and have \a toplevel as their
    ultimate SieveProduction::parent().

    The list may be empty (hopefully that is the common case) but the
    returned pointer is never null.
*/

List<SieveProduction> * SieveParser::bad( class SieveProduction * toplevel )
{
    List<SieveProduction> * r = new List<SieveProduction>;
    if ( !d->bad )
        return r;
    List<SieveProduction>::Iterator i( d->bad );
    while ( i ) {
        SieveProduction * p = i;
        ++i;
        if ( !p->error().isEmpty() ) {
            SieveProduction * t = p;
            while ( t && t != toplevel )
                t = t->parent();
            if ( t == toplevel )
                r->append( p );
        }
    }
    return r;
}


/*! Adds \a p to the list of bad sieve productions generated by this
    parser. Provided as a public function because a sieve production
    may discover its badness long after SieveParser has done its job.
*/

void SieveParser::rememberBadProduction( class SieveProduction * p )
{
    if ( !p )
        return;
    if ( !d->bad )
        d->bad = new List<SieveProduction>;
    List<SieveProduction>::Iterator i( d->bad );
    while ( i && i != p && i->start() <= p->start() )
        ++i;
    if ( !i )
        d->bad->append( p );
    else if ( i != p )
        d->bad->insert( i, p );
}


/*! Returns a list of the strings specified to
    rememberNeededExtension(), freed of duplication. The return value
    is never a null pointer.
*/

EStringList * SieveParser::extensionsNeeded() const
{
    d->extensions.removeDuplicates();
    return &d->extensions;
}


/*! Adds \a extension to the list of extensions needed by the
    generated productions. The SieveProduction subclasses call this,
    ie. using SieveParser as a common storage object.
*/

void SieveParser::rememberNeededExtension( const EString & extension )
{
    d->extensions.append( extension );
}


/*! Parses/skips a bracket comment: bracket-comment = "/" "*"
    *not-star 1*STAR *(not-star-slash *not-star 1*STAR) "/"

    No "*" "/" allowed inside a comment. (No * is allowed unless it is
    the last character, or unless it is followed by a character that
    isn't a slash.)
*/

void SieveParser::bracketComment()
{
    if ( !present( "/" "*" ) )
        return;
    int i = input().find( "*" "/", pos() );
    if ( i < 0 )
        setError( "Bracket comment not terminated" );
    else
        step( i+2-pos() );
}


/*! Parses/skips a comment: comment = bracket-comment / hash-comment.

*/

void SieveParser::comment()
{
    bracketComment();
    hashComment();
}


/*! Parses/skips a hash-cmment: hash-comment = "#" *octet-not-crlf
    CRLF
*/

void SieveParser::hashComment()
{
    if ( !present( "#" ) )
        return;
    int i = input().find( "\r" "\n", pos() );
    if ( i < 0 )
        setError( "Could not find CRLF in hash comment" );
    else
        step( i+2-pos() );

}


/*! identifier = (ALPHA / "_") *(ALPHA / DIGIT / "_")

    Records an error if no identifier is present.
*/

EString SieveParser::identifier()
{
    whitespace();
    EString r;
    char c = nextChar();
    while ( ( c == '_' ) ||
            ( c >= 'a' && c <= 'z' ) ||
            ( c >= 'A' && c <= 'Z' ) ||
            ( c >= '0' && c <= '9' && !r.isEmpty() ) ) {
        r.append( c );
        step();
        c = nextChar();
    }
    if ( r.isEmpty() )
        setError( "Could not find an identifier" );
    return r; // XXX - better to return r.lower()?

}


/*! multi-line = "text:" *(SP / HTAB) (hash-comment / CRLF)
    *(multiline-literal / multiline-dotstuff) "." CRLF

    multiline-literal = [octet-not-period *octet-not-crlf] CRLF

    multiline-dotstuff = "." 1*octet-not-crlf CRLF

    Returns an empty string if the cursor doesn't point at a
    multi-line string.
*/

UString SieveParser::multiLine()
{
    EString r;
    require( "text:" );
    while ( ok() && ( nextChar() == ' ' || nextChar() == '\t' ) )
        step();
    if ( !present( "\r\n" ) )
        hashComment();
    while ( ok() && !atEnd() && !present( ".\r\n" ) ) {
        if ( nextChar() == '.' )
            step();
        while ( ok() && !atEnd() && nextChar() != '\r' ) {
            r.append( nextChar() );
            step();
        }
        require( "\r\n" );
        r.append( "\r\n" );
    }
    Utf8Codec c;
    UString u = c.toUnicode( r );
    if ( !c.valid() )
        setError( "Encoding error: " + c.error() );
    return u;
}


/*! number = 1*DIGIT [ QUANTIFIER ]

    QUANTIFIER = "K" / "M" / "G"

    Returns 0 and calls setError() in case of error.
*/

uint SieveParser::number()
{
    bool ok = false;
    EString d = digits( 1, 30 );
    uint n = d.number( &ok );
    uint f = 1;
    if ( present( "k" ) )
        f = 1024;
    else if ( present( "m" ) )
        f = 1024*1024;
    else if ( present( "g" ) )
        f = 1024*1024*1024;

    if ( !ok )
        setError( "Number " + d + " is too large" );
    else if ( n > UINT_MAX / f )
        setError( "Number " + fn( n ) + " is too large when scaled by " +
                  fn( f ) );
    n = n * f;
    return n;
}


/*! quoted-string = DQUOTE quoted-text DQUOTE

    quoted-text = *(quoted-safe / quoted-special / quoted-other)

    quoted-other = "\" octet-not-qspecial

    quoted-safe = CRLF / octet-not-qspecial

    quoted-special     = "\" ( DQUOTE / "\" )
*/

UString SieveParser::quotedString()
{
    EString r;
    require( "\"" );
    while ( ok() && !atEnd() && nextChar() != '"' ) {
        if ( present( "\r\n" ) ) {
            r.append( "\r\n" );
        }
        else {
            if ( nextChar() == '\\' )
                step();
            r.append( nextChar() );
            step();
        }
    }
    require( "\"" );
    Utf8Codec c;
    UString u = c.toUnicode( r );
    if ( !c.valid() )
        setError( "Encoding error: " + c.error() );
    return u;
}


/*! tag = ":" identifier */

EString SieveParser::tag()
{
    whitespace();
    require( ":" );
    return ":" + identifier();
}


/*! Parses and skips whatever whitespace is at pos(). */

void SieveParser::whitespace()
{
    uint p;
    do {
        p = pos();
        switch( nextChar() ) {
        case '#':
        case '/':
            comment();
            break;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            step();
            break;
        }
    } while ( ok() && pos() > p );
}


/*! Parses, constructs and returns a SieveArgument object. Never
    returns 0.

    argument     = string-list / number / tag
*/

SieveArgument * SieveParser::argument()
{
    whitespace();

    SieveArgument * sa = new SieveArgument;
    sa->setParser( this );
    sa->setStart( pos() );

    if ( nextChar() == ':' )
        sa->setTag( tag().lower() );
    else if ( nextChar() >= '0' && nextChar() <= '9' )
        sa->setNumber( number() );
    else
        sa->setStringList( stringList() );

    sa->setError( error() );
    sa->setEnd( pos() );

    // hack: if we set a number, clear the error so the arguments()
    // will read the next argument, too
    if ( sa->number() )
        setError( "" );

    return sa;
}


/*! arguments = *argument [test / test-list]

    This is tricky. The first difficult bit. This class takes either a
    list of SieveArgument objects, or a SieveTest, or a list of
    SieveTest objects. We implement both the arguments and tests
    productions in this function.
*/

class SieveArgumentList * SieveParser::arguments()
{
    whitespace();

    SieveArgumentList * sal = new SieveArgumentList;
    sal->setParser( this );
    sal->setStart( pos() );

    uint m = 0;
    while ( ok() ) {
        m = mark();
        SieveArgument * sa = argument();
        if ( ok() )
            sal->append( sa );
    }
    restore( m );

    whitespace();
    if ( present( "(" ) ) {
        // it's a test-list
        sal->append( test() );
        whitespace();
        while ( ok() && present( "," ) )
            sal->append( test() );
        whitespace();
        require( ")" );
    }
    else if ( ok() ) {
        // it's either a test or nothing
        m = mark();
        SieveTest * st = test();
        if ( ok() )
            sal->append( st );
        else
            restore( m );
    }

    sal->setError( error() );
    sal->setEnd( pos() );
    return sal;
}


/*! block = "{" *command "}"
*/

class SieveBlock * SieveParser::block()
{
    whitespace();

    SieveBlock * sb = new SieveBlock;
    sb->setParser( this );
    sb->setStart( pos() );
    require( "{" );

    uint m = 0;
    while ( ok() ) {
        m = mark();
        SieveCommand * c = command();
        if ( ok() )
            sb->append( c );
    }
    restore( m );

    whitespace();
    require( "}" );
    sb->setError( error() );
    sb->setEnd( pos() );
    return sb;
}


/*! command = identifier arguments ( ";" / block )
*/

class SieveCommand * SieveParser::command()
{
    whitespace();

    SieveCommand * sc = new SieveCommand;
    sc->setParser( this );
    sc->setStart( pos() );

    sc->setIdentifier( identifier() );
    sc->setArguments( arguments() );
    whitespace();
    if ( nextChar() == '{' ) {
        sc->setBlock( block() );
    }
    else if ( present( ";" ) ) {
        // fine
    }
    else {
        setError( "Garbage after command: " + following() );
        // if the line ends with ';', skip ahead to it
        uint x = pos();
        while ( x < input().length() &&
                input()[x] != '\n' && input()[x] != '\r' )
            x++;
        if ( x > pos() && input()[x-1] == ';' )
            step( x - pos() );
    }

    sc->setError( error() );
    sc->setEnd( pos() );
    return sc;
}


/*! commands = *command

    start = commands

    Never returns a null pointer.
*/

List<SieveCommand> * SieveParser::commands()
{
    whitespace();
    List<SieveCommand> * l = new List<SieveCommand>;
    uint m = 0;
    while ( ok() ) {
        m = mark();
        SieveCommand * c = command();
        if ( ok() )
            l->append( c );
    }
    restore( m );
    return l;
}


/*! COMPARATOR = ":comparator" string

    Returns just the string, not ":comparator" or the whitespace.
*/

EString SieveParser::comparator()
{
    whitespace();
    require( ":comparator" );
    UString c = string();
    if ( !c.isAscii() )
        setError( "Comparator name must be all-ASCII" );
    return c.ascii();
}


/*! string = quoted-string / multi-line

*/

UString SieveParser::string()
{
    whitespace();
    if ( nextChar() == '"' )
        return quotedString();
    return multiLine();
}


/*! string-list = "[" string *("," string) "]" / string

    If there is only a single string, the brackets are optional.

    Never returns a null pointer.
*/

UStringList * SieveParser::stringList()
{
    whitespace();
    UStringList * l = new UStringList;
    if ( present( "[" ) ) {
        l->append( string() );
        whitespace();
        while ( ok() && present( "," ) )
            l->append( string() );
        whitespace();
        require( "]" );
    }
    else {
        l->append( string() );
    }
    return l;
}


/*! test = identifier arguments
*/

class SieveTest * SieveParser::test()
{
    whitespace();

    SieveTest * t = new SieveTest;
    t->setParser( this );
    t->setStart( pos() );

    t->setIdentifier( identifier() );
    if ( ok() )
        t->setArguments( arguments() );

    t->setError( error() );
    t->setEnd( pos() );
    return t;
}
