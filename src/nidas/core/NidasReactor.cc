

#include "NidasReactor.h"




    while (!isInterrupted()) {

#ifdef HAVE_PPOLL
        int nfd = ::ppoll(&fds,1,NULL,&sigmask);
        if (nfd < 0) {
	    if (errno == EINTR) continue;
            throw nidas::util::IOException(_readsock->getLocalSocketAddress().toString(), "ppoll",errno);
        }
        if (fds.revents & POLLERR)
            throw nidas::util::IOException(_readsock->getLocalSocketAddress().toString(), "receive",errno);

#ifdef POLLRDHUP
        if (fds.revents & (POLLHUP | POLLRDHUP))
#else
        if (fds.revents & POLLHUP)
#endif
            WLOG(("%s POLLHUP",_readsock->getLocalSocketAddress().toString().c_str()));

        if (!fds.revents & POLLIN) continue;

#else
        int fd = _readsock->getFd();
        assert(fd >= 0 && fd < FD_SETSIZE);     // FD_SETSIZE=1024
        FD_SET(fd,&readfds);
        int nfd = ::pselect(fd+1,&readfds,NULL,NULL,0,&sigmask);
        if (nfd < 0) {
	    if (errno == EINTR) continue;
            throw nidas::util::IOException(_readsock->getLocalSocketAddress().toString(), "pselect",errno);
        }
#endif

        _readsock->receive(dgram,pktinfo,0);





	
