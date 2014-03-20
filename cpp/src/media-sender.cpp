//
//  media-sender.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "media-sender.h"
#include "ndnrtc-namespace.h"
#include "ndnrtc-utils.h"

using namespace ndnlog::new_api;
using namespace ndnrtc;
using namespace webrtc;
using namespace std;

//******************************************************************************
#pragma mark - construction/destruction
MediaSender::MediaSender(const ParamsStruct &params) :
NdnRtcObject(params),
packetNo_(0),
pitCs_(*webrtc::CriticalSectionWrapper::CreateCriticalSection()),
faceThread_(*webrtc::ThreadWrapper::CreateThread(processEventsRoutine, this))
{
    packetRateMeter_ = NdnRtcUtils::setupFrequencyMeter();
    dataRateMeter_ = NdnRtcUtils::setupDataRateMeter(10);
}

MediaSender::~MediaSender()
{
    stop();
    
    NdnRtcUtils::releaseFrequencyMeter(packetRateMeter_);
    NdnRtcUtils::releaseDataRateMeter(dataRateMeter_);
}

//******************************************************************************
//******************************************************************************
#pragma mark - static


//******************************************************************************
#pragma mark - public
int MediaSender::init(const shared_ptr<Face> &face,
                      const shared_ptr<ndn::Transport> &transport)
{
    shared_ptr<string> userPrefix = NdnRtcNamespace::getStreamPrefix(params_);
    
    if (!userPrefix.get())
        notifyError(-1, "bad user prefix");
    
    certificateName_ = NdnRtcNamespace::certificateNameForUser(*userPrefix);
    
    ndnFace_ = face;
    ndnTransport_ = transport;
    ndnKeyChain_ = NdnRtcNamespace::keyChainForUser(*userPrefix);
    
    registerPrefix();
    
    shared_ptr<string> packetPrefix = NdnRtcNamespace::getStreamFramePrefix(params_);
    
    if (!packetPrefix.get())
        notifyError(-1, "bad frame prefix");
    
    packetPrefix_.reset(new Name(packetPrefix->c_str()));
    
    segmentSize_ = params_.segmentSize;
    freshnessInterval_ = params_.freshness;
    packetNo_ = 0;
    
    isProcessing_ = true;
    
    unsigned int tid;
    faceThread_.Start(tid);
    
    return RESULT_OK;
}

void MediaSender::stop()
{
    if (isProcessing_)
    {
        isProcessing_ = false;
        faceThread_.SetNotAlive();
        faceThread_.Stop();
    }
}

//******************************************************************************
#pragma mark - private
bool MediaSender::processEvents()
{
    try
    {
        ndnFace_->processEvents();
        usleep(10000);
    }
    catch (std::exception &e)
    {
        notifyError(-1, "ndn exception while processing %s", e.what());
        isProcessing_ = false;
    }
    return isProcessing_;
}

int MediaSender::publishPacket(const PacketData &packetData,
                               shared_ptr<Name> packetPrefix,
                               PacketNumber packetNo,
                               PrefixMetaInfo prefixMeta)
{
    if (!ndnTransport_->getIsConnected())
        return notifyError(-1, "transport is not connected");
    
    Name prefix = *packetPrefix;
    prefix.append(NdnRtcUtils::componentFromInt(packetNo));

    Segmentizer::SegmentList segments;
    
    if (RESULT_FAIL(Segmentizer::segmentize(packetData, segments, segmentSize_)))
        return notifyError(-1, "packet segmentation failed");
    
    try {
        prefixMeta.totalSegmentsNum_ = segments.size();
        Name metaSuffix = PrefixMetaInfo::toName(prefixMeta);
        
        for (Segmentizer::SegmentList::iterator it = segments.begin();
             it != segments.end(); ++it)
        {
            // add segment #
            Name segmentName = prefix;
            segmentName.appendSegment(it-segments.begin());

            // lookup for pending interests and construct metaifno accordingly
            SegmentData::SegmentMetaInfo meta = {0,0,0};
            lookupPrefixInPit(segmentName, meta);
            
            // add name suffix meta info
            segmentName.append(metaSuffix);
            
            // pack into network data
            SegmentData segmentData(it->getDataPtr(), it->getPayloadSize(), meta);
            
            Data ndnData(segmentName);
            ndnData.getMetaInfo().setFreshnessPeriod(params_.freshness*1000);
            ndnData.setContent(segmentData.getData(), segmentData.getLength());
            
            ndnKeyChain_->sign(ndnData, *certificateName_);
            
            SignedBlob encodedData = ndnData.wireEncode();
            ndnTransport_->send(*encodedData);
            
            NdnRtcUtils::dataRateMeterMoreData(dataRateMeter_,
                                               ndnData.getContent().size());

            LogTrace(NdnRtcUtils::toString("publisher-%s.log", params_.producerId))
            << "published " << segmentName << endl;
        }
    }
    catch (std::exception &e)
    {
        return notifyError(-1,
                           "got error from ndn library while sending data: %s",
                           e.what());
    }
    
    return segments.size();
}

void MediaSender::registerPrefix()
{
    shared_ptr<string> packetPrefix = NdnRtcNamespace::getStreamFramePrefix(params_);
    
    if (packetPrefix.get())
    {
        uint64_t prefixId = ndnFace_->registerPrefix(Name(packetPrefix->c_str()),
                                                     bind(&MediaSender::onInterest,
                                                          this, _1, _2, _3),
                                                     bind(&MediaSender::onRegisterFailed,
                                                          this, _1));
        if (prefixId != 0)
            LogTrace(NdnRtcUtils::toString("publisher-%s.log", params_.producerId))
            << "registered prefix " << *packetPrefix << endl;
    }
    else
        notifyError(-1, "bad packet prefix");
}

void MediaSender::onInterest(const shared_ptr<const Name>& prefix,
                             const shared_ptr<const Interest>& interest,
                             ndn::Transport& transport)
{
    PacketNumber packetNo = NdnRtcNamespace::getPacketNumber(interest->getName());
    
    if (packetNo > packetNo_)
    {
        addToPit(interest);
    }
    else
    {
        LogTrace(NdnRtcUtils::toString("old-%s.log", params_.producerId))
        << "interest for old " << interest->getName() << endl;
    }
}

void MediaSender::onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix)
{
    notifyError(-1, "registration on %s has failed", prefix->toUri().c_str());
}

void MediaSender::addToPit(const shared_ptr<const ndn::Interest> &interest)
{
    const Name& name = interest->getName();
    PitEntry pitEntry;
    
    pitEntry.arrivalTimestamp_ = NdnRtcUtils::millisecondTimestamp();
    pitEntry.interest_ = interest;
    
    {
        webrtc::CriticalSectionScoped scopedCs_(&pitCs_);
        
        if (pit_.find(name) != pit_.end())
            LogTrace(NdnRtcUtils::toString("addpit-%s.log", params_.producerId))
            << "pit exists " << name << endl;
        
        pit_[name] = pitEntry;
    }
    
    LogTrace(NdnRtcUtils::toString("addpit-%s.log", params_.producerId))
    << "new pit entry " << name << " " << pit_.size() << endl;
    
}

void MediaSender::lookupPrefixInPit(const ndn::Name &prefix,
                                    SegmentData::SegmentMetaInfo &metaInfo)
{
    {
        webrtc::CriticalSectionScoped scopedCs_(&pitCs_);
        
        map<Name, PitEntry>::iterator pitHit = pit_.find(prefix);
        
//        LogTrace(NdnRtcUtils::toString("publisher-%s.log", params_.producerId))
//        << "published " << prefix << endl;
        
        if (pitHit != pit_.end())
        {
            int64_t currentTime = NdnRtcUtils::millisecondTimestamp();
            
            shared_ptr<const Interest> pendingInterest = pitHit->second.interest_;
            
            metaInfo.interestNonce_ =
            NdnRtcUtils::blobToNonce(pendingInterest->getNonce());
            metaInfo.interestArrivalMs_ = pitHit->second.arrivalTimestamp_;
            metaInfo.generationDelayMs_ = (uint32_t)(currentTime - pitHit->second.arrivalTimestamp_);
            
            pit_.erase(pitHit);
            
            LogTrace(NdnRtcUtils::toString("pithit-%s.log", params_.producerId))
            << "pit hit [" << prefix.toUri() << "] -> ["
            << pendingInterest->getName().toUri() << " (size " << pit_.size() << endl;
            
        }
        else
        {
            LogTrace(NdnRtcUtils::toString("nopit-%s.log", params_.producerId))
            << "no pit entry " << prefix.toUri() << endl;
        }
    }
}
