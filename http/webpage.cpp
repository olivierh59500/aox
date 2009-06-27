// Copyright 2009 The Archiveopteryx Developers <info@aox.org>

#include "webpage.h"

#include "map.h"
#include "link.h"
#include "http.h"
#include "query.h"
#include "field.h"
#include "message.h"
#include "mailbox.h"
#include "fetcher.h"
#include "pagecomponent.h"
#include "messagecache.h"
#include "httpsession.h"
#include "frontmatter.h"
#include "mimefields.h"
#include "ustring.h"
#include "codec.h"
#include "user.h"
#include "utf.h"

#include "components/loginform.h"


static User * archiveUser;
static Map<Permissions> * archiveMailboxPermissions;


class WebPageData
    : public Garbage
{
public:
    WebPageData()
        : link( 0 ),
          checker( 0 ), requiresUser( false ), user( 0 ),
          uniq( 0 ), finished( false )
    {}

    struct PermissionRequired
        : public Garbage
    {
        PermissionRequired(): m( 0 ), r( Permissions::Read ) {}
        Mailbox * m;
        Permissions::Right r;
    };

    Link * link;

    List<PageComponent> components;

    List<PermissionRequired> needed;
    PermissionsChecker * checker;
    bool requiresUser;
    User * user;

    EString contentType;
    EString contents;

    uint uniq;
    bool finished;
};


/*! \class WebPage webpage.h

    A WebPage is a collection of PageComponents, each with some relevant
    FrontMatter objects. It waits for all its components to assemble
    their contents, and then composes the response.
*/

/*! Creates a new WebPage to serve \a link. */

WebPage::WebPage( Link * link )
    : d( new WebPageData )
{
    d->link = link;
    if ( d->link->type() == Link::Webmail )
        requireUser();
}


/*! Returns a non-zero pointer to this WebPage's Link object. */

Link * WebPage::link() const
{
    return d->link;
}


/*! Returns a pointer to the currently authenticated user, which may be
    0 if no user is authenticated. Provided for convenience. */

User * WebPage::user() const
{
    return d->link->server()->user();
}


/*! Returns a pointer to the HTTP server that is serving this page.
    Provided for convenience. */

HTTP * WebPage::server() const
{
    return d->link->server();
}


/*! Returns the value of the parameter named \a s, or an empty string if
    that parameter was not specified in the request. Provided for
    convenience. */

UString WebPage::parameter( const EString & s ) const
{
    return d->link->server()->parameter( s );
}


/*! Sets the contents of this response to \a text, with the
    Content-type \a type. (Headers are added to the server
    instead.)
*/

void WebPage::setContents( const EString &type, const EString &text )
{
    d->contentType = type;
    d->contents = text;
}


/*! This function is meant to be called by subclasses' "execute()"
    method, and is responsible for sending the text specified with
    setContents(). If setContents() has not been called, finish()
    doesn't send anything, just marks this response as finished().
*/

void WebPage::finish()
{
    if ( !d->contentType.isEmpty() )
        d->link->server()->respond( d->contentType, d->contents );
    d->finished = true;
}


/*! Returns true only if finish() has been called. */

bool WebPage::finished() const
{
    return d->finished;
}


/*! Adds the PageComponent \a pc to this WebPage. If \a after is
    present and non-null, \a pc is added immediately after \a
    after. If \a after is null (this is the default), \a pc is added
    at the end. */

void WebPage::addComponent( PageComponent * pc, const PageComponent * after )
{
    List<PageComponent>::Iterator i;
    if ( after ) {
        i = d->components.find( after );
        if ( i )
            ++i;
    }
    if ( i )
        d->components.insert( i, pc );
    else
        d->components.append( pc );

    pc->setPage( this );
}


void WebPage::execute()
{
    if ( finished() )
        return;

    if ( !permitted() )
        return;

    bool done = true;
    uint status = 200;
    uint c = 0;
    while ( c != d->components.count() ) {
        c = d->components.count();
        List<PageComponent>::Iterator it( d->components );
        while ( it ) {
            PageComponent * p = it;
            ++it;
            if ( !p->done() ) {
                p->execute();
                done = false;
            }
            if ( p->status() > status )
                status = p->status();
        }
    }

    if ( !done )
        return;

    link()->server()->setStatus( status, "OK" );
    setContents( "text/html; charset=utf-8", contents() );
    finish();
}


/*! Returns the text formed by assembling the contents of all of the
    FrontMatter objects required by the components of this page. The
    return value is meaningful only when all the components are done
    executing; and it is meant for inclusion in the HEAD section of
    an HTML page.
*/

EString WebPage::frontMatter() const
{
    EString title;
    EString style;
    EString script;
    List<FrontMatter> frontMatter;

    List<PageComponent>::Iterator it( d->components );
    while ( it ) {
        List<FrontMatter>::Iterator f( it->frontMatter() );
        while ( f ) {
            if ( f->element() == "title" && title.isEmpty() )
                title = *f;
            else if ( f->element() == "style" )
                style.append( *f );
            else if ( f->element() == "script" )
                script.append( *f );
            else
                frontMatter.append( f );
            ++f;
        }
        ++it;
    }

    EString s;

    s.append( "<title>" );
    s.append( title );
    s.append( "</title>\n" );

    s.append( "<style type=\"text/css\">\n" );
    s.append( *FrontMatter::styleSheet() );
    s.append( style );
    s.append( "</style>\n" );

    if ( !script.isEmpty() ) {
        s.append( *FrontMatter::jQuery() );
        s.append( "<script type=\"text/javascript\">\n" );
        s.append( script );
        s.append( "</script>\n" );
    }

    List<FrontMatter>::Iterator f( frontMatter );
    while ( f ) {
        s.append( *f );
        ++f;
    }

    return s;
}


/*! Returns the text formed by concatenating the contents of all of this
    page's constituent components. The return value is not meaningful if
    PageComponent::done() is not true for all of the components.
*/

EString WebPage::componentText() const
{
    EString s;

    List<PageComponent>::Iterator it( d->components );
    while ( it ) {
        s.append( it->contents() );
        ++it;
    }

    return s;
}


/*! Returns the HTML output of this page, as it currently looks. The
    return value isn't sensible until all components are ready, as
    execute() checks.
*/

EString WebPage::contents() const
{
    EString html(
        "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\">\n"
    );

    html.append( "<html><head>\n" );
    html.append( frontMatter() );
    html.append( "</head><body>\n" );
    html.append( componentText() );
    html.append( "</body></html>\n" );

    return html;
}


/*! Demands that the webpage be accessed only by authenticated users.
    This is used instead of requireRight() when there is no mailbox to
    be accessed, e.g. by /webmail/views. */

void WebPage::requireUser()
{
    d->requiresUser = true;
}


/*! Notes that this WebPage requires \a r on \a m. execute() should
    proceed only if and when permitted() is true.
*/

void WebPage::requireRight( Mailbox * m, Permissions::Right r )
{
    WebPageData::PermissionRequired * pr = new WebPageData::PermissionRequired;
    pr->m = m;
    pr->r = r;
    d->needed.append( pr );
}


/*! Returns true if this WebPage has the rights demanded by
    requireRight(), and is permitted to proceed, and false if
    it either must abort due to lack of rights or wait until
    Permissions has fetched more information.

    If permitted() denies permission, it also sets a suitable error
    message.
*/

bool WebPage::permitted()
{
    if ( finished() )
        return false;

    if ( !d->requiresUser && d->needed.isEmpty() )
        return true;

    HTTP * server = link()->server();
    UString login( server->parameter( "login" ) );

    if ( d->user ) {
        // leave it
    }
    else if ( link()->type() == Link::Archive ) {
        if ( !::archiveUser ) {
            ::archiveUser = new User;
            UString u;
            u.append( "anonymous" );
            ::archiveUser->setLogin( u );
            ::archiveUser->refresh( this );
            Allocator::addEternal( ::archiveUser, "anonymous archive user" );
        }
        d->user = ::archiveUser;
    }
    else if ( !login.isEmpty() ) {
        d->user = new User;
        d->user->setLogin( login );
        d->user->refresh( this );
    }
    else if ( server->session() ) {
        d->user = server->session()->user();
    }

    if ( !d->user ) {
        handleAuthentication();
        return false;
    }

    if ( d->user->state() == User::Unverified ) {
        d->user->refresh( this );
        return false;
    }

    if ( !d->checker && !d->needed.isEmpty() ) {
        bool anon = false;
        if ( d->user->login() == "anonymous" )
            anon = true;
        d->checker = new PermissionsChecker;
        List<WebPageData::PermissionRequired>::Iterator i( d->needed );
        while ( i ) {
            Permissions * p = d->checker->permissions( i->m, d->user );
            if ( p ) {
                // just use that
            }
            else if ( d->user->state() != User::Refreshed ) {
                // don't add anything
            }
            else if ( !anon ) {
                p = new Permissions( i->m, d->user, this );
            }
            else {
                if ( !::archiveMailboxPermissions ) {
                    ::archiveMailboxPermissions = new Map<Permissions>;
                    Allocator::addEternal( ::archiveMailboxPermissions,
                                           "archives permissions checkers" );
                }
                p = ::archiveMailboxPermissions->find( i->m->id() );
                if ( !p ) {
                    p = new Permissions( i->m, d->user, this );
                    ::archiveMailboxPermissions->insert( i->m->id(), p );
                }
            }
            if ( p )
                d->checker->require( p, i->r );
            ++i;
        }
    }

    if ( d->checker && !d->checker->ready() )
        return false;

    if ( link()->type() == Link::Archive ) {
        if ( d->user->state() == User::Refreshed &&
             d->checker && d->checker->allowed() )
            return true;
        server->setStatus( 403, "Forbidden" );
        setContents( "text/plain",
                     d->checker->error().simplified() + "\n" );
        finish();
        return false;
    }
    else {
        UString passwd( server->parameter( "passwd" ) );
        if ( d->user->state() != User::Refreshed ||
             ( ( !server->session() || server->session()->expired() ) &&
               passwd != d->user->secret() ) ||
             ( d->checker && !d->checker->allowed() ) )
        {
            handleAuthentication();
            return false;
        }
        else {
            HttpSession * s = server->session();
            if ( !s || s->user()->login() != d->user->login() ) {
                s = new HttpSession;
                server->setSession( s );
            }
            s->setUser( d->user );
            s->refresh();
            return true;
        }
    }

    return false;
}


/*! Rewrites this page to be a plain login form which will reload the
    same page when the correct password is entered.
*/

void WebPage::handleAuthentication()
{
    d->finished = true;

    d->needed.clear();
    d->checker = 0;
    d->components.clear();
    PageComponent * lf = new LoginForm;
    addComponent( lf );

    // XXX: The following call will actually cause our own execute() to
    // be called again. If that call is not ignored (as we do by setting
    // d->finished early), the resulting loop is staggering in its
    // infinitude. What a hack.
    lf->execute();

    link()->server()->setStatus( 200, "OK" );
    setContents( "text/html; charset=utf-8", contents() );
    finish();
}



/*! \class PageFragment webpage.h
    Collects output from PageComponents and serves it unadorned.

    This is meant to serve text from components for AJAX requests.
*/

/*! Creates a new PageFragment for \a link. */

PageFragment::PageFragment( Link * link )
    : WebPage( link )
{
}


/*! Returns the text assembled from the constituent component(s). */

EString PageFragment::contents() const
{
    return componentText();
}


/*! AJAX calls without authentication are simply rejected. */

void PageFragment::handleAuthentication()
{
    link()->server()->setStatus( 403, "Forbidden" );
    setContents( "text/html", "Forbidden." );
    finish();
}



class BodypartPageData
    : public Garbage
{
public:
    BodypartPageData()
        : b( 0 ), c( 0 )
    {}

    Query * b;
    Query * c;
};


/*! \class BodypartPage webpage.h
    A subclass of WebPage, meant to serve unadorned bodyparts.
*/

/*! Creates a BodypartPage object to serve \a link, which must refer to
    a message, uid, and part number (which may or may not be valid).
*/

BodypartPage::BodypartPage( Link * link )
    : WebPage( link ),
      d( new BodypartPageData )
{
}


void BodypartPage::execute()
{
    if ( !d->b ) {
        requireRight( link()->mailbox(), Permissions::Read );

        d->b = new Query(
            "select text, data from bodyparts b join "
            "part_numbers p on (p.bodypart=b.id) join "
            "mailbox_messages mm on (mm.message=p.message) "
            "where mm.mailbox=$1 and mm.uid=$2 and p.part=$3",
            this
        );
        d->b->bind( 1, link()->mailbox()->id() );
        d->b->bind( 2, link()->uid() );
        d->b->bind( 3, link()->part() );
        d->b->execute();

        d->c = new Query(
            "select value from header_fields hf join mailbox_messages mm "
            "on (mm.message=hf.message) where mm.mailbox=$1 and mm.uid=$2 "
            "and (hf.part=$3 or hf.part=$4) and hf.field=$5 "
            "order by part<>$3",
            this
        );
        d->c->bind( 1, link()->mailbox()->id() );
        d->c->bind( 2, link()->uid() );

        EString part( link()->part() );
        d->c->bind( 3, part );
        if ( part == "1" )
            d->c->bind( 4, "" );
        else if ( part.endsWith( ".1" ) )
            d->c->bind( 4, part.mid( 0, part.length()-1 ) + "rfc822" );
        else
            d->c->bind( 4, part );

        d->c->bind( 5, HeaderField::ContentType );
        d->c->execute();
    }

    if ( !permitted() )
        return;

    if ( !d->b->done() || !d->c->done() )
        return;

    Row * r;

    EString t( "text/plain" );
    r = d->c->nextRow();
    if ( r )
        t = r->getEString( "value" );

    EString b;
    r = d->b->nextRow();

    if ( !r ) {
        // XXX: Invalid part
    }
    else if ( r->isNull( "data" ) ) {
        b = r->getEString( "text" );

        ContentType * ct = new ContentType;
        ct->parse( t );

        if ( !ct->parameter( "charset" ).isEmpty() ) {
            Utf8Codec u;
            Codec * c = Codec::byName( ct->parameter( "charset" ) );
            if ( c )
                b = c->fromUnicode( u.toUnicode( b ) );
            else
                ct->addParameter( "charset", "utf-8" );
        }
    }
    else {
        b = r->getEString( "data" );
    }

    setContents( t, b );
    finish();
}


class MessagePageData
    : public Garbage
{
public:
    MessagePageData()
        : message( 0 ), query( 0 )
    {}

    Message * message;
    Query * query;
};


/*! \class MessagePage webpage.h
    Renders a single RFC822 message.
*/


MessagePage::MessagePage( Link * link )
    : WebPage( link ),
      d( new MessagePageData )
{
}


void MessagePage::execute()
{
    if ( !d->message ) {
        Mailbox * m = link()->mailbox();

        requireRight( m, Permissions::Read );

        d->message = MessageCache::find( m, link()->uid() );
        if ( !d->message ) {
            if ( !d->query ) {
                d->query = new Query( "select message from mailbox_messages "
                                      "where mailbox=$1 and uid=$2", this );
                d->query->bind( 1, m->id() );
                d->query->bind( 2, link()->uid() );
                d->query->execute();
            }
            if ( !d->query->done() )
                return;
            Row * r = d->query->nextRow();
            d->message = new Message;
            if ( r ) {
                d->message->setDatabaseId( r->getInt( "message" ) );
                MessageCache::insert( m, link()->uid(), d->message );
            }
            else {
                // the message has been deleted or never was
                // there. XXX what to do?
                d->message->setHeadersFetched();
                d->message->setAddressesFetched();
                d->message->setBodiesFetched();
            }
        }
        List<Message> messages;
        messages.append( d->message );

        Fetcher * f = new Fetcher( &messages, this );
        if ( !d->message->hasHeaders() )
            f->fetch( Fetcher::OtherHeader );
        if ( !d->message->hasBodies() )
            f->fetch( Fetcher::Body );
        if ( d->message->hasAddresses() )
            f->fetch( Fetcher::Addresses );
        f->execute();
    }

    if ( !permitted() )
        return;

    if ( !( d->message->hasHeaders() &&
            d->message->hasAddresses() &&
            d->message->hasBodies() ) )
        return;

    setContents( "message/rfc822", d->message->rfc822() );
    finish();
}


/*! Returns a different nonzero number each time called. For use by
    components who need to make unique identifiers of some kind.
*/

uint WebPage::uniqueNumber()
{
    return ++d->uniq;
}


/*! \class StaticBlob webpage.h
    A class to serve a static blob of content (e.g. jquery.js).

    The caller is expected to call setContents() before execute().
*/

StaticBlob::StaticBlob( Link * link )
    : WebPage( link )
{
}


void StaticBlob::execute()
{
    link()->server()->setStatus( 200, "OK" );
    finish();
}
