/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_INPUT_H
#define DSM_INPUT_H

#include <atdUtil/McSocket.h>
#include <DOMable.h>

namespace dsm {

/**
 * A virtual base class for reading data.
 *
 * Types of inputs, and what they need to get connected:
 *
 *	interface SocketAcceptor:
 *		void offer(Socket* sock,int pseudoPort);
 *      interface Input, Output
 *		void offer(Socket* sock);
 *	interface SampleInput, SampleOutput
 *		getPseudoPort()
 *		requestConnection(SocketAcceptor*);
 *		offer(Socket*)
 *		clone()
 *
 *	DSMServices are SocketAcceptors
 *
 *	characteristics of an Input/Output:
 *		immediate connection or listen/accept
 *		tricky to implement simultaneous input & output of
 *			listen/accept type
 *
 *	DSMService:
 *		offer() method:
 *			pseudoPort matches input:
 *			clone service, clone input, clone non-singleton outputs
 *			clone->input->offer()
 *			clone->start()
 *	McServerSocketInput:
 *		service calls Input::requestConnection(this,pseudoPort)
 *			Input::requestConnection does
 *				listen(service);
 *      ServerSocketInput
 *		service calls Input::requestConnection(this)
 *			this starts accept thread
 *			on accept, thread calls service->offer(Socket)
 *			run method continues
 *	McSocketInput (no cloning of services)
 *		Service calls Input::requestConnection(this)
 *			start multicaster thread
 *			start acceptor detached thread
 *			wait on accept
 *			on accept, cancel/join requestor
 *			call service->offer(Socket)
 *			return from run
 *	SocketInput (no cloning of services)
 *		Service calls Input::requestConnection(this)
 *			socket->connect
 *			call service->offer(Socket)
 *	FileSet
 *		getInet4SocketAddress() returns any
 *		offer() does nothing
 *		Input::requestConnection(this)	does a fileset open
 *			clone->start() (no offer)
 *		
 *	SampleOutputInterface:
 *		SampleClient
 *		requestConnection
 *		clone?
 *		offer
 *      SampleInputInterface
 *		SampleSource
 *		requestConnection
 *		clone, offer
 *	RawSampleOutputStream:
 *		requestConnection() -> Output::requestConnection
 *		clone
 *		offer -> Output::offer
 *		fromDOM: ctor, output=Output::outputFromDOM
 *		enable (done at beginning of run method
 *			create OutputStream wrapper
 *			init buffers
 */
class Input : public DOMable {

public:

    virtual const std::string& getName() const = 0;
    /**
     * After the Input is configured, a user of Input calls requestConnection
     * to get things started. It is like opening a device, but in the case
     * of sockets, it just starts the process of accepting or connecting.
     * The caller also sets the pseudoPort number that should be used.
     * Only when the offer() method calls back is the socket actually
     * open.
     */
    virtual void requestConnection(atdUtil::SocketAccepter*,int pseudoPort)
    	throw(atdUtil::IOException);

    /**
     * Derived classes must provide clone.
     */
    virtual Input* clone() const = 0;

    /**
     * When the socket connection has been established, this offer
     * method will be called.  Actual socket inputs must override this.
     */
    virtual void offer(atdUtil::Socket* sock) throw(atdUtil::IOException);

    /**
     * Return suggested buffer length.
     */
    virtual size_t getBufferSize() const { return 8192; }

    /**
     * Physical read method which must be implemented in derived
     * classes. Returns the number of bytes written, which
     * may be less than the number requested.
     */
    virtual size_t read(void* buf, size_t len) throw(atdUtil::IOException) = 0;

    virtual void close() throw(atdUtil::IOException) = 0;

    virtual int getFd() const = 0;

    static Input* fromInputDOMElement(const xercesc::DOMElement* node)
            throw(atdUtil::InvalidParameterException);

};

}

#endif
