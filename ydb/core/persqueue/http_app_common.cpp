#include "http_app_common.h"

#include <library/cpp/monlib/service/pages/templates.h>
#include <util/stream/str.h>

namespace NKikimr::NPQ::NApp {

TPageBuilder::TPageBuilder() {
    Out << "<html>";
    Out << "<body>";
}

TTabBarBuilder TPageBuilder::BeginTabBar() {
    return TTabBarBuilder(*this);
}

TTabContentBuilder TPageBuilder::BeginTabContent() {
    return TTabContentBuilder(*this);
}

TStringStream&& TPageBuilder::Finish() {
    Out << "</body></html>";

    return std::move(Out);
}

//
// TTabBarBuilder
//
TTabBarBuilder::TTabBarBuilder(TPageBuilder& b)
    :PageBuilder(b)
    , FirstTab(true) {
    PageBuilder.Out << "<ul class=\"nav nav-tabs\">";
}

TTabBarBuilder& TTabBarBuilder::AddTab(const TString& anchor, const TString& name) {
    PageBuilder.Out << "<li " << (FirstTab ? "class=\"active\"" : "") << ">";
    PageBuilder.Out << "<a href=\"#" << anchor << "\" >" << name << "</a>";
    PageBuilder.Out << "</li>";

    return *this;
}

TPageBuilder& TTabBarBuilder::Finish() {
    PageBuilder.Out << "</ul>";
    return PageBuilder;
}

//
// TTabContentBuilder
//
TTabContentBuilder::TTabContentBuilder(TPageBuilder& b)
    : PageBuilder(b) {

}

void Body(const NTabletFlatExecutor::NFlatExecutorSetup::ITablet& tablet,
          const TString& path,
          const std::function<void (TStringStream& out)>& tabs,
          const std::function<void (TStringStream& out)>& content) {

    TStringStream str;
    HTML(str) {
        str << "<style>"
            << " .properties { border-bottom-style: solid; border-top-style: solid; border-width: 1px; border-color: darkgrey; padding-bottom: 10px; } "
            << " .properties>tbody>tr>td { padding-left: 10px; padding-right: 10px; } "
            << " .tgrid { width: 100%; border: 0; }"
            << " .tgrid>tbody>tr>td { vertical-align: top; }"
            << "</style>";

        TAG(TH3) {str << tablet.TabletType() << " " << tablet.TabletID() << " (" << path << ")";}
    }
}

}
