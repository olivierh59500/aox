#ifndef OCSERVER_H
#define OCSERVER_H

#include "list.h"
#include "connection.h"

class String;


class OCServer
    : public Connection
{
public:
    OCServer( int );

    void parse();
    void react( Event );

    static void distribute( const String & );
    static List< OCServer > *connections();

private:
    class OCSData *d;
};


#endif
