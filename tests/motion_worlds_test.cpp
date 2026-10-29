#include "motion_poses.h"
#include <iostream>
using namespace rarestage;
int main(){
 bool passed=true;auto check=[&](const char* name,bool value){std::cout<<name<<": "<<(value?"PASS":"FAIL")<<'\n';passed&=value;};
 auto difference=[](motion::Pose a,motion::Pose b){return std::hypot(a.x-b.x,a.y-b.y)+std::abs(a.a-b.a)+std::abs(a.b-b.b)+std::abs(a.angle-b.angle)*.1f+std::abs(a.round-b.round)*.1f+std::abs(a.span-b.span)*.1f;};
 bool finite=true,continuous=true,distinct=true,endpoints=true;float maxStep=0;
 for(int scene=38;scene<42;++scene)for(int mode=0;mode<10;++mode){
  for(float aspect:{.5625f,1.777778f,2.666667f})for(float start:{0.f,8192.f})for(int frame=0;frame<960;++frame)for(int id=0;id<16;++id){
   float beat=start+frame/60.f;auto p=motion::pose(scene,mode,id,beat,aspect),q=motion::pose(scene,mode,id,beat+1.f/60,aspect);
   finite&=std::isfinite(p.x+p.y+p.angle+p.a+p.b+p.round+p.span)&&p.a>0&&p.b>0&&p.round>=0&&p.round<=1;
   maxStep=std::max(maxStep,difference(p,q));
  }
  for(int other=mode+1;other<10;++other){double total=0;for(float beat:{0.f,5.f,11.f,19.f})for(int id=0;id<16;++id)total+=difference(motion::pose(scene,mode,id,beat,1.777778f),motion::pose(scene,other,id,beat,1.777778f));distinct&=total/64>.015;}
 }
 for(int from=38;from<42;++from)for(int to=38;to<42;++to)if(from!=to&&motion::Correspondence(from,to))for(float beat:{5.f,8192.f})for(int a=0;a<10;++a)for(int b=0;b<10;++b){
  show::PhraseFrame old{float(a),float(a),1,1,1,1,0,0},now{float(b),float(b),1,1,1,1,0,0};
  std::array<std::array<float,4>,40> source{},destination{},joined{};
  motion::prepare(from,from,old,old,beat,1.777778f,1,source);motion::prepare(to,to,now,now,beat,1.777778f,1,destination);
  motion::prepare(to,from,now,old,beat,1.777778f,0,joined);endpoints&=source==joined;
  motion::prepare(to,from,now,old,beat,1.777778f,1,joined);endpoints&=destination==joined;
  for(int id=0;id<16;++id){auto p=motion::mix(motion::program(from,id,old,beat,1.777778f),motion::program(to,id,now,beat,1.777778f),.5f);auto q=motion::mix(motion::program(from,id,old,beat+1.f/60,1.777778f),motion::program(to,id,now,beat+1.f/60,1.777778f),.5f);continuous&=difference(p,q)<.03;}
 }
 check("finite_poses_across_aspects_and_long_clocks",finite);check("continuous_piece_paths",continuous&&maxStep<.03f);check("forty_geometrically_distinct_programs",distinct);check("authored_pairs_preserve_exact_piece_endpoints",endpoints);
 std::cout<<"maximum_step="<<maxStep<<'\n';return passed?0:1;
}
