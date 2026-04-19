#include <netdb.h>
#include <sys/socket.h>
#include <iostream>
#include <unistd.h>

struct url_parts
{
    std::string scheme;
    std::string host;
    std::string port;
    std::string path;
};

url_parts parse_url(std::string url)
{
    url_parts parts;
    size_t scheme_end = url.find("://");

    if(scheme_end != std::string::npos)
    {
        parts.scheme = url.substr(0, scheme_end);
        url.erase(0, scheme_end + 3);   // remove the scheme
    }

    size_t path_start = url.find("/");
    if(path_start != std::string::npos)
    {
        parts.path = url.substr(path_start);
        url.erase(path_start);  // now 'url' is just host[:port]
    }
    else
    {
        parts.path = "/";
    }

    size_t port_start = url.find(":");
    if(port_start != std::string::npos)
    {
        parts.host = url.substr(0, port_start);
        parts.port = url.substr(port_start + 1);
    }
    else
    {
        parts.host = url;
        parts.port = (parts.scheme == "https") ? "443" : "80";
    }

    return parts;
}

int main()
{
    url_parts parts = parse_url("http://nossl.thatjessebloke.co.uk");

    addrinfo hints = {0};
    addrinfo *result = {0};

    hints.ai_family = AF_INET;            // Supports IPv4 
    hints.ai_socktype = SOCK_STREAM;      // TCP connection

    // use .c_str() to convert std::string to a style string which getaddrinfo needs
    // this then uses getaddrinfo to do a DNS lookup
    int status = getaddrinfo(parts.host.c_str(), parts.port.c_str(), &hints, &result);

    if(status != 0)
    {
        std::cerr << "DNS Lookup failed: " << gai_strerror(status) << std::endl;
        return 1;
    }

    // Create a socket
    int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if(sock == -1)
    {
        perror("Socket creation failed");
        freeaddrinfo(result);
        return 1;
    }

    // Connect to the server
    if(connect(sock, result->ai_addr, result->ai_addrlen) == -1)
    {
        perror("Connection failed");
        close(sock);
        freeaddrinfo(result);
        return 1;
    }

    std::cout << "Successfully connected to " << parts.host << " on port " << parts.port << std::endl;

    char buffer[4096] = {0};
    std::string full_response;

    std::string command = "GET " + parts.path + " HTTP/1.0\r\nHost: " + parts.host + "\r\n\r\n";

    std::cout << command.c_str();

    int send_result = send(sock, command.c_str(), command.length(), 0);
    if(send_result != -1)
    {
        int bytes_received = 0;

        while((bytes_received = recv(sock, buffer, 4096, 0)) > 0)
        {
            full_response.append(buffer, bytes_received);
        }

        std::cout << full_response << std::endl;

        
    }

    close(sock);
    freeaddrinfo(result);

    return 0;
}