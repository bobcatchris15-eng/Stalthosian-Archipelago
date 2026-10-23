#define NOMINMAX
#include <Windows.h>
#ifndef small
#define small char
#endif
#include "domain/brackenback.hpp"
#include <iostream>
int main(){auto result=stalthos::domain::brackenProtection(1,stalthos::HeroSpecies::Brackenback,{0,0,0},stalthos::LifeStage::Adult,true,{});std::cout<<"BRACKEN_WINDOWS_MACRO_OK\n";return result?1:0;}
