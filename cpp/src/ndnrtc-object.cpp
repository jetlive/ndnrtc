//
//  ndnrtc-object.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/21/13
//

#include "ndnrtc-object.h"

using namespace webrtc;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace std;
using namespace ndnlog;

//******************************************************************************
/**
 * @name NdnRtcComponent class
 */
NdnRtcComponent::NdnRtcComponent():
callbackSync_(*CriticalSectionWrapper::CreateCriticalSection())
{}

NdnRtcComponent::NdnRtcComponent(INdnRtcComponentCallback *callback):
callback_(callback),
callbackSync_(*CriticalSectionWrapper::CreateCriticalSection())
{}

NdnRtcComponent::~NdnRtcComponent()
{
    std::cout << description_ << " component dtor" << std::endl;
    callbackSync_.~CriticalSectionWrapper();
}

void NdnRtcComponent::onError(const char *errorMessage, const int errorCode)
{
    callbackSync_.Enter();
    
    if (hasCallback())
        callback_->onError(errorMessage, errorCode);
    else
    {
        LogErrorC << "error occurred: " << string(errorMessage) << endl;
        if (logger_) logger_->flush();
    }
    
    callbackSync_.Leave();
}

int NdnRtcComponent::notifyError(const int ecode, const char *format, ...)
{
    va_list args;
    
    static char emsg[256];
    
    va_start(args, format);
    vsprintf(emsg, format, args);
    va_end(args);
    
    if (hasCallback())
    {
        callback_->onError(emsg, ecode);
    }
    else
        LogErrorC
        << "error (" << ecode << ") occurred: "
        << string(emsg) << endl;
    
    return ecode;
}

std::string
NdnRtcComponent::getDescription() const
{
    if (description_ == "")
    {
        std::stringstream ss;
        ss << "NdnRtcObject "<< std::hex << this;
        return ss.str();
    }
    
    return description_;
}
