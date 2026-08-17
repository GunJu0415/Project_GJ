// Fill out your copyright notice in the Description page of Project Settings.


#include "GJGameTypes.h"

FStatValues& FStatValues::operator+=(const FStatValues& Other)
{
    // 필드가 늘어나면 여기에도 한 줄 추가해야 한다. 컴파일러가 안 잡아주는 지점이다.
    MaxHP             += Other.MaxHP;
    MaxMP             += Other.MaxMP;
    BaseAttackPower   += Other.BaseAttackPower;
    SkillPower        += Other.SkillPower;
    RequiredEXP       += Other.RequiredEXP;
    Defense           += Other.Defense;
    MoveSpeed         += Other.MoveSpeed;
    CooldownReduction += Other.CooldownReduction;
    CritChance        += Other.CritChance;
    CritMultiplier    += Other.CritMultiplier;
    return *this;
}
