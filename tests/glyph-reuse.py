#!/usr/bin/env python3
"""Exercise the actual styled lookup + hook body with deterministic font/atlas fixtures."""
from pathlib import Path
import argparse, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('--negative-control',action='store_true');a=p.parse_args()
s=(Path(__file__).resolve().parents[1]/'src/main.cpp').read_text()
lookup=s[s.index('TMP_SpriteCharacter* FindStyledSprite('):s.index('\nvoid DrawTexture(')]
hook=s[s.index('MAKE_HOOK_MATCH(\n    TMP_Text_GetTextElement,'):s.index('\nMAKE_HOOK_MATCH(TMP_Text_SaveSpriteVertexInfo')]
if a.negative_control: hook=hook.replace('if (auto existing = FindStyledSprite(withStyle)) return existing;', '')
# Compile the actual production upload-and-destroy statements too.
upload=s[s.index('    UnityEngine::Graphics::CopyTexture_Region(tex,'):s.index('\n    // add a little bit of spacing',s.index('    UnityEngine::Graphics::CopyTexture_Region(tex,'))]
stubs=r'''
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>
using uint=unsigned;
template<class T> struct by_ref {T& value; by_ref(T& v):value(v){}};
template<class T> struct UnityW {T* value; UnityW(T* v):value(v){} T* ptr(){return value;}};
template<class T> struct Fallbacks : std::vector<T> { operator bool() const { return true; } };
template<class T> struct ListW : std::vector<T> { ListW(const std::vector<T>& v):std::vector<T>(v){} };
struct TMP_TextElement {virtual ~TMP_TextElement()=default;};
struct TMP_SpriteCharacter : TMP_TextElement {uint unicode; explicit TMP_SpriteCharacter(uint u):unicode(u){}};
struct TMP_SpriteGlyph {uint unicode;};
struct Lookup {std::unordered_map<uint,TMP_SpriteCharacter*> entries; bool TryGetValue(uint u,by_ref<TMP_SpriteCharacter*> v){auto i=entries.find(u);v.value=i==entries.end()?nullptr:i->second;return v.value;}};
struct Texture {int width=108,height=108;bool destroyed=false;};
struct TMP_SpriteAsset {struct {int m_value=1;} m_CachedPtr;Lookup lookup; Lookup* m_SpriteCharacterLookup=&lookup; Lookup* spriteCharacterLookupTable=&lookup; Fallbacks<UnityW<TMP_SpriteAsset>> fallbackSpriteAssets;Texture* spriteSheet=nullptr;};
struct TMP_Text{};struct TMP_FontAsset{};using FontStyles=int;using FontWeight=int;
TMP_SpriteAsset* rootEmojiAsset=nullptr;TMP_SpriteAsset* currentEmojiAsset=nullptr;
int currentEmojiIndex=0, created=0,draws=0,copies=0,destroys=0;
int GetStyleOffset(FontStyles s){return 0x10ffff*(1+s);}
std::vector<std::unique_ptr<TMP_SpriteAsset>> assets;
std::vector<std::unique_ptr<TMP_SpriteCharacter>> characters;
TMP_SpriteGlyph* PushSprite(uint key){
 if(currentEmojiIndex>=100){assets.push_back(std::make_unique<TMP_SpriteAsset>());currentEmojiAsset=assets.back().get();rootEmojiAsset->fallbackSpriteAssets.push_back(currentEmojiAsset);currentEmojiIndex=0;}
 ++created;characters.push_back(std::make_unique<TMP_SpriteCharacter>(key));
 currentEmojiAsset->lookup.entries.emplace(key,characters.back().get());
 static TMP_SpriteGlyph glyph;glyph.unicode=key;return &glyph;
}
void DrawTexture(uint,TMP_SpriteGlyph*,FontStyles){++draws;}
TMP_TextElement originalCharacter;
TMP_TextElement* TMP_Text_GetTextElement(TMP_Text*,uint u,TMP_FontAsset*,FontStyles,FontWeight,by_ref<bool>){return u==65?&originalCharacter:nullptr;}
struct Logger {template<class... T> void debug(T...) {}} logger;
#define MAKE_HOOK_MATCH(name,match,ret,...) ret hook_m_##name(__VA_ARGS__)
namespace UnityEngine { struct Graphics {static void CopyTexture_Region(Texture* t,int,int,int,int,int,int,Texture*,int,int,int,int){assert(!t->destroyed);++copies;}};struct Object {static void Destroy(Texture* t){assert(copies==destroys+1);t->destroyed=true;++destroys;}};}
'''
main=r'''
void uploadTexture(Texture* tex){int x=0,y=0;
UPLOAD
}
int main(){
 assert(FindStyledSprite(124)==nullptr);
 assets.push_back(std::make_unique<TMP_SpriteAsset>());rootEmojiAsset=currentEmojiAsset=assets.back().get();
 bool alt=false;
 auto lookup=[&](uint u,int style){return hook_m_TMP_Text_GetTextElement(nullptr,u,nullptr,style,400,by_ref(alt));};
 assert(lookup(65,0)==&originalCharacter && created==0);
 auto pipe=lookup(124,3);assert(pipe && created==1);
 for(int i=0;i<10000;i++)assert(lookup(124,3)==pipe);
 assert(created==1 && draws==1 && assets.size()==1);
 auto plain=lookup(124,0);assert(plain && plain!=pipe && created==2);
 for(uint u=1000;u<1200;u++)assert(lookup(u,0));
 assert(created==202 && assets.size()==3);
 for(int i=0;i<100;i++) {assert(lookup(124,3)==pipe);for(uint u=1000;u<1200;u++)assert(lookup(u,0));}
 assert(created==202 && draws==202 && assets.size()==3);
 rootEmojiAsset->m_CachedPtr.m_value=0; assert(lookup(124,3)==nullptr); rootEmojiAsset->m_CachedPtr.m_value=1;
 Texture temp;uploadTexture(&temp);assert(copies==1 && destroys==1 && temp.destroyed);
 std::cout<<"PASS: actual hook/lookup + upload disposal; 30,100 repeats, style variants, original font preference, 3 atlas sheets\n";
}
'''.replace('UPLOAD',upload)
with tempfile.TemporaryDirectory(prefix='bs1451-unicode-reuse-') as d:
 source=Path(d)/'test.cpp';binary=Path(d)/'test';source.write_text(stubs+lookup+hook+main)
 subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',str(source),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
