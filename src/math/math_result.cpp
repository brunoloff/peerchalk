#include "math/math_engine.hpp"

#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace peerchalk {
namespace {

constexpr qsizetype headerBytes = 40;

MathFormula failure(const QString &message) {
    MathFormula formula;
    formula.error = message;
    return formula;
}

quint32 wordAt(const char *data) { return qFromLittleEndian<quint32>(data); }

double doubleAt(const char *data) {
    const auto bits = qFromLittleEndian<quint64>(data);
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

float floatAt(const char *data) {
    const quint32 bits = wordAt(data);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

bool indicesAreValid(const MathFormula &formula) {
    const quint64 vertexCount = formula.vertices.size() / 2;
    return std::all_of(formula.indices.begin(), formula.indices.end(),
                       [&](quint32 index) { return index < vertexCount; }) &&
           std::all_of(formula.runs.begin(), formula.runs.end(), [&](const MathRun &run) {
               return quint64{run.firstIndex} + run.indexCount <= formula.indices.size();
           });
}

void readMesh(const char *data, MathFormula &formula) {
    for (float &value : formula.vertices) {
        value = floatAt(data);
        data += 4;
    }
    for (quint32 &value : formula.indices) {
        value = wordAt(data);
        data += 4;
    }
    for (MathRun &run : formula.runs) {
        run = {wordAt(data), wordAt(data + 4), wordAt(data + 8)};
        data += 12;
    }
}

} // namespace

quint32 toMathRgba(QRgb color) {
    return quint32(qRed(color)) << 24 | quint32(qGreen(color)) << 16 |
           quint32(qBlue(color)) << 8 | quint32(qAlpha(color));
}

MathFormula parseMathResult(QByteArrayView bytes) {
    const QString malformed = QStringLiteral("the math engine returned malformed data");
    if (bytes.size() < headerBytes)
        return failure(malformed);
    const char *data = bytes.data();
    const quint64 vertices = wordAt(data);
    const quint64 indices = wordAt(data + 4);
    const quint64 runs = wordAt(data + 8);
    const quint64 errorSize = wordAt(data + 12);
    const quint64 meshSize = vertices * 8 + indices * 4 + runs * 12;
    if (quint64(bytes.size()) != quint64(headerBytes) + meshSize + errorSize)
        return failure(malformed);
    const char *mesh = data + headerBytes;
    if (errorSize > 0)
        return failure(QString::fromUtf8(mesh + meshSize, qsizetype(errorSize)));
    MathFormula formula;
    formula.width = doubleAt(data + 16);
    formula.height = doubleAt(data + 24);
    formula.depth = doubleAt(data + 32);
    formula.vertices.resize(vertices * 2);
    formula.indices.resize(indices);
    formula.runs.resize(runs);
    readMesh(mesh, formula);
    const bool finite = std::isfinite(formula.width) && std::isfinite(formula.height) &&
                        std::isfinite(formula.depth);
    return finite && indicesAreValid(formula) ? formula : failure(malformed);
}

} // namespace peerchalk
