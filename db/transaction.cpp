// Copyright Oryx Mail Systems GmbH. All enquiries to info@oryx.com, please.

#include "transaction.h"

#include "allocator.h"
#include "database.h"
#include "query.h"
#include "event.h"
#include "scope.h"
#include "list.h"


class TransactionData
    : public Garbage
{
public:
    TransactionData()
        : state( Transaction::Inactive ), owner( 0 ), db( 0 ),
          queries( 0 ), failedQuery( 0 )
    {}

    Transaction::State state;
    EventHandler *owner;
    Database *db;

    List< Query > *queries;

    Query * failedQuery;
    String error;
};


/*! \class Transaction transaction.h
    This class manages a single database transaction.

    A Transaction accepts a series of queries via enqueue(), and sends
    them to the server when execute() is called. It ends when commit()
    or rollback() is called. Its state() indicates its progress.

    During its lifetime, a Transaction commandeers a database handle.
*/


/*! Creates a new Transaction owned by \a ev (which MUST NOT be 0). */

Transaction::Transaction( EventHandler *ev )
    : d( new TransactionData )
{
    d->owner = ev;
}


/*! Sets this Transaction's Database handle to \a db.
    This function is used by the Database when the BEGIN is processed.
*/

void Transaction::setDatabase( Database *db )
{
    d->db = db;
}


/*! Sets this Transaction's state to \a s, which must be one of Inactive
    (if it has not yet been started), Executing, Completed, or Failed.
*/

void Transaction::setState( State s )
{
    d->state = s;
}


/*! Returns the current state of this Transaction. */

Transaction::State Transaction::state() const
{
    return d->state;
}


/*! Returns true only if this Transaction has failed, and false if it
    succeeded, or is in progress.
*/

bool Transaction::failed() const
{
    return d->state == Failed;
}


/*! Returns true only if this Transaction has either succeeded or
    failed, and false if it is still awaiting completion.
*/

bool Transaction::done() const
{
    return d->state == Completed || d->state == Failed;
}


/*! Sets this Transaction's state() to Failed, and records the error
    message \a s. The first \a query that failed is recorded, and is
    returned by failedQuery().
*/

void Transaction::setError( Query * query, const String &s )
{
    // We want to keep only the first recorded error.
    if ( d->state != Failed ) {
        d->failedQuery = query;
        d->error = s;
    }
    d->state = Failed;
}


/*! Returns the error message associated with this Transaction. This
    value is meaningful only if the Transaction has failed().
*/

String Transaction::error() const
{
    return d->error;
}


/*! Returns a pointer to the first Query in this transaction that
    failed. The return value is meaningful only if the transaction
    has failed, and 0 otherwise.

    The return value may also be 0 if the Transaction has been forcibly
    rolled back by the Postgres class because of a timeout (such as the
    caller forgetting to ever commit() the Transaction).

    This function is useful in composing error messages.
*/

Query * Transaction::failedQuery() const
{
    return d->failedQuery;
}


static PreparedStatement * begin = 0;
static PreparedStatement * commit = 0;
static PreparedStatement * rollback = 0;
static void prepare()
{
    if ( begin )
        return;
    begin = new PreparedStatement( "begin" );
    commit = new PreparedStatement( "commit" );
    rollback = new PreparedStatement( "rollback" );
    Allocator::addEternal( begin, "SQL begin" );
    Allocator::addEternal( commit, "SQL commit" );
    Allocator::addEternal( rollback, "SQL rollback" );
}


/*! Enqueues the query \a q within this Transaction, to be sent to the
    server only after execute() is called. The BEGIN is automatically
    enqueued before the first query in a Transaction.
*/

void Transaction::enqueue( Query *q )
{
    if ( !d->queries ) {
        d->queries = new List< Query >;
        ::prepare();
        Query * begin = new Query( *::begin, 0 );
        begin->setTransaction( this );

        // If setDatabase() has already been called, we were probably
        // started by updateSchema().
        if ( !d->db )
            Database::submit( begin );
        else
            d->queries->append( begin );
    }

    q->setTransaction( this );
    d->queries->append( q );
}


/*! Issues a ROLLBACK to abandon the Transaction (after sending any
    queries that were already enqueued). The owner is notified of
    completion.
*/

void Transaction::rollback()
{
    ::prepare();
    Query * q = new Query( *::rollback, d->owner );
    q->setTransaction( this );
    if ( d->queries )
        d->queries->append( q );
    execute();
}


/*! Issues a COMMIT to complete the Transaction (after sending any
    queries that were already enqueued). The owner is notified of
    completion.

    For a failed() Transaction, commit() is equivalent to rollback().
*/

void Transaction::commit()
{
    ::prepare();
    Query * q = new Query( *::commit, d->owner );
    q->setTransaction( this );
    if ( d->queries )
        d->queries->append( q );
    execute();
}


/*! Executes the queries enqueued so far. */

void Transaction::execute()
{
    List< Query >::Iterator it( d->queries );
    while ( it ) {
        it->setState( Query::Submitted );
        ++it;
    }

    // If our BEGIN has not yet been processed (and setDatabase() called
    // as a result), the queue will eventually be processed anyway.
    if ( d->db )
        d->db->processQueue();
}


/*! Returns a pointer to the List of queries that have been enqueue()d
    within this Transaction, but not yet processed by the database. The
    pointer will not be 0 after the first query has been enqueued. The
    state of each Query will be Submitted if execute() has been called
    after it was enqueued. Queries are removed from the list once they
    have been processed.

    This function is meant for use by the Database, in order to retrieve
    the Queries that need processing.
*/

List< Query > *Transaction::enqueuedQueries() const
{
    return d->queries;
}


/*! Returns a pointer to the owner of this query, as specified to the
    constructor. Transactions MUST have owners, so this function may
    not return 0.
*/

EventHandler * Transaction::owner() const
{
    return d->owner;
}


/*! Notifies the owner of this Transaction about a significant event. */

void Transaction::notify()
{
    Scope s( d->owner->log() );
    d->owner->execute();
}
