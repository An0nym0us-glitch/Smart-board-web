#include "math/Expression.h"

#include <QCoreApplication>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <vector>

namespace cb::math {

namespace {

QString tr(const char* s) { return QCoreApplication::translate("Expression", s); }

enum class Fn {
    Sin, Cos, Tan, Cot, Sec, Csc, Asin, Acos, Atan, Sinh, Cosh, Tanh,
    Sqrt, Cbrt, Abs, Ln, Log, Exp, Floor, Ceil, Round, Sign, Min, Max, Fact,
};

struct FnInfo
{
    const char* name;
    Fn fn;
    int arity;
};

const FnInfo kFunctions[] = {
    {"sin", Fn::Sin, 1},     {"cos", Fn::Cos, 1},     {"tan", Fn::Tan, 1},     {"cot", Fn::Cot, 1},
    {"sec", Fn::Sec, 1},     {"csc", Fn::Csc, 1},     {"asin", Fn::Asin, 1},   {"acos", Fn::Acos, 1},
    {"atan", Fn::Atan, 1},   {"arcsin", Fn::Asin, 1}, {"arccos", Fn::Acos, 1}, {"arctan", Fn::Atan, 1},
    {"sinh", Fn::Sinh, 1},   {"cosh", Fn::Cosh, 1},   {"tanh", Fn::Tanh, 1},   {"sqrt", Fn::Sqrt, 1},
    {"cbrt", Fn::Cbrt, 1},   {"abs", Fn::Abs, 1},     {"ln", Fn::Ln, 1},       {"log", Fn::Log, 1},
    {"exp", Fn::Exp, 1},     {"floor", Fn::Floor, 1}, {"ceil", Fn::Ceil, 1},   {"round", Fn::Round, 1},
    {"sign", Fn::Sign, 1},   {"sgn", Fn::Sign, 1},    {"min", Fn::Min, 2},     {"max", Fn::Max, 2},
};

const FnInfo* findFunction(const QString& name)
{
    for (const auto& f : kFunctions)
        if (name == QLatin1String(f.name))
            return &f;
    return nullptr;
}

bool isConstant(const QString& name) { return name == QLatin1String("pi") || name == QStringLiteral("π") || name == QLatin1String("e"); }

double constantValue(const QString& name)
{
    if (name == QLatin1String("e"))
        return 2.71828182845904523536;
    return 3.14159265358979323846;
}

// ------------------------------------------------------------------------------------------ lexer

enum class Tok { Number, Ident, Plus, Minus, Star, Slash, Caret, LParen, RParen, Comma, Bar, Root, Bang, Percent, End, Invalid };

struct Token
{
    Tok type = Tok::End;
    double number = 0.0;
    QString text;
    int pos = 0;
};

QString normalize(QString s)
{
    s.replace(QChar(0x00D7), QLatin1Char('*')); // ×
    s.replace(QChar(0x00B7), QLatin1Char('*')); // ·
    s.replace(QChar(0x22C5), QLatin1Char('*')); // ⋅
    s.replace(QChar(0x2217), QLatin1Char('*')); // ∗
    s.replace(QChar(0x00F7), QLatin1Char('/')); // ÷
    s.replace(QChar(0x2212), QLatin1Char('-')); // −
    s.replace(QChar(0x2013), QLatin1Char('-')); // –
    s.replace(QChar(0x00B2), QStringLiteral("^2"));
    s.replace(QChar(0x00B3), QStringLiteral("^3"));
    s.replace(QStringLiteral("**"), QStringLiteral("^"));
    return s;
}

std::vector<Token> tokenize(const QString& input, QString* error)
{
    std::vector<Token> out;
    const QString s = normalize(input);
    int i = 0;
    const int n = s.size();
    while (i < n) {
        const QChar c = s[i];
        if (c.isSpace()) {
            ++i;
            continue;
        }
        Token t;
        t.pos = i;
        if (c.isDigit() || (c == QLatin1Char('.') && i + 1 < n && s[i + 1].isDigit())) {
            int j = i;
            while (j < n && (s[j].isDigit() || s[j] == QLatin1Char('.')))
                ++j;
            // Scientific notation: 1e-3 (only when followed by digits).
            if (j < n && (s[j] == QLatin1Char('E')) && j + 1 < n
                && (s[j + 1].isDigit() || ((s[j + 1] == QLatin1Char('-') || s[j + 1] == QLatin1Char('+')) && j + 2 < n && s[j + 2].isDigit()))) {
                j += 2;
                while (j < n && s[j].isDigit())
                    ++j;
            }
            bool ok = false;
            t.number = s.mid(i, j - i).toDouble(&ok);
            if (!ok) {
                if (error)
                    *error = tr("Invalid number '%1'").arg(s.mid(i, j - i));
                return {};
            }
            t.type = Tok::Number;
            i = j;
        } else if (c.isLetter() || c == QLatin1Char('_')) {
            int j = i;
            while (j < n && (s[j].isLetter() || s[j] == QLatin1Char('_')))
                ++j;
            t.type = Tok::Ident;
            t.text = s.mid(i, j - i);
            i = j;
        } else {
            switch (c.unicode()) {
            case '+': t.type = Tok::Plus; break;
            case '-': t.type = Tok::Minus; break;
            case '*': t.type = Tok::Star; break;
            case '/': t.type = Tok::Slash; break;
            case '^': t.type = Tok::Caret; break;
            case '(': case '[': case '{': t.type = Tok::LParen; break;
            case ')': case ']': case '}': t.type = Tok::RParen; break;
            case ',': case ';': t.type = Tok::Comma; break;
            case '|': t.type = Tok::Bar; break;
            case 0x221A: t.type = Tok::Root; break; // √
            case '!': t.type = Tok::Bang; break;
            case '%': t.type = Tok::Percent; break;
            default:
                if (error)
                    *error = tr("Unexpected character '%1'").arg(c);
                return {};
            }
            ++i;
        }
        out.push_back(t);
    }
    Token end;
    end.type = Tok::End;
    end.pos = n;
    out.push_back(end);
    return out;
}

/// Splits an identifier into known names and single letters ("sinx" -> sin, x; "ab" -> a, b).
QStringList splitIdentifier(const QString& ident)
{
    if (findFunction(ident) || isConstant(ident) || ident.size() == 1)
        return {ident};
    QStringList names;
    for (const auto& f : kFunctions)
        names << QString::fromLatin1(f.name);
    names << QStringLiteral("pi");
    std::sort(names.begin(), names.end(), [](const QString& a, const QString& b) { return a.size() > b.size(); });
    QStringList out;
    int i = 0;
    while (i < ident.size()) {
        bool matched = false;
        for (const QString& name : names) {
            if (name.size() > 1 && ident.midRef(i, name.size()) == name) {
                out << name;
                i += name.size();
                matched = true;
                break;
            }
        }
        if (!matched) {
            out << ident.mid(i, 1);
            ++i;
        }
    }
    return out;
}

// ------------------------------------------------------------------------------------------ AST

struct Node
{
    enum class Kind { Num, Var, Neg, Add, Sub, Mul, Div, Pow, Call };
    Kind kind = Kind::Num;
    double value = 0.0;
    QString name;
    Fn fn = Fn::Sin;
    std::vector<std::unique_ptr<Node>> args;
};

using NodePtr = std::unique_ptr<Node>;

NodePtr makeNum(double v)
{
    auto n = std::make_unique<Node>();
    n->kind = Node::Kind::Num;
    n->value = v;
    return n;
}

NodePtr makeBin(Node::Kind k, NodePtr a, NodePtr b)
{
    auto n = std::make_unique<Node>();
    n->kind = k;
    n->args.push_back(std::move(a));
    n->args.push_back(std::move(b));
    return n;
}

NodePtr makeCall(Fn fn, std::vector<NodePtr> args)
{
    auto n = std::make_unique<Node>();
    n->kind = Node::Kind::Call;
    n->fn = fn;
    n->args = std::move(args);
    return n;
}

class Parser
{
public:
    Parser(std::vector<Token> tokens)
        : m_tokens(std::move(tokens))
    {
    }

    NodePtr parse(QString* error)
    {
        if (m_tokens.size() <= 1) {
            setError(tr("Empty expression"));
        } else {
            NodePtr n = parseExpr();
            if (!m_error.isEmpty() || !n) {
                // fall through
            } else if (peek().type != Tok::End) {
                setError(peek().type == Tok::RParen ? tr("Unexpected ')'") : tr("Unexpected input"));
            } else {
                return n;
            }
        }
        if (error)
            *error = m_error;
        return nullptr;
    }

private:
    const Token& peek() const { return m_tokens[std::min(m_pos, m_tokens.size() - 1)]; }
    Token take() { return m_tokens[std::min(m_pos++, m_tokens.size() - 1)]; }
    void setError(const QString& e)
    {
        if (m_error.isEmpty())
            m_error = e;
    }

    bool startsPrimary(const Token& t) const
    {
        switch (t.type) {
        case Tok::Number:
        case Tok::Ident:
        case Tok::LParen:
        case Tok::Root:
            return true;
        case Tok::Bar:
            return m_absDepth == 0;
        default:
            return false;
        }
    }

    NodePtr parseExpr()
    {
        NodePtr left = parseTerm();
        while (left && (peek().type == Tok::Plus || peek().type == Tok::Minus)) {
            const Tok op = take().type;
            NodePtr right = parseTerm();
            if (!right)
                return nullptr;
            left = makeBin(op == Tok::Plus ? Node::Kind::Add : Node::Kind::Sub, std::move(left), std::move(right));
        }
        return left;
    }

    NodePtr parseTerm()
    {
        NodePtr left = parseUnary();
        while (left) {
            const Tok t = peek().type;
            if (t == Tok::Star || t == Tok::Slash) {
                take();
                NodePtr right = parseUnary();
                if (!right)
                    return nullptr;
                left = makeBin(t == Tok::Star ? Node::Kind::Mul : Node::Kind::Div, std::move(left), std::move(right));
            } else if (startsPrimary(peek())) {
                NodePtr right = parsePower();
                if (!right)
                    return nullptr;
                left = makeBin(Node::Kind::Mul, std::move(left), std::move(right));
            } else {
                break;
            }
        }
        return left;
    }

    NodePtr parseUnary()
    {
        if (peek().type == Tok::Minus) {
            take();
            NodePtr operand = parseUnary();
            if (!operand)
                return nullptr;
            auto n = std::make_unique<Node>();
            n->kind = Node::Kind::Neg;
            n->args.push_back(std::move(operand));
            return n;
        }
        if (peek().type == Tok::Plus) {
            take();
            return parseUnary();
        }
        return parsePower();
    }

    // Postfix operators bind tightest: 3!^2 = (3!)^2, 2^3! = 2^(3!), 50% = 0.5.
    NodePtr parsePostfix(NodePtr operand)
    {
        while (operand && (peek().type == Tok::Bang || peek().type == Tok::Percent)) {
            if (take().type == Tok::Bang) {
                std::vector<NodePtr> args;
                args.push_back(std::move(operand));
                operand = makeCall(Fn::Fact, std::move(args));
            } else {
                operand = makeBin(Node::Kind::Div, std::move(operand), makeNum(100.0));
            }
        }
        return operand;
    }

    NodePtr parsePower()
    {
        NodePtr base = parsePostfix(parsePrimary());
        if (base && peek().type == Tok::Caret) {
            take();
            NodePtr exponent = parseUnary();
            if (!exponent)
                return nullptr;
            return makeBin(Node::Kind::Pow, std::move(base), std::move(exponent));
        }
        return base;
    }

    std::vector<NodePtr> parseArguments(int arity)
    {
        std::vector<NodePtr> args;
        take(); // (
        if (peek().type != Tok::RParen) {
            while (true) {
                NodePtr a = parseExpr();
                if (!a)
                    return {};
                args.push_back(std::move(a));
                if (peek().type == Tok::Comma) {
                    take();
                    continue;
                }
                break;
            }
        }
        if (peek().type != Tok::RParen) {
            setError(tr("Missing ')'"));
            return {};
        }
        take();
        if (static_cast<int>(args.size()) != arity) {
            setError(tr("Wrong number of arguments"));
            return {};
        }
        return args;
    }

    NodePtr applyFunction(const FnInfo& f, const QStringList& rest)
    {
        std::vector<NodePtr> args;
        if (!rest.isEmpty()) {
            // "sinx": the remaining letters form the argument.
            NodePtr arg = namesProduct(rest);
            if (!arg)
                return nullptr;
            args.push_back(std::move(arg));
        } else if (peek().type == Tok::LParen) {
            args = parseArguments(f.arity);
            if (args.empty())
                return nullptr;
            return makeCall(f.fn, std::move(args));
        } else {
            NodePtr arg = parsePower();
            if (!arg) {
                setError(tr("'%1' needs an argument").arg(QString::fromLatin1(f.name)));
                return nullptr;
            }
            args.push_back(std::move(arg));
        }
        if (f.arity != 1) {
            setError(tr("'%1' needs %2 arguments").arg(QString::fromLatin1(f.name)).arg(f.arity));
            return nullptr;
        }
        return makeCall(f.fn, std::move(args));
    }

    NodePtr nameNode(const QString& name)
    {
        if (isConstant(name))
            return makeNum(constantValue(name));
        auto n = std::make_unique<Node>();
        n->kind = Node::Kind::Var;
        n->name = name;
        return n;
    }

    NodePtr namesProduct(const QStringList& names)
    {
        NodePtr result;
        for (int i = 0; i < names.size(); ++i) {
            NodePtr factor;
            if (const FnInfo* f = findFunction(names[i])) {
                factor = applyFunction(*f, names.mid(i + 1));
                if (!factor)
                    return nullptr;
                result = result ? makeBin(Node::Kind::Mul, std::move(result), std::move(factor)) : std::move(factor);
                return result;
            }
            factor = nameNode(names[i]);
            result = result ? makeBin(Node::Kind::Mul, std::move(result), std::move(factor)) : std::move(factor);
        }
        return result;
    }

    NodePtr parsePrimary()
    {
        const Token t = take();
        switch (t.type) {
        case Tok::Number:
            return makeNum(t.number);
        case Tok::LParen: {
            NodePtr e = parseExpr();
            if (!e)
                return nullptr;
            if (peek().type != Tok::RParen) {
                setError(tr("Missing ')'"));
                return nullptr;
            }
            take();
            return e;
        }
        case Tok::Bar: {
            ++m_absDepth;
            NodePtr e = parseExpr();
            --m_absDepth;
            if (!e)
                return nullptr;
            if (peek().type != Tok::Bar) {
                setError(tr("Missing '|'"));
                return nullptr;
            }
            take();
            std::vector<NodePtr> args;
            args.push_back(std::move(e));
            return makeCall(Fn::Abs, std::move(args));
        }
        case Tok::Root: {
            NodePtr arg = parsePrimary();
            if (!arg)
                return nullptr;
            std::vector<NodePtr> args;
            args.push_back(std::move(arg));
            return makeCall(Fn::Sqrt, std::move(args));
        }
        case Tok::Ident:
            return namesProduct(splitIdentifier(t.text));
        case Tok::End:
            setError(tr("Incomplete expression"));
            return nullptr;
        default:
            setError(tr("Unexpected symbol"));
            return nullptr;
        }
    }

    std::vector<Token> m_tokens;
    size_t m_pos = 0;
    int m_absDepth = 0;
    QString m_error;
};

} // namespace

// ---------------------------------------------------------------------------------------- program

struct Expression::Program
{
    enum class Op : unsigned char { Const, Var, Neg, Add, Sub, Mul, Div, Pow, Call1, Call2 };
    struct Instr
    {
        Op op;
        Fn fn;
        int index;
        double value;
    };
    std::vector<Instr> code;
    int maxDepth = 0;
    bool degrees = false;
};

namespace {

void emitNode(const Node& n, Expression::Program& prog, QStringList& vars, int depth, int& maxDepth)
{
    using Op = Expression::Program::Op;
    auto push = [&](Op op, double v = 0.0, int index = 0, Fn fn = Fn::Sin) {
        prog.code.push_back({op, fn, index, v});
    };
    switch (n.kind) {
    case Node::Kind::Num:
        push(Op::Const, n.value);
        maxDepth = std::max(maxDepth, depth + 1);
        return;
    case Node::Kind::Var: {
        int slot = vars.indexOf(n.name);
        if (slot < 0) {
            vars << n.name;
            slot = vars.size() - 1;
        }
        push(Op::Var, 0.0, slot);
        maxDepth = std::max(maxDepth, depth + 1);
        return;
    }
    case Node::Kind::Neg:
        emitNode(*n.args[0], prog, vars, depth, maxDepth);
        push(Op::Neg);
        return;
    case Node::Kind::Call:
        for (size_t i = 0; i < n.args.size(); ++i)
            emitNode(*n.args[i], prog, vars, depth + static_cast<int>(i), maxDepth);
        push(n.args.size() == 2 ? Op::Call2 : Op::Call1, 0.0, 0, n.fn);
        return;
    default:
        emitNode(*n.args[0], prog, vars, depth, maxDepth);
        emitNode(*n.args[1], prog, vars, depth + 1, maxDepth);
        switch (n.kind) {
        case Node::Kind::Add: push(Op::Add); break;
        case Node::Kind::Sub: push(Op::Sub); break;
        case Node::Kind::Mul: push(Op::Mul); break;
        case Node::Kind::Div: push(Op::Div); break;
        case Node::Kind::Pow: push(Op::Pow); break;
        default: break;
        }
        return;
    }
}

constexpr double kPi = 3.14159265358979323846;

// Sine of an angle in degrees, exact at the multiples of 30° and 90° school work uses.
double sinDeg(double d)
{
    double r = std::fmod(d, 360.0);
    if (r < 0)
        r += 360.0;
    if (r == 0.0 || r == 180.0)
        return 0.0;
    if (r == 90.0)
        return 1.0;
    if (r == 270.0)
        return -1.0;
    if (r == 30.0 || r == 150.0)
        return 0.5;
    if (r == 210.0 || r == 330.0)
        return -0.5;
    return std::sin(r * kPi / 180.0);
}

double cosDeg(double d) { return sinDeg(d + 90.0); }

double tanDeg(double d)
{
    double r = std::fmod(d, 180.0);
    if (r < 0)
        r += 180.0;
    if (r == 0.0)
        return 0.0;
    if (r == 90.0)
        return std::nan(""); // undefined
    if (r == 45.0)
        return 1.0;
    if (r == 135.0)
        return -1.0;
    return sinDeg(r) / cosDeg(r);
}

double factorial(double a)
{
    if (a < 0 || a != std::floor(a) || a > 170)
        return a > 170 ? HUGE_VAL : std::nan("");
    double r = 1.0;
    for (int i = 2; i <= static_cast<int>(a); ++i)
        r *= i;
    return r;
}

double call1Degrees(Fn fn, double a)
{
    switch (fn) {
    case Fn::Sin: return sinDeg(a);
    case Fn::Cos: return cosDeg(a);
    case Fn::Tan: return tanDeg(a);
    case Fn::Cot: return 1.0 / tanDeg(a);
    case Fn::Sec: return 1.0 / cosDeg(a);
    case Fn::Csc: return 1.0 / sinDeg(a);
    case Fn::Asin: return std::asin(a) * 180.0 / kPi;
    case Fn::Acos: return std::acos(a) * 180.0 / kPi;
    case Fn::Atan: return std::atan(a) * 180.0 / kPi;
    default: return std::nan("");
    }
}

double call1(Fn fn, double a, bool degrees)
{
    if (degrees && (fn <= Fn::Atan))
        return call1Degrees(fn, a);
    switch (fn) {
    case Fn::Sin: return std::sin(a);
    case Fn::Cos: return std::cos(a);
    case Fn::Tan: return std::tan(a);
    case Fn::Cot: return 1.0 / std::tan(a);
    case Fn::Sec: return 1.0 / std::cos(a);
    case Fn::Csc: return 1.0 / std::sin(a);
    case Fn::Asin: return std::asin(a);
    case Fn::Acos: return std::acos(a);
    case Fn::Atan: return std::atan(a);
    case Fn::Sinh: return std::sinh(a);
    case Fn::Cosh: return std::cosh(a);
    case Fn::Tanh: return std::tanh(a);
    case Fn::Sqrt: return std::sqrt(a);
    case Fn::Cbrt: return std::cbrt(a);
    case Fn::Abs: return std::abs(a);
    case Fn::Ln: return std::log(a);
    case Fn::Log: return std::log10(a);
    case Fn::Exp: return std::exp(a);
    case Fn::Floor: return std::floor(a);
    case Fn::Ceil: return std::ceil(a);
    case Fn::Round: return std::round(a);
    case Fn::Sign: return a > 0 ? 1.0 : (a < 0 ? -1.0 : 0.0);
    case Fn::Fact: return factorial(a);
    default: return std::nan("");
    }
}

double power(double a, double b)
{
    // Real odd roots of negative numbers, e.g. x^(1/3) for x < 0.
    if (a < 0 && std::floor(b) != b) {
        const double inv = 1.0 / b;
        if (std::abs(inv - std::round(inv)) < 1e-9 && static_cast<long long>(std::round(inv)) % 2 != 0)
            return -std::pow(-a, b);
    }
    return std::pow(a, b);
}

} // namespace

Expression::Expression() = default;

Expression Expression::compile(const QString& text, QString* error)
{
    return compile(text, error, Options());
}

Expression Expression::compile(const QString& text, QString* error, const Options& options)
{
    Expression e;
    e.m_source = text;
    QString err;
    std::vector<Token> tokens = tokenize(text, &err);
    if (tokens.empty()) {
        if (error)
            *error = err.isEmpty() ? tr("Empty expression") : err;
        return e;
    }
    Parser parser(std::move(tokens));
    NodePtr root = parser.parse(&err);
    if (!root) {
        if (error)
            *error = err;
        return e;
    }
    auto prog = std::make_shared<Program>();
    prog->degrees = options.degrees;
    QStringList vars;
    emitNode(*root, *prog, vars, 0, prog->maxDepth);
    e.m_program = prog;
    e.m_variables = vars;
    if (error)
        error->clear();
    return e;
}

double Expression::evaluate(const double* values, int count) const
{
    if (!m_program)
        return std::nan("");
    const Program& p = *m_program;
    double local[64];
    std::vector<double> heap;
    double* stack = local;
    if (p.maxDepth + 2 > 64) {
        heap.resize(static_cast<size_t>(p.maxDepth + 2));
        stack = heap.data();
    }
    int sp = 0;
    for (const Program::Instr& in : p.code) {
        switch (in.op) {
        case Program::Op::Const: stack[sp++] = in.value; break;
        case Program::Op::Var: stack[sp++] = in.index < count ? values[in.index] : 0.0; break;
        case Program::Op::Neg: stack[sp - 1] = -stack[sp - 1]; break;
        case Program::Op::Add: --sp; stack[sp - 1] += stack[sp]; break;
        case Program::Op::Sub: --sp; stack[sp - 1] -= stack[sp]; break;
        case Program::Op::Mul: --sp; stack[sp - 1] *= stack[sp]; break;
        case Program::Op::Div: --sp; stack[sp - 1] /= stack[sp]; break;
        case Program::Op::Pow: --sp; stack[sp - 1] = power(stack[sp - 1], stack[sp]); break;
        case Program::Op::Call1: stack[sp - 1] = call1(in.fn, stack[sp - 1], p.degrees); break;
        case Program::Op::Call2:
            --sp;
            stack[sp - 1] = in.fn == Fn::Min ? std::min(stack[sp - 1], stack[sp]) : std::max(stack[sp - 1], stack[sp]);
            break;
        }
    }
    return sp == 1 ? stack[0] : std::nan("");
}

double Expression::evaluate(const QHash<QString, double>& values) const
{
    QVector<double> slotVals(m_variables.size(), 0.0);
    for (int i = 0; i < m_variables.size(); ++i)
        slotVals[i] = values.value(m_variables[i], 0.0);
    return evaluate(slotVals.constData(), slotVals.size());
}

QString Expression::stripDefinition(const QString& text, QString* name)
{
    static const QRegularExpression re(QStringLiteral("^\\s*([A-Za-z])\\s*(\\(\\s*[A-Za-z]\\s*\\))?\\s*=\\s*(.*)$"));
    const QRegularExpressionMatch m = re.match(text);
    if (m.hasMatch()) {
        if (name)
            *name = m.captured(1);
        return m.captured(3);
    }
    if (name)
        name->clear();
    return text;
}

} // namespace cb::math
