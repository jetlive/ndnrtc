//
//  ndnrtc-utils-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "test-common.h"
#include "ndnrtc-utils.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace std;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

TEST(NdnRtcUtilsTests, TestSegmentsNumber)
{
    {
        int segmentSize = 3800;
        int dataSize = 2000;
        int expectedSegmentsNum = 1;
        
        EXPECT_EQ(expectedSegmentsNum, NdnRtcUtils::getSegmentsNumber(segmentSize, dataSize));
    }
    {
        int segmentSize = 3800;
        int dataSize = 0;
        int expectedSegmentsNum = 0;
        
        EXPECT_EQ(expectedSegmentsNum, NdnRtcUtils::getSegmentsNumber(segmentSize, dataSize));
    }
    {
        int segmentSize = 20;
        int dataSize = 100;
        int expectedSegmentsNum = 5;
        
        EXPECT_EQ(expectedSegmentsNum, NdnRtcUtils::getSegmentsNumber(segmentSize, dataSize));
    }
    {
        int segmentSize = 20;
        int dataSize = 101;
        int expectedSegmentsNum = 6;
        
        EXPECT_EQ(expectedSegmentsNum, NdnRtcUtils::getSegmentsNumber(segmentSize, dataSize));
    }
    {
        int segmentSize = 20;
        int dataSize = 99;
        int expectedSegmentsNum = 5;
        
        EXPECT_EQ(expectedSegmentsNum, NdnRtcUtils::getSegmentsNumber(segmentSize, dataSize));
    }
    {
        int segmentSize = 100;
        int dataSize = 100;
        int expectedSegmentsNum = 1;
        
        EXPECT_EQ(expectedSegmentsNum, NdnRtcUtils::getSegmentsNumber(segmentSize, dataSize));
    }
    {
        int segmentSize = 100;
        int dataSize = 101;
        int expectedSegmentsNum = 2;
        
        EXPECT_EQ(expectedSegmentsNum, NdnRtcUtils::getSegmentsNumber(segmentSize, dataSize));
    }
}

TEST(NdnRtcUtilsTests, TestFrameNumber)
{
    {
        char *prefixStr = (char*)"/ndn/ndnrtc/user/testuser/streams/video0/vp8-640/frames/1";
        Name prefix(prefixStr);
        int frameNo = NdnRtcUtils::frameNumber(prefix.getComponent(prefix.getComponentCount()-1));
        
        EXPECT_EQ(1, frameNo);
    }
    
    { // check error
        char *prefixStr = (char*)"/ndn/ndnrtc/user/testuser/streams/video0/vp8-640/frames";
        Name prefix(prefixStr);
        int frameNo = NdnRtcUtils::frameNumber(prefix.getComponent(prefix.getComponentCount()-1));
        
        EXPECT_EQ(-1, frameNo);
    }
}

TEST(NdnRtcUtilsTests, TestVoiceEngineInstance)
{
    VoiceEngine *voiceEngine = NdnRtcUtils::sharedVoiceEngine();
    
    ASSERT_FALSE(NULL == voiceEngine);
    
    { // test requesting interfaces
        for (int i = 0; i < 5; i++)
        {// get VoEBase sub API
            VoEBase *voe_base = VoEBase::
            GetInterface(NdnRtcUtils::sharedVoiceEngine());
            
            EXPECT_FALSE(NULL == voe_base);
            EXPECT_LE(0, voe_base->Release());
        }
        
        for (int i = 0; i < 5; i++)
        {// get VoENetwork
            VoENetwork *voe_network = VoENetwork::
            GetInterface(NdnRtcUtils::sharedVoiceEngine());
            
            EXPECT_FALSE(NULL == voe_network);
            EXPECT_LE(0, voe_network->Release());
        }
        
        for (int i = 0; i < 5; i++)
        {// get VoeProcessing
            VoEAudioProcessing *voe_proc = VoEAudioProcessing::
            GetInterface(NdnRtcUtils::sharedVoiceEngine());
            
            EXPECT_FALSE(NULL == voe_proc);
            EXPECT_LE(0, voe_proc->Release());
        }
    }
    
    { // test requesting interfaces in a row
        {// get VoEBase sub API
            VoEBase *voe_base[5];
            
            for (int i = 0; i < 5; i++)
            {
                voe_base[i]= VoEBase::
                GetInterface(NdnRtcUtils::sharedVoiceEngine());
                EXPECT_FALSE(NULL == voe_base[i]);
            }
            
            for (int i = 0; i < 5; i++)
                EXPECT_EQ(5-i, voe_base[i]->Release());
        }
        
        for (int i = 0; i < 5; i++)
        {// get VoENetwork
            VoENetwork *voe_network[5];
            
            for (int i = 0; i < 5; i++)
            {
                voe_network[i]= VoENetwork::
                GetInterface(NdnRtcUtils::sharedVoiceEngine());
                EXPECT_FALSE(NULL == voe_network[i]);
            }
            
            for (int i = 0; i < 5; i++)
                EXPECT_EQ(5-i, voe_network[i]->Release());
        }
        
        for (int i = 0; i < 5; i++)
        {// get VoeProcessing
            VoEAudioProcessing *voe_proc[5];
            
            for (int i = 0; i < 5; i++)
            {
                voe_proc[i]= VoEAudioProcessing::
                GetInterface(NdnRtcUtils::sharedVoiceEngine());
                EXPECT_FALSE(NULL == voe_proc[i]);
            }
            
            for (int i = 0; i < 5; i++)
                EXPECT_EQ(5-i, voe_proc[i]->Release());
        }
    }
    
    NdnRtcUtils::releaseVoiceEngine();
}

TEST(NdnRtcUtilsTests, TestTimestamps)
{
    { // test microseconds timestamps
        int sleepUS = 100000;
        
        struct timeval tv;
        gettimeofday(&tv, NULL);
        
        int64_t ticks = 1000000LL * static_cast<int64_t>(tv.tv_sec) +
        static_cast<int64_t>(tv.tv_usec);
        
        
        int64_t a = NdnRtcUtils::microsecondTimestamp();
        usleep(sleepUS);
        int64_t b = NdnRtcUtils::microsecondTimestamp();
        
        EXPECT_LE(a,b);
        int64_t lb = (b-a)-(int64_t)(0.1*(double)sleepUS),
                rb = (b-a)+(int64_t)(0.1*(double)sleepUS);
        EXPECT_GE(sleepUS, lb);
        EXPECT_LE(sleepUS, rb);
    }
}

TEST(NdnRtcUtilsTests, TestNonce)
{
    {
        uint32_t nonce = NdnRtcUtils::generateNonceValue();
        Blob nonceBlob = NdnRtcUtils::nonceToBlob(nonce);
        
        for (int i = 0; i < 4; i++)
        {
            uint8_t b = ((uint8_t*)&nonce)[i];
            
            EXPECT_EQ(b, nonceBlob.buf()[3-i]);
        }
    }
}
