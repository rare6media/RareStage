#pragma once
#include <algorithm>
#include <cmath>
// Seconds, deliberately independent of beat-phase corrections. A corner approach blends
// the continuing orbit into an exact corner with zero terminal velocity and acceleration.
struct CornerChoreography {
    float x{},y{},vx{.16f},vy{.119f},lastBeat{-1},startBeat{},targetBeat{},targetX{},targetY{},hitBeat{-100};
    float orbitOrigin{},originX{},originY{};bool fromCorner{},travelling{},predictionLatched{},holdCorner{};
    unsigned drops{};
    void orbit(float seconds,float&px,float&py,float&dx,float&dy)const{
        const float t=std::max(0.f,seconds-orbitOrigin),ramp=1-std::exp(-t*2),phase=fromCorner?t-.5f*ramp:t;
        if(fromCorner){px=originX*std::cos(phase*.16f);py=originY*std::cos(phase*.119f);dx=-originX*.16f*std::sin(phase*.16f)*ramp;dy=-originY*.119f*std::sin(phase*.119f)*ramp;}
        else{px=std::sin(phase*.16f);py=std::sin(phase*.119f);dx=.16f*std::cos(phase*.16f);dy=.119f*std::cos(phase*.119f);}
    }
    void aim(float seconds,float arrival,bool hold=false){
        if(travelling){holdCorner|=hold;return;}
        if(holdCorner)return;
        travelling=true;holdCorner=hold;startBeat=seconds;targetBeat=std::max(seconds+1.5f,arrival);
        float px,py,dx,dy;orbit(targetBeat,px,py,dx,dy);targetX=px>=0?1.f:-1.f;targetY=py>=0?1.f:-1.f;
    }
    void release(float seconds){if(!holdCorner)return;holdCorner=false;if(travelling)return;orbitOrigin=seconds;originX=x;originY=y;fromCorner=true;}
    void update(float seconds,bool predicted,float lead,unsigned dropCount){
        if(lastBeat<0){lastBeat=seconds;orbitOrigin=seconds;drops=dropCount;}
        lastBeat=seconds;
        if(predicted&&!predictionLatched&&!travelling&&!holdCorner&&seconds-hitBeat>3&&lead>=1.5f){aim(seconds,seconds+std::min(lead,6.f));predictionLatched=true;}
        if(!predicted)predictionLatched=false;
        // An unanticipated drop lights the environment; it never changes the mark's path.
        if(dropCount!=drops){drops=dropCount;hitBeat=seconds;}
        if(holdCorner&&!travelling){vx=vy=0;return;}
        float px,py,dx,dy;orbit(seconds,px,py,dx,dy);
        if(travelling){
            const float duration=targetBeat-startBeat,u=std::clamp((seconds-startBeat)/duration,0.f,1.f);
            const float s=u*u*u*(u*(u*6-15)+10),ds=30*u*u*(u-1)*(u-1)/duration;
            x=std::lerp(px,targetX,s);y=std::lerp(py,targetY,s);
            vx=(1-s)*dx+ds*(targetX-px);vy=(1-s)*dy+ds*(targetY-py);
            if(u>=1){travelling=false;hitBeat=seconds;orbitOrigin=seconds;originX=x;originY=y;fromCorner=true;vx=vy=0;}
        }else{x=px;y=py;vx=dx;vy=dy;}
    }
    bool arrived()const{return holdCorner&&!travelling;}
    float impact(float seconds)const{return std::exp(-std::max(0.f,seconds-hitBeat)*5.f);}
};
