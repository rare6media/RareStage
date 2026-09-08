#pragma once
struct CornerChoreography {
    float x{},y{},vx{.11f},vy{.08f},lastBeat{-1},startBeat{},targetBeat{},startX{},startY{},startVx{},startVy{},targetX{},targetY{},hitBeat{-100};
    unsigned drops{};bool travelling{},predictionLatched{};
    void aim(float beat,float arrival){
        travelling=true;startBeat=beat;targetBeat=std::max(beat+.5f,arrival);startX=x;startY=y;startVx=vx;startVy=vy;
        targetX=x>=0?1.f:-1.f;targetY=y>=0?1.f:-1.f;
    }
    void update(float beat,bool predicted,float lead,unsigned dropCount){
        if(lastBeat<0){lastBeat=beat;drops=dropCount;}
        const float dt=std::clamp(beat-lastBeat,0.f,.25f);lastBeat=beat;
        if(predicted&&!predictionLatched&&!travelling&&beat-hitBeat>2){aim(beat,beat+std::clamp(lead,.5f,8.f));predictionLatched=true;}
        if(!predicted)predictionLatched=false;
        if(dropCount!=drops){drops=dropCount;if(!travelling&&beat-hitBeat>1)aim(beat,beat+1.f);}
        if(travelling){
            const float duration=targetBeat-startBeat,u=std::clamp((beat-startBeat)/duration,0.f,1.f);
            const float a=2*u*u*u-3*u*u+1,b=u*u*u-2*u*u+u,c=-2*u*u*u+3*u*u;
            x=std::clamp(a*startX+b*duration*startVx+c*targetX,-1.f,1.f);y=std::clamp(a*startY+b*duration*startVy+c*targetY,-1.f,1.f);
            if(u>=1){travelling=false;hitBeat=beat;vx=-targetX*.11f;vy=-targetY*.08f;}
        }else{
            x+=vx*dt;y+=vy*dt;
            if(x>1){x=2-x;vx=-std::abs(vx);}if(x<-1){x=-2-x;vx=std::abs(vx);}
            if(y>1){y=2-y;vy=-std::abs(vy);}if(y<-1){y=-2-y;vy=std::abs(vy);}
        }
    }
    float impact(float beat)const{return std::exp(-std::max(0.f,beat-hitBeat)*8.f);}
};
