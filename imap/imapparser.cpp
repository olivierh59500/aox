// Copyright 2009 The Archiveopteryx Developers <info@aox.org>

#include "imapparser.h"

#include "configuration.h"
#include "ustring.h"
#include "utf.h"


/*! \class ImapParser imapparser.h
    IMAP-specific ABNF parsing functions.

    This subclass of AbnfParser provides functions like nil(), string(),
    and literal() for use by IMAP and individual IMAP Commands.
*/

/*! Creates a new ImapParser for the string \a s.

    In typical use, the parser object is created by IMAP::addCommand()
    for a complete (possibly multi-line, in the presence of literals)
    command received from the client.
*/

ImapParser::ImapParser( const EString &s )
    : AbnfParser( s )
{
}


/*! Returns the first line of this IMAP command, meant for logging.

    This function assumes that the object was constructed for the entire
    text of an IMAP command, and that multiline commands are constructed
    with CRLF, as IMAP::parse() does.
*/

EString ImapParser::firstLine()
{
    return str.mid( 0, str.find( '\r' ) );
}


/*! Extracts and returns an IMAP command tag (a non-empty sequence of
    any ASTRING-CHAR except '+'), advancing the cursor past the end of
    the tag. It is an error if no valid tag is found at the cursor.
*/

EString ImapParser::tag()
{
    EString r;

    char c = nextChar();
    while ( c > ' ' && c < 127 && c != '(' && c != ')' && c != '{' &&
            c != '%' && c != '*' && c != '"' && c != '\\' && c != '+' )
    {
        step();
        r.append( c );
        c = nextChar();
    }

    if ( r.isEmpty() )
        setError( "Expected IMAP tag, but saw: " + following().quoted() );

    return r;
}


/*! Extracts and returns an IMAP command name (a single atom, optionally
    prefixed by "UID "), advancing the cursor past the end of the name.
    It is an error if no syntactically valid command name is found at
    the cursor.
*/

EString ImapParser::command()
{
    EString r;

    if ( present( "uid " ) )
        r.append( "uid " );

    char c = nextChar();
    while ( c > ' ' && c < 127 && c != '(' && c != ')' && c != '{' &&
            c != '%' && c != '*' && c != '"' && c != '\\' && c != ']' )
    {
        step();
        r.append( c );
        c = nextChar();
    }

    if ( r.isEmpty() || r == "uid " )
        setError( "Expected IMAP command name, but saw: '" +
                  following() + "'" );

    return r;
}


/*! Extracts and returns a non-zero number at the cursor, advancing the
    cursor past its end. It is an error if there is no non-zero number()
    at the cursor.
*/

uint ImapParser::nzNumber()
{
    uint n = number();
    if ( ok() && n == 0 )
        setError( "Expected nonzero number, but saw 0 followed by: " +
                  following() );
    return n;
}


/*! Extracts and returns a single atom at the cursor, advancing the
    cursor past its end. It is an error if no atom is found at the
    cursor.
*/

EString ImapParser::atom()
{
    EString r;

    char c = nextChar();
    while ( c > ' ' && c < 127 &&
            c != '(' && c != ')' && c != '{' && c != ']' &&
            c != '"' && c != '\\' && c != '%' && c != '*' )
    {
        step();
        r.append( c );
        c = nextChar();
    }

    if ( r.isEmpty() )
        setError( "Expected IMAP atom, but saw: " + following() );

    return r;
}


/*! Extracts and returns one or more consecutive list-chars (ATOM-CHAR
    or list-wildcards or resp-specials) at the cursor, and advances the
    cursor to point past the last one. It is an error if no list-chars
    are found at the cursor.
*/

EString ImapParser::listChars()
{
    EString r;

    char c = nextChar();
    while ( c > ' ' && c < 127 && c != '(' && c != ')' && c != '{' &&
            c != '"' && c != '\\' )
    {
        step();
        r.append( c );
        c = nextChar();
    }

    if ( r.isEmpty() )
        setError( "Expected 1*list-char, but saw: " + following() );

    return r;
}


/*! Requires that the atom "NIL" be present, and advances the cursor
    past its end. It is an error if NIL is not present at the cursor.
*/

void ImapParser::nil()
{
    EString n( atom() );
    if ( n.lower() != "nil" )
        setError( "Expected NIL, but saw: " + n );
}


/*! Parses and returns an IMAP quoted-string at the cursor, and advances
    the cursor past the ending '"' character. It is an error if a valid
    quoted-string does not occur at the cursor.
*/

EString ImapParser::quoted()
{
    EString r;

    char c = nextChar();
    if ( c != '"' ) {
        setError( "Expected quoted string, but saw: " + following() );
        return r;
    }

    step();
    c = nextChar();
    while ( c != '"' && c > 0 && c != 10 && c != 13 ) {
        if ( c == '\\' ) {
            step();
            c = nextChar();
            if ( c == 0 || c == 10 || c == 13 )
                setError( "Quoted string contained bad char: " +
                          following() );
        }
        step();
        r.append( c );
        c = nextChar();
    }

    if ( c != '"' )
        setError( "Quoted string incorrectly terminated: " + following() );
    else
        step();

    Utf8Codec utf8;
    UString u( utf8.toUnicode( r ) );

    return utf8.fromUnicode( u );
}


/*! Parses and returns an IMAP literal at the cursor, and advances the
    cursor past its contents. It is an error if a valid literal is not
    found at the cursor.

    This function depends on the IMAP parser to insert the CRLF before
    the literal's contents, and to ensure that the literal's contents
    are the right size.
*/

EString ImapParser::literal()
{
    char c = nextChar();
    if ( c == '~' ) {
        step();
        c = nextChar();
    }
    if ( c != '{' ) {
        setError( "Expected literal, but saw: " + following() );
        return "";
    }

    step();
    uint len = number();
    if ( !ok() )
        return "";
    if ( nextChar() == '+' )
        step();
    if ( nextChar() != '}' ) {
        setError( "Expected literal-}, but saw: " + following() );
        return "";
    }
    if ( len > literalSizeLimit() ) {
        setError( "Literal too big: " +
                  fn( len ) + ">" + fn( literalSizeLimit() ) );
        return "";
    }

    step();
    require( "\r\n" );
    if ( !ok() )
        return "";

    EString r( str.mid( pos(), len ) );
    step( len );
    return r;
}


/*! Parses and returns an IMAP string at the cursor, and advances the
    cursor past its end. It is an error if no string is found at the
    cursor.
*/

EString ImapParser::string()
{
    char c = nextChar();

    if ( c == '"' )
        return quoted();
    else if ( c == '{' || c == '~' )
        return literal();

    setError( "Expected string, but saw: " + following() );
    return "";
}


/*! Parses and returns an IMAP nstring at the cursor, and advances the
    cursor past its end. It is an error if no nstring is found at the
    cursor.
*/

EString ImapParser::nstring()
{
    char c = nextChar();
    if ( c == '"' || c == '{' )
        return string();

    nil();
    return "";
}


/*! Parses and returns an IMAP astring at the cursor, and advances the
    cursor past its end. It is an error if no astring is found at the
    cursor.
*/

EString ImapParser::astring()
{
    char c = nextChar();
    if ( c == '"' || c == '{' )
        return string();

    EString r;
    while ( c > ' ' && c < 128 &&
            c != '(' && c != ')' && c != '{' &&
            c != '"' && c != '\\' &&
            c != '%' && c != '*' )
    {
        step();
        r.append( c );
        c = nextChar();
    }

    if ( r.isEmpty() )
        setError( "Expected astring, but saw: " + following() );

    return r;
}


/*! Parses and returns an IMAP list-mailbox (which is the same as an
    atom(), except that the three additional characters %, *, and ] are
    allowed), advancing the cursor past its end. It is an error if no
    list-mailbox is found at the cursor.
*/

EString ImapParser::listMailbox()
{
    EString r;

    char c = nextChar();
    if ( c == '"' || c == '{' )
        return string();

    while ( c > ' ' &&
            c != '(' && c != ')' && c != '{' &&
            c != '"' && c != '\\' )
    {
        step();
        r.append( c );
        c = nextChar();
    }

    if ( r.isEmpty() )
        setError( "Expected list-mailbox, but saw: " + following() );

    return r;
}


/*! Parses a returns flag name, advancing the cursor past its end. It is
    an error if no valid flag name was present at the cursor.
*/

EString ImapParser::flag()
{
    if ( !present( "\\" ) )
        return atom();

    EString r = "\\" + atom();
    EString l = r.lower();
    if ( l == "\\answered" || l == "\\flagged" || l == "\\deleted" ||
         l == "\\seen" || l == "\\draft" )
        return r;

    setError( "Expected flag name, but saw: " + r );
    return "";
}


/*! Returns a string of between \a min and \a max letters ([A-Za-z]),
    digits ([0-9]) and dots at the cursor, advancing the cursor past
    them. It is an error if fewer than \a min letters/digits/dots are
    found at the cursor. Consecutive dots are accepted.
*/

EString ImapParser::dotLetters( uint min, uint max )
{
    EString r;
    uint i = 0;
    char c = nextChar();
    while ( i < max &&
            ( ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' ) ||
              ( c >= '0' && c <= '9' ) || ( c == '.' ) ) )
    {
        step();
        r.append( c );
        c = nextChar();
        i++;
    }

    if ( i < min )
        setError( "Expected at least " + fn( min-i ) + " more "
                  "letters/digits/dots, but saw: " + following() );

    return r;
}


static uint literalSizeLimit = 0;


/*! Returns the maximum size if an inbound literal; depends on the
    memory-limit configuration variable. SmtpClient::observedSize() is
    related to this, although slightly different.
*/

uint ImapParser::literalSizeLimit()
{
    if ( !::literalSizeLimit )
        ::literalSizeLimit =
              150000 * Configuration::scalar( Configuration::MemoryLimit );
    return ::literalSizeLimit;
}
