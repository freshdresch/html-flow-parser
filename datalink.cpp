/*
 * Notice: this file is a modified version of datalink.cpp from the true tcpflow.
 * I wanted to simplify it for my moderately narrow purposes.
 * The original license may be found below. -Adam Drescher
 * ---------------------------------------------------------------------
 * This file is part of tcpflow by Jeremy Elson <jelson@circlemud.org>
 * Initial Release: 7 April 1999.
 *
 * This source code is under the GNU Public License (GPL).  See
 * LICENSE for details.
 *
 */

#include "tcpflow.h"

/* The DLT_NULL packet header is 4 bytes long. It contains a network
 * order 32 bit integer that specifies the family, e.g. AF_INET.
 * DLT_NULL is used by the localhost interface. */
#define	NULL_HDRLEN 4

/* Some systems hasn't defined ETHERTYPE_IPV6 */
#ifndef ETHERTYPE_IPV6
# define ETHERTYPE_IPV6 0x86DD
#endif

void dl_null(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
    tcpdemux &demux = *(tcpdemux *)user;
    u_int caplen = h->caplen;
    uint32_t family = *(uint32_t *)p;;

    if (caplen < NULL_HDRLEN) return;

    /* One of the symptoms of a broken DLT_NULL is that this value is
     * not set correctly, so we don't check for it -- instead, just
     * assume everything is IP.  --JE 20 April 1999
     */
#ifndef DLT_NULL_BROKEN
    /* make sure this is AF_INET */
    //memcpy((char *)&family, (char *)p, sizeof(family));
    //family = ntohl(family);
    if (family != AF_INET && family != AF_INET6) return;
#endif

    demux.process_ip(&h->ts,p + NULL_HDRLEN, caplen - NULL_HDRLEN,flow::NO_VLAN);
}


/* Ethernet datalink handler; used by all 10 and 100 mbit/sec
 * ethernet.  We are given the entire ethernet header so we check to
 * make sure it's marked as being IP. */
void dl_ethernet(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
    tcpdemux &demux = *(tcpdemux *)user;
    u_int caplen = h->caplen;
    struct ether_header *eth_header = (struct ether_header *) p;

    /* Variables to support VLAN */
    int32_t vlan = flow::NO_VLAN;			       /* default is no vlan */
    const u_short *ether_type = &eth_header->ether_type; /* where the ether type is located */
    const u_char *ether_data = p+sizeof(struct ether_header); /* where the data is located */

    /* Handle basic VLAN packets */
    if (ntohs(*ether_type) == ETHERTYPE_VLAN) {
	vlan = ntohs(*(u_short *)(p+sizeof(struct ether_header)));
	ether_type += 2;			/* skip past VLAN header (note it skips by 2s) */
	ether_data += 4;			/* skip past VLAN header */
	caplen     -= 4;
    }
  
    if (caplen < sizeof(struct ether_header)) return;

    /* At this point we can only handle IP datagrams, nothing else */
    if (ntohs(*ether_type) == ETHERTYPE_IP || ntohs(*ether_type)==ETHERTYPE_IPV6) {
	demux.process_ip(&h->ts,ether_data, caplen - sizeof(struct ether_header),vlan);
	return;
    }
}

#if 0
u_int16_t ethtype = ntohs(eth_header->ether_type);
if (ethtype != ETHERTYPE_IP && ethtype != ETHERTYPE_IPV6) return;
/* Handle basic Ethernet packets */
if (ntohs(*ether_type) == ETHERTYPE_IP) {
 }
#endif  


/* The DLT_PPP packet header is 4 bytes long.  We just move past it
 * without parsing it.  It is used for PPP on some OSs (DLT_RAW is
 * used by others; see below) */
#define	PPP_HDRLEN 4

void dl_ppp(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
    tcpdemux &demux = *(tcpdemux *)user;
    u_int caplen = h->caplen;

    if (caplen < PPP_HDRLEN) return;

    demux.process_ip(&h->ts,p + PPP_HDRLEN, caplen - PPP_HDRLEN,flow::NO_VLAN);
}


/* DLT_RAW: just a raw IP packet, no encapsulation or link-layer
 * headers.  Used for PPP connections under some OSs including Linux
 * and IRIX. */
void dl_raw(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
    tcpdemux &demux = *(tcpdemux *)user;
    u_int caplen = h->caplen;

    demux.process_ip(&h->ts,p, caplen,flow::NO_VLAN);
}

#define SLL_HDR_LEN       16

void dl_linux_sll(u_char *user, const struct pcap_pkthdr *h, const u_char *p){
    tcpdemux &demux = *(tcpdemux *)user;
    u_int caplen = h->caplen;

    if (caplen < SLL_HDR_LEN) return;
  
    demux.process_ip(&h->ts,p + SLL_HDR_LEN, caplen - SLL_HDR_LEN,flow::NO_VLAN);
}


pcap_handler find_handler(int datalink_type, const char *device)
{
    int i;

    struct {
	pcap_handler handler;
	int type;
    } handlers[] = {
	{ dl_null,	DLT_NULL },
#ifdef DLT_RAW /* older versions of libpcap do not have DLT_RAW */
	{ dl_raw,	DLT_RAW },
#endif
	{ dl_ethernet,	DLT_EN10MB },
	{ dl_ethernet,	DLT_IEEE802 },
	{ dl_ppp,	DLT_PPP },
#ifdef DLT_LINUX_SLL
	{ dl_linux_sll, DLT_LINUX_SLL },
#endif
	{ NULL, 0 },
    };

    for (i = 0; handlers[i].handler != NULL; i++)
	if (handlers[i].type == datalink_type)
	    return handlers[i].handler;

    die("sorry - unknown datalink type %d on interface %s", datalink_type,
	device);
    /* NOTREACHED */
    return NULL;
}

