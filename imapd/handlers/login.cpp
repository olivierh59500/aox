#include "login.h"

#include "imap.h"


/*! \class Login login.h
    Performs plaintext authentication (RFC 3501, �6.2.3)

    The client supplies us with a plaintext username and password, and
    we treat it as we would an AUTH=PLAIN request. (We should disallow
    this mechanism until after STARTTLS.)
*/

/*! \reimp */

void Login::parse()
{
    space();
    n = astring();
    space();
    p = astring();
    end();
}


/*! This function creates a Plain SaslMechanism, bypasses CR negotiation
    by feeding it the data it would otherwise issue a challenge for, and
    waits for its verdict.

    \sa Authenticate::execute()
*/

void Login::execute()
{
    if ( !m ) {
        m = new Plain( this );
        m->setLogin( n );
        m->setSecret( p );
    }

    m->verify();

    if ( m->done() ) {
        if ( m->state() == SaslMechanism::Failed ) {
            error( No, "LOGIN failed for '" + n + "'" );
        }
        else {
            imap()->setUid( m->uid() );
            imap()->setLogin( n );
        }
        finish();
    }
}
