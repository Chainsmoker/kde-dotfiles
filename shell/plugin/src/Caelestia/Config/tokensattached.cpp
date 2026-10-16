#include "tokensattached.hpp"

#include <qquickitem.h>

#include "anim.hpp"
#include "appearanceconfig.hpp"
#include "font.hpp"
#include "rootnodes.hpp"
#include "tokens.hpp"

namespace caelestia::config {

namespace {

const AppearanceConfig* resolveAppearance(ConfigRoot* config, bool complete, const char* prop, QObject* parent) {
    if (config)
        return config->appearance();
    if ((complete || !qobject_cast<QQuickItem*>(parent)) && parent)
        qCWarning(lcConfig, "Tokens.%s accessed without a screen set on %s", prop, parent->metaObject()->className());
    return ConfigSingleton::instance()->appearance();
}

AnimTokens* globalAnimTokens() {
    static AnimTokens* s_anim = []() {
        auto* anim = new AnimTokens(TokensSingleton::instance());
        anim->bindDurations(ConfigSingleton::instance()->appearance()->anim()->durations());
        anim->bindCurves(TokensSingleton::instance()->appearance()->curves());
        return anim;
    }();
    return s_anim;
}

FontTokens* fontTokensForScreen(const QString& screen) {
    static QHash<QString, FontTokens*> s_cache;
    auto it = s_cache.find(screen);
    if (it != s_cache.end())
        return *it;

    auto* config = screen.isEmpty() ? nullptr : ConfigSingleton::instance()->forScreen(screen);
    auto* tokensRoot = screen.isEmpty() ? nullptr : TokensSingleton::instance()->forScreen(screen);
    auto* appearance = config ? config->appearance() : ConfigSingleton::instance()->appearance();
    auto* tokens = tokensRoot ? tokensRoot->appearance() : TokensSingleton::instance()->appearance();

    auto* fontTokens = new FontTokens(TokensSingleton::instance());
    fontTokens->bindFont(appearance->font());
    fontTokens->bindTokens(tokens);
    s_cache.insert(screen, fontTokens);
    return fontTokens;
}

} // namespace

Tokens::Tokens(QObject* parent)
    : QQuickAttachedPropertyPropagator(parent)
    , m_font(fontTokensForScreen(QString()))
    , m_anim(globalAnimTokens()) {
    initialize();
}

void Tokens::classBegin() {}

void Tokens::componentComplete() {
    m_complete = true;
}

QString Tokens::screen() const {
    return m_screen;
}

void Tokens::inheritScreen(const QString& screen) {
    if (screen == m_screen)
        return;

    m_screen = screen;

    if (m_screen.isEmpty()) {
        m_config = nullptr;
        m_tokens = nullptr;
    } else {
        m_config = ConfigSingleton::instance()->forScreen(m_screen);
        m_tokens = TokensSingleton::instance()->forScreen(m_screen);
    }

    m_font = fontTokensForScreen(m_screen);
    propagateScreen();
    emit sourceChanged();
}

void Tokens::propagateScreen() {
    const auto children = attachedChildren();
    for (auto* const child : children) {
        auto* const tokens = qobject_cast<Tokens*>(child);
        if (tokens)
            tokens->inheritScreen(m_screen);
    }
}

void Tokens::attachedParentChange(
    QQuickAttachedPropertyPropagator* newParent, QQuickAttachedPropertyPropagator* oldParent) {
    Q_UNUSED(oldParent);
    auto* const tokens = qobject_cast<Tokens*>(newParent);
    if (tokens)
        inheritScreen(tokens->screen());
}

void Tokens::bindAnim() {}

void Tokens::bindFont() {}

#define TOKENS_ATTACHED_GETTER(Type, name)                                                                             \
    const Type* Tokens::name() const {                                                                                 \
        auto* a = resolveAppearance(m_config, m_complete, #name, parent());                                            \
        return a ? a->name() : nullptr;                                                                                \
    }

TOKENS_ATTACHED_GETTER(AppearanceRounding, rounding)
TOKENS_ATTACHED_GETTER(AppearanceSpacing, spacing)
TOKENS_ATTACHED_GETTER(AppearancePadding, padding)

#undef TOKENS_ATTACHED_GETTER

const AppearanceTransparency* Tokens::transparency() const {
    return ConfigSingleton::instance()->appearance()->transparency();
}

const SizeTokens* Tokens::sizes() const {
    if (m_tokens)
        return m_tokens->sizes();
    if ((m_complete || !qobject_cast<QQuickItem*>(parent())) && parent())
        qCWarning(lcConfig, "Tokens.sizes accessed without a screen set on %s", parent()->metaObject()->className());
    return TokensSingleton::instance()->sizes();
}

const FontTokens* Tokens::font() const {
    return m_font;
}

const AnimTokens* Tokens::anim() const {
    return m_anim;
}

TokensRoot* Tokens::forScreen(const QString& screen) {
    return TokensSingleton::instance()->forScreen(screen);
}

Tokens* Tokens::qmlAttachedProperties(QObject* object) {
    return new Tokens(object);
}

} // namespace caelestia::config
