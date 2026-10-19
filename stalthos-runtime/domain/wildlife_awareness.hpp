#pragma once
#include "foundation.hpp"
namespace stalthos::fauna {
struct AwarenessTuning {double noticeMeters=12,comfortMeters=6,avoidanceSpeedMetersPerSecond=.7;};
struct BodyAwareness {Vec3 body;AwarenessTuning tuning;};
struct AwarenessDecision {bool noticed=false,avoid=false;double distance=0,headingToBody=0;};
inline AwarenessDecision awareness(Vec3 actor,const BodyAwareness& context) {
    const auto& t=context.tuning;
    if(!finite(actor)||!finite(context.body)||!std::isfinite(t.noticeMeters)||!std::isfinite(t.comfortMeters)||!std::isfinite(t.avoidanceSpeedMetersPerSecond)||t.comfortMeters<=0||t.noticeMeters<=t.comfortMeters||t.avoidanceSpeedMetersPerSecond<=0||t.avoidanceSpeedMetersPerSecond>3) throw std::invalid_argument("invalid wildlife awareness context");
    const double dx=context.body.x-actor.x,dz=context.body.z-actor.z;
    const double distance=std::hypot(dx,dz);
    return {distance<t.noticeMeters,distance<t.comfortMeters,distance,std::atan2(dx,dz)};
}
}
