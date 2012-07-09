
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

#include <iostream>
#include <sstream>
#include <fstream>
#include <list>
#include <map>
#include <stdint.h>
#include <cstdlib>
#include <string.h>
#include <string>
#include <zlib.h>

// C++11 includes
#include <forward_list>
#include <memory>


using namespace std;


// Trailer for gzip compression
struct gzip_trailer 
{
    uint32_t crc32;          // Cyclic Redundancy Check value
    uint32_t uncomp_len;     // Uncompressed length of original input data
};
#define GZIP_TRAILER_LEN 8

// Parser returns
#define PARSE_SUCCESS 1
#define PARSE_FAILURE 0

// HTTP text options
#define NONE       0
#define COMPRESSED 1
#define CHUNKED    2
#define BOTH       3

// Either make the host global or make it an argument through 
// 3 functions where it's only needed in the last one.
// Shitty, I know...
string host;

int parseHTML(char *buf, size_t buf_len)
{
    forward_list<string> page;
    forward_list<string>::iterator it;
    istringstream iss(buf);
    string str;
    // size_t offset;

    // Put each line in the forward iterator.
    // I put the buffer in the string stream to have access to getline.
    it = page.before_begin();
    if (iss.good())
    {
         while(!iss.eof()) 
        {
            getline(iss,str);
            page.emplace_after(it, str);
        }
        
    }
    else
        return PARSE_FAILURE;
    
    // // Print the list of html lines.
    // for (it = page.begin(); it != page.end(); ++it) 
    //     cout << *it << endl;

    //collect all of the links in a page
    // list<string> links;
    // list<string>::iterator itr;
    // for (it = page.begin(); it != page.end(); ++it) 
    // {
    //     offset = it->find("href=\""); 
    //     if (offset != string::npos)
    //         links.push_back(*it);
    // }

    // for (itr = links.begin(); itr != links.end(); ++it)
    //     cout << *itr << endl;
    // cout << "num links: " << links.size() << endl;
    // check if a lot of the links navigate outside of the website
    

    return PARSE_SUCCESS;
}

int decompress(string compression, unique_ptr<char[]> pBuf, size_t buf_len) 
{
    // zlib should work on the gzip and deflate encodings.
    // Need to test deflate though, can't find one that uses it (so maybe I don't need to worry about it).
    if (strncmp(compression.c_str(), "gzip", 4) == 0 || strncmp(compression.c_str(), "deflate", 7) == 0) 
    {
        struct gzip_trailer *gz_trailer = (struct gzip_trailer *)(pBuf.get() + buf_len - GZIP_TRAILER_LEN);
        int err, ret;

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

        //cout.write((char *)pUncomp.get(), uncomp_len);
        ret = parseHTML((char *)pUncomp.get(), uncomp_len);
        return ret;
    }
    else 
        cout << "Error! not in gzip for decompression" << endl;
    return PARSE_FAILURE;
}

int parseHTTP(ifstream& in, map<string, string>& header)
{
    map<string, string>::iterator itr;
    string line;
    size_t length; 

    // Initialized return value to PARSE_FAILURE.
    // Let the returns from decompression and parseHTML turn it into PARSE_SUCCESS.
    int ret = PARSE_FAILURE;

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

    // Let it be known, the abnormal { } for the cases are to induce explicit
    // scope restrictions, so that the unique_ptr is cleaned immediately.
    switch (text_option) {
    case NONE:
    {
        if (strncmp(itr->second.c_str(), "text/html", 9) == 0) 
        {
            length = atoi(header["Content-Length"].c_str());
            unique_ptr<char[]> pBuf(new char[length]);
            in.read(pBuf.get(), length);
            //cout.write(buf.get(), length);
            ret = parseHTML((char *)pBuf.get(), length);
        } 
        else if (strncmp(itr->second.c_str(), "text/css", 8))
        {
            length = atoi(header["Content-Length"].c_str());
            unique_ptr<char[]> pBuf(new char[length]);
            in.read(pBuf.get(), length);
            //cout.write(buf.get(), length);
            ret = parseHTML((char *)pBuf.get(), length);
        }
        break;
    }
    case CHUNKED:
    {
        // Get the first chunk length.
        getline(in,line);
        length = strtoul(line.c_str(),NULL,16);

        // Repeat reading the buffer and new chunk length until the length is 0.
        // 0 is the value in concordance with the HTTP spec, it signifies the end of the chunks.
        while (length != 0) 
        {
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
            parseHTML((char *)pBuf.get(), length);
        }
        
        // Once it gets to this point, the chunked length last fed was 0
        // Get last CLRF and quit
        getline(in,line);

        // TODO: Return value for this. Multiple parseHTML calls equates to...?
        ret = PARSE_SUCCESS;
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
        // Same as the CHUNKED case but with decompression. Comments included again for verbosity.
        // TODO: figure out way to combine this and CHUNKED to eliminate repetitive code

        // Get the first chunk length.
        getline(in,line);
        length = strtoul(line.c_str(),NULL,16);

        // Repeat reading the buffer and new chunk length until the length is 0.
        // 0 is the value in concordance with the HTTP spec, it signifies the end of the chunks.
        while (length != 0) 
        {
            unique_ptr<char[]> pBuf(new char[length]);
            in.read(pBuf.get(), length);
            decompress(header["Content-Encoding"],std::move(pBuf),length);
            
            // Consume the trailing CLRF before the length.
            getline(in,line);

            // Consume the new chunk length or the terminating zero.
            getline(in,line);

            // I have to use strtoul with base 16 because the HTTP spec
            // says the chunked encoding length is presented in hex.
            length = strtoul(line.c_str(),NULL,16);
        }

        // Once it gets to this point, the chunked length last fed was 0
        // Get last CLRF and quit
        getline(in,line);

        // TODO: Return value for this. Multiple decompress calls equates to...?
        ret = PARSE_SUCCESS;
        break;
    }
    default:
        break;

    }

    return ret;
}

int main(int argc, char **argv) 
{
    string line; 
    string host;
    size_t offset;
    size_t host_off;
    
    // If argc is two, a pcap input file should be given.
    if (argc == 3)
    {
        map<string, string> header;
        ifstream in;
        
        int outbound;
        int inbound;
        char port[5];

        /*
         * Check the destination port of the first file.
         * If it is 80, then it's the outbound file.
         * If not, then it's the inbound file.
         * Set the file's argument number accordingly.
         */
        strncpy(port, argv[1] + strlen(argv[1]) - 5, 5);
        if (atoi(port) == 80) {
            outbound = 1;
            inbound = 2;
        } else {
            inbound = 1;
            outbound = 2;
        }

        // Get the hostname
        in.open(argv[outbound]);
        if (in.is_open()) {
            while (!in.eof()) {
                // search for a GET request that specifies the host
                // should be the first line, so quit after we find it
                getline(in, line);

                host_off = line.find("Host: ");
                    
                if (host_off != string::npos) {
                    host = line.substr(host_off + 6);
                    break;
                }
            }
        }
        in.close();

        cout << "host: " << host << endl;

        // Let's parse some replies!
        in.open(argv[inbound]);
        if (in.is_open()) 
        {
            while (!in.eof()) 
            {
                // Search for the "HTTP/1.1 200 OK" line.
                getline(in, line);

                offset = line.find("HTTP/1.1 200 OK");
                if (offset != string::npos) 
                {
                    // We found the reply.
                    header["HTTP/1.1"] = "200 OK";

                    // Read the HTTP header labels and values into our map.
                    // The header ending is denoted by a carriage return '\r'.
                    getline(in, line);
                    while (line != "\r") 
                    {
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
        } 
        else 
            cout << "Error! File did not open correctly." << endl;
        
        in.close();
    }
    // Input is piped to our program from stdout with our pcap_wrap format.
    else if (argc == 1)
    {
        map<string, string> key;
        string first, second;
        unsigned long length;
        size_t span;

        while(!cin.eof())
        {
            // Get the six lines pertinent to our TCP flows, put them in our key map.
            for (int i = 0; i < 6; ++i)
            {
                getline(cin, line);
                
                offset = line.find("->");
                span = line.find(":"); 

                if (offset != string::npos && span != string::npos)
                {
                    second = line.substr(span+2);
                    first = line.substr(offset+2, span - offset - 2);
                    key[first] = second; 
                }
            }

            // Print the key.
            map<string, string>::iterator it;
            for (it = key.begin(); it != key.end(); ++it)
                cout << it->first << ": " << it->second << endl;

            // Get the packet information. This line specifically is packet length.
            getline(cin, line);
            offset = line.find(":");
            if (offset != string::npos)
            {
                // The length line exists.  Read that length of data from cin soon.
                length = atoi(line.substr(offset+2).c_str());
                unique_ptr<char[]> pBuf(new char[length]);

                // Consume the packet_data label.
                getline(cin, line);
                
                // Get the packet data.
                cin.read(pBuf.get(), length);

                // TODO: Here is the exit point from main.
                // Guessing this will go to the hashmap? We'll see when it's developed.

                cout.write(pBuf.get(), length);
            }

            // Clear the key map for other packet keys.
            key.clear();
        }
    } else {
        cout << "Usage: html_parser outbound-flow inbound-flow" << endl;
        cout << "[NOTE] the flow order is not important" << endl;
    }


    return 0;
}

