// Copyright Oryx Mail Systems GmbH. All enquiries to info@oryx.com, please.

#ifndef SERVER_H
#define SERVER_H


class Server {
public:
    Server( const char *, int, char *[] );

    enum Stage {
        Test, // MUST BE FIRST
        Configuration,
        Files,
        LogSetup,
        Loop,
        Report,
        Fork,
        PidFile,
        LogStartup,
        Secure,
        Finish // MUST BE LAST
    };

    void setup( Stage );
    void execute();

private:
    class ServerData * d;

    void test();
    void configuration();
    void files();
    void loop();
    void logSetup();
    void fork();
    void pidFile();
    void logStartup();
    void secure();
};


#endif
