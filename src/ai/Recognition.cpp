#include "ai/Recognition.h"

#include <QDir>
#include <QPluginLoader>

namespace cb {

void RecognizerRegistry::registerEquationRecognizer(std::unique_ptr<EquationRecognizer> recognizer)
{
    if (recognizer)
        m_equation.push_back(std::move(recognizer));
}

const EquationRecognizer* RecognizerRegistry::equationRecognizer() const
{
    return m_equation.empty() ? nullptr : m_equation.front().get();
}

int RecognizerRegistry::loadPlugins(const QString& directory)
{
    int loaded = 0;
    const QDir dir(directory);
    if (!dir.exists())
        return 0;
    for (const QString& file : dir.entryList(QDir::Files)) {
        QPluginLoader loader(dir.absoluteFilePath(file));
        QObject* instance = loader.instance();
        if (!instance)
            continue;
        if (auto* factory = qobject_cast<EquationRecognizerFactory*>(instance)) {
            registerEquationRecognizer(factory->create());
            ++loaded;
        }
    }
    return loaded;
}

} // namespace cb
