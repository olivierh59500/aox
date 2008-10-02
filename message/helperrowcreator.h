// Copyright Oryx Mail Systems GmbH. All enquiries to info@oryx.com, please.

#ifndef HELPERROWCREATOR_H
#define HELPERROWCREATOR_H

#include "injector.h"
#include "stringlist.h"


class HelperRowCreator
    : public EventHandler
{
public:
    HelperRowCreator( const String &, class Transaction *, const String & );

    bool done() const;

    void execute();

    uint id( const String & );

protected:
    void add( const String &, uint );

private:
    virtual Query * makeSelect() = 0;
    virtual void processSelect( Query * );
    virtual Query * makeCopy() = 0;

private:
    class HelperRowCreatorData * d;
};


class FlagCreator
    : public HelperRowCreator
{
public:
    FlagCreator( const StringList &, class Transaction * );

private:
    Query * makeSelect();
    Query * makeCopy();

private:
    StringList names;
};


class FieldNameCreator
    : public HelperRowCreator
{
public:
    FieldNameCreator( const StringList &, class Transaction * );

private:
    Query * makeSelect();
    Query * makeCopy();

private:
    StringList names;
};


class AnnotationNameCreator
    : public HelperRowCreator
{
public:
    AnnotationNameCreator( const StringList &, class Transaction * );

private:
    Query * makeSelect();
    Query * makeCopy();

private:
    StringList names;
};


#endif