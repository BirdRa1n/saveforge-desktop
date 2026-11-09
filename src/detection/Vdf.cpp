#include "saveforge/detection/Vdf.h"

namespace saveforge::detection {

namespace {

constexpr qsizetype kMaxInputBytes = 16 * 1024 * 1024;
constexpr int kMaxDepth = 64;

class Tokenizer {
public:
    enum class Kind { String, Open, Close, End, Error };

    struct Token {
        Kind kind = Kind::End;
        QString text;
    };

    explicit Tokenizer(const QString &input) : m_in(input) {}

    Token next()
    {
        skipSpaceAndComments();
        if (m_pos >= m_in.size())
            return {Kind::End, {}};
        const QChar c = m_in.at(m_pos);
        if (c == u'{') { ++m_pos; return {Kind::Open, {}}; }
        if (c == u'}') { ++m_pos; return {Kind::Close, {}}; }
        if (c == u'"')
            return quoted();
        return unquoted();
    }

    // Skips a platform conditional such as [$WIN32] or [!$OSX] after a value.
    void skipConditional()
    {
        skipSpaceAndComments();
        if (m_pos < m_in.size() && m_in.at(m_pos) == u'[') {
            const qsizetype end = m_in.indexOf(u']', m_pos);
            m_pos = end < 0 ? m_in.size() : end + 1;
        }
    }

    [[nodiscard]] qsizetype position() const { return m_pos; }

private:
    void skipSpaceAndComments()
    {
        while (m_pos < m_in.size()) {
            const QChar c = m_in.at(m_pos);
            if (c.isSpace()) {
                ++m_pos;
            } else if (c == u'/' && m_pos + 1 < m_in.size() && m_in.at(m_pos + 1) == u'/') {
                const qsizetype end = m_in.indexOf(u'\n', m_pos);
                m_pos = end < 0 ? m_in.size() : end + 1;
            } else {
                break;
            }
        }
    }

    Token quoted()
    {
        QString text;
        ++m_pos; // opening quote
        while (m_pos < m_in.size()) {
            const QChar c = m_in.at(m_pos++);
            if (c == u'"')
                return {Kind::String, text};
            if (c == u'\\' && m_pos < m_in.size()) {
                const QChar escaped = m_in.at(m_pos++);
                if (escaped == u'n')
                    text += u'\n';
                else if (escaped == u't')
                    text += u'\t';
                else if (escaped == u'\\' || escaped == u'"')
                    text += escaped;
                else
                    text += u'\\', text += escaped; // unknown escapes are kept verbatim
                continue;
            }
            text += c;
        }
        return {Kind::Error, QStringLiteral("unterminated string")};
    }

    Token unquoted()
    {
        const qsizetype start = m_pos;
        while (m_pos < m_in.size()) {
            const QChar c = m_in.at(m_pos);
            if (c.isSpace() || c == u'{' || c == u'}' || c == u'"')
                break;
            ++m_pos;
        }
        return {Kind::String, m_in.mid(start, m_pos - start)};
    }

    const QString &m_in;
    qsizetype m_pos = 0;
};

Error syntaxError(const QString &message, qsizetype position)
{
    return Error{ErrorCode::Corrupted, QStringLiteral("invalid VDF: ") + message,
                 QStringLiteral("offset %1").arg(position)};
}

// Parses "key value" / "key { ... }" pairs until `}` (depth > 0) or end of input.
Result<void> parseObject(Tokenizer &tokens, VdfNode &parent, int depth)
{
    if (depth > kMaxDepth)
        return syntaxError(QStringLiteral("nesting too deep"), tokens.position());
    for (;;) {
        Tokenizer::Token key = tokens.next();
        if (key.kind == Tokenizer::Kind::End)
            return depth == 0 ? Result<void>() : syntaxError(QStringLiteral("missing '}'"), tokens.position());
        if (key.kind == Tokenizer::Kind::Close)
            return depth > 0 ? Result<void>() : syntaxError(QStringLiteral("unexpected '}'"), tokens.position());
        if (key.kind != Tokenizer::Kind::String)
            return syntaxError(key.kind == Tokenizer::Kind::Error ? key.text : QStringLiteral("expected a key"),
                               tokens.position());

        VdfNode node;
        node.key = key.text;
        Tokenizer::Token value = tokens.next();
        if (value.kind == Tokenizer::Kind::Open) {
            node.isObject = true;
            if (auto nested = parseObject(tokens, node, depth + 1); !nested.ok())
                return nested;
        } else if (value.kind == Tokenizer::Kind::String) {
            node.value = value.text;
            tokens.skipConditional();
        } else {
            return syntaxError(QStringLiteral("expected a value for '%1'").arg(node.key), tokens.position());
        }
        parent.children.append(std::move(node));
    }
}

} // namespace

const VdfNode *VdfNode::child(QStringView name) const
{
    for (const VdfNode &node : children) {
        if (node.key.compare(name, Qt::CaseInsensitive) == 0)
            return &node;
    }
    return nullptr;
}

QString VdfNode::valueOf(QStringView name, const QString &fallback) const
{
    const VdfNode *node = child(name);
    return node && !node->isObject ? node->value : fallback;
}

Result<VdfNode> parseVdf(const QByteArray &text)
{
    if (text.size() > kMaxInputBytes)
        return Error{ErrorCode::InvalidArgument, QStringLiteral("VDF file is too large"), {}};
    QString input = QString::fromUtf8(text);
    if (input.startsWith(QChar(0xFEFF)))
        input.remove(0, 1);

    Tokenizer tokens(input);
    VdfNode root;
    root.isObject = true;
    if (auto parsed = parseObject(tokens, root, 0); !parsed.ok())
        return parsed.error();
    return root;
}

} // namespace saveforge::detection
