// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2016 UCAR, NCAR, All Rights Reserved
 ********************************************************************
*/
#ifndef NIDAS_CORE_NIDASREACTOR_H
#define NIDAS_CORE_NIDASREACTOR_H


namespace nidas { namespace core {

class NidasReactorHandler;
class NidasReactorImpl;

/**
 * NidasReactor implements the core of a reactor for responding to file
 * descriptors.  Register different wrappers of IO streams directly, like
 * SampleInputStream, or register an IOChannel.  When an IO event is ready,
 * the event is passed to the handler.  Handlers are implementations of the
 * NidasReactorHandler interface.
 **/
class NidasReactor
{
public:

    NidasReactor();



private:

    NidasReactor(const NidasReactor&);
    NidasReactor& operator=(const NidasReactor&);

    NidasReactorImpl* _impl;

};


class NidasReactorHandler
{
public:

    virtual int getHandle() = 0;

    virtual int handleInput();

    virtual int handleOutput();

    virtual int handleSignal();

    virtual int handleTimeout();

    virtual int close();
    
    virtual NidasReactor*
    reactor()
    {
        return _reactor;
    }

protected:

    /**
     * Force abstract base class, and allow constructing without a
     * NidasReactor reference.
     **/
    NidasReactorHandler(NidasReactor* reactor = 0) :
        _reactor(reactor)
    {}

private:

    NidasReactor* _reactor;

};


}}	// namespace nidas namespace core

#endif  // NIDAS_CORE_NIDASREACTOR_H
