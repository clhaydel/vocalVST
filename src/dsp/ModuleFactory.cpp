#include "modules/Modules.h"

namespace hv
{
int syllableLatency (float windowMs, double sr);

std::unique_ptr<VocalModule> createModule (ModuleType type)
{
    switch (type)
    {
        case ModuleType::SourceDrive:     return makeSourceDrive();
        case ModuleType::PeriodDoubler:   return makePeriodDoubler();
        case ModuleType::FryGate:         return makeFryGate();
        case ModuleType::FormantFreeze:   return makeFormantFreeze();
        case ModuleType::Splitter:        return makeSplitter();
        case ModuleType::SyllableShuffle: return makeSyllableShuffle();
        case ModuleType::CuppedMic:       return makeCuppedMic();
        case ModuleType::ContactMic:      return makeContactMic();
        case ModuleType::ModalRattle:     return makeModalRattle();
        case ModuleType::MembraneBuzz:    return makeMembraneBuzz();
        case ModuleType::CarrierVocoder:  return makeCarrierVocoder();
        case ModuleType::SalivaClicks:    return makeSalivaClicks();
        case ModuleType::Mob:             return makeMob();
        default:                          return nullptr;
    }
}

int moduleLatencySamples (ModuleType type, const float* v, double sr)
{
    switch (type)
    {
        case ModuleType::PeriodDoubler:   return (int) std::lround (kPeriodDoublerLatencyMs * 0.001 * sr);
        case ModuleType::SyllableShuffle: return syllableLatency (v[1], sr);
        default:                          return 0;
    }
}
} // namespace hv
