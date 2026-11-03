#include "tools/MagicEquation.h"

#include "core/Geometry.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "document/StrokeObject.h"
#include "math/equation/EquationObject.h"

#include <QCoreApplication>

#include <algorithm>

namespace cb::magic {

bool isHandwriting(const Page& page, const QVector<ObjectId>& ids)
{
    if (ids.isEmpty())
        return false;
    for (const ObjectId& id : ids) {
        const DocumentObject* o = page.object(id);
        if (!o || o->type() != ObjectType::Stroke)
            return false;
    }
    return true;
}

InkSample collectInk(const Page& page, const QVector<ObjectId>& ids)
{
    InkSample ink;
    for (const auto& o : page.objects()) {
        if (o->type() != ObjectType::Stroke || !ids.contains(o->id()))
            continue;
        ink.strokes.push_back(static_cast<const StrokeObject*>(o.get())->pagePoints());
    }
    return ink;
}

ObjectId acceptEquation(Document& doc, const Page& page, const QVector<ObjectId>& strokes, const QString& latex)
{
    if (latex.trimmed().isEmpty() || !isHandwriting(page, strokes))
        return ObjectId();
    QRectF bounds;
    QColor color;
    for (const ObjectId& id : strokes) {
        const auto* s = static_cast<const StrokeObject*>(page.object(id));
        bounds = bounds.isNull() ? s->sceneBounds() : bounds.united(s->sceneBounds());
        if (!color.isValid())
            color = s->ink().color;
    }
    // Handwriting is usually larger than typeset text: about 70 % of its height, with bounds.
    const bool tall = latex.contains(QLatin1String("\\frac"));
    const qreal size = std::clamp(bounds.height() * (tall ? 0.42 : 0.7), 24.0, 400.0);
    auto equation = EquationObject::create(latex, bounds.center(), size, color.isValid() ? color : QColor(Qt::white));
    const ObjectId id = equation->id();

    auto macro = std::make_unique<CompositeCommand>(QCoreApplication::translate("MagicEquation", "Magic equation"));
    std::vector<ObjectId> ids(strokes.begin(), strokes.end());
    auto remove = std::make_unique<RemoveObjectsCommand>(page.id(), std::move(ids));
    remove->redo(doc);
    macro->add(std::move(remove));
    std::vector<ObjectPtr> added;
    added.push_back(std::move(equation));
    auto add = std::make_unique<AddObjectsCommand>(page.id(), std::move(added));
    add->redo(doc);
    macro->add(std::move(add));
    doc.commands().pushApplied(std::move(macro));
    return id;
}

} // namespace cb::magic
