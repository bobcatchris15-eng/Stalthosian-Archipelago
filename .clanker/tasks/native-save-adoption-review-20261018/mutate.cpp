#include "domain/host_save.hpp"
#include <iostream>
namespace hs=stalthos::persistence::host_save;
static void put(const std::filesystem::path& p,const std::string& b){std::ofstream out(p,std::ios::binary);out.write(b.data(),static_cast<std::streamsize>(b.size()));if(!out)throw std::runtime_error("write failed");}
int main(int argc,char** argv){try{if(argc!=3)return 2;
 if(std::string(argv[1])=="--verify-successor"){auto saved=hs::read(argv[2],5);if(!saved.host.hero || saved.host.hero->id!=2 || !std::any_of(saved.host.deadHeroes.begin(),saved.host.deadHeroes.end(),[](const auto& d){return d.id==1;}) || !saved.regional || saved.host.faunaPopulation->actors.size()!=8)throw std::runtime_error("saved successor/death/population lost");std::cout<<"ACTUAL_STL9_SUCCESSOR_HERO2_DEATH1_EIGHT_ANIMALS_OK\n";return 0;}
 auto c=hs::read(argv[1],5);if(!c.regional)return 3;const auto original=hs::encode(c);const std::filesystem::path dir=argv[2];
 auto bad=c;bad.regional->regions[0].anchors[0].position.x+=.125;put(dir/"bad-anchor.stl",hs::encode(bad));
 bad=c;bad.host.faunaPopulation->habitatSignature++;bad.regional->regions[0].population.habitatSignature++;put(dir/"bad-habitat.stl",hs::encode(bad));
 auto bytes=original;bytes.back()^=1;put(dir/"bad-checksum.stl",bytes);
 bad=c;bad.configuration++;put(dir/"bad-configuration.stl",hs::encode(bad));
 bytes=original;auto h=bad.host;h.faunaPopulation->actors.front().thirst=h.faunaPopulation->actors.front().thirst>.5?.1:.8;const auto legacy=hs::legacyBytes(h);bytes.replace(72,legacy.size(),legacy);const auto sum=hs::checksum(std::string_view(bytes).substr(0,bytes.size()-8));for(unsigned i=0;i<8;++i)bytes[bytes.size()-8+i]=static_cast<char>(sum>>(8*i));put(dir/"bad-payload-binding.stl",bytes);
 auto terminal=c;if(!terminal.host.hero || terminal.host.hero->id!=1)throw std::runtime_error("expected hero1 terminal fixture source");terminal.host.deadHeroes.push_back({1,terminal.host.hero->region});terminal.host.hero.reset();terminal.host.heroLifecycle.reset();terminal.host.heroNeeds.reset();for(auto& r:terminal.host.retained)if(r.id==1){r.alive=false;r.active=false;}put(dir/"terminal.stl",hs::encode(terminal));
 std::cout<<"NATIVE_INVALID_AND_TERMINAL_FIXTURES_READY\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
