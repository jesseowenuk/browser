#include <netdb.h>
#include <sys/socket.h>
#include <iostream>
#include <unistd.h>
#include <assert.h>
#include <map>
#include <algorithm>

struct uri
{
    std::string uri;
    std::string scheme;
    std::string host;
    std::string port;
    std::string path;
    std::map<std::string, std::string> headers = {};
};

uri parse_url(std::string url)
{
    uri current_uri;
    size_t scheme_end = url.find("://");

    if(scheme_end != std::string::npos)
    {
        current_uri.scheme = url.substr(0, scheme_end);
        url.erase(0, scheme_end + 3);   // remove the scheme
    }

    size_t path_start = url.find("/");
    if(path_start != std::string::npos)
    {
        current_uri.path = url.substr(path_start);
        url.erase(path_start);  // now 'url' is just host[:port]
    }
    else
    {
        current_uri.path = "/";
    }

    size_t port_start = url.find(":");
    if(port_start != std::string::npos)
    {
        current_uri.host = url.substr(0, port_start);
        current_uri.port = url.substr(port_start + 1);
    }
    else
    {
        current_uri.host = url;
        current_uri.port = (current_uri.scheme == "https") ? "443" : "80";
    }

    return current_uri;
}

int main()
{
    uri current_uri = parse_url("http://nossl.thatjessebloke.co.uk");

    addrinfo hints = {0};
    addrinfo *result = {0};

    hints.ai_family = AF_INET;            // Supports IPv4 
    hints.ai_socktype = SOCK_STREAM;      // TCP connection

    // use .c_str() to convert std::string to a style string which getaddrinfo needs
    // this then uses getaddrinfo to do a DNS lookup
    int status = getaddrinfo(current_uri.host.c_str(), current_uri.port.c_str(), &hints, &result);

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

    std::cout << "Successfully connected to " << current_uri.host << " on port " << current_uri.port << std::endl;

    char buffer[4096] = {0};
    std::string full_response;

    std::string command = "GET " + current_uri.path + " HTTP/1.0\r\nHost: " + current_uri.host + "\r\n\r\n";

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

        size_t deliminator = full_response.find("\r\n");
        std::string status_line;

        if(deliminator != std::string::npos)
        {
            status_line = full_response.substr(0, deliminator);
            full_response.erase(0, status_line.length());   
        }

        deliminator = status_line.find(" ");
        std::string version;
        if(deliminator != std::string::npos)
        {
            version = status_line.substr(0, deliminator);
            status_line.erase(0, version.length() + 1);     
        }
        std::cout << "Version: " << version << std::endl;

        deliminator = status_line.find(" ");
        std::string status;
        std::string explaination;

        if(deliminator != std::string::npos)
        {
            status = status_line.substr(0, deliminator);
            status_line.erase(0, status.length() + 1);
            explaination = status_line;
        }
        std::cout << "Status: " << status << std::endl;

        std::cout << "Explaination: " << explaination << std::endl;

        std::string header_line;

        while((deliminator = full_response.find("\r\n")) != std::string::npos)
        {
            header_line = full_response.substr(0, deliminator);

            size_t header_deliminator = header_line.find(":");
            std::string header;
            std::string value;

            if(header_deliminator != std::string::npos)
            {
                header = header_line.substr(0, header_deliminator);
                std::transform(header.begin(), header.end(), header.begin(), ::tolower);

                value = header_line.erase(0, header.length() + 2);
                std::transform(value.begin(), value.end(), value.begin(), ::tolower);

                current_uri.headers.insert({header, value});
            }

            full_response.erase(0, deliminator + 2);
        }

        for(const auto& element : current_uri.headers)
        {
            std::cout << element.first << " -> " << element.second << std::endl;
        }
    }

    close(sock);
    freeaddrinfo(result);

    return 0;
}