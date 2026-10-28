// Members of ConsoleApp. Browsing and previewing do not change live output.
std::filesystem::path catalogRoot()const{wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,DWORD(std::size(path)));return std::filesystem::path(path).parent_path();}
void loadFavorites(){std::ifstream input(catalogRoot()/"catalog-favorites.txt");int id;while(input>>id)if(id>=0&&id<int(WorldNames.size()))catalogFavorites_[id]=true;}
void saveFavorites(){std::ofstream output(catalogRoot()/"catalog-favorites.txt");for(int id=0;id<int(WorldNames.size());++id)if(catalogFavorites_[id])output<<id<<'\n';if(!output.good())statusText_=L"Favorite is active for this session; this folder could not save it.";}
void paintCatalog(HDC dc,RECT body){
 using namespace rarestage::show;
 const int pad=scale(14),gap=scale(8),leftWidth=int((body.right-body.left)*.51f);
 RECT left{body.left,body.top,body.left+leftWidth,body.bottom},right{left.right+scale(12),body.top,body.right,body.bottom};
 fillRound(dc,left,11,Panel,Line);fillRound(dc,right,11,Panel,Line);
 sectionLabel(dc,L"CATALOG / "+std::to_wstring(SceneCount)+L" SCENES / "+std::to_wstring(NamedProgramCount())+L" NAMED PROGRAMS",left.left+pad,left.top+scale(8),leftWidth-2*pad);
 int y=left.top+scale(38);
 addButton(dc,RECT{left.left+pad,y,left.right-pad-scale(112),y+scale(32)},(catalogEditing_?L"TYPE: ":L"SEARCH: ")+(catalogQuery_.empty()?L"scene or pattern":catalogQuery_),Action::CatalogSearch,0,catalogEditing_,Cyan);
 addButton(dc,RECT{left.right-pad-scale(104),y,left.right-pad,y+scale(32)},L"CLEAR",Action::CatalogClear,0);
 y+=scale(40);const int filterWidth=(leftWidth-2*pad-4*gap)/5;
 const wchar_t* filters[]{L"ALL",L"PANELS",L"RIBBONS",L"DEPTH",L"ATMOS"};
 for(int i=0;i<5;++i)addButton(dc,RECT{left.left+pad+i*(filterWidth+gap),y,left.left+pad+i*(filterWidth+gap)+filterWidth,y+scale(27)},filters[i],Action::CatalogFilter,i,catalogCategory_==i,Cyan);
 y+=scale(35);addButton(dc,RECT{left.left+pad,y,left.left+pad+scale(150),y+scale(27)},catalogOnlyFavorites_?L"FAVORITES ONLY":L"ALL / FAVORITES",Action::CatalogFavorites,0,catalogOnlyFavorites_,Amber);
 auto results=CatalogSearch(catalogQuery_,catalogCategory_,catalogOnlyFavorites_,catalogFavorites_);
 const int rows=std::max(1,int(left.bottom-y-scale(78))/scale(30)),pages=std::max(1,(int(results.size())+rows-1)/rows);catalogPage_=std::clamp(catalogPage_,0,pages-1);
 drawText(dc,std::to_wstring(results.size())+L" matches",RECT{left.left+pad+scale(165),y,left.right-pad,y+scale(27)},smallFont_,Muted);
 y+=scale(35);
 for(int row=0;row<rows&&catalogPage_*rows+row<int(results.size());++row){const int id=results[catalogPage_*rows+row];
  RECT line{left.left+pad,y+row*scale(30),left.right-pad,y+row*scale(30)+scale(25)};
  addButton(dc,RECT{line.left,line.top,line.right-scale(36),line.bottom},std::wstring(Scenes[id].wideName)+(PhraseFamily(id)>=0?L" / "+std::to_wstring(ProgramCount(id)):L" / CONTINUOUS"),Action::CatalogSelect,id,catalogScene_==id,Cyan);
  addButton(dc,RECT{line.right-scale(30),line.top,line.right,line.bottom},catalogFavorites_[id]?L"★":L"☆",Action::CatalogStar,id,catalogFavorites_[id],Amber);
 }
 if(results.empty())drawText(dc,L"No matching scenes. Clear the search or filters.",RECT{left.left+pad,y,left.right-pad,y+scale(45)},bodyFont_,Muted);
 addButton(dc,RECT{left.left+pad,left.bottom-scale(38),left.left+pad+scale(70),left.bottom-scale(12)},L"BACK",Action::CatalogPage,-1);
 addButton(dc,RECT{left.right-pad-scale(70),left.bottom-scale(38),left.right-pad,left.bottom-scale(12)},L"MORE",Action::CatalogPage,1);
 drawText(dc,std::to_wstring(catalogPage_+1)+L" / "+std::to_wstring(pages),RECT{left.left+pad+scale(80),left.bottom-scale(38),left.right-pad-scale(80),left.bottom-scale(12)},smallFont_,Muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
 sectionLabel(dc,Scenes[catalogScene_].wideName,right.left+pad,right.top+scale(8),right.right-right.left-2*pad);
 RECT preview{right.left+pad,right.top+scale(38),right.right-pad,right.top+scale(215)};
 const auto path=catalogRoot()/"assets"/"catalog"/("scene-"+std::to_string(catalogScene_)+"-mode-"+std::to_string(catalogMode_)+".bmp");
 if(path!=catalogPreviewPath_){if(catalogPreview_)DeleteObject(catalogPreview_);catalogPreview_=static_cast<HBITMAP>(LoadImageW(nullptr,path.c_str(),IMAGE_BITMAP,0,0,LR_LOADFROMFILE|LR_CREATEDIBSECTION));catalogPreviewPath_=path;}
 fillRect(dc,preview,Background);
 if(catalogPreview_){BITMAP bmp{};GetObjectW(catalogPreview_,sizeof(bmp),&bmp);HDC src=CreateCompatibleDC(dc);auto old=SelectObject(src,catalogPreview_);int h=preview.bottom-preview.top,w=std::min(preview.right-preview.left,h*bmp.bmWidth/bmp.bmHeight);int x=preview.left+(preview.right-preview.left-w)/2;SetStretchBltMode(dc,HALFTONE);StretchBlt(dc,x,preview.top,w,h,src,0,0,bmp.bmWidth,bmp.bmHeight,SRCCOPY);SelectObject(src,old);DeleteDC(src);}
 else drawText(dc,L"Preview unavailable in this build",preview,bodyFont_,Muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
 y=preview.bottom+scale(7);drawText(dc,L"RENDERED STILL / browsing leaves the live scene playing",RECT{right.left+pad,y,right.right-pad,y+scale(20)},smallFont_,Muted);y+=scale(28);
 if(PhraseFamily(catalogScene_)>=0){const int count=ProgramCount(catalogScene_),width=(right.right-right.left-2*pad-gap)/2;
  for(int mode=0;mode<count;++mode){const int col=mode%2,row=mode/2;addButton(dc,RECT{right.left+pad+col*(width+gap),y+row*scale(32),right.left+pad+col*(width+gap)+width,y+row*scale(32)+scale(27)},PhraseName(catalogScene_,mode),Action::CatalogMode,mode,catalogMode_==mode,Purple);}
 }else drawText(dc,L"Continuous authored choreography / no manual program selector",RECT{right.left+pad,y,right.right-pad,y+scale(50)},smallFont_,Muted);
 y=right.bottom-scale(104);const int half=(right.right-right.left-2*pad-gap)/2;
 addButton(dc,RECT{right.left+pad,y,right.left+pad+half,y+scale(31)},L"CUE SCENE",Action::World,catalogScene_,connected_&&state_.world==catalogScene_,Cyan);
 addButton(dc,RECT{right.left+pad+half+gap,y,right.right-pad,y+scale(31)},L"APPLY PATTERN",Action::CatalogApply,0,false,Purple);
 addButton(dc,RECT{right.left+pad,y+scale(39),right.right-pad,y+scale(69)},state_.programHeld?L"PATTERN HELD / RESUME DIRECTOR":L"DIRECTOR CONTROLS PATTERN",Action::CatalogResume,0,state_.programHeld!=0,Amber);
 drawText(dc,L"Cue the scene first. Apply after its movement settles.",RECT{right.left+pad,right.bottom-scale(28),right.right-pad,right.bottom-scale(7)},smallFont_,Muted);
}
