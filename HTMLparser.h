#ifndef HTMLPARSER_H
#define HTMLPARSER_H


#include <stdint.h>
#include <string>

// Trailer for gzip compression
struct gzip_trailer {
    uint32_t crc32;          // Cyclic Redundancy Check value
    uint32_t uncomp_len;     // Uncompressed length of original input data
};
#define GZIP_TRAILER_LEN 8

// HTTP text options
#define NONE       0
#define COMPRESSED 1
#define CHUNKED    2
#define BOTH       3

// Protocols to look for
#define NUM_PROTOS 3
const char *protocols[NUM_PROTOS] = {"http://", "https://", "ftp://"};

/**
 * Keywords to look for, gathered from Bannur, Saul, and Savage's
 * "Judging a site by its content: learning the textual, structural,
 * and visual features of malicious Web pages"
 */
#define NUM_CATEGORIES 8
#define NUM_KEYWORDS   10

const char *meds[NUM_KEYWORDS] = {"hormone", "sildenafil", "medication", "cure",
                        "sexual", "impotence", "levitra", "erection",
                        "cialis", "viagra"};

const char *site[NUM_KEYWORDS] = {"sign", "form", "case", "password", "lock",
                        "caps", "sensitive", "username", "log", "forgot"};

const char *shopping[NUM_KEYWORDS] = {"shipping", "shopping", "code", "discount",
                            "cart", "refund", "special", "checkout",
                            "prices", "clearance"};

const char *porn[NUM_KEYWORDS] = {"lesbian", "slut", "horny", "busty", "blowjob", 
                        "tits", "pussy", "cock", "anal", "ebony"};

const char *homeowner[NUM_KEYWORDS] = {"mortgage", "financing", "debt", "wealth",
                             "bank", "mortgages", "applicants", "homeowner",
                             "freedom", "interest"};

const char *server[NUM_KEYWORDS] = {"error", "timed", "temporarily", "unavailable",
                          "server", "problem", "moments", "busy", "proxy",
                          "promotions"};

const char *gambling[NUM_KEYWORDS] = {"craps", "blackjack", "slots", "roulette",
                            "poker", "casino", "promotions", "games",
                            "video", "seven"};

const char *fitness[NUM_KEYWORDS] = {"slim", "cambogia", "polynicotinate", "powerslim", 
                           "extract", "garcinia", "exercising", "berry",
                           "antioxidant", "dieting"};

const char **all_keywords[NUM_CATEGORIES] = {meds, site, shopping, porn, homeowner, 
                                server, gambling, fitness};


// Either make the host global or make it an argument through 
// 3 functions where it's only needed in the last one.
// Shitty, I know...
std::string host;

#endif 

