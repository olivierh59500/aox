#ifndef MIMEFIELDS_H
#define MIMEFIELDS_H

#include "string.h"
#include "list.h"

class Parser822;


class MimeField {
public:
    MimeField();

    void parse( Parser822 * );

    List<String> * parameterList() const;
    String parameter( const String & ) const;

    bool valid() const;
    void setValid( bool );

    void removeParameter( const String & );
    void addParameter( const String &, const String & );

private:
    class MimeFieldData * d;
};


class ContentType: public MimeField {
public:
    ContentType( const String & );

    String type() const;
    String subtype() const;

private:
    class CTData *d;
};


class ContentTransferEncoding {
public:
    ContentTransferEncoding( const String & );

    enum Encoding {
        Binary,
        Base64,
        QuotedPrintable
    };

    Encoding encoding() const;
    bool valid() const;

private:
    class CTEData *d;
};


class ContentDisposition
    : public MimeField
{
public:
    ContentDisposition( const String & );

    enum Disposition {
        Inline,
        Attachment,
        Unknown
    };

    Disposition disposition() const;

private:
    class CDData *d;
};


#endif
