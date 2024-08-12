#include <library/cpp/monlib/service/pages/templates.h>
#include <ydb/core/tablet_flat/tablet_flat_executor.h>

namespace NKikimr::NPQ::NApp {

class TPageBuilder;
class TPropertiesBuilder;
class TTableBuilder;
class TLayoutBuilder;

class TTabBarBuilder {
public:
    TTabBarBuilder(TPageBuilder& b);

    TTabBarBuilder& AddTab(const TString& anchor, const TString& name);
    TPageBuilder& Finish();

private:
    TPageBuilder& PageBuilder;
    bool FirstTab;
};

class TPropertiesBuilder {
public:
    TPropertiesBuilder(TPageBuilder& b);

    TPropertiesBuilder& Add(const TString& title, const TString& value);
    TLayoutBuilder& Finish();

private:
    TPageBuilder& PageBuilder;
};

class TTableBuilder {
public:
    TTableBuilder(TPageBuilder& b);

private:
    TPageBuilder& b;
};

class TLayoutBuilder {
public:
    TLayoutBuilder(TPageBuilder& b);

    TPropertiesBuilder AddProperties(const TString& caption);
    TTableBuilder AddTable(const TString& caption);

private:
    TPageBuilder& PageBuilder;
};

class TTabContentBuilder {
public:
    TTabContentBuilder(TPageBuilder& b);

    TLayoutBuilder BeginColumn();

private:
    TPageBuilder& PageBuilder;
};

class TPageBuilder {
    friend class TTabBarBuilder;
    friend class TPropertiesBuilder;
    friend class TTableBuilder;
    friend class TLayoutBuilder;
    friend class TTabContentBuilder;
public:
    TPageBuilder();

    TTabBarBuilder BeginTabBar();
    TTabContentBuilder BeginTabContent();

    TStringStream&& Finish();

private:
    TStringStream Out;
};



void Body(const NTabletFlatExecutor::NFlatExecutorSetup::ITablet& tablet,
          const TString& path,
          const std::function<void (TStringStream& out)>& tabs,
          const std::function<void (TStringStream& out)>& content);

}
