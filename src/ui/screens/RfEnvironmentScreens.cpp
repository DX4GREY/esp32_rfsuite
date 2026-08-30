#include "ui/DisplayManager.h"
#include "ui/DisplaySupport.h"
#include "core/RfEnvironmentState.h"
#include "core/RfEnvironmentMath.h"
#include "services/RfEnvironmentAnalyzer.h"
#include "drivers/RadioManager.h"
#include "services/RfAuthorizedProbe.h"
using namespace DisplayUi;

void DisplayManager::renderRfEnvironmentScreen() {
    const uint16_t bg=SPECTRUM_CARD_BG, accent=SPECTRUM_ACCENT;
    (void)bg;
    // Input changes also set needRedraw, so it cannot identify a page-layout
    // transition. Keep an independent layout flag: static chrome is drawn once
    // after entering the page, while subsequent renders update dirty regions.
    const bool layout = !envLayoutDrawn;
    auto clearLine = [&](int y) { tft.fillRect(2, y, 156, 9, ST77XX_BLACK); };
    if(appState.appMode==APP_MODE_ENV_OCCUPANCY){
        if(layout){drawModernHeader("OCCUPANCY",accent);tft.fillRoundRect(3,16,154,86,5,SPECTRUM_CARD_BG);tft.drawRoundRect(3,16,154,86,5,SPECTRUM_BORDER);}
        uint8_t top[5];rfEnvironmentState.topChannels(top,5);if(layout){tft.fillRect(7,19,146,11,SPECTRUM_CARD_BG);tft.setCursor(9,21);tft.setTextColor(ST77XX_GRAY,SPECTRUM_CARD_BG);tft.printf("CH %u-%u",rfEnvironmentState.config.minChannel,rfEnvironmentState.config.maxChannel);tft.setCursor(104,21);tft.printf("WIN %us",rfEnvironmentState.config.sampleWindowSeconds);}
        for(int i=0;i<5;i++){uint8_t value=rfEnvironmentState.channels[top[i]].movingAverage;if(!layout&&previousEnvTopChannels[i]==top[i]&&previousEnvTopLevels[i]==value)continue;int y=32+i*11;tft.fillRect(8,y,144,9,SPECTRUM_CARD_BG);tft.setCursor(9,y+1);tft.setTextColor(i==0?SPECTRUM_HIGH:ST77XX_WHITE,SPECTRUM_CARD_BG);tft.printf("%d CH%3u",i+1,top[i]);tft.fillRoundRect(55,y+2,72,5,2,SPECTRUM_BORDER);if(value)tft.fillRoundRect(55,y+2,(value*72)/100,5,2,getSignalColor(value));tft.setCursor(132,y+1);tft.printf("%3u",value);previousEnvTopChannels[i]=top[i];previousEnvTopLevels[i]=value;}
        uint8_t avg=rfEnvironmentState.averageOccupancy(),score=rfEnvironmentState.overallScore();if(layout||previousEnvAverage!=avg){tft.fillRoundRect(8,89,68,10,3,SPECTRUM_HEADER_BG);tft.setCursor(12,91);tft.setTextColor(ST77XX_WHITE,SPECTRUM_HEADER_BG);tft.printf("AVG %u%%",avg);previousEnvAverage=avg;}if(layout||previousEnvScore!=score){tft.fillRoundRect(82,89,70,10,3,SPECTRUM_HEADER_BG);tft.setCursor(86,91);tft.setTextColor(ST77XX_WHITE,SPECTRUM_HEADER_BG);tft.printf("SCORE %u",score);previousEnvScore=score;}
    } else if(appState.appMode==APP_MODE_ENV_HEATMAP){
        if(layout){drawModernHeader("HEATMAP",accent);tft.fillRoundRect(21,16,136,80,4,SPECTRUM_CARD_BG);tft.drawRoundRect(21,16,136,80,4,SPECTRUM_BORDER);tft.setCursor(4,18);tft.setTextColor(ST77XX_GRAY,ST77XX_BLACK);tft.print("HI");tft.setCursor(4,86);tft.print("LO");tft.setCursor(24,97);tft.print("RING HISTORY");}const uint16_t colors[6]={SPECTRUM_CARD_BG,ST77XX_DARKGRAY,SPECTRUM_LOW,SPECTRUM_MID,SPECTRUM_HIGH,SPECTRUM_CRITICAL};
        const int cols=rfEnvironmentState.historyCount;const int newest=(rfEnvironmentState.historyHead+RF_ENV_HISTORY_BUCKETS-1)%RF_ENV_HISTORY_BUCKETS;const int firstCol=layout?0:newest;const int lastCol=layout?cols:firstCol+1;for(int x=firstCol;x<lastCol;x++){uint8_t bi=x;for(int row=0;row<21;row++){int a=rfEnvironmentState.config.minChannel+row*(rfEnvironmentState.config.maxChannel-rfEnvironmentState.config.minChannel+1)/21,b=rfEnvironmentState.config.minChannel+(row+1)*(rfEnvironmentState.config.maxChannel-rfEnvironmentState.config.minChannel+1)/21;uint16_t sum=0,n=0;for(int ch=a;ch<b;ch++){sum+=rfEnvironmentState.history[bi][ch];n++;}uint8_t v=n?sum/n:0;tft.fillRect(25+x*4,19+(20-row)*3,4,3,colors[v?min(5,1+v/20):0]);}}tft.fillRect(24,83,130,3,SPECTRUM_CARD_BG);if(cols)tft.fillTriangle(25+newest*4,85,29+newest*4,85,27+newest*4,82,SPECTRUM_ACCENT);
        if(layout){for(int i=0;i<5;i++)tft.fillRect(61+i*10,98,8,4,colors[i+1]);}
    } else if(appState.appMode==APP_MODE_ENV_BURSTS){
        if(layout){drawModernHeader("BURSTS",SPECTRUM_HIGH);tft.fillRoundRect(4,17,152,84,5,SPECTRUM_CARD_BG);tft.drawRoundRect(4,17,152,84,5,SPECTRUM_BORDER);}tft.fillRect(8,20,144,76,SPECTRUM_CARD_BG);tft.setCursor(10,21);tft.setTextColor(ST77XX_GRAY,SPECTRUM_CARD_BG);tft.printf("EVENTS %u  VIEW %u/%u",rfEnvironmentState.eventCount,rfEnvironmentState.eventCount?envEventScroll+1:0,rfEnvironmentState.eventCount);
        if(rfEnvironmentState.eventCount){uint8_t idx=(rfEnvironmentState.eventHead+RF_ENV_BURST_EVENTS-1-envEventScroll)%RF_ENV_BURST_EVENTS;const RfBurstEvent&e=rfEnvironmentState.events[idx];uint16_t sev=e.severity==RF_BURST_HIGH?SPECTRUM_CRITICAL:e.severity==RF_BURST_MEDIUM?SPECTRUM_HIGH:SPECTRUM_LOW;tft.fillRoundRect(108,34,40,12,3,sev);tft.setCursor(112,37);tft.setTextColor(ST77XX_BLACK,sev);tft.print(e.severity==RF_BURST_HIGH?"HIGH":e.severity==RF_BURST_MEDIUM?"MED":"LOW");tft.setCursor(10,36);tft.setTextColor(ST77XX_WHITE,SPECTRUM_CARD_BG);tft.printf("#%lu CH%u",(unsigned long)e.id,e.channel);tft.setCursor(10,51);tft.printf("%u MHz DUR %lums",e.frequencyMHz,(unsigned long)e.durationMs);tft.setCursor(10,66);tft.printf("BASE %u%% PEAK %u%%",e.baseline,e.peak);tft.fillRoundRect(10,82,138,7,3,SPECTRUM_BORDER);tft.fillRoundRect(10,82,(e.peak*138)/100,7,3,sev);tft.setCursor(10,92);tft.setTextColor(sev,SPECTRUM_CARD_BG);tft.printf("DELTA +%u%%",e.delta);}else{tft.setCursor(46,57);tft.setTextColor(ST77XX_GRAY,SPECTRUM_CARD_BG);tft.print("NO BURSTS YET");}
    } else if(appState.appMode==APP_MODE_ENV_COMPARE){
        if(layout){drawModernHeader("CH COMPARE",accent);tft.fillRoundRect(3,16,154,86,5,SPECTRUM_CARD_BG);tft.drawRoundRect(3,16,154,86,5,SPECTRUM_BORDER);}for(int i=0;i<rfEnvironmentState.config.compareCount;i++){uint8_t ch=rfEnvironmentState.config.compareChannels[i];const RfChannelStats&s=rfEnvironmentState.channels[ch];if(!layout&&previousCompareChannels[i]==ch&&previousCompareLevels[i]==s.movingAverage&&previousCompareScores[i]==s.score)continue;int y=20+i*20;uint16_t color=getSignalColor(s.score);tft.fillRect(8,y,144,17,SPECTRUM_CARD_BG);tft.fillRoundRect(9,y,32,15,3,SPECTRUM_HEADER_BG);tft.setCursor(14,y+4);tft.setTextColor(ST77XX_WHITE,SPECTRUM_HEADER_BG);tft.printf("CH%u",ch);tft.fillRoundRect(47,y+2,70,7,3,SPECTRUM_BORDER);if(s.movingAverage)tft.fillRoundRect(47,y+2,(s.movingAverage*70)/100,7,3,getSignalColor(s.movingAverage));tft.setCursor(122,y+1);tft.setTextColor(ST77XX_WHITE,SPECTRUM_CARD_BG);tft.printf("%u%%",s.movingAverage);tft.setCursor(47,y+11);tft.setTextColor(color,SPECTRUM_CARD_BG);tft.printf("%s  S%u",rfEnvironmentState.scoreLabel(s.score),s.score);previousCompareChannels[i]=ch;previousCompareLevels[i]=s.movingAverage;previousCompareScores[i]=s.score;}
    } else if(appState.appMode==APP_MODE_ENV_STATUS){
        if(layout){drawModernHeader("RF STATUS",accent);tft.fillRoundRect(4,17,152,84,5,SPECTRUM_CARD_BG);tft.drawRoundRect(4,17,152,84,5,SPECTRUM_BORDER);tft.setCursor(10,22);tft.setTextColor(ST77XX_GRAY,SPECTRUM_CARD_BG);tft.print("INTERFERENCE SCORE");tft.setCursor(20,89);tft.print("RELATIVE ACTIVITY ONLY");}uint8_t score=rfEnvironmentState.overallScore(),top[1];rfEnvironmentState.topChannels(top,1);uint16_t bursts=0;for(int i=0;i<TOTAL_CHANNELS;i++)bursts+=rfEnvironmentState.channels[i].burstCount;uint8_t occ=rfEnvironmentState.averageOccupancy();uint16_t color=getSignalColor(score);if(layout||previousEnvScore!=score){tft.fillRect(108,19,42,18,SPECTRUM_CARD_BG);tft.setCursor(112,20);tft.setTextColor(color,SPECTRUM_CARD_BG);tft.setTextSize(2);tft.print(score);tft.setTextSize(1);tft.fillRoundRect(10,39,138,9,4,SPECTRUM_BORDER);if(score)tft.fillRoundRect(10,39,(score*138)/100,9,4,color);tft.fillRoundRect(10,53,138,13,3,SPECTRUM_HEADER_BG);tft.setCursor(16,56);tft.setTextColor(color,SPECTRUM_HEADER_BG);tft.print(rfEnvironmentState.scoreLabel(score));previousEnvScore=score;}if(layout||previousEnvAverage!=occ){tft.fillRect(8,70,50,10,SPECTRUM_CARD_BG);tft.setCursor(10,72);tft.setTextColor(ST77XX_WHITE,SPECTRUM_CARD_BG);tft.printf("OCC %u%%",occ);previousEnvAverage=occ;}if(layout||previousEnvBursts!=bursts){tft.fillRect(59,70,58,10,SPECTRUM_CARD_BG);tft.setCursor(62,72);tft.setTextColor(ST77XX_WHITE,SPECTRUM_CARD_BG);tft.printf("BURST %u",bursts);previousEnvBursts=bursts;}if(layout||previousEnvPeakChannel!=top[0]){tft.fillRect(117,70,34,10,SPECTRUM_CARD_BG);tft.setCursor(119,72);tft.setTextColor(ST77XX_WHITE,SPECTRUM_CARD_BG);tft.printf("CH%u",top[0]);previousEnvPeakChannel=top[0];}
    } else if(appState.appMode==APP_MODE_ENV_BEFORE_AFTER){
        if(layout){drawModernHeader("BEFORE/AFTER",accent);tft.fillRoundRect(3,16,154,86,5,SPECTRUM_CARD_BG);tft.drawRoundRect(3,16,154,86,5,SPECTRUM_BORDER);}const auto&a=rfEnvironmentState.before;const auto&b=rfEnvironmentState.after;const bool snapshotsChanged=layout||previousBeforeCapturedMs!=a.capturedMs||previousAfterCapturedMs!=b.capturedMs;if(snapshotsChanged){tft.fillRect(8,20,144,77,SPECTRUM_CARD_BG);if(!a.valid||!b.valid){tft.fillRoundRect(20,37,120,34,5,SPECTRUM_HEADER_BG);tft.setCursor(a.valid?38:32,45);tft.setTextColor(SPECTRUM_ACCENT,SPECTRUM_HEADER_BG);tft.print(a.valid?"CAPTURE AFTER":"CAPTURE BEFORE");tft.setCursor(42,59);tft.setTextColor(ST77XX_GRAY,SPECTRUM_HEADER_BG);tft.print("PRESS A");}else{tft.setCursor(10,22);tft.setTextColor(ST77XX_GRAY,SPECTRUM_CARD_BG);tft.print("METRIC");tft.setCursor(74,22);tft.print("BEFORE > AFTER");const char* names[3]={"AVG","PEAK","SCORE"};uint16_t av[3]={a.average,a.peak,a.score},bv[3]={b.average,b.peak,b.score};for(int i=0;i<3;i++){int y=36+i*14;tft.setCursor(10,y);tft.setTextColor(ST77XX_WHITE,SPECTRUM_CARD_BG);tft.print(names[i]);tft.setCursor(70,y);tft.printf("%3u  >  %3u",av[i],bv[i]);}}previousBeforeCapturedMs=a.capturedMs;previousAfterCapturedMs=b.capturedMs;previousSnapshotChannel=0xFF;}if(a.valid&&b.valid&&(snapshotsChanged||previousSnapshotChannel!=envBandChannel)){int cd=(int)b.channelOccupancy[envBandChannel]-a.channelOccupancy[envBandChannel];uint16_t dc=cd>0?SPECTRUM_HIGH:cd<0?SPECTRUM_LOW:ST77XX_GRAY;tft.fillRoundRect(9,80,142,16,4,SPECTRUM_HEADER_BG);tft.setCursor(14,84);tft.setTextColor(dc,SPECTRUM_HEADER_BG);tft.printf("CH%u %u>%u DELTA %+d%%",envBandChannel,a.channelOccupancy[envBandChannel],b.channelOccupancy[envBandChannel],cd);previousSnapshotChannel=envBandChannel;}
    } else if(appState.appMode==APP_MODE_ENV_BAND_INFO){
        if(layout){
            drawModernHeader("BAND INFO",accent);
            tft.fillRoundRect(3,16,154,86,5,SPECTRUM_CARD_BG);
            tft.drawRoundRect(3,16,154,86,5,SPECTRUM_BORDER);
            tft.setCursor(10,47);
            tft.setTextColor(ST77XX_GRAY,SPECTRUM_CARD_BG);
            tft.print("POSSIBLE BAND REGIONS");
        }
        if(layout||previousEnvBandChannel!=envBandChannel){
            const uint16_t f=RfEnvironmentMath::frequencyMHz(envBandChannel);
            // Dirty redraw: only the value card and the three result chips can
            // change while browsing channels. The outer card, title, label,
            // and footer remain untouched.
            tft.fillRoundRect(10,20,138,22,5,SPECTRUM_HEADER_BG);
            tft.setCursor(16,24);
            tft.setTextColor(SPECTRUM_ACCENT,SPECTRUM_HEADER_BG);
            tft.printf("CH %u",envBandChannel);
            tft.setCursor(91,24);
            tft.setTextColor(ST77XX_WHITE,SPECTRUM_HEADER_BG);
            tft.printf("%u MHz",f);

            int8_t wifi=RfEnvironmentMath::wifiChannelForMHz(f);
            const char* tags[3];
            char wifiTag[20];
            if(wifi>0)snprintf(wifiTag,sizeof(wifiTag),"WIFI CH%d OVERLAP",wifi);
            else snprintf(wifiTag,sizeof(wifiTag),"WIFI OUTSIDE");
            tags[0]=wifiTag;
            tags[1]=(f>=2402&&f<=2480)?"BLE / BT REGION":"BLE / BT OUTSIDE";
            int8_t z=RfEnvironmentMath::zigbeeChannelForMHz(f);
            char zigTag[22];
            if(z>0)snprintf(zigTag,sizeof(zigTag),"ZIGBEE CH%d CENTER",z);
            else snprintf(zigTag,sizeof(zigTag),f>=2405&&f<=2480?"ZIGBEE OVERLAP":"ZIGBEE OUTSIDE");
            tags[2]=zigTag;
            for(int i=0;i<3;i++){
                const int y=60+i*13;
                const uint16_t tc=strstr(tags[i],"OUTSIDE")?ST77XX_GRAY:(i==0?SPECTRUM_HIGH:i==1?SPECTRUM_ACCENT:SPECTRUM_LOW);
                tft.fillRoundRect(10,y,138,10,3,SPECTRUM_HEADER_BG);
                tft.setCursor(15,y+2);
                tft.setTextColor(tc,SPECTRUM_HEADER_BG);
                tft.print(tags[i]);
            }
            previousEnvBandChannel=envBandChannel;
        }
    } else {if(layout){drawModernHeader("AUTH PROBE",SPECTRUM_HIGH);tft.fillRoundRect(3,16,154,87,5,SPECTRUM_CARD_BG);tft.drawRoundRect(3,16,154,87,5,SPECTRUM_HIGH);tft.fillRoundRect(46,18,68,11,3,DISPLAY_ACTIVE_BG);tft.setCursor(50,20);tft.setTextColor(SPECTRUM_HIGH,DISPLAY_ACTIVE_BG);tft.print("LAB USE ONLY");}
#if RF_LAB_TX_ENABLED
        const auto&c=rfEnvironmentState.config;const char* labels[5]={"CHANNEL","INTERVAL","PACKETS","DURATION",rfAuthorizedProbe.isRunning()?"STOP PROBE":"START PROBE"};char values[4][12];snprintf(values[0],12,"CH %u",c.probeChannel);snprintf(values[1],12,"%u ms",c.probeIntervalMs);snprintf(values[2],12,"%u",c.probePacketCount);snprintf(values[3],12,"%u sec",c.probeMaxDurationSeconds);for(int i=0;i<5;i++){int y=31+i*13;bool sel=probeSelection==i;uint16_t rowBg=sel?SPECTRUM_HEADER_BG:SPECTRUM_CARD_BG;tft.fillRoundRect(8,y,144,11,3,rowBg);if(sel)tft.fillRect(8,y,3,11,SPECTRUM_ACCENT);tft.setCursor(14,y+2);tft.setTextColor(sel?ST77XX_WHITE:ST77XX_GRAY,rowBg);tft.print(labels[i]);if(i<4){tft.setCursor(104,y+2);tft.setTextColor(sel?SPECTRUM_ACCENT:ST77XX_WHITE,rowBg);tft.print(values[i]);}else{tft.setCursor(140,y+2);tft.setTextColor(rfAuthorizedProbe.isRunning()?SPECTRUM_CRITICAL:SPECTRUM_LOW,rowBg);tft.print(">");}}tft.fillRect(8,97,144,4,SPECTRUM_BORDER);int progress=c.probePacketCount?min<int>(144,(rfAuthorizedProbe.packetsSent()*144UL)/c.probePacketCount):0;if(progress)tft.fillRect(8,97,progress,4,rfAuthorizedProbe.isRunning()?SPECTRUM_HIGH:SPECTRUM_LOW);
#else
        tft.fillRoundRect(15,42,130,38,5,SPECTRUM_HEADER_BG);tft.setCursor(43,50);tft.setTextColor(SPECTRUM_LOW,SPECTRUM_HEADER_BG);tft.print("RX ONLY BUILD");tft.setCursor(27,65);tft.setTextColor(ST77XX_GRAY,SPECTRUM_HEADER_BG);tft.print("TX code not compiled");
#endif
    }
    if(layout)drawModernFooter(appState.appMode==APP_MODE_ENV_PROBE?"U/D SEL":"U/D VIEW",appState.appMode==APP_MODE_ENV_PROBE?"A ACTION":(rfEnvironmentState.running?"A STOP":"A START"),"B BACK");
    else if(!envRunningStatusValid||previousEnvRunning!=rfEnvironmentState.running)drawFooterChip(56,49,rfEnvironmentState.running?"A STOP":"A START");
    previousEnvRunning=rfEnvironmentState.running;envRunningStatusValid=true;
    envLayoutDrawn=true;
}
