// Copyright Oryx Mail Systems GmbH. All enquiries to info@oryx.com, please.

#include "sendmail.h"

#include "link.h"
#include "webpage.h"
#include "frontmatter.h"
#include "message.h"


class SendmailData
    : public Garbage
{
public:
	SendmailData()
        : message( 0 )
    {}

    Message * message;
};


/*! \class Sendmail sendmail.h
    Sends a single message (submitted via FormMail).
*/

/*! Creates a new Sendmail component. */

Sendmail::Sendmail()
    : PageComponent( "sendmail" )
{
}


void Sendmail::execute()
{
    setContents( "sent" );
}
