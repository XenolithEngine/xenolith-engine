/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

// <sys/socket.h> + <netinet/in.h> + <poll.h> + <sys/select.h> over a real 127.0.0.1
// loopback. The host (Linux glibc) and the freestanding Windows libc_impl (winsock via
// the ws2_32 loader, run under wine) must produce byte-identical output, so nothing that
// varies between runs or platforms is printed:
//   - descriptors are a small POSIX int on host but a 64-bit winsock SOCKET on Windows,
//     so the SPRT `SOCKET` type carries them and only "ok"/"fail" is shown;
//   - the ephemeral port the OS assigns is never printed - it is only COMPARED
//     (getpeername() must match the bound getsockname(), the port must be non-zero);
//   - only success paths are exercised: the winsock wrapper reports failures through
//     WSAGetLastError, not errno, so an error-path errno would diverge from glibc.
// select()/poll() are driven on sockets on purpose - winsock's select/WSAPoll accept
// only sockets, so this is the sole portable way to test them. `struct sockaddr_in` is the
// SPRT type from <netinet/in.h> (its wire image - family, network-order port, IPv4 addr -
// is identical across every target this runs on).

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> // htons / htonl
#include <poll.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "test_util.h"

#ifndef __SPRT_PLATFORM_ID
typedef int SOCKET;
typedef void *sockdata_t;
#define closesocket(fd) close(fd)
#endif

namespace sprt::test {

// Loopback datagram byte counts fit an int; print via (long long)/%lld so the width
// is identical whether send()/recv() return ssize_t (host) or int (winsock).
static long long ll(long long v) { return v; }

// socket()/accept() report failure as -1 on POSIX and INVALID_SOCKET==(SOCKET)-1 on
// Windows; both collapse to this single test.
static bool sockOk(SOCKET s) { return s != (SOCKET)-1; }

static struct sockaddr *sa(struct sockaddr_in *a) { return (struct sockaddr *)a; }

// 127.0.0.1:netPort (netPort already network order; 0 = "assign one").
static void loopback(struct sockaddr_in *a, unsigned short netPort) {
	::memset(a, 0, sizeof(*a));
	a->sin_family = AF_INET;
	a->sin_port = netPort;
	a->sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
}

// <sys/socket.h>: socket/bind/getsockname/getpeername/connect/send/recv/sendto/
// recvfrom/getsockopt/setsockopt/shutdown over loopback UDP.
void performSocketTest() {
	// socket() for both types + SO_TYPE readback. SOCK_STREAM==1 / SOCK_DGRAM==2 are
	// identical everywhere, so the getsockopt result compares equal on every platform.
	SOCKET stream = socket(AF_INET, SOCK_STREAM, 0);
	printf("socket(STREAM)=%s\n", sockOk(stream) ? "ok" : "fail");
	SOCKET dgram = socket(AF_INET, SOCK_DGRAM, 0);
	printf("socket(DGRAM)=%s\n", sockOk(dgram) ? "ok" : "fail");

	int soType = -1;
	socklen_t soTypeLen = sizeof(soType);
	int gto = getsockopt(dgram, SOL_SOCKET, SO_TYPE, (sockdata_t *)&soType, &soTypeLen);
	printf("getsockopt(SO_TYPE)=%d type==DGRAM=%d\n", gto, soType == SOCK_DGRAM);

	// setsockopt: enabling SO_REUSEADDR succeeds on both stacks.
	int one = 1;
	int sso = setsockopt(dgram, SOL_SOCKET, SO_REUSEADDR, (sockdata_t *)&one, sizeof(one));
	printf("setsockopt(SO_REUSEADDR)=%d\n", sso);
	closesocket(stream);

	// bind a receiver to an ephemeral loopback port.
	struct sockaddr_in ba;
	loopback(&ba, 0);
	SOCKET rcv = socket(AF_INET, SOCK_DGRAM, 0);
	printf("bind=%d\n", bind(rcv, sa(&ba), sizeof(ba)));

	// getsockname reports the assigned address: AF_INET, non-zero port (value not printed).
	struct sockaddr_in la;
	::memset(&la, 0, sizeof(la));
	socklen_t laLen = sizeof(la);
	int gsn = getsockname(rcv, sa(&la), &laLen);
	unsigned short rport = la.sin_port; // network order
	printf("getsockname=%d family==INET=%d port!=0=%d\n", gsn, la.sin_family == AF_INET,
			rport != 0);

	// sendto() from an unbound sender to the receiver, then recvfrom().
	SOCKET snd = socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in da;
	loopback(&da, rport);
	long long sn = sendto(snd, "ping", 4, 0, sa(&da), sizeof(da));
	printf("sendto=%lld\n", ll(sn));

	char buf[32];
	memset(buf, 0, sizeof(buf));
	struct sockaddr_in fa;
	::memset(&fa, 0, sizeof(fa));
	socklen_t faLen = sizeof(fa);
	long long rn = recvfrom(rcv, buf, sizeof(buf), 0, sa(&fa), &faLen);
	printf("recvfrom=%lld data=[%.*s] from==INET=%d\n", ll(rn), (int)(rn > 0 ? rn : 0), buf,
			fa.sin_family == AF_INET);

	// connect() the sender to the receiver; getpeername() must report that exact port.
	int cx = connect(snd, sa(&da), sizeof(da));
	struct sockaddr_in pa;
	::memset(&pa, 0, sizeof(pa));
	socklen_t paLen = sizeof(pa);
	int gpn = getpeername(snd, sa(&pa), &paLen);
	printf("connect=%d getpeername=%d peer_port==bound=%d\n", cx, gpn, pa.sin_port == rport);

	// The receiver replies to the address recvfrom() reported; the CONNECTED sender then
	// recv()s it (a connected datagram socket only accepts its peer, i.e. the receiver).
	long long sn2 = sendto(rcv, "pong", 4, 0, sa(&fa), faLen);
	printf("reply sendto=%lld\n", ll(sn2));
	::memset(buf, 0, sizeof(buf));
	long long rn2 = recv(snd, buf, sizeof(buf), 0);
	printf("recv=%lld data=[%.*s]\n", ll(rn2), (int)(rn2 > 0 ? rn2 : 0), buf);

	// shutdown() a connected datagram socket returns 0 on both stacks.
	printf("shutdown=%d\n", shutdown(snd, SHUT_RDWR));

	printf("close rcv=%d snd=%d dgram=%d\n", closesocket(rcv), closesocket(snd),
			closesocket(dgram));
}

// <sys/socket.h>: the connection-oriented path - listen/accept/connect + stream
// send/recv over loopback TCP. connect() to a listening loopback socket completes the
// handshake synchronously, so the single-threaded connect-then-accept sequence never
// blocks and stays deterministic.
void performSocketStreamTest() {
	// server: bind + listen on an ephemeral loopback port.
	struct sockaddr_in ba;
	loopback(&ba, 0);
	SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
	int one = 1;
	setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (sockdata_t *)&one, sizeof(one));
	printf("bind=%d\n", bind(srv, sa(&ba), sizeof(ba)));
	printf("listen=%d\n", listen(srv, 1));

	struct sockaddr_in la;
	::memset(&la, 0, sizeof(la));
	socklen_t laLen = sizeof(la);
	getsockname(srv, sa(&la), &laLen);
	unsigned short sport = la.sin_port;

	// client: connect() to the listening port (completes locally for loopback).
	SOCKET cli = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in da;
	loopback(&da, sport);
	printf("connect=%d\n", connect(cli, sa(&da), sizeof(da)));

	// accept() dequeues the completed connection; the peer is on 127.0.0.1.
	struct sockaddr_in pa;
	::memset(&pa, 0, sizeof(pa));
	socklen_t paLen = sizeof(pa);
	SOCKET acc = accept(srv, sa(&pa), &paLen);
	printf("accept=%s peer==INET=%d\n", sockOk(acc) ? "ok" : "fail", pa.sin_family == AF_INET);

	// the accepted socket's local port is the listening port.
	struct sockaddr_in aa;
	::memset(&aa, 0, sizeof(aa));
	socklen_t aaLen = sizeof(aa);
	getsockname(acc, sa(&aa), &aaLen);
	printf("accepted local_port==listen=%d\n", aa.sin_port == sport);

	// stream round-trip both ways.
	char buf[32];
	long long c2s = send(cli, "tcp-up", 6, 0);
	::memset(buf, 0, sizeof(buf));
	long long c2sr = recv(acc, buf, sizeof(buf), 0);
	printf("client->server=%lld recv=%lld data=[%.*s]\n", ll(c2s), ll(c2sr),
			(int)(c2sr > 0 ? c2sr : 0), buf);

	long long s2c = send(acc, "tcp-down", 8, 0);
	::memset(buf, 0, sizeof(buf));
	long long s2cr = recv(cli, buf, sizeof(buf), 0);
	printf("server->client=%lld recv=%lld data=[%.*s]\n", ll(s2c), ll(s2cr),
			(int)(s2cr > 0 ? s2cr : 0), buf);

	// The two constants portable code reaches for that Winsock has no spelling of. Both are
	// supposed to work here unchanged, and both fail only at run time when they do not:
	//   - SOL_IP has to BE the IP protocol number (0), the setsockopt() level winsock,
	//     Linux and BSD all expect. Deriving it from SOL_SOCKET, as this table once did,
	//     compiles everywhere and then fails every call with WSAEINVAL;
	//   - MSG_NOSIGNAL asks send() to suppress a signal Windows does not have, so the flag
	//     word that carries it must be one winsock accepts - zero. Winsock fails send()
	//     with WSAEOPNOTSUPP for any bit it does not recognize, and curl passes this flag
	//     on every send it makes.
	// Darwin spells neither name; fall back the way portable code does so the output of
	// this test stays identical on every target.
#ifdef SOL_IP
	const int ipLevel = SOL_IP;
#else
	const int ipLevel = IPPROTO_IP;
#endif
#ifdef MSG_NOSIGNAL
	const int noSignal = MSG_NOSIGNAL;
#else
	const int noSignal = 0;
#endif
	int ttl = 64;
	int sttl = setsockopt(cli, ipLevel, IP_TTL, (sockdata_t *)&ttl, sizeof(ttl));
	int ttlBack = 0;
	socklen_t ttlLen = sizeof(ttlBack);
	int gttl = getsockopt(cli, ipLevel, IP_TTL, (sockdata_t *)&ttlBack, &ttlLen);
	printf("setsockopt(SOL_IP,IP_TTL)=%d getsockopt=%d readback==64=%d\n", sttl, gttl,
			ttlBack == 64);

	long long ns = send(cli, "nosig", 5, noSignal);
	::memset(buf, 0, sizeof(buf));
	long long nsr = recv(acc, buf, sizeof(buf), 0);
	printf("send(MSG_NOSIGNAL)=%lld recv=%lld data=[%.*s]\n", ll(ns), ll(nsr),
			(int)(nsr > 0 ? nsr : 0), buf);

	printf("shutdown=%d\n", shutdown(cli, SHUT_WR));
	printf("close acc=%d cli=%d srv=%d\n", closesocket(acc), closesocket(cli), closesocket(srv));
}

// <poll.h> / <sys/select.h>: readiness on a loopback UDP socket, both the "nothing
// pending -> timeout" and the "datagram queued -> readable" transitions.
void performSelectPollTest() {
	struct sockaddr_in ba;
	loopback(&ba, 0);
	SOCKET rcv = socket(AF_INET, SOCK_DGRAM, 0);
	bind(rcv, sa(&ba), sizeof(ba));
	struct sockaddr_in la;
	::memset(&la, 0, sizeof(la));
	socklen_t laLen = sizeof(la);
	getsockname(rcv, sa(&la), &laLen);

	// Nothing has been sent: poll() with a 0 timeout returns 0 (not readable).
	struct pollfd pfd;
	pfd.fd = rcv;
	pfd.events = POLLIN;
	pfd.revents = 0;
	printf("poll(empty)=%d\n", poll(&pfd, 1, 0));

	// select() with a zero timeout likewise reports nothing ready.
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(rcv, &rfds);
	struct timeval z;
	z.tv_sec = 0;
	z.tv_usec = 0;
	printf("select(empty)=%d\n", select((int)rcv + 1, &rfds, nullptr, nullptr, &z));

	// Queue a datagram to ourselves via a second socket.
	SOCKET snd = socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in da;
	loopback(&da, la.sin_port);
	sendto(snd, "R", 1, 0, sa(&da), sizeof(da));

	// poll() now reports POLLIN (generous timeout - the datagram is already queued).
	pfd.fd = rcv;
	pfd.events = POLLIN;
	pfd.revents = 0;
	int pr = poll(&pfd, 1, 2'000);
	printf("poll(ready)=%d POLLIN=%d\n", pr, (pfd.revents & POLLIN) ? 1 : 0);

	// select() reports the descriptor readable.
	FD_ZERO(&rfds);
	FD_SET(rcv, &rfds);
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	int sr = select((int)rcv + 1, &rfds, nullptr, nullptr, &tv);
	printf("select(ready)=%d isset=%d\n", sr, FD_ISSET(rcv, &rfds) ? 1 : 0);

	// Drain the datagram so the run leaves no residue.
	char buf[8];
	memset(buf, 0, sizeof(buf));
	long long rn = recv(rcv, buf, sizeof(buf), 0);
	printf("drain=%lld data=[%.*s]\n", ll(rn), (int)(rn > 0 ? rn : 0), buf);

	closesocket(rcv);
	closesocket(snd);
}

} // namespace sprt::test
