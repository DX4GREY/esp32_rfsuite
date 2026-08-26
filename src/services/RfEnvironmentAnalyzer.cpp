#include "services/RfEnvironmentAnalyzer.h"
#include "drivers/RadioManager.h"
#include "core/RfEnvironmentMath.h"
#include "services/SessionRecorder.h"

RfEnvironmentAnalyzer rfEnvironmentAnalyzer;

bool RfEnvironmentAnalyzer::setRange(int a,int b){if(a<0||b>125||a>b)return false;rfEnvironmentState.config.minChannel=a;rfEnvironmentState.config.maxChannel=b;return true;}
bool RfEnvironmentAnalyzer::setWindow(int s){if(s!=1&&s!=5&&s!=10&&s!=30&&s!=60)return false;rfEnvironmentState.config.sampleWindowSeconds=s;return true;}
bool RfEnvironmentAnalyzer::start(RfEnvMode m){
    if(rfEnvironmentState.running||!radioManager.hasAnyRadio())return false;
    radioManager.stopAll(); radioManager.enterRxMode(); rfEnvironmentState.resetRuntime();
    rfEnvironmentState.mode=m; rfEnvironmentState.running=true; rfEnvironmentState.startedMs=millis(); stopRequested=false;
    if(xTaskCreatePinnedToCore(taskEntry,"RFEnvironment",6144,this,2,&task,0)!=pdPASS){rfEnvironmentState.running=false;rfEnvironmentState.mode=RF_ENV_IDLE;task=nullptr;return false;}return true;
}
void RfEnvironmentAnalyzer::stop(){stopRequested=true;}
bool RfEnvironmentAnalyzer::stopAndWait(uint32_t timeoutMs){
    stopRequested=true;const uint32_t started=millis();
    while(task&&millis()-started<timeoutMs)vTaskDelay(pdMS_TO_TICKS(2));
    return task==nullptr;
}
void RfEnvironmentAnalyzer::service(){if(task && !rfEnvironmentState.running) task=nullptr;}
void RfEnvironmentAnalyzer::taskEntry(void* p){static_cast<RfEnvironmentAnalyzer*>(p)->run();vTaskDelete(nullptr);}
void RfEnvironmentAnalyzer::run(){
    uint32_t lastRateMs=millis(), rateSamples=0, eventId=0,windowStarted=millis();
    uint32_t windowHits[TOTAL_CHANNELS]={},windowSamples[TOTAL_CHANNELS]={};
    while(!stopRequested){
        uint32_t cycleStart=micros(); uint8_t minCh=rfEnvironmentState.config.minChannel,maxCh=rfEnvironmentState.config.maxChannel;
        for(uint16_t ch=minCh;ch<=maxCh&&!stopRequested;ch++){
            uint16_t hits=0,samples=0; if(!radioManager.sampleCarrier(static_cast<uint8_t>(ch),32,hits,samples)){vTaskDelay(1);continue;}
            rateSamples+=samples;windowHits[ch]+=hits;windowSamples[ch]+=samples;RfChannelStats &s=rfEnvironmentState.channels[ch]; uint8_t occ=RfEnvironmentMath::percent(windowHits[ch],windowSamples[ch]), baseline=s.movingAverage;
            s.sampleCount+=samples;s.carrierHits+=hits;s.occupancy=occ;s.movingAverage=s.sampleCount==samples?occ:RfEnvironmentMath::ema(baseline,occ,rfEnvironmentState.config.emaAlpha);
            if(occ>s.peak)s.peak=occ;if(occ<s.minimum)s.minimum=occ;if(occ>s.maximum)s.maximum=occ;
            if(hits){s.lastActivityMs=millis();if(s.consecutiveActive<65535)s.consecutiveActive++;}else s.consecutiveActive=0;
            s.persistence=static_cast<uint8_t>(min<uint32_t>(100,(s.consecutiveActive*100U)/max<uint16_t>(1,rfEnvironmentState.config.sampleWindowSeconds*4U)));
            uint16_t neighborTotal=0;uint8_t n=0;for(int d=-2;d<=2;d++)if(d&&ch+d>=minCh&&ch+d<=maxCh){neighborTotal+=rfEnvironmentState.channels[ch+d].movingAverage;n++;}const uint8_t neighbors=n?neighborTotal/n:0;
            uint8_t burstActivity=min<uint16_t>(100,s.burstCount*10);s.score=RfEnvironmentMath::interferenceScore(s.movingAverage,s.persistence,burstActivity,neighbors);
            if(occ>baseline+rfEnvironmentState.config.burstThreshold&&baseline>0){RfBurstEvent &e=rfEnvironmentState.events[rfEnvironmentState.eventHead];e.id=++eventId;e.timestampMs=millis();e.channel=ch;e.frequencyMHz=2400+ch;e.peak=occ;e.baseline=baseline;e.delta=occ-baseline;e.durationMs=max<uint32_t>(1,(micros()-cycleStart)/1000U);e.severity=e.delta>=50?RF_BURST_HIGH:e.delta>=30?RF_BURST_MEDIUM:RF_BURST_LOW;s.burstCount++;rfEnvironmentState.eventHead=(rfEnvironmentState.eventHead+1)%RF_ENV_BURST_EVENTS;if(rfEnvironmentState.eventCount<RF_ENV_BURST_EVENTS)rfEnvironmentState.eventCount++;}
            if((ch&7)==0)vTaskDelay(1);
        }
        if(stopRequested)break;
        const uint32_t nowWindow=millis();if(nowWindow-windowStarted>=rfEnvironmentState.config.sampleWindowSeconds*1000UL){for(uint8_t ch=minCh;ch<=maxCh;ch++){rfEnvironmentState.history[rfEnvironmentState.historyHead][ch]=rfEnvironmentState.channels[ch].occupancy;windowHits[ch]=windowSamples[ch]=0;}rfEnvironmentState.historyHead=(rfEnvironmentState.historyHead+1)%rfEnvironmentState.config.historyDepth;if(rfEnvironmentState.historyCount<rfEnvironmentState.config.historyDepth)rfEnvironmentState.historyCount++;windowStarted=nowWindow;}
        rfEnvironmentState.completedCycles++;rfEnvironmentState.lastCycleUs=micros()-cycleStart;if(rfEnvironmentState.lastCycleUs>rfEnvironmentState.maxCycleUs)rfEnvironmentState.maxCycleUs=rfEnvironmentState.lastCycleUs;
        uint32_t now=millis();if(now-lastRateMs>=1000){rfEnvironmentState.samplesPerSecond=(rateSamples*1000U)/(now-lastRateMs);rateSamples=0;lastRateMs=now;}
    }
    sessionRecorder.recordEnvironmentSummary(rfEnvironmentState, rfEnvironmentState.mode==RF_ENV_COMPARE?"compare":"occupancy");
    rfEnvironmentState.running=false;rfEnvironmentState.mode=RF_ENV_IDLE;task=nullptr;radioManager.enterRxMode();
}
