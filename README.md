# HTML Flow Parser #

This software is fed a network capture, retrieves the HTML payloads, and 
parses them for symptoms of unethical search engine optimization.  This 
seeks to avoid the embarrassing scenario of a home institution being 
notified by a user or outside entity that their server appears to be hacked 
(e.g. viagra is advertized).  By taking an active approach, we hope the 
uptime of these problems is cut drastically.

## Instructions ##
Download a copy of the repository by running the command 
`git clone https://github.com/DoctorSher/html-flow-parser.git`.  Navigate 
to the created folder, and type `make` to compile the project.  To 
run, type `./tcpflow pcap-file`, where pcap-file is the packet capture 
file you'd like to search.

## Design Components ##
The first component is a modified `tcpflow`; the original version is 
maintained by Simson Garfinkel at https://github.com/simsong/tcpflow. I 
slimmed it down for my narrow purposes (~4500 lines of code reduction).  My 
version of tcpflow receives a packet capture file, and produces the 
flow files as the original did.  However, once an inbound/outbound flow 
pair is ready (client to server and server to client per tcp connection), 
it sends both of these as arguments to the second and last component.

This component is the `HTML Flow Parser`, and the focus of the project.  It 
receives the flow files, and does the following steps:

1. Determines from the port numbers on the ends of the flow pair which one 
the outbound flow is.  This is a simple check to see which file's remote port is 80.  
2. Retrieves the host domain from the outbound flow, which will come into
play later.
3. Parses the HTTP options of the inbound flow (containing the HTTP 
response(s) that we are concerned with). Deals with chunked and compressed
sending options.  Does whatever recipe of instrutions it needs to retrieve 
the HTML payload.
4. Parses the HTML payload for symptoms of unethical SEO.  We have two
currently implemented tests.  
    1. The first is a remote URL frequency counter. This collects all of 
the remote URLs from the page (e.g. using http). With these URLs, it 
checks to see if a host domain was linked to with immediate proximity of 
another link to the same domain. The reason we extract the hostname from 
the inbound flow, is that we don't want to penalize a website for linking 
remotely to another website in the same domain.  If the frequency of this 
links is higher than a user-configured threshold, a warning is raised.  The 
reason for the frequency counter is that many times malicious posts are 
very repetitious, in order to maximize the SEO optimization for their 
malicious site.
    2. The second is a simple keyword search.  Just as it sounds, it scans the 
page for certain keywords.  If a keyword appears more than a user-configured
threshold, a warning is raised. The keywords used were garnered from a Stefan
Savage et al. paper entitled "Judging a site by its content: learning the textual, 
structural, and visual features of malicious Web pages."  In this paper, they 
used Machine Learning techniques to illuminate the most common keywords for 
malicious websites.  This seemed like a great starting point to us.
5. With all of these warnings, a simple textual report is generated. Ideally,
one would run this and then check out the warnings immediately.  

## Results ##
The program isn't anywhere close to being polished, but it works quite well
for what it does. There are false positives, but part of that can lie in the
user-configured thresholds chosen.  Future considerations are also looking
to reduce the number. 

## Future Work ##
 - Implementing more and better tests. Open to ideas!
 - Making the parser more robust (e.g. with regular expressions instead of
string searches).  
 - Allow for user-uploadable keywords. Currently they can pick which categories
they expect to not appear on their site, but cannot upload their own.
 - Parse Javascript and CSS, as URLs and other black hat techniques can appear
in there.
 - Make tests more pluggable.  Instead of being hardcoded into the parser,
they could be loaded much like kernel modules. 

## Licenses ##
Copyright 2012 Washington University in St Louis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

> Please see `PARSER LICENSE` for more details.

The original license of `tcpflow` is also included.

> Please see `TCPFLOW LICENSE` for more details.