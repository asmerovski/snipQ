#include "highlighter.h"
#include <QTextDocument>


// ─── Public helpers ──────────────────────────────────────────────────────────

QTextCharFormat Highlighter::hlFmt(const QColor& c, bool bold, bool italic)
{
    QTextCharFormat f;
    f.setForeground(c);
    if (bold)   f.setFontWeight(QFont::Bold);
    if (italic) f.setFontItalic(true);
    return f;
}

void Highlighter::addHlRule(const QString& pattern, const QTextCharFormat& f, int cap)
{
    Rule r;
    r.pattern      = QRegularExpression(pattern);
    r.format       = f;
    r.captureGroup = cap;
    m_rules.append(r);
}

void Highlighter::addHlKeywords(const QStringList& words, const QTextCharFormat& f)
{
    for (const QString& w : words)
        addHlRule(QStringLiteral("\\b") + w + QStringLiteral("\\b"), f);
}

void Highlighter::addMultiLineComment(const Highlighter::MultiLineComment& mlc)
{
    m_mlComments.append(mlc);
}

void Highlighter::clearRules()
{
    m_rules.clear();
    m_mlComments.clear();
}

// ─── Constructor ─────────────────────────────────────────────────────────────

Highlighter::Highlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    setLanguage("plaintext");
}

void Highlighter::setLanguage(const QString& lang)
{
    m_lang = lang.toLower();
    clearRules();

    if      (m_lang == "c")           setupC();
    else if (m_lang == "cpp"    ||
             m_lang == "c++")         setupCpp();
    else if (m_lang == "csharp" ||
             m_lang == "c#")          setupCsharp();
    else if (m_lang == "java")        setupJava();
    else if (m_lang == "javascript"||
             m_lang == "js")          setupJavaScript();
    else if (m_lang == "typescript"||
             m_lang == "ts")          setupTypeScript();
    else if (m_lang == "python")      setupPython();
    else if (m_lang == "rust")        setupRust();
    else if (m_lang == "go")          setupGo();
    else if (m_lang == "kotlin")      setupKotlin();
    else if (m_lang == "swift")       setupSwift();
    else if (m_lang == "scala")       setupScala();
    else if (m_lang == "dart")        setupDart();
    else if (m_lang == "ruby")        setupRuby();
    else if (m_lang == "php")         setupPhp();
    else if (m_lang == "bash")        setupBash();
    else if (m_lang == "shell")       setupShell();
    else if (m_lang == "powershell")  setupPowerShell();
    else if (m_lang == "sql")         setupSql();
    else if (m_lang == "json")        setupJson();
    else if (m_lang == "xml")         setupXml();
    else if (m_lang == "html")        setupHtml();
    else if (m_lang == "css")         setupCss();
    else if (m_lang == "yaml")        setupYaml();
    else if (m_lang == "toml")        setupToml();
    else if (m_lang == "markdown"||
             m_lang == "md")          setupMarkdown();
    else if (m_lang == "diff")        setupDiff();
    else if (m_lang == "dockerfile")  setupDockerfile();
    else if (m_lang == "makefile")    setupMakefile();
    else if (m_lang == "nginx")       setupNginx();
    else if (m_lang == "latex")       setupLatex();
    else if (m_lang == "lua")         setupLua();
    else if (m_lang == "r")           setupR();
    else if (m_lang == "graphql")     setupGraphql();
    else if (m_lang == "http")        setupHttp();
    else if (m_lang == "objectivec"||
             m_lang == "objc")        setupObjectiveC();
    else                              setupPlaintext();

    rehighlight();
}

// ─── Core highlighter ─────────────────────────────────────────────────────────

void Highlighter::highlightBlock(const QString& text)
{
    setCurrentBlockState(-1);

    // ── Multi-line spans (comments, triple-quoted strings) ────
    // Each mlc has a unique stateId. We check if the previous block
    // ended inside any of them, or if one starts on this line.
    for (const auto& mlc : m_mlComments) {
        int prevState = previousBlockState();
        int startIndex = 0;

        if (prevState == mlc.stateId) {
            // We are continuing inside this span from the previous block
            startIndex = 0;
        } else {
            // Look for the opening delimiter on this line
            startIndex = text.indexOf(mlc.start);
            if (startIndex < 0)
                continue; // this mlc doesn't apply to this line, try next
        }

        while (startIndex >= 0) {
            // Search for closing delimiter after the opening
            int searchFrom = (prevState == mlc.stateId)
                             ? startIndex
                             : startIndex + mlc.start.pattern().length();
            auto endMatch = mlc.end.match(text, searchFrom);
            int  endIndex = endMatch.capturedStart();
            int  len;
            if (endIndex < 0) {
                // Span continues into next block
                setCurrentBlockState(mlc.stateId);
                len = text.length() - startIndex;
                setFormat(startIndex, len, mlc.format);
                return; // rest of line is inside this span
            } else {
                len = endIndex - startIndex + endMatch.capturedLength();
                setFormat(startIndex, len, mlc.format);
            }
            prevState  = -1; // no longer in a "previous" continuation
            startIndex = text.indexOf(mlc.start, startIndex + len);
        }
        // If we formatted anything from this mlc, we can still fall through
        // to single-line rules for the unformatted portions.
    }

    // ── Single-line rules ─────────────────────────────────────
    for (const Rule& rule : m_rules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            int  start = match.capturedStart(rule.captureGroup);
            int  len   = match.capturedLength(rule.captureGroup);
            if (start >= 0 && len > 0)
                setFormat(start, len, rule.format);
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// Language definitions
// ══════════════════════════════════════════════════════════════════════════════

// Shared number rule used by many languages
static const QString NUM_RE =
    R"(\b(0x[0-9A-Fa-f]+|0b[01]+|0o[0-7]+|\d+\.?\d*([eE][+-]?\d+)?[fFuUlL]*)\b)";

// Shared string rules
static void addStrings(Highlighter* h, const QTextCharFormat& sf)
{
    // Double-quoted
    h->addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    // Single-quoted
    h->addHlRule(R"('(?:[^'\\]|\\.)*')", sf);
    // Template literals (JS/TS)
    h->addHlRule(R"(`(?:[^`\\]|\\.)*`)", sf);
}

// ─── C shared base (used by C, C++, ObjC) ────────────────────────────────────

static void setupCBase(Highlighter* h)
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto pp = Highlighter::hlFmt(Preprocessor);
    auto es = Highlighter::hlFmt(Escape);

    // Preprocessor
    h->addHlRule(R"(^\s*#\s*\w+)", pp);

    h->addHlKeywords({
        "auto","break","case","continue","default","do","else","enum",
        "extern","for","goto","if","inline","register","return","sizeof",
        "static","struct","switch","typedef","union","volatile","while",
        "const","restrict","_Alignas","_Alignof","_Atomic","_Bool",
        "_Complex","_Generic","_Imaginary","_Noreturn","_Static_assert",
        "_Thread_local","nullptr","true","false"
    }, kw);

    h->addHlKeywords({
        "void","int","char","short","long","float","double","unsigned",
        "signed","size_t","ssize_t","ptrdiff_t","intptr_t","uintptr_t",
        "int8_t","int16_t","int32_t","int64_t",
        "uint8_t","uint16_t","uint32_t","uint64_t",
        "bool","FILE","NULL"
    }, ty);

    // Function calls
    h->addHlRule(R"(\b([A-Za-z_]\w*)\s*(?=\())", fn, 1);
    // Strings + escape seqs
    h->addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    h->addHlRule(R"('(?:[^'\\]|\\.)+')", sf);
    h->addHlRule(R"(\\[nrtabfv\\'"0])", es);
    // Numbers
    h->addHlRule(NUM_RE, nm);
}

void Highlighter::setupC()
{
    using namespace HlColor;
    auto cm = Highlighter::hlFmt(Comment, false, true);

    setupCBase(this);
    addHlRule(R"(//[^\n]*)", cm);
    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression(R"(/\*)");
    mlc.end     = QRegularExpression(R"(\*/)");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);
}

void Highlighter::setupCpp()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto cm = Highlighter::hlFmt(Comment, false, true);

    setupCBase(this);

    addHlKeywords({
        "class","template","typename","namespace","using","public","private",
        "protected","virtual","override","final","explicit","friend","operator",
        "new","delete","try","catch","throw","noexcept","constexpr","consteval",
        "constinit","concept","requires","co_await","co_return","co_yield",
        "decltype","static_assert","this","nullptr","export","import","module"
    }, kw);

    addHlKeywords({
        "string","wstring","vector","list","map","unordered_map","set",
        "unordered_set","pair","tuple","optional","variant","any","array",
        "deque","queue","stack","priority_queue","shared_ptr","unique_ptr",
        "weak_ptr","function","thread","mutex","atomic","iostream","ostream",
        "istream","fstream","stringstream","auto"
    }, ty);

    addHlRule(R"(//[^\n]*)", cm);
    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression(R"(/\*)");
    mlc.end     = QRegularExpression(R"(\*/)");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);
}

void Highlighter::setupObjectiveC()
{
    setupCpp();
    using namespace HlColor;
    addHlKeywords({
        "@interface","@implementation","@end","@property","@synthesize",
        "@dynamic","@protocol","@required","@optional","@class","@selector",
        "@encode","@synchronized","@autoreleasepool","@try","@catch","@finally",
        "@throw","@import","@available","nil","YES","NO","BOOL","NSInteger",
        "NSUInteger","CGFloat","NSString","NSArray","NSDictionary","NSObject"
    }, Highlighter::hlFmt(Keyword, true));
}

void Highlighter::setupCsharp()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto pp = Highlighter::hlFmt(Preprocessor);

    addHlRule(R"(^\s*#\s*\w+)", pp);
    addHlRule(R"(//[^\n]*)", cm);
    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression(R"(/\*)");
    mlc.end     = QRegularExpression(R"(\*/)");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);

    addHlKeywords({
        "abstract","as","base","break","case","catch","checked","class",
        "const","continue","default","delegate","do","else","enum","event",
        "explicit","extern","finally","fixed","for","foreach","goto","if",
        "implicit","in","interface","internal","is","lock","namespace","new",
        "null","operator","out","override","params","partial","private",
        "protected","public","readonly","record","ref","return","sealed",
        "sizeof","stackalloc","static","struct","switch","this","throw","try",
        "typeof","unchecked","unsafe","using","virtual","void","volatile",
        "while","async","await","yield","when","with","init","required",
        "file","scoped","true","false"
    }, kw);

    addHlKeywords({
        "bool","byte","char","decimal","double","float","int","long","object",
        "sbyte","short","string","uint","ulong","ushort","dynamic","var",
        "nint","nuint","Task","List","Dictionary","IEnumerable","IList",
        "ICollection","Action","Func","Tuple","ValueTuple","Nullable",
        "StringBuilder","Exception","Console","Math","Type","Attribute"
    }, ty);

    addHlRule(R"(\[([A-Za-z]\w*(?:\(.*\))?)\])", Highlighter::hlFmt(Preprocessor));
    addHlRule(R"(\b([A-Za-z_]\w*)\s*(?=\())", fn, 1);
    addHlRule(R"(@?"(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'\\]|\\.)')", sf);
    addHlRule(NUM_RE, nm);
}

void Highlighter::setupJava()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto pp = Highlighter::hlFmt(Preprocessor);

    addHlRule(R"(//[^\n]*)", cm);
    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression(R"(/\*)");
    mlc.end     = QRegularExpression(R"(\*/)");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);

    addHlKeywords({
        "abstract","assert","break","case","catch","class","continue","default",
        "do","else","enum","extends","final","finally","for","goto","if",
        "implements","import","instanceof","interface","module","native","new",
        "null","package","private","protected","public","record","return",
        "sealed","static","strictfp","super","switch","synchronized","this",
        "throw","throws","transient","try","var","volatile","while",
        "true","false","permits","yield","non-sealed"
    }, kw);

    addHlKeywords({
        "boolean","byte","char","double","float","int","long","short","void",
        "String","Integer","Long","Double","Float","Boolean","Character","Byte",
        "Short","Object","Class","System","Math","List","ArrayList","HashMap",
        "Map","Set","HashSet","Optional","Stream","StringBuilder","Exception",
        "RuntimeException","Thread","Runnable","Callable","Future"
    }, ty);

    // Annotations
    addHlRule(R"(@[A-Za-z]\w*)", pp);
    addHlRule(R"(\b([A-Za-z_]\w*)\s*(?=\())", fn, 1);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'\\]|\\.)')", sf);
    addHlRule(NUM_RE, nm);
}

void Highlighter::setupJavaScript()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto co = Highlighter::hlFmt(Constant, true);
    auto pp = Highlighter::hlFmt(Preprocessor);

    addHlRule(R"(//[^\n]*)", cm);
    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression(R"(/\*)");
    mlc.end     = QRegularExpression(R"(\*/)");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);

    addHlKeywords({
        "async","await","break","case","catch","class","const","continue",
        "debugger","default","delete","do","else","export","extends","finally",
        "for","from","function","generator","if","import","in","instanceof",
        "let","new","of","return","static","super","switch","target","this",
        "throw","try","typeof","var","void","while","with","yield","as","get","set"
    }, kw);

    addHlKeywords({"true","false","null","undefined","NaN","Infinity"}, co);

    addHlKeywords({
        "Array","Boolean","Date","Error","Function","JSON","Map","Math",
        "Number","Object","Promise","Proxy","Reflect","RegExp","Set",
        "String","Symbol","WeakMap","WeakSet","console","document","window",
        "globalThis","process","module","require","exports"
    }, ty);

    // Arrow function / named function
    addHlRule(R"(\b(function\*?)\s+([A-Za-z_$]\w*))", Highlighter::hlFmt(Function), 2);
    addHlRule(R"(\b([A-Za-z_$]\w*)\s*(?=\())", fn, 1);
    // Template literals
    addHlRule(R"(`(?:[^`\\]|\\.)*`)", sf);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'\\]|\\.)*')", sf);
    addHlRule(NUM_RE, nm);
    // Decorators / JSX
    addHlRule(R"(@[A-Za-z]\w*)", pp);
}

void Highlighter::setupTypeScript()
{
    setupJavaScript();
    using namespace HlColor;
    addHlKeywords({
        "abstract","as","declare","enum","implements","interface","is",
        "keyof","module","namespace","never","override","private","protected",
        "public","readonly","satisfies","type","typeof","unknown","infer",
        "asserts","accessor","using","override"
    }, Highlighter::hlFmt(Keyword, true));
    addHlKeywords({
        "any","bigint","boolean","never","number","object","string","symbol",
        "undefined","unknown","void","Record","Partial","Required","Readonly",
        "Pick","Omit","Exclude","Extract","NonNullable","ReturnType",
        "InstanceType","Parameters","ConstructorParameters"
    }, Highlighter::hlFmt(Type));
}

void Highlighter::setupPython()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto co = Highlighter::hlFmt(Constant, true);
    auto pp = Highlighter::hlFmt(Preprocessor);

    // Comments
    addHlRule(R"(#[^\n]*)", cm);

    addHlKeywords({
        "and","as","assert","async","await","break","class","continue",
        "def","del","elif","else","except","finally","for","from","global",
        "if","import","in","is","lambda","match","case","nonlocal","not",
        "or","pass","raise","return","try","while","with","yield"
    }, kw);

    addHlKeywords({"True","False","None"}, co);

    addHlKeywords({
        "abs","all","any","bin","bool","bytes","callable","chr","classmethod",
        "compile","complex","delattr","dict","dir","divmod","enumerate","eval",
        "exec","filter","float","format","frozenset","getattr","globals",
        "hasattr","hash","help","hex","id","input","int","isinstance",
        "issubclass","iter","len","list","locals","map","max","memoryview",
        "min","next","object","oct","open","ord","pow","print","property",
        "range","repr","reversed","round","set","setattr","slice","sorted",
        "staticmethod","str","sum","super","tuple","type","vars","zip",
        "Exception","ValueError","TypeError","KeyError","IndexError",
        "AttributeError","RuntimeError","StopIteration","GeneratorExit",
        "NotImplementedError","OSError","IOError"
    }, ty);

    // Decorators
    addHlRule(R"(^\s*@[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*)", pp);
    // Function name after def
    addHlRule(R"(\bdef\s+([A-Za-z_]\w*))", fn, 1);
    // Class name after class
    addHlRule(R"(\bclass\s+([A-Za-z_]\w*))", Highlighter::hlFmt(Type, true), 1);
    // f-strings and regular strings (triple-quoted handled by multi-line state)
    addHlRule(R"([fFrRbBuU]?"(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"([fFrRbBuU]?'(?:[^'\\]|\\.)*')", sf);
    addHlRule(NUM_RE, nm);

    // Triple-quoted strings as multi-line
    Highlighter::MultiLineComment tdq, tsq;
    tdq.start   = QRegularExpression(R"(""")");
    tdq.end     = QRegularExpression(R"(""")");
    tdq.format  = sf;
    tdq.stateId = 2;
    m_mlComments.append(tdq);

    tsq.start   = QRegularExpression(R"(''')");
    tsq.end     = QRegularExpression(R"(''')");
    tsq.format  = sf;
    tsq.stateId = 3;
    m_mlComments.append(tsq);
}

void Highlighter::setupRust()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto co = Highlighter::hlFmt(Constant, true);
    auto pp = Highlighter::hlFmt(Preprocessor);
    auto lt = Highlighter::hlFmt(Attribute); // lifetimes

    addHlRule(R"(//[^\n]*)", cm);
    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression(R"(/\*)");
    mlc.end     = QRegularExpression(R"(\*/)");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);

    addHlKeywords({
        "as","async","await","break","const","continue","crate","dyn","else",
        "enum","extern","false","fn","for","if","impl","in","let","loop",
        "match","mod","move","mut","pub","ref","return","self","Self","static",
        "struct","super","trait","true","type","union","unsafe","use","where",
        "while","yield","abstract","become","box","do","final","macro",
        "override","priv","try","typeof","unsized","virtual"
    }, kw);

    addHlKeywords({"true","false","None","Some","Ok","Err"}, co);

    addHlKeywords({
        "i8","i16","i32","i64","i128","isize","u8","u16","u32","u64","u128",
        "usize","f32","f64","bool","char","str","String","Vec","HashMap",
        "HashSet","BTreeMap","BTreeSet","Option","Result","Box","Rc","Arc",
        "RefCell","Cell","Mutex","RwLock","Pin","PhantomData","Iterator",
        "IntoIterator","From","Into","AsRef","AsMut","Clone","Copy","Send",
        "Sync","Sized","Default","Debug","Display","PartialEq","Eq",
        "PartialOrd","Ord","Hash","Drop","std","core","alloc"
    }, ty);

    // Attributes / macros
    addHlRule(R"(#\[(?:[^\]]*)\])", pp);
    addHlRule(R"(\b[a-z_]\w*!(?=\s*[\(\[\{]))", pp); // macro calls
    // Lifetimes
    addHlRule(R"('[a-z_]\w*\b)", lt);
    // Function name after fn
    addHlRule(R"(\bfn\s+([A-Za-z_]\w*))", fn, 1);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'\\]|\\.)')", sf);  // char literals
    addHlRule(R"(r#*".*?"#*)", sf);         // raw strings (simplified)
    addHlRule(NUM_RE, nm);
}

void Highlighter::setupGo()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto co = Highlighter::hlFmt(Constant, true);

    addHlRule(R"(//[^\n]*)", cm);
    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression(R"(/\*)");
    mlc.end     = QRegularExpression(R"(\*/)");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);

    addHlKeywords({
        "break","case","chan","const","continue","default","defer","else",
        "fallthrough","for","func","go","goto","if","import","interface",
        "map","package","range","return","select","struct","switch","type",
        "var"
    }, kw);

    addHlKeywords({"true","false","nil","iota"}, co);

    addHlKeywords({
        "bool","byte","complex64","complex128","error","float32","float64",
        "int","int8","int16","int32","int64","rune","string","uint","uint8",
        "uint16","uint32","uint64","uintptr","any","comparable",
        "append","cap","clear","close","complex","copy","delete","imag",
        "len","make","max","min","new","panic","print","println","real",
        "recover","string"
    }, ty);

    addHlRule(R"(\bfunc\s+(?:\([^)]*\)\s*)?([A-Za-z_]\w*))", fn, 1);
    addHlRule(R"(`(?:[^`])*`)", sf);   // raw string
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'\\]|\\.)')", sf);
    addHlRule(NUM_RE, nm);
}

void Highlighter::setupKotlin()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto co = Highlighter::hlFmt(Constant, true);
    auto pp = Highlighter::hlFmt(Preprocessor);

    addHlRule(R"(//[^\n]*)", cm);
    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression(R"(/\*)");
    mlc.end     = QRegularExpression(R"(\*/)");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);

    addHlKeywords({
        "abstract","actual","annotation","as","break","by","catch","class",
        "companion","const","constructor","continue","crossinline","data",
        "delegate","do","dynamic","else","enum","expect","external","finally",
        "for","fun","get","if","import","in","infix","init","inline","inner",
        "interface","internal","is","it","lateinit","noinline","object",
        "open","operator","out","override","package","private","protected",
        "public","reified","return","sealed","set","super","suspend","tailrec",
        "this","throw","try","typealias","typeof","val","value","var","vararg",
        "when","where","while"
    }, kw);

    addHlKeywords({"true","false","null"}, co);
    addHlKeywords({
        "Any","Array","Boolean","Byte","Char","Double","Float","Int","Long",
        "Nothing","Number","Short","String","Unit","List","MutableList","Map",
        "MutableMap","Set","MutableSet","Pair","Triple","Sequence",
        "Collection","Iterable","Exception","Throwable","Comparable"
    }, ty);

    addHlRule(R"(@[A-Za-z]\w*)", pp);
    addHlRule(R"(\bfun\s+([A-Za-z_]\w*))", fn, 1);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'\\]|\\.)')", sf);
    addHlRule(R"("""(?:(?!""")[\s\S])*""")", sf);
    addHlRule(NUM_RE, nm);
}

void Highlighter::setupSwift()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto co = Highlighter::hlFmt(Constant, true);
    auto pp = Highlighter::hlFmt(Preprocessor);

    addHlRule(R"(//[^\n]*)", cm);
    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression(R"(/\*)");
    mlc.end     = QRegularExpression(R"(\*/)");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);

    addHlKeywords({
        "actor","any","as","associatedtype","async","await","break","case",
        "catch","class","continue","convenience","default","defer","deinit",
        "didSet","do","dynamic","else","enum","extension","fallthrough",
        "fileprivate","final","for","func","get","guard","if","import","in",
        "indirect","infix","init","inout","internal","is","isolated","lazy",
        "let","mutating","nonisolated","nonmutating","open","operator",
        "optional","override","postfix","precedencegroup","prefix","private",
        "protocol","public","repeat","rethrows","return","self","set",
        "some","static","struct","subscript","super","switch","throw",
        "throws","try","typealias","unowned","var","weak","where","while",
        "willSet","@discardableResult","@objc","@MainActor"
    }, kw);

    addHlKeywords({"true","false","nil"}, co);
    addHlKeywords({
        "Any","AnyObject","Array","Bool","Character","Dictionary","Double",
        "Float","Int","Optional","Set","String","Void","Error","Never",
        "Result","Codable","Encodable","Decodable","Hashable","Equatable",
        "Comparable","Identifiable","ObservableObject","Published"
    }, ty);

    addHlRule(R"(@\w+)", pp);
    addHlRule(R"(\bfunc\s+([A-Za-z_]\w*))", fn, 1);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(NUM_RE, nm);
}

void Highlighter::setupScala()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto co = Highlighter::hlFmt(Constant, true);
    auto pp = Highlighter::hlFmt(Preprocessor);

    addHlRule(R"(//[^\n]*)", cm);
    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression(R"(/\*)");
    mlc.end     = QRegularExpression(R"(\*/)");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);

    addHlKeywords({
        "abstract","case","catch","class","def","do","else","enum","export",
        "extends","final","finally","for","forSome","given","if","implicit",
        "import","lazy","match","new","object","override","package","private",
        "protected","return","sealed","super","then","this","throw","trait",
        "transparent","try","type","using","val","var","while","with","yield"
    }, kw);

    addHlKeywords({"true","false","null","None","Some","Nil"}, co);
    addHlKeywords({
        "Any","AnyRef","AnyVal","Array","Boolean","Byte","Char","Double",
        "Float","Int","List","Long","Map","Nothing","Option","Seq","Set",
        "Short","String","Unit","Vector","Either","Try","Future","IO"
    }, ty);

    addHlRule(R"(@[A-Za-z]\w*)", pp);
    addHlRule(R"(\bdef\s+([A-Za-z_]\w*))", fn, 1);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'\\]|\\.)')", sf);
    addHlRule(R"("""(?:(?!""")[\s\S])*""")", sf);
    addHlRule(NUM_RE, nm);
}

void Highlighter::setupDart()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto co = Highlighter::hlFmt(Constant, true);
    auto pp = Highlighter::hlFmt(Preprocessor);

    addHlRule(R"(//[^\n]*)", cm);
    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression(R"(/\*)");
    mlc.end     = QRegularExpression(R"(\*/)");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);

    addHlKeywords({
        "abstract","as","assert","async","await","base","break","case","catch",
        "class","const","continue","covariant","default","deferred","do","else",
        "enum","export","extends","extension","external","factory","final",
        "finally","for","Function","get","hide","if","implements","import","in",
        "interface","is","late","library","mixin","new","on","operator","part",
        "required","rethrow","return","sealed","set","show","static","super",
        "switch","sync","this","throw","try","typedef","var","void","when",
        "with","while","yield"
    }, kw);

    addHlKeywords({"true","false","null"}, co);
    addHlKeywords({
        "bool","double","dynamic","int","List","Map","Never","Null","num",
        "Object","Set","String","Symbol","Type","Uri","BigInt","DateTime",
        "Duration","Future","Iterable","Iterator","Pattern","RegExp",
        "Stream","StringBuffer","Completer"
    }, ty);

    addHlRule(R"(@[A-Za-z]\w*)", pp);
    addHlRule(R"(\b([A-Za-z_]\w*)\s*(?=\())", fn, 1);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'\\]|\\.)*')", sf);
    addHlRule(NUM_RE, nm);
}

void Highlighter::setupRuby()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto co = Highlighter::hlFmt(Constant, true);
    auto sy = Highlighter::hlFmt(Attribute);  // symbols

    addHlRule(R"(#[^\n]*)", cm);
    addHlKeywords({
        "alias","and","BEGIN","begin","break","case","class","def","defined?",
        "do","else","elsif","END","end","ensure","for","if","in","module",
        "next","nil","not","or","puts","raise","redo","rescue","retry","return",
        "self","super","then","unless","until","when","while","yield","attr",
        "attr_reader","attr_writer","attr_accessor","private","protected",
        "public","require","require_relative","include","extend","prepend",
        "lambda","proc","block_given?"
    }, kw);

    addHlKeywords({"true","false","nil"}, co);
    addHlKeywords({
        "Array","Class","Comparable","Enumerable","Exception","Float","Hash",
        "Integer","IO","Kernel","Module","Numeric","Object","Range","Regexp",
        "RuntimeError","StandardError","String","Symbol","Thread","Time"
    }, ty);

    addHlRule(R"(:(?:[A-Za-z_]\w*|"[^"]*"|'[^']*'))", sy);
    addHlRule(R"(\bdef\s+(?:self\.)?([A-Za-z_]\w*[!?]?))", fn, 1);
    addHlRule(R"("(?:[^"\\#]|#\{[^}]*\}|\\.)*")", sf);
    addHlRule(R"('(?:[^'\\]|\\.)*')", sf);
    addHlRule(NUM_RE, nm);
}

void Highlighter::setupPhp()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto co = Highlighter::hlFmt(Constant, true);
    auto pp = Highlighter::hlFmt(Preprocessor);
    auto va = Highlighter::hlFmt(Attribute);  // variables

    addHlRule(R"(//[^\n]*|#[^\n]*)", cm);
    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression(R"(/\*)");
    mlc.end     = QRegularExpression(R"(\*/)");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);

    addHlRule(R"(<\?php|\?>)", pp);
    // Variables
    addHlRule(R"(\$[A-Za-z_]\w*)", va);

    addHlKeywords({
        "abstract","and","array","as","break","callable","case","catch",
        "class","clone","const","continue","declare","default","die","do",
        "echo","else","elseif","empty","enddeclare","endfor","endforeach",
        "endif","endswitch","endwhile","enum","eval","exit","extends",
        "final","finally","fn","for","foreach","function","global","goto",
        "if","implements","include","include_once","instanceof","insteadof",
        "interface","isset","list","match","namespace","new","or","print",
        "private","protected","public","readonly","require","require_once",
        "return","static","switch","throw","trait","try","unset","use",
        "var","while","xor","yield","__halt_compiler"
    }, kw);

    addHlKeywords({"true","false","null","TRUE","FALSE","NULL"}, co);
    addHlKeywords({
        "int","float","bool","string","array","object","callable","void",
        "never","mixed","iterable","self","parent","static","null",
        "Exception","Error","Throwable","stdClass","DateTime","DateInterval",
        "SplStack","SplQueue","ArrayObject","ArrayIterator","Generator"
    }, ty);

    addHlRule(R"(\b([A-Za-z_]\w*)\s*(?=\())", fn, 1);
    addHlRule(R"("(?:[^"\\$]|\\.|\$(?!\{)|\$\{[^}]*\})*")", sf);
    addHlRule(R"('(?:[^'\\]|\\.)*')", sf);
    addHlRule(NUM_RE, nm);
}

void Highlighter::setupBash()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto va = Highlighter::hlFmt(Attribute);
    auto pp = Highlighter::hlFmt(Preprocessor);

    addHlRule(R"(#![^\n]*)", pp);   // shebang
    addHlRule(R"(#[^\n]*)", cm);

    addHlKeywords({
        "if","then","else","elif","fi","for","while","until","do","done",
        "case","esac","function","select","in","break","continue","return",
        "exit","trap","exec","eval","source","export","local","declare",
        "readonly","unset","shift","getopts","set","shopt","alias","unalias",
        "command","builtin","type","which","true","false"
    }, kw);

    addHlKeywords({
        "echo","printf","read","test","[","[[","cd","ls","pwd","mkdir","rm",
        "cp","mv","cat","grep","sed","awk","find","sort","uniq","cut","tr",
        "head","tail","wc","xargs","curl","wget","ssh","scp","git","make",
        "sudo","chmod","chown"
    }, ty);

    // Variables
    addHlRule(R"(\$(?:\{[^}]*\}|[A-Za-z_]\w*|\d+|[@#?$!*-]))", va);
    addHlRule(R"(\b([A-Za-z_]\w*)\s*\(\))", fn, 1);
    addHlRule(R"("(?:[^"\\$]|\\.|\$[^(])*")", sf);
    addHlRule(R"('(?:[^'])*')", sf);
    addHlRule(R"(\b\d+\b)", nm);
}

void Highlighter::setupShell()    { setupBash(); }

void Highlighter::setupPowerShell()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto va = Highlighter::hlFmt(Attribute);
    auto pp = Highlighter::hlFmt(Preprocessor);

    addHlRule(R"(#[^\n]*)", cm);
    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression(R"(<#)");
    mlc.end     = QRegularExpression(R"(#>)");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);

    addHlKeywords({
        "Begin","Break","Catch","Class","Continue","Data","Define","Do",
        "DynamicParam","Else","ElseIf","End","Enum","Exit","Filter",
        "Finally","For","ForEach","From","Function","Hidden","If","In",
        "Param","Process","Return","Static","Switch","Throw","Trap","Try",
        "Until","Using","Var","While","workflow","parallel","sequence"
    }, kw);

    // Attributes
    addHlRule(R"(\[(?:[A-Za-z]\w*(?:\[\])?)\])", pp);
    // Variables
    addHlRule(R"(\$(?:global:|local:|script:|private:|env:)?[A-Za-z_]\w*)", va);
    addHlRule(R"(\b([A-Za-z]+-[A-Za-z]+)(?:\s|$))", fn, 1);  // cmdlets
    addHlRule(R"("(?:[^"``]|``[\s\S])*")", sf);
    addHlRule(R"('(?:[^'])*')", sf);
    addHlRule(R"(@"[\s\S]*?"@)", sf);
    addHlRule(NUM_RE, nm);
}

void Highlighter::setupSql()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);

    addHlRule(R"(--[^\n]*)", cm);
    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression(R"(/\*)");
    mlc.end     = QRegularExpression(R"(\*/)");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);

    addHlKeywords({
        "SELECT","FROM","WHERE","AND","OR","NOT","IN","BETWEEN","LIKE","IS",
        "NULL","EXISTS","CASE","WHEN","THEN","ELSE","END","ORDER","BY","ASC",
        "DESC","GROUP","HAVING","LIMIT","OFFSET","JOIN","INNER","LEFT","RIGHT",
        "FULL","OUTER","CROSS","ON","UNION","ALL","INTERSECT","EXCEPT",
        "INSERT","INTO","VALUES","UPDATE","SET","DELETE","CREATE","TABLE",
        "VIEW","INDEX","DROP","ALTER","ADD","COLUMN","PRIMARY","KEY",
        "FOREIGN","REFERENCES","UNIQUE","DEFAULT","CHECK","CONSTRAINT",
        "BEGIN","COMMIT","ROLLBACK","TRANSACTION","GRANT","REVOKE",
        "WITH","AS","DISTINCT","TOP","FETCH","NEXT","ROWS","ONLY",
        // lowercase too
        "select","from","where","and","or","not","in","between","like","is",
        "null","exists","case","when","then","else","end","order","by","asc",
        "desc","group","having","limit","offset","join","inner","left","right",
        "full","outer","cross","on","union","all","intersect","except",
        "insert","into","values","update","set","delete","create","table",
        "view","index","drop","alter","add","column","primary","key",
        "foreign","references","unique","default","check","constraint",
        "begin","commit","rollback","transaction","with","as","distinct"
    }, kw);

    addHlKeywords({
        "INT","INTEGER","BIGINT","SMALLINT","TINYINT","FLOAT","DOUBLE",
        "DECIMAL","NUMERIC","REAL","CHAR","VARCHAR","TEXT","NCHAR","NVARCHAR",
        "NTEXT","DATE","TIME","DATETIME","TIMESTAMP","BOOLEAN","BOOL",
        "BINARY","VARBINARY","BLOB","CLOB","JSON","JSONB","UUID","SERIAL",
        "BIGSERIAL","SMALLSERIAL","MONEY","BIT","BYTEA","ARRAY","ENUM",
        "int","integer","bigint","smallint","float","double","decimal",
        "numeric","char","varchar","text","date","time","datetime",
        "timestamp","boolean","bool","binary","blob","json","uuid","serial"
    }, ty);

    addHlKeywords({
        "COUNT","SUM","AVG","MIN","MAX","COALESCE","NULLIF","IFNULL",
        "ISNULL","NVL","CAST","CONVERT","LEN","LENGTH","SUBSTR","SUBSTRING",
        "UPPER","LOWER","TRIM","LTRIM","RTRIM","REPLACE","CHARINDEX",
        "POSITION","CONCAT","NOW","GETDATE","DATEADD","DATEDIFF","YEAR",
        "MONTH","DAY","ROUND","FLOOR","CEIL","CEILING","ABS","MOD","POWER",
        "count","sum","avg","min","max","coalesce","nullif","cast","convert",
        "len","length","substr","substring","upper","lower","trim","replace",
        "concat","now","round","floor","ceil","abs","mod","power"
    }, fn);

    addHlRule(R"('(?:[^'\\]|\\.)*')", sf);
    addHlRule(NUM_RE, nm);
}

void Highlighter::setupJson()
{
    using namespace HlColor;
    auto key = Highlighter::hlFmt(Attribute);      // JSON keys
    auto sf  = Highlighter::hlFmt(String);         // string values
    auto nm  = Highlighter::hlFmt(Number);
    auto co  = Highlighter::hlFmt(Constant, true); // true/false/null

    addHlRule(R"("(?:[^"\\]|\\.)*"\s*:)", key);    // key
    addHlRule(R"(:\s*"(?:[^"\\]|\\.)*")", sf);      // string value
    addHlRule(R"(\b(true|false|null)\b)", co);
    addHlRule(NUM_RE, nm);
}

void Highlighter::setupXml()
{
    using namespace HlColor;
    auto tag = Highlighter::hlFmt(Tag, true);
    auto att = Highlighter::hlFmt(Attribute);
    auto sf  = Highlighter::hlFmt(String);
    auto cm  = Highlighter::hlFmt(Comment, false, true);
    auto pp  = Highlighter::hlFmt(Preprocessor);

    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression("<!--");
    mlc.end     = QRegularExpression("-->");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);

    addHlRule(R"(<\?[^?]*\?>)", pp);    // processing instruction
    addHlRule(R"(<!DOCTYPE[^>]*>)", pp);
    addHlRule(R"(</?[A-Za-z][A-Za-z0-9_:.-]*)", tag);
    addHlRule(R"([A-Za-z_:][A-Za-z0-9_:.-]*\s*=)", att);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'\\]|\\.)*')", sf);
    addHlRule(R"(&(?:[A-Za-z]+|#\d+|#x[0-9A-Fa-f]+);)", Highlighter::hlFmt(Escape));
}

void Highlighter::setupHtml()
{
    using namespace HlColor;
    auto tag  = Highlighter::hlFmt(Tag, true);
    auto att  = Highlighter::hlFmt(Attribute);
    auto sf   = Highlighter::hlFmt(String);
    auto cm   = Highlighter::hlFmt(Comment, false, true);
    auto pp   = Highlighter::hlFmt(Preprocessor);
    auto scr  = Highlighter::hlFmt(Type);   // script/style tags

    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression("<!--");
    mlc.end     = QRegularExpression("-->");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);

    addHlRule(R"(<!DOCTYPE[^>]*>)", pp);
    addHlRule(R"(</?(?:script|style|html|head|body|link|meta|base|title)\b)", scr);
    addHlRule(R"(</?[A-Za-z][A-Za-z0-9_-]*)", tag);
    addHlRule(R"([A-Za-z_-][A-Za-z0-9_-]*\s*=)", att);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'\\]|\\.)*')", sf);
    addHlRule(R"(&(?:[A-Za-z]+|#\d+|#x[0-9A-Fa-f]+);)", Highlighter::hlFmt(Escape));
}

void Highlighter::setupCss()
{
    using namespace HlColor;
    auto sel  = Highlighter::hlFmt(Tag, true);       // selectors
    auto prop = Highlighter::hlFmt(Attribute);       // properties
    auto val  = Highlighter::hlFmt(String);          // values
    auto nm   = Highlighter::hlFmt(Number);
    auto cm   = Highlighter::hlFmt(Comment, false, true);
    auto pp   = Highlighter::hlFmt(Preprocessor);   // @rules

    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression(R"(/\*)");
    mlc.end     = QRegularExpression(R"(\*/)");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);

    addHlRule(R"(@[A-Za-z-]+)", pp);
    addHlRule(R"((?:^|\})\s*([^{]+)\s*\{)", sel, 1);
    addHlRule(R"([A-Za-z-]+\s*(?=:))", prop);
    addHlRule(R"(:\s*[^;{}]+)", val);
    addHlRule(R"(#[0-9A-Fa-f]{3,8}\b)", nm);
    addHlRule(R"(\b\d+(?:\.\d+)?(?:px|em|rem|vh|vw|%|pt|cm|mm|s|ms|deg|fr)?\b)", nm);
    addHlRule(R"("(?:[^"\\]|\\.)*")", Highlighter::hlFmt(String));
    addHlRule(R"('(?:[^'\\]|\\.)*')", Highlighter::hlFmt(String));
}

void Highlighter::setupYaml()
{
    using namespace HlColor;
    auto key = Highlighter::hlFmt(Attribute);
    auto sf  = Highlighter::hlFmt(String);
    auto nm  = Highlighter::hlFmt(Number);
    auto cm  = Highlighter::hlFmt(Comment, false, true);
    auto co  = Highlighter::hlFmt(Constant, true);
    auto pp  = Highlighter::hlFmt(Preprocessor);

    addHlRule(R"(#[^\n]*)", cm);
    addHlRule(R"(^---$|^\.\.\.$)", pp);   // document markers
    addHlRule(R"(^\s*(?:-\s+)?([A-Za-z_][A-Za-z0-9_.-]*)(?=\s*:))", key, 1);
    addHlRule(R"(\b(true|false|yes|no|null|on|off|True|False|Yes|No|Null|ON|OFF)\b)", co);
    addHlRule(NUM_RE, nm);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'])*')", sf);
    addHlRule(R"(\|[-+]?|\>[-+]?)", pp);   // block scalars
    addHlRule(R"(&\w+|\*\w+|<<:)", Highlighter::hlFmt(Escape));  // anchors/aliases
}

void Highlighter::setupToml()
{
    using namespace HlColor;
    auto key  = Highlighter::hlFmt(Attribute);
    auto sf   = Highlighter::hlFmt(String);
    auto nm   = Highlighter::hlFmt(Number);
    auto cm   = Highlighter::hlFmt(Comment, false, true);
    auto co   = Highlighter::hlFmt(Constant, true);
    auto sect = Highlighter::hlFmt(Tag, true);

    addHlRule(R"(#[^\n]*)", cm);
    addHlRule(R"(^\s*\[{1,2}[^\]]*\]{1,2})", sect);   // [section] [[array]]
    addHlRule(R"(^\s*([A-Za-z_][A-Za-z0-9_.-]*)\s*=)", key, 1);
    addHlRule(R"(\b(true|false)\b)", co);
    addHlRule(R"("""(?:(?!""")[\s\S])*""")", sf);
    addHlRule(R"('''(?:(?!''')[\s\S])*''')", sf);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'])*')", sf);
    addHlRule(NUM_RE, nm);
    addHlRule(R"(\d{4}-\d{2}-\d{2}(?:T\d{2}:\d{2}:\d{2})?(?:Z|[+-]\d{2}:\d{2})?)", nm);
}

void Highlighter::setupMarkdown()
{
    using namespace HlColor;
    auto h1  = Highlighter::hlFmt(Tag,      true);
    auto h2  = Highlighter::hlFmt(Attribute,true);
    auto em  = Highlighter::hlFmt(Type,     false, true);
    auto bold= Highlighter::hlFmt(Type,     true);
    auto cod = Highlighter::hlFmt(String);
    auto lnk = Highlighter::hlFmt(Constant);
    auto hr  = Highlighter::hlFmt(Comment);
    auto blk = Highlighter::hlFmt(Preprocessor);

    addHlRule(R"(^#{1}\s+.+$)", h1);
    addHlRule(R"(^#{2,3}\s+.+$)", h2);
    addHlRule(R"(^#{4,6}\s+.+$)", Highlighter::hlFmt(Comment, true));
    addHlRule(R"(\*\*(?:[^*]|\*(?!\*))+\*\*|__(?:[^_]|_(?!_))+__)", bold);
    addHlRule(R"(\*(?:[^*])+\*|_(?:[^_])+_)", em);
    addHlRule(R"(`[^`]+`)", cod);
    addHlRule(R"(```[\s\S]*?```)", cod);
    addHlRule(R"(\[([^\]]+)\]\([^\)]+\))", lnk);
    addHlRule(R"(!\[([^\]]*)\]\([^\)]+\))", lnk);
    addHlRule(R"(^[-*_]{3,}\s*$)", hr);
    addHlRule(R"(^\s*[-*+]\s)", blk);
    addHlRule(R"(^\s*\d+\.\s)", blk);
    addHlRule(R"(^>\s)", Highlighter::hlFmt(Comment, false, true));
}

void Highlighter::setupDiff()
{
    using namespace HlColor;
    addHlRule(R"(^\+[^\+].*$)", Highlighter::hlFmt(QColor(0x3f,0xb9,0x50)));  // green added
    addHlRule(R"(^-[^-].*$)",   Highlighter::hlFmt(QColor(0xf8,0x51,0x49)));  // red removed
    addHlRule(R"(^@@.*@@.*$)",  Highlighter::hlFmt(QColor(0x58,0xa6,0xff)));  // blue hunk
    addHlRule(R"(^---.*$|^\+\+\+.*$)", Highlighter::hlFmt(QColor(0xff,0xb8,0x6c), true)); // orange file
    addHlRule(R"(^diff .*$)",   Highlighter::hlFmt(QColor(0xff,0xb8,0x6c), true));
    addHlRule(R"(^index .*$)",  Highlighter::hlFmt(Comment, false, true));
}

void Highlighter::setupDockerfile()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto sf = Highlighter::hlFmt(String);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto va = Highlighter::hlFmt(Attribute);

    addHlRule(R"(#[^\n]*)", cm);
    addHlKeywords({
        "FROM","RUN","CMD","LABEL","EXPOSE","ENV","ADD","COPY","ENTRYPOINT",
        "VOLUME","USER","WORKDIR","ARG","ONBUILD","STOPSIGNAL","HEALTHCHECK",
        "SHELL","MAINTAINER","from","run","cmd","label","expose","env","add",
        "copy","entrypoint","volume","user","workdir","arg","onbuild"
    }, kw);
    addHlRule(R"(\$(?:\{[^}]*\}|[A-Za-z_]\w*))", va);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'])*')", sf);
}

void Highlighter::setupMakefile()
{
    using namespace HlColor;
    auto tg = Highlighter::hlFmt(Tag, true);
    auto va = Highlighter::hlFmt(Attribute);
    auto sf = Highlighter::hlFmt(String);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto fn = Highlighter::hlFmt(Function);

    addHlRule(R"(#[^\n]*)", cm);
    addHlRule(R"(^[A-Za-z_][A-Za-z0-9_.-]*\s*:(?!=))", tg);
    addHlRule(R"(\$(?:\([^)]*\)|\{[^}]*\}|[@<^?*%|]))", va);
    addHlKeywords({".PHONY",".DEFAULT",".PRECIOUS",".INTERMEDIATE",
                 ".SECONDARY",".SUFFIXES",".DELETE_ON_ERROR",
                 "include","-include","sinclude","vpath"}, kw);
    addHlRule(R"(\b([A-Za-z_][A-Za-z0-9_]*)\s*:?=)", fn, 1);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'])*')", sf);
}

void Highlighter::setupNginx()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto va = Highlighter::hlFmt(Attribute);

    addHlRule(R"(#[^\n]*)", cm);
    addHlKeywords({
        "server","location","upstream","events","http","stream","mail","geo",
        "map","split_clients","limit_req_zone","limit_conn_zone","if","set",
        "return","rewrite","break","last","redirect","permanent","add_header",
        "proxy_pass","proxy_set_header","proxy_redirect","proxy_cache",
        "fastcgi_pass","uwsgi_pass","scgi_pass","grpc_pass",
        "listen","server_name","root","index","try_files","alias",
        "include","error_page","access_log","error_log","log_format",
        "worker_processes","worker_connections","keepalive_timeout",
        "client_max_body_size","gzip","gzip_types","ssl_certificate",
        "ssl_certificate_key","ssl_protocols","ssl_ciphers",
        "autoindex","sendfile","tcp_nopush","tcp_nodelay"
    }, kw);

    addHlRule(R"(\$[A-Za-z_]\w*)", va);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'])*')", sf);
    addHlRule(R"(\b\d+(?:s|ms|m|h|d|w|M|y|k|K|m|M|g|G)?\b)", nm);
    addHlKeywords({"on","off"}, Highlighter::hlFmt(Constant, true));
}

void Highlighter::setupLatex()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto sf = Highlighter::hlFmt(String);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto pp = Highlighter::hlFmt(Preprocessor);
    auto ma = Highlighter::hlFmt(Type);   // math

    addHlRule(R"(%[^\n]*)", cm);
    addHlRule(R"(\\(?:begin|end)\{[^}]*\})", kw);
    addHlRule(R"(\\[A-Za-z]+(?:\*)?)", pp);
    addHlRule(R"(\{[^}]*\})", sf);
    addHlRule(R"(\$[^$]+\$|\$\$[^$]+\$\$)", ma);
}

void Highlighter::setupLua()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto co = Highlighter::hlFmt(Constant, true);

    addHlRule(R"(--(?!\[=*\[)[^\n]*)", cm);
    Highlighter::MultiLineComment mlc;
    mlc.start   = QRegularExpression(R"(--\[=*\[)");
    mlc.end     = QRegularExpression(R"(\]=*\])");
    mlc.format  = cm;
    mlc.stateId = 1;
    addMultiLineComment(mlc);

    addHlKeywords({
        "and","break","do","else","elseif","end","for","function","goto","if",
        "in","local","not","or","repeat","return","then","until","while"
    }, kw);
    addHlKeywords({"true","false","nil"}, co);
    addHlKeywords({
        "print","require","pairs","ipairs","next","select","type","tostring",
        "tonumber","error","assert","pcall","xpcall","rawget","rawset",
        "rawequal","setmetatable","getmetatable","unpack","table","string",
        "math","io","os","coroutine","package","load","loadfile","dofile",
        "collectgarbage","module"
    }, ty);

    addHlRule(R"(\bfunction\s+(?:[A-Za-z_]\w*\.)*([A-Za-z_]\w*))", fn, 1);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'\\]|\\.)*')", sf);
    addHlRule(R"(\[=*\[(?:(?!\]=*\])[\s\S])*\]=*\])", sf);
    addHlRule(NUM_RE, nm);
}

void Highlighter::setupR()
{
    using namespace HlColor;
    auto kw = Highlighter::hlFmt(Keyword, true);
    auto ty = Highlighter::hlFmt(Type);
    auto fn = Highlighter::hlFmt(Function);
    auto sf = Highlighter::hlFmt(String);
    auto nm = Highlighter::hlFmt(Number);
    auto cm = Highlighter::hlFmt(Comment, false, true);
    auto co = Highlighter::hlFmt(Constant, true);
    auto op = Highlighter::hlFmt(Operator);

    addHlRule(R"(#[^\n]*)", cm);
    addHlKeywords({
        "if","else","for","while","repeat","function","return","next","break",
        "in","NULL","NA","NA_integer_","NA_real_","NA_complex_","NA_character_",
        "Inf","NaN","TRUE","FALSE","T","F","..."
    }, kw);
    addHlKeywords({"TRUE","FALSE","NULL","NA","Inf","NaN"}, co);
    addHlKeywords({
        "c","list","data.frame","matrix","array","factor","vector","numeric",
        "integer","character","logical","complex","raw","which","seq","rep",
        "length","nrow","ncol","dim","names","colnames","rownames","head",
        "tail","str","summary","print","cat","paste","paste0","sprintf",
        "format","strsplit","gsub","sub","grep","grepl","apply","lapply",
        "sapply","tapply","mapply","Reduce","Filter","Map","environment",
        "library","require","source","read.csv","write.csv","ggplot","aes",
        "geom_point","geom_line","dplyr","tidyverse","lm","glm","optim"
    }, ty);
    addHlRule(R"(%[A-Za-z]+%)", op);   // infix operators like %in%, %>%
    addHlRule(R"(\b([A-Za-z_.][A-Za-z0-9_.]*)\s*(?=\())", fn, 1);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"('(?:[^'\\]|\\.)*')", sf);
    addHlRule(NUM_RE, nm);
}

void Highlighter::setupGraphql()
{
    using namespace HlColor;
    auto kw  = Highlighter::hlFmt(Keyword, true);
    auto ty  = Highlighter::hlFmt(Type);
    auto fn  = Highlighter::hlFmt(Function);
    auto sf  = Highlighter::hlFmt(String);
    auto nm  = Highlighter::hlFmt(Number);
    auto cm  = Highlighter::hlFmt(Comment, false, true);
    auto dir = Highlighter::hlFmt(Preprocessor);

    addHlRule(R"(#[^\n]*)", cm);
    addHlKeywords({
        "query","mutation","subscription","fragment","on","inline",
        "schema","scalar","type","interface","union","enum","input",
        "directive","extend","implements","repeatable","true","false","null"
    }, kw);
    addHlKeywords({
        "String","Int","Float","Boolean","ID","__Schema","__Type",
        "__Field","__InputValue","__EnumValue","__Directive"
    }, ty);
    addHlRule(R"(@[A-Za-z]\w*)", dir);
    addHlRule(R"(\$[A-Za-z_]\w*)", Highlighter::hlFmt(Attribute));
    addHlRule(R"(\b([A-Za-z_]\w*)\s*(?=[\(\{:]))", fn, 1);
    addHlRule(R"("(?:[^"\\]|\\.)*")", sf);
    addHlRule(R"("""(?:(?!""")[\s\S])*""")", sf);
    addHlRule(NUM_RE, nm);
}

void Highlighter::setupHttp()
{
    using namespace HlColor;
    // Status line / method
    addHlRule(R"(^(?:GET|POST|PUT|DELETE|PATCH|HEAD|OPTIONS|CONNECT|TRACE)\s)", Highlighter::hlFmt(Keyword, true));
    addHlRule(R"(^HTTP/\d\.\d\s+\d{3}\s+[^\n]+)", Highlighter::hlFmt(Tag, true));
    // Header names
    addHlRule(R"(^[A-Za-z][A-Za-z0-9-]*(?=\s*:))", Highlighter::hlFmt(Attribute));
    // Header values
    addHlRule(R"(:\s*[^\n]+$)", Highlighter::hlFmt(String));
    // URLs
    addHlRule(R"(https?://[^\s]+)", Highlighter::hlFmt(Constant));
    // Status codes
    addHlRule(R"(\b[1-5]\d{2}\b)", Highlighter::hlFmt(Number));
}

void Highlighter::setupPlaintext()
{
    // No rules — plain text has no highlighting
    clearRules();
}
