#include "params.h"

#include <library/cpp/cgiparam/cgiparam.h>

namespace NKikimr::NSQS {

TParameters ParseParameters(const NHttp::THttpIncomingRequestPtr& params) {
    TParameters queryParams;

    TParametersParser parser(&queryParams);
    if (params->Method == "GET") {
        for (auto pi = params->GetParameters().Parameters.begin(); pi != params->GetParameters().Parameters.end(); ++pi) {
            parser.Append(TString(pi->first), TString(pi->second));
        }
    } else if (params->Method == "POST") {
        parser.Append({}, TString(params->Body));
    } else {
        // TODO error
    }

    return queryParams;
}

} // namespace NKikimr::NSQS
