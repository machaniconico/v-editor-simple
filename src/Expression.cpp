#include "Expression.h"

#include <QJsonArray>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>

// ============================================================
// Tokenizer
// ============================================================

namespace {

enum class TokenType {
    Number,
    Identifier,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    LParen,
    RParen,
    Comma,
    Less,
    LessEq,
    Greater,
    GreaterEq,
    EqualEqual,
    NotEqual,
    End,
    Unknown
};

struct Token {
    TokenType type = TokenType::Unknown;
    QString text;
    double number = 0.0;
};

class Tokenizer
{
public:
    explicit Tokenizer(const QString &input) : m_input(input) {}

    QVector<Token> tokenize()
    {
        QVector<Token> tokens;
        while (m_pos < m_input.size()) {
            skipWhitespace();
            if (m_pos >= m_input.size()) break;

            QChar ch = m_input[m_pos];

            // Numbers (including decimals like .5)
            if (ch.isDigit() || (ch == '.' && m_pos + 1 < m_input.size()
                                 && m_input[m_pos + 1].isDigit())) {
                tokens.append(readNumber());
                continue;
            }

            // Identifiers (functions / variables)
            if (ch.isLetter() || ch == '_') {
                tokens.append(readIdentifier());
                continue;
            }

            // Two-character operators
            if (m_pos + 1 < m_input.size()) {
                QString two = m_input.mid(m_pos, 2);
                if (two == "<=") { tokens.append({TokenType::LessEq, two, 0}); m_pos += 2; continue; }
                if (two == ">=") { tokens.append({TokenType::GreaterEq, two, 0}); m_pos += 2; continue; }
                if (two == "==") { tokens.append({TokenType::EqualEqual, two, 0}); m_pos += 2; continue; }
                if (two == "!=") { tokens.append({TokenType::NotEqual, two, 0}); m_pos += 2; continue; }
            }

            // Single-character operators
            switch (ch.toLatin1()) {
            case '+': tokens.append({TokenType::Plus, "+", 0}); break;
            case '-': tokens.append({TokenType::Minus, "-", 0}); break;
            case '*': tokens.append({TokenType::Star, "*", 0}); break;
            case '/': tokens.append({TokenType::Slash, "/", 0}); break;
            case '%': tokens.append({TokenType::Percent, "%", 0}); break;
            case '(': tokens.append({TokenType::LParen, "(", 0}); break;
            case ')': tokens.append({TokenType::RParen, ")", 0}); break;
            case ',': tokens.append({TokenType::Comma, ",", 0}); break;
            case '<': tokens.append({TokenType::Less, "<", 0}); break;
            case '>': tokens.append({TokenType::Greater, ">", 0}); break;
            default:
                tokens.append({TokenType::Unknown, QString(ch), 0});
                break;
            }
            ++m_pos;
        }
        tokens.append({TokenType::End, {}, 0});
        return tokens;
    }

private:
    void skipWhitespace()
    {
        while (m_pos < m_input.size() && m_input[m_pos].isSpace())
            ++m_pos;
    }

    Token readNumber()
    {
        int start = m_pos;
        while (m_pos < m_input.size() && (m_input[m_pos].isDigit() || m_input[m_pos] == '.'))
            ++m_pos;
        QString text = m_input.mid(start, m_pos - start);
        return {TokenType::Number, text, text.toDouble()};
    }

    Token readIdentifier()
    {
        int start = m_pos;
        while (m_pos < m_input.size() && (m_input[m_pos].isLetterOrNumber() || m_input[m_pos] == '_'))
            ++m_pos;
        QString text = m_input.mid(start, m_pos - start);
        return {TokenType::Identifier, text, 0};
    }

    QString m_input;
    int m_pos = 0;
};

// ============================================================
// Recursive descent parser
// ============================================================

// Grammar:
//   expr       = comparison
//   comparison = additive ( ("<" | "<=" | ">" | ">=" | "==" | "!=") additive )*
//   additive   = multiplicative ( ("+" | "-") multiplicative )*
//   multiplicative = unary ( ("*" | "/" | "%") unary )*
//   unary      = ("-" | "+") unary | primary
//   primary    = NUMBER | IDENTIFIER | IDENTIFIER "(" arglist ")" | "(" expr ")"
//   arglist    = expr ("," expr)*

class Parser
{
public:
    Parser(const QVector<Token> &tokens, const ExpressionContext &ctx)
        : m_tokens(tokens), m_ctx(ctx) {}

    ExpressionResult parse()
    {
        auto result = parseExpression();
        if (!result.success) return result;
        if (current().type != TokenType::End)
            return ExpressionResult::fail("Unexpected token: " + current().text);
        return result;
    }

    // Validate-only mode (no context needed)
    QString validateOnly()
    {
        auto result = parseExpression();
        if (!result.success) return result.error;
        if (current().type != TokenType::End)
            return "Unexpected token: " + current().text;
        return {};
    }

private:
    const Token &current() const { return m_tokens[m_pos]; }
    const Token &advance() { return m_tokens[m_pos++]; }
    bool match(TokenType t) { if (current().type == t) { ++m_pos; return true; } return false; }

    // --- comparison ---
    ExpressionResult parseExpression() { return parseComparison(); }

    ExpressionResult parseComparison()
    {
        auto left = parseAdditive();
        if (!left.success) return left;

        while (current().type == TokenType::Less || current().type == TokenType::LessEq
               || current().type == TokenType::Greater || current().type == TokenType::GreaterEq
               || current().type == TokenType::EqualEqual || current().type == TokenType::NotEqual) {
            TokenType op = advance().type;
            auto right = parseAdditive();
            if (!right.success) return right;
            switch (op) {
            case TokenType::Less:       left.value = left.value < right.value ? 1.0 : 0.0; break;
            case TokenType::LessEq:     left.value = left.value <= right.value ? 1.0 : 0.0; break;
            case TokenType::Greater:    left.value = left.value > right.value ? 1.0 : 0.0; break;
            case TokenType::GreaterEq:  left.value = left.value >= right.value ? 1.0 : 0.0; break;
            case TokenType::EqualEqual: left.value = std::abs(left.value - right.value) < 1e-9 ? 1.0 : 0.0; break;
            case TokenType::NotEqual:   left.value = std::abs(left.value - right.value) >= 1e-9 ? 1.0 : 0.0; break;
            default: break;
            }
        }
        return left;
    }

    ExpressionResult parseAdditive()
    {
        auto left = parseMultiplicative();
        if (!left.success) return left;

        while (current().type == TokenType::Plus || current().type == TokenType::Minus) {
            TokenType op = advance().type;
            auto right = parseMultiplicative();
            if (!right.success) return right;
            left.value = (op == TokenType::Plus) ? left.value + right.value
                                                 : left.value - right.value;
        }
        return left;
    }

    ExpressionResult parseMultiplicative()
    {
        auto left = parseUnary();
        if (!left.success) return left;

        while (current().type == TokenType::Star || current().type == TokenType::Slash
               || current().type == TokenType::Percent) {
            TokenType op = advance().type;
            auto right = parseUnary();
            if (!right.success) return right;
            if (op == TokenType::Star) {
                left.value *= right.value;
            } else if (op == TokenType::Slash) {
                if (std::abs(right.value) < 1e-15)
                    return ExpressionResult::fail("Division by zero");
                left.value /= right.value;
            } else {
                if (std::abs(right.value) < 1e-15)
                    return ExpressionResult::fail("Modulo by zero");
                left.value = std::fmod(left.value, right.value);
            }
        }
        return left;
    }

    ExpressionResult parseUnary()
    {
        if (current().type == TokenType::Minus) {
            advance();
            auto r = parseUnary();
            if (r.success) r.value = -r.value;
            return r;
        }
        if (current().type == TokenType::Plus) {
            advance();
            return parseUnary();
        }
        return parsePrimary();
    }

    ExpressionResult parsePrimary()
    {
        // Number literal
        if (current().type == TokenType::Number) {
            double v = current().number;
            advance();
            return ExpressionResult::ok(v);
        }

        // Parenthesized expression
        if (current().type == TokenType::LParen) {
            advance();
            auto r = parseExpression();
            if (!r.success) return r;
            if (!match(TokenType::RParen))
                return ExpressionResult::fail("Expected ')'");
            return r;
        }

        // Identifier: variable or function call
        if (current().type == TokenType::Identifier) {
            QString name = current().text;
            advance();

            // Function call?
            if (current().type == TokenType::LParen) {
                advance();
                QVector<double> args;
                if (current().type != TokenType::RParen) {
                    auto arg = parseExpression();
                    if (!arg.success) return arg;
                    args.append(arg.value);
                    while (match(TokenType::Comma)) {
                        arg = parseExpression();
                        if (!arg.success) return arg;
                        args.append(arg.value);
                    }
                }
                if (!match(TokenType::RParen))
                    return ExpressionResult::fail("Expected ')' after function arguments");
                return callFunction(name, args);
            }

            // Variable lookup
            return resolveVariable(name);
        }

        return ExpressionResult::fail("Unexpected token: " + current().text);
    }

    // --- Variable resolution ---

    ExpressionResult resolveVariable(const QString &name) const
    {
        if (name == "time")     return ExpressionResult::ok(m_ctx.time);
        if (name == "index")    return ExpressionResult::ok(m_ctx.layerIndex);
        if (name == "fps")      return ExpressionResult::ok(m_ctx.fps);
        if (name == "duration") return ExpressionResult::ok(m_ctx.duration);
        if (name == "width")    return ExpressionResult::ok(m_ctx.canvasWidth);
        if (name == "height")   return ExpressionResult::ok(m_ctx.canvasHeight);
        if (name == "value")    return ExpressionResult::ok(m_ctx.value);
        if (name == "PI")       return ExpressionResult::ok(M_PI);
        if (name == "E")        return ExpressionResult::ok(M_E);
        return ExpressionResult::fail("Unknown variable: " + name);
    }

    // --- Function dispatch ---

    ExpressionResult callFunction(const QString &name, const QVector<double> &args) const
    {
        // Math functions (1 arg)
        if (name == "sin")   { if (args.size() != 1) return argError(name, 1); return ExpressionResult::ok(std::sin(args[0])); }
        if (name == "cos")   { if (args.size() != 1) return argError(name, 1); return ExpressionResult::ok(std::cos(args[0])); }
        if (name == "tan")   { if (args.size() != 1) return argError(name, 1); return ExpressionResult::ok(std::tan(args[0])); }
        if (name == "abs")   { if (args.size() != 1) return argError(name, 1); return ExpressionResult::ok(std::abs(args[0])); }
        if (name == "floor") { if (args.size() != 1) return argError(name, 1); return ExpressionResult::ok(std::floor(args[0])); }
        if (name == "ceil")  { if (args.size() != 1) return argError(name, 1); return ExpressionResult::ok(std::ceil(args[0])); }
        if (name == "round") { if (args.size() != 1) return argError(name, 1); return ExpressionResult::ok(std::round(args[0])); }
        if (name == "sqrt")  { if (args.size() != 1) return argError(name, 1); return ExpressionResult::ok(std::sqrt(args[0])); }

        // Math functions (2 args)
        if (name == "pow") { if (args.size() != 2) return argError(name, 2); return ExpressionResult::ok(std::pow(args[0], args[1])); }
        if (name == "min") { if (args.size() != 2) return argError(name, 2); return ExpressionResult::ok(std::min(args[0], args[1])); }
        if (name == "max") { if (args.size() != 2) return argError(name, 2); return ExpressionResult::ok(std::max(args[0], args[1])); }

        // clamp(val, lo, hi)
        if (name == "clamp") {
            if (args.size() != 3) return argError(name, 3);
            return ExpressionResult::ok(std::clamp(args[0], args[1], args[2]));
        }

        // AE-style: wiggle(freq, amp)
        if (name == "wiggle") {
            if (args.size() != 2) return argError(name, 2);
            return ExpressionResult::ok(wiggle(args[0], args[1]));
        }

        // linear(t, tMin, tMax, vMin, vMax)
        if (name == "linear") {
            if (args.size() != 5) return argError(name, 5);
            return ExpressionResult::ok(linearInterp(args[0], args[1], args[2], args[3], args[4]));
        }

        // ease(t, tMin, tMax, vMin, vMax)
        if (name == "ease") {
            if (args.size() != 5) return argError(name, 5);
            return ExpressionResult::ok(easeInterp(args[0], args[1], args[2], args[3], args[4]));
        }

        // random(min, max)
        if (name == "random") {
            if (args.size() != 2) return argError(name, 2);
            double lo = args[0], hi = args[1];
            double r = QRandomGenerator::global()->generateDouble();
            return ExpressionResult::ok(lo + r * (hi - lo));
        }

        // noise(t) -- simple value noise with linear interpolation
        if (name == "noise") {
            if (args.size() != 1) return argError(name, 1);
            return ExpressionResult::ok(valueNoise(args[0]));
        }

        // loopIn(type, numKeyframes) / loopOut(type, numKeyframes)
        // type: 0 = cycle, 1 = pingpong   (simplified — returns modulated time)
        if (name == "loopIn" || name == "loopOut") {
            if (args.size() != 2) return argError(name, 2);
            return ExpressionResult::ok(loopTime(args[0], args[1], name == "loopOut"));
        }

        // toFixed(val, decimals) -- returns val rounded to N decimal places
        if (name == "toFixed") {
            if (args.size() != 2) return argError(name, 2);
            double factor = std::pow(10.0, std::round(args[1]));
            return ExpressionResult::ok(std::round(args[0] * factor) / factor);
        }

        return ExpressionResult::fail("Unknown function: " + name);
    }

    static ExpressionResult argError(const QString &func, int expected)
    {
        return ExpressionResult::fail(func + "() expects " + QString::number(expected) + " argument(s)");
    }

    // --- AE-style function implementations ---

    // wiggle: sin-based pseudo-random oscillation seeded by time * frequency
    double wiggle(double freq, double amp) const
    {
        double t = m_ctx.time * freq;
        // Combine multiple sin waves for a more organic look
        double v = std::sin(t * 6.2831853)
                 + 0.5 * std::sin(t * 6.2831853 * 2.13 + 1.7)
                 + 0.25 * std::sin(t * 6.2831853 * 4.37 + 3.1);
        v /= 1.75; // normalize roughly to [-1, 1]
        return m_ctx.value + v * amp;
    }

    // linear interpolation / remap
    static double linearInterp(double t, double tMin, double tMax, double vMin, double vMax)
    {
        if (std::abs(tMax - tMin) < 1e-15) return vMin;
        double f = (t - tMin) / (tMax - tMin);
        f = std::clamp(f, 0.0, 1.0);
        return vMin + f * (vMax - vMin);
    }

    // eased interpolation (smoothstep)
    static double easeInterp(double t, double tMin, double tMax, double vMin, double vMax)
    {
        if (std::abs(tMax - tMin) < 1e-15) return vMin;
        double f = (t - tMin) / (tMax - tMin);
        f = std::clamp(f, 0.0, 1.0);
        f = f * f * (3.0 - 2.0 * f); // smoothstep
        return vMin + f * (vMax - vMin);
    }

    // Simple value noise with linear interpolation
    static double valueNoise(double t)
    {
        auto hash = [](int n) -> double {
            n = (n << 13) ^ n;
            n = n * (n * n * 15731 + 789221) + 1376312589;
            return 1.0 - (double)(n & 0x7fffffff) / 1073741824.0;
        };

        int i = (int)std::floor(t);
        double frac = t - i;
        double a = hash(i);
        double b = hash(i + 1);
        // Smooth interpolation
        double u = frac * frac * (3.0 - 2.0 * frac);
        return a + u * (b - a);
    }

    // Loop time modulation (simplified)
    double loopTime(double type, double numKeyframes, bool isOut) const
    {
        (void)numKeyframes; // simplified: use full duration
        double dur = m_ctx.duration;
        if (dur < 1e-15) return m_ctx.time;

        double t = m_ctx.time;
        int loopType = (int)type; // 0 = cycle, 1 = pingpong

        if (isOut) {
            t = std::fmod(t, dur);
            if (t < 0) t += dur;
        } else {
            t = std::fmod(t, dur);
            if (t < 0) t += dur;
        }

        if (loopType == 1) { // pingpong
            double period = dur * 2.0;
            double pos = std::fmod(m_ctx.time, period);
            if (pos < 0) pos += period;
            t = (pos <= dur) ? pos : period - pos;
        }

        return t;
    }

    const QVector<Token> &m_tokens;
    const ExpressionContext &m_ctx;
    int m_pos = 0;
};

} // anonymous namespace

// ============================================================
// Expression (public static interface)
// ============================================================

ExpressionResult Expression::evaluate(const QString &expressionString,
                                      const ExpressionContext &context)
{
    if (expressionString.trimmed().isEmpty())
        return ExpressionResult::fail("Empty expression");

    Tokenizer tokenizer(expressionString);
    auto tokens = tokenizer.tokenize();

    // Check for unknown tokens
    for (const auto &tok : tokens) {
        if (tok.type == TokenType::Unknown)
            return ExpressionResult::fail("Unexpected character: " + tok.text);
    }

    Parser parser(tokens, context);
    return parser.parse();
}

QString Expression::validate(const QString &expressionString)
{
    if (expressionString.trimmed().isEmpty())
        return QStringLiteral("Empty expression");

    Tokenizer tokenizer(expressionString);
    auto tokens = tokenizer.tokenize();

    for (const auto &tok : tokens) {
        if (tok.type == TokenType::Unknown)
            return "Unexpected character: " + tok.text;
    }

    // Use a dummy context for validation
    ExpressionContext dummyCtx;
    Parser parser(tokens, dummyCtx);
    return parser.validateOnly();
}

QStringList Expression::availableFunctions()
{
    return {
        // Variables
        "time", "index", "fps", "duration", "width", "height", "value",
        // Constants
        "PI", "E",
        // Math (1 arg)
        "sin(x)", "cos(x)", "tan(x)", "abs(x)", "floor(x)", "ceil(x)",
        "round(x)", "sqrt(x)",
        // Math (2 args)
        "pow(x,y)", "min(x,y)", "max(x,y)",
        // Math (3 args)
        "clamp(val,min,max)",
        // AE-style
        "wiggle(freq,amp)", "linear(t,tMin,tMax,vMin,vMax)",
        "ease(t,tMin,tMax,vMin,vMax)", "random(min,max)", "noise(t)",
        "loopIn(type,numKeyframes)", "loopOut(type,numKeyframes)",
        // String/utility
        "toFixed(val,decimals)"
    };
}

// ============================================================
// ExpressionEngine
// ============================================================

void ExpressionEngine::addExpression(const QString &propertyName, const QString &code)
{
    for (auto &expr : m_expressions) {
        if (expr.propertyName == propertyName) {
            expr.expressionCode = code;
            expr.enabled = true;
            return;
        }
    }
    m_expressions.append({propertyName, code, true});
}

void ExpressionEngine::removeExpression(const QString &propertyName)
{
    m_expressions.erase(
        std::remove_if(m_expressions.begin(), m_expressions.end(),
            [&](const PropertyExpression &e) { return e.propertyName == propertyName; }),
        m_expressions.end());
}

void ExpressionEngine::setEnabled(const QString &propertyName, bool enabled)
{
    for (auto &expr : m_expressions) {
        if (expr.propertyName == propertyName) {
            expr.enabled = enabled;
            return;
        }
    }
}

bool ExpressionEngine::hasExpression(const QString &propertyName) const
{
    for (const auto &expr : m_expressions)
        if (expr.propertyName == propertyName) return true;
    return false;
}

const PropertyExpression *ExpressionEngine::expression(const QString &propertyName) const
{
    for (const auto &expr : m_expressions)
        if (expr.propertyName == propertyName) return &expr;
    return nullptr;
}

QStringList ExpressionEngine::propertyNames() const
{
    QStringList names;
    for (const auto &expr : m_expressions)
        names.append(expr.propertyName);
    return names;
}

QMap<QString, double> ExpressionEngine::evaluateAll(const ExpressionContext &context) const
{
    QMap<QString, double> results;
    for (const auto &expr : m_expressions) {
        if (!expr.enabled) continue;
        auto result = Expression::evaluate(expr.expressionCode, context);
        if (result.success)
            results.insert(expr.propertyName, result.value);
    }
    return results;
}

ExpressionResult ExpressionEngine::evaluateProperty(const QString &propertyName,
                                                     const ExpressionContext &context) const
{
    const auto *expr = expression(propertyName);
    if (!expr)
        return ExpressionResult::fail("No expression for property: " + propertyName);
    if (!expr->enabled)
        return ExpressionResult::fail("Expression disabled for: " + propertyName);
    return Expression::evaluate(expr->expressionCode, context);
}

// --- Serialisation ---

QJsonObject ExpressionEngine::toJson() const
{
    QJsonObject obj;
    QJsonArray arr;
    for (const auto &expr : m_expressions) {
        QJsonObject e;
        e["property"] = expr.propertyName;
        e["code"] = expr.expressionCode;
        e["enabled"] = expr.enabled;
        arr.append(e);
    }
    obj["expressions"] = arr;
    return obj;
}

void ExpressionEngine::fromJson(const QJsonObject &obj)
{
    m_expressions.clear();
    const QJsonArray arr = obj["expressions"].toArray();
    for (const auto &val : arr) {
        QJsonObject e = val.toObject();
        PropertyExpression pe;
        pe.propertyName = e["property"].toString();
        pe.expressionCode = e["code"].toString();
        pe.enabled = e["enabled"].toBool(true);
        m_expressions.append(pe);
    }
}
