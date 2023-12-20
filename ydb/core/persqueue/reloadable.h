#pragma once

#include <util/string/builder.h>

#include <ydb/library/actors/core/actor.h>
#include <ydb/library/actors/core/log.h>
#include <ydb/library/services/services.pb.h>

namespace NKikimr {

enum struct EVerificationStatus {
    Reloaded = 0,
    Ok = 1
};

inline bool operator !(EVerificationStatus value) {
    return EVerificationStatus::Reloaded == value;
}


class TReloadable {
protected:
    [[nodiscard]] EVerificationStatus Verify(bool condition, const TString& details, const NActors::TActorContext& ctx) {
        if (condition) {
            return EVerificationStatus::Ok;
        }

        Reload(details, ctx);
        return EVerificationStatus::Reloaded;
    }

    virtual void Reload(const TString& message, const NActors::TActorContext& ctx) = 0;
};

#define VERIFY(expr, details, ctx)                            \
    do {                                                      \
        if (Y_UNLIKELY(!(expr))) {                            \
            Reload(TStringBuilder() << details, ctx);                             \
            return ::NKikimr::EVerificationStatus::Reloaded;  \
        }                                                     \
    } while (false)

} // namespace NKikimr
