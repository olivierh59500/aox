// Copyright 2009 The Archiveopteryx Developers <info@aox.org>

#ifndef PERMISSIONS_H
#define PERMISSIONS_H

#include "event.h"
#include "estring.h"
#include "ustring.h"


class Permissions
    : public EventHandler
{
public:
    Permissions( class Mailbox *, const UString &, const EString & );

    Permissions( class Mailbox *, class User *,
                 class EventHandler * );

    enum Right {
        Lookup, // l
        Read, // r
        KeepSeen, // s
        Write, // w
        Insert, // i
        Post, // p
        CreateMailboxes, // k
        DeleteMailbox, // x
        DeleteMessages, // t
        Expunge, // e
        Admin, // a
        WriteSharedAnnotation, // n
        // New rights go above this line.
        NumRights
    };

    bool ready();
    void execute();

    void set( const EString & );
    void allow( const EString & );
    void disallow( const EString & );
    bool allowed( Right );

    EString string() const;

    Mailbox * mailbox() const;
    User * user() const;

    static char rightChar( Permissions::Right );
    static EString describe( char );

    static bool validRight( char );
    static bool validRights( const EString & );

    static EString all();

    static const char * rights;

private:
    class PermissionData * d;
};


class PermissionsChecker
    : public Garbage
{
public:
    PermissionsChecker();

    void require( Permissions *, Permissions::Right );

    Permissions * permissions( class Mailbox *, class User * ) const;

    bool allowed() const;
    bool ready() const;

    EString error() const;

private:
    class PermissionsCheckerData * d;
};


#endif
