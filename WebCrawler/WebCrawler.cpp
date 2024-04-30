﻿#include <iostream>
#include <windows.h>
#include <wininet.h>
#include <fstream>
#include <vector>
#include <regex>
#include <string>
// idk why but it does not work without it
#pragma comment(lib, "wininet.lib")


/*
    Function that downloads webpage content
*/
std::string getWebPageContent( const std::string& url ) {
    // session opening
    HINTERNET hInternet = InternetOpenA( "Mozilla/5.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0 );
    if ( !hInternet ) {
        std::cerr << "The session wasn't started correctly." << std::endl;
        return "";
    }

    // opening given URL in a session
    HINTERNET hUrl = InternetOpenUrlA( hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0 );
    if ( !hUrl ) {
        std::cerr << "Invalid URL." << std::endl;
        InternetCloseHandle( hInternet );
        return "";
    }

    // the content
    std::string content;
    // the buffer
    constexpr DWORD bufferSize = 1024;
    char buffer[bufferSize];
    DWORD bytesRead = 0;

    // reading (idk if it is truly optimized)
    while ( InternetReadFile( hUrl, buffer, bufferSize - 1, &bytesRead ) && bytesRead > 0 ) {
        buffer[bytesRead] = '\0'; // null terminator
        content.append( buffer, bytesRead );
    }

    // don't forget this!
    InternetCloseHandle( hUrl );
    InternetCloseHandle( hInternet );

    return content;
}

/*
    Function that extracts every link from a given HTML page
*/
std::vector<std::string> extractLinks( const std::string& htmlContent, const std::string& baseURL ) {
    // regex to find the <a></a>
    std::regex linkRegex( "<a\\s+(?:[^>]*?\\s+)?href[\\s]*=[\\s]*['\"]([^'\"]*?)['\"][^>]*?>" );

    // links vector
    std::vector<std::string> links;

    // searching for the link
    std::smatch match;
    auto text = htmlContent;
    while ( std::regex_search( text, match, linkRegex ) ) {
        std::string link = match[1].str(); // extracting

        // validating the url and correcting errors
        if ( link.substr( 0, 7 ) != "http://" && link.substr( 0, 8 ) != "https://" ) {
            if ( link.substr( 0, 2 ) == "./" ) {
                link = baseURL + link.substr( 2 );
            } else if ( link.find( "/" ) == 0 ) {
                link = baseURL + link.substr( 1 );
            } else if ( link.find( "www." ) == std::string::npos ) {
                link = "http://www." + link;
            } else {
                link = "http://" + link;
            }
        }

        links.push_back( link );

        // going further in the webpage
        text = match.suffix();
    }

    return links;
}



int main() {
    std::string url;
    uint_fast64_t depth;
    std::cout << "The starting URL >> ";
    std::cin >> url;
    //std::cout << "Depth >> ";
    //std::cin >> depth;

    std::string content = getWebPageContent( url );

    std::vector<std::string> extractedLinks = extractLinks( content, url );
    for ( const auto& link : extractedLinks ) {
        std::cout << link << std::endl;
    }

    /*if ( !content.empty() ) {
        std::ofstream outputFile( "webpage_content.html" );
        outputFile << content;
        outputFile.close();
        std::cout << "Website saved as webpage_content.html" << std::endl;
    } else {
        std::cout << "Error." << std::endl;
    }*/
}