﻿/*
    WebCrawler project, Krzysztof Łuczka 2024, MIT license
*/
#include <iostream>
#include <windows.h>
#include <wininet.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <regex>
#include <string>
#include <map>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <chrono>
// idk why but it does not work without it
#pragma comment(lib, "wininet.lib")
#include "dictionaries.h"

// damn
typedef std::vector<std::pair<std::string, uint_fast64_t>> keywords;

/*
    Function that downloads webpage content
*/
std::string getSite( const std::string& url ) {
    // session opening
    HINTERNET hInternet = InternetOpenA( "Mozilla/5.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0 );
    if ( !hInternet ) {
        std::cerr << "The session wasn't started correctly." << std::endl;
        return "";
    }

    // opening given URL in a session
    HINTERNET hUrl = InternetOpenUrlA( hInternet, url.c_str(), "Accept: text/html", 0, INTERNET_FLAG_RELOAD, 0 );
    if ( !hUrl ) {
        std::cerr << "Invalid URL." << std::endl;
        InternetCloseHandle( hInternet );
        return "";
    }

    // the content
    std::string content("");
    // the buffer
    constexpr DWORD bufferSize( 1024 );
    char buffer[bufferSize];
    DWORD bytesRead( 0 );

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
    Function that validates the URL
*/
inline bool validateURL( const std::string& url ) {
    std::regex urlRegex( "^(http|https)://[a-zA-Z0-9-.]+\\.[a-zA-Z]{2,}(?:/[^\\s]*)?$" );
    return std::regex_match(url, urlRegex);
}

/*
    Function that extracts:
        * links
        * title
        * language
    from a HTML document
*/
void extractInfo( const std::string& url, std::string& content, std::vector<std::string*>& links, std::string& title, std::string& language, bool extract_links = true ) {
    // removing diactric characters
    content = remove_diactric( content );
    
    // regexes
    std::regex titleRegex( "<title>(.*?)</title>" );
    std::regex langRegex( "<html(?:[^>]*\\s+)?(?:lang|xml:lang)[\\s]*=[\\s]*['\"]([^'\"]*)['\"]" );
    std::smatch match;
    
    // searching for the title
    if ( std::regex_search( content, match, titleRegex ) )
        title = match[1].str();
    else title = "";

    // removing title's punctuation marks
    title.erase( std::remove_if( title.begin(), title.end(), []( char c ) {
        return std::ispunct( static_cast<unsigned char>(c) );
    } ), title.end() );

    // changing to lowercase
    std::transform( title.begin(), title.end(), title.begin(), ::tolower );

    // searching for the language
    if ( std::regex_search( content, match, langRegex ) )
        language = match[1].str().empty() ? match[2].str() : match[1].str();
    else language = "en";

    // searching for the link
    if ( extract_links ) {
        std::regex linkRegex( "<a\\s+(?:[^>]*?\\s+)?href[\\s]*=[\\s]*['\"]([^'\"]*?)['\"][^>]*?>" );
        auto text( content ); // it doesn't work without this lol
        std::string* link;
        while ( std::regex_search( text, match, linkRegex ) ) {
            link = new std::string( match[1].str() ); // extracting

            // validating the url and correcting errors
            if ( link->substr( 0, 7 ) != "http://" && link->substr( 0, 8 ) != "https://" ) {
                if ( link->substr( 0, 2 ) == "./" ) {
                    *link = url + link->substr( 2 );
                } else if ( link->find( "/" ) == 0 ) {
                    *link = url + link->substr( 1 );
                } else if ( link->find( "www." ) == std::string::npos ) {
                    *link = "http://www." + *link;
                } else {
                    *link = "http://" + *link;
                }
            }

            // validating the URL
            if ( validateURL( *link ) )
                links.push_back( link );

            // going further in the webpage
            text = match.suffix();
        }
    }
}

/*
    Function that returns the most occuring words in a string
*/
keywords getKeywords( const std::string& text, std::string language, uint_fast64_t top ) {
    std::map<std::string, uint_fast64_t> wordCount;
    std::istringstream iss( text );
    std::string word;

    // iterating through the whole content
    while ( iss >> word ) {
        // removing all punctuation marks
        word.erase( std::remove_if( word.begin(), word.end(), []( char c ) {
            return std::ispunct( static_cast<unsigned char>(c) );
        }), word.end() );

        // changing to lowercase
        std::transform( word.begin(), word.end(), word.begin(), ::tolower );

        // checking if document's language has the dictionary prepared
        if ( dictionary.find( language ) != dictionary.end() ) {
            const std::vector<std::string>& ignoredWords = dictionary.at( language );
            // if the word is in the dictionary, ignore it and move to the next
            if ( std::find( ignoredWords.begin(), ignoredWords.end(), word ) != ignoredWords.end() ) {
                continue;
            }
        }

        if ( word != "" )
            wordCount[word]++;
    }

    // keywording the keywords
    keywords words;
    for ( const auto& pair : wordCount ) {
        words.push_back( pair );
    }

    // hate to use this way of sorting but it really is better than by hand
    std::sort( words.begin(), words.end(), []( const auto& a, const auto& b ) {
        return a.second > b.second;
    });

    // preventing the potential going out of scope
    if ( top > words.size() )
        top = words.size();

    // returning sorted array of keywords
    return keywords( words.begin(), words.begin() + top );
}

/*
    Function that extracts raw content from HTML
    To rewrite!
*/
std::string extractContent( const std::string& content ) {
    std::regex tagsRegex( "<(?:script|style|noscript)[^>]*>[\\s\\S]*?<\\/\\s*(?:script|style|noscript)>" );
    std::regex htmlRegex( "<[^>]*>" );
    std::regex specialRegex( "&[a-zA-Z0-9#]+;" );

    // removing <script>, <style> and <noscript>
    std::string result( std::regex_replace( content, tagsRegex, " " ) );

    // removing other tags
    result = std::regex_replace( result, htmlRegex, " " );

    // removing special characters
    result = std::regex_replace( result, specialRegex, " " );

    return result;
}

/*
    Function that crawls through given URL
*/
uint_fast64_t crawl( std::string url, uint_fast64_t depth, std::vector<std::string*>& all_time_links, std::fstream& index ) {
    // downloading site's content
    std::string content( getSite( url ) );
    
    // extracting info about the document
    std::vector<std::string*> links;
    std::string language;
    std::string title;
    uint_fast64_t amount( 0 );
    extractInfo( url, content, links, title, language, (depth > 1) );

    // deHTMLing
    content = extractContent( content );

    // extracting keywords
    keywords topWords( getKeywords( content, language, 5 ) );

    // saving indexed site's data
    std::string record( url + " " + title + " " );
    for ( const auto& pair : topWords ) {
        record += pair.first + " ";
    }
    index << record << "\n";

    // debugging
    std::cout << url << "\n";

    --depth;
    // crawl my spiders!
    if ( depth > 0 ) {
        for ( std::string* l : links ) {
            // searching whether link was already indexed
            auto it = std::find_if( all_time_links.begin(), all_time_links.end(),
                [l](std::string* ptr) {
                    return *ptr == *l;
                });

            // if it wasn't
            if ( it == all_time_links.end() ) {
                std::string* new_link( new std::string );
                *new_link = *l;
                all_time_links.push_back( new_link );
                crawl( *l, depth, all_time_links, index );
            }
        }
        amount = links.size();
    } else amount = 1;

    // yes yes, remember to delete your pointers, kids!
    for ( std::string* l : links )
        delete l;

    return amount;
}

int main() {
    std::string url;
    uint_fast64_t depth;
    std::vector<std::string*> searched_links;
    std::fstream index( "index.txt", std::ios::in | std::ios::out | std::ios::app );

    if ( !index.is_open() ) {
        std::cerr << "File couldn't be opened nor created." << std::endl;
        return 1;
    }

    std::cout << "The starting URL >> ";
    std::cin >> url;
    std::cout << "Depth >> ";
    std::cin >> depth;

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    uint_fast64_t links_amount = crawl( url, depth, searched_links, index );
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

    std::cout << "Elapsed time >> " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]\n";
    std::cout << "Total links indexed >> " << links_amount << "\n";
    
    for ( std::string* l : searched_links )
        delete l;
    searched_links.clear();

    index.close();
}

/*
    Results of the web crawling are saved in a 'index.txt' file
    with this structure of every site (every line)

        URL keyword1 keyword2 ... score1 score2 ... date_of_indexing

    Where first keywords are document's title, and their score is
    set to 1000. Next keywords are those estimated while indexing,
    and their scores are calculated by this formula

        Sk = 1000 * Ok / (O1 + O2 + O3 + ... + On)

        n - amount of keywords (without the title)
        Sk - integer score of k-th keyword
        Ok - number of occurences in the document of k-th keyword

    The main 5 most occuring words on a page are set to be the
    keywords.
*/