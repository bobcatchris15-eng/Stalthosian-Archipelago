#define main candidate_test_main
#include "../../../stalthos-runtime/tests/regional_archive.cpp"
#undef main
int main() {
 auto r=registry(); auto d=r.continuationSnapshot(); auto& a=d.regions[0].population.actors[0];
 const auto before=a.species;
 a.species=before==HeroSpecies::Smalltrotter?HeroSpecies::Longstride:HeroSpecies::Smalltrotter;
 a.calendarAge=7000; a.stage=LifeStage::Adult;
 std::string bytes; const bool encoded=archive::encode(d,bytes); const bool restored=archive::restore(r,bytes);
 std::cout<<"species_original="<<int(before)<<" species_supplied="<<int(a.species)<<" encoded="<<encoded<<" restored="<<restored<<" resulting="<<int(r.snapshot(7)->population.actors[0].species)<<'\n';
 return (!restored && r.snapshot(7)->population.actors[0].species==before)?0:1;
}
