#pragma once
#include "WickedEngine.h"
#include "domain/day_night.hpp"
namespace stalthos::engine {
inline void dayNightScene(wi::scene::Scene& scene,wi::ecs::Entity sun,wi::ecs::Entity fill,float clearIntensity,const environment::DayNightLight& light){
 using namespace DirectX;const auto direction=XMVectorSet(float(light.sunDirection.x),float(light.sunDirection.y),float(light.sunDirection.z),0);const auto axis=XMVector3Cross(XMVectorSet(0,1,0,0),direction);const auto rotation=XMVectorGetX(XMVector3LengthSq(axis))<1e-10f?(light.sunDirection.y>0?XMQuaternionIdentity():XMQuaternionRotationAxis(XMVectorSet(1,0,0,0),3.14159265359f)):XMQuaternionRotationAxis(XMVector3Normalize(axis),float(std::acos(std::clamp(light.sunDirection.y,-1.0,1.0))));
 auto* transform=scene.transforms.GetComponent(sun);transform->ClearTransform();transform->Rotate(rotation);transform->UpdateTransform();auto* lamp=scene.lights.GetComponent(sun);lamp->intensity=clearIntensity*float(light.sunScale);lamp->color={float(light.sunColor.x),float(light.sunColor.y),float(light.sunColor.z)};scene.lights.GetComponent(fill)->intensity=float(light.nightFill);scene.weather.sunDirection={float(light.sunDirection.x),float(light.sunDirection.y),float(light.sunDirection.z)};scene.weather.skyExposure=float(light.skyExposure);scene.weather.ambient={float(light.ambient.x),float(light.ambient.y),float(light.ambient.z)};
}
}
