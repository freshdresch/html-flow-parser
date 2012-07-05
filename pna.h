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


/* Some generic PNA data */

#ifndef _PNA_H_
# define _PNA_H_

#define PNA_PREFIX        0x80fc0000 /* 128.252.0.0 */
#define PNA_MASK          0xffff0000 /* 255.255.0.0 */
#define PNA_DIR_OUTBOUND  0
#define PNA_DIR_INBOUND   1 

/* useful structures */
struct session_key {
    unsigned short l3_protocol;
    unsigned char l4_protocol;
    unsigned int local_ip;
    unsigned int remote_ip;
    unsigned short local_port;
    unsigned short remote_port;
};

struct packet {
    const unsigned long length;
    const unsigned char *data;
};

#endif /* _PNA_H_ */
