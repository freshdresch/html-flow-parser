/* Notice: this file is a modified version of main.cpp from the true tcpflow.
 * I wanted to simplify it for my moderately narrow purposes.
 * The original license may be found below. -Adam Drescher
 * ---------------------------------------------------------------------
 * This file is part of tcpflow by Jeremy Elson <jelson@circlemud.org>
 * Initial Release: 7 April 1999.
 *
 * This source code is under the GNU Public License (GPL).  See
 * LICENSE for details.
 */

#include "tcpflow.h"

#include <netinet/ip6.h>		/*  SLG */
#include <iostream>
#include <sstream>
#include <utility>

/**
 * Close all of the flows in the fd_ring
 */
void tcpdemux::close_all()
{
    for(tcpset::iterator it = openflows.begin();it!=openflows.end();it++){
	(*it)->close_file();
    }
    openflows.clear();
}


void tcpdemux::close_tcpip(tcpip *i)
{
    i->close_file();
    openflows.erase(i);
}

void tcpdemux::close_oldest()
{
    tcpip *oldest_tcp=0;
    for(tcpset::iterator it = openflows.begin();it!=openflows.end();it++){
	if(oldest_tcp==0 || (*it)->last_packet_time < oldest_tcp->last_packet_time){
	    oldest_tcp = (*it);
	}
    }
    if(oldest_tcp) close_tcpip(oldest_tcp);
}

/* Find previously a previously created flow state in the database.
 */
tcpip *tcpdemux::find_tcpip(const flow_addr &flow)
{
    flow_map_t::const_iterator it = flow_map.find(flow);
    if (it==flow_map.end()){
	return NULL; // flow not found
    }
    return it->second;
}

/* Create a new flow state structure for a given flow.
 * Puts the flow in the map.
 * Returns a pointer to the new state.
 * Do we really want to keep pointers to tcp structures in the map, rather than
 * the structures themselves?
 */

tcpip *tcpdemux::create_tcpip(const flow_addr &flowa, int32_t vlan,tcp_seq isn,
			   const timeval &ts,uint64_t connection_count)
{
    /* create space for the new state */
    flow flow(flowa,vlan,ts,ts,flow_counter++,connection_count);

    tcpip *new_tcpip = new tcpip(*this,flow,isn);
    new_tcpip->last_packet_time = packet_time++;
    flow_map[flow] = new_tcpip;
    return new_tcpip;
}

void tcpdemux::remove_flow(const flow_addr &flow)
{
    flow_map_t::iterator it = flow_map.find(flow);
    if(it!=flow_map.end()){
	close_tcpip(it->second);
	delete it->second;
	flow_map.erase(it);
    }
}

void tcpdemux::flow_map_clear()
{
    for(flow_map_t::iterator it=flow_map.begin();it!=flow_map.end();it++){
	delete it->second;
    }
    flow_map.clear();
}

tcpip::tcpip(tcpdemux &demux_,const flow &flow_,tcp_seq isn_):
    demux(demux_),myflow(flow_),isn(isn_),flow_pathname(),fp(0),pos(0),pos_min(0),pos_max(0),
    last_packet_time(0),bytes_processed(0),finished(0),file_created(false),dir(unknown),
    out_of_order_count(0),md5(0)
{
    // compute the filename
    static const std::string slash("/");
    if(demux.outdir==".")
        flow_pathname = myflow.filename();
    else
        flow_pathname = demux.outdir + slash + myflow.filename();
}


#ifndef O_BINARY
#define O_BINARY 0
#endif
#include <sys/mman.h>

/* This could be much more efficient */
const char *find_crlfcrlf(const char *base,size_t len)
{
    while(len>4){
	if(base[0]=='\r' && base[1]=='\n' && base[2]=='\r' && base[3]=='\n')
	    return base;

	len--;
	base++;
    }
    return 0;
}


/**
 * Destructor is called when flow is closed.
 * It implements "after" processing.
 */
tcpip::~tcpip()
{
    // close the file if it is open for some reason
    if(fp) close_file();	
}


int tcpdemux::open_tcpfile(tcpip *tcp)
{
    /* This shouldn't be called if the file is already open */
    if (tcp->fp) return 0;

    /* Now try and open the file */
    if(tcp->file_created)
	tcp->fp = retrying_fopen(tcp->flow_pathname.c_str(),"r+b");
    else 
	tcp->fp = retrying_fopen(tcp->flow_pathname.c_str(),"wb");

    /* If the file isn't open at this point, there's a problem */
    if (tcp->fp == NULL) {
	/* we had some problem opening the file -- set FINISHED so we
	 * don't keep trying over and over again to reopen it
	 */
	perror(tcp->flow_pathname.c_str());
	return -1;
    }

    openflows.insert(tcp);
    tcp->file_created = true;		// remember we made it
    tcp->pos = ftell(tcp->fp);		// remember where it is
    return 0;
}

/* Closes the file belonging to a flow, but don't take it out of the map.
 */
void tcpip::close_file()
{
    if (fp){
	struct timeval times[2];
	times[0] = myflow.tstart;
	times[1] = myflow.tstart;

	/* close the file and remember that it's closed */
	fflush(fp);		/* flush the file */
	if(futimes(fileno(fp),times)){
	    perror("futimes");
	}
	fclose(fp);
        
        // insert source-destination combo into finished_flows
        std::string source;
        std::string destination;
        size_t offset;

        bool found = false;
        
        // if it can't find the '-' in the filename, something is horridly wrong
        offset = myflow.filename().find("-");
        if (offset == std::string::npos) {
            perror("find");
            exit(EXIT_FAILURE);
        }

        source = myflow.filename().substr(0, offset);
        destination = myflow.filename().substr(offset+1);
        demux.finished_flows.push_back(std::make_pair(source,destination));

        // check and see if it's paired file is also in
        std::pair<std::string, std::string> flow_pair;
        finished_flows_t::iterator itr;

        flow_pair = std::make_pair(destination, source);
        itr = demux.finished_flows.begin();

        for (; itr != demux.finished_flows.end(); ++itr) {
            if (*itr == flow_pair) {
                found = true;
                break;
            }
        }

        if (found) {
            if (fork() == 0) {
                std::string file_pair = demux.finished_flows[itr - demux.finished_flows.begin()].first + 
                    "-" + demux.finished_flows[itr - demux.finished_flows.begin()].second;

                char *args[4];
                // these are casted to get rid of const char* -> char* warnings
                args[0] = (char *)"html_parser";  
                args[1] = (char *)flow_pathname.c_str();
                args[2] = (char *)file_pair.c_str();
                args[3] = NULL;
                
                // TODO: figure out a relative directory scheme
                if (execv("/home/adam/html-flow-parser/html_parser",args) != -1) {
                    // It worked as planned
                } else {
                    perror("execv");
                }
            }
        }

	fp = NULL;
	pos = 0;
    }
}


/*************************************************************************/


/* print the contents of this packet to the console */
void tcpip::print_packet(const u_char *data, uint32_t length)
{
    if(demux.max_bytes_per_flow>0){
	if(bytes_processed > demux.max_bytes_per_flow) return; /* too much has been printed */
	if(length > demux.max_bytes_per_flow - bytes_processed){
	    length = demux.max_bytes_per_flow - bytes_processed; /* can only output this much */
	    if(length==0) return;
	}
    }

#ifdef HAVE_PTHREAD
    if(semlock){
	if(sem_wait(semlock)){
	    fprintf(stderr,"%s: attempt to acquire semaphore failed: %s\n",progname,strerror(errno));
	    exit(1);
	}
    }
#endif

    if(length != fwrite(data, 1, length, stdout)){
	std::cerr << "\nwrite error to fwrite?\n";
    }
    bytes_processed += length;

    putchar('\n');
    fflush(stdout);

#ifdef HAVE_PTHREAD
    if(semlock){
	if(sem_post(semlock)){
	    fprintf(stderr,"%s: attempt to post semaphore failed: %s\n",progname,strerror(errno));
	    exit(1);
	}
    }
#endif
}


/* store the contents of this packet to its place in its file */
void tcpip::store_packet(const u_char *data, uint32_t length, uint32_t seq, int syn_set)
{
    /* If we got a SYN reset the sequence number */
    if (syn_set) 
	isn = seq - pos +1;

    /* if we're done collecting for this flow, return now */
    if (finished) 
        return;



    /* calculate the offset into this flow -- should handle seq num
     * wrapping correctly because tcp_seq is the right size */
    tcp_seq offset = seq - isn;

    /* I want to guard against receiving a packet with a sequence number
     * slightly less than what we consider the ISN to be; the max
     * (though admittedly non-scaled) window of 64K should be enough */
    if (offset >= 0xffff0000) 
        return;


    /* reject this packet if it falls entirely outside of the range of
     * bytes we want to receive for the flow */
    if (demux.max_bytes_per_flow && (offset > demux.max_bytes_per_flow))
	return;

    /* reduce length if it goes beyond the number of bytes per flow */
    if (demux.max_bytes_per_flow && (offset + length > demux.max_bytes_per_flow)) {
	finished = true;
	length = demux.max_bytes_per_flow - offset;
    }

    /* if we don't have a file open for this flow, try to open it.
     * return if the open fails.  Note that we don't have to explicitly
     * save the return value because open_tcpfile() puts the file pointer
     * into the structure for us. */
    if (fp == NULL) {
        if (demux.open_tcpfile(this)) 
            return;
    }
	
    /* if we're not at the correct point in the file, seek there */
    if (offset != pos) {
        fseek(fp, offset, SEEK_SET);
        out_of_order_count++;
    }
	
    /* write the data into the file */
    if (fwrite(data, length, 1, fp) != 1) 
        perror("");
    if (out_of_order_count==0 && md5)
        MD5Update(md5,data,length);

    fflush(fp);

    /* update instance variables */
    if(bytes_processed==0 || pos<pos_min) pos_min = pos;

    bytes_processed += length;		// more bytes have been processed
    pos = offset + length;		// new pos
    if (pos>pos_max) pos_max = pos;	// new max

    if (finished) close_file();
}

/****************************************************************
 *** tcpdemultiplexer 
 ****************************************************************/

/* Try to find the maximum number of FDs this system can have open */
unsigned int tcpdemux::get_max_fds(void)
{
    int max_descs = 0;
    /* complains about this being set but not used
     * however, it being defined in the branches is way better than
     * putting the declaration in each of the branches (to fix
     * the warning). 
     */
    const char *method;

    /* Use OPEN_MAX if it is available */
#if defined (OPEN_MAX)
    method = "OPEN_MAX";
    max_descs = OPEN_MAX;
#elif defined(RLIMIT_NOFILE)
    {
	struct rlimit limit;
	memset(&limit,0,sizeof(limit));

	method = "rlimit";
	if (getrlimit(RLIMIT_NOFILE, &limit) < 0) {
	    perror("getrlimit");
	    exit(1);
	}

	/* set the current to the maximum or specified value */
	if (max_desired_fds) limit.rlim_cur = max_desired_fds;
	else limit.rlim_cur = limit.rlim_max;

	if (setrlimit(RLIMIT_NOFILE, &limit) < 0) {
	    perror("setrlimit");
	    exit(1);
	}
	max_descs = limit.rlim_max;

#ifdef RLIM_INFINITY
	if (limit.rlim_max == RLIM_INFINITY) max_descs = MAX_FD_GUESS * 4;	/* pick a more reasonable max */
#endif
    }
#elif defined (_SC_OPEN_MAX)
    /* Okay, you don't have getrlimit() and you don't have OPEN_MAX.
     * Time to try the POSIX sysconf() function.  (See Stevens'
     * _Advanced Programming in the UNIX Environment_).  */
    method = "POSIX sysconf";
    errno = 0;
    if ((max_descs = sysconf(_SC_OPEN_MAX)) < 0) {
	if (errno == 0)
	    max_descs = MAX_FD_GUESS * 4;
	else {
	    perror("calling sysconf");
	    exit(1);
	}
    }

    /* if everything has failed, we'll just take a guess */
#else
    method = "random guess";
    max_descs = MAX_FD_GUESS;
#endif

    /* this must go here, after rlimit code */
    if (max_desired_fds) return max_desired_fds;

    return max_descs;
}


tcpdemux::tcpdemux():outdir("."),flow_counter(0),packet_time(0),
		     max_fds(10),flow_map(),
		     start_new_connections(false),
		     openflows(), max_bytes_per_flow(),
                     max_desired_fds()
{
    /* Find out how many files we can have open safely...subtract 4 for
     * stdin, stdout, stderr, and the packet filter; one for breathing
     * room (we open new files before closing old ones), and one more to
     * be safe.
     */
    max_fds = get_max_fds() - NUM_RESERVED_FDS;
}


/* write a portion of memory to the disk. */
void tcpdemux::write_to_file(std::stringstream &ss,
			  const std::string &fname,const uint8_t *base,const uint8_t *buf,size_t buflen)
{
    int fd = retrying_open(fname.c_str(),O_WRONLY|O_CREAT|O_BINARY|O_TRUNC,0644);
    if(fd>=0){
	size_t count = write(fd,buf,buflen);
	if(close(fd)!=0 || count!=buflen){
	    std::cerr << "cannot write " << fname << ": " << strerror(errno) << "\n";
	    ss << "<write_error errno='" << errno << "' buflen='" << buflen << "' count='" << count << "'>";
	} else {
	    ss << "<byte_run file_offset='" << buf-base << "' len='" << buflen << "'>";
	    ss << "<filename>" << fname << "</filename>";
	    ss << "<hashdigest type='MD5'>" << md5_generator::hash_buf(buf,buflen).hexdigest() << "</hashdigest>";
	    ss << "</byte_run>\n";
	}
    }
}

/* Open a file, shriking the ring if necessary */
int tcpdemux::retrying_open(const char *filename,int oflag,int mask)
{
    while(true){
	if(openflows.size() >= max_fds) close_oldest();
	int fd = ::open(filename,oflag,mask);

	if(fd>=0) return fd;

        // wonder what it was
	if (errno != ENFILE && errno != EMFILE) return -1;    

	close_oldest();
    }
}

FILE *tcpdemux::retrying_fopen(const char *filename,const char *mode)
{
    while(true){
	if(openflows.size() >= max_fds) close_oldest();
	FILE *f = fopen(filename,mode);
	if(f) return f;
	
	if (errno != ENFILE && errno != EMFILE) {
	    return 0;
	}
	/* open failed because too many files are open... close one
	 * and try again
	 */
	close_oldest();
    };
}

/*
 * Called to processes a tcp packet
 */

void tcpdemux::process_tcp(const struct timeval *ts,const u_char *data, uint32_t length,
			const ipaddr &src, const ipaddr &dst,int32_t vlan,sa_family_t family)
{
    struct tcphdr *tcp_header = (struct tcphdr *) data;
    u_int tcp_header_len;
    tcp_seq seq;

    if (length < sizeof(struct tcphdr)) return;


    /* calculate the total length of the TCP header including options */
    tcp_header_len = tcp_header->th_off * 4;

    /* fill in the flow_addr structure with info that identifies this flow */
    flow_addr this_flow(src,dst,ntohs(tcp_header->th_sport),ntohs(tcp_header->th_dport),family);

    seq = ntohl(tcp_header->th_seq);
    int syn_set = IS_SET(tcp_header->th_flags, TH_SYN);
    /* recalculate the beginning of data and its length, moving past the
     * TCP header
     */
    data   += tcp_header_len;
    length -= tcp_header_len;

    /* see if we have state about this flow; if not, create it */
    tcpip *tcp = find_tcpip(this_flow);
    
    if(tcp==0 && start_new_connections==false) return; // don't start new ones

    uint64_t connection_count = 0;
    if(tcp){
	/* If offset will be too much, throw away this_flow and create a new one */
	tcp_seq isn2 = tcp->isn;		// local copy
	if(syn_set){
	    isn2 = seq - tcp->pos + 1;
	}
	tcp_seq offset  = seq - isn2;	// offset from the last seen packet to beginning of current
	tcp_seq roffset = isn2 - seq;
	if(offset != tcp->pos) {
            /** We need to seek if offset doesn't match current position.
	     * If the seek is too much, start a new connection.
	     * roffset handles reverse seek, which happens with out-of-order packet delivery.
	     */
	    if((offset > tcp->pos)
	       && (offset > tcp->pos_max)
	       && (length > 0)
	       && (max_bytes_per_flow==0) 
	       && ((uint64_t)offset > (uint64_t)tcp->pos +  min_skip)
	       && (roffset > min_skip)){
		connection_count = tcp->myflow.connection_count+1;
		remove_flow(this_flow);
		tcp = 0;
	    }
	}
    }
    if (tcp==NULL){
	tcp = create_tcpip(this_flow, vlan, seq, *ts,connection_count);
    }

    tcp->myflow.packet_count++;

    /* Handle empty packets (from Debian patch 10) */
    if (length == 0) {
	/* examine TCP flags for initial TCP handshake segments:
	 * - SYN means that the flow is a client -> server flow
	 * - SYN/ACK means that the flow is a server -> client flow.
	 */
	if ((tcp->isn - seq) == 0) {
	    if (IS_SET(tcp_header->th_flags, TH_SYN) && IS_SET(tcp_header->th_flags, TH_ACK)) {
		tcp->dir = tcpip::dir_sc;
		/* If the SYN flag is set the first data byte is offset by one,
		 * account for it (note: if we're here we have just created
		 * tcp, so it's safe to change isn).
		 */
		tcp->isn++;
	    } else if (IS_SET(tcp_header->th_flags, TH_SYN)) {
		tcp->dir = tcpip::dir_cs;
		tcp->isn++;
	    }
	}
    }

    if (length>0)
        tcp->store_packet(data, length, seq, syn_set);

    /* Finally, if there is a FIN, then kill this TCP connection*/
    if (IS_SET(tcp_header->th_flags, TH_FIN))
        remove_flow(this_flow);	// take it out of the map
}





/* This is called when we receive an IPv4 datagram.  We make sure that
 * it's valid and contains a TCP segment; if so, we pass it to
 * process_tcp() for further processing.
 *
 * Note: we currently don't know how to handle IP fragments. */
void tcpdemux::process_ip4(const struct timeval *ts,const u_char *data, uint32_t caplen,int32_t vlan)
{
    const struct ip *ip_header = (struct ip *) data;
    u_int ip_header_len;
    u_int ip_total_len;

    /* make sure that the packet is at least as long as the min IP header */
    if (caplen < sizeof(struct ip)) return;

    /* for now we're only looking for TCP; throw away everything else */
    if (ip_header->ip_p != IPPROTO_TCP) return;

    /* check and see if we got everything.  NOTE: we must use
     * ip_total_len after this, because we may have captured bytes
     * beyond the end of the packet (e.g. ethernet padding).
     */
    ip_total_len = ntohs(ip_header->ip_len);

    /* XXX - throw away everything but fragment 0; this version doesn't
     * know how to do fragment reassembly.
     */
    if (ntohs(ip_header->ip_off) & 0x1fff) return;

    /* figure out where the IP header ends */
    ip_header_len = ip_header->ip_hl * 4;

    /* make sure there's some data */
    if (ip_header_len > ip_total_len) return;

    /* do TCP processing, faking an ipv6 address  */
    process_tcp(ts,data + ip_header_len, ip_total_len - ip_header_len,
		ipaddr(ip_header->ip_src.s_addr),
		ipaddr(ip_header->ip_dst.s_addr),
		vlan,AF_INET);
}


/* This is called when we receive an IPv6 datagram.
 *
 * Note: we don't support IPv6 extended headers
 */


void tcpdemux::process_ip6(const struct timeval *ts,const u_char *data, const uint32_t caplen, const int32_t vlan)
{
    const struct ip6_hdr *ip_header = (struct ip6_hdr *) data;
    u_int16_t ip_payload_len;

    /* make sure that the packet is at least as long as the IPv6 header */
    if (caplen < sizeof(struct ip6_hdr)) return;



    /* for now we're only looking for TCP; throw away everything else */
    if (ip_header->ip6_nxt != IPPROTO_TCP) return;

    ip_payload_len = ntohs(ip_header->ip6_plen);

    /* make sure there's some data */
    if (ip_payload_len == 0) return;

    /* do TCP processing */

    process_tcp(ts,
		data + sizeof(struct ip6_hdr),
		ip_payload_len,
		ipaddr(ip_header->ip6_src.s6_addr),
		ipaddr(ip_header->ip6_dst.s6_addr),
		vlan,AF_INET6);
}



/* This is called when we receive an IPv4 or IPv6 datagram.
 * This function calls process_ip4 or process_ip6
 */

void tcpdemux::process_ip(const struct timeval *ts,const u_char *data, uint32_t caplen,int32_t vlan)
{
    const struct ip *ip_header = (struct ip *) data;
    if (caplen < sizeof(struct ip)) return;

    if(ip_header->ip_v == 6) {
	process_ip6(ts,data, caplen,vlan);
    } else {
	process_ip4(ts,data, caplen,vlan);
    }
}
 
 
/* This has to go somewhere; might as well be here */
static void terminate(int sig) __attribute__ ((__noreturn__));
static void terminate(int sig)
{
    exit(0); /* libpcap uses onexit to clean up */
}



/*
 * process an input file or device
 * May be repeated.
 * If start is false, do not initiate new connections
 */
void tcpdemux::process_infile(const char *device, const std::string &infile, bool start)
{
    char error[PCAP_ERRBUF_SIZE];
    pcap_t *pd=0;
    int dlt=0;
    pcap_handler handler;

    start_new_connections = start;
    if (infile!=""){
	if ((pd = pcap_open_offline(infile.c_str(), error)) == NULL){	/* open the capture file */
	    die("%s", error);
	}

	dlt = pcap_datalink(pd);	/* get the handler for this kind of packets */
	handler = find_handler(dlt, infile.c_str());
    } else {
	/* if the user didn't specify a device, try to find a reasonable one */
	if (device == NULL){
	    if ((device = pcap_lookupdev(error)) == NULL){
		die("%s", error);
	    }
	}

	/* make sure we can open the device */
	// if ((pd = pcap_open_live(device, SNAPLEN, !opt_no_promisc, 1000, error)) == NULL){
	//     die("%s", error);
	// }
	if ((pd = pcap_open_live(device, SNAPLEN, true, 1000, error)) == NULL){
	    die("%s", error);
	}


	/* drop root privileges - we don't need them any more */
	setuid(getuid());

	/* get the handler for this kind of packets */
	dlt = pcap_datalink(pd);
	handler = find_handler(dlt, device);
    }

    /* install the filter expression in libpcap */
    struct bpf_program	fcode;
    if (pcap_compile(pd, &fcode, "", 1, 0) < 0){
	die("%s", pcap_geterr(pd));
    }

    if (pcap_setfilter(pd, &fcode) < 0){
	die("%s", pcap_geterr(pd));
    }

    /* initialize our flow state structures */

    /* set up signal handlers for graceful exit (pcap uses onexit to put
     * interface back into non-promiscuous mode
     */
    portable_signal(SIGTERM, terminate);
    portable_signal(SIGINT, terminate);
    portable_signal(SIGHUP, terminate);

    /* start listening! */
    if (pcap_loop(pd, -1, handler, (u_char *)this) < 0){
	die("%s", pcap_geterr(pd));
    }
}

