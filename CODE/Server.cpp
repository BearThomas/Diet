// Server.cpp

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <algorithm> 

#ifdef _WIN32
    // Windows
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    
    #pragma comment(lib, "Ws2_32.lib")
    #pragma comment(lib, "Mswsock.lib")
    
    #define close closesocket
    #define read recv
    #define write send
    #define socklen_t int
#else
    // Linux/Mac
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <errno.h>
#endif

#include <thread>
#include <mutex>

using namespace std;

