
/** 
 *  Copyright 2012 Washington University in St Louis
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *  http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "pna.h"
#include "HTMLparser.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <list>
#include <map>

#include <cstdlib>
#include <cstring>
#include <zlib.h>

// C++11 includes
#include <forward_list>
#include <memory>


using namespace std;

template<typename T>
void printContainer(const T& container) 
{
    typename T::const_iterator it;
    for (it = container.begin(); it != container.end(); ++it) 
        cout << *it << endl;
    cout << endl;
}

void inspectLinks(const forward_list<string>& page)
{
    forward_list<string>::const_iterator it;

    //collect all of the links in a page
    list<string> local_links;
    list<string> remote_links;
    list<pair<string, unsigned int> > domains;
    list<string>::iterator itr;
    
    string link;
    size_t offset;
    size_t span;

    bool remote = false;

    // TODO: think about better ways to distinguish between an in-domain link and internet link
    // besides that internet starts with "http://" and in-domain starts with "/"
    // if it has an href, parse out the link
    for (it = page.begin(); it != page.end(); ++it) {
        offset = it->find("href=\""); 
        
        // TODO: change this to the ability to catch  multiple hrefs per line
        // TODO: consider regular expressions, but not worthwhile during the 10 weeks
        // necessary for the gameforum case where it has all of the html on one line
        if (offset != string::npos) {
            // find the ending quote for the link, and push the url int
            // cout << *it << endl;
            //links.push_back(it->substr(offset+6, span));
            span = it->substr(offset+6).find("\"");
            link = it->substr(offset+6,span);
            
            for (int i = 0; i < NUM_PROTOS; ++i) {
                if (strncmp(link.c_str(),protocols[i],
                            strlen(protocols[i])) == 0) {
                    remote = true;
                    break;
                } 
            }

            if (remote) {
                remote_links.push_back(link);
            } else {
                // ignore in-page references
                if (strncmp(link.c_str(),"#",1) != 0)
                    local_links.push_back(link);
            }
            
            remote = false;
        }
    }

    pair<string, unsigned int> temp;
    string last = "";

    for (itr = remote_links.begin(); itr != remote_links.end(); ++itr) {
        offset = itr->find("//");

        if (offset != string::npos) {
            link = itr->substr(offset+2);
            
            if (link.substr(0,4) == "www.") 
                link = link.substr(4);
            else if (link.substr(0,3) == "www" && link[4] == '.') 
                link = link.substr(5);
            // else, it doesn't have a prefix, leave it alone
            
            offset = link.find("/");
            if (offset != string::npos) 
                link = link.substr(0,offset);
            
            temp = make_pair(link,1);
            
            // don't worry about the hostname
            if (link != host) {
                if (last != link)
                    domains.push_back(temp);
                else 
                    domains.back().second++;
            }
            
            last = link;
        }
    }

    cout << "Remote Links:" << endl;
    printContainer(remote_links);
    // cout << endl << "Local Links:" << endl;
    // printContainer(local_links);

    // TODO: the value 1 should be a user-configured threshold
    // TODO: subdomain issues
    // also, whitelist? .edu, google.com domain
    list<pair<string, unsigned int> >::iterator dom_itr = domains.begin();
    for ( ; dom_itr != domains.end(); ++dom_itr) {
        if (dom_itr->second > 1) {
            cout << "WARNING: " << dom_itr->first << " is a suspicious URL." << endl;
            cout << "It was sequentially linked " << dom_itr->second << 
                " times in close proximity." << endl;
            cout << "Please check this location." << endl << endl;
        }
    }

    
    cout << endl << "----------Numbers-----------" << endl;
    cout << "Remote Links: " << remote_links.size() << endl;
    cout << "Local Links: " << local_links.size() << endl << endl << endl;
}

void inspectKeywords(const forward_list<string>& page)
{
    // TODO: make sure none of the keywords we're using source code keywords
    // form appears to be so, especially for what looks to be BBcode forums
    // TODO: allow the keyword test to be configured
    
    // regular expressions would probably be really helpful for this
    forward_list<string>::const_iterator it;
    int frequency[NUM_CATEGORIES][NUM_KEYWORDS];
    string link;
    size_t offset;

    // set the inital frequencies to 0
    memset(frequency, 0, sizeof(int) * NUM_CATEGORIES * NUM_KEYWORDS);


    for (it = page.begin(); it != page.end(); ++it) {
        link = *it;
        
        for (int i = 0; i < NUM_CATEGORIES; ++i) {
            if ( ((keyfield >> i) & 0x01) == 1 ) {
                for (int j = 0; j < NUM_KEYWORDS; ++j) {
                    offset = link.find(all_keywords[i][j]);
                    while (offset != string::npos) {
                        frequency[i][j]++;
                        link = link.substr(offset + strlen(all_keywords[i][j]));
                        offset = link.find(all_keywords[i][j]);
                    }
                }
            }
        }
    }

    bool suspicious = false;
    for (int i = 0; i < NUM_CATEGORIES; ++i) {
        for (int j = 0; j < NUM_KEYWORDS; ++j) {
            if (frequency[i][j] > 1)
                suspicious = true;
        }
    }

    if (!suspicious) {
        return;
    }


    cout << "WARNING: this webpage has suspicious keyword repetition!" << endl;
    cout << "If the following keywords do not reflect the nature of your website, " << 
        "please act immediately." << endl;
    
    for (int i = 0; i < NUM_CATEGORIES; ++i) {
        if ( ((keyfield >> i) & 0x01) == 1 ) {
            for (int j = 0; j < NUM_KEYWORDS; ++j) {
                if (frequency[i][j] > 1) {
                    cout << "The keyword \"" << all_keywords[i][j] << "\" was repeated " <<
                        frequency[i][j] << " times." << endl;
                }
            }
        }
    }

    // printContainer(page);
}

bool parseHTML(char *buf)
{
    forward_list<string> page;
    forward_list<string>::iterator it;
    istringstream iss(buf);
    string str;


    // Put each line in the forward iterator.
    // I put the buffer in the string stream to have access to getline.
    it = page.before_begin();
    if (iss.good()) {
        while(!iss.eof()) {
            getline(iss,str);
            page.emplace_after(it, str);
        }
    } else {
        return false;
    }
    

    // inspectLinks(page);
    inspectKeywords(page);

    return true;
}

bool decompress(string compression, unique_ptr<char[]> pBuf, size_t buf_len) 
{
    // zlib should work on the gzip and deflate encodings.
    // Need to test deflate though, can't find one that uses it (so maybe I don't need to worry about it).
    if (strncmp(compression.c_str(), "gzip", 4) == 0 || strncmp(compression.c_str(), "deflate", 7) == 0) 
    {
        int err, ret;

        struct gzip_trailer *gz_trailer = (struct gzip_trailer *)(pBuf.get() + buf_len - GZIP_TRAILER_LEN);
        uLongf uncomp_len = (uLongf)gz_trailer->uncomp_len;
        unique_ptr<Bytef[]> pUncomp(new Bytef[uncomp_len]);

        // Set up the z_stream.
        z_stream d_stream;
        d_stream.zalloc = Z_NULL;
        d_stream.zfree = Z_NULL;
        d_stream.opaque = (voidpf)0;

        d_stream.next_in = (Bytef *)pBuf.get();
        d_stream.avail_in = (uInt)buf_len;

        // Prep for inflate.
        err = inflateInit2(&d_stream, 47);
        if (err != Z_OK) 
            cout << "inflateInit Error: " << err << endl;

        // Inflate so that it decompresses the whole buffer.
        d_stream.next_out = pUncomp.get();
        d_stream.avail_out = uncomp_len;
        err = inflate(&d_stream, Z_FINISH);
        if (err < 0)
            cout << "inflate Error: " << err << ": " << d_stream.msg << endl;
        
        // ret = parseHTML((char *)pUncomp.get(), uncomp_len);
        ret = parseHTML((char *)pUncomp.get());
        return ret;
    } 
    
    cout << "Error! not in gzip for decompression" << endl;
    return false;
}

bool parseHTTP(ifstream& in, map<string, string>& header)
{
    map<string, string>::iterator itr;
    string line;
    size_t length; 

    // Initialized return value to false.
    // Let the returns from decompression and parseHTML turn it into true.
    int ret = false;

    // Tally for what combination of Chunked and/or Compressed it is.
    int text_option = 0;

    // Check to see if it is chunked.
    itr = header.find("Transfer-Encoding");
    if (itr != header.end()) 
        text_option += CHUNKED;

    // Check to see if it is compressed.
    itr = header.find("Content-Encoding");
    if (itr != header.end()) 
        text_option += COMPRESSED;

    // the abnormal { } for the cases are to induce explicit scope
    // restrictions, so that the unique_ptr is cleaned immediately.
    switch (text_option) {
    case NONE:
    {
        length = atoi(header["Content-Length"].c_str());
        unique_ptr<char[]> pBuf(new char[length]);
        in.read(pBuf.get(), length);
        //cout.write(buf.get(), length);
        ret = parseHTML((char *)pBuf.get());
        break;
    }
    case CHUNKED:
    {
        // Get the first chunk length.
        getline(in,line);
        length = strtoul(line.c_str(),NULL,16);

        // Repeat reading the buffer and new chunk length until the length is 0.
        // 0 is the value in concordance with the HTTP spec, it signifies the end of the chunks.
        while (length != 0) {
            unique_ptr<char[]> pBuf(new char[length]);
            in.read(pBuf.get(), length);
             
            // Consume the trailing CLRF before the length.
            getline(in,line);

            // Consume the new chunk length or the terminating zero.
            getline(in,line);

            // I have to use strtoul with base 16 because the HTTP spec
            // says the chunked encoding length is presented in hex.
            length = strtoul(line.c_str(),NULL,16);

            //cout.write(buf,length);
            // parseHTML((char *)pBuf.get(), length);
            parseHTML((char *)pBuf.get());
        }
        
        // Once it gets to this point, the chunked length last fed was 0
        // Get last CLRF and quit
        getline(in,line);

        // TODO: Return value for this. Multiple parseHTML calls equates to...?
        ret = true;
        break;
    }
    case COMPRESSED:
    {
        // Read the compressed data into a buffer and ship off to be decompressed.
        length = atoi(header["Content-Length"].c_str());
        unique_ptr<char[]> pBuf(new char[length]);
        in.read(pBuf.get(), length);
        ret = decompress(header["Content-Encoding"], std::move(pBuf), length);
        break;
    }
    case BOTH:
    {
        size_t totalLength = 0;
        
        // Get the first chunk length.
        getline(in,line);
        length = strtoul(line.c_str(),NULL,16);

        // Repeat reading the buffer and new chunk length until the length is 0.
        // 0 is the value in concordance with the HTTP spec, it signifies the end of the chunks.
        ostringstream oss;
        while (length != 0) {
            totalLength += length;

            char *chunk = new char[length];
            // unique_ptr<char[]> pChunk(new char[length]);
            in.read(chunk, length);
            // oss << chunk;
            oss.write(chunk, length);

            // Consume the trailing CLRF before the length.
            getline(in,line);

            // Consume the new chunk length or the terminating zero.
            getline(in,line);

            // I have to use strtoul with base 16 because the HTTP spec
            // says the chunked encoding length is presented in hex.
            length = strtoul(line.c_str(),NULL,16);

            delete [] chunk;
        }

        // Once it gets to this point, the chunked length last fed was 0
        // Get last CLRF and quit
        getline(in,line);

        if (oss.str().size() != totalLength) {
            cout << "Error! stringstream length" << endl;
            return false;
        }

        unique_ptr<char[]> pBuf(new char[totalLength + 1]);
        memcpy(pBuf.get(), oss.str().c_str(), totalLength + 1);
        
        ret = decompress(header["Content-Encoding"], std::move(pBuf), totalLength);
        break;
    }
    default:
        break;
    }

    return ret;
}

void getResponse(char *flows[])
{
    map<string, string> header;
    ifstream in;

    string line; 
    size_t offset;
    size_t host_off;


    // const char *flows[] = {flowOne, flowTwo}; 
    int outbound;
    int inbound;
    char portOne[5], portTwo[5];
    const int tcp_port = 80;

    /*
     * Check the destination port of the first file.
     * If it is 80, then it's the outbound file.
     * If not, then it's the inbound file.
     * Set the file's argument number accordingly.
     */
    strncpy(portOne, flows[0] + strlen(flows[0]) - 5, 5);
    strncpy(portTwo, flows[1] + strlen(flows[1]) - 5, 5);

    if (atoi(portOne) == tcp_port) {
        outbound = 0;
        inbound = 1;
    } else if (atoi(portTwo) == tcp_port) {
        inbound = 0;
        outbound = 1;
    } else {
        // not communication between the server and the client
        // quit
        cout << "No communication to the website's server. Exiting..." << endl;
        exit(0);
    }

    // Get the hostname
    in.open(flows[outbound]);
    if (in.is_open()) {
        while (!in.eof()) {
            // search for a GET request that specifies the host
            // should be the first line, so quit after we find it
            getline(in, line);

            host_off = line.find("Host: ");
            if (host_off != string::npos) {
                host = line.substr(host_off + 6);

                host_off = host.find("\r");
                if (host_off != string::npos) 
                    host = host.substr(0,host_off);
                
                break;
            }
        }
    }
    in.close();

    // Let's parse some replies!
    in.open(flows[inbound]);
    if (in.is_open()) {
        while (!in.eof()) {
            // Search for the "HTTP/1.1 200 OK" line.
            getline(in, line);

            offset = line.find("HTTP/1.1 200 OK");
            if (offset != string::npos) { 
                // We found the reply.
                header["HTTP/1.1"] = "200 OK";

                // Read the HTTP header labels and values into our map.
                // The header ending is denoted by a carriage return '\r'.
                getline(in, line);
                while (line != "\r") {
                    offset = line.find(":");
                    header[line.substr(0, offset)] = line.substr(offset + 2);
                    getline(in, line);
                }

                // Print the http header
                // map<string, string>::iterator it;
                // for (it = header.begin(); it != header.end(); ++it)
                //     cout << it->first << ": " << it->second << endl;
                // cout << endl;

                // Send the HTTP header and our ifstream to parseHTTP.
                parseHTTP(in, header);

                // Clear the header map for the next reply found in the file.
                header.clear();
            }
        }
    } else  {
        cerr << "Error! File did not open correctly." << endl;
    }

    in.close();
}

void printUsage()
{
    cout << "Usage: HTMLparser <options> outbound-flow inbound-flow" << endl;
    cout << "options:" << endl;
    cout << "  -c : perform keyword search, specify the target categories." << endl;
    cout << "\tCategories:" << endl;
    cout << "\tf : fitness-themed keywords.        Example: antioxidant" << endl;
    cout << "\tg : gambling-themed keywords.       Example: blackjack" << endl;
    cout << "\th : house-themed keywords.          Example: mortgage" << endl;
    cout << "\tl : login-themed keywords.          Example: password" << endl;
    cout << "\tm : medication-themed keywords.     Example: viagra" << endl;
    cout << "\to : order-themed keywords.          Example: discount" << endl;
    cout << "\tp : pornographic-themed keywords.   Example: busty" << endl;
    cout << "\ts : server-themed keywords.         Example: proxy" << endl;
    cout << "\ta : all categories." << endl;
    cout << "  -h : display help" << endl;
}

int main(int argc, char **argv) 
{
    char *cval = NULL;
    char temp;
    int c;

    while ((c = getopt (argc, argv, "c:h ")) != -1) {
        switch (c) {
        case 'c':
            cval = optarg;
            // fghlmops a
            for (unsigned int i = 0; i < strlen(cval); ++i) {
                temp = cval[i];
                switch (temp) {
                case 'a':
                    keyfield |= ALL;
                    break;
                case 'f':
                    keyfield |= FITNESS;
                    break;
                case 'g':
                    keyfield |= GAMBLING;
                    break;
                case 'h':
                    keyfield |= HOMEOWNER;
                    break;
                case 'l':
                    keyfield |= SITE;
                    break;
                case 'm':
                    keyfield |= MEDS;
                    break;
                case 'o':
                    keyfield |= SHOPPING;
                    break;
                case 'p':
                    keyfield |= PORN;
                    break;
                case 's':
                    keyfield |= SERVER;
                    break;
                default:
                    break;
                }
            }
            break;
        case 'h':
            printUsage();
            return EXIT_SUCCESS;
            break;
        case '?':
            if (optopt == 'c')
                cerr << "Option -" << optopt << " requres an argument." << endl;
            else if (isprint(optopt))
                cerr << "Unknown option -" << optopt << "." << endl;
            else
                cerr << "Unknown option character " << optopt << "." << endl;
            return EXIT_FAILURE;
        default:
            return EXIT_FAILURE;
        }
    }

    if (argc < optind + 2) {
        printUsage();
        return EXIT_FAILURE;
    }


    char *flows[2];
        
    if (argc == optind + 2) {
        flows[0] = argv[optind];
        flows[1] = argv[optind+1];
    } else {
        cerr << "Too many args!" << endl;
        printUsage();
        return EXIT_FAILURE;
    }

    getResponse(flows);

    return 0;
}


// Input is piped to our program from stdout with our pcap_wrap format.
// else if (argc == 1)
// {
//     map<string, string> key;
//     string first, second;
//     unsigned long length;
//     size_t span;

//     while(!cin.eof())
//     {
//         // Get the six lines pertinent to our TCP flows, put them in our key map.
//         for (int i = 0; i < 6; ++i)
//         {
//             getline(cin, line);
                
//             offset = line.find("->");
//             span = line.find(":"); 

//             if (offset != string::npos && span != string::npos)
//             {
//                 second = line.substr(span+2);
//                 first = line.substr(offset+2, span - offset - 2);
//                 key[first] = second; 
//             }
//         }

//         // Print the key.
//         map<string, string>::iterator it;
//         for (it = key.begin(); it != key.end(); ++it)
//             cout << it->first << ": " << it->second << endl;

//         // Get the packet information. This line specifically is packet length.
//         getline(cin, line);
//         offset = line.find(":");
//         if (offset != string::npos)
//         {
//             // The length line exists.  Read that length of data from cin soon.
//             length = atoi(line.substr(offset+2).c_str());
//             unique_ptr<char[]> pBuf(new char[length]);

//             // Consume the packet_data label.
//             getline(cin, line);
                
//             // Get the packet data.
//             cin.read(pBuf.get(), length);

//             // TODO: Here is the exit point from main.
//             // Guessing this will go to the hashmap? We'll see when it's developed.

//             cout.write(pBuf.get(), length);
//         }

//         // Clear the key map for other packet keys.
//         key.clear();
//     }
// } 

